#include "FloatOrigin.h"

#include <vector>

namespace dl {

struct Entry {
    HWND window;
    FloatOrigin origin;
};

static std::vector<Entry> g_entries;

static bool g_pending = false;
static FloatOrigin g_pendingOrigin;

void SetPendingFloat(const FloatOrigin& origin) {
    g_pending = true;
    g_pendingOrigin = origin;
}

bool TakePendingFloat(FloatOrigin* out) {
    if (!g_pending) return false;
    g_pending = false;
    if (out) *out = g_pendingOrigin;
    return true;
}

void RememberFloat(HWND window, const FloatOrigin& origin) {
    if (!window) return;
    for (Entry& entry : g_entries) {
        if (entry.window != window) continue;
        entry.origin = origin;
        return;
    }
    g_entries.push_back(Entry{ window, origin });
}

void ForgetFloat(HWND window) {
    for (size_t i = 0; i < g_entries.size(); i++) {
        if (g_entries[i].window != window) continue;
        g_entries.erase(g_entries.begin() + i);
        return;
    }
}

bool FloatOriginOf(HWND window, FloatOrigin* out) {
    for (const Entry& entry : g_entries) {
        if (entry.window != window) continue;
        if (out) *out = entry.origin;
        return true;
    }
    return false;
}

void ClearFloatOrigins() {
    g_entries.clear();
    g_pending = false;
}

}
