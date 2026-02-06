#include "globals.h"
#include "g_memory.h"
#include "config_file.h"
#include <cmath>
#include <thread>
#include <algorithm>

float HzToVtFOV(float hFov) {
    float ar = 1.777f;
    if (g_ActiveOffsets) {
        float* ptr = (float*)(g_gameBaseAddr + g_ActiveOffsets->ViewMFOV + 0x4);
        if (!IsBadReadPtr(ptr, sizeof(float)) && *ptr > 0.1f && *ptr < 5.0f) ar = *ptr;
    }
    return (2.0f * atan(tan((hFov * PI / 180.0f) / 2.0f) / ar)) * (180.0f / PI);
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

void ApplyValues() {
    if (!g_ActiveOffsets) return;
    float w = HzToVtFOV(g_smoothWorld);
    float v = HzToVtFOV(g_smoothViewM);
    PatchMemory((void*)(g_gameBaseAddr + g_ActiveOffsets->WorldFOV), &w, sizeof(float));
    PatchMemory((void*)(g_gameBaseAddr + g_ActiveOffsets->ViewMFOV), &v, sizeof(float));
}

bool DetectVersion() {
    g_gameBaseAddr = (uintptr_t)GetModuleHandle(NULL);
    if (!g_gameBaseAddr) return false;

    auto Check = [](GameOffsets& g) -> bool {
        float* p = (float*)(g_gameBaseAddr + g.WorldFOV);
        if (IsBadReadPtr(p, 4) || *p < 40.0f || *p > 150.0f) return false;
        g_ActiveOffsets = &g;
        return true;
        };

    if (Check(g_Steam)) return true;
    if (Check(g_Epic)) return true;

    return false;
}

void SetHooks(bool enable) {
    if (!g_ActiveOffsets) return;
    void* i1 = (void*)(g_gameBaseAddr + g_ActiveOffsets->ViewM_Inst1);
    void* i2 = (void*)(g_gameBaseAddr + g_ActiveOffsets->ViewM_Inst2);

    if (IsBadReadPtr(i1, 1) || IsBadReadPtr(i2, 1)) return;

    if (enable) {
        if (g_bakViewM1.empty()) { 
            g_bakViewM1.resize(g_instLen); memcpy(g_bakViewM1.data(), i1, g_instLen);
            g_bakViewM2.resize(g_instLen); memcpy(g_bakViewM2.data(), i2, g_instLen);
        }
        NopMemory(i1, g_instLen);
        NopMemory(i2, g_instLen);
    }
    else {
        if (!g_bakViewM1.empty()) PatchMemory(i1, g_bakViewM1.data(), g_instLen);
        if (!g_bakViewM2.empty()) PatchMemory(i2, g_bakViewM2.data(), g_instLen);

        float def = DEFAULT_WORLD_FOV;
        PatchMemory((void*)(g_gameBaseAddr + g_ActiveOffsets->WorldFOV), &def, 4);
        PatchMemory((void*)(g_gameBaseAddr + g_ActiveOffsets->ViewMFOV), &def, 4);
    }
}

void MainLoop() {
    LoadConfig();
    g_smoothWorld = g_targetWorldHFOV.load();
    g_smoothViewM = g_targetViewMHFOV.load();

    bool ready = DetectVersion();
    if (ready && g_modEnabled) SetHooks(true);

    static bool loggedAR = false;
    static DWORD startTime = GetTickCount64();

    while (g_initialized.load()) {
        if (HasConfigChanged()) { LoadConfig(); }


        if (!loggedAR && ready && (GetTickCount64() - startTime > 15000)) {
            if (g_ActiveOffsets) {
                float* ptr = (float*)(g_gameBaseAddr + g_ActiveOffsets->ViewMFOV + 0x4);
                if (!IsBadReadPtr(ptr, 4)) {
                    loggedAR = true;
                }
            }
        }

        if (GetAsyncKeyState(g_ToggleKey) & 1) {
            g_modEnabled = !g_modEnabled;
            if (ready) SetHooks(g_modEnabled);
        }

        if (g_modEnabled && ready) {
            float tworld = g_targetWorldHFOV.load();
            float tviewModel = g_targetViewMHFOV.load();
            bool changed = false;

            auto IsPressed = [](int k) { return (GetAsyncKeyState(k) & 0x8000); };

            if (IsPressed(g_World_Inc)) { tworld += g_Step; changed = true; }
            if (IsPressed(g_World_Dec)) { tworld -= g_Step; changed = true; }

            if (IsPressed(g_ViewM_Inc)) { tviewModel += g_Step; changed = true; }
            if (IsPressed(g_ViewM_Dec)) { tviewModel -= g_Step; changed = true; }

            if (changed) {
                g_targetWorldHFOV.store(std::clamp(tworld, g_MinWorldFOV, g_MaxWorldFOV));
                g_targetViewMHFOV.store(std::clamp(tviewModel, g_MinViewMFOV, g_MaxViewMFOV));
                SaveConfig();
            }

            float activeW = tworld, activeV = tviewModel;
            if (IsPressed(g_ZoomHoldKey)) {
                float z = std::clamp(g_ZoomFOV.load(), g_MinZoomFOV, g_MaxZoomFOV);
                activeW = activeV = z;
            }

            auto Smooth = [](float& curr, float target) {
                if (abs(curr - target) > 0.01f) curr = Lerp(curr, target, 0.20f);
                else curr = target;
                };
            Smooth(g_smoothWorld, activeW);
            Smooth(g_smoothViewM, activeV);

            ApplyValues();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) {
        g_initialized.store(true);
        std::thread(MainLoop).detach();
    }
    else if (r == DLL_PROCESS_DETACH) {
        g_initialized.store(false);
        if (!g_bakViewM1.empty()) SetHooks(false);
    }
    return TRUE;
}