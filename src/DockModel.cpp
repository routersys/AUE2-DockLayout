#include "DockModel.h"

#include "HostContext.h"
#include "WindowMenu.h"

namespace dl {

static const int kProbeInset = 2;

void DockModel::Clear() {
    panels_.clear();
    region_ = RECT{};
}

bool DockModel::BuildRegion() {
    Clear();
    region_ = DockRegion();
    return region_.right > 0 && region_.bottom > 0;
}

bool DockModel::Build() {
    Clear();
    if (!layout_.Build()) return false;

    region_ = layout_.Region();
    for (const Panel& panel : layout_.Panels()) {
        DockPanel entry = {};
        entry.rect = panel.rect;
        entry.title = panel.title;
        const int top = panel.HasTitle() ? panel.title.top : panel.rect.top;
        entry.probe = POINT{ panel.rect.left + kProbeInset, top + kProbeInset };
        entry.stack = panel.stack;
        entry.order = panel.order;
        entry.area = kAreaUnknown;

        const WindowState state = QueryWindow(HostWindow(), entry.probe);
        if (state.valid) {
            entry.area = state.area;
            entry.grouped = state.grouped;
        }
        panels_.push_back(entry);
    }
    return true;
}

const DockPanel* DockModel::Get(int index) const {
    if (index < 0 || index >= static_cast<int>(panels_.size())) return nullptr;
    return &panels_[index];
}

int DockModel::IndexAt(POINT pt) const {
    for (size_t i = 0; i < panels_.size(); i++) {
        const RECT& r = panels_[i].rect;
        if (pt.x >= r.left && pt.x < r.right && pt.y >= r.top && pt.y < r.bottom)
            return static_cast<int>(i);
    }
    return -1;
}

RECT DockModel::AreaRect(int area) const {
    RECT bounds = {};
    bool found = false;
    for (const DockPanel& panel : panels_) {
        if (panel.area != area) continue;
        if (!found) { bounds = panel.rect; found = true; continue; }
        bounds.left = min(bounds.left, panel.rect.left);
        bounds.top = min(bounds.top, panel.rect.top);
        bounds.right = max(bounds.right, panel.rect.right);
        bounds.bottom = max(bounds.bottom, panel.rect.bottom);
    }
    return bounds;
}

StackAxis DockModel::AxisOf(int index) const {
    const DockPanel* panel = Get(index);
    return layout_.AxisOf(panel ? panel->stack : -1);
}

int DockModel::GroupedCountIn(int area) const {
    int count = 0;
    for (const DockPanel& panel : panels_)
        if (panel.area == area && panel.grouped) count++;
    return count;
}

}
