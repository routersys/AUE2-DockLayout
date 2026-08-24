#include "HostWindowHook.h"

#include <windows.h>
#include <windowsx.h>

#include "DragSession.h"
#include "GuideWindow.h"
#include "HostContext.h"

namespace dl {

static bool Involved(HWND hwnd) {
    return Drag().Window() == hwnd && (Drag().Holding() || Drag().Dragging());
}

static POINT ScreenPoint(HWND hwnd, LPARAM lp) {
    POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    ClientToScreen(hwnd, &pt);
    return pt;
}

static LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_LBUTTONDOWN:
            if (Drag().BeginPanel(POINT{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) })) return 0;
            break;

        case WM_MOUSEMOVE:
            if (Involved(hwnd)) { Drag().Track(ScreenPoint(hwnd, lp)); return 0; }
            break;

        case WM_LBUTTONUP:
            if (Involved(hwnd)) { Drag().Commit(ScreenPoint(hwnd, lp)); return 0; }
            break;

        case WM_CAPTURECHANGED:
            if (Drag().Dragging()) Drag().Cancel();
            break;

        case WM_KEYDOWN:
            if (wp == VK_ESCAPE && Involved(hwnd)) { Drag().Cancel(); return 0; }
            break;

        case WM_DESTROY:
            Drag().Cancel();
            Overlay().Destroy();
            break;
    }
    return CallOriginalProc(hwnd, msg, wp, lp);
}

void InstallHostWindowHook() {
    HWND host = HostWindow();
    if (!host) return;
    SetWindowProc(host, (WNDPROC)SetWindowLongPtrW(host, GWLP_WNDPROC, (LONG_PTR)HostProc));
}

void UninstallHostWindowHook() {
    HWND host = HostWindow();
    WNDPROC original = host ? OriginalProc(host) : nullptr;
    if (!original) return;
    SetWindowLongPtrW(host, GWLP_WNDPROC, (LONG_PTR)original);
    ClearWindowProc(host);
}

}
