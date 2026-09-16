# Resident Evil Gaiden: Recompiled (PC / Native)

[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Android%20(ARM64)%20%7C%20Linux%20(WIP)-blue.svg)]()
[![C++](https://img.shields.io/badge/Language-C11%20%2F%20C%2B%2B20-orange.svg)]()
[![Backend](https://img.shields.io/badge/Graphics-SDL2%20%7C%20Dear%20ImGui%20%7C%20GLES3-green.svg)]()
[![Release](https://img.shields.io/badge/Release-v0.4.1-red.svg)]()
[![License](https://img.shields.io/badge/License-MIT-purple.svg)]()

A native static recompilation of **Resident Evil Gaiden** (Game Boy Color, 2001) for modern PC and Android platforms, built in C/C++ with hardware-accelerated SDL2, OpenGLES 3, and Dear ImGui.

Running directly on native hardware without CPU emulation overhead, this project modernizes the classic maritime survival horror experience with **Configurable Dash / Sprint**, **True Widescreen**, **Dynamic 2D Flashlight Lighting**, **Atmospheric Horror Shaders**, **Touch Controls**, a **Verified Cheats Engine**, and an **HD Texture Pack Engine**.

---

> **Note on defaults:** the game boots looking like an unmodified Game Boy Color - native 10:9 aspect, native GBC colours, no shaders, no flashlight, no HD pack. Every enhancement below is opt-in or configurable from the in-game menu (`F10`), and your choices are saved to `config.ini`.

## Highlights & Features

### 1. Configurable Dash / Sprint System (Windows & Android)
- **Fixes Sluggish Overworld Exploration**: Addresses *Resident Evil Gaiden*'s notoriously slow walking speed by adding an optional sprint system.
- **Context-Sensitive B-Button Dash (Enabled by default)**: Holding Game Boy **`B`** while moving with the D-Pad automatically sprints without opening the inventory. Tapping **`B`** while stationary opens the inventory screen as normal.
- **Dedicated Dash Bindings**:
  - **Keyboard**: Hold `Left Shift` or `Right Shift`.
  - **Gamepad**: Hold `R1` (Right Shoulder) or `L1` (Left Shoulder).
  - **Android Touch Screen**: Dedicated on-screen `>>` sprint button in Electric Blue positioned above the `B` button.
- **Configurable Modes & Speeds**:
  - `0 = Disabled (1x Walk Only)`
  - `1 = Hold Button to Run (Default - 200% / 2x Speed)`
  - `2 = Always Run (Whenever Moving)`
  - Speed slider adjustable from 125% to 250% in the new **Gameplay** settings tab.
- **Combat & Menu Safety**: Automatically deactivates in battle (ROM Bank 13/14) and menus/inventory (`gb_state_is_ui_screen`), preserving 100% original combat meter timing, hit detection, and CD audio pitch.

### 2. True Widescreen & Ultrawide Viewports (16:9 & 21:9)
- **Eliminates Camera Crunch**: Expands the horizontal exploration viewport from 20 tiles (160px) to **32 tiles (256px - 16:9)** and **42 tiles (336px - 21:9)** directly in C.
- **Corridor Sightlines**: Look deep down the narrow corridors of the luxury ocean liner *Starlight* to spot approaching zombies before bumping into them.
- **Aspect Ratio Modes**: Toggle seamlessly between *Native 10:9 (160×144)*, *True Widescreen 16:9 (256×144)*, and *True Ultrawide 21:9 (336×144)* at runtime.

### 3. Dynamic 2D Flashlight Lighting & 2D Horror Atmosphere
- **Real-Time Directional Flashlight**: Barry's flashlight casts a real-time directional beam of light down dark ship hallways.
- **Directional Tracking**: Light cone automatically rotates to match Barry's movement (Up, Down, Left, Right) based on input.
- **Ambient Darkness & Attenuation**: Unlit areas are shrouded in darkness with smooth radial distance gradients and cosine angular falloff.
- **Halogen Bulb Jitter**: Realistic high-frequency subtle bulb flicker and warm halogen color temperature.

### 4. Atmospheric Retro Survival Horror Shaders
- **Vignette Lighting**: Radial corner shadow falloff for an authentic claustrophobic survival horror atmosphere.
- **Cinematic Film Grain**: Temporal animated procedural noise simulating 90s classic survival horror film grain.
- **CRT Scanlines & Phosphor Mask**: Alternating horizontal scanline darkening and subpixel RGB shadowmask.
- **5 Horror Color Grading Profiles**:
  - `Native GBC Colors`
  - `Cold Biohazard Blue` (cool teal cast with rich shadow contrast)
  - `Bleach Bypass Gritty` (high-contrast desaturated look)
  - `Sepia Retro` (aged vintage horror tone)
  - `Silent Monochrome` (classic black-and-white noir mode)

### 5. HD Texture Pack & Modding Engine (`hd_pack/`)
> **The bundled `hd_pack/` is AI slop.** It is a quick proof of concept to show the engine works, nothing more - machine-generated placeholder art that does not match the game's style and was never meant to ship as a finished look. **Please replace it.** See [Making Your Own Asset Packs](#making-your-own-asset-packs) below. If you make something good, open a PR or an issue and it can be linked from here.

- **Native PNG Decoder**: Embedded `stb_image` for zero-dependency high-speed image loading.
- **Host-Resolution Compositing**: High-definition assets render at **full native monitor resolution (1080p / 4K)**:
  - `hd_pack/backgrounds/`: HD 16:9 pre-rendered battle backgrounds (e.g. *Starlight Corridor*).
  - `hd_pack/monsters/`: HD battle monster and zombie sprites.
  - `hd_pack/portraits/`: HD character dialogue portraits (*Barry Burton*, *Leon S. Kennedy*, *Lucia*).
- **Hot-Reloading**: Edit or swap PNG assets and click **"Reload HD Textures"** in the in-game menu without restarting the game.

### 6. Replacement Soundtrack (`music_pack/`)
- **Bring Your Own Music**: Drop `.ogg` or `.wav` files into `music_pack/` to replace the in-game soundtrack.
- **Follows the Game**: Tracks are matched to the game's own music ids, so the right music plays in the right place. Any id you have not supplied keeps the original music.
- **Sound Effects Preserved**: Game Boy music and SFX share the same four channels, so the emulated audio is *ducked* rather than muted while your music plays - gunshots, doors and menu blips still come through.
- **No music is bundled.** Like the ROM, the files are yours to supply.

### 7. Built-in Cheats & GameShark Code Engine
- **Hardware-Verified Built-in Cheats**: Re-anchored to real Game Boy Color disassembly memory addresses:
  - **Infinite Health**: Barry (`100 HP`), Leon (`100 HP`), Lucia (`120 HP`), with poison status and counters cleared automatically.
  - **Infinite Ammo**: Max ammo (99) for all 5 weapon ammo pools (Handgun, Shotgun, Grenade Launcher, Assault Rifle, Rocket Launcher).
  - **Unlock All Weapons**: Grants knife, handgun, shotgun, grenade launcher, and rocket launcher in inventory bitmasks.
  - **Infinite Items**: Sets max First Aid Sprays, Green/Red Herbs, and Kevlar Armor in inventory.
  - **Freeze Combat Reticle / Always Perfect Hit**: Centers and locks combat reticle to target, overrides hit outcome to 0 (perfect hit), and guarantees critical hits.
  - **One-Hit Kill in Battle**: Direct damage calculation hooks at `pc_44e3`, `loc_0d_49c2`, and `loc_0d_5f8e` + RAM clamping, defeating any zombie, creature, or the Tyrant in 1 hit.
- **Banked GameShark Code Support**: Full support for Game Boy Color GameShark codes (`01xx` standard and `9Bxx` WRAM banked codes) with whitespace/hyphen tolerance.

### 8. Multi-Slot Savestate Manager
- **10 Dedicated Savestate Slots**: Save and load instantly via the in-game overlay menu or shortcut keys (`F5` Save, `F8` Load, `F6`/`F7` Slot change).
- Automatic battery-backed SRAM persistence for native in-game typewriter save points.

### 9. Modern Controller, Keyboard & Mobile Touch Controls
- Full support for **XInput (Xbox)**, **PlayStation (DualShock / DualSense)**, **Retroid Pocket**, and **generic USB gamepads**.
- **Android Virtual Touch Gamepad**: On-screen D-Pad, action buttons, dedicated `>>` sprint button, quick settings button, and controller show/hide toggle.
- **Portrait & Landscape Adaptive Layouts** with in-game orientation locking.
- Live in-game rebinding interface with analog stick support and rumble-ready architecture.

---

## Platform Availability & Roadmap

- [x] **Windows (x86_64)**: Fully supported with static CRT, embedded icon, and release archive (**Release v0.4.1**).
- [x] **Android (ARM64-v8a)**: Fully supported with touch gamepad, Retroid Pocket optimization, and legal ROM onboarding (**Release v0.4.1**).
- [ ] **Linux (x86_64 / ARM64)**: Native SDL2 + Vulkan build in active preparation.

---

## Making Your Own Asset Packs

Both asset packs are plain folders of ordinary files. Nothing is compiled in, nothing is packed into an archive - drop files in, restart or hot-reload, done. No tooling required.

> **About the bundled `hd_pack/`:** it is **AI slop**. It exists purely to prove the engine works. The art is machine-generated placeholder junk that does not match the game's aesthetic, and it should not be taken as the intended look. If you have any pixel-art ability at all you will do better. Please replace it - and if you make something good, open a PR or an issue so it can be linked here for everyone.

### Where the folders live

| Platform | Location |
| :--- | :--- |
| **Windows** | `hd_pack/` and `music_pack/` next to `Resident_Evil_Gaiden__USA_.exe` |
| **Android** | `/sdcard/Android/data/com.capcom.regaiden/files/hd_pack/` and `.../music_pack/` |

Both folders (and the `hd_pack` subfolders) are created automatically the first time you run the game, so the easiest way to find them is to launch once and then look.

---

### HD Texture Pack (`hd_pack/`)

HD textures are drawn at your **monitor's** resolution, on top of the Game Boy image - so they are not limited to 160x144. A 1024x1024 PNG is perfectly reasonable.

```
hd_pack/
  backgrounds/
    battle.png          <- pre-rendered battle background
    battle_0.png
  monsters/
    zombie_0.png        <- battle monster / zombie sprites
    monster.png
  portraits/
    barry.png           <- character dialogue portraits
    leon.png
    lucia.png
```

**Format:** PNG, RGBA. Transparency is respected, so give monsters and portraits a transparent background rather than a solid colour.

**Sizing:** backgrounds are stretched to the whole game viewport, so match your aspect ratio (16:9 works well). Monsters and portraits are scaled proportionally to the viewport height, so square images are easiest to work with.

**Matching is by filename prefix.** A background is used if its name contains `backgrounds/battle`, a monster if it contains `monsters/zombie` or `monsters/monster`, and so on. That means `battle_starlight_corridor.png` works fine - you are not limited to the exact names above.

**Hot-reload:** edit a PNG and press **Reload HD Textures** in the in-game menu (`F10`). No restart needed, which makes iterating on art quick.

Enable the pack with **Enable HD Texture Pack** in the menu. It is off by default.

---

### Replacement Soundtrack (`music_pack/`)

```
music_pack/
  track_1.ogg           <- replaces Battle music (ID 1)
  track_10.ogg          <- replaces Title Screen (ID 10)
  ...
```

**Formats:** `.ogg` (Ogg Vorbis) and `.wav`. OGG is strongly preferred - a three-minute WAV is about 30 MB, which adds up fast, especially on a phone. Any sample rate and channel count works; files are converted to 44.1 kHz stereo when loaded.

**Naming:** `track_<id>`, where `<id>` is the game's own music ID (from 1 to 27). Tracks then follow the game automatically - the right music plays in the right place, and any ID you have not supplied simply keeps the original Game Boy music. That means you can replace one track and leave the rest alone.

> [!NOTE]
> **Track 0 is reserved for silence/stop**: During room transitions and scene fades, the sound driver sets the music register to 0 (silence). Do NOT name any music `track_0.ogg`, as it is not a playable track.

**Finding or verifying track IDs:**

1. Open the in-game menu (`F10` on desktop, the gear icon on Android)
2. Go to the **HD Pack** tab
3. The menu always displays **`Current Game Track: ID N (Name)`** regardless of whether a custom track is playing or not.
4. Name your file `track_N.ogg` and click **Reload Music Pack** to test it immediately without restarting.

#### Track ID Index

| Track ID | Description / Scene |
| :---: | :--- |
| `0` | *Silence / Stop / Room Transitions* (Reserved - not a playable track) |
| `1` | Battle |
| `2` | Introduction (Part 1 - Capcom & Legal disclaimers) |
| `3` | Introduction (Part 2 - Virgin Interactive & M4) |
| `4` | Introduction (Part 3 - In-game Intro Cutscene) |
| `5` | Restaurant |
| `6` | Restaurant (Alternate Loop) |
| `7` | Restaurant X |
| `8` | Prologue / Cutscenes |
| `9` | *Unused / Silence* |
| `10` | **Title Screen** |
| `11` | Side Deck |
| `12` | Side Deck X |
| `13` | Main Hall X (Variation) |
| `14` | Side Deck X (Mild Drums) |
| `15` | Main Hall |
| `16` | Main Hall (Slower, No Bass) |
| `17` | Main Hall X |
| `18` | Title (Battle Version) |
| `19` | Zombie Attack |
| `20` | Class Cabin |
| `21` | Class Cabin X |
| `22` | Class Cabin (With Bass) |
| `23` | Corridor |
| `24` | Corridor X |
| `25` | Corridor X (Variation) |
| `26` | Corridor X (No Drums, Slower) |
| `27` | Class Cabin (With Bass & Drums) |

#### Seamless Loop Point Support

Custom tracks can loop seamlessly from a designated loop point (skipping the song's intro on loop repetitions). Loop points can be configured in 3 flexible ways:

1. **WAV Files (`smpl` Chunk)**: Standard sampler loop points embedded by digital audio workstations (DAWs) like FL Studio, Reaper, Wavosaur, Audacity, GoldWave, etc., are detected automatically.
2. **OGG Vorbis Comments**: Standard comment tags like `LOOPSTART` (or `LOOP_START`) and `LOOPEND` / `LOOPLENGTH` (or `LOOP_END` / `LOOP_LENGTH`). Supports integer sample offsets, seconds (e.g. `3.292s`), or `MM:SS.mmm` timestamps.
3. **External `music_pack.ini`**: Create a plain text `music_pack/music_pack.ini` file (or `track_<id>.ini`) to set loop points without needing an audio editor:
   ```ini
   [track_10]
   loop_start = 3.292s
   loop_end = 85.5s

   [track_1]
   loop_start = 145200
   ```
When loop points are present, the in-game menu (`F10`) displays the detected loop range (`[Loop: 00:03.29 -> 01:25.50]`).

**Sound effects.** Game Boy music and SFX share the same four audio channels, so there is no way to mute only the music. The emulated audio is turned **down** instead, which keeps gunshots, doors and menu blips audible under your track. Two sliders control the balance:

- **Music Volume** - how loud your replacement track is
- **Game Audio While Music Plays** - how loud the original Game Boy audio stays (default 25%). Lower it for less of the original melody bleeding through; raise it to keep sound effects punchier.

**Looping** is on by default, so short tracks repeat rather than falling silent.

---

### Getting your files onto Android

Everything lives in the app's external storage folder, which is reachable without root:

```
/sdcard/Android/data/com.capcom.regaiden/files/
    hd_pack/
    music_pack/
```

**Run the game once first** - that creates the folders.

- **USB from a PC:** connect the phone, set the USB mode to *File Transfer / MTP*, then browse to `Internal storage > Android > data > com.capcom.regaiden > files` and copy your `hd_pack` / `music_pack` contents in.
- **On the device:** most file managers can reach `Android/data` directly. On Android 11+ some stock file managers restrict it - if yours does, use a manager that supports the Storage Access Framework picker, or copy the files over USB.
- **ADB:**
  ```bash
  adb push music_pack/. /sdcard/Android/data/com.capcom.regaiden/files/music_pack/
  adb push hd_pack/.    /sdcard/Android/data/com.capcom.regaiden/files/hd_pack/
  ```

After copying, either restart the game or use **Reload HD Textures** / **Reload Music Pack** in the menu.

> The same folder is also where **state snapshots** are written if you use the diagnostics button, which makes them easy to pull off the device and attach to a bug report.

---

## Controls

### Keyboard Defaults
| Action | Primary Key | Secondary Key |
| :--- | :--- | :--- |
| **Move Up** | `W` | `Up Arrow` |
| **Move Down** | `S` | `Down Arrow` |
| **Move Left** | `A` | `Left Arrow` |
| **Move Right** | `D` | `Right Arrow` |
| **A / Confirm / Shoot** | `Z` | `J` |
| **B / Cancel / Run** | `X` | `K` (Hold while moving to Run) |
| **Dash / Sprint (Dedicated)** | `Left Shift` | `Right Shift` |
| **Select / Map** | `Backspace` | `Tab` |
| **Start / Inventory** | `Enter` | - |
| **In-Game Settings Menu** | `F10` | `Escape` |
| **Quick Save State** | `F5` | - |
| **Quick Load State** | `F8` | - |
| **Previous / Next Save Slot** | `F6` | `F7` |
| **Fast Forward** | `Tab` | - |
| **Toggle Max Speed** | `` ` `` (backtick) | - |
| **Toggle Mute** | `M` | - |
| **Performance Overlay** | `F1` | - |
| **Capture State Snapshot** | `F4` | - |

### Gamepad Defaults (Xbox / PlayStation)
| Action | Xbox Button | PlayStation Button |
| :--- | :--- | :--- |
| **Movement** | D-Pad / Left Stick | D-Pad / Left Stick |
| **A / Action** | `B` | `Circle` |
| **B / Cancel** | `A` | `Cross` (Hold while moving to Run) |
| **Dash / Sprint (Dedicated)** | `Right Shoulder (RB)` | `R1` / `L1` |
| **Select** | `Back` / `View` | `Share` / `Select` |
| **Start** | `Start` / `Menu` | `Options` / `Start` |
| **In-Game Menu** | `Left Stick Click (L3)` | `L3` |
| **Quick Save** | `X` | `Square` |
| **Quick Load** | `Y` | `Triangle` |
| **Fast Forward** | `Right Trigger (RT)` | `R2` |

> The face buttons follow the Nintendo layout: the **right-hand** button is Game Boy `A`, so on an Xbox pad that is the `B` button. All bindings are remappable in the in-game menu. On Android, a dedicated on-screen `>>` sprint button is placed above the virtual `B` button.

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
.\scripts\package_release.ps1 -Version "0.4.0" -SkipAndroid
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
