#pragma once

#include <windows.h>
#include <vector>

#include "HostSurface.h"

namespace dl {

RECT DockRegion();

enum class StackAxis { Vertical, Horizontal };

struct Panel {
    RECT rect;
    RECT title;
    int stack;
    int order;

    bool HasTitle() const { return title.bottom > title.top; }
};

class PanelLayout {
public:
    bool Build();

    const RECT& Region() const { return region_; }
    const std::vector<Panel>& Panels() const { return panels_; }
    int IndexAt(POINT pt) const;
    StackAxis AxisOf(int stack) const;

private:
    enum RowClass { Content = -1, Gap = 0, Line = 1 };

    int ClassifyRow(int y, int x0, int x1) const;
    int ClassifyColumn(int x, int y0, int y1) const;
    bool RowMostly(int y, int x0, int x1, int color, bool* full) const;
    std::vector<RECT> SplitBands(const RECT& area, StackAxis axis) const;
    RECT TitleBandFrom(const RECT& area, int top) const;
    RECT TitleBand(const RECT& area) const;
    void Divide(const RECT& area, int stack, int order);

    HostSurface surface_;
    RECT region_ = {};
    std::vector<Panel> panels_;
    std::vector<StackAxis> stacks_;
};

}
