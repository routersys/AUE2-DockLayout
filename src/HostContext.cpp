#include "HostContext.h"

#include <vector>

namespace dl {

struct Subclassed {
    HWND window;
    WNDPROC proc;
};

static CONFIG_HANDLE* g_config = nullptr;
static HWND           g_host   = nullptr;
static std::vector<Subclassed> g_procs;

void SetConfigHandle(CONFIG_HANDLE* handle) { g_config = handle; }
void SetHostWindow(HWND window) { g_host = window; }

CONFIG_HANDLE* Config() { return g_config; }
HWND HostWindow() { return g_host; }

void SetWindowProc(HWND window, WNDPROC proc) {
    for (Subclassed& entry : g_procs) {
        if (entry.window == window) { entry.proc = proc; return; }
    }
    g_procs.push_back(Subclassed{ window, proc });
}

void ClearWindowProc(HWND window) {
    for (size_t i = 0; i < g_procs.size(); i++) {
        if (g_procs[i].window != window) continue;
        g_procs.erase(g_procs.begin() + i);
        return;
    }
}

WNDPROC OriginalProc(HWND window) {
    for (const Subclassed& entry : g_procs) {
        if (entry.window == window) return entry.proc;
    }
    return nullptr;
}

LRESULT CallOriginalProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    WNDPROC proc = OriginalProc(window);
    return proc ? CallWindowProcW(proc, window, msg, wp, lp)
                : DefWindowProcW(window, msg, wp, lp);
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
