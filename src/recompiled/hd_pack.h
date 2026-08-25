#ifndef RE_HD_PACK_H
#define RE_HD_PACK_H

#include <stdint.h>
#include <stdbool.h>
#include "gbrt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[64];
    int width;
    int height;
    uint32_t* pixels; // ARGB8888
    void* sdl_texture; // SDL_Texture*
    bool loaded;
} HDTexture;

typedef struct {
    bool enabled;
    char pack_dir[256];
    int loaded_count;
    bool enable_hd_backgrounds;
    bool enable_hd_monsters;
    bool enable_hd_portraits;
} HDPackConfig;

extern HDPackConfig g_hd_pack_config;

void hd_pack_init(const char* base_dir);
void hd_pack_reload(void* sdl_renderer);
void hd_pack_shutdown(void);
int hd_pack_get_texture_count(void);
const HDTexture* hd_pack_get_texture(int index);
void hd_pack_render_host_overlay(GBContext* ctx, void* sdl_renderer, int vp_x, int vp_y, int vp_w, int vp_h);

#ifdef __cplusplus
}
#endif

#endif // RE_HD_PACK_H
