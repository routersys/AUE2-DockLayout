#include <windows.h>

#include "DragSession.h"
#include "FloatOrigin.h"
#include "FloatWindowHook.h"
#include "GuideWindow.h"
#include "HostContext.h"
#include "HostWindowHook.h"
#include "Log.h"
#include "WindowMenu.h"

#include "plugin2.h"

using namespace dl;

COMMON_PLUGIN_TABLE common_plugin_table = {
    L"ドッキング配置",
    L"ドッキング配置 version 1.1.0",
};

EXTERN_C __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable(void) {
    return &common_plugin_table;
}
EXTERN_C __declspec(dllexport) DWORD RequiredVersion() { return 2010000; }
EXTERN_C __declspec(dllexport) void InitializeLogger(LOG_HANDLE* h) { SetLogHandle(h); }
EXTERN_C __declspec(dllexport) void InitializeConfig(CONFIG_HANDLE* h) { SetConfigHandle(h); }
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD) { return true; }

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    SetHostWindow(host->create_edit_handle()->get_host_app_window());

    Overlay().Create(HostWindow());
    InstallWindowMenuHook();
    InstallHostWindowHook();
    InstallFloatWindowHook();

    LogF(L"ドッキング配置: 有効になりました");
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {
    Drag().Release();
    Overlay().Destroy();
    UninstallFloatWindowHook();
    UninstallHostWindowHook();
    UninstallWindowMenuHook();
    ClearFloatOrigins();
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
