# Metro Reduxed FOV

![Metro Redux FOV Preview](https://github.com/user-attachments/assets/4d65b534-4d3e-4846-a0b2-fb0db738beac)

Field of view modifier for **Metro 2033 Redux** and **Metro: Last Light Redux** (Steam, GOG, and Epic Games Store).

Features independent world and viewmodel FOV adjustments, runtime hotkeys, automatic aspect ratio correction, dynamic ADS zoom scaling, and configuration persistence.

---

## Features

- **Dual FOV Adjustment**: Change world FOV and weapon viewmodel FOV independently.
- **Dynamic ADS Scaling**: Automatically scales viewmodel FOV when aiming down sights so scopes and ironsights remain readable.
- **Runtime Hotkeys & Smooth Interpolation**: Adjust FOV on the fly or hold to zoom with configurable smoothing.
- **Auto-Detection & Pattern Scanning**: Dynamically resolves camera and FOV addresses across game updates and store versions.
- **Full State Restoration**: Toggling the mod off restores native engine values (`50.625` vertical FOV) and unpatches camera instructions.
- **Config Persistence**: Saves settings to `MetroReduxedFOV.cfg`.

---

## Default Controls

| Action | Key / Input |
|---|---|
| **Toggle Mod** | `F7` |
| **Hold Zoom** | `Mouse Button 5` |
| **Increase / Decrease World FOV** | `NumPad 8` / `NumPad 5` |
| **Increase / Decrease Viewmodel FOV** | `NumPad 7` / `NumPad 4` |

*Keys can be customized in `MetroReduxedFOV.cfg` using standard Windows Virtual-Key codes.*

---

## Installation

### Option 1: DLL Sideloading (Recommended)
1. Copy `MetroReduxedFOV.dll` into your game directory alongside `metro.exe`.
2. Choose one sideloading method:
   - **dinput8**: Copy `dinput8.dll` to the game folder.
   - **winmm**: Copy `winmm.dll` to the game folder.
3. Launch the game normally.

### Option 2: DLL Injection
Inject `MetroReduxedFOV.dll` into `metro.exe` using any standard 64-bit DLL injector once the game process starts.

---

## Building from Source

- **Toolchain**: Visual Studio 2022 / v145 toolset (C++20)
- **Architecture**: x64

Open `Metro_Reduxed_FOV.slnx` or `Metro_Reduxed_FOV.vcxproj` and build in **Release x64**. Output binary: `x64Build/Release/MetroReduxedFOV.dll`.

---

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
