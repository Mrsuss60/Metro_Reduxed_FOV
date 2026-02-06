#pragma once
#include <windows.h>
#include <atomic>
#include <vector>
#include <string>


#define PI 3.14159265358979323846f

constexpr float DEFAULT_WORLD_FOV = 50.625f;
inline float g_Step = 0.25f;

inline int g_ToggleKey = 0x76;   
inline int g_ZoomHoldKey = 0x06; 
inline int g_World_Inc = 0x68;   
inline int g_World_Dec = 0x65;   
inline int g_ViewM_Inc = 0x67;      
inline int g_ViewM_Dec = 0x64;

inline float g_MinWorldFOV = 10.0f;
inline float g_MaxWorldFOV = 120.0f;

inline float g_MinViewMFOV = 10.0f;
inline float g_MaxViewMFOV = 100.0f; 

inline float g_MinZoomFOV = 10.0f;
inline float g_MaxZoomFOV = 60.0f;

inline std::atomic<float> g_targetWorldHFOV = 90.0f;
inline std::atomic<float> g_targetViewMHFOV = 80.0f;
inline std::atomic<float> g_ZoomFOV = 35.0f;
inline float g_smoothWorld = 90.0f;
inline float g_smoothViewM = 80.0f;

inline std::atomic<bool> g_initialized = false;
inline std::atomic<bool> g_modEnabled = true;
inline time_t g_lastConfigModTime = 0;

inline uintptr_t g_gameBaseAddr = 0;

enum class GameVersion { UNKNOWN, STEAM_GOG, EPIC };
inline GameVersion g_CurrentVersion = GameVersion::UNKNOWN;

struct GameOffsets {
    uintptr_t WorldFOV;
    uintptr_t ViewMFOV;
    uintptr_t ViewM_Inst1;
    uintptr_t ViewM_Inst2;
};

inline GameOffsets g_Steam = {
    0xCE710C, 
    0xD07A18, 
    0x69BBE0, 
    0x69CCAF  
};

inline GameOffsets g_Epic = {
    0xCE09C0, 
    0xCFEE98, 
    0x68E18C, 
    0x68F2FC  
};

inline GameOffsets g_Gog = g_Steam;

inline GameOffsets* g_ActiveOffsets = nullptr;

inline size_t g_instLen = 8;
inline std::vector<BYTE> g_bakViewM1, g_bakViewM2;