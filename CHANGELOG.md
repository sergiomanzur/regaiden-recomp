# Changelog

All notable changes to **Resident Evil Gaiden Recompiled** will be documented in this file.

## [v0.3.1] - 2026-08-29

### Asset Packs on Android
- **`hd_pack/` and `music_pack/` now work on Android.** Both resolved a bare relative path against the process working directory, which is neither writable nor reachable with a file manager on Android, so both packs were silently dead there. They now resolve into external app storage (`/sdcard/Android/data/com.capcom.regaiden/files/`), which a file manager, USB transfer or `adb push` can reach. Desktop behaviour is unchanged.

### Documentation
- **Added a "Making Your Own Asset Packs" guide** to the README covering HD texture and music pack layout, formats, naming, hot-reload, and how to get files onto an Android device. States plainly that the bundled `hd_pack/` is AI-generated placeholder art shipped only as a proof of concept and should be replaced.
- **Corrected the control tables**, which did not match the actual default bindings: Quick Load is `F8` (not `F7`), the settings menu is `F10` (not `F1`, which is the performance overlay), the A/B secondary keys were swapped, Select's secondary is Right Shift (not Tab), Start has no secondary, and Fast Forward is Tab (not Space). The gamepad face buttons follow the Nintendo layout, so Game Boy `A` is the Xbox `B` button; Quick Save/Load are `X`/`Y`, not the bumpers.

> Ships the same engine as v0.3.0; the Android asset-pack fix landed after those
> binaries were built, so this release rebuilds both platforms.

## [v0.3.0] - 2026-08-29

### Rendering Fixes (Windows & Android)
- **Fixed Dynamic Flashlight Doing Nothing** ([#1](https://github.com/sergiomanzur/regaiden-recomp/issues/1)):
  - The precomputed light cone lookup table is stored with a fixed 336-pixel stride but was being walked linearly against the framebuffer, so the cone only landed on the correct coordinates in 21:9 Ultrawide. In Native 10:9 and the default 16:9 Widescreen the cone was sampled from the wrong pixels entirely, leaving nothing on screen but flat ambient darkness. The table is now indexed row by row.
  - `flashlight_intensity` is a 0-100 percentage but was fed into the blend as a 0-256 fixed-point fraction, capping the beam at roughly a third of its intended brightness. It is now converted properly, so 100% reaches full brightness.
- **Fixed Flickering & Glitching Item Pickups and Dialogue Portraits** ([#2](https://github.com/sergiomanzur/regaiden-recomp/issues/2)):
  - The widescreen renderer re-drew all 144 scanlines from a single end-of-frame snapshot of VRAM, OAM and the LCD registers, discarding the dot-accurate per-scanline output the PPU had already produced. Any scene that changes state part-way through a frame - HBlank HDMA tile streaming on the item viewer, window and palette raster effects behind talking-head portraits - was rendered from state that never existed at draw time, producing the striping and flicker. The native viewport is now taken verbatim from the PPU, and only the newly revealed side columns come from the wide re-render.
  - Window-layer pixels beyond the native 160-pixel width wrapped through `tile_x & 0x1F` into unused tilemap columns 20-31 and drew garbage tiles down both edges of full-screen UI. The window is now clamped to its real extents and its edge column extends outwards instead.

### Game-State Detection Fixes (Windows & Android)
- **Flashlight no longer runs on menus** ([#1](https://github.com/sergiomanzur/regaiden-recomp/issues/1)): The exploration test was simply "is any sprite on screen", which is true on the title screen, in menus and during dialogue. Menus in this game are drawn on the *window* layer starting at the very top of the screen (captured from the running game: `LCDC=0xE7, WY=0, WX=7` on the title/save-select screen), so a window anchored to the top is now recognised as full-screen UI and the flashlight is skipped there.
- **Removed the bogus "battle mode" branch**: The flashlight switched to a flat darkening when `wram[0x0900] > 0`. `$C900` is not a battle flag - the only code in the ROM touching it is a 0x50-byte save/restore `memcpy`, and it reads as all zeroes outside gameplay - so the branch fired unpredictably and the light cone ran during shooting sequences.
- **HD portrait no longer appears over menus and cutscenes** ([#2](https://github.com/sergiomanzur/regaiden-recomp/issues/2)): The portrait was drawn whenever the window layer was enabled with `WY < 120`, which matched the title screen and the save menu. It now requires a window anchored to the *bottom* of the screen, which is what an actual dialogue box looks like.
- **HD portrait no longer flickers**: `hd_pack` read `LCDC`/`WY` live while the frame was still being drawn. With smooth LCD transitions enabled the host presents several times per guest frame, so the overlay saw mid-frame raster state and toggled on and off between presents. Frame composition (widescreen re-render, flashlight, atmosphere shaders) now happens once per guest frame and extra presents repaint the cached result - which also stops the film grain from reseeding on every present.
- **HD portrait is no longer oversized**: It was a fixed 40x144 = 28% of screen height regardless of the dialogue box. It is now sized to fit inside the box, and its horizontal placement accounts for the active aspect ratio instead of always dividing by 160.

### Diagnostics
- **Added "Capture State Snapshot"** (menu -> Config & INI, or `F4` on desktop, plus a `--snapshot-frames` command-line option): writes a full dump of guest WRAM/HRAM/IO/OAM plus a readable register summary. Added to locate the game's real state addresses by diffing snapshots taken in different game states, since the flashlight gate, portrait selection and the built-in cheats were all built on addresses the disassembly does not corroborate.

### Replacement Soundtrack / Music Pack (Windows & Android)
- **New `music_pack/` folder**, working the same way as `hd_pack/`: drop in `.ogg` or `.wav` files named `track_<id>` and they replace the in-game music. Nothing ships with the project - the files are user supplied, exactly like the ROM.
- **Tracks follow the game automatically.** The sound driver's current-song byte was located at `$CE8C` by narrowing to the driver's RAM block (`$CE80-$CEFF`, the only addresses bank 1's genuine APU code touches) and then measuring it: 93/93 snapshots roaming one area read 2, 75/75 in a separate session read 2, the title screen reads a distinct id, and it changes exactly at music boundaries while holding steady when the inventory is opened over gameplay. A short debounce stops the brief dip between pieces from restarting a replacement track.
- **Sound effects are preserved.** Game Boy music and SFX share the same four APU channels, so the emulated audio is ducked to a configurable level (default 25%) while a replacement track is audible rather than muted, keeping gunshots, doors and menu blips underneath the new music.
- OGG decoding via vendored `stb_vorbis` (public domain, same collection as the existing `stb_image.h`); WAV via SDL. Tracks are decoded to PCM up front so the audio callback never decodes, and track swaps are made with the audio device locked.
- In-game controls for volume, duck level and looping, plus hot-reload and a display of the music id currently playing - useful for naming your files.

### Verified Game-State Detection (Windows & Android)
- **Replaced guessed RAM addresses with a measured signal.** The flashlight gate, the HD portrait selection and every built-in cheat keyed off `$C800`/`$C900`. Those are not game state: the only ROM code touching them is a 0x50-byte save/restore `memcpy`, every other apparent reference is misdisassembled data, and they read as **all zeroes even during gameplay**. Detection now uses the CGB WRAM bank, measured from the running game:
  - 75/75 snapshots taken while walking around a room -> bank 1
  - inventory, item info panel, PDA/map, title screen, save menu -> bank 2, every sample
- **Flashlight no longer lights up UI screens**: the cone is skipped on the inventory, the item info panel, the PDA/map and the menus. Confirmed on composed 16:9 output with the flashlight enabled - cone present while walking, gone the instant the inventory opens.
- **Widescreen sides no longer show tile garbage on UI screens**: a Game Boy tilemap is 32 columns wide but only 20 are ever shown, and on a UI screen the other 12 still hold whatever room the player was standing in - the stray `yyyyyy` / `[000]` font tiles down both sides of the item pickup screen. Those columns are now painted black, since there is no real data to draw. Sprites are kept out of them too.
- **HD portraits are limited to in-game dialogue** rather than any window layer.
- **The intro cutscene is covered too**: both intro paths (New Game and the attract loop) run with WRAM bank 2 selected, so the same test that excludes menus excludes them. Verified by instrumenting the gate - across 5200-frame runs of each path the flashlight was applied on zero frames, while a gameplay run applied it normally with 397/400 samples in bank 1.

### Diagnostics
- **`--snapshot-frames`** and a **Capture State Snapshot** button (menu -> Config & INI, or `F4`): dumps guest WRAM/HRAM/IO/OAM plus a register summary, for locating real game-state addresses by diffing.
- **`--composed-frames`**: dumps the fully composed frame (widescreen fill, lighting, shaders) rather than the raw guest framebuffer, so rendering changes can be verified visually.

### Fixed
- **`speed_percent` did nothing**: the `[General] speed_percent` setting was parsed and saved but never applied to the frame pacer.

### Native-First Defaults (Windows & Android)
- **First run now looks like an unmodified Game Boy Color**: Widescreen, the dynamic flashlight, the vignette, film grain, scanlines, CRT phosphor mask and the *Cold Biohazard* colour grade are all **off** by default, and the colour grade defaults to *Native GBC Colors*. Every enhancement stays one toggle away in the in-game menu (`F10`).
- **HD Texture Pack is now opt-in** rather than enabled out of the box.
- **Stopped shipping developer settings in releases**: The packaged `config.ini` was a copy of the maintainer's live settings file, which the running game rewrites - so releases went out with Widescreen forced on, every atmosphere shader enabled, and the **Infinite Health cheat switched on**. Releases now ship a pristine `config.default.ini`, and the working `config.ini` is no longer tracked in git.

### Android Fixes
- **Settings Now Persist**: `config.ini` was read from and written to the process working directory, which is not writable on Android - so every launch silently fell back to compiled defaults and no setting ever survived a restart. It is now stored in per-app storage alongside the other runtime preferences.
- **Fixed Broken Release Build**: All ten `ic_launcher*.png` launcher icons were JPEG data with a `.png` extension, which AAPT2 rejects; the APK could not be built at all. Regenerated as real PNGs at the correct mdpi/hdpi/xhdpi/xxhdpi/xxxhdpi densities, with a properly masked round variant.

## [v0.2.1] - 2026-08-26

### Audio & APU Emulation Fixes (Windows & Android)
- **Preserved Wave RAM Across APU Power Off Cycles**:
  - Prevented APU power-down routines (`NR52` bit 7 = 0) from clearing Channel 3 Wave RAM (`0xFF30`-`0xFF3F`), matching Game Boy hardware behavior. Fixes room ambient tracks, synth basslines, and SFX wave instruments becoming permanently silenced or corrupted after fight screens, room transitions, or sound driver resets.
- **Fixed Game Boy Color Wave RAM Write Dropping**:
  - Allowed continuous CPU write access to Wave RAM regardless of Channel 3 playback state, in accordance with CGB hardware specifications. Eliminates dropped waveform updates during battle sound effects, item pickups, and music instrument swaps.
- **Fixed Hardware-Accurate Sound Register Read Masks**:
  - Corrected `NR14`, `NR24`, `NR34`, `NR44` read masking (`0xBF` with bit 6) so that write-only trigger bit 7 does not falsely read as 1. Fixes sound driver length-counter misdetections where looping music tracks were prematurely stopped or tracks overlapped incorrectly.
  - Corrected `NR30`, `NR32`, `NR41`, and `NR52` power/channel status bit masking.
- **Fixed DAC Enable & Volume Envelope Mixing**:
  - Changed channel mixing DAC checks from initial envelope volume (`NRx2 & 0xF0`) to DAC power status (`NRx2 & 0xF8`). Fixes notes with initial volume 0 and upward volume sweeps (fades/swells) being muted.
- **Fixed Channel 1 Sweep Subtraction Underflow**:
  - Clamped downward frequency sweep calculations to 0 on underflow instead of overflowing `uint16_t` to 65535, preventing false sweep overflow disables.
- **Audio Batch Publication on VSync**:
  - Flushed pending audio sample batches at each 60 FPS frame boundary (`gb_platform_vsync`), minimizing audio buffer jitter.

## [v0.2.0] - 2026-08-25

### Native Android Port & Handheld Gaming Support
- **Full Native Android Architecture (ARM64-v8a)**:
  - Engineered direct native compilation pipeline using Android NDK (r26d), Clang 17, SDL2, OpenGL ES 3, and Dear ImGui.
  - Locked 60 FPS gameplay execution directly on ARM64-v8a hardware with zero emulation overhead.
- **On-Screen Touch Controller Overlay**:
  - Full touch D-Pad, A/B action buttons, Start/Select pills, and corner menu icons.
  - Multi-touch responsive tracking, haptic feedback toggle, and on-the-fly controller Show/Hide toggle icon.
- **Adaptive Screen Orientation & Display Layouts**:
  - Support for **Portrait** and **Landscape** modes with dynamic HUD/touch positioning.
  - In-Game Screen Orientation Lock settings (*Auto-Rotate*, *Force Landscape*, *Force Portrait*).
- **Touch-Friendly & Controller-Scrollable In-Game Settings UI**:
  - Fixed, pinned tab header and footer bars for all 8 in-game configuration tabs.
  - Smooth vertical touch/gesture swipe scrolling and physical controller stick/D-Pad scrolling.
  - Enlarged touch scrollbars (`ScrollbarSize = 28px`, `GrabMinSize = 36px`) with always-visible tracks.
- **Clean Legal Distribution & First-Run Storage Access Framework (SAF) Onboarding**:
  - Removed all proprietary ROMs, images, and copyrighted game data from the APK package for 100% legal open-source redistribution.
  - Added interactive first-run storage permission request and ROM picker dialog.
  - Automatic ROM discovery across standard device directories (`/sdcard/ROMs/GBC/`, `/sdcard/Download/`, app storage).
  - Native Storage Access Framework (`ACTION_OPEN_DOCUMENT`) file picker with SHA-256 integrity validation (`9a97678cbd8da02c8763e977674e17f460c06ea8b73bad35c52fe6817f506d44`) and 2,097,152-byte size verification.
  - Dedicated "Select / Change ROM Image..." option in the in-game settings menu.
- **Verified Handheld Hardware Support**:
  - Deployed and tested on **Retroid Pocket 6** (`cf916605`) and Android devices.

### Windows Port Enhancements
- **Embedded Application Icon**:
  - Embedded 256×256 `.ico` binary PE resource (`Resident_Evil_Gaiden.rc`) into `Resident_Evil_Gaiden__USA_.exe` for Windows Explorer.
  - Embedded high-resolution 32-bit RGBA window and taskbar application icons.
- **Static C/C++ CRT Runtime Linkage**:
  - Static `/MT` MSVC/Clang runtime linkage eliminating all `MSVCP140D.dll` / `VCRUNTIME140D.dll` dependencies.
- **Unified Multi-Platform Release Packager**:
  - Automated PowerShell build and packaging script (`scripts/package_release.ps1`) for generating self-contained Windows `.zip` and Android `.apk` artifacts.

---

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
