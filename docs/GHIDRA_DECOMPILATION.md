# Ghidra & Reverse Engineering Workflow for Resident Evil Gaiden

This guide outlines how to use **Ghidra** and the included disassembly tools to reverse-engineer Resident Evil Gaiden, extract symbols, and replace recompiled routines with clean, human-written C functions.

---

## 1. Tooling Overview

1. **Static Recompiler (`tools/gb-recompiled/`)**:
   - Converts the GBC binary into optimized native C.
   - Supports **Native Patch Manifests** (`--native-patch`) allowing individual recompiled functions to be replaced with human-authored C functions at build time.

2. **Disassembler & Bank Mapper (`disassembly/` & `tools/mgbdis/`)**:
   - Contains complete disassembled assembly files (`bank_000.asm` through `bank_07f.asm`) with Game Boy hardware register definitions (`hardware.inc`).
   - Use `python tools/mgbdis/mgbdis.py "rom/Resident Evil Gaiden (USA).gbc"` to regenerate or re-map labels.

3. **Ghidra with Game Boy Plugin**:
   - For interactive decompilation into pseudo-C, data structure mapping, and flow graphing.

---

## 2. Setting Up Ghidra for Game Boy Color ROMs

### Option A: Using Ghidra-GameBoy Plugin
1. Download or clone the [Ghidra-GameBoy plugin](https://github.com/Ghidra-GameBoy) or [Ghidra-DMG](https://github.com/Ghidra-DMG).
2. In Ghidra, go to **File -> Install Extensions** and point to the `.zip` extension archive.
3. Restart Ghidra.
4. Import `rom/Resident Evil Gaiden (USA).gbc`.
5. Select format **Game Boy ROM** (or processor **LR35902 / Z80 (Game Boy variant)**).
6. Ghidra will automatically map:
   - Bank 00 (`0x0000 - 0x3FFF`)
   - Switchable ROM Banks (`0x4000 - 0x7FFF` with MBC5 banking)
   - VRAM (`0x8000 - 0x9FFF`)
   - WRAM (`0xC000 - 0xDFFF` / Echo RAM)
   - OAM (`0xFE00 - 0xFE9F`)
   - High RAM / IO (`0xFF00 - 0xFFFF`)

### Option B: Using Symbols from `disassembly/`
You can import symbols and labels directly into Ghidra or export them into `.sym` format to pass to `gbrecomp --symbols symbols.sym`.

---

## 3. Replacing Recompiled Code with Native C (Native Patches)

`gb-recompiled` supports replacement of specific LR35902 functions with hand-crafted C routines.

### Step 1: Create a Native Patch Manifest (`native_patch.json`)
```json
{
  "patch_id": "custom_sound_engine",
  "rom_sha256": "9a97678cbd8da02c8763e977674e17f460c06ea8b73bad35c52fe6817f506d44",
  "bindings": [
    {
      "bank": 1,
      "address": "0x4050",
      "native_function": "custom_play_sound_effect"
    }
  ]
}
```

### Step 2: Implement the C Function
In `src/custom/` or `src/recompiled/`:
```c
#include "gbrt.h"

void custom_play_sound_effect(GBContext* ctx) {
    uint8_t sound_id = ctx->cpu.a;
    // Native audio playback or enhanced logic here!
}
```

### Step 3: Recompile with the Patch
```bash
tools/gb-recompiled/build/bin/gbrecomp "rom/Resident Evil Gaiden (USA).gbc" -o src/recompiled --native-patch native_patch.json
```
The recompiler will automatically wire the native C function into the dispatch table and compile it natively.
