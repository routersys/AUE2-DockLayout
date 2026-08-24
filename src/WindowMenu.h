#pragma once

#include <windows.h>

namespace dl {

enum class WindowCommand {
    AreaLeft, AreaRight, AreaTop, AreaBottom, AreaCenter,
    MoveFirst, MoveUp, MoveDown, MoveLast,
    Detach, Group, Merge, Close,
};

inline constexpr int kAreaCount = 5;
inline constexpr int kAreaUnknown = -1;

struct WindowState {
    bool valid = false;
    int area = kAreaUnknown;
    bool grouped = false;
    bool canMove = false;
};

void InstallWindowMenuHook();
void UninstallWindowMenuHook();

WindowCommand AreaCommand(int area);

WindowState QueryWindow(HWND window, POINT clientPt);
bool InvokeWindowCommand(HWND window, POINT clientPt, WindowCommand command);

}
