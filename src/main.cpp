#include "../include/engine_sim_application.h"

#include <iostream>

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    (void)nCmdShow;
    (void)lpCmdLine;
    (void)hPrevInstance;

    // Simple Edition: always run from the program's own folder so the engine
    // files are found no matter how the app was started (e.g. a shortcut).
    char modulePath[MAX_PATH] = { 0 };
    const DWORD length = GetModuleFileNameA(NULL, modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        std::string folder(modulePath, length);
        const size_t slash = folder.find_last_of("\\/");
        if (slash != std::string::npos) {
            folder = folder.substr(0, slash);
            SetCurrentDirectoryA(folder.c_str());
        }
    }

    EngineSimApplication application;
    application.initialize((void *)&hInstance, ysContextObject::DeviceAPI::DirectX11);
    application.run();
    application.destroy();

    return 0;
}
