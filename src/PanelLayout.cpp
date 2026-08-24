#include "PanelLayout.h"

#include <stdlib.h>

#include "HostContext.h"
#include "StyleMetrics.h"

namespace dl {

static const RECT kEmptyBand = {};

struct HeaderStyle {
    int color;
    int height;
};

RECT DockRegion() {
    HWND host = HostWindow();
    RECT client = {};
    if (!host || !GetClientRect(host, &client)) return kEmptyBand;
    RefreshStyle();
    return RECT{ 0, 0, client.right, client.bottom - Style().footerHeight };
}

int PanelLayout::ClassifyRow(int y, int x0, int x1) const {
    const StyleMetrics& style = Style();
    int borders = 0;
    for (int x = x0; x < x1; x++) {
        const int color = surface_.At(x, y);
        if (color == style.windowBorder) borders++;
        else if (color != style.windowSeparator) return Content;
    }
    return borders * 2 >= (x1 - x0) ? Line : Gap;
}

int PanelLayout::ClassifyColumn(int x, int y0, int y1) const {
    const StyleMetrics& style = Style();
    int borders = 0;
    for (int y = y0; y < y1; y++) {
        const int color = surface_.At(x, y);
        if (color == style.windowBorder) borders++;
        else if (color != style.windowSeparator) return Content;
    }
    return borders * 2 >= (y1 - y0) ? Line : Gap;
}

bool PanelLayout::RowMostly(int y, int x0, int x1, int color, bool* full) const {
    int hits = 0;
    for (int x = x0; x < x1; x++)
        if (surface_.At(x, y) == color) hits++;
    *full = hits == (x1 - x0);
    return hits * 2 > (x1 - x0);
}

std::vector<RECT> PanelLayout::SplitBands(const RECT& area, StackAxis axis) const {
    const bool vertical = axis == StackAxis::Vertical;
    const int lo = vertical ? area.top : area.left;
    const int hi = vertical ? area.bottom : area.right;
    const int from = vertical ? area.left : area.top;
    const int to = vertical ? area.right : area.bottom;

    std::vector<RECT> bands;
    int start = lo;
    for (int at = lo + 1; at + 2 < hi; at++) {
        const int kind = vertical ? ClassifyRow(at, from, to) : ClassifyColumn(at, from, to);
        if (kind != Line) continue;
        int gap = 0;
        while (at + 1 + gap < hi &&
               (vertical ? ClassifyRow(at + 1 + gap, from, to)
                         : ClassifyColumn(at + 1 + gap, from, to)) == Gap)
            gap++;
        if (gap == 0 || at + 1 + gap >= hi) continue;
        if ((vertical ? ClassifyRow(at + 1 + gap, from, to)
                      : ClassifyColumn(at + 1 + gap, from, to)) != Line)
            continue;
        RECT band = area;
        (vertical ? band.top : band.left) = start;
        (vertical ? band.bottom : band.right) = at + 1;
        bands.push_back(band);
        start = at + 1 + gap;
        at = start;
    }
    if (bands.empty()) return bands;
    RECT last = area;
    (vertical ? last.top : last.left) = start;
    bands.push_back(last);
    return bands;
}

RECT PanelLayout::TitleBandFrom(const RECT& area, int top) const {
    const StyleMetrics& style = Style();
    const int left = area.left + 1;
    const int right = area.right - 1;
    if (right - left <= 0 || top >= area.bottom) return kEmptyBand;

    const HeaderStyle styles[] = {
        { style.titleHeader,    style.titleHeaderHeight },
        { style.grouping,       style.settingItemHeight },
        { style.groupingHover,  style.settingItemHeight },
        { style.groupingSelect, style.settingItemHeight },
    };
    for (const HeaderStyle& header : styles) {
        int rows = 0;
        bool glyph = false;
        bool full = false;
        while (top + rows < area.bottom && RowMostly(top + rows, left, right, header.color, &full)) {
            if (!full) glyph = true;
            rows++;
        }
        if (rows > 0 && glyph && abs(rows - header.height) <= 1)
            return RECT{ area.left, top, area.right, top + rows };
    }
    return kEmptyBand;
}

int PanelLayout::GroupTabRows(const RECT& area, int top) const {
    const StyleMetrics& style = Style();
    const int left = area.left + 1;
    const int right = area.right - 1;
    const int span = right - left;
    if (span <= 0) return 0;

    int rows = 0;
    while (top + rows < area.bottom) {
        int hits = 0;
        for (int x = left; x < right; x++) {
            const int color = surface_.At(x, top + rows);
            if (color == style.grouping || color == style.groupingHover ||
                color == style.groupingSelect)
                hits++;
        }
        if (hits * 2 <= span) break;
        rows++;
    }
    return rows;
}

RECT PanelLayout::TitleBand(const RECT& area) const {
    const StyleMetrics& style = Style();
    const int top = surface_.At(area.left + 1, area.top) == style.windowBorder ? area.top + 1
                                                                              : area.top;
    RECT band = TitleBandFrom(area, top);
    if (band.bottom <= band.top) {
        const int rows = GroupTabRows(area, top);
        if (abs(rows - (style.groupTabHeight - 2)) > 1) return kEmptyBand;
        band = RECT{ area.left, top, area.right, top + rows };
    }

    const RECT below = TitleBandFrom(area, band.bottom + 1);
    if (below.bottom <= below.top) return band;
    return RECT{ area.left, band.top, area.right, below.bottom };
}

void PanelLayout::Divide(const RECT& area, int stack, int order) {
    for (const StackAxis axis : { StackAxis::Vertical, StackAxis::Horizontal }) {
        const std::vector<RECT> bands = SplitBands(area, axis);
        if (bands.size() < 2) continue;
        const int id = static_cast<int>(stacks_.size());
        stacks_.push_back(axis);
        for (size_t i = 0; i < bands.size(); i++)
            Divide(bands[i], id, static_cast<int>(i));
        return;
    }
    panels_.push_back(Panel{ area, TitleBand(area), stack, order });
}

bool PanelLayout::Build() {
    panels_.clear();
    stacks_.clear();
    region_ = kEmptyBand;

    HWND host = HostWindow();
    RECT client = {};
    if (!host || !GetClientRect(host, &client)) return false;

    const RECT region = DockRegion();
    if (region.right <= 0 || region.bottom <= 0) return false;
    if (!surface_.Capture(host, client.right, client.bottom)) return false;

    region_ = region;
    Divide(region_, -1, 0);
    surface_.Release();
    return !panels_.empty();
}

int PanelLayout::IndexAt(POINT pt) const {
    for (size_t i = 0; i < panels_.size(); i++) {
        const RECT& r = panels_[i].rect;
        if (pt.x >= r.left && pt.x < r.right && pt.y >= r.top && pt.y < r.bottom)
            return static_cast<int>(i);
    }
    return -1;
}

StackAxis PanelLayout::AxisOf(int stack) const {
    if (stack < 0 || stack >= static_cast<int>(stacks_.size())) return StackAxis::Vertical;
    return stacks_[stack];
}

}
