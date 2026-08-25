# Changelog

All notable changes to **Resident Evil Gaiden Recompiled** will be documented in this file.

## [v0.1b Hotfix] - 2026-08-25

### Fixed (Windows)
- **Resolved Missing CRT Runtime Errors**: Enforced static C/C++ runtime linkage (`/MT` with MSVC / Clang) across all Windows release builds.
  - Fixes missing `MSVCP140D.dll`, `VCRUNTIME140D.dll`, `VCRUNTIME140_1D.dll`, and `ucrtbased.dll` dialog errors on Windows systems without developer tools installed.
  - The Windows release is now fully self-contained and does not require any external Visual C++ Redistributable packages.
- **SDL2 DLL Bundling**: Ensured `SDL2.dll` is packaged alongside `Resident_Evil_Gaiden__USA_.exe` in the release archive.

---

## [v0.1b] - 2026-08-25

### Initial Public Beta Release

#### Engine & Performance
- **Static C Recompilation**: Direct native hardware execution running at locked 60 FPS on x86_64 and ARM64.
- **Zero Input Latency**: Frame-pacing and direct audio buffer synchronization.

#### Display & Widescreen
- **True Widescreen (16:9 - 256×144)**: Expands the exploration viewport by 96 horizontal pixels, widening corridor sightlines and eliminating camera crunch without stretching.
- **True Ultrawide (21:9 - 336×144)**: 42×18 tile viewport expansion for ultrawide PC monitors and tall mobile screens.
- **Native 10:9 Mode**: Classic 160×144 Game Boy Color resolution with authentic integer scaling.
- **V-Sync & Window Scaling Presets** (1x to 8x, Fullscreen Borderless).

#### Atmospheric Horror Shaders & Lighting
- **Dynamic 2D Flashlight Lighting**: Real-time directional lighting cone tracking Barry, Leon, and Lucia with halogen bulb flicker and ambient room darkness.
- **Horror Post-Processing Shaders**: Vignette corner shadows, 35mm film grain, CRT scanlines, and CRT phosphor mask.
- **5 Cinematic Color Grading Profiles**: Native GBC, Cold Biohazard Blue, Bleach Bypass Gritty, Sepia Retro, and Silent Monochrome.

#### HD Texture Pack Engine
- **Hardware-Accelerated PNG Texture Replacement**: High-resolution host overlays for battle backgrounds, zombie & boss combat sprites, and character dialogue portraits.
- **In-Game Texture Gallery**: Live visual inspection of loaded HD textures and asset hot-reloading.

#### Cheats & Savestates
- **Built-in Cheats Engine**: Infinite Health, Infinite Ammo, One-Hit Kill, Reticle Freeze / Always Perfect Hit, Unlock All Weapons, Infinite Items.
- **GameShark RAM Code Engine**: Live hex patch editor with persistence.
- **10-Slot Savestate Manager**: Instant save, load, and slot management.

#### Controls
- **Gamepad Support**: Native plug-and-play for Xbox, PlayStation (DualShock/DualSense), Nintendo, 8BitDo, and generic USB/Bluetooth gamepads with live rebinding.
- **Full Keyboard Customization**.
