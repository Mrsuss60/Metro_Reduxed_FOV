#include "globals.h"
#include "g_memory.h"
#include "config_file.h"
#include "hook.h"
#include "console_log.h"
#include <cmath>
#include <thread>
#include <algorithm>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

static inline bool IsSafeMemory(const void* ptr, size_t size = sizeof(float)) {
    if (!ptr) return false;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                               PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE;
    return (mbi.Protect & readable) != 0;
}

float GetAspectRatio() {
    float ar = 1.777778f;
    if (g_ActiveOffsets && g_ActiveOffsets->ViewMFOV) {
        float* ptr = reinterpret_cast<float*>(g_gameBaseAddr + g_ActiveOffsets->ViewMFOV + 0x4);
        if (IsSafeMemory(ptr, sizeof(float)) && *ptr > 0.1f && *ptr < 5.0f) {
            ar = *ptr;
        }
    }
    return ar;
}

float HzToVtFOV(float hFov) {
    float ar = GetAspectRatio();
    return (2.0f * atan(tan((hFov * PI / 180.0f) / 2.0f) / ar)) * (180.0f / PI);
}

float VtToHzFOV(float vFov) {
    float ar = GetAspectRatio();
    return (2.0f * atan(ar * tan((vFov * PI / 180.0f) / 2.0f))) * (180.0f / PI);
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// CameraRenderPass: sub_14069C370 (Steam/GOG) / sub_14068E9B0 (Epic)
typedef int64_t (__fastcall *CameraRenderPass_t)(int64_t a1, char a2);

int64_t __fastcall Hook_CameraRenderPass(int64_t a1, char a2) {
    CameraRenderPass_t orig = g_CameraHook.GetOriginal<CameraRenderPass_t>();
    int64_t ret = orig ? orig(a1, a2) : 0;

    if (g_modEnabled && g_ActiveOffsets && g_ActiveOffsets->ViewMFOV) {
        float* pViewM = reinterpret_cast<float*>(g_gameBaseAddr + g_ActiveOffsets->ViewMFOV);
        if (IsSafeMemory(pViewM, sizeof(float))) {
            float customViewMVt = HzToVtFOV(g_smoothViewM);
            float finalViewMVt = customViewMVt;

            if (g_EnableADSZoom.load()) {
                float engineViewMVt = *pViewM;
                float zoomRatio = engineViewMVt / DEFAULT_WORLD_FOV;

                // Scale custom viewmodel FOV proportionally when aiming down sights
                if (zoomRatio > 0.05f && zoomRatio < 0.98f) {
                    finalViewMVt = customViewMVt * zoomRatio;
                }
            }

            *pViewM = finalViewMVt;
        }
    }

    return ret;
}

static GameOffsets s_DynamicOffsets = {};

bool ResolveDynamicOffsets() {
    MODULEINFO modInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), reinterpret_cast<HMODULE>(g_gameBaseAddr), &modInfo, sizeof(modInfo))) {
        Console::Error("GetModuleInformation failed (%lu)", GetLastError());
        return false;
    }

    size_t imageSize = modInfo.SizeOfImage;
    memset(&s_DynamicOffsets, 0, sizeof(s_DynamicOffsets));
    const uint8_t* pModule = reinterpret_cast<const uint8_t*>(g_gameBaseAddr);

    auto IsValidWorldFOVCandidate = [](uintptr_t target) -> bool {
        if (!target) return false;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery((void*)target, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE))) return false;

        float val = *reinterpret_cast<float*>(target);
        return (val >= 20.0f && val <= 130.0f);
    };

    // WorldFOV cvar: string pointer + 0x10 holds float pointer
    uintptr_t strBaseFov = 0;
    const char* strTarget = "r_base_fov";
    size_t strLen = strlen(strTarget) + 1;

    for (size_t i = 0; i < imageSize - strLen; ++i) {
        if (memcmp(pModule + i, strTarget, strLen) == 0) {
            strBaseFov = g_gameBaseAddr + i;
            break;
        }
    }

    if (strBaseFov) {
        for (size_t i = 0; i < imageSize - 32; i += sizeof(uintptr_t)) {
            uintptr_t val = *reinterpret_cast<const uintptr_t*>(pModule + i);
            if (val == strBaseFov) {
                uintptr_t pCandidate = *reinterpret_cast<const uintptr_t*>(pModule + i + 0x10);
                if (IsValidWorldFOVCandidate(pCandidate)) {
                    s_DynamicOffsets.WorldFOV = pCandidate - g_gameBaseAddr;
                    Console::Fov("WorldFOV @ 0x%p (0x%X)",
                        (void*)pCandidate, (uint32_t)s_DynamicOffsets.WorldFOV);
                    break;
                }
            }
        }
    }

    if (s_DynamicOffsets.WorldFOV == 0) {
        uintptr_t candidateRVAs[] = { g_Steam.WorldFOV, g_Epic.WorldFOV };
        for (uintptr_t rva : candidateRVAs) {
            uintptr_t target = g_gameBaseAddr + rva;
            if (IsValidWorldFOVCandidate(target)) {
                s_DynamicOffsets.WorldFOV = rva;
                Console::Fov("WorldFOV @ 0x%p (0x%X)",
                    (void*)target, (uint32_t)rva);
                break;
            }
        }
    }

    if (s_DynamicOffsets.WorldFOV == 0) {
        return false;
    }

    uintptr_t absWorldFOV = g_gameBaseAddr + s_DynamicOffsets.WorldFOV;
    uintptr_t matchMulSS = 0;

    // mulss xmm?, cs:[WorldFOV] (F3 0F 59)
    for (size_t i = 0; i < imageSize - 8; ++i) {
        if (pModule[i] == 0xF3 && pModule[i + 1] == 0x0F && pModule[i + 2] == 0x59) {
            uint8_t modrm = pModule[i + 3];
            if ((modrm & 0xC7) == 0x05) {
                int32_t disp = *reinterpret_cast<const int32_t*>(&pModule[i + 4]);
                uintptr_t target = reinterpret_cast<uintptr_t>(&pModule[i + 8]) + disp;
                if (target == absWorldFOV) {
                    matchMulSS = g_gameBaseAddr + i;
                    break;
                }
            }
        }
    }

    if (!matchMulSS) {
        // movss [rip+dispViewM], xmm ; movss [reg+68h], xmm
        const char* patCameraFOV = "F3 0F 11 ? ? ? ? ? F3 0F 11 ? 68 00 00 00";
        matchMulSS = FindPatternIDA(g_gameBaseAddr, imageSize, patCameraFOV);
    }

    if (matchMulSS) {
        for (uintptr_t addr = matchMulSS; addr > matchMulSS - 0x1200; --addr) {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(addr);
            if ((*(b - 1) == 0xCC || *(b - 1) == 0xC3 || *(b - 1) == 0x90) &&
                ((b[0] == 0x48 && b[1] == 0x8B && b[2] == 0xC4) ||
                 (b[0] == 0x40 && b[1] == 0x55) ||
                 (b[0] == 0x48 && b[1] == 0x83 && b[2] == 0xEC) ||
                 (b[0] == 0x48 && b[1] == 0x81 && b[2] == 0xEC) ||
                 (b[0] == 0x48 && b[1] == 0x89 && b[2] == 0x5C) ||
                 (b[0] == 0x55 && b[1] == 0x48 && b[2] == 0x83 && b[3] == 0xEC) ||
                 (b[0] == 0x55 && b[1] == 0x48 && b[2] == 0x8B && b[3] == 0xEC)))
            {
                s_DynamicOffsets.CameraRenderPass = addr - g_gameBaseAddr;
                Console::Fov("CameraRenderPass @ 0x%p (0x%X)",
                    (void*)addr, (uint32_t)s_DynamicOffsets.CameraRenderPass);
                break;
            }
        }

        // ViewMFOV: movss [rip+disp], xmm (F3 0F 11)
        uintptr_t absViewMFOV = 0;
        uintptr_t matchViewMStore = 0;

        for (size_t offset = 0; offset < 256; ++offset) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(matchMulSS + offset);
            if (p[0] == 0xF3 && p[1] == 0x0F && p[2] == 0x11) {
                uint8_t modrm = p[3];
                if ((modrm & 0xC7) == 0x05) {
                    int32_t disp = *reinterpret_cast<const int32_t*>(p + 4);
                    uintptr_t target = reinterpret_cast<uintptr_t>(p + 8) + disp;
                    if (target != absWorldFOV && IsSafeMemory((void*)target, sizeof(float))) {
                        absViewMFOV = target;
                        matchViewMStore = matchMulSS + offset;
                        s_DynamicOffsets.ViewMFOV = absViewMFOV - g_gameBaseAddr;
                        Console::Fov("ViewMFOV @ 0x%p (0x%X)",
                            (void*)absViewMFOV, (uint32_t)s_DynamicOffsets.ViewMFOV);
                        break;
                    }
                }
            }
        }

        // ViewM_Inst1: sub_14069B900 (Steam) / sub_14068DED0+2BC (Epic)
        if (absViewMFOV) {
            for (size_t i = 0; i < imageSize - 8; ++i) {
                if (pModule[i] == 0xF3 && pModule[i + 1] == 0x0F && pModule[i + 2] == 0x11) {
                    uint8_t modrm = pModule[i + 3];
                    if ((modrm & 0xC7) == 0x05) {
                        int32_t disp = *reinterpret_cast<const int32_t*>(&pModule[i + 4]);
                        uintptr_t target = reinterpret_cast<uintptr_t>(&pModule[i + 8]) + disp;
                        uintptr_t curInstr = g_gameBaseAddr + i;
                        if (target == absViewMFOV && curInstr != matchViewMStore) {
                            s_DynamicOffsets.ViewM_Inst1 = i;
                            Console::Fov("ViewM_Inst1 @ 0x%p (0x%X)",
                                (void*)curInstr, (uint32_t)s_DynamicOffsets.ViewM_Inst1);
                            break;
                        }
                    }
                }
            }
        }
    }

    if (s_DynamicOffsets.WorldFOV && s_DynamicOffsets.ViewMFOV &&
        s_DynamicOffsets.ViewM_Inst1 && s_DynamicOffsets.CameraRenderPass)
    {
        g_ActiveOffsets = &s_DynamicOffsets;
        return true;
    }

    return false;
}

bool DetectVersion() {
    g_gameBaseAddr = reinterpret_cast<uintptr_t>(GetModuleHandle(NULL));
    if (!g_gameBaseAddr) {
        Console::Error("GetModuleHandle failed");
        return false;
    }

    Console::Fov("Base address: 0x%p", (void*)g_gameBaseAddr);

    if (ResolveDynamicOffsets()) {
        g_CurrentVersion = GameVersion::DYNAMIC;
        Console::Fov("Dynamic offsets resolved");
        return true;
    }

    float* pSteam = reinterpret_cast<float*>(g_gameBaseAddr + g_Steam.WorldFOV);
    bool steamValid = (IsSafeMemory(pSteam, sizeof(float)) && *pSteam >= 10.0f && *pSteam <= 150.0f);

    float* pEpic = reinterpret_cast<float*>(g_gameBaseAddr + g_Epic.WorldFOV);
    bool epicValid = (IsSafeMemory(pEpic, sizeof(float)) && *pEpic >= 10.0f && *pEpic <= 150.0f);

    if (steamValid) {
        g_ActiveOffsets = &g_Steam;
        g_CurrentVersion = GameVersion::STEAM_GOG;
        Console::Fov("Static table: Steam/GOG (0x%X, 0x%X, 0x%X)",
            (uint32_t)g_Steam.WorldFOV, (uint32_t)g_Steam.ViewMFOV, (uint32_t)g_Steam.CameraRenderPass);
        return true;
    }
    else if (epicValid) {
        g_ActiveOffsets = &g_Epic;
        g_CurrentVersion = GameVersion::EPIC;
        Console::Fov("Static table: Epic Games (0x%X, 0x%X, 0x%X)",
            (uint32_t)g_Epic.WorldFOV, (uint32_t)g_Epic.ViewMFOV, (uint32_t)g_Epic.CameraRenderPass);
        return true;
    }

    Console::Error("Failed to find valid WorldFOV pointer");
    return false;
}

void SetHooks(bool enable) {
    if (!g_ActiveOffsets) return;

    void* i1 = g_ActiveOffsets->ViewM_Inst1 ? reinterpret_cast<void*>(g_gameBaseAddr + g_ActiveOffsets->ViewM_Inst1) : nullptr;
    void* pCameraPass = g_ActiveOffsets->CameraRenderPass ? reinterpret_cast<void*>(g_gameBaseAddr + g_ActiveOffsets->CameraRenderPass) : nullptr;

    if (i1 && !IsSafeMemory(i1, 1)) return;
    if (pCameraPass && !IsSafeMemory(pCameraPass, 1)) return;

    if (enable) {
        // NOP ViewM_Inst1: sub_14069B900 / sub_14068DED0+2BC
        if (i1) {
            if (g_bakViewM1.empty()) {
                g_bakViewM1.resize(g_instLen);
                memcpy(g_bakViewM1.data(), i1, g_instLen);
            }
            NopMemory(i1, g_instLen);
        }

        // Hook CameraRenderPass: sub_14069C370 / sub_14068E9B0
        if (pCameraPass && !g_CameraHook.IsInstalled()) {
            g_CameraHook.Install(pCameraPass, reinterpret_cast<void*>(&Hook_CameraRenderPass));
        }

        g_smoothWorld = g_targetWorldHFOV.load();
        g_smoothViewM = g_targetViewMHFOV.load();

        float w = HzToVtFOV(g_targetWorldHFOV.load());
        float v = HzToVtFOV(g_targetViewMHFOV.load());
        if (g_ActiveOffsets->WorldFOV) PatchMemory(reinterpret_cast<void*>(g_gameBaseAddr + g_ActiveOffsets->WorldFOV), &w, sizeof(float));
        if (g_ActiveOffsets->ViewMFOV) PatchMemory(reinterpret_cast<void*>(g_gameBaseAddr + g_ActiveOffsets->ViewMFOV), &v, sizeof(float));
    }
    else {
        // Restore ViewM_Inst1
        if (i1 && !g_bakViewM1.empty()) {
            PatchMemory(i1, g_bakViewM1.data(), g_instLen);
        }

        // Restore default FOV values
        float def = DEFAULT_WORLD_FOV;
        if (g_ActiveOffsets->WorldFOV) PatchMemory(reinterpret_cast<void*>(g_gameBaseAddr + g_ActiveOffsets->WorldFOV), &def, sizeof(float));
        if (g_ActiveOffsets->ViewMFOV) PatchMemory(reinterpret_cast<void*>(g_gameBaseAddr + g_ActiveOffsets->ViewMFOV), &def, sizeof(float));
    }
}

void MainLoop() {
    LoadConfig();
    g_smoothWorld = g_targetWorldHFOV.load();
    g_smoothViewM = g_targetViewMHFOV.load();

    bool ready = DetectVersion();
    if (ready && g_modEnabled) {
        SetHooks(true);
        Console::Fov("Ready (World: %.1f, ViewM: %.1f, ADSZoom: %s)",
            g_targetWorldHFOV.load(), g_targetViewMHFOV.load(), g_EnableADSZoom.load() ? "on" : "off");
    }

    bool lastToggleDown = false;

    while (g_initialized.load()) {
        if (HasConfigChanged()) {
            LoadConfig();
        }

        bool toggleDown = (GetAsyncKeyState(g_ToggleKey) & 0x8000) != 0;
        if (toggleDown && !lastToggleDown) {
            g_modEnabled = !g_modEnabled;
            Console::Fov("Mod %s", g_modEnabled ? "enabled" : "disabled");
            if (ready) SetHooks(g_modEnabled);
        }
        lastToggleDown = toggleDown;

        if (g_modEnabled && ready) {
            float tworld = g_targetWorldHFOV.load();
            float tviewModel = g_targetViewMHFOV.load();
            bool changed = false;

            auto IsPressed = [](int k) { return (GetAsyncKeyState(k) & 0x8000); };
            bool isZooming = IsPressed(g_ZoomHoldKey);

            if (IsPressed(g_World_Inc)) { tworld += g_Step; changed = true; }
            if (IsPressed(g_World_Dec)) { tworld -= g_Step; changed = true; }
            if (IsPressed(g_ViewM_Inc)) { tviewModel += g_Step; changed = true; }
            if (IsPressed(g_ViewM_Dec)) { tviewModel -= g_Step; changed = true; }

            if (changed) {
                g_targetWorldHFOV.store(std::clamp(tworld, g_MinWorldFOV, g_MaxWorldFOV));
                g_targetViewMHFOV.store(std::clamp(tviewModel, g_MinViewMFOV, g_MaxViewMFOV));
                SaveConfig();
            }

            float activeW = tworld;
            float activeV = tviewModel;

            if (isZooming) {
                float z = std::clamp(g_ZoomFOV.load(), g_MinZoomFOV, g_MaxZoomFOV);
                activeW = activeV = z;
            }

            auto Smooth = [](float& curr, float target) {
                if (std::abs(curr - target) > 0.01f) curr = Lerp(curr, target, 0.20f);
                else curr = target;
            };
            Smooth(g_smoothWorld, activeW);
            Smooth(g_smoothViewM, activeV);

            float targetWorldVt = HzToVtFOV(g_smoothWorld);
            float targetViewMVt = HzToVtFOV(g_smoothViewM);

            if (g_ActiveOffsets) {
                if (g_ActiveOffsets->WorldFOV) {
                    PatchMemory(reinterpret_cast<void*>(g_gameBaseAddr + g_ActiveOffsets->WorldFOV), &targetWorldVt, sizeof(float));
                }
                if (g_ActiveOffsets->ViewMFOV && !g_CameraHook.IsInstalled()) {
                    PatchMemory(reinterpret_cast<void*>(g_gameBaseAddr + g_ActiveOffsets->ViewMFOV), &targetViewMVt, sizeof(float));
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        Console::Init();
        Console::Fov("Initializing mod...");

        g_initialized.store(true);
        std::thread(MainLoop).detach();
    }
    else if (r == DLL_PROCESS_DETACH) {
        g_initialized.store(false);
        SetHooks(false);
        if (g_CameraHook.IsInstalled()) {
            g_CameraHook.Remove();
        }
        Console::Shutdown();
    }
    return TRUE;
}