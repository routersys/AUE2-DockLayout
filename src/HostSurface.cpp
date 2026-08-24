#include "HostSurface.h"

namespace dl {

bool HostSurface::Capture(HWND window, int width, int height) {
    Release();
    if (!window || width <= 0 || height <= 0) return false;

    HDC hostDc = GetDC(window);
    if (!hostDc) return false;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC memDc = CreateCompatibleDC(hostDc);
    HBITMAP bmp = CreateDIBSection(hostDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool captured = false;
    if (memDc && bmp && bits) {
        HGDIOBJ old = SelectObject(memDc, bmp);
        if (BitBlt(memDc, 0, 0, width, height, hostDc, 0, 0, SRCCOPY)) {
            GdiFlush();
            const BYTE* src = static_cast<const BYTE*>(bits);
            pixels_.assign(src, src + static_cast<size_t>(width) * height * 4);
            width_ = width;
            height_ = height;
            captured = true;
        }
        SelectObject(memDc, old);
    }
    if (bmp) DeleteObject(bmp);
    if (memDc) DeleteDC(memDc);
    ReleaseDC(window, hostDc);
    return captured;
}

void HostSurface::Release() {
    pixels_.clear();
    width_ = 0;
    height_ = 0;
}

int HostSurface::At(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return -1;
    const BYTE* p = pixels_.data() + (static_cast<size_t>(y) * width_ + x) * 4;
    return (p[2] << 16) | (p[1] << 8) | p[0];
}

}
