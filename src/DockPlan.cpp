#include "DockPlan.h"

#include <algorithm>

#include "StyleMetrics.h"
#include "WindowMenu.h"

namespace dl {

namespace {

const int kBandAreas[3] = { 2, 4, 3 };

const double kRound = 1e-6;

struct Span {
    double low;
    double high;
};

struct Slot {
    int low;
    int high;
};

void Nest(const std::vector<Span>& spans, double start, double total, int separator,
          std::vector<Slot>* out) {
    if (spans.empty()) return;
    if (spans.size() == 1) {
        out->push_back(Slot{ static_cast<int>(start + kRound),
                             static_cast<int>(start + total - separator + kRound) });
        return;
    }
    const double first = spans[0].high - spans[0].low;
    double low = spans[1].low;
    double high = spans[1].high;
    for (size_t i = 2; i < spans.size(); i++) {
        low = min(low, spans[i].low);
        high = max(high, spans[i].high);
    }
    const double rest = high - low;
    const double slot = first + rest > 0 ? total * first / (first + rest)
                                         : total / static_cast<double>(spans.size());
    out->push_back(Slot{ static_cast<int>(start + kRound),
                         static_cast<int>(start + slot - separator + kRound) });
    Nest(std::vector<Span>(spans.begin() + 1, spans.end()), start + slot, total - slot,
         separator, out);
}

std::vector<Slot> Spread(const std::vector<double>& weights, int start, int length, int separator) {
    std::vector<Slot> slots;
    double total = 0;
    for (double weight : weights) total += weight;
    if (total <= 0 || length <= 0) return slots;

    double at = start;
    const double extended = static_cast<double>(length) + separator;
    for (double weight : weights) {
        const double size = weight / total * extended;
        slots.push_back(Slot{ static_cast<int>(at + kRound),
                              static_cast<int>(at + size - separator + kRound) });
        at += size;
    }
    return slots;
}

Span Bounds(const std::vector<PlanItem*>& members, bool vertical) {
    Span span = vertical ? Span{ members[0]->top, members[0]->bottom }
                         : Span{ members[0]->left, members[0]->right };
    for (const PlanItem* item : members) {
        span.low = min(span.low, vertical ? item->top : item->left);
        span.high = max(span.high, vertical ? item->bottom : item->right);
    }
    return span;
}

void SortMembers(std::vector<PlanItem*>& members, bool vertical) {
    std::stable_sort(members.begin(), members.end(),
                     [vertical](const PlanItem* a, const PlanItem* b) {
                         const double al = vertical ? a->top : a->left;
                         const double bl = vertical ? b->top : b->left;
                         if (al != bl) return al < bl;
                         return (vertical ? a->bottom : a->right) < (vertical ? b->bottom : b->right);
                     });
}

void StackVertical(std::vector<PlanItem*>& members, int left, int right,
                   int top, int bottom, int separator) {
    SortMembers(members, true);
    std::vector<double> weights;
    for (const PlanItem* item : members) weights.push_back(item->bottom - item->top);
    const std::vector<Slot> slots = Spread(weights, top, bottom - top, separator);
    for (size_t i = 0; i < slots.size() && i < members.size(); i++)
        members[i]->rect = RECT{ left, slots[i].low, right, slots[i].high };
}

void StackHorizontal(std::vector<PlanItem*>& members, int left, int right,
                     int top, int bottom, int separator) {
    SortMembers(members, false);
    std::vector<double> weights;
    for (const PlanItem* item : members) weights.push_back(item->right - item->left);
    const std::vector<Slot> slots = Spread(weights, left, right - left, separator);
    for (size_t i = 0; i < slots.size() && i < members.size(); i++)
        members[i]->rect = RECT{ slots[i].low, top, slots[i].high, bottom };
}

void LayoutMiddle(const std::vector<PlanItem*>& members, int left, int right,
                  int top, int bottom, int separator) {
    std::vector<PlanItem*> bands[3];
    for (PlanItem* item : members) {
        for (int i = 0; i < 3; i++)
            if (item->area == kBandAreas[i]) bands[i].push_back(item);
    }

    std::vector<Span> spans;
    std::vector<int> present;
    for (int i = 0; i < 3; i++) {
        if (bands[i].empty()) continue;
        spans.push_back(Bounds(bands[i], true));
        present.push_back(i);
    }
    std::vector<Slot> rows;
    Nest(spans, top, static_cast<double>(bottom - top) + separator, separator, &rows);
    for (size_t i = 0; i < rows.size() && i < present.size(); i++)
        StackHorizontal(bands[present[i]], left, right, rows[i].low, rows[i].high, separator);
}

}

int SeparatorWidth() {
    const int size = Style().windowSeparatorSize - 2;
    return size > 0 ? size : 1;
}

bool DockPlan::Load(const DockModel& model) {
    items_.clear();
    region_ = model.Region();
    separator_ = SeparatorWidth();

    const double width = static_cast<double>(region_.right - region_.left) + separator_;
    const double height = static_cast<double>(region_.bottom - region_.top) + separator_;
    if (width <= separator_ || height <= separator_) return false;

    for (size_t i = 0; i < model.Panels().size(); i++) {
        const DockPanel& panel = model.Panels()[i];
        if (panel.area == kAreaUnknown) return false;
        PlanItem item = {};
        item.panel = static_cast<int>(i);
        item.left = (panel.rect.left - region_.left) / width;
        item.top = (panel.rect.top - region_.top) / height;
        item.right = (panel.rect.right - region_.left + separator_) / width;
        item.bottom = (panel.rect.bottom - region_.top + separator_) / height;
        item.area = panel.area;
        items_.push_back(item);
    }
    return !items_.empty();
}

bool DockPlan::SetArea(int panel, int area) {
    for (size_t i = 0; i < items_.size(); i++) {
        if (items_[i].panel != panel) continue;
        const PlanItem moved = items_[i];
        items_.erase(items_.begin() + i);
        items_.push_back(moved);
        items_.back().area = area;
        return true;
    }
    return false;
}

bool DockPlan::Add(double left, double top, double right, double bottom, int area) {
    if (items_.empty()) return false;
    PlanItem item = {};
    item.panel = kIncomingPanel;
    item.left = left;
    item.top = top;
    item.right = right;
    item.bottom = bottom;
    item.area = area;
    items_.push_back(item);
    return true;
}

bool DockPlan::Detach(int panel) {
    for (size_t i = 0; i < items_.size(); i++) {
        if (items_[i].panel != panel) continue;
        items_.erase(items_.begin() + i);
        return true;
    }
    return false;
}

void DockPlan::Compute() {
    for (PlanItem& item : items_) item.rect = RECT{};
    if (items_.empty()) return;

    std::vector<PlanItem*> groups[3];
    for (PlanItem& item : items_) {
        if (item.area == 0) groups[0].push_back(&item);
        else if (item.area == 1) groups[2].push_back(&item);
        else groups[1].push_back(&item);
    }

    std::vector<Span> spans;
    std::vector<int> present;
    for (int i = 0; i < 3; i++) {
        if (groups[i].empty()) continue;
        spans.push_back(Bounds(groups[i], false));
        present.push_back(i);
    }
    std::vector<Slot> columns;
    Nest(spans, region_.left, static_cast<double>(region_.right - region_.left) + separator_,
         separator_, &columns);

    for (size_t i = 0; i < columns.size() && i < present.size(); i++) {
        std::vector<PlanItem*>& members = groups[present[i]];
        if (present[i] == 1)
            LayoutMiddle(members, columns[i].low, columns[i].high,
                         region_.top, region_.bottom, separator_);
        else
            StackVertical(members, columns[i].low, columns[i].high,
                          region_.top, region_.bottom, separator_);
    }
}

bool DockPlan::NormalizedOf(int panel, double* left, double* top,
                            double* right, double* bottom) const {
    for (const PlanItem& item : items_) {
        if (item.panel != panel) continue;
        *left = item.left;
        *top = item.top;
        *right = item.right;
        *bottom = item.bottom;
        return true;
    }
    return false;
}

bool DockPlan::RectOf(int panel, RECT* out) const {
    for (const PlanItem& item : items_) {
        if (item.panel != panel) continue;
        if (item.rect.right <= item.rect.left || item.rect.bottom <= item.rect.top) return false;
        *out = item.rect;
        return true;
    }
    return false;
}

}
