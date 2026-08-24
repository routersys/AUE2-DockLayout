#include "HostContext.h"

namespace dl {

static CONFIG_HANDLE* g_config   = nullptr;
static HWND           g_host     = nullptr;
static WNDPROC        g_hostProc = nullptr;

void SetConfigHandle(CONFIG_HANDLE* handle) { g_config = handle; }
void SetHostWindow(HWND window) { g_host = window; }
void SetHostWindowProc(WNDPROC proc) { g_hostProc = proc; }

CONFIG_HANDLE* Config() { return g_config; }
HWND HostWindow() { return g_host; }
WNDPROC HostWindowProc() { return g_hostProc; }

LRESULT CallHostWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return CallWindowProcW(g_hostProc, hwnd, msg, wp, lp);
}

int LayoutSize(const char* key) {
    return g_config ? g_config->get_layout_size(g_config, key) : 0;
}

int ColorCode(const char* key) {
    return g_config ? g_config->get_color_code(g_config, key) : 0;
}

const wchar_t* MenuText(const wchar_t* text) {
    return g_config ? g_config->get_language_text(g_config, L"Menu", text) : text;
}

}
