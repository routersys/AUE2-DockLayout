#include "FloatWindowHook.h"

#include <windows.h>
#include <vector>

#include "DragSession.h"
#include "FloatOrigin.h"
#include "HostContext.h"

namespace dl {

static const wchar_t* const kHostClassName = L"aviutl2Manager";
static const wchar_t* const kWatchClassName = L"DockLayoutFloatWatch";
static const UINT_PTR kWatchTimerId = 1;
static const UINT kWatchIntervalMs = 500;

static HWND g_watch = nullptr;
static std::vector<HWND> g_hooked;

static void Unhook(HWND window) {
    WNDPROC original = OriginalProc(window);
    if (original) SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)original);
    ClearWindowProc(window);
    for (size_t i = 0; i < g_hooked.size(); i++) {
        if (g_hooked[i] != window) continue;
        g_hooked.erase(g_hooked.begin() + i);
        return;
    }
}

static LRESULT CALLBACK FloatProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_MOVE) Drag().BeginFloat(hwnd);
            break;

        case WM_SIZING:
            if (Drag().Window() == hwnd) Drag().Cancel();
            break;

        case WM_MOVING: {
            if (!Drag().Dragging()) Drag().BeginFloat(hwnd);
            if (Drag().Window() == hwnd) {
                POINT cursor = {};
                GetCursorPos(&cursor);
                Drag().Track(cursor);
            }
            break;
        }

        case WM_EXITSIZEMOVE: {
            const LRESULT result = CallOriginalProc(hwnd, msg, wp, lp);
            if (Drag().Window() == hwnd) {
                POINT cursor = {};
                GetCursorPos(&cursor);
                Drag().Commit(cursor);
            }
            return result;
        }

        case WM_NCDESTROY: {
            if (Drag().Window() == hwnd) Drag().Cancel();
            const LRESULT result = CallOriginalProc(hwnd, msg, wp, lp);
            ForgetFloat(hwnd);
            Unhook(hwnd);
            return result;
        }
    }
    return CallOriginalProc(hwnd, msg, wp, lp);
}

static bool Hooked(HWND window) {
    for (HWND hooked : g_hooked)
        if (hooked == window) return true;
    return false;
}

static BOOL CALLBACK Collect(HWND window, LPARAM) {
    if (window == HostWindow() || Hooked(window)) return TRUE;

    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process != GetCurrentProcessId()) return TRUE;

    wchar_t name[64] = {};
    GetClassNameW(window, name, 63);
    if (wcscmp(name, kHostClassName) != 0) return TRUE;

    SetWindowProc(window, (WNDPROC)SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)FloatProc));
    g_hooked.push_back(window);

    FloatOrigin origin;
    if (TakePendingFloat(&origin)) RememberFloat(window, origin);
    return TRUE;
}

static LRESULT CALLBACK WatchProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TIMER && wp == kWatchTimerId) {
        EnumWindows(Collect, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void InstallFloatWindowHook() {
    if (g_watch) return;

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WatchProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWatchClassName;
    RegisterClassExW(&wc);

    g_watch = CreateWindowExW(0, kWatchClassName, L"", 0, 0, 0, 0, 0,
                              HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (g_watch) SetTimer(g_watch, kWatchTimerId, kWatchIntervalMs, nullptr);
    EnumWindows(Collect, 0);
}

void UninstallFloatWindowHook() {
    if (g_watch) {
        KillTimer(g_watch, kWatchTimerId);
        DestroyWindow(g_watch);
        g_watch = nullptr;
    }
    while (!g_hooked.empty()) Unhook(g_hooked.back());
}

}
