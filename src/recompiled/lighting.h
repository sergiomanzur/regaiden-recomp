#ifndef RE_LIGHTING_H
#define RE_LIGHTING_H

#include <stdint.h>
#include <stdbool.h>
#include "gbrt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DIR_DOWN = 0,
    DIR_UP = 1,
    DIR_LEFT = 2,
    DIR_RIGHT = 3
} PlayerFacingDir;

typedef struct {
    bool enabled;
    int intensity;       // 0 to 100
    int ambient_darkness;// 0 (pitch black) to 100 (fully bright)
    bool flicker_enabled;
    int cone_angle_deg;  // e.g. 65 degrees
    int cone_distance;   // e.g. 140 pixels
} LightingConfig;

extern LightingConfig g_lighting_config;

void lighting_init(void);
void lighting_update_player_dir(uint8_t dpad_state);
void lighting_apply(GBContext* ctx, uint32_t* framebuffer, int width, int height);

#ifdef __cplusplus
}
#endif

#endif // RE_LIGHTING_H
