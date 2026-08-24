#pragma once

#include <windows.h>

#include "config2.h"

namespace dl {

void SetConfigHandle(CONFIG_HANDLE* handle);
void SetHostWindow(HWND window);

CONFIG_HANDLE* Config();
HWND HostWindow();

void SetWindowProc(HWND window, WNDPROC proc);
void ClearWindowProc(HWND window);
WNDPROC OriginalProc(HWND window);
LRESULT CallOriginalProc(HWND window, UINT msg, WPARAM wp, LPARAM lp);

int LayoutSize(const char* key);
int ColorCode(const char* key);
const wchar_t* MenuText(const wchar_t* text);

}
