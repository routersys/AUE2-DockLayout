#include "DockGuides.h"

#include "HostContext.h"

namespace dl {

static const int kAreaBandDivisor = 4;

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

static RECT EdgeBand(const RECT& region, int area) {
    const int width = (region.right - region.left) / kAreaBandDivisor;
    const int height = (region.bottom - region.top) / kAreaBandDivisor;
    switch (area) {
        case 0: return RECT{ region.left, region.top, region.left + width, region.bottom };
        case 1: return RECT{ region.right - width, region.top, region.right, region.bottom };
        case 2: return RECT{ region.left, region.top, region.right, region.top + height };
        case 3: return RECT{ region.left, region.bottom - height, region.right, region.bottom };
        default: break;
    }
    return RECT{ region.left, region.top + height, region.right, region.bottom - height };
}

static RECT SideHalf(const RECT& r, StackAxis axis, bool after) {
    RECT half = r;
    if (axis == StackAxis::Vertical) {
        const int middle = (r.top + r.bottom) / 2;
        (after ? half.top : half.bottom) = middle;
    } else {
        const int middle = (r.left + r.right) / 2;
        (after ? half.left : half.right) = middle;
    }
    return half;
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

void DockGuides::Clear() {
    buttons_.clear();
    hot_ = -1;
    target_ = DropTarget();
}

void DockGuides::AddAreaCross(const DockModel& model) {
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
        RECT preview = model.AreaRect(area);
        button.target.preview = Empty(preview) ? EdgeBand(region, area) : preview;
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
        button.target.preview = entry.kind == DropKind::Group
            ? onto->rect
            : SideHalf(onto->rect, axis, entry.after);
        buttons_.push_back(button);
    }
}

void DockGuides::Update(const DockModel& model, int source, POINT cursor, bool inside) {
    buttons_.clear();
    hot_ = -1;
    target_ = DropTarget();

    if (Empty(model.Region())) return;

    if (source < 0) {
        GuideButton button = {};
        button.rect = CenteredRect(CenterOf(model.Region()), ButtonSize());
        button.icon = GuideIcon::Merge;
        button.target.kind = DropKind::Merge;
        button.target.preview = model.Region();
        buttons_.push_back(button);
    } else {
        AddAreaCross(model);
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
