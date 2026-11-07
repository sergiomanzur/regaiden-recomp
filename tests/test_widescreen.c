#include "widescreen_ppu.h"
#include "ppu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AppConfig g_app_config;
static GBContext ctx;
static GBPPU ppu;
static uint8_t vram[2 * VRAM_SIZE], io[0x80], oam[OAM_SIZE];
static uint32_t native[160 * 144], output[GB_MAX_FRAMEBUFFER_SIZE];
const uint32_t* gb_get_framebuffer(GBContext* context) {
    (void)context;
    return native;
}
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); } } while (0)

static void render(void) {
    int w = 0, h = 0;
    widescreen_render_frame(&ctx, native, output, &w, &h);
    CHECK(w == (g_app_config.widescreen_mode == ASPECT_ULTRAWIDE_21_9 ? 336 : 256));
    CHECK(h == 144);
}
static void setup(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(&ppu, 0, sizeof(ppu));
    memset(vram, 0, sizeof(vram));
    memset(io, 0, sizeof(io));
    memset(oam, 0, sizeof(oam));
    ctx.ppu = &ppu; ctx.vram = vram; ctx.io = io; ctx.oam = oam;
    ctx.wram_bank = 1;
    g_app_config.widescreen_mode = ASPECT_ULTRAWIDE_21_9;
    render(); /* LCD-off frame invalidates a previous scene. */
    io[0x40] = LCDC_LCD_ENABLE | LCDC_BG_ENABLE | LCDC_TILE_DATA;
    /* Every map cell uses an opaque red tile. The tile bytes are synthetic. */
    memset(vram, 0xff, 16);
    ppu.bg_palette_ram[6] = 0x1f;
    for (int i = 0; i < 160 * 144; ++i) native[i] = 0xff123456u + (uint32_t)i;
}
static void check_center(void) {
    int w = g_app_config.widescreen_mode == ASPECT_ULTRAWIDE_21_9 ? 336 : 256;
    for (int y = 0; y < 144; ++y)
        for (int x = 0; x < 160; ++x)
            CHECK(output[y*w + (w-160)/2 + x] == native[y*160+x]);
}
static void fresh_scene_does_not_alias_unseen_ring(void) {
    setup(); render();
    CHECK(output[0] == 0xff000000u);
    CHECK(output[335] == 0xff000000u);
    check_center();
}
static void scrolling_reveals_observed_world_without_reused_vram(void) {
    setup(); render();
    io[0x43] = 8;
    /* Column zero is now offscreen and reused by the game's ring. */
    vram[0x1800] = 1;
    render();
    CHECK(output[80] == 0xffff0000u); /* World x=0, seen last frame. */
    CHECK(output[79] == 0xff000000u); /* World x=-1 never observed. */
    CHECK(output[248] == 0xff000000u); /* World x=168 never observed. */
    check_center();
}
static void scroll_wrap_is_continuous_in_both_axes(void) {
    setup(); io[0x43] = 248; io[0x42] = 248; render();
    io[0x43] = 0; io[0x42] = 0; render();
    CHECK(output[80] == 0xffff0000u); /* Previous x=248 at new world y=256. */
    CHECK(output[79] == 0xff000000u);
}
static void scene_changes_discard_previous_world(void) {
    setup(); render(); io[0x43] = 8; render();
    CHECK(output[80] == 0xffff0000u);
    /* A changed overlapping map cell denotes a different/changed scene. */
    vram[0x1801] = 1; render();
    CHECK(output[80] == 0xff000000u);
    setup(); render(); io[0x43] = 8; render();
    ctx.wram_bank = 2; render(); ctx.wram_bank = 1; render();
    CHECK(output[80] == 0xff000000u);
    setup(); render(); io[0x43] = 8; render();
    io[0x43] = 80; render(); /* A camera teleport cannot imply continuity. */
    CHECK(output[80] == 0xff000000u);
}
static void window_and_palette_changes_do_not_leak_history(void) {
    setup(); render(); io[0x43] = 8; render();
    io[0x40] |= LCDC_WINDOW_ENABLE; io[0x4a] = 100; io[0x4b] = 7;
    render(); CHECK(output[100*336+80] == 0xff000000u);
    io[0x40] &= (uint8_t)~LCDC_WINDOW_ENABLE; render();
    CHECK(output[80] == 0xff000000u);
    setup(); render(); io[0x43] = 8; render();
    ppu.bg_palette_ram[6] = 0xe0; ppu.bg_palette_ram[7] = 3; render();
    CHECK(output[80] == 0xff000000u);
}
static void reused_tile_graphics_and_explicit_reload_discard_history(void) {
    setup(); render(); io[0x43] = 8; render();
    /* Same tile numbers, new graphics: a room/animation must not retain old scenery. */
    vram[0] = 0; render(); CHECK(output[80] == 0xff000000u);
    setup(); render(); io[0x43] = 8; render();
    widescreen_reset_history(); render(); CHECK(output[80] == 0xff000000u);
    setup(); render(); io[0x43] = 8; render();
    ctx.total_cycles = 1000; render(); ctx.total_cycles = 0; render();
    CHECK(output[80] == 0xff000000u);
    setup(); render(); io[0x43] = 8; render();
    GBContext replacement = ctx;
    widescreen_render_frame(&replacement, native, output, NULL, NULL);
    CHECK(output[80] == 0xff000000u);
}
static void negative_scroll_and_vertical_coverage_are_world_relative(void) {
    setup(); render(); io[0x43] = 248; render();
    CHECK(output[248] == 0xffff0000u); /* Old world x=152, now right of viewport. */
    CHECK(output[256] == 0xff000000u); /* World x=160 was never visible. */
    setup(); render(); io[0x43] = 8; io[0x42] = 8; render();
    CHECK(output[135*336+80] == 0xffff0000u); /* World (0,143) was observed. */
    CHECK(output[136*336+80] == 0xff000000u); /* World (0,144) was not. */
}
static void cache_eviction_does_not_alias_distant_world(void) {
    setup(); render();
    for (int step = 1; step <= 90; ++step) {
        io[0x43] = (uint8_t)(step*8); render();
        CHECK(output[248] == 0xff000000u); /* Ring slot may hold a much older tile. */
    }
    CHECK(output[80] == 0xffff0000u);
    check_center();
    g_app_config.widescreen_mode = ASPECT_WIDESCREEN_16_9;
    render(); CHECK(output[40] == 0xffff0000u); check_center();
    g_app_config.widescreen_mode = ASPECT_NATIVE_10_9;
    int w = 0, h = 0;
    widescreen_render_frame(&ctx, native, output, &w, &h);
    CHECK(w == 160 && h == 144);
    CHECK(memcmp(output, native, sizeof(native)) == 0);
    g_app_config.widescreen_mode = ASPECT_WIDESCREEN_16_9;
    render(); CHECK(output[40] == 0xff000000u);
}
int main(void) {
    fresh_scene_does_not_alias_unseen_ring();
    scrolling_reveals_observed_world_without_reused_vram();
    scroll_wrap_is_continuous_in_both_axes();
    scene_changes_discard_previous_world();
    window_and_palette_changes_do_not_leak_history();
    reused_tile_graphics_and_explicit_reload_discard_history();
    negative_scroll_and_vertical_coverage_are_world_relative();
    cache_eviction_does_not_alias_distant_world();
    puts("widescreen regression tests passed");
    return 0;
}
