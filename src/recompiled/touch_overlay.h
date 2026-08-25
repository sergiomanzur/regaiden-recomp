#ifndef RE_TOUCH_OVERLAY_H
#define RE_TOUCH_OVERLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include "gbrt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool enabled;                  // Master toggle for touch overlay
    bool visible;                  // Dynamic show/hide state of on-screen gamepad
    bool auto_hide_on_controller;  // Auto-hide when physical gamepad is active
    bool controller_active;        // State tracking for physical controller
    int opacity_pct;               // 10 to 100%
    bool haptic_feedback;          // Vibration on touch
    float scale;                   // Button scale multiplier (0.6x to 1.5x)
} TouchOverlayConfig;

extern TouchOverlayConfig g_touch_overlay_config;

void touch_overlay_init(void);
void touch_overlay_handle_event(const SDL_Event* event, int window_w, int window_h);
void touch_overlay_render(SDL_Renderer* renderer, int window_w, int window_h);
uint8_t touch_overlay_get_dpad_mask(void);
uint8_t touch_overlay_get_buttons_mask(void);
bool touch_overlay_menu_requested(void);
void touch_overlay_clear_menu_request(void);

#ifdef __cplusplus
}
#endif

#endif // RE_TOUCH_OVERLAY_H
