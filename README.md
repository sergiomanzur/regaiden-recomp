# Resident Evil Gaiden: Recompiled (PC / Native)

[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Android%20(ARM64)%20%7C%20Linux%20(WIP)-blue.svg)]()
[![C++](https://img.shields.io/badge/Language-C11%20%2F%20C%2B%2B20-orange.svg)]()
[![Backend](https://img.shields.io/badge/Graphics-SDL2%20%7C%20Dear%20ImGui%20%7C%20GLES3-green.svg)]()
[![Release](https://img.shields.io/badge/Release-v0.2.0-red.svg)]()
[![License](https://img.shields.io/badge/License-MIT-purple.svg)]()

A native static recompilation of **Resident Evil Gaiden** (Game Boy Color, 2001) for modern PC and Android platforms, built in C/C++ with hardware-accelerated SDL2, OpenGLES 3, and Dear ImGui.

Running directly on native hardware without CPU emulation overhead, this project modernizes the classic maritime survival horror experience with **True Widescreen**, **Dynamic 2D Flashlight Lighting**, **Atmospheric Horror Shaders**, **Touch Controls**, and an **HD Texture Pack Engine**.

---

## Highlights & Features

### 1. True Widescreen & Ultrawide Viewports (16:9 & 21:9)
- **Eliminates Camera Crunch**: Expands the horizontal exploration viewport from 20 tiles (160px) to **32 tiles (256px - 16:9)** and **42 tiles (336px - 21:9)** directly in C.
- **Corridor Sightlines**: Look deep down the narrow corridors of the luxury ocean liner *Starlight* to spot approaching zombies before bumping into them.
- **Aspect Ratio Modes**: Toggle seamlessly between *Native 10:9 (160×144)*, *True Widescreen 16:9 (256×144)*, and *True Ultrawide 21:9 (336×144)* at runtime.

### 2. Dynamic 2D Flashlight Lighting & 2D Horror Atmosphere
- **Real-Time Directional Flashlight**: Barry's flashlight casts a real-time directional beam of light down dark ship hallways.
- **Directional Tracking**: Light cone automatically rotates to match Barry's movement (Up, Down, Left, Right) based on input.
- **Ambient Darkness & Attenuation**: Unlit areas are shrouded in darkness with smooth radial distance gradients and cosine angular falloff.
- **Halogen Bulb Jitter**: Realistic high-frequency subtle bulb flicker and warm halogen color temperature.

### 3. Atmospheric Retro Survival Horror Shaders
- **Vignette Lighting**: Radial corner shadow falloff for an authentic claustrophobic survival horror atmosphere.
- **Cinematic Film Grain**: Temporal animated procedural noise simulating 90s classic survival horror film grain.
- **CRT Scanlines & Phosphor Mask**: Alternating horizontal scanline darkening and subpixel RGB shadowmask.
- **5 Horror Color Grading Profiles**:
  - `Native GBC Colors`
  - `Cold Biohazard Blue` (cool teal cast with rich shadow contrast)
  - `Bleach Bypass Gritty` (high-contrast desaturated look)
  - `Sepia Retro` (aged vintage horror tone)
  - `Silent Monochrome` (classic black-and-white noir mode)

### 4. HD Texture Pack & Modding Engine (`hd_pack/`)
- **Native PNG Decoder**: Embedded `stb_image` for zero-dependency high-speed image loading.
- **Host-Resolution Compositing**: High-definition assets render at **full native monitor resolution (1080p / 4K)**:
  - `hd_pack/backgrounds/`: HD 16:9 pre-rendered battle backgrounds (e.g. *Starlight Corridor*).
  - `hd_pack/monsters/`: HD battle monster and zombie sprites.
  - `hd_pack/portraits/`: HD character dialogue portraits (*Barry Burton*, *Leon S. Kennedy*, *Lucia*).
- **Hot-Reloading**: Edit or swap PNG assets and click **"Reload HD Textures"** in the in-game menu without restarting the game.

### 5. Built-in Cheats & GameShark Code Engine
- **One-Click Cheats**: Infinite Health (Barry, Leon, Lucia), Infinite Ammo (All Weapons), One-Hit Kill, Reticle Freeze / Always Perfect Hit, Unlock All Weapons (Shotgun, Grenades, Rifle), and Infinite Items.
- **Custom GameShark Codes**: Add and manage arbitrary 8-character GameShark / GameGenie RAM patch codes at runtime.

### 6. Multi-Slot Savestate Manager
- **10 Dedicated Savestate Slots**: Save and load instantly via the in-game overlay menu or shortcut keys (`F5` Save, `F7` Load, `F6`/`F8` Slot change).
- Automatic battery-backed SRAM persistence for native in-game typewriter save points.

### 7. Modern Controller, Keyboard & Mobile Touch Controls
- Full support for **XInput (Xbox)**, **PlayStation (DualShock / DualSense)**, **Retroid Pocket**, and **generic USB gamepads**.
- **Android Virtual Touch Gamepad**: On-screen D-Pad, action buttons, quick settings button, and controller show/hide toggle.
- **Portrait & Landscape Adaptive Layouts** with in-game orientation locking.
- Live in-game rebinding interface with analog stick support and rumble-ready architecture.

---

## Platform Availability & Roadmap

- [x] **Windows (x86_64)**: Fully supported with static CRT, embedded icon, and release archive (**Release v0.2.0**).
- [x] **Android (ARM64-v8a)**: Fully supported with touch gamepad, Retroid Pocket optimization, and legal ROM onboarding (**Release v0.2.0**).
- [ ] **Linux (x86_64 / ARM64)**: Native SDL2 + Vulkan build in active preparation.

---

## Controls

### Keyboard Defaults
| Action | Primary Key | Secondary Key |
| :--- | :--- | :--- |
| **Move Up** | `W` | `Up Arrow` |
| **Move Down** | `S` | `Down Arrow` |
| **Move Left** | `A` | `Left Arrow` |
| **Move Right** | `D` | `Right Arrow` |
| **A / Confirm / Shoot** | `K` | `Z` |
| **B / Cancel / Run** | `J` | `X` |
| **Select / Map** | `Backspace` | `Tab` |
| **Start / Inventory** | `Enter` | `Space` |
| **In-Game Settings Menu** | `Escape` | `F1` |
| **Quick Save State** | `F5` | - |
| **Quick Load State** | `F7` | - |
| **Fast Forward (2x)** | `Space` | `Tab` |

### Gamepad Defaults (Xbox / PlayStation)
| Action | Xbox Button | PlayStation Button |
| :--- | :--- | :--- |
| **Movement** | D-Pad / Left Stick | D-Pad / Left Stick |
| **A / Action** | `A` | `Cross` |
| **B / Cancel** | `B` | `Circle` |
| **Select** | `Back` / `View` | `Share` / `Select` |
| **Start** | `Start` / `Menu` | `Options` / `Start` |
| **In-Game Menu** | `Left Stick Click (L3)` | `L3` |
| **Quick Save** | `Right Bumper (RB)` | `R1` |
| **Quick Load** | `Left Bumper (LB)` | `L1` |

---

## Building from Source

### Prerequisites
- **CMake** (3.15 or newer)
- **C/C++ Compiler**: Clang, GCC, or MSVC (Visual Studio 2022) with C11 and C++20 support
- **Ninja** or MSBuild
- **SDL2 Development Libraries** (included in `deps/`)

### Build on Windows
```powershell
# Clone the repository
git clone https://github.com/sergiomanzur/regaiden-recomp.git
cd regaiden-recomp

# Build with static CRT and package Windows release
.\scripts\package_release.ps1 -Version "0.2.0" -SkipAndroid
```

### Build on Android (APK)
```powershell
# Navigate to Android directory
cd android

# Compile ARM64-v8a debug APK
.\gradlew assembleDebug
```
The resulting APK is generated at `android/app/build/outputs/apk/debug/app-debug.apk`.

---

## ROM Compatibility & Legal Notice

This repository contains **only clean-room reverse-engineered recompilation source code, runtime wrappers, shaders, and original mod assets**. It does **NOT** contain copyrighted game ROMs, proprietary audio, or commercial Game Boy Color assets.

To play, provide a legally acquired ROM dump of:
- **Title**: *Resident Evil Gaiden (USA)*
- **Format**: `.gbc`
- **Expected Size**: `2,097,152 bytes`
- **Expected SHA256**: `9a97678cbd8da02c8763e977674e17f460c06ea8b73bad35c52fe6817f506d44`

Place `Resident Evil Gaiden (USA).gbc` in the root directory (Windows) or select it via the in-game ROM setup picker on first launch (Android / Windows).

---

## License

- Source code: [MIT License](LICENSE)
- Dear ImGui: [MIT License](https://github.com/ocornut/imgui/blob/master/LICENSE.txt)
- stb_image: [Public Domain](https://github.com/nothings/stb)
- SDL2: [zlib License](https://www.libsdl.org/license.php)

*Resident Evil* and *Resident Evil Gaiden* are registered trademarks of Capcom Co., Ltd. and Virgin Interactive. This project is an independent open-source recreation for preservation and modern enhancement purposes.
