#include "config_ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

AppConfig g_app_config;

/*
 * Where config.ini lives when callers pass NULL.
 *
 * Defaults to the process working directory, which is right for desktop
 * builds. Android has no writable working directory, so every load fell back
 * to compiled defaults and every save silently failed - the platform layer
 * calls config_set_default_path() at startup to point this at app storage.
 */
static char s_default_ini_path[512] = "config.ini";

void config_set_default_path(const char* path) {
    if (!path || !path[0]) {
        return;
    }
    snprintf(s_default_ini_path, sizeof(s_default_ini_path), "%s", path);
}

const char* config_get_default_path(void) {
    return s_default_ini_path;
}

void config_set_defaults(AppConfig* cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    // Defaults deliberately reproduce the original Game Boy Color presentation.
    // Every enhancement (widescreen, flashlight, atmosphere shaders, HD pack) is
    // opt-in from the in-game menu so a first run looks like the real hardware.

    // [Display]
    cfg->widescreen_mode = ASPECT_NATIVE_10_9;
    cfg->scaling_mode = 1; // Aspect Fit
    cfg->filter_mode = 0;  // Nearest
    cfg->fullscreen = false;
    cfg->window_scale = 5;
    cfg->vsync = true;
    cfg->palette_idx = 0;
    cfg->orientation_lock = 0; // 0=Auto (Sensor), 1=Landscape, 2=Portrait

    // [Lighting]
    cfg->flashlight_enabled = false;
    cfg->flashlight_intensity = 85;
    cfg->ambient_darkness = 25;
    cfg->flashlight_flicker = true;

    // [Atmosphere]
    cfg->vignette_enabled = false;
    cfg->vignette_intensity = 45;
    cfg->film_grain_enabled = false;
    cfg->grain_intensity = 25;
    cfg->scanlines_enabled = false;
    cfg->scanline_intensity = 30;
    cfg->crt_mask_enabled = false;
    cfg->crt_mask_intensity = 20;
    cfg->color_grade_mode = 0; // Native GBC Colors

    // [HDPack]
    cfg->enable_hd_pack = false;
    strncpy(cfg->hd_pack_path, "hd_pack", sizeof(cfg->hd_pack_path) - 1);
    cfg->enable_hd_backgrounds = true;
    cfg->enable_hd_monsters = true;
    cfg->enable_hd_portraits = true;

    // [MusicPack] - user-supplied replacement soundtrack, off until enabled
    cfg->enable_music_pack = false;
    strncpy(cfg->music_pack_path, "music_pack", sizeof(cfg->music_pack_path) - 1);
    cfg->music_volume = 85;
    cfg->music_duck_percent = 25;
    cfg->music_loop = true;

    // [Audio]
    cfg->audio_enabled = true;
    cfg->audio_muted = false;
    cfg->audio_volume = 100;
    cfg->audio_device_name[0] = '\0';
    cfg->audio_latency_ms = 80;

    // [Cheats]
    cfg->cheat_infinite_health = false;
    cfg->cheat_infinite_ammo = false;
    cfg->cheat_one_hit_kill = false;
    cfg->cheat_freeze_reticle = false;
    cfg->cheat_all_weapons = false;
    cfg->cheat_infinite_items = false;

    // [General]
    cfg->last_rom_path[0] = '\0';
    cfg->auto_save_state_on_exit = false;
    cfg->speed_percent = 100;
}

static char* trim_whitespace(char* str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

bool config_load_ini(const char* file_path) {
    const char* path = (file_path && file_path[0]) ? file_path : s_default_ini_path;
    FILE* f = fopen(path, "r");
    if (!f) {
        config_set_defaults(&g_app_config);
        config_save_ini(path);
        return false;
    }

    config_set_defaults(&g_app_config);

    char line[512];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        char* trimmed = trim_whitespace(line);
        if (trimmed[0] == ';' || trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }

        if (trimmed[0] == '[' && trimmed[strlen(trimmed) - 1] == ']') {
            trimmed[strlen(trimmed) - 1] = '\0';
            strncpy(section, trimmed + 1, sizeof(section) - 1);
            section[sizeof(section) - 1] = '\0';
            continue;
        }

        char* eq = strchr(trimmed, '=');
        if (!eq) continue;

        *eq = '\0';
        char* key = trim_whitespace(trimmed);
        char* val = trim_whitespace(eq + 1);

        if (strcmp(section, "Display") == 0) {
            if (strcmp(key, "widescreen_mode") == 0) g_app_config.widescreen_mode = atoi(val);
            else if (strcmp(key, "scaling_mode") == 0) g_app_config.scaling_mode = atoi(val);
            else if (strcmp(key, "filter_mode") == 0) g_app_config.filter_mode = atoi(val);
            else if (strcmp(key, "fullscreen") == 0) g_app_config.fullscreen = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "window_scale") == 0) g_app_config.window_scale = atoi(val);
            else if (strcmp(key, "vsync") == 0) g_app_config.vsync = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "palette_idx") == 0) g_app_config.palette_idx = atoi(val);
            else if (strcmp(key, "orientation_lock") == 0) g_app_config.orientation_lock = atoi(val);
        } else if (strcmp(section, "Lighting") == 0) {
            if (strcmp(key, "flashlight_enabled") == 0) g_app_config.flashlight_enabled = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "flashlight_intensity") == 0) g_app_config.flashlight_intensity = atoi(val);
            else if (strcmp(key, "ambient_darkness") == 0) g_app_config.ambient_darkness = atoi(val);
            else if (strcmp(key, "flashlight_flicker") == 0) g_app_config.flashlight_flicker = (atoi(val) != 0 || strcmp(val, "true") == 0);
        } else if (strcmp(section, "Atmosphere") == 0) {
            if (strcmp(key, "vignette_enabled") == 0) g_app_config.vignette_enabled = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "vignette_intensity") == 0) g_app_config.vignette_intensity = atoi(val);
            else if (strcmp(key, "film_grain_enabled") == 0) g_app_config.film_grain_enabled = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "grain_intensity") == 0) g_app_config.grain_intensity = atoi(val);
            else if (strcmp(key, "scanlines_enabled") == 0) g_app_config.scanlines_enabled = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "scanline_intensity") == 0) g_app_config.scanline_intensity = atoi(val);
            else if (strcmp(key, "crt_mask_enabled") == 0) g_app_config.crt_mask_enabled = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "crt_mask_intensity") == 0) g_app_config.crt_mask_intensity = atoi(val);
            else if (strcmp(key, "color_grade_mode") == 0) g_app_config.color_grade_mode = atoi(val);
        } else if (strcmp(section, "HDPack") == 0) {
            if (strcmp(key, "enable_hd_pack") == 0) g_app_config.enable_hd_pack = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "hd_pack_path") == 0) {
                strncpy(g_app_config.hd_pack_path, val, sizeof(g_app_config.hd_pack_path) - 1);
                g_app_config.hd_pack_path[sizeof(g_app_config.hd_pack_path) - 1] = '\0';
            }
            else if (strcmp(key, "enable_hd_backgrounds") == 0) g_app_config.enable_hd_backgrounds = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "enable_hd_monsters") == 0) g_app_config.enable_hd_monsters = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "enable_hd_portraits") == 0) g_app_config.enable_hd_portraits = (atoi(val) != 0 || strcmp(val, "true") == 0);
        } else if (strcmp(section, "MusicPack") == 0) {
            if (strcmp(key, "enable_music_pack") == 0) g_app_config.enable_music_pack = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "music_pack_path") == 0) {
                strncpy(g_app_config.music_pack_path, val, sizeof(g_app_config.music_pack_path) - 1);
                g_app_config.music_pack_path[sizeof(g_app_config.music_pack_path) - 1] = '\0';
            }
            else if (strcmp(key, "volume") == 0) g_app_config.music_volume = atoi(val);
            else if (strcmp(key, "duck_percent") == 0) g_app_config.music_duck_percent = atoi(val);
            else if (strcmp(key, "loop") == 0) g_app_config.music_loop = (atoi(val) != 0 || strcmp(val, "true") == 0);
        } else if (strcmp(section, "Audio") == 0) {
            if (strcmp(key, "enabled") == 0) g_app_config.audio_enabled = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "muted") == 0) g_app_config.audio_muted = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "volume") == 0) g_app_config.audio_volume = atoi(val);
            else if (strcmp(key, "device_name") == 0) {
                strncpy(g_app_config.audio_device_name, val, sizeof(g_app_config.audio_device_name) - 1);
                g_app_config.audio_device_name[sizeof(g_app_config.audio_device_name) - 1] = '\0';
            }
            else if (strcmp(key, "latency_ms") == 0) g_app_config.audio_latency_ms = atoi(val);
        } else if (strcmp(section, "Cheats") == 0) {
            if (strcmp(key, "infinite_health") == 0) g_app_config.cheat_infinite_health = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "infinite_ammo") == 0) g_app_config.cheat_infinite_ammo = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "one_hit_kill") == 0) g_app_config.cheat_one_hit_kill = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "freeze_reticle") == 0) g_app_config.cheat_freeze_reticle = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "all_weapons") == 0) g_app_config.cheat_all_weapons = (atoi(val) != 0 || strcmp(val, "true") == 0);
            else if (strcmp(key, "infinite_items") == 0) g_app_config.cheat_infinite_items = (atoi(val) != 0 || strcmp(val, "true") == 0);
        } else if (strcmp(section, "General") == 0) {
            if (strcmp(key, "last_rom_path") == 0) {
                strncpy(g_app_config.last_rom_path, val, sizeof(g_app_config.last_rom_path) - 1);
                g_app_config.last_rom_path[sizeof(g_app_config.last_rom_path) - 1] = '\0';
            } else if (strcmp(key, "auto_save_state_on_exit") == 0) {
                g_app_config.auto_save_state_on_exit = (atoi(val) != 0 || strcmp(val, "true") == 0);
            } else if (strcmp(key, "speed_percent") == 0) {
                g_app_config.speed_percent = atoi(val);
            }
        }
    }

    fclose(f);
    printf("[Config] Loaded configuration from '%s'\n", path);
    return true;
}

bool config_save_ini(const char* file_path) {
    const char* path = (file_path && file_path[0]) ? file_path : s_default_ini_path;
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[Config] Failed to open '%s' for writing\n", path);
        return false;
    }

    fprintf(f, "# Resident Evil Gaiden - Recompilation Settings\n\n");

    fprintf(f, "[Display]\n");
    fprintf(f, "widescreen_mode=%d\n", g_app_config.widescreen_mode);
    fprintf(f, "scaling_mode=%d\n", g_app_config.scaling_mode);
    fprintf(f, "filter_mode=%d\n", g_app_config.filter_mode);
    fprintf(f, "fullscreen=%d\n", g_app_config.fullscreen ? 1 : 0);
    fprintf(f, "window_scale=%d\n", g_app_config.window_scale);
    fprintf(f, "vsync=%d\n", g_app_config.vsync ? 1 : 0);
    fprintf(f, "palette_idx=%d\n", g_app_config.palette_idx);
    fprintf(f, "orientation_lock=%d\n\n", g_app_config.orientation_lock);

    fprintf(f, "[Lighting]\n");
    fprintf(f, "flashlight_enabled=%d\n", g_app_config.flashlight_enabled ? 1 : 0);
    fprintf(f, "flashlight_intensity=%d\n", g_app_config.flashlight_intensity);
    fprintf(f, "ambient_darkness=%d\n", g_app_config.ambient_darkness);
    fprintf(f, "flashlight_flicker=%d\n\n", g_app_config.flashlight_flicker ? 1 : 0);

    fprintf(f, "[Atmosphere]\n");
    fprintf(f, "vignette_enabled=%d\n", g_app_config.vignette_enabled ? 1 : 0);
    fprintf(f, "vignette_intensity=%d\n", g_app_config.vignette_intensity);
    fprintf(f, "film_grain_enabled=%d\n", g_app_config.film_grain_enabled ? 1 : 0);
    fprintf(f, "grain_intensity=%d\n", g_app_config.grain_intensity);
    fprintf(f, "scanlines_enabled=%d\n", g_app_config.scanlines_enabled ? 1 : 0);
    fprintf(f, "scanline_intensity=%d\n", g_app_config.scanline_intensity);
    fprintf(f, "crt_mask_enabled=%d\n", g_app_config.crt_mask_enabled ? 1 : 0);
    fprintf(f, "crt_mask_intensity=%d\n", g_app_config.crt_mask_intensity);
    fprintf(f, "color_grade_mode=%d\n\n", g_app_config.color_grade_mode);

    fprintf(f, "[HDPack]\n");
    fprintf(f, "enable_hd_pack=%d\n", g_app_config.enable_hd_pack ? 1 : 0);
    fprintf(f, "hd_pack_path=%s\n", g_app_config.hd_pack_path);
    fprintf(f, "enable_hd_backgrounds=%d\n", g_app_config.enable_hd_backgrounds ? 1 : 0);
    fprintf(f, "enable_hd_monsters=%d\n", g_app_config.enable_hd_monsters ? 1 : 0);
    fprintf(f, "enable_hd_portraits=%d\n\n", g_app_config.enable_hd_portraits ? 1 : 0);

    fprintf(f, "[MusicPack]\n");
    fprintf(f, "enable_music_pack=%d\n", g_app_config.enable_music_pack ? 1 : 0);
    fprintf(f, "music_pack_path=%s\n", g_app_config.music_pack_path);
    fprintf(f, "volume=%d\n", g_app_config.music_volume);
    fprintf(f, "duck_percent=%d\n", g_app_config.music_duck_percent);
    fprintf(f, "loop=%d\n\n", g_app_config.music_loop ? 1 : 0);

    fprintf(f, "[Audio]\n");
    fprintf(f, "enabled=%d\n", g_app_config.audio_enabled ? 1 : 0);
    fprintf(f, "muted=%d\n", g_app_config.audio_muted ? 1 : 0);
    fprintf(f, "volume=%d\n", g_app_config.audio_volume);
    fprintf(f, "device_name=%s\n", g_app_config.audio_device_name);
    fprintf(f, "latency_ms=%d\n\n", g_app_config.audio_latency_ms);

    fprintf(f, "[Cheats]\n");
    fprintf(f, "infinite_health=%d\n", g_app_config.cheat_infinite_health ? 1 : 0);
    fprintf(f, "infinite_ammo=%d\n", g_app_config.cheat_infinite_ammo ? 1 : 0);
    fprintf(f, "one_hit_kill=%d\n", g_app_config.cheat_one_hit_kill ? 1 : 0);
    fprintf(f, "freeze_reticle=%d\n", g_app_config.cheat_freeze_reticle ? 1 : 0);
    fprintf(f, "all_weapons=%d\n", g_app_config.cheat_all_weapons ? 1 : 0);
    fprintf(f, "infinite_items=%d\n\n", g_app_config.cheat_infinite_items ? 1 : 0);

    fprintf(f, "[General]\n");
    fprintf(f, "last_rom_path=%s\n", g_app_config.last_rom_path);
    fprintf(f, "auto_save_state_on_exit=%d\n", g_app_config.auto_save_state_on_exit ? 1 : 0);
    fprintf(f, "speed_percent=%d\n", g_app_config.speed_percent);

    fclose(f);
    printf("[Config] Saved configuration to '%s'\n", path);
    return true;
}
