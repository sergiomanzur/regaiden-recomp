#include "touch_overlay.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "ReGaidenTouch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#endif

TouchOverlayConfig g_touch_overlay_config = {
#ifdef __ANDROID__
    .enabled = true,
#else
    .enabled = false,
#endif
    .auto_hide_on_controller = true,
    .controller_active = false,
    .opacity_pct = 65,
    .haptic_feedback = true,
    .scale = 1.0f
};

typedef enum {
    TOUCH_BTN_NONE = 0,
    TOUCH_BTN_UP = (1 << 0),
    TOUCH_BTN_DOWN = (1 << 1),
    TOUCH_BTN_LEFT = (1 << 2),
    TOUCH_BTN_RIGHT = (1 << 3),
    TOUCH_BTN_A = (1 << 4),
    TOUCH_BTN_B = (1 << 5),
    TOUCH_BTN_SELECT = (1 << 6),
    TOUCH_BTN_START = (1 << 7),
    TOUCH_BTN_MENU = (1 << 8)
} TouchButtonFlag;

#define MAX_TOUCH_FINGERS 10

typedef struct {
    SDL_FingerID id;
    float x; // 0.0 to 1.0
    float y; // 0.0 to 1.0
    uint32_t active_buttons;
    bool down;
} TouchFinger;

static TouchFinger s_fingers[MAX_TOUCH_FINGERS];
static uint32_t s_active_mask = 0;
static bool s_menu_requested = false;

void touch_overlay_init(void) {
    memset(s_fingers, 0, sizeof(s_fingers));
    s_active_mask = 0;
    s_menu_requested = false;
}

static uint32_t evaluate_point(float x, float y, int screen_w, int screen_h) {
    uint32_t mask = TOUCH_BTN_NONE;
    float px = x * (float)screen_w;
    float py = y * (float)screen_h;
    float scale = (float)screen_h / 480.0f * g_touch_overlay_config.scale;
    if (scale < 0.6f) scale = 0.6f;

    // 1. Virtual D-Pad (Bottom-Left)
    float dpad_cx = 90.0f * scale;
    float dpad_cy = (float)screen_h - (90.0f * scale);
    float dpad_radius = 80.0f * scale;
    float deadzone = 16.0f * scale;

    float ddx = px - dpad_cx;
    float ddy = py - dpad_cy;
    float dist_sq = ddx * ddx + ddy * ddy;

    if (dist_sq <= dpad_radius * dpad_radius && dist_sq >= deadzone * deadzone) {
        float angle = atan2f(ddy, ddx) * (180.0f / 3.14159265f); // -180 to 180
        if (angle >= -157.5f && angle <= -22.5f) mask |= TOUCH_BTN_UP;
        if (angle >= 22.5f && angle <= 157.5f) mask |= TOUCH_BTN_DOWN;
        if (angle >= 112.5f || angle <= -112.5f) mask |= TOUCH_BTN_LEFT;
        if (angle >= -67.5f && angle <= 67.5f) mask |= TOUCH_BTN_RIGHT;
    }

    // 2. Action Button A (Bottom-Right, Upper)
    float a_cx = (float)screen_w - (55.0f * scale);
    float a_cy = (float)screen_h - (105.0f * scale);
    float btn_r = 38.0f * scale;
    if ((px - a_cx) * (px - a_cx) + (py - a_cy) * (py - a_cy) <= btn_r * btn_r) {
        mask |= TOUCH_BTN_A;
    }

    // 3. Action Button B (Bottom-Right, Lower)
    float b_cx = (float)screen_w - (125.0f * scale);
    float b_cy = (float)screen_h - (55.0f * scale);
    if ((px - b_cx) * (px - b_cx) + (py - b_cy) * (py - b_cy) <= btn_r * btn_r) {
        mask |= TOUCH_BTN_B;
    }

    // 4. Select Button (Bottom-Center Left)
    float sel_cx = (float)screen_w * 0.40f;
    float sel_cy = (float)screen_h - (30.0f * scale);
    if (fabsf(px - sel_cx) < 35.0f * scale && fabsf(py - sel_cy) < 20.0f * scale) {
        mask |= TOUCH_BTN_SELECT;
    }

    // 5. Start Button (Bottom-Center Right)
    float start_cx = (float)screen_w * 0.60f;
    float start_cy = (float)screen_h - (30.0f * scale);
    if (fabsf(px - start_cx) < 35.0f * scale && fabsf(py - start_cy) < 20.0f * scale) {
        mask |= TOUCH_BTN_START;
    }

    // 6. Menu Toggle Button (Top-Right)
    float menu_cx = (float)screen_w - (40.0f * scale);
    float menu_cy = 35.0f * scale;
    if ((px - menu_cx) * (px - menu_cx) + (py - menu_cy) * (py - menu_cy) <= (30.0f * scale) * (30.0f * scale)) {
        mask |= TOUCH_BTN_MENU;
    }

    return mask;
}

static void update_aggregate_mask(void) {
    uint32_t new_mask = 0;
    for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
        if (s_fingers[i].down) {
            new_mask |= s_fingers[i].active_buttons;
        }
    }
    s_active_mask = new_mask;
}

void touch_overlay_handle_event(const SDL_Event* event, int window_w, int window_h) {
    if (!event) return;

    // Physical controller wake/sleep detection
    if (event->type == SDL_CONTROLLERBUTTONDOWN || event->type == SDL_CONTROLLERAXISMOTION) {
        g_touch_overlay_config.controller_active = true;
    }

    if (event->type == SDL_FINGERDOWN || event->type == SDL_FINGERUP || event->type == SDL_FINGERMOTION) {
        g_touch_overlay_config.controller_active = false; // Touch takes priority on tap
    }

    if (!g_touch_overlay_config.enabled) return;

    if (event->type == SDL_FINGERDOWN) {
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            if (!s_fingers[i].down) {
                s_fingers[i].id = event->tfinger.fingerId;
                s_fingers[i].x = event->tfinger.x;
                s_fingers[i].y = event->tfinger.y;
                s_fingers[i].down = true;
                s_fingers[i].active_buttons = evaluate_point(event->tfinger.x, event->tfinger.y, window_w, window_h);

                if (s_fingers[i].active_buttons & TOUCH_BTN_MENU) {
                    s_menu_requested = true;
                }
                break;
            }
        }
        update_aggregate_mask();
    } else if (event->type == SDL_FINGERMOTION) {
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            if (s_fingers[i].down && s_fingers[i].id == event->tfinger.fingerId) {
                s_fingers[i].x = event->tfinger.x;
                s_fingers[i].y = event->tfinger.y;
                s_fingers[i].active_buttons = evaluate_point(event->tfinger.x, event->tfinger.y, window_w, window_h);
                break;
            }
        }
        update_aggregate_mask();
    } else if (event->type == SDL_FINGERUP) {
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            if (s_fingers[i].down && s_fingers[i].id == event->tfinger.fingerId) {
                s_fingers[i].down = false;
                s_fingers[i].active_buttons = 0;
                break;
            }
        }
        update_aggregate_mask();
    }
}

static void draw_filled_circle(SDL_Renderer* renderer, int cx, int cy, int radius) {
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

void touch_overlay_render(SDL_Renderer* renderer, int window_w, int window_h) {
    if (!g_touch_overlay_config.enabled || !renderer) return;
    if (g_touch_overlay_config.auto_hide_on_controller && g_touch_overlay_config.controller_active) return;
    if (g_touch_overlay_config.opacity_pct <= 0) return;

    float scale = (float)window_h / 480.0f * g_touch_overlay_config.scale;
    if (scale < 0.6f) scale = 0.6f;

    uint8_t alpha = (uint8_t)(g_touch_overlay_config.opacity_pct * 255 / 100);
    uint8_t active_alpha = (alpha < 200) ? alpha + 55 : 255;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 1. D-Pad Base Circle
    float dpad_cx = 90.0f * scale;
    float dpad_cy = (float)window_h - (90.0f * scale);
    float dpad_radius = 70.0f * scale;

    SDL_SetRenderDrawColor(renderer, 30, 30, 35, alpha / 2);
    draw_filled_circle(renderer, (int)dpad_cx, (int)dpad_cy, (int)dpad_radius);

    // D-Pad Cross Highlights
    int cross_w = (int)(24.0f * scale);
    int cross_len = (int)(44.0f * scale);

    // UP
    bool up_act = (s_active_mask & TOUCH_BTN_UP) != 0;
    SDL_SetRenderDrawColor(renderer, up_act ? 180 : 120, up_act ? 220 : 120, up_act ? 255 : 130, up_act ? active_alpha : alpha);
    SDL_Rect r_up = { (int)dpad_cx - cross_w / 2, (int)dpad_cy - cross_len, cross_w, cross_len / 2 + 4 };
    SDL_RenderFillRect(renderer, &r_up);

    // DOWN
    bool down_act = (s_active_mask & TOUCH_BTN_DOWN) != 0;
    SDL_SetRenderDrawColor(renderer, down_act ? 180 : 120, down_act ? 220 : 120, down_act ? 255 : 130, down_act ? active_alpha : alpha);
    SDL_Rect r_down = { (int)dpad_cx - cross_w / 2, (int)dpad_cy + cross_len / 2 - 4, cross_w, cross_len / 2 + 4 };
    SDL_RenderFillRect(renderer, &r_down);

    // LEFT
    bool left_act = (s_active_mask & TOUCH_BTN_LEFT) != 0;
    SDL_SetRenderDrawColor(renderer, left_act ? 180 : 120, left_act ? 220 : 120, left_act ? 255 : 130, left_act ? active_alpha : alpha);
    SDL_Rect r_left = { (int)dpad_cx - cross_len, (int)dpad_cy - cross_w / 2, cross_len / 2 + 4, cross_w };
    SDL_RenderFillRect(renderer, &r_left);

    // RIGHT
    bool right_act = (s_active_mask & TOUCH_BTN_RIGHT) != 0;
    SDL_SetRenderDrawColor(renderer, right_act ? 180 : 120, right_act ? 220 : 120, right_act ? 255 : 130, right_act ? active_alpha : alpha);
    SDL_Rect r_right = { (int)dpad_cx + cross_len / 2 - 4, (int)dpad_cy - cross_w / 2, cross_len / 2 + 4, cross_w };
    SDL_RenderFillRect(renderer, &r_right);

    // 2. Action Buttons (A & B)
    float a_cx = (float)window_w - (55.0f * scale);
    float a_cy = (float)window_h - (105.0f * scale);
    float b_cx = (float)window_w - (125.0f * scale);
    float b_cy = (float)window_h - (55.0f * scale);
    float btn_r = 34.0f * scale;

    // A Button (Crimson/Red)
    bool a_act = (s_active_mask & TOUCH_BTN_A) != 0;
    SDL_SetRenderDrawColor(renderer, a_act ? 230 : 160, a_act ? 70 : 40, a_act ? 70 : 40, a_act ? active_alpha : alpha);
    draw_filled_circle(renderer, (int)a_cx, (int)a_cy, (int)btn_r);

    // B Button (Amber/Orange)
    bool b_act = (s_active_mask & TOUCH_BTN_B) != 0;
    SDL_SetRenderDrawColor(renderer, b_act ? 230 : 160, b_act ? 160 : 90, b_act ? 40 : 20, b_act ? active_alpha : alpha);
    draw_filled_circle(renderer, (int)b_cx, (int)b_cy, (int)btn_r);

    // 3. Select & Start (Pills)
    float sel_cx = (float)window_w * 0.40f;
    float sel_cy = (float)window_h - (30.0f * scale);
    float start_cx = (float)window_w * 0.60f;
    float start_cy = (float)window_h - (30.0f * scale);
    int pill_w = (int)(50.0f * scale);
    int pill_h = (int)(16.0f * scale);

    bool sel_act = (s_active_mask & TOUCH_BTN_SELECT) != 0;
    SDL_SetRenderDrawColor(renderer, sel_act ? 200 : 100, sel_act ? 200 : 100, sel_act ? 200 : 100, sel_act ? active_alpha : alpha);
    SDL_Rect r_sel = { (int)sel_cx - pill_w / 2, (int)sel_cy - pill_h / 2, pill_w, pill_h };
    SDL_RenderFillRect(renderer, &r_sel);

    bool start_act = (s_active_mask & TOUCH_BTN_START) != 0;
    SDL_SetRenderDrawColor(renderer, start_act ? 200 : 100, start_act ? 200 : 100, start_act ? 200 : 100, start_act ? active_alpha : alpha);
    SDL_Rect r_start = { (int)start_cx - pill_w / 2, (int)start_cy - pill_h / 2, pill_w, pill_h };
    SDL_RenderFillRect(renderer, &r_start);

    // 4. Menu Gear / Icon (Top-Right)
    float menu_cx = (float)window_w - (40.0f * scale);
    float menu_cy = 35.0f * scale;
    bool menu_act = (s_active_mask & TOUCH_BTN_MENU) != 0;
    SDL_SetRenderDrawColor(renderer, menu_act ? 240 : 120, menu_act ? 240 : 120, menu_act ? 240 : 120, menu_act ? active_alpha : alpha);
    draw_filled_circle(renderer, (int)menu_cx, (int)menu_cy, (int)(18.0f * scale));
}

// Active LOW mask for Game Boy D-Pad: Bit 0=Right, 1=Left, 2=Up, 3=Down
uint8_t touch_overlay_get_dpad_mask(void) {
    if (!g_touch_overlay_config.enabled) return 0x0F;
    if (g_touch_overlay_config.auto_hide_on_controller && g_touch_overlay_config.controller_active) return 0x0F;

    uint8_t mask = 0x0F;
    if (s_active_mask & TOUCH_BTN_RIGHT) mask &= ~0x01;
    if (s_active_mask & TOUCH_BTN_LEFT)  mask &= ~0x02;
    if (s_active_mask & TOUCH_BTN_UP)    mask &= ~0x04;
    if (s_active_mask & TOUCH_BTN_DOWN)  mask &= ~0x08;
    return mask;
}

// Active LOW mask for Game Boy Buttons: Bit 0=A, 1=B, 2=Select, 3=Start
uint8_t touch_overlay_get_buttons_mask(void) {
    if (!g_touch_overlay_config.enabled) return 0x0F;
    if (g_touch_overlay_config.auto_hide_on_controller && g_touch_overlay_config.controller_active) return 0x0F;

    uint8_t mask = 0x0F;
    if (s_active_mask & TOUCH_BTN_A)      mask &= ~0x01;
    if (s_active_mask & TOUCH_BTN_B)      mask &= ~0x02;
    if (s_active_mask & TOUCH_BTN_SELECT) mask &= ~0x04;
    if (s_active_mask & TOUCH_BTN_START)  mask &= ~0x08;
    return mask;
}

bool touch_overlay_menu_requested(void) {
    return s_menu_requested;
}

void touch_overlay_clear_menu_request(void) {
    s_menu_requested = false;
}
