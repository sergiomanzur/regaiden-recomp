#ifndef CONFIG_INI_H
#define CONFIG_INI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ASPECT_NATIVE_10_9 = 0,     // Original 160x144 Game Boy Color display
    ASPECT_WIDESCREEN_16_9 = 1, // True 16:9 Widescreen (256x144 - 32x18 tiles)
    ASPECT_ULTRAWIDE_21_9 = 2,  // True 21:9 Ultrawide (336x144 - 42x18 tiles)
    ASPECT_MODE_COUNT = 3
} WidescreenMode;

typedef struct {
    // [Display]
    int widescreen_mode;        // WidescreenMode enum
    int scaling_mode;           // 0=Pixel Perfect, 1=Aspect Fit, 2=Aspect Fill, 3=Stretch
    int filter_mode;            // 0=Nearest, 1=Linear
    bool fullscreen;
    int window_scale;           // 1x to 8x
    bool vsync;
    int palette_idx;
    int orientation_lock;       // 0=Auto (Sensor), 1=Lock Landscape, 2=Lock Portrait

    // [Lighting]
    bool flashlight_enabled;
    int flashlight_intensity;   // 0 - 100
    int ambient_darkness;       // 0 - 100
    bool flashlight_flicker;

    // [Atmosphere]
    bool vignette_enabled;
    int vignette_intensity;     // 0 - 100
    bool film_grain_enabled;
    int grain_intensity;        // 0 - 100
    bool scanlines_enabled;
    int scanline_intensity;     // 0 - 100
    bool crt_mask_enabled;
    int crt_mask_intensity;     // 0 - 100
    int color_grade_mode;       // 0=Off, 1=Cold Biohazard, 2=Bleach Bypass, 3=Sepia, 4=Monochrome

    // [HDPack]
    bool enable_hd_pack;
    char hd_pack_path[256];
    bool enable_hd_backgrounds;
    bool enable_hd_monsters;
    bool enable_hd_portraits;

    // [Audio]
    bool audio_enabled;
    bool audio_muted;
    int audio_volume;           // 0 - 200%
    char audio_device_name[128];
    int audio_latency_ms;

    // [Cheats]
    bool cheat_infinite_health;
    bool cheat_infinite_ammo;
    bool cheat_one_hit_kill;
    bool cheat_freeze_reticle;
    bool cheat_all_weapons;
    bool cheat_infinite_items;

    // [General]
    char last_rom_path[512];
    bool auto_save_state_on_exit;
    int speed_percent;
} AppConfig;

extern AppConfig g_app_config;

/**
 * @brief Initialize configuration with defaults.
 */
void config_set_defaults(AppConfig* cfg);

/**
 * @brief Load configuration from config.ini.
 * @param file_path Optional path; if NULL, uses "config.ini" in current working dir.
 * @return true if loaded successfully.
 */
bool config_load_ini(const char* file_path);

/**
 * @brief Save configuration to config.ini.
 * @param file_path Optional path; if NULL, uses "config.ini" in current working dir.
 * @return true if saved successfully.
 */
bool config_save_ini(const char* file_path);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_INI_H
