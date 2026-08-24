#include "DragSession.h"

#include <stdlib.h>
#include <utility>
#include <vector>

#include "DockGuides.h"
#include "DockModel.h"
#include "GuideWindow.h"
#include "HostContext.h"
#include "Log.h"
#include "PanelLayout.h"
#include "StyleMetrics.h"
#include "WindowMenu.h"

namespace dl {

static const wchar_t* const kCommandClassName = L"DockLayoutCommand";
static const UINT kExecuteMessage = WM_APP;
static const int kFloatProbeInset = 2;

static DragSession g_drag;
static DockModel   g_model;
static DockGuides  g_guides;
static PanelLayout g_layout;
static HWND        g_commandWindow = nullptr;

DragSession& Drag() { return g_drag; }

static LRESULT CALLBACK CommandProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == kExecuteMessage) {
        g_drag.Finish();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND CommandWindow() {
    if (g_commandWindow) return g_commandWindow;

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = CommandProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kCommandClassName;
    RegisterClassExW(&wc);

    g_commandWindow = CreateWindowExW(0, kCommandClassName, L"", 0, 0, 0, 0, 0,
                                      HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    return g_commandWindow;
}

static bool ModifierHeld() {
    return (GetKeyState(VK_MENU) & 0x8000) != 0;
}

static bool HeaderColored(POINT clientPt) {
    HDC dc = GetDC(HostWindow());
    if (!dc) return false;
    const COLORREF pixel = GetPixel(dc, clientPt.x, clientPt.y);
    ReleaseDC(HostWindow(), dc);
    if (pixel == CLR_INVALID) return false;

    const int color = (GetRValue(pixel) << 16) | (GetGValue(pixel) << 8) | GetBValue(pixel);
    const StyleMetrics& style = Style();
    return color == style.titleHeader || color == style.grouping ||
           color == style.groupingHover || color == style.groupingSelect;
}

static bool InTitle(const Panel& panel, POINT clientPt) {
    return panel.HasTitle() && clientPt.y >= panel.title.top && clientPt.y < panel.title.bottom;
}

static POINT ToHostClient(POINT screenPt) {
    POINT pt = screenPt;
    ScreenToClient(HostWindow(), &pt);
    return pt;
}

static POINT ToScreen(HWND window, POINT clientPt) {
    POINT pt = clientPt;
    ClientToScreen(window, &pt);
    return pt;
}

bool DragSession::Holding() const { return phase_ == Phase::Armed; }
bool DragSession::Dragging() const { return phase_ == Phase::Dragging; }

bool DragSession::BeginPanel(POINT clientPt) {
    if (phase_ != Phase::Idle || pending_ || !HostWindow()) return false;

    const bool modifier = ModifierHeld();
    if (!modifier && !HeaderColored(clientPt)) return false;
    if (!g_layout.Build()) return false;
    const int index = g_layout.IndexAt(clientPt);
    if (index < 0) return false;
    if (!modifier && !InTitle(g_layout.Panels()[index], clientPt)) return false;

    window_ = HostWindow();
    origin_ = clientPt;
    panel_ = -1;
    phase_ = Phase::Armed;
    return true;
}

bool DragSession::BeginFloat(HWND window) {
    if (phase_ != Phase::Idle || pending_ || !window || window == HostWindow()) return false;
    window_ = window;
    origin_ = POINT{};
    panel_ = -1;
    if (!Start()) { Reset(); return false; }
    return true;
}

bool DragSession::Start() {
    panel_ = -1;
    if (window_ == HostWindow()) {
        if (!g_model.Build()) return false;
        panel_ = g_model.IndexAt(origin_);
        if (panel_ < 0) return false;
    } else if (!g_model.BuildRegion()) {
        return false;
    }

    g_guides.Clear();
    phase_ = Phase::Dragging;
    return true;
}

void DragSession::Track(POINT screenPt) {
    if (phase_ == Phase::Armed) {
        const POINT start = ToScreen(window_, origin_);
        if (abs(screenPt.x - start.x) < GetSystemMetrics(SM_CXDRAG) &&
            abs(screenPt.y - start.y) < GetSystemMetrics(SM_CYDRAG))
            return;
        if (!Start()) { Reset(); return; }
        SetCapture(window_);
    }
    if (phase_ != Phase::Dragging) return;

    const POINT hostPt = ToHostClient(screenPt);
    const bool inside = PtInRect(&g_model.Region(), hostPt) != 0;
    g_guides.Update(g_model, panel_, hostPt, inside);
    Overlay().Update(g_guides);
}

void DragSession::ExecuteGroup(int target) {
    const DockPanel* from = g_model.Get(panel_);
    const DockPanel* onto = g_model.Get(target);
    if (!from || !onto) return;

    if (!onto->grouped) InvokeWindowCommand(HostWindow(), onto->probe, WindowCommand::Group);
    if (!from->grouped) InvokeWindowCommand(HostWindow(), origin_, WindowCommand::Group);
    if (from->area != onto->area)
        InvokeWindowCommand(HostWindow(), origin_, AreaCommand(onto->area));
    LogF(L"ドッキング配置: パネルをまとめました");
}

void DragSession::ExecuteInsert(int target, bool after) {
    const DockPanel* from = g_model.Get(panel_);
    const DockPanel* onto = g_model.Get(target);
    if (!from || !onto) return;

    std::vector<const DockPanel*> slots;
    for (const DockPanel& panel : g_model.Panels())
        if (panel.stack == from->stack) slots.push_back(&panel);
    for (size_t i = 1; i < slots.size(); i++)
        for (size_t j = i; j > 0 && slots[j - 1]->order > slots[j]->order; j--)
            std::swap(slots[j - 1], slots[j]);

    int at = -1, to = -1;
    for (size_t i = 0; i < slots.size(); i++) {
        if (slots[i] == from) at = static_cast<int>(i);
        if (slots[i] == onto) to = static_cast<int>(i);
    }
    if (at < 0 || to < 0) return;

    const int destination = after ? (at < to ? to : to + 1) : (at < to ? to - 1 : to);
    if (destination == at) return;

    const int step = destination < at ? -1 : 1;
    const WindowCommand command = step < 0 ? WindowCommand::MoveUp : WindowCommand::MoveDown;
    const POINT grab = { origin_.x - from->rect.left, origin_.y - from->rect.top };
    for (int index = at; index != destination; index += step) {
        const RECT& slot = slots[index]->rect;
        const POINT point = { min(slot.left + grab.x, slot.right - 1),
                              min(slot.top + grab.y, slot.bottom - 1) };
        InvokeWindowCommand(HostWindow(), point, command);
    }
    LogF(L"ドッキング配置: パネルの並び順を変えました");
}

void DragSession::Execute() {
    const DropTarget target = g_guides.Target();
    switch (target.kind) {
        case DropKind::Area: {
            const DockPanel* from = g_model.Get(panel_);
            if (!from || from->area == target.area) return;
            if (InvokeWindowCommand(HostWindow(), origin_, AreaCommand(target.area)))
                LogF(L"ドッキング配置: パネルをエリア %d へ移しました", target.area);
            return;
        }
        case DropKind::Group:
            ExecuteGroup(target.panel);
            return;
        case DropKind::Insert:
            ExecuteInsert(target.panel, target.after);
            return;
        case DropKind::Detach:
            if (InvokeWindowCommand(HostWindow(), origin_, WindowCommand::Detach))
                LogF(L"ドッキング配置: パネルを分離しました");
            return;
        case DropKind::Merge: {
            RECT client = {};
            if (!GetClientRect(window_, &client)) return;
            const POINT candidates[] = {
                { kFloatProbeInset, kFloatProbeInset },
                { client.right - kFloatProbeInset, kFloatProbeInset },
                { client.right / 2, client.bottom / 2 },
            };
            for (const POINT& point : candidates) {
                if (!InvokeWindowCommand(window_, point, WindowCommand::Merge)) continue;
                LogF(L"ドッキング配置: ウィンドウを本体へ戻しました");
                return;
            }
            return;
        }
        default:
            return;
    }
}

void DragSession::Replay(POINT screenPt) {
    POINT release = screenPt;
    ScreenToClient(window_, &release);
    CallOriginalProc(window_, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(origin_.x, origin_.y));
    CallOriginalProc(window_, WM_LBUTTONUP, 0, MAKELPARAM(release.x, release.y));
}

void DragSession::Commit(POINT screenPt) {
    if (phase_ == Phase::Armed) {
        phase_ = Phase::Idle;
        Replay(screenPt);
        Reset();
        return;
    }
    if (phase_ != Phase::Dragging) return;

    phase_ = Phase::Idle;
    if (GetCapture() == window_) ReleaseCapture();
    Overlay().Hide();
    pending_ = true;
    PostMessageW(CommandWindow(), kExecuteMessage, 0, 0);
}

void DragSession::Finish() {
    if (!pending_) return;
    pending_ = false;
    Execute();
    Reset();
}

void DragSession::Cancel() {
    const bool dragging = phase_ == Phase::Dragging;
    phase_ = Phase::Idle;
    if (dragging) {
        if (GetCapture() == window_) ReleaseCapture();
        Overlay().Hide();
    }
    Reset();
}

void DragSession::Release() {
    Cancel();
    if (!g_commandWindow) return;
    DestroyWindow(g_commandWindow);
    g_commandWindow = nullptr;
}

void DragSession::Reset() {
    phase_ = Phase::Idle;
    pending_ = false;
    window_ = nullptr;
    panel_ = -1;
    origin_ = POINT{};
    g_guides.Clear();
    g_model.Clear();
}

}
