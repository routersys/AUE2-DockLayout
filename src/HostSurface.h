#pragma once

#include <windows.h>
#include <vector>

namespace dl {

class HostSurface {
public:
    bool Capture(HWND window, int width, int height);
    void Release();

    int Width() const { return width_; }
    int Height() const { return height_; }
    int At(int x, int y) const;

private:
    std::vector<BYTE> pixels_;
    int width_ = 0;
    int height_ = 0;
};

}
