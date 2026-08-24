#pragma once

#include <windows.h>

namespace dl {

struct FloatOrigin {
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;
    int area = -1;
};

void SetPendingFloat(const FloatOrigin& origin);
bool TakePendingFloat(FloatOrigin* out);

void RememberFloat(HWND window, const FloatOrigin& origin);
void ForgetFloat(HWND window);
bool FloatOriginOf(HWND window, FloatOrigin* out);
void ClearFloatOrigins();

}
