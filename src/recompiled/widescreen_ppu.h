#ifndef WIDESCREEN_PPU_H
#define WIDESCREEN_PPU_H

#include "gbrt.h"
#include "config_ini.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GB_NATIVE_WIDTH 160
#define GB_NATIVE_HEIGHT 144

#define GB_WIDESCREEN_WIDTH 256
#define GB_WIDESCREEN_HEIGHT 144

#define GB_ULTRAWIDE_WIDTH 336
#define GB_ULTRAWIDE_HEIGHT 144

#define GB_MAX_FRAMEBUFFER_SIZE (GB_ULTRAWIDE_WIDTH * GB_ULTRAWIDE_HEIGHT)

extern uint32_t g_wide_framebuffer[GB_MAX_FRAMEBUFFER_SIZE];

/**
 * @brief Get the current target framebuffer width according to widescreen mode.
 */
int widescreen_get_target_width(void);

/**
 * @brief Get the current target framebuffer height.
 */
int widescreen_get_target_height(void);

/**
 * @brief Render the expanded True Widescreen or Native frame from Game Boy VRAM.
 * @param ctx Game Boy context
 * @param native_fb Dot-accurate 160x144 frame from the PPU, copied verbatim into
 *                  the centre of the output. Pass NULL to sample the PPU directly.
 * @param out_fb Output buffer (at least GB_MAX_FRAMEBUFFER_SIZE uint32_t)
 * @param out_width Output width (160, 256, or 336)
 * @param out_height Output height (144)
 */
void widescreen_render_frame(GBContext* ctx, const uint32_t* native_fb, uint32_t* out_fb,
                             int* out_width, int* out_height);

#ifdef __cplusplus
}
#endif

#endif /* WIDESCREEN_PPU_H */
