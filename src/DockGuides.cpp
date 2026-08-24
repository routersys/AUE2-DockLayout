#include "DockGuides.h"

#include <utility>

#include "DockPlan.h"
#include "HostContext.h"

namespace dl {

static int ButtonSize() {
    const int base = LayoutSize("TitleHeaderHeight");
    return base > 0 ? base * 2 : 40;
}

static int ButtonGap() {
    return ButtonSize() / 4;
}

static RECT CenteredRect(POINT center, int size) {
    const int half = size / 2;
    return RECT{ center.x - half, center.y - half, center.x - half + size, center.y - half + size };
}

static bool Intersects(const RECT& a, const RECT& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

static POINT CenterOf(const RECT& r) {
    return POINT{ (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
}

static bool Empty(const RECT& r) {
    return r.right <= r.left || r.bottom <= r.top;
}

bool CanGroupOnto(const DockModel& model, int source, int target) {
    const DockPanel* from = model.Get(source);
    const DockPanel* onto = model.Get(target);
    if (!from || !onto) return false;
    if (from->area == kAreaUnknown || onto->area == kAreaUnknown) return false;

    if (from->area == onto->area) {
        if (from->grouped && onto->grouped) return false;
        if (from->grouped || onto->grouped) return true;
        return model.GroupedCountIn(onto->area) == 0;
    }
    if (from->grouped) return true;
    return model.GroupedCountIn(from->area) == 0;
}

bool CanInsertInto(const DockModel& model, int source, int target) {
    const DockPanel* from = model.Get(source);
    const DockPanel* onto = model.Get(target);
    if (!from || !onto) return false;
    return from->stack >= 0 && from->stack == onto->stack &&
           from->area != kAreaUnknown && from->area == onto->area;
}

bool BuildInsertPlan(const DockModel& model, int source, int target, bool after, InsertPlan* plan) {
    const DockPanel* from = model.Get(source);
    const DockPanel* onto = model.Get(target);
    if (!from || !onto || !plan) return false;

    plan->slots.clear();
    for (size_t i = 0; i < model.Panels().size(); i++)
        if (model.Panels()[i].stack == from->stack) plan->slots.push_back(static_cast<int>(i));
    for (size_t i = 1; i < plan->slots.size(); i++)
        for (size_t j = i; j > 0 && model.Panels()[plan->slots[j - 1]].order >
                                    model.Panels()[plan->slots[j]].order; j--)
            std::swap(plan->slots[j - 1], plan->slots[j]);

    int at = -1, to = -1;
    for (size_t i = 0; i < plan->slots.size(); i++) {
        if (plan->slots[i] == source) at = static_cast<int>(i);
        if (plan->slots[i] == target) to = static_cast<int>(i);
    }
    if (at < 0 || to < 0) return false;

    plan->from = at;
    plan->to = after ? (at < to ? to : to + 1) : (at < to ? to - 1 : to);
    return plan->to != at;
}

static bool PlanAreaMove(const DockModel& model, int source, int area, RECT* out) {
    const DockPanel* from = model.Get(source);
    if (!from || from->grouped) return false;
    if (from->area == area) { *out = from->rect; return true; }

    DockPlan plan;
    if (!plan.Load(model)) return false;
    if (!plan.SetArea(source, area)) return false;
    plan.Compute();
    return plan.RectOf(source, out);
}

static bool PlanGroup(const DockModel& model, int source, int target, RECT* out) {
    const DockPanel* from = model.Get(source);
    if (!from || from->grouped) return false;

    DockPlan plan;
    if (!plan.Load(model)) return false;
    if (!plan.Detach(source)) return false;
    plan.Compute();
    return plan.RectOf(target, out);
}

static bool PlanMerge(const DockModel& model, const FloatOrigin& incoming, RECT* out) {
    DockPlan plan;
    if (!plan.Load(model)) return false;
    if (!plan.Add(incoming.left, incoming.top, incoming.right, incoming.bottom, incoming.area))
        return false;
    plan.Compute();
    return plan.RectOf(kIncomingPanel, out);
}

void DockGuides::Clear() {
    buttons_.clear();
    hot_ = -1;
    target_ = DropTarget();
}

void DockGuides::AddAreaCross(const DockModel& model, int source) {
    const RECT& region = model.Region();
    const POINT center = CenterOf(region);
    const int size = ButtonSize();
    const int step = size + ButtonGap();

    const GuideIcon icons[kAreaCount] = {
        GuideIcon::AreaLeft, GuideIcon::AreaRight, GuideIcon::AreaTop,
        GuideIcon::AreaBottom, GuideIcon::AreaCenter,
    };
    const POINT offsets[kAreaCount] = {
        { -step, 0 }, { step, 0 }, { 0, -step }, { 0, step }, { 0, 0 },
    };

    for (int area = 0; area < kAreaCount; area++) {
        GuideButton button = {};
        button.rect = CenteredRect(POINT{ center.x + offsets[area].x, center.y + offsets[area].y }, size);
        button.icon = icons[area];
        button.target.kind = DropKind::Area;
        button.target.area = area;
        button.target.known = PlanAreaMove(model, source, area, &button.target.preview);
        if (!button.target.known) button.target.preview = RECT{};
        buttons_.push_back(button);
    }
}

void DockGuides::AddPanelCluster(const DockModel& model, int source, int hovered) {
    const DockPanel* onto = model.Get(hovered);
    if (!onto) return;

    const bool group = CanGroupOnto(model, source, hovered);
    const bool insert = CanInsertInto(model, source, hovered);
    if (!group && !insert) return;

    const StackAxis axis = model.AxisOf(hovered);
    const int size = ButtonSize();
    const int step = size + ButtonGap();
    POINT center = CenterOf(onto->rect);

    const int span = insert ? size + step * 2 : size;
    RECT cluster = axis == StackAxis::Vertical
        ? RECT{ center.x - size / 2, center.y - span / 2, center.x + size / 2, center.y + span / 2 }
        : RECT{ center.x - span / 2, center.y - size / 2, center.x + span / 2, center.y + size / 2 };

    RECT cross = { 0, 0, 0, 0 };
    if (!buttons_.empty()) {
        cross = buttons_[0].rect;
        for (const GuideButton& button : buttons_) {
            cross.left = min(cross.left, button.rect.left);
            cross.top = min(cross.top, button.rect.top);
            cross.right = max(cross.right, button.rect.right);
            cross.bottom = max(cross.bottom, button.rect.bottom);
        }
    }
    if (!Empty(cross) && Intersects(cluster, cross)) {
        const int lift = (cross.bottom - cross.top) / 2 + (cluster.bottom - cluster.top) / 2 + ButtonGap();
        const int above = center.y - lift;
        const int below = center.y + lift;
        center.y = (above - (cluster.bottom - cluster.top) / 2 >= onto->rect.top) ? above : below;
    }

    const bool vertical = axis == StackAxis::Vertical;
    struct Entry { GuideIcon icon; int offset; DropKind kind; bool after; };
    const Entry entries[] = {
        { vertical ? GuideIcon::InsertUp : GuideIcon::InsertLeft, -step, DropKind::Insert, false },
        { GuideIcon::Group, 0, DropKind::Group, false },
        { vertical ? GuideIcon::InsertDown : GuideIcon::InsertRight, step, DropKind::Insert, true },
    };
    for (const Entry& entry : entries) {
        if (entry.kind == DropKind::Insert && !insert) continue;
        if (entry.kind == DropKind::Group && !group) continue;
        const POINT at = vertical ? POINT{ center.x, center.y + entry.offset }
                                  : POINT{ center.x + entry.offset, center.y };
        GuideButton button = {};
        button.rect = CenteredRect(at, size);
        button.icon = entry.icon;
        button.target.kind = entry.kind;
        button.target.panel = hovered;
        button.target.after = entry.after;
        if (entry.kind == DropKind::Group) {
            button.target.known = PlanGroup(model, source, hovered, &button.target.preview);
            if (!button.target.known) button.target.preview = RECT{};
        }
        if (entry.kind == DropKind::Insert) {
            InsertPlan plan;
            if (BuildInsertPlan(model, source, hovered, entry.after, &plan)) {
                const DockPanel* slot = model.Get(plan.slots[plan.to]);
                if (slot) {
                    button.target.preview = slot->rect;
                    button.target.known = true;
                }
            } else {
                const DockPanel* from = model.Get(source);
                if (from) {
                    button.target.preview = from->rect;
                    button.target.known = true;
                }
            }
        }
        buttons_.push_back(button);
    }
}

void DockGuides::Update(const DockModel& model, int source, POINT cursor, bool inside,
                        const FloatOrigin* incoming) {
    buttons_.clear();
    hot_ = -1;
    target_ = DropTarget();

    if (Empty(model.Region())) return;

    if (source < 0) {
        GuideButton button = {};
        button.rect = CenteredRect(CenterOf(model.Region()), ButtonSize());
        button.icon = GuideIcon::Merge;
        button.target.kind = DropKind::Merge;
        if (incoming && incoming->area != kAreaUnknown)
            button.target.known = PlanMerge(model, *incoming, &button.target.preview);
        if (!button.target.known) button.target.preview = RECT{};
        buttons_.push_back(button);
    } else {
        AddAreaCross(model, source);
        const int hovered = model.IndexAt(cursor);
        if (hovered >= 0 && hovered != source) AddPanelCluster(model, source, hovered);
    }

    if (!inside) {
        if (source >= 0) {
            target_.kind = DropKind::Detach;
            target_.panel = source;
        }
        return;
    }

    for (size_t i = 0; i < buttons_.size(); i++) {
        if (!PtInRect(&buttons_[i].rect, cursor)) continue;
        hot_ = static_cast<int>(i);
        target_ = buttons_[i].target;
        return;
    }
}

}
