#ifndef RE_POSTPROCESS_H
#define RE_POSTPROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include "gbrt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COLOR_GRADE_OFF = 0,
    COLOR_GRADE_COLD_BIOHAZARD = 1,
    COLOR_GRADE_BLEACH_BYPASS = 2,
    COLOR_GRADE_SEPIA_RETRO = 3,
    COLOR_GRADE_MONOCHROME = 4
} ColorGradeMode;

typedef struct {
    bool vignette_enabled;
    int vignette_intensity;    // 0 to 100

    bool film_grain_enabled;
    int grain_intensity;       // 0 to 100

    bool scanlines_enabled;
    int scanline_intensity;    // 0 to 100

    bool crt_mask_enabled;
    int crt_mask_intensity;    // 0 to 100

    ColorGradeMode color_grade;
} PostProcessConfig;

extern PostProcessConfig g_postprocess_config;

void postprocess_init(void);
void postprocess_apply(GBContext* ctx, uint32_t* framebuffer, int width, int height);

#ifdef __cplusplus
}
#endif

#endif // RE_POSTPROCESS_H
