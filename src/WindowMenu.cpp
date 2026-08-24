#include "WindowMenu.h"

#include <string.h>

#include "HostContext.h"

namespace dl {

static const wchar_t* const kCommandKeys[] = {
    L"左側エリア", L"右側エリア", L"上側エリア", L"下側エリア", L"中央エリア",
    L"先頭に移動", L"上に移動", L"下に移動", L"最後に移動",
    L"ウィンドウを分離", L"ウィンドウのグループ化", L"ウィンドウを統合", L"ウィンドウを閉じる",
};
static const wchar_t* const kPlacementKey = L"ウィンドウ配置";

typedef BOOL (WINAPI* TrackPopupMenu_t)(HMENU, UINT, int, int, int, HWND, const RECT*);
static TrackPopupMenu_t g_original = nullptr;
static void** g_slot = nullptr;

static bool        g_pending = false;
static int         g_wanted = -1;
static UINT        g_resultId = 0;
static WindowState g_state;

static void** FindIatSlot(HMODULE mod, const char* dll, const char* fn) {
    auto base = (BYTE*)mod;
    auto dos = (IMAGE_DOS_HEADER*)base;
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    auto nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    auto dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return nullptr;
    for (auto imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); imp->Name; ++imp) {
        if (_stricmp((const char*)(base + imp->Name), dll) != 0) continue;
        auto oft = (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk
                                                                       : imp->FirstThunk));
        auto ft = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (; oft->u1.AddressOfData; ++oft, ++ft) {
            if (IMAGE_SNAP_BY_ORDINAL(oft->u1.Ordinal)) continue;
            auto ibn = (IMAGE_IMPORT_BY_NAME*)(base + oft->u1.AddressOfData);
            if (strcmp((const char*)ibn->Name, fn) == 0) return (void**)&ft->u1.Function;
        }
    }
    return nullptr;
}

static bool PatchSlot(void** slot, void* fn, void** old) {
    DWORD prot = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &prot)) return false;
    if (old) *old = *slot;
    *slot = fn;
    VirtualProtect(slot, sizeof(void*), prot, &prot);
    return true;
}

static HMENU FindSubmenu(HMENU menu, const wchar_t* text, int depth) {
    if (!menu || depth > 3) return nullptr;
    const int count = GetMenuItemCount(menu);
    for (int i = 0; i < count; i++) {
        wchar_t label[256] = {};
        MENUITEMINFOW info = { sizeof(info) };
        info.fMask = MIIM_SUBMENU | MIIM_STRING;
        info.dwTypeData = label;
        info.cch = 255;
        if (!GetMenuItemInfoW(menu, i, TRUE, &info) || !info.hSubMenu) continue;
        if (wcscmp(label, text) == 0) return info.hSubMenu;
        if (HMENU found = FindSubmenu(info.hSubMenu, text, depth + 1)) return found;
    }
    return nullptr;
}

static bool FindItem(HMENU menu, const wchar_t* text, int depth, UINT* id, UINT* state) {
    if (!menu || depth > 2) return false;
    const int count = GetMenuItemCount(menu);
    for (int i = 0; i < count; i++) {
        wchar_t label[256] = {};
        MENUITEMINFOW info = { sizeof(info) };
        info.fMask = MIIM_ID | MIIM_STATE | MIIM_SUBMENU | MIIM_STRING;
        info.dwTypeData = label;
        info.cch = 255;
        if (!GetMenuItemInfoW(menu, i, TRUE, &info)) continue;
        if (!info.hSubMenu && wcscmp(label, text) == 0) {
            *id = info.wID;
            *state = info.fState;
            return true;
        }
        if (info.hSubMenu && FindItem(info.hSubMenu, text, depth + 1, id, state)) return true;
    }
    return false;
}

static bool ItemEnabled(UINT state) { return (state & MFS_GRAYED) == 0; }

static void Collect(HMENU menu) {
    g_state = WindowState();
    g_resultId = 0;

    HMENU placement = FindSubmenu(menu, MenuText(kPlacementKey), 0);
    if (!placement) return;
    g_state.valid = true;

    UINT id = 0, state = 0;
    for (int i = 0; i < kAreaCount; i++) {
        if (FindItem(placement, MenuText(kCommandKeys[i]), 0, &id, &state) && (state & MFS_CHECKED))
            g_state.area = i;
    }
    if (FindItem(placement, MenuText(kCommandKeys[(int)WindowCommand::Group]), 0, &id, &state))
        g_state.grouped = (state & MFS_CHECKED) != 0;
    if (FindItem(placement, MenuText(kCommandKeys[(int)WindowCommand::MoveUp]), 0, &id, &state))
        g_state.canMove = ItemEnabled(state);
    if (!g_state.canMove &&
        FindItem(placement, MenuText(kCommandKeys[(int)WindowCommand::MoveDown]), 0, &id, &state))
        g_state.canMove = ItemEnabled(state);

    if (g_wanted < 0) return;
    if (FindItem(placement, MenuText(kCommandKeys[g_wanted]), 0, &id, &state) && ItemEnabled(state))
        g_resultId = id;
}

static BOOL WINAPI HookTrackPopupMenu(HMENU menu, UINT flags, int x, int y,
                                      int reserved, HWND hwnd, const RECT* area) {
    if (!g_pending) return g_original(menu, flags, x, y, reserved, hwnd, area);
    Collect(menu);
    return (BOOL)g_resultId;
}

static bool Dispatch(HWND window, POINT clientPt, int wanted, WindowState* state) {
    if (!window || !g_original || g_pending) return false;

    g_pending = true;
    g_wanted = wanted;
    g_state = WindowState();
    g_resultId = 0;
    CallOriginalProc(window, WM_RBUTTONUP, 0, MAKELPARAM(clientPt.x, clientPt.y));
    g_pending = false;
    g_wanted = -1;

    if (state) *state = g_state;
    return g_state.valid && (wanted < 0 || g_resultId != 0);
}

void InstallWindowMenuHook() {
    if (g_slot) return;
    g_slot = FindIatSlot(GetModuleHandleW(nullptr), "USER32.dll", "TrackPopupMenu");
    if (g_slot) PatchSlot(g_slot, (void*)&HookTrackPopupMenu, (void**)&g_original);
}

void UninstallWindowMenuHook() {
    if (g_slot && g_original) PatchSlot(g_slot, (void*)g_original, nullptr);
    g_slot = nullptr;
    g_original = nullptr;
}

WindowCommand AreaCommand(int area) {
    return static_cast<WindowCommand>(area);
}

WindowState QueryWindow(HWND window, POINT clientPt) {
    WindowState state;
    Dispatch(window, clientPt, -1, &state);
    return state;
}

bool InvokeWindowCommand(HWND window, POINT clientPt, WindowCommand command) {
    return Dispatch(window, clientPt, static_cast<int>(command), nullptr);
}

}
