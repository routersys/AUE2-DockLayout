#pragma once

#include <windows.h>

#include "config2.h"

namespace dl {

void SetConfigHandle(CONFIG_HANDLE* handle);
void SetHostWindow(HWND window);
void SetHostWindowProc(WNDPROC proc);

CONFIG_HANDLE* Config();
HWND HostWindow();
WNDPROC HostWindowProc();
LRESULT CallHostWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

int LayoutSize(const char* key);
int ColorCode(const char* key);
const wchar_t* MenuText(const wchar_t* text);

}
