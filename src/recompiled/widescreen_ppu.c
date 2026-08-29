#include "widescreen_ppu.h"
#include "ppu.h"
#include "game_state.h"
#include <string.h>

uint32_t g_wide_framebuffer[GB_MAX_FRAMEBUFFER_SIZE];

/* Window start line at or above which the window is covering the whole screen. */
#define FULLSCREEN_UI_MAX_WY 16

static inline uint32_t rgb555_to_rgba(uint16_t color) {
    uint8_t r = (uint8_t)(((color >> 0) & 0x1F) * 255 / 31);
    uint8_t g = (uint8_t)(((color >> 5) & 0x1F) * 255 / 31);
    uint8_t b = (uint8_t)(((color >> 10) & 0x1F) * 255 / 31);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

int widescreen_get_target_width(void) {
    switch (g_app_config.widescreen_mode) {
        case ASPECT_WIDESCREEN_16_9:
            return GB_WIDESCREEN_WIDTH; // 256
        case ASPECT_ULTRAWIDE_21_9:
            return GB_ULTRAWIDE_WIDTH;  // 336
        case ASPECT_NATIVE_10_9:
        default:
            return GB_NATIVE_WIDTH;      // 160
    }
}

int widescreen_get_target_height(void) {
    return GB_NATIVE_HEIGHT; // 144
}


/*
 * Copy the dot-accurate 160x144 frame produced by the real PPU into the middle
 * of the widescreen buffer.
 *
 * The wide re-render below works off a single end-of-frame snapshot of VRAM,
 * OAM and the LCD registers, so it cannot reproduce anything the game changes
 * part-way through a frame (HBlank HDMA tile streaming on the item viewer,
 * window and palette raster effects behind dialogue portraits). Those scenes
 * came out striped and flickering. The PPU already rendered them correctly
 * scanline by scanline, so the native viewport is taken verbatim from it and
 * only the newly revealed side columns come from the approximate re-render.
 */
static void blit_native_center(uint32_t* out_fb, int target_w, const uint32_t* native_fb) {
    const int x_offset = (target_w - GB_NATIVE_WIDTH) / 2;
    for (int y = 0; y < GB_NATIVE_HEIGHT; y++) {
        memcpy(&out_fb[(size_t)y * (size_t)target_w + (size_t)x_offset],
               &native_fb[(size_t)y * GB_NATIVE_WIDTH],
               GB_NATIVE_WIDTH * sizeof(uint32_t));
    }
}

void widescreen_render_frame(GBContext* ctx, const uint32_t* native_fb, uint32_t* out_fb,
                             int* out_width, int* out_height) {
    if (!ctx || !out_fb) return;

    int target_w = widescreen_get_target_width();
    int target_h = widescreen_get_target_height();

    if (out_width) *out_width = target_w;
    if (out_height) *out_height = target_h;

    // Prefer the exact frame the platform layer chose to present; fall back to
    // the PPU's live framebuffer only when the caller has none.
    if (!native_fb) {
        native_fb = gb_get_framebuffer(ctx);
    }

    const GBPPU* ppu = (const GBPPU*)ctx->ppu;
    const uint8_t* vram = ctx->vram;

    if (!ppu || !vram || (ctx->io[0x40] & LCDC_LCD_ENABLE) == 0) {
        memset(out_fb, 0, (size_t)target_w * (size_t)target_h * sizeof(uint32_t));
        if (native_fb) {
            blit_native_center(out_fb, target_w, native_fb);
        }
        return;
    }

    // Native 10:9 mode
    if (g_app_config.widescreen_mode == ASPECT_NATIVE_10_9 || target_w == GB_NATIVE_WIDTH) {
        if (native_fb) {
            memcpy(out_fb, native_fb, GB_NATIVE_WIDTH * GB_NATIVE_HEIGHT * sizeof(uint32_t));
        }
        return;
    }

    uint8_t lcdc = ctx->io[0x40];
    uint8_t scy = ctx->io[0x42];
    uint8_t scx = ctx->io[0x43];
    uint8_t wy = ctx->io[0x4A];
    uint8_t wx = ctx->io[0x4B];

    bool bg_enable = (lcdc & LCDC_BG_ENABLE) != 0;
    bool win_enable = (lcdc & LCDC_WINDOW_ENABLE) != 0;
    bool obj_enable = (lcdc & LCDC_OBJ_ENABLE) != 0;
    bool obj_8x16 = (lcdc & LCDC_OBJ_SIZE) != 0;
    bool unsigned_tile_data = (lcdc & LCDC_TILE_DATA) != 0;

    uint16_t bg_tilemap_offset = (lcdc & LCDC_BG_TILEMAP) ? 0x1C00 : 0x1800;
    uint16_t win_tilemap_offset = (lcdc & LCDC_WINDOW_TILEMAP) ? 0x1C00 : 0x1800;

    int x_offset = (target_w - GB_NATIVE_WIDTH) / 2; // +48 for 16:9 (256w), +88 for 21:9 (336w)
    int cam_x = (int)scx - x_offset;

    // A Game Boy window only ever covers native screen columns [wx-7, 160), so
    // the tilemap holds no data for the widened viewport. Reading past it wraps
    // through `& 0x1F` into unused tilemap columns and draws garbage tiles, so
    // clamp instead and let the window's edge column extend outwards. When the
    // window already touches the native left edge it is stretched to the left
    // border too, keeping full-width dialogue boxes continuous.
    int win_origin_x = (int)wx - 7;
    int win_native_w = GB_NATIVE_WIDTH - win_origin_x;
    if (win_native_w > GB_NATIVE_WIDTH) win_native_w = GB_NATIVE_WIDTH;
    if (win_native_w < 1) win_native_w = 1;
    int win_screen_x = win_origin_x + x_offset;
    int win_screen_start = (win_origin_x <= 0) ? 0 : win_screen_x;

    /*
     * The extension columns are only trustworthy when they hold real map data.
     *
     * A Game Boy tilemap is 32 tiles (256px) wide but the console only ever
     * shows 20 of those columns, so columns 20-31 hold whatever the game last
     * left there. While scrolling a room the game keeps them updated and the
     * extension is genuine; on a UI screen - the item viewer, the inventory, a
     * menu - it is stale font and tile garbage, which is what showed up down
     * both sides of the item pickup screen.
     *
     * There is no data to invent for those pixels, so they are painted black
     * rather than filled with whatever happens to be in VRAM.
     */
    const bool ui_covers_screen = win_enable && (wx <= 166) && (wy <= FULLSCREEN_UI_MAX_WY);

    /*
     * On the game's UI screens (inventory, item info, PDA, menus) only the 20
     * visible tile columns are written; the rest of the map still holds
     * whatever the player was standing in, which is where the stray font tiles
     * down both sides of the item screen came from.
     */
    const bool extension_untrusted =
        ui_covers_screen || gb_state_is_ui_screen(ctx) || !bg_enable;

    // Buffer to track BG priority and color index per pixel
    static uint8_t bg_color_idx[336];
    static uint8_t bg_priority_flags[336];

    for (int y = 0; y < GB_NATIVE_HEIGHT; y++) {
        uint32_t* row_dst = &out_fb[y * target_w];
        int bg_y = (y + (int)scy) & 0xFF;
        uint8_t tile_y = (uint8_t)(bg_y / 8);
        uint8_t fine_y = (uint8_t)(bg_y % 8);

        bool line_in_window = win_enable && (y >= (int)wy);

        /* A window row shows UI, not world: nothing to extend outwards. */
        const bool blank_extension = extension_untrusted || line_in_window;

        // 1. Render Background & Window across full width
        for (int x = 0; x < target_w; x++) {
            const bool x_in_native = (x >= x_offset) && (x < x_offset + GB_NATIVE_WIDTH);

            if (blank_extension && !x_in_native) {
                bg_color_idx[x] = 0;
                bg_priority_flags[x] = 0;
                row_dst[x] = 0xFF000000u;
                continue;
            }

            bool pixel_in_win = line_in_window && (x >= win_screen_start);
            uint16_t tilemap_base;
            uint8_t tile_x, t_y, f_x, f_y;

            if (pixel_in_win) {
                int win_x = x - win_screen_x;
                if (win_x < 0) win_x = 0;
                if (win_x >= win_native_w) win_x = win_native_w - 1;
                int win_y = y - (int)wy;
                tilemap_base = win_tilemap_offset;
                tile_x = (uint8_t)((win_x / 8) & 0x1F);
                t_y = (uint8_t)((win_y / 8) & 0x1F);
                f_x = (uint8_t)(win_x % 8);
                f_y = (uint8_t)(win_y % 8);
            } else {
                int world_x = (x + cam_x) & 0xFF;
                tilemap_base = bg_tilemap_offset;
                tile_x = (uint8_t)(world_x / 8);
                t_y = tile_y;
                f_x = (uint8_t)(world_x % 8);
                f_y = fine_y;
            }

            uint16_t map_idx = tilemap_base + (t_y * 32) + tile_x;
            uint8_t tile_idx = vram[map_idx];
            uint8_t attr = vram[VRAM_SIZE + map_idx]; // CGB attributes bank 1

            uint8_t pal_idx = attr & 0x07;
            uint8_t tile_bank = (attr & 0x08) ? 1 : 0;
            bool flip_x = (attr & 0x20) != 0;
            bool flip_y = (attr & 0x40) != 0;
            bool priority = (attr & 0x80) != 0;

            uint8_t px_y = flip_y ? (7 - f_y) : f_y;
            uint8_t px_x = flip_x ? (7 - f_x) : f_x;

            uint16_t tile_offset = unsigned_tile_data
                ? (uint16_t)(tile_idx * 16 + px_y * 2)
                : (uint16_t)(0x1000 + ((int8_t)tile_idx * 16) + px_y * 2);

            uint8_t lo = vram[(tile_bank * VRAM_SIZE) + tile_offset];
            uint8_t hi = vram[(tile_bank * VRAM_SIZE) + tile_offset + 1];

            int bit = 7 - px_x;
            uint8_t color_idx = (uint8_t)(((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));

            bg_color_idx[x] = color_idx;
            bg_priority_flags[x] = priority ? 1 : 0;

            size_t pal_byte_idx = (size_t)pal_idx * 8 + (size_t)color_idx * 2;
            uint16_t cgb_color = (uint16_t)(ppu->bg_palette_ram[pal_byte_idx] | (ppu->bg_palette_ram[pal_byte_idx + 1] << 8));
            row_dst[x] = rgb555_to_rgba(cgb_color);
        }

        // 2. Render Sprites (OAM) across full width
        if (obj_enable) {
            uint8_t spr_height = obj_8x16 ? 16 : 8;

            for (int i = 0; i < 40; i++) {
                const uint8_t* sprite = ctx->oam + (i * 4);
                int spr_y = (int)sprite[0] - 16;
                int spr_x = (int)sprite[1] - 8 + x_offset;
                uint8_t tile_idx = sprite[2];
                uint8_t flags = sprite[3];

                if (y < spr_y || y >= spr_y + spr_height) {
                    continue;
                }

                int line = y - spr_y;
                bool flip_x = (flags & OAM_FLIP_X) != 0;
                bool flip_y = (flags & OAM_FLIP_Y) != 0;
                bool behind_bg = (flags & OAM_PRIORITY) != 0;
                uint8_t pal_idx = flags & OAM_CGB_PALETTE;
                uint8_t tile_bank = (flags & OAM_CGB_BANK) ? 1 : 0;

                if (obj_8x16) {
                    tile_idx &= 0xFE;
                    if (flip_y) line = 15 - line;
                    if (line >= 8) {
                        tile_idx |= 1;
                        line -= 8;
                    }
                } else if (flip_y) {
                    line = 7 - line;
                }

                uint16_t tile_offset = (uint16_t)(tile_idx * 16 + line * 2);
                uint8_t lo = vram[(tile_bank * VRAM_SIZE) + tile_offset];
                uint8_t hi = vram[(tile_bank * VRAM_SIZE) + tile_offset + 1];

                for (int px = 0; px < 8; px++) {
                    int screen_x = spr_x + px;
                    if (screen_x < 0 || screen_x >= target_w) {
                        continue;
                    }
                    /* Keep sprites out of the blanked side columns so nothing
                     * floats in the black. */
                    if (blank_extension &&
                        (screen_x < x_offset || screen_x >= x_offset + GB_NATIVE_WIDTH)) {
                        continue;
                    }

                    int bit = flip_x ? px : (7 - px);
                    uint8_t color_idx = (uint8_t)(((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));

                    if (color_idx == 0) {
                        continue; // Transparent sprite pixel
                    }

                    // Priority handling: if behind BG and BG color != 0, skip
                    if (behind_bg && bg_color_idx[screen_x] != 0) {
                        continue;
                    }
                    if (bg_priority_flags[screen_x] && bg_color_idx[screen_x] != 0) {
                        continue;
                    }

                    size_t pal_byte_idx = (size_t)pal_idx * 8 + (size_t)color_idx * 2;
                    uint16_t cgb_color = (uint16_t)(ppu->obj_palette_ram[pal_byte_idx] | (ppu->obj_palette_ram[pal_byte_idx + 1] << 8));
                    row_dst[screen_x] = rgb555_to_rgba(cgb_color);
                }
            }
        }
    }

    // Replace the native viewport with the PPU's dot-accurate output.
    if (native_fb) {
        blit_native_center(out_fb, target_w, native_fb);
    }
}
