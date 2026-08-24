#include "Log.h"

#include <stdio.h>

namespace dl {

static LOG_HANDLE* g_log = nullptr;

void SetLogHandle(LOG_HANDLE* handle) {
    g_log = handle;
}

#ifdef DL_DEBUG_LOG
static void WriteTrace(const wchar_t* text) {
    static wchar_t logPath[MAX_PATH] = {};
    if (!logPath[0]) {
        wchar_t dir[MAX_PATH] = {};
        if (GetTempPathW(MAX_PATH, dir))
            _snwprintf_s(logPath, _TRUNCATE, L"%sDockLayout.log", dir);
    }
    if (!logPath[0]) return;
    FILE* f = nullptr;
    if (_wfopen_s(&f, logPath, L"a+, ccs=UTF-8") == 0 && f) {
        fwprintf(f, L"%s\n", text);
        fclose(f);
    }
}
#endif

void LogF(const wchar_t* fmt, ...) {
    wchar_t buf[1024];
    va_list ap; va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    if (g_log) g_log->log(g_log, buf);
#ifdef DL_DEBUG_LOG
    WriteTrace(buf);
#endif
}

void LogTraceF(const wchar_t* fmt, ...) {
#ifdef DL_DEBUG_LOG
    wchar_t buf[1024];
    va_list ap; va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    WriteTrace(buf);
#else
    (void)fmt;
#endif
}

void LogWarn(const wchar_t* text) {
    if (g_log) g_log->warn(g_log, text);
}

}
