#include "DragSession.h"

#include <stdlib.h>
#include <vector>

#include "DockGuides.h"
#include "DockModel.h"
#include "DockPlan.h"
#include "FloatOrigin.h"
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

static void TraceGuides() {
    const std::vector<GuideButton>& buttons = g_guides.Buttons();
    for (size_t i = 0; i < buttons.size(); i++) {
        const GuideButton& button = buttons[i];
        LogTraceF(L"guide %d kind %d button %d %d %d %d known %d preview %d %d %d %d",
                  static_cast<int>(i), static_cast<int>(button.target.kind),
                  button.rect.left, button.rect.top, button.rect.right, button.rect.bottom,
                  button.target.known ? 1 : 0,
                  button.target.preview.left, button.target.preview.top,
                  button.target.preview.right, button.target.preview.bottom);
    }
    LogTraceF(L"guide end hot %d", g_guides.Hot());
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
    hasOrigin_ = false;
    if (window_ == HostWindow()) {
        if (!g_model.Build()) return false;
        panel_ = g_model.IndexAt(origin_);
        if (panel_ < 0) return false;
    } else {
        const bool built = g_model.Build();
        if (built) hasOrigin_ = FloatOriginOf(window_, &floatOrigin_);
        if (!built && !g_model.BuildRegion()) return false;
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
    g_guides.Update(g_model, panel_, hostPt, inside, hasOrigin_ ? &floatOrigin_ : nullptr);
    TraceGuides();
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
    InsertPlan plan;
    if (!from || !BuildInsertPlan(g_model, panel_, target, after, &plan)) return;

    const int step = plan.to < plan.from ? -1 : 1;
    const WindowCommand command = step < 0 ? WindowCommand::MoveUp : WindowCommand::MoveDown;
    const POINT grab = { origin_.x - from->rect.left, origin_.y - from->rect.top };
    for (int index = plan.from; index != plan.to; index += step) {
        const DockPanel* slot = g_model.Get(plan.slots[index]);
        if (!slot) return;
        const POINT point = { min(slot->rect.left + grab.x, slot->rect.right - 1),
                              min(slot->rect.top + grab.y, slot->rect.bottom - 1) };
        InvokeWindowCommand(HostWindow(), point, command);
    }
    LogF(L"ドッキング配置: パネルの並び順を変えました");
}

void DragSession::ExecuteDetach() {
    const DockPanel* from = g_model.Get(panel_);
    if (!from) return;

    DockPlan plan;
    FloatOrigin origin;
    const bool known = plan.Load(g_model) &&
                       plan.NormalizedOf(panel_, &origin.left, &origin.top,
                                         &origin.right, &origin.bottom);
    origin.area = from->area;

    if (!InvokeWindowCommand(HostWindow(), origin_, WindowCommand::Detach)) return;
    LogF(L"ドッキング配置: パネルを分離しました");
    if (known) SetPendingFloat(origin);
}

void DragSession::Execute() {
    const DropTarget target = g_guides.Target();
    LogTraceF(L"drop kind %d known %d preview %d %d %d %d",
              static_cast<int>(target.kind), target.known ? 1 : 0,
              target.preview.left, target.preview.top,
              target.preview.right, target.preview.bottom);
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
            ExecuteDetach();
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
                ForgetFloat(window_);
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
