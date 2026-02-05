#include "Windows.h"
#include <process.h>
#include <fstream>

constexpr char MOD_DLL[] = "MetroReduxedFOV.dll";

typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

namespace {
    DirectInput8Create_t g_originalDirectInput8Create = nullptr;
    HMODULE g_originalDinput8 = nullptr;
    HMODULE g_fovDll = nullptr;

    void LoadOriginalDinput8() {
        if (g_originalDinput8) return;

        wchar_t systemPath[MAX_PATH];
        GetSystemDirectoryW(systemPath, MAX_PATH);
        wsprintfW(systemPath, L"%s\\dinput8.dll", systemPath);

        g_originalDinput8 = LoadLibraryW(systemPath);
        if (g_originalDinput8) {
            g_originalDirectInput8Create = reinterpret_cast<DirectInput8Create_t>(
                GetProcAddress(g_originalDinput8, "DirectInput8Create")
                );
        }
    }

    bool FileExists(const char* name) {
        std::ifstream f(name);
        return f.good();
    }

    unsigned int __stdcall InjectThread(void*) {
        Sleep(5000);

        if (g_fovDll) return 0;

        if (FileExists(MOD_DLL)) {
            g_fovDll = LoadLibraryA(MOD_DLL);
        }

        return 0;
    }

    void Cleanup() {
        if (g_fovDll) FreeLibrary(g_fovDll);
        if (g_originalDinput8) FreeLibrary(g_originalDinput8);
    }
}

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPVOID* ppvOut,
    LPUNKNOWN punkOuter
) {
    if (!g_originalDinput8) LoadOriginalDinput8();

    if (g_originalDirectInput8Create) {
        return g_originalDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    }
    return E_FAIL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        LoadOriginalDinput8();
        _beginthreadex(nullptr, 0, InjectThread, nullptr, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        Cleanup();
        break;
    }
    return TRUE;
}