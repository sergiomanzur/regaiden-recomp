#include "state_dump.h"
#include "ppu.h"
#include "audio.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * Guest memory snapshotting.
 *
 * The port's game-state detection (flashlight gating, HD portrait selection,
 * the built-in cheats) reads hardcoded WRAM addresses that the disassembly
 * cannot corroborate - the only real code touching $C800/$C900 is a save and
 * restore memcpy. Rather than keep guessing, capture the whole of WRAM in a
 * known game state and diff snapshots to find the bytes that actually track
 * exploration vs. shooting vs. menus, and which character is speaking.
 */

/* Mirrors the allocation sizes in gbrt.c, which keeps these macros private. */
#define SD_WRAM_BYTES  (0x1000 * 8)  /* CGB: 8 banks x 4 KiB */
#define SD_HRAM_BYTES  0x7F
#define SD_IO_BYTES    0x80
#define SD_OAM_BYTES   OAM_SIZE

#define SD_MAGIC "REGAIDEN-SNAPSHOT-1"

static char s_output_dir[512] = "";
static int s_count = 0;

void state_dump_set_output_dir(const char* dir) {
    if (!dir || !dir[0]) {
        return;
    }
    snprintf(s_output_dir, sizeof(s_output_dir), "%s", dir);
}

int state_dump_count(void) {
    return s_count;
}

const char* state_dump_output_dir(void) {
    return s_output_dir[0] ? s_output_dir : ".";
}

static void build_path(char* out, size_t size, int index, const char* ext) {
    if (s_output_dir[0]) {
        snprintf(out, size, "%s/state_%02d.%s", s_output_dir, index, ext);
    } else {
        snprintf(out, size, "state_%02d.%s", index, ext);
    }
}

/* Registers worth eyeballing directly, so the .txt is useful without tooling. */
static void write_summary(FILE* f, GBContext* ctx, const char* label, int index) {
    const uint8_t* io = ctx->io;

    fprintf(f, "# Resident Evil Gaiden - guest state snapshot %02d\n", index);
    fprintf(f, "label=%s\n", (label && label[0]) ? label : "(none)");
    fprintf(f, "rom_bank=%u\n", (unsigned)ctx->rom_bank);
    fprintf(f, "wram_bank=%u\n", (unsigned)ctx->wram_bank);
    fprintf(f, "pc=%04X sp=%04X\n", ctx->pc, ctx->sp);

    fprintf(f, "\n[LCD]\n");
    fprintf(f, "LCDC(FF40)=%02X STAT(FF41)=%02X\n", io[0x40], io[0x41]);
    fprintf(f, "SCY(FF42)=%02X SCX(FF43)=%02X LY(FF44)=%02X\n", io[0x42], io[0x43], io[0x44]);
    fprintf(f, "WY(FF4A)=%02X WX(FF4B)=%02X\n", io[0x4A], io[0x4B]);
    fprintf(f, "window_enabled=%d sprites_enabled=%d bg_enabled=%d\n",
            (io[0x40] & 0x20) ? 1 : 0, (io[0x40] & 0x02) ? 1 : 0, (io[0x40] & 0x01) ? 1 : 0);

    /* The APU keeps its own register state; ctx->io is never updated for
     * FF10-FF3F, so read it back through the emulated bus. */
    fprintf(f, "\n[APU registers FF10-FF26]\n");
    for (int a = 0x10; a <= 0x26; a++) {
        fprintf(f, "FF%02X=%02X%s", a, gb_audio_read(ctx, (uint16_t)(0xFF00 + a)),
                ((a - 0x10) % 8 == 7) ? "\n" : " ");
    }
    fprintf(f, "\n[Wave RAM FF30-FF3F]\n");
    for (int a = 0x30; a <= 0x3F; a++) {
        fprintf(f, "%02X%s", gb_audio_read(ctx, (uint16_t)(0xFF00 + a)),
                ((a - 0x30) % 16 == 15) ? "\n" : " ");
    }

    /* The two buffers every existing heuristic keys off, dumped in full so the
     * guesses can be checked against reality state by state. */
    fprintf(f, "\n[WRAM $C800-$C84F]\n");
    for (int i = 0; i < 0x50; i++) {
        fprintf(f, "%02X%s", ctx->wram[0x0800 + i], (i % 16 == 15) ? "\n" : " ");
    }
    fprintf(f, "\n[WRAM $C900-$C94F]\n");
    for (int i = 0; i < 0x50; i++) {
        fprintf(f, "%02X%s", ctx->wram[0x0900 + i], (i % 16 == 15) ? "\n" : " ");
    }

    fprintf(f, "\n[Active sprites]\n");
    int active = 0;
    for (int i = 0; i < 40; i++) {
        const uint8_t* s = ctx->oam + (i * 4);
        if (s[0] >= 16 && s[0] <= 160) {
            fprintf(f, "obj%02d y=%3u x=%3u tile=%02X flags=%02X\n", i, s[0], s[1], s[2], s[3]);
            active++;
        }
    }
    fprintf(f, "active_sprite_count=%d\n", active);
}

bool state_dump_capture(GBContext* ctx, const char* label,
                        char* out_status, size_t status_size) {
    if (!ctx || !ctx->wram || !ctx->hram || !ctx->io || !ctx->oam) {
        if (out_status && status_size) {
            snprintf(out_status, status_size, "Snapshot failed: guest not running");
        }
        return false;
    }

    const int index = s_count + 1;
    char bin_path[640];
    char txt_path[640];
    build_path(bin_path, sizeof(bin_path), index, "bin");
    build_path(txt_path, sizeof(txt_path), index, "txt");

    FILE* bin = fopen(bin_path, "wb");
    if (!bin) {
        if (out_status && status_size) {
            snprintf(out_status, status_size, "Snapshot failed: cannot write %s", bin_path);
        }
        return false;
    }

    /* Fixed-layout container so snapshots can be diffed byte for byte. */
    fwrite(SD_MAGIC, 1, sizeof(SD_MAGIC), bin);
    const uint32_t sizes[4] = { SD_WRAM_BYTES, SD_HRAM_BYTES, SD_IO_BYTES, SD_OAM_BYTES };
    fwrite(sizes, sizeof(sizes), 1, bin);
    fwrite(ctx->wram, 1, SD_WRAM_BYTES, bin);
    fwrite(ctx->hram, 1, SD_HRAM_BYTES, bin);
    fwrite(ctx->io, 1, SD_IO_BYTES, bin);
    fwrite(ctx->oam, 1, SD_OAM_BYTES, bin);
    fclose(bin);

    FILE* txt = fopen(txt_path, "w");
    if (txt) {
        write_summary(txt, ctx, label, index);
        fclose(txt);
    }

    s_count = index;
    printf("[Snapshot] Wrote %s and %s\n", bin_path, txt_path);
    fflush(stdout);

    if (out_status && status_size) {
        snprintf(out_status, status_size, "Snapshot %02d saved -> %s", index, bin_path);
    }
    return true;
}
