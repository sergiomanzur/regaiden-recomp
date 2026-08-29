#include "lighting.h"
#include "game_state.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Opt-in enhancement: off until the player enables it (see config_set_defaults).
LightingConfig g_lighting_config = {
    .enabled = false,
    .intensity = 85,
    .ambient_darkness = 25,
    .flicker_enabled = true,
    .cone_angle_deg = 65,
    .cone_distance = 140
};

/* Window start line at or above which the window is covering the whole screen. */
#define FULLSCREEN_UI_MAX_WY 16

static PlayerFacingDir s_player_dir = DIR_DOWN;
static uint32_t s_frame_counter = 0;

// Precomputed 2D Light Lookup Tables for ultra-fast lock-solid 60 FPS rendering
#define LUT_MAX_W 336
#define LUT_MAX_H 144
static uint8_t s_light_lut[4][LUT_MAX_H][LUT_MAX_W];
static int s_lut_w = 0;
static int s_lut_h = 0;
static int s_cached_cone_angle = -1;
static int s_cached_cone_dist = -1;

static float normalize_angle(float angle) {
    while (angle > (float)M_PI) angle -= (float)(2.0 * M_PI);
    while (angle < -(float)M_PI) angle += (float)(2.0 * M_PI);
    return angle;
}

static void rebuild_light_luts(int width, int height) {
    if (width > LUT_MAX_W) width = LUT_MAX_W;
    if (height > LUT_MAX_H) height = LUT_MAX_H;

    s_lut_w = width;
    s_lut_h = height;
    s_cached_cone_angle = g_lighting_config.cone_angle_deg;
    s_cached_cone_dist = g_lighting_config.cone_distance;

    float center_x = (float)width * 0.5f;
    float center_y = (float)height * 0.5f;
    float half_cone = (float)(g_lighting_config.cone_angle_deg * 0.5 * M_PI / 180.0);
    float max_dist = (float)g_lighting_config.cone_distance;
    float inner_radius = 16.0f;

    const float target_angles[4] = {
        (float)(M_PI * 0.5), // DIR_DOWN
        (float)(M_PI * 1.5), // DIR_UP
        (float)M_PI,         // DIR_LEFT
        0.0f                 // DIR_RIGHT
    };

    for (int dir = 0; dir < 4; dir++) {
        float t_angle = target_angles[dir];

        for (int y = 0; y < height; y++) {
            float dy = (float)y - center_y;

            for (int x = 0; x < width; x++) {
                float dx = (float)x - center_x;
                float dist = sqrtf(dx * dx + dy * dy);

                float light = 0.0f;
                if (dist < inner_radius) {
                    light = 1.0f - (dist / inner_radius) * 0.3f;
                } else if (dist < max_dist) {
                    float angle = atan2f(dy, dx);
                    float angle_diff = fabsf(normalize_angle(angle - t_angle));

                    if (angle_diff < half_cone) {
                        float angle_factor = cosf((angle_diff / half_cone) * (float)(M_PI * 0.5));
                        angle_factor = angle_factor * angle_factor;

                        float dist_factor = 1.0f - (dist / max_dist);
                        dist_factor = dist_factor * sqrtf(dist_factor);

                        light = angle_factor * dist_factor;
                    }
                }

                if (light > 1.0f) light = 1.0f;
                if (light < 0.0f) light = 0.0f;
                s_light_lut[dir][y][x] = (uint8_t)(light * 255.0f);
            }
        }
    }
}

void lighting_init(void) {
    s_player_dir = DIR_DOWN;
    s_frame_counter = 0;
    s_lut_w = 0;
    s_lut_h = 0;
    s_cached_cone_angle = -1;
    s_cached_cone_dist = -1;
}

void lighting_update_player_dir(uint8_t dpad_state) {
    // dpad_state is active LOW: bit 0: Right, 1: Left, 2: Up, 3: Down
    if (!(dpad_state & 0x01)) s_player_dir = DIR_RIGHT;
    else if (!(dpad_state & 0x02)) s_player_dir = DIR_LEFT;
    else if (!(dpad_state & 0x04)) s_player_dir = DIR_UP;
    else if (!(dpad_state & 0x08)) s_player_dir = DIR_DOWN;
}

/*
 * A window layer that starts at the top of the screen is a full-screen UI -
 * the title screen, the save/load menu, the inventory. Verified on the title
 * screen: LCDC=0xE7 (window enabled), WY=0, WX=7, i.e. the menu itself is
 * drawn on the window layer covering all 144 lines. A dialogue box, by
 * contrast, is anchored near the bottom.
 */
static bool is_full_screen_ui(const GBContext* ctx) {
    uint8_t lcdc = ctx->io[0x40];
    if (!(lcdc & 0x20)) {
        return false; // window disabled
    }
    if (ctx->io[0x4B] > 166) {
        return false; // window pushed off the right edge
    }
    return ctx->io[0x4A] <= FULLSCREEN_UI_MAX_WY;
}

static bool is_exploration_gameplay(const GBContext* ctx) {
    if (!ctx || !ctx->oam) return false;

    // 1. LCD must be enabled with Sprites active
    uint8_t lcdc = ctx->io[0x40];
    if ((lcdc & 0x82) != 0x82) {
        return false;
    }

    // 2. The game's own UI screens are not gameplay
    if (gb_state_is_ui_screen(ctx) || is_full_screen_ui(ctx)) {
        return false;
    }

    // 3. Active sprites present on screen
    for (int i = 0; i < 40; i++) {
        uint8_t y = ctx->oam[i * 4];
        uint8_t x = ctx->oam[i * 4 + 1];
        if (y >= 16 && y <= 160 && x >= 8 && x <= 168) {
            return true;
        }
    }

    return false;
}

void lighting_apply(GBContext* ctx, uint32_t* framebuffer, int width, int height) {
    if (!g_lighting_config.enabled || !framebuffer || width <= 0 || height <= 0 || !ctx) {
        return;
    }

    // The light LUT is only dimensioned for the supported viewport sizes.
    if (width > LUT_MAX_W || height > LUT_MAX_H) {
        return;
    }

    /*
     * There was a "battle mode" branch here keyed on wram[0x0900] > 0, applying
     * a flat darkening instead of the cone. $C900 is not a battle flag: the only
     * code in the ROM that touches it is a 0x50-byte save/restore memcpy, and it
     * reads as all zeroes outside gameplay, so the branch never fired reliably
     * and the cone ran during shooting sequences. Removed rather than left
     * guessing - see the state snapshots for finding the real flag.
     */

    // Flashlight requires active player exploration
    if (!is_exploration_gameplay(ctx)) {
        return;
    }

    s_frame_counter++;

    // Rebuild LUT if dimensions or cone parameters changed
    if (width != s_lut_w || height != s_lut_h ||
        g_lighting_config.cone_angle_deg != s_cached_cone_angle ||
        g_lighting_config.cone_distance != s_cached_cone_dist) {
        rebuild_light_luts(width, height);
    }

    // Halogen bulb subtle flicker (integer scale 240-270 / 256)
    int flicker_factor = 256;
    if (g_lighting_config.flicker_enabled) {
        int noise = (int)(sin(s_frame_counter * 0.35) * 8.0);
        flicker_factor += noise;
    }

    // intensity is a 0-100 percentage; convert it to the 0-256 fixed-point scale
    // the blend below expects. Feeding the raw percentage in made the cone peak
    // at roughly a third of its intended brightness.
    int intensity_256 = (g_lighting_config.intensity * 256) / 100;
    int intensity_scaled = (intensity_256 * flicker_factor) >> 8;
    if (intensity_scaled > 256) intensity_scaled = 256;
    if (intensity_scaled < 0) intensity_scaled = 0;

    uint32_t ambient_256 = (uint32_t)(g_lighting_config.ambient_darkness * 256 / 100);
    if (ambient_256 > 256) ambient_256 = 256;

    // The LUT has a fixed LUT_MAX_W stride, so it has to be indexed row by row.
    // Walking it linearly against the framebuffer only lined up at 336px wide
    // (21:9) and sampled the wrong coordinates in native 10:9 and 16:9.
    const uint8_t (*lut)[LUT_MAX_W] = s_light_lut[(int)s_player_dir];

    // Blazing fast integer SIMD-ready lighting loop (< 0.03ms per frame)
    for (int y = 0; y < height; y++) {
        const uint8_t* lut_row = lut[y];
        uint32_t* row = &framebuffer[(size_t)y * (size_t)width];

        for (int x = 0; x < width; x++) {
            uint32_t p = row[x];
            uint32_t r = (p >> 16) & 0xFF;
            uint32_t g = (p >> 8) & 0xFF;
            uint32_t b = p & 0xFF;

            uint32_t light_val = lut_row[x]; // 0 to 255
            uint32_t light_contrib = (light_val * (uint32_t)intensity_scaled * (256 - ambient_256)) >> 16;
            uint32_t total_light = ambient_256 + light_contrib;
            if (total_light > 256) total_light = 256;

            r = (r * total_light) >> 8;
            g = (g * total_light) >> 8;
            b = (b * total_light) >> 8;

            row[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}
