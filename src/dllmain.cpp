#include <windows.h>
#include "config.h"
#include "controls.h"
#include "memory.h"
#include <string>

static std::string GetDllDir(HMODULE hModule) {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(hModule, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    return std::string(path);
}

static DWORD WINAPI InitThread(LPVOID lpParam) {
    HMODULE hModule = (HMODULE)lpParam;
    std::string dir = GetDllDir(hModule);
    std::string iniPath = dir + "ErFluidControls.ini";

    LoadConfig(iniPath);
    if (g_config.logging) {
        SetLogFilePath(dir + "ErFluidControls.log");
        LogMessage("[ErFluidControls] Initialized with config from %s", iniPath.c_str());
    }
    StartControlsService();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, InitThread, hModule, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        // On process exit the other threads are already gone; joining them under
        // the loader lock can hang shutdown. Clean up only on explicit FreeLibrary.
        if (lpReserved == nullptr) StopControlsService();
        break;
    }
    return TRUE;
}
