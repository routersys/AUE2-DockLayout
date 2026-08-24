#pragma once

#include <windows.h>

#include "DockGuides.h"

namespace dl {

class GuideWindow {
public:
    void Create(HWND parent);
    void Destroy();
    void Update(const DockGuides& guides);
    void Hide();

    bool Visible() const;

private:
    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    bool EnsureSurface(int width, int height);
    void ReleaseSurface();

    HWND hwnd_ = nullptr;
    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    BYTE* bits_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

GuideWindow& Overlay();

}
