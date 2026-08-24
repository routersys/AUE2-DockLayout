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

static const int kFloatProbeInset = 2;

static DragSession g_drag;
static DockModel   g_model;
static DockGuides  g_guides;
static PanelLayout g_layout;

DragSession& Drag() { return g_drag; }

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
    return color == style.titleHeader || color == style.grouping;
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
    if (phase_ != Phase::Idle || !HostWindow()) return false;

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
    if (phase_ != Phase::Idle || !window || window == HostWindow()) return false;
    window_ = window;
    origin_ = POINT{};
    panel_ = -1;
    if (!Start()) { Reset(); return false; }
    return true;
}

bool DragSession::Start() {
    panel_ = -1;
    if (!g_model.Build()) return false;
    if (window_ == HostWindow()) {
        panel_ = g_model.IndexAt(origin_);
        if (panel_ < 0) return false;
    }

    g_guides.Clear();
    Overlay().Create(HostWindow());
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
    if (!from->grouped) InvokeWindowCommand(HostWindow(), from->probe, WindowCommand::Group);
    if (from->area != onto->area)
        InvokeWindowCommand(HostWindow(), from->probe, AreaCommand(onto->area));
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
    for (int index = at; index != destination; index += step)
        InvokeWindowCommand(HostWindow(), slots[index]->probe, command);
    LogF(L"ドッキング配置: パネルの並び順を変えました");
}

void DragSession::Execute() {
    const DropTarget target = g_guides.Target();
    switch (target.kind) {
        case DropKind::Area: {
            const DockPanel* from = g_model.Get(panel_);
            if (!from || from->area == target.area) return;
            InvokeWindowCommand(HostWindow(), from->probe, AreaCommand(target.area));
            LogF(L"ドッキング配置: パネルをエリア %d へ移しました", target.area);
            return;
        }
        case DropKind::Group:
            ExecuteGroup(target.panel);
            return;
        case DropKind::Insert:
            ExecuteInsert(target.panel, target.after);
            return;
        case DropKind::Detach: {
            const DockPanel* from = g_model.Get(panel_);
            if (!from) return;
            InvokeWindowCommand(HostWindow(), from->probe, WindowCommand::Detach);
            LogF(L"ドッキング配置: パネルを分離しました");
            return;
        }
        case DropKind::Merge:
            InvokeWindowCommand(window_, POINT{ kFloatProbeInset, kFloatProbeInset },
                                WindowCommand::Merge);
            LogF(L"ドッキング配置: ウィンドウを本体へ戻しました");
            return;
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

void DragSession::Reset() {
    phase_ = Phase::Idle;
    window_ = nullptr;
    panel_ = -1;
    origin_ = POINT{};
    g_guides.Clear();
    g_model.Clear();
}

}
