#include "GuideWindow.h"

#include <string.h>

#include "HostContext.h"

namespace dl {

static const wchar_t* const kClassName = L"DockLayoutGuide";
static const int kPreviewFillAlpha = 72;
static const int kPreviewEdgeAlpha = 224;
static const int kButtonAlpha = 236;
static const int kIconAlpha = 224;
static const int kPreviewEdgeWidth = 2;

namespace {

class Canvas {
public:
    Canvas(BYTE* bits, int width, int height) : bits_(bits), width_(width), height_(height) {}

    void Blend(int x, int y, int color, int alpha) {
        if (x < 0 || y < 0 || x >= width_ || y >= height_ || alpha <= 0) return;
        BYTE* p = bits_ + (static_cast<size_t>(y) * width_ + x) * 4;
        const int inverse = 255 - alpha;
        const int channels[3] = { color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF };
        for (int i = 0; i < 3; i++)
            p[i] = static_cast<BYTE>((channels[i] * alpha + p[i] * inverse) / 255);
        p[3] = static_cast<BYTE>(alpha + p[3] * inverse / 255);
    }

    void Fill(const RECT& area, int color, int alpha) {
        for (int y = max(0, (int)area.top); y < min(height_, (int)area.bottom); y++)
            for (int x = max(0, (int)area.left); x < min(width_, (int)area.right); x++)
                Blend(x, y, color, alpha);
    }

    void Frame(const RECT& area, int thickness, int color, int alpha) {
        Fill(RECT{ area.left, area.top, area.right, area.top + thickness }, color, alpha);
        Fill(RECT{ area.left, area.bottom - thickness, area.right, area.bottom }, color, alpha);
        Fill(RECT{ area.left, area.top + thickness, area.left + thickness, area.bottom - thickness }, color, alpha);
        Fill(RECT{ area.right - thickness, area.top + thickness, area.right, area.bottom - thickness }, color, alpha);
    }

    void Triangle(POINT a, POINT b, POINT c, int color, int alpha) {
        const int left = min(a.x, min(b.x, c.x));
        const int right = max(a.x, max(b.x, c.x));
        const int top = min(a.y, min(b.y, c.y));
        const int bottom = max(a.y, max(b.y, c.y));
        for (int y = top; y <= bottom; y++) {
            for (int x = left; x <= right; x++) {
                const int e0 = Edge(a, b, x, y);
                const int e1 = Edge(b, c, x, y);
                const int e2 = Edge(c, a, x, y);
                if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0))
                    Blend(x, y, color, alpha);
            }
        }
    }

private:
    static int Edge(POINT from, POINT to, int x, int y) {
        return (to.x - from.x) * (y - from.y) - (to.y - from.y) * (x - from.x);
    }

    BYTE* bits_;
    int width_;
    int height_;
};

RECT Band(const RECT& box, GuideIcon icon) {
    const int width = box.right - box.left;
    const int height = box.bottom - box.top;
    switch (icon) {
        case GuideIcon::AreaLeft:   return RECT{ box.left, box.top, box.left + width / 3, box.bottom };
        case GuideIcon::AreaRight:  return RECT{ box.right - width / 3, box.top, box.right, box.bottom };
        case GuideIcon::AreaTop:    return RECT{ box.left, box.top, box.right, box.top + height / 3 };
        case GuideIcon::AreaBottom: return RECT{ box.left, box.bottom - height / 3, box.right, box.bottom };
        default: break;
    }
    return RECT{ box.left, box.top + height / 3, box.right, box.bottom - height / 3 };
}

void PaintArrow(Canvas& canvas, const RECT& box, GuideIcon icon, int color) {
    const POINT center = { (box.left + box.right) / 2, (box.top + box.bottom) / 2 };
    const int reach = min(box.right - box.left, box.bottom - box.top) / 2;
    switch (icon) {
        case GuideIcon::InsertUp:
            canvas.Triangle(POINT{ center.x, center.y - reach },
                            POINT{ center.x - reach, center.y + reach },
                            POINT{ center.x + reach, center.y + reach }, color, kIconAlpha);
            return;
        case GuideIcon::InsertDown:
            canvas.Triangle(POINT{ center.x, center.y + reach },
                            POINT{ center.x - reach, center.y - reach },
                            POINT{ center.x + reach, center.y - reach }, color, kIconAlpha);
            return;
        case GuideIcon::InsertLeft:
            canvas.Triangle(POINT{ center.x - reach, center.y },
                            POINT{ center.x + reach, center.y - reach },
                            POINT{ center.x + reach, center.y + reach }, color, kIconAlpha);
            return;
        default:
            canvas.Triangle(POINT{ center.x + reach, center.y },
                            POINT{ center.x - reach, center.y - reach },
                            POINT{ center.x - reach, center.y + reach }, color, kIconAlpha);
            return;
    }
}

void PaintIcon(Canvas& canvas, const RECT& button, GuideIcon icon, int color) {
    const int inset = (button.right - button.left) / 5;
    const RECT box = { button.left + inset, button.top + inset,
                       button.right - inset, button.bottom - inset };
    switch (icon) {
        case GuideIcon::AreaLeft:
        case GuideIcon::AreaRight:
        case GuideIcon::AreaTop:
        case GuideIcon::AreaBottom:
        case GuideIcon::AreaCenter:
            canvas.Frame(box, 1, color, kIconAlpha);
            canvas.Fill(Band(box, icon), color, kIconAlpha);
            return;
        case GuideIcon::Group: {
            const int shift = inset / 2 + 1;
            canvas.Frame(RECT{ box.left, box.top, box.right - shift, box.bottom - shift }, 1, color, kIconAlpha);
            canvas.Fill(RECT{ box.left + shift, box.top + shift, box.right, box.bottom }, color, kIconAlpha);
            return;
        }
        case GuideIcon::Merge: {
            canvas.Frame(box, 1, color, kIconAlpha);
            canvas.Fill(RECT{ box.left + inset, box.top + inset,
                              box.right - inset, box.bottom - inset }, color, kIconAlpha);
            return;
        }
        default:
            PaintArrow(canvas, box, icon, color);
            return;
    }
}

void Paint(Canvas& canvas, const DockGuides& guides) {
    const int accent = ColorCode("BorderFocus");
    const int body = ColorCode("ButtonBody");
    const int bodyHot = ColorCode("ButtonBodySelect");
    const int border = ColorCode("WindowBorder");
    const int text = ColorCode("Text");

    const DropTarget& target = guides.Target();
    const RECT& preview = target.preview;
    if (target.kind != DropKind::None && target.kind != DropKind::Detach &&
        preview.right > preview.left && preview.bottom > preview.top) {
        canvas.Fill(preview, accent, kPreviewFillAlpha);
        canvas.Frame(preview, kPreviewEdgeWidth, accent, kPreviewEdgeAlpha);
    }

    const std::vector<GuideButton>& buttons = guides.Buttons();
    for (size_t i = 0; i < buttons.size(); i++) {
        const bool hot = static_cast<int>(i) == guides.Hot();
        canvas.Fill(buttons[i].rect, hot ? bodyHot : body, kButtonAlpha);
        canvas.Frame(buttons[i].rect, 1, border, 255);
        PaintIcon(canvas, buttons[i].rect, buttons[i].icon, text);
    }
}

}

static GuideWindow g_overlay;

GuideWindow& Overlay() { return g_overlay; }

LRESULT CALLBACK GuideWindow::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void GuideWindow::Create(HWND parent) {
    if (hwnd_ || !parent) return;
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpfnWndProc = Proc;
        wc.lpszClassName = kClassName;
        RegisterClassExW(&wc);
        registered = true;
    }
    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST |
                                WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                            kClassName, L"", WS_POPUP, 0, 0, 1, 1,
                            parent, nullptr, GetModuleHandleW(nullptr), nullptr);
}

void GuideWindow::Destroy() {
    ReleaseSurface();
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
}

bool GuideWindow::Visible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void GuideWindow::Hide() {
    if (Visible()) ShowWindow(hwnd_, SW_HIDE);
}

void GuideWindow::ReleaseSurface() {
    if (dc_ && previous_) SelectObject(dc_, previous_);
    if (bitmap_) DeleteObject(bitmap_);
    if (dc_) DeleteDC(dc_);
    dc_ = nullptr;
    bitmap_ = nullptr;
    previous_ = nullptr;
    bits_ = nullptr;
    width_ = 0;
    height_ = 0;
}

bool GuideWindow::EnsureSurface(int width, int height) {
    if (bits_ && width_ == width && height_ == height) return true;
    ReleaseSurface();
    if (width <= 0 || height <= 0) return false;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    dc_ = CreateCompatibleDC(screen);
    void* bits = nullptr;
    bitmap_ = dc_ ? CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    ReleaseDC(nullptr, screen);
    if (!dc_ || !bitmap_ || !bits) { ReleaseSurface(); return false; }

    previous_ = SelectObject(dc_, bitmap_);
    bits_ = static_cast<BYTE*>(bits);
    width_ = width;
    height_ = height;
    return true;
}

void GuideWindow::Update(const DockGuides& guides) {
    if (!hwnd_ || !HostWindow()) return;
    RECT client = {};
    if (!GetClientRect(HostWindow(), &client)) return;
    if (!EnsureSurface(client.right, client.bottom)) return;
    POINT origin = { 0, 0 };
    ClientToScreen(HostWindow(), &origin);

    memset(bits_, 0, static_cast<size_t>(width_) * height_ * 4);
    Canvas canvas(bits_, width_, height_);
    Paint(canvas, guides);

    SetWindowPos(hwnd_, HWND_TOPMOST, origin.x, origin.y, width_, height_,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    SIZE size = { width_, height_ };
    POINT source = { 0, 0 };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd_, nullptr, nullptr, &size, dc_, &source, 0, &blend, ULW_ALPHA);

    if (!IsWindowVisible(hwnd_)) ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

}
