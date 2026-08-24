#pragma once

#include <windows.h>
#include <vector>

#include "PanelLayout.h"

namespace dl {

struct DockPanel {
    RECT rect;
    RECT title;
    POINT probe;
    int stack;
    int order;
    int area;
    bool grouped;

    bool HasTitle() const { return title.bottom > title.top; }
};

class DockModel {
public:
    bool Build();
    bool BuildRegion();
    void Clear();

    const RECT& Region() const { return region_; }
    const std::vector<DockPanel>& Panels() const { return panels_; }
    const DockPanel* Get(int index) const;

    int IndexAt(POINT pt) const;
    RECT AreaRect(int area) const;
    StackAxis AxisOf(int index) const;
    int GroupedCountIn(int area) const;

private:
    PanelLayout layout_;
    RECT region_ = {};
    std::vector<DockPanel> panels_;
};

}
