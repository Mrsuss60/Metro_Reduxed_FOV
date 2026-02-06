#include "globals.h"
#include <fstream>
#include <string>
#include <algorithm> 

static std::string GetPath() { return "MetroReduxedFOV.cfg"; }

bool HasConfigChanged() {
    struct stat st;
    if (stat(GetPath().c_str(), &st) == 0) {
        if (st.st_mtime != g_lastConfigModTime) {
            g_lastConfigModTime = st.st_mtime;
            return true;
        }
    }
    return false;
}

void SaveConfig() {
    std::ofstream f(GetPath());
    if (!f.is_open()) return;

    f << "### Metro Reduxed FOV Config ###\n\n";

    f << "# Stepping Factor (Set how fast FOV changes)\n";
    f << "Step=" << g_Step << "\n";
    f << "ToggleMOD_KEY=0x" << std::hex << std::uppercase << g_ToggleKey << std::dec << "\n\n";

    f << "# --- World FOV ---\n";
    f << "MinWorldFOV=" << g_MinWorldFOV << "\n";
    f << "MaxWorldFOV=" << g_MaxWorldFOV << "\n";
    f << "WorldHorizontalFOV=" << g_targetWorldHFOV.load() << "\n";
    f << "\n";
    f << "WorldFOV_IncreaseKEY=0x" << std::hex << std::uppercase << g_World_Inc << std::dec << "\n";
    f << "WorldFOV_DecreaseKEY=0x" << std::hex << std::uppercase << g_World_Dec << std::dec << "\n\n";

    f << "# --- ViewModel FOV (Setting ViewModelHorizontalFOV above 88.25 can reveal unfinished player model when using certain weapons) ---\n";
    f << "MinViewModelFOV=" << g_MinViewMFOV << "\n";
    f << "MaxViewModelFOV=" << g_MaxViewMFOV << "\n";
    f << "ViewModelHorizontalFOV=" << g_targetViewMHFOV.load() << "\n";
    f << "\n";
    f << "ViewModelFOV_IncreaseKEY=0x" << std::hex << std::uppercase << g_ViewM_Inc << std::dec << "\n";
    f << "ViewModelFOV_DecreaseKEY=0x" << std::hex << std::uppercase << g_ViewM_Dec << std::dec << "\n\n";

    f << "# --- Zoom ---\n";
    f << "MinZoomFOV=" << g_MinZoomFOV << "\n";
    f << "MaxZoomFOV=" << g_MaxZoomFOV << "\n";
    f << "HoldZoomFOV=" << g_ZoomFOV.load() << "\n";
    f << "\n";
    f << "HOLD_ZOOM_KEY=0x" << std::hex << std::uppercase << g_ZoomHoldKey << std::dec << "\n\n";

    f << "# VirtualKey Codes: https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes\n";

    f << "# --- Mouse ---\n";
    f << "# VK_LBUTTON    0x01 (Left Mouse Button)\n";
    f << "# VK_RBUTTON    0x02 (Right Mouse Button)\n";
    f << "# VK_MBUTTON    0x04 (Middle Mouse Button)\n";
    f << "# VK_XBUTTON1   0x05 (Side Mouse Button1)\n";
    f << "# VK_XBUTTON2   0x06 (Side Mouse Button2)\n";

    f << "# --- Standard Keys ---\n";
    f << "# VK_BACK       0x08 (Backspace)\n";
    f << "# VK_TAB        0x09 (Tab)\n";
    f << "# VK_RETURN     0x0D (Enter)\n";
    f << "# VK_SHIFT      0x10 (Shift)\n";
    f << "# VK_CONTROL    0x11 (Ctrl)\n";
    f << "# VK_MENU       0x12 (Alt)\n";
    f << "# VK_PAUSE      0x13 (Pause)\n";
    f << "# VK_CAPITAL    0x14 (Caps Lock)\n";
    f << "# VK_ESCAPE     0x1B (Esc)\n";
    f << "# VK_SPACE      0x20 (Spacebar)\n";

    f << "# --- Navigation ---\n";
    f << "# VK_PRIOR      0x21 (Page Up)\n";
    f << "# VK_NEXT       0x22 (Page Down)\n";
    f << "# VK_END        0x23 (End)\n";
    f << "# VK_HOME       0x24 (Home)\n";
    f << "# VK_LEFT       0x25 (Left Arrow)\n";
    f << "# VK_UP         0x26 (Up Arrow)\n";
    f << "# VK_RIGHT      0x27 (Right Arrow)\n";
    f << "# VK_DOWN       0x28 (Down Arrow)\n";
    f << "# VK_INSERT     0x2D (Insert)\n";
    f << "# VK_DELETE     0x2E (Delete)\n";
    f << "# VK_SNAPSHOT   0x2C (Print Screen)\n";

    f << "# --- Numbers (0-9) ---\n";
    f << "# 0=0x30, 1=0x31, 2=0x32, 3=0x33, 4=0x34\n";
    f << "# 5=0x35, 6=0x36, 7=0x37, 8=0x38, 9=0x39\n";
    f << "# --- Alphabet (A-Z) ---\n";
    f << "# A=0x41, B=0x42, C=0x43, D=0x44, E=0x45, F=0x46\n";
    f << "# G=0x47, H=0x48, I=0x49, J=0x4A, K=0x4B, L=0x4C\n";
    f << "# M=0x4D, N=0x4E, O=0x4F, P=0x50, Q=0x51, R=0x52\n";
    f << "# S=0x53, T=0x54, U=0x55, V=0x56, W=0x57, X=0x58\n";
    f << "# Y=0x59, Z=0x5A\n";

    f << "# --- Numpad ---\n";
    f << "# VK_NUMPAD0    0x60 (Numpad 0)\n";
    f << "# VK_NUMPAD1    0x61 (Numpad 1)\n";
    f << "# VK_NUMPAD2    0x62 (Numpad 2)\n";
    f << "# VK_NUMPAD3    0x63 (Numpad 3)\n";
    f << "# VK_NUMPAD4    0x64 (Numpad 4)\n";
    f << "# VK_NUMPAD5    0x65 (Numpad 5)\n";
    f << "# VK_NUMPAD6    0x66 (Numpad 6)\n";
    f << "# VK_NUMPAD7    0x67 (Numpad 7)\n";
    f << "# VK_NUMPAD8    0x68 (Numpad 8)\n";
    f << "# VK_NUMPAD9    0x69 (Numpad 9)\n";
    f << "# VK_MULTIPLY   0x6A (Numpad *)\n";
    f << "# VK_ADD        0x6B (Numpad +)\n";
    f << "# VK_SEPARATOR  0x6C (Numpad Enter/Separator)\n";
    f << "# VK_SUBTRACT   0x6D (Numpad -)\n";
    f << "# VK_DECIMAL    0x6E (Numpad .)\n";
    f << "# VK_DIVIDE     0x6F (Numpad /)\n";

    f << "# --- F-Keys ---\n";
    f << "# VK_F1         0x70\n";
    f << "# VK_F2         0x71\n";
    f << "# VK_F3         0x72\n";
    f << "# VK_F4         0x73\n";
    f << "# VK_F5         0x74\n";
    f << "# VK_F6         0x75\n";
    f << "# VK_F7         0x76\n";
    f << "# VK_F8         0x77\n";
    f << "# VK_F9         0x78\n";
    f << "# VK_F10        0x79\n";
    f << "# VK_F11        0x7A\n";
    f << "# VK_F12        0x7B\n";

    f << "# --- Other ---\n";
    f << "# VK_NUMLOCK    0x90\n";
    f << "# VK_SCROLL     0x91 (Scroll Lock)\n";
    f << "# VK_LSHIFT     0xA0\n";
    f << "# VK_RSHIFT     0xA1\n";
    f << "# VK_LCONTROL   0xA2\n";
    f << "# VK_RCONTROL   0xA3\n";

    f.close();

    struct stat st;
    if (stat(GetPath().c_str(), &st) == 0) g_lastConfigModTime = st.st_mtime;
}

void LoadConfig() {
    std::ifstream f(GetPath());
    if (!f.is_open()) { SaveConfig(); return; }
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#') continue;
        std::string key = line.substr(0, eq);
        std::string valStr = line.substr(eq + 1);

        float valF = 0.0f;
        try { valF = std::stof(valStr); }
        catch (...) { continue; }

        int valI = 0;
        try { valI = std::stoi(valStr, nullptr, 0); }
        catch (...) { continue; }

        if (key == "MinWorldFOV") g_MinWorldFOV = valF;
        else if (key == "MaxWorldFOV") g_MaxWorldFOV = valF;
        else if (key == "MinViewModelFOV") g_MinViewMFOV = valF;
        else if (key == "MaxViewModelFOV") g_MaxViewMFOV = valF;
        else if (key == "MinZoomFOV") g_MinZoomFOV = valF;
        else if (key == "MaxZoomFOV") g_MaxZoomFOV = valF;

        else if (key == "WorldHorizontalFOV")
            g_targetWorldHFOV.store(std::clamp(valF, g_MinWorldFOV, g_MaxWorldFOV));

        else if (key == "ViewModelHorizontalFOV")
            g_targetViewMHFOV.store(std::clamp(valF, g_MinViewMFOV, g_MaxViewMFOV));

        else if (key == "HoldZoomFOV")
            g_ZoomFOV.store(std::clamp(valF, g_MinZoomFOV, g_MaxZoomFOV));

        else if (key == "Step") g_Step = std::clamp(valF, 0.1f, 1.5f);

        else if (key == "ToggleMOD_KEY") g_ToggleKey = valI;
        else if (key == "HOLD_ZOOM_KEY") g_ZoomHoldKey = valI;
        else if (key == "WorldFOV_IncreaseKEY") g_World_Inc = valI;
        else if (key == "WorldFOV_DecreaseKEY") g_World_Dec = valI;
        else if (key == "ViewModelFOV_IncreaseKEY") g_ViewM_Inc = valI;
        else if (key == "ViewModelFOV_DecreaseKEY") g_ViewM_Dec = valI;
    }
}