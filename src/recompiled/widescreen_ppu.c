#include "widescreen_ppu.h"
#include "ppu.h"
#include "game_state.h"
#include <string.h>

uint32_t g_wide_framebuffer[GB_MAX_FRAMEBUFFER_SIZE];

/* The GB's 32x32 tilemap is a streaming ring, not the world map. Keep only
 * tiles observed in the native viewport, tagged with unwrapped coordinates.
 * Store pixels as well as attributes: the game can reuse a tile-data slot as
 * soon as a tile leaves the screen. Unknown world tiles remain black.
 *
 * This is background history, not a room decoder. It cannot reveal scenery
 * never visited, reconstruct offscreen actors, or infer an undetectable room
 * change with identical overlapping tiles. Explicit resets cover load/reset.
 */
#define HISTORY_TILES 64
#define MAX_CONTINUOUS_SCROLL 16

typedef struct {
    int32_t x, y;
    uint8_t pixels[16];
    uint8_t attr;
    bool valid;
} BackgroundTile;

static struct {
    const GBContext* context;
    const uint8_t* rom;
    uint64_t cycles;
    int32_t camera_x, camera_y;
    uint8_t scx, scy, mode;
    uint8_t palette[64];
    bool active;
    BackgroundTile tiles[HISTORY_TILES * HISTORY_TILES];
} history;

void widescreen_reset_history(void) {
    if (history.active) memset(&history, 0, sizeof(history));
}

int widescreen_get_target_width(void) {
    switch (g_app_config.widescreen_mode) {
        case ASPECT_WIDESCREEN_16_9: return GB_WIDESCREEN_WIDTH;
        case ASPECT_ULTRAWIDE_21_9: return GB_ULTRAWIDE_WIDTH;
        default: return GB_NATIVE_WIDTH;
    }
}
int widescreen_get_target_height(void) { return GB_NATIVE_HEIGHT; }

static int32_t tile_coordinate(int32_t pixel) {
    return pixel >= 0 ? pixel / 8 : (pixel - 7) / 8;
}
static BackgroundTile* history_tile(int32_t x, int32_t y) {
    /* Unsigned conversion gives defined modulo for negative coordinates. */
    return &history.tiles[((uint32_t)y & (HISTORY_TILES-1)) * HISTORY_TILES +
                          ((uint32_t)x & (HISTORY_TILES-1))];
}
static int scroll_delta(uint8_t current, uint8_t previous) {
    int delta = (int)current - previous;
    if (delta > 127) delta -= 256;
    if (delta < -128) delta += 256;
    return delta;
}
static void read_visible_tile(const GBContext* ctx, int32_t x, int32_t y,
                              BackgroundTile* tile) {
    uint8_t lcdc = ctx->io[0x40];
    unsigned map = (lcdc & LCDC_BG_TILEMAP) ? 0x1c00u : 0x1800u;
    unsigned address = map + ((uint32_t)y & 31u) * 32u + ((uint32_t)x & 31u);
    uint8_t index = ctx->vram[address];
    tile->attr = ctx->vram[VRAM_SIZE + address];
    unsigned bank = (tile->attr & OAM_CGB_BANK) ? VRAM_SIZE : 0;
    int offset = (lcdc & LCDC_TILE_DATA) ? index * 16 : 0x1000 + (int8_t)index * 16;
    memcpy(tile->pixels, ctx->vram + bank + offset, 16);
    tile->x = x; tile->y = y; tile->valid = true;
}
static bool same_tile(const BackgroundTile* a, const BackgroundTile* b) {
    return a->attr == b->attr && memcmp(a->pixels, b->pixels, 16) == 0;
}
static void observe_background(GBContext* ctx, const GBPPU* ppu) {
    uint8_t scx = ctx->io[0x43], scy = ctx->io[0x42];
    uint8_t mode = ctx->io[0x40] & (LCDC_TILE_DATA | LCDC_BG_TILEMAP);
    int dx = scroll_delta(scx, history.scx), dy = scroll_delta(scy, history.scy);
    if (history.active &&
        (history.context != ctx || history.rom != ctx->rom ||
         ctx->total_cycles < history.cycles || history.mode != mode ||
         dx > MAX_CONTINUOUS_SCROLL || dx < -MAX_CONTINUOUS_SCROLL ||
         dy > MAX_CONTINUOUS_SCROLL || dy < -MAX_CONTINUOUS_SCROLL ||
         memcmp(history.palette, ppu->bg_palette_ram, 64) != 0)) {
        widescreen_reset_history();
    }
    int32_t camera_x = history.active ? history.camera_x + dx : scx;
    int32_t camera_y = history.active ? history.camera_y + dy : scy;
    if (camera_x > 1000000 || camera_x < -1000000 ||
        camera_y > 1000000 || camera_y < -1000000) {
        widescreen_reset_history(); camera_x = scx; camera_y = scy;
    }
    const int32_t left = tile_coordinate(camera_x), top = tile_coordinate(camera_y);
    const int32_t right = tile_coordinate(camera_x + GB_NATIVE_WIDTH - 1);
    const int32_t bottom = tile_coordinate(camera_y + GB_NATIVE_HEIGHT - 1);
    /* If overlapping scenery changes, discard history before recording the
     * new frame. Conservative for animated backgrounds, safe for room changes.
     * Do not compare unobserved ring cells: they may already have been reused. */
    bool conflict = false;
    for (int32_t y = top; y <= bottom && !conflict; ++y) {
        for (int32_t x = left; x <= right; ++x) {
            BackgroundTile current;
            read_visible_tile(ctx, x, y, &current);
            const BackgroundTile* old = history_tile(x, y);
            if (old->valid && old->x == x && old->y == y && !same_tile(old, &current)) {
                conflict = true; break;
            }
        }
    }
    if (conflict) widescreen_reset_history();
    for (int32_t y = top; y <= bottom; ++y)
        for (int32_t x = left; x <= right; ++x)
            read_visible_tile(ctx, x, y, history_tile(x, y));
    history.context = ctx; history.rom = ctx->rom; history.cycles = ctx->total_cycles;
    history.camera_x = camera_x; history.camera_y = camera_y;
    history.scx = scx; history.scy = scy; history.mode = mode;
    memcpy(history.palette, ppu->bg_palette_ram, 64);
    history.active = true;
}
static uint32_t background_pixel(const GBPPU* ppu, int32_t x, int32_t y) {
    int32_t tx = tile_coordinate(x), ty = tile_coordinate(y);
    const BackgroundTile* tile = history_tile(tx, ty);
    if (!tile->valid || tile->x != tx || tile->y != ty) return 0xff000000u;
    unsigned px = (uint32_t)x & 7u, py = (uint32_t)y & 7u;
    if (tile->attr & 0x20) px = 7-px;
    if (tile->attr & 0x40) py = 7-py;
    unsigned bit = 7-px;
    unsigned color = ((tile->pixels[py*2] >> bit) & 1u) |
                     (((tile->pixels[py*2+1] >> bit) & 1u) << 1);
    unsigned offset = (tile->attr & 7u)*8u + color*2u;
    uint16_t rgb = (uint16_t)(ppu->bg_palette_ram[offset] |
                             (ppu->bg_palette_ram[offset+1] << 8));
    return 0xff000000u | (((rgb & 31u)*255u/31u) << 16) |
           ((((rgb >> 5) & 31u)*255u/31u) << 8) | (((rgb >> 10) & 31u)*255u/31u);
}
void widescreen_render_frame(GBContext* ctx, const uint32_t* native_fb,
                             uint32_t* out_fb, int* out_width, int* out_height) {
    if (!ctx || !out_fb) return;
    int width = widescreen_get_target_width();
    int offset = (width-GB_NATIVE_WIDTH)/2;
    if (out_width) *out_width = width;
    if (out_height) *out_height = GB_NATIVE_HEIGHT;
    if (!native_fb) native_fb = gb_get_framebuffer(ctx);
    if (offset == 0 && native_fb) {
        widescreen_reset_history();
        memcpy(out_fb, native_fb, GB_NATIVE_WIDTH * GB_NATIVE_HEIGHT * sizeof(uint32_t));
        return;
    }
    const GBPPU* ppu = (const GBPPU*)ctx->ppu;
    uint8_t lcdc = ctx->io ? ctx->io[0x40] : 0;
    /* Window rows can use raster effects/HDMA. A single end-frame snapshot
     * cannot identify their source pixels reliably, so invalidate on any
     * visible window rather than retain UI tiles as room history. */
    bool visible_window = (lcdc & LCDC_WINDOW_ENABLE) && ctx->io[0x4b] <= 166 &&
                          ctx->io[0x4a] < GB_NATIVE_HEIGHT;
    bool trusted = ppu && ctx->vram && (lcdc & LCDC_LCD_ENABLE) &&
                   (lcdc & LCDC_BG_ENABLE) && !gb_state_is_ui_screen(ctx) && !visible_window;
    if (trusted) observe_background(ctx, ppu);
    else widescreen_reset_history();
    for (int y = 0; y < GB_NATIVE_HEIGHT; ++y) {
        for (int x = 0; x < width; ++x) {
            if (x >= offset && x < offset+GB_NATIVE_WIDTH && native_fb)
                out_fb[y*width+x] = native_fb[y*GB_NATIVE_WIDTH+x-offset];
            else
                out_fb[y*width+x] = trusted
                    ? background_pixel(ppu, history.camera_x+x-offset, history.camera_y+y)
                    : 0xff000000u;
        }
    }
}
