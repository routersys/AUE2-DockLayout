#include <windows.h>

#include "plugin2.h"

COMMON_PLUGIN_TABLE common_plugin_table = {
    L"ドッキング配置",
    L"ドッキング配置 version 1.0.0",
};

EXTERN_C __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable(void) {
    return &common_plugin_table;
}
EXTERN_C __declspec(dllexport) DWORD RequiredVersion() { return 2010000; }
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD) { return true; }

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE*) {
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
