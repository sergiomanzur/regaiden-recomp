#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include "hd_pack.h"
#include "runtime/vendor/stb/stb_image.h"

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define MAX_HD_TEXTURES 64

static HDTexture s_textures[MAX_HD_TEXTURES];
static int s_texture_count = 0;
static SDL_Renderer* s_active_renderer = NULL;

HDPackConfig g_hd_pack_config = {
    .enabled = true,
    .pack_dir = "hd_pack",
    .loaded_count = 0,
    .enable_hd_backgrounds = true,
    .enable_hd_monsters = true,
    .enable_hd_portraits = true
};

int hd_pack_get_texture_count(void) {
    return s_texture_count;
}

const HDTexture* hd_pack_get_texture(int index) {
    if (index >= 0 && index < s_texture_count) {
        return &s_textures[index];
    }
    return NULL;
}

static void free_all_textures(void) {
    for (int i = 0; i < s_texture_count; i++) {
        if (s_textures[i].sdl_texture) {
            SDL_DestroyTexture((SDL_Texture*)s_textures[i].sdl_texture);
            s_textures[i].sdl_texture = NULL;
        }
        if (s_textures[i].pixels) {
            stbi_image_free(s_textures[i].pixels);
            s_textures[i].pixels = NULL;
        }
        s_textures[i].loaded = false;
    }
    s_texture_count = 0;
    g_hd_pack_config.loaded_count = 0;
}

static bool load_texture_file(const char* filepath, const char* name, SDL_Renderer* renderer) {
    if (s_texture_count >= MAX_HD_TEXTURES) {
        return false;
    }

    int w = 0, h = 0, channels = 0;
    unsigned char* data = NULL;

    SDL_RWops* rw = SDL_RWFromFile(filepath, "rb");
    if (rw) {
        Sint64 sz = SDL_RWsize(rw);
        if (sz > 0 && sz <= 32 * 1024 * 1024) {
            unsigned char* raw = (unsigned char*)malloc((size_t)sz);
            if (raw) {
                if (SDL_RWread(rw, raw, 1, (size_t)sz) == (size_t)sz) {
                    data = stbi_load_from_memory(raw, (int)sz, &w, &h, &channels, 4);
                }
                free(raw);
            }
        }
        SDL_RWclose(rw);
    }
    if (!data) {
        data = stbi_load(filepath, &w, &h, &channels, 4);
    }
    if (!data) {
        return false;
    }

    HDTexture* tex = &s_textures[s_texture_count++];
    strncpy(tex->name, name, sizeof(tex->name) - 1);
    tex->name[sizeof(tex->name) - 1] = '\0';
    tex->width = w;
    tex->height = h;
    tex->pixels = (uint32_t*)data;
    tex->sdl_texture = NULL;
    tex->loaded = true;

    // Convert raw RGBA bytes -> ARGB8888 for SDL renderer
    for (int i = 0; i < w * h; i++) {
        uint8_t* p = (uint8_t*)&tex->pixels[i];
        uint8_t r = p[0];
        uint8_t g = p[1];
        uint8_t b = p[2];
        uint8_t a = p[3];
        tex->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    // Create high-resolution GPU texture for host presentation
    if (renderer) {
        SDL_Texture* tex_sdl = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
        if (tex_sdl) {
            SDL_SetTextureBlendMode(tex_sdl, SDL_BLENDMODE_BLEND);
            SDL_UpdateTexture(tex_sdl, NULL, tex->pixels, w * sizeof(uint32_t));
            tex->sdl_texture = (void*)tex_sdl;
        }
    }

    g_hd_pack_config.loaded_count = s_texture_count;
    printf("[HD Pack] Loaded HD Texture: %s (%dx%d)\n", name, w, h);
    return true;
}

static void ensure_directories(const char* base_dir) {
#ifdef _WIN32
    _mkdir(base_dir);
    char sub[512];
    snprintf(sub, sizeof(sub), "%s/backgrounds", base_dir);
    _mkdir(sub);
    snprintf(sub, sizeof(sub), "%s/monsters", base_dir);
    _mkdir(sub);
    snprintf(sub, sizeof(sub), "%s/portraits", base_dir);
    _mkdir(sub);
#else
    mkdir(base_dir, 0755);
    char sub[512];
    snprintf(sub, sizeof(sub), "%s/backgrounds", base_dir);
    mkdir(sub, 0755);
    snprintf(sub, sizeof(sub), "%s/monsters", base_dir);
    mkdir(sub, 0755);
    snprintf(sub, sizeof(sub), "%s/portraits", base_dir);
    mkdir(sub, 0755);
#endif
}

static void scan_and_load_folder(const char* base_dir, const char* subfolder, SDL_Renderer* renderer) {
    char search_path[512];
#ifdef _WIN32
    snprintf(search_path, sizeof(search_path), "%s/%s/*.png", base_dir, subfolder);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char fullpath[512];
                snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", base_dir, subfolder, fd.cFileName);
                char tex_name[64];
                snprintf(tex_name, sizeof(tex_name), "%s/%s", subfolder, fd.cFileName);
                load_texture_file(fullpath, tex_name, renderer);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    snprintf(search_path, sizeof(search_path), "%s/%s", base_dir, subfolder);
    DIR* dir = opendir(search_path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".png") || strstr(entry->d_name, ".PNG")) {
                char fullpath[512];
                snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", base_dir, subfolder, entry->d_name);
                char tex_name[64];
                snprintf(tex_name, sizeof(tex_name), "%s/%s", subfolder, entry->d_name);
                load_texture_file(fullpath, tex_name, renderer);
            }
        }
        closedir(dir);
    }
#endif
}

static void resolve_hd_pack_path(char* out_path, size_t max_len, const char* requested) {
    char test_path[512];
    snprintf(test_path, sizeof(test_path), "%s/portraits", requested);
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(test_path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        strncpy(out_path, requested, max_len - 1);
        out_path[max_len - 1] = '\0';
        return;
    }

    // Check ../hd_pack (if running from bin/)
    snprintf(test_path, sizeof(test_path), "../%s/portraits", requested);
    attr = GetFileAttributesA(test_path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        snprintf(out_path, max_len, "../%s", requested);
        return;
    }

    // Check next to executable
    char exe_path[512];
    if (GetModuleFileNameA(NULL, exe_path, sizeof(exe_path))) {
        char* last_slash = strrchr(exe_path, '\\');
        if (!last_slash) last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(test_path, sizeof(test_path), "%s/%s/portraits", exe_path, requested);
            attr = GetFileAttributesA(test_path);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                snprintf(out_path, max_len, "%s/%s", exe_path, requested);
                return;
            }
            snprintf(test_path, sizeof(test_path), "%s/../%s/portraits", exe_path, requested);
            attr = GetFileAttributesA(test_path);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                snprintf(out_path, max_len, "%s/../%s", exe_path, requested);
                return;
            }
        }
    }
#endif
    strncpy(out_path, requested, max_len - 1);
    out_path[max_len - 1] = '\0';
}

void hd_pack_init(const char* base_dir) {
    const char* target_dir = (base_dir && base_dir[0]) ? base_dir : "hd_pack";
    resolve_hd_pack_path(g_hd_pack_config.pack_dir, sizeof(g_hd_pack_config.pack_dir), target_dir);
    ensure_directories(g_hd_pack_config.pack_dir);
    hd_pack_reload(NULL);
}

static const char* s_known_textures[] = {
    "backgrounds/battle.png",
    "backgrounds/battle_0.png",
    "monsters/monster.png",
    "monsters/zombie_0.png",
    "portraits/barry.png",
    "portraits/leon.png",
    "portraits/lucia.png",
    NULL
};

void hd_pack_reload(void* sdl_renderer) {
    if (sdl_renderer) {
        s_active_renderer = (SDL_Renderer*)sdl_renderer;
    }
    free_all_textures();
    scan_and_load_folder(g_hd_pack_config.pack_dir, "backgrounds", s_active_renderer);
    scan_and_load_folder(g_hd_pack_config.pack_dir, "monsters", s_active_renderer);
    scan_and_load_folder(g_hd_pack_config.pack_dir, "portraits", s_active_renderer);

    if (s_texture_count == 0) {
        for (int i = 0; s_known_textures[i] != NULL; ++i) {
            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", g_hd_pack_config.pack_dir, s_known_textures[i]);
            load_texture_file(fullpath, s_known_textures[i], s_active_renderer);
        }
    }

    printf("[HD Pack] Total HD textures loaded: %d (Folder: %s)\n", s_texture_count, g_hd_pack_config.pack_dir);
}

void hd_pack_shutdown(void) {
    free_all_textures();
    s_active_renderer = NULL;
}

static const HDTexture* find_texture_by_prefix(const char* prefix) {
    for (int i = 0; i < s_texture_count; i++) {
        if (s_textures[i].loaded && strstr(s_textures[i].name, prefix) != NULL) {
            return &s_textures[i];
        }
    }
    return NULL;
}

void hd_pack_render_host_overlay(GBContext* ctx, void* sdl_renderer, int vp_x, int vp_y, int vp_w, int vp_h) {
    if (!g_hd_pack_config.enabled || !ctx || !sdl_renderer || s_texture_count == 0) {
        return;
    }

    SDL_Renderer* renderer = (SDL_Renderer*)sdl_renderer;

    // Do NOT render during logos or title screen
    if ((ctx->io[0x40] & 0x80) == 0) return; // LCD off

    // 1. HD Battle Background & Monster (when in battle mode WRAM 0xC900 > 0)
    if (ctx->wram && ctx->wram[0x0900] > 0) {
        if (g_hd_pack_config.enable_hd_backgrounds) {
            const HDTexture* bg = find_texture_by_prefix("backgrounds/battle");
            if (bg && bg->sdl_texture) {
                SDL_Rect dst_rect = { vp_x, vp_y, vp_w, vp_h };
                SDL_RenderCopy(renderer, (SDL_Texture*)bg->sdl_texture, NULL, &dst_rect);
            }
        }

        if (g_hd_pack_config.enable_hd_monsters) {
            const HDTexture* mon = find_texture_by_prefix("monsters/zombie");
            if (!mon) mon = find_texture_by_prefix("monsters/monster");
            if (mon && mon->sdl_texture) {
                int mon_h = vp_h * 7 / 10;
                int mon_w = mon_h;
                int mon_x = vp_x + (vp_w - mon_w) / 2;
                int mon_y = vp_y + (vp_h - mon_h) / 2 - (vp_h / 20);
                SDL_Rect dst_rect = { mon_x, mon_y, mon_w, mon_h };
                SDL_RenderCopy(renderer, (SDL_Texture*)mon->sdl_texture, NULL, &dst_rect);
            }
        }
        return;
    }

    // 2. HD Character Dialog Portraits (ONLY when dialogue window is open on screen)
    if (g_hd_pack_config.enable_hd_portraits && ctx->wram) {
        // Window must be enabled with WY active on screen (WY < 144)
        bool win_active = (ctx->io[0x40] & 0x20) && (ctx->io[0x4A] < 120);
        if (win_active) {
            uint8_t char_id = ctx->wram[0x0800]; // 0=Barry, 1=Leon, 2=Lucia
            const HDTexture* portrait = NULL;
            if (char_id == 0) portrait = find_texture_by_prefix("portraits/barry");
            else if (char_id == 1) portrait = find_texture_by_prefix("portraits/leon");
            else if (char_id == 2) portrait = find_texture_by_prefix("portraits/lucia");
            if (!portrait) portrait = find_texture_by_prefix("portraits/barry");

            if (portrait && portrait->sdl_texture) {
                // Scale portrait position relative to the host display viewport
                float scale_x = (float)vp_w / 160.0f;
                float scale_y = (float)vp_h / 144.0f;
                int p_size = (int)(40.0f * scale_y);
                int p_x = vp_x + (int)(4.0f * scale_x);
                int p_y = vp_y + (int)(((float)ctx->io[0x4A] + 4.0f) * scale_y);

                SDL_Rect dst_rect = { p_x, p_y, p_size, p_size };
                SDL_RenderCopy(renderer, (SDL_Texture*)portrait->sdl_texture, NULL, &dst_rect);
            }
        }
    }
}
