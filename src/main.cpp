#include "settings.h"
#define NOMINMAX
#include <windows.h>
#include "mem.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include "networking.h"
#include "logger.h"


void run_main(HINSTANCE instance) {
#ifdef _DEBUG
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
#endif

    char path[MAX_PATH] = {};
    if(GetModuleFileNameA(instance, path, MAX_PATH) != 0) {
        SetDllPath(path);
        Log(string_format("module_path: %s\n", path), LOG_LEVEL::LOG_ERROR);
    }

    const Settings& s = Settings::Get();
    Mem_Initialize();

    for(auto& web : s.websocket_connections) {
        //Net_ConnectToWebsocket(web);
    }
    //Net_ConnectToWebsocket("ws://localhost:6251");
    Net_StartListening(s.listen_port);


#ifdef _DEBUG
    while(!GetAsyncKeyState(VK_NUMPAD5)) {
        Sleep(100);
    }

    fclose(f);
    FreeConsole();
#endif
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch(fdwReason) { 
        case DLL_PROCESS_ATTACH:
            {
                DisableThreadLibraryCalls(hinstDLL);
                HANDLE thread_handle_value = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)run_main, hinstDLL, 0, 0);
                if(thread_handle_value != INVALID_HANDLE_VALUE) {
                    CloseHandle(thread_handle_value);
                }
                else {
                    MessageBoxA(NULL, "Failed to create Thread", "Elden Tracker", 0);
                }
            }
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
