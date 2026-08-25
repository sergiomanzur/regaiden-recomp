#include "postprocess.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

PostProcessConfig g_postprocess_config = {
    .vignette_enabled = true,
    .vignette_intensity = 45,
    .film_grain_enabled = true,
    .grain_intensity = 25,
    .scanlines_enabled = true,
    .scanline_intensity = 30,
    .crt_mask_enabled = false,
    .crt_mask_intensity = 20,
    .color_grade = COLOR_GRADE_COLD_BIOHAZARD
};

#define VIG_LUT_MAX_W 336
#define VIG_LUT_MAX_H 144
static uint8_t s_vignette_lut[VIG_LUT_MAX_H][VIG_LUT_MAX_W];
static int s_vig_w = 0;
static int s_vig_h = 0;
static int s_vig_cached_int = -1;

static uint32_t s_noise_seed = 0x12345678;

static inline uint32_t fast_rand(void) {
    s_noise_seed = (s_noise_seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return s_noise_seed;
}

static inline uint8_t clamp_u8(int val) {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return (uint8_t)val;
}

static void rebuild_vignette_lut(int width, int height, int intensity) {
    if (width > VIG_LUT_MAX_W) width = VIG_LUT_MAX_W;
    if (height > VIG_LUT_MAX_H) height = VIG_LUT_MAX_H;

    s_vig_w = width;
    s_vig_h = height;
    s_vig_cached_int = intensity;

    float vig_int = (float)intensity / 100.0f;
    float cx = (float)width * 0.5f;
    float cy = (float)height * 0.5f;
    float max_radius_sq = (cx * cx + cy * cy);

    for (int y = 0; y < height; y++) {
        float dy = (float)y - cy;
        for (int x = 0; x < width; x++) {
            float dx = (float)x - cx;
            float dist_sq = dx * dx + dy * dy;
            float vig = 1.0f - (dist_sq / max_radius_sq) * vig_int;
            if (vig < 0.2f) vig = 0.2f;
            if (vig > 1.0f) vig = 1.0f;
            s_vignette_lut[y][x] = (uint8_t)(vig * 256.0f);
        }
    }
}

void postprocess_init(void) {
    s_noise_seed = 0x12345678;
    s_vig_w = 0;
    s_vig_h = 0;
    s_vig_cached_int = -1;
}

void postprocess_apply(GBContext* ctx, uint32_t* framebuffer, int width, int height) {
    (void)ctx;
    if (!framebuffer || width <= 0 || height <= 0) {
        return;
    }

    bool vig_on = g_postprocess_config.vignette_enabled && (g_postprocess_config.vignette_intensity > 0);
    bool grain_on = g_postprocess_config.film_grain_enabled && (g_postprocess_config.grain_intensity > 0);
    bool scan_on = g_postprocess_config.scanlines_enabled && (g_postprocess_config.scanline_intensity > 0);
    bool crt_on = g_postprocess_config.crt_mask_enabled && (g_postprocess_config.crt_mask_intensity > 0);
    ColorGradeMode grade = g_postprocess_config.color_grade;

    if (!vig_on && !grain_on && !scan_on && !crt_on && grade == COLOR_GRADE_OFF) {
        return;
    }

    if (vig_on) {
        if (width != s_vig_w || height != s_vig_h || g_postprocess_config.vignette_intensity != s_vig_cached_int) {
            rebuild_vignette_lut(width, height, g_postprocess_config.vignette_intensity);
        }
    }

    uint32_t scanline_factor_256 = scan_on ? (256 - (g_postprocess_config.scanline_intensity * 128 / 100)) : 256;
    uint32_t grain_amp = grain_on ? (uint32_t)(g_postprocess_config.grain_intensity * 30 / 100) : 0;
    uint32_t crt_atten = crt_on ? (uint32_t)(g_postprocess_config.crt_mask_intensity * 80 / 100) : 0;

    for (int y = 0; y < height; y++) {
        bool is_scanline = (y % 2) != 0;
        uint32_t row_scan = is_scanline ? scanline_factor_256 : 256;
        const uint8_t* vig_row = vig_on ? &s_vignette_lut[y][0] : NULL;

        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            uint32_t p = framebuffer[idx];

            int r = (p >> 16) & 0xFF;
            int g = (p >> 8) & 0xFF;
            int b = p & 0xFF;

            // 1. Color Grading Profiles (fast integer math)
            if (grade == COLOR_GRADE_COLD_BIOHAZARD) {
                int lum = (r * 30 + g * 59 + b * 11) >> 8;
                r = ((r * 192 + lum * 64) >> 8) - 5;
                g = (g * 217 + lum * 38) >> 8;
                b = ((b * 204 + lum * 51) >> 8) + 15;
            } else if (grade == COLOR_GRADE_BLEACH_BYPASS) {
                int lum = (r * 30 + g * 59 + b * 11) >> 8;
                r = (r + lum) >> 1;
                g = (g + lum) >> 1;
                b = (b + lum) >> 1;
                r = (r < 128) ? ((r * r) >> 7) : (255 - (((255 - r) * (255 - r)) >> 7));
                g = (g < 128) ? ((g * g) >> 7) : (255 - (((255 - g) * (255 - g)) >> 7));
                b = (b < 128) ? ((b * b) >> 7) : (255 - (((255 - b) * (255 - b)) >> 7));
            } else if (grade == COLOR_GRADE_SEPIA_RETRO) {
                int lum = (r * 30 + g * 59 + b * 11) >> 8;
                r = lum + 25;
                g = lum + 10;
                b = lum - 20;
            } else if (grade == COLOR_GRADE_MONOCHROME) {
                int lum = (r * 30 + g * 59 + b * 11) >> 8;
                r = lum;
                g = lum;
                b = lum;
            }

            // 2. Scanline filter
            if (row_scan < 256) {
                r = (r * (int)row_scan) >> 8;
                g = (g * (int)row_scan) >> 8;
                b = (b * (int)row_scan) >> 8;
            }

            // 3. CRT Phosphor Triad Mask
            if (crt_atten > 0) {
                int subpix = x % 3;
                if (subpix != 0) r = (r * (int)(256 - crt_atten)) >> 8;
                if (subpix != 1) g = (g * (int)(256 - crt_atten)) >> 8;
                if (subpix != 2) b = (b * (int)(256 - crt_atten)) >> 8;
            }

            // 4. Vignette Shadow (LUT lookup)
            if (vig_row) {
                uint32_t vig_val = vig_row[x];
                r = (r * (int)vig_val) >> 8;
                g = (g * (int)vig_val) >> 8;
                b = (b * (int)vig_val) >> 8;
            }

            // 5. Film Grain Noise
            if (grain_amp > 0) {
                int noise = ((int)(fast_rand() % 51) - 25) * (int)grain_amp / 25;
                r += noise;
                g += noise;
                b += noise;
            }

            framebuffer[idx] = 0xFF000000u | ((uint32_t)clamp_u8(r) << 16) | ((uint32_t)clamp_u8(g) << 8) | (uint32_t)clamp_u8(b);
        }
    }
}
