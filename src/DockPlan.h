#pragma once

#include <windows.h>
#include <vector>

#include "DockModel.h"

namespace dl {

inline constexpr int kIncomingPanel = -2;

struct PlanItem {
    int panel;
    double left;
    double top;
    double right;
    double bottom;
    int area;
    RECT rect;
};

class DockPlan {
public:
    bool Load(const DockModel& model);
    bool SetArea(int panel, int area);
    bool Add(double left, double top, double right, double bottom, int area);
    bool Detach(int panel);
    void Compute();
    bool RectOf(int panel, RECT* out) const;
    bool NormalizedOf(int panel, double* left, double* top, double* right, double* bottom) const;

private:
    std::vector<PlanItem> items_;
    RECT region_ = {};
    int separator_ = 0;
};

int SeparatorWidth();

}
