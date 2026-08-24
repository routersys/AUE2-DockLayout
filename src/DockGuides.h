#pragma once

#include <windows.h>
#include <vector>

#include "DockModel.h"
#include "WindowMenu.h"

namespace dl {

enum class DropKind { None, Area, Group, Insert, Detach, Merge };

enum class GuideIcon {
    AreaLeft, AreaRight, AreaTop, AreaBottom, AreaCenter,
    InsertLeft, InsertRight, InsertUp, InsertDown, Group, Merge,
};

struct DropTarget {
    DropKind kind = DropKind::None;
    int area = kAreaUnknown;
    int panel = -1;
    bool after = false;
    RECT preview = {};
};

struct GuideButton {
    RECT rect;
    GuideIcon icon;
    DropTarget target;
};

class DockGuides {
public:
    void Update(const DockModel& model, int source, POINT cursor, bool inside);
    void Clear();

    const std::vector<GuideButton>& Buttons() const { return buttons_; }
    int Hot() const { return hot_; }
    const DropTarget& Target() const { return target_; }

private:
    void AddAreaCross(const DockModel& model);
    void AddPanelCluster(const DockModel& model, int source, int hovered);

    std::vector<GuideButton> buttons_;
    int hot_ = -1;
    DropTarget target_;
};

bool CanGroupOnto(const DockModel& model, int source, int target);
bool CanInsertInto(const DockModel& model, int source, int target);

}
