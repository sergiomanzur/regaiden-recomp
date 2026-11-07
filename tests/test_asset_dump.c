/* Synthetic graphics only. Decode the exported files independently with stb. */
#include "asset_dump.h"
#include "ppu.h"
#ifdef NDEBUG
#undef NDEBUG /* Keep test checks active in release builds. */
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#define remove_dir(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(p) mkdir(p, 0755)
#define remove_dir(p) rmdir(p)
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "stb_image.h"
static GBContext ctx;
static GBPPU ppu;
static unsigned char vram[0x4000], io[0x80];
static char root[512], capture[600];
static unsigned char* load(const char* name, int w, int h) {
    char path[800]; int x,y,c;
    snprintf(path, sizeof(path), "%s/%s", capture, name);
    unsigned char* rgba = stbi_load(path, &x, &y, &c, 4);
    assert(rgba && x == w && y == h); return rgba;
}
static void pixel(const unsigned char* img, int w, int x, int y,
                  int r, int g, int b, int a) {
    const unsigned char* p = img + (y*w+x)*4;
    assert(p[0]==r && p[1]==g && p[2]==b && p[3]==a);
}
static void palette(unsigned char* ram, int p, int c, unsigned value) {
    ram[p*8+c*2] = value & 255; ram[p*8+c*2+1] = value >> 8;
}
static void remove_capture(const char* dir) {
    char path[800];
    for (int bank=0;bank<2;bank++) for (int obj=0;obj<2;obj++) for (int p=0;p<8;p++) {
        snprintf(path,sizeof(path),"%s/tiles_bank%d_%s_palette%d.png",dir,bank,obj?"obj":"bg",p);
        assert(remove(path)==0);
    }
    const char* names[]={"map_9800.png","map_9c00.png","framebuffer.png","manifest.txt"};
    for (unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++) {
        snprintf(path,sizeof(path),"%s/%s",dir,names[i]); assert(remove(path)==0);
    }
    assert(remove_dir(dir)==0);
}
int main(void) {
    char status[1024];
    unsigned attempt=0;
    do {
        snprintf(root, sizeof(root), "asset_dump_test_%lu_%u", (unsigned long)time(NULL),attempt++);
        assert(attempt<1000);
    } while (make_dir(root)!=0);
    ctx.vram = vram; ctx.io = io; ctx.ppu = &ppu;
    io[0x40] = 0x10;
    palette(ppu.bg_palette_ram, 0, 0, 0x7fff);
    palette(ppu.bg_palette_ram, 0, 1, 0x001f);
    palette(ppu.obj_palette_ram, 7, 1, 0x03e0);
    palette(ppu.bg_palette_ram, 3, 2, 0x7c00);
    vram[0] = 0x80; /* bank 0 tile 0 top-left = color 1 */
    vram[0x2001] = 0x80; /* bank 1 tile 0 top-left = color 2 */
    vram[0x3800] = 0x6b; /* map 9800: bank 1, palette 3, XY flipped */
    vram[0x1c00] = 0x80; /* unsigned tile 128 */
    vram[128*16] = 0x80;
    vram[383*16+14] = 1; /* last physical tile bottom-right */
    ppu.color_framebuffer[0] = 0x03e0;
    assert(asset_dump_capture(&ctx, root, status, sizeof(status)));
    snprintf(capture, sizeof(capture), "%s/capture_0001", root);
    for (int bank=0;bank<2;bank++) for (int obj=0;obj<2;obj++) for (int p=0;p<8;p++) {
        char name[64]; snprintf(name,sizeof(name),"tiles_bank%d_%s_palette%d.png",bank,obj?"obj":"bg",p);
        stbi_image_free(load(name,128,192));
    }
    unsigned char* img = load("tiles_bank0_bg_palette0.png",128,192);
    pixel(img,128,0,0,255,0,0,255); pixel(img,128,1,0,255,255,255,255);
    pixel(img,128,127,191,255,0,0,255); stbi_image_free(img);
    img = load("tiles_bank0_obj_palette7.png",128,192);
    pixel(img,128,0,0,0,255,0,255); pixel(img,128,1,0,0,0,0,0); stbi_image_free(img);
    img = load("map_9800.png",256,256);
    pixel(img,256,7,7,0,0,255,255); pixel(img,256,0,0,0,0,0,255); stbi_image_free(img);
    img = load("map_9c00.png",256,256);
    pixel(img,256,0,0,255,0,0,255); stbi_image_free(img);
    img = load("framebuffer.png",160,144);
    pixel(img,160,0,0,0,255,0,255); stbi_image_free(img);
    char manifest_path[800]; snprintf(manifest_path,sizeof(manifest_path),"%s/manifest.txt",capture);
    FILE* manifest=fopen(manifest_path,"rb"); assert(manifest);
    assert(fgetc(manifest)!=EOF); fclose(manifest);
    /* Signed mode: tile zero means physical tile 256 ($9000),
     * 0x80 means physical tile 128 ($8800). */
    io[0x40] = 0; vram[0x3800] = 0; vram[256*16] = 0x40;
    assert(asset_dump_capture(&ctx, root, status, sizeof(status)));
    snprintf(capture, sizeof(capture), "%s/capture_0002", root);
    img = load("map_9800.png",256,256);
    pixel(img,256,0,0,255,255,255,255); pixel(img,256,1,0,255,0,0,255); stbi_image_free(img);
    img = load("map_9c00.png",256,256);
    pixel(img,256,0,0,255,0,0,255); stbi_image_free(img);
    assert(!asset_dump_capture(NULL, root, status, sizeof(status)) && status[0]);
    assert(!asset_dump_capture(&ctx, "", status, sizeof(status)));
    assert(!asset_dump_capture(&ctx, NULL, NULL, 0));
    ctx.vram = NULL; assert(!asset_dump_capture(&ctx, root, status, sizeof(status))); ctx.vram = vram;
    ctx.io = NULL; assert(!asset_dump_capture(&ctx, root, status, sizeof(status))); ctx.io = io;
    ctx.ppu = NULL; assert(!asset_dump_capture(&ctx, root, status, sizeof(status))); ctx.ppu = &ppu;
    char invalid[700]; snprintf(invalid,sizeof(invalid),"%s/missing/child",root);
    assert(!asset_dump_capture(&ctx, invalid, status, sizeof(status)));
    char file_path[700]; snprintf(file_path,sizeof(file_path),"%s/file",root);
    FILE* f = fopen(file_path,"wb"); assert(f); fclose(f);
    assert(!asset_dump_capture(&ctx,file_path,status,sizeof(status)));
    char long_path[1400]; memset(long_path,'x',sizeof(long_path)-1); long_path[sizeof(long_path)-1]=0;
    assert(!asset_dump_capture(&ctx,long_path,status,sizeof(status)));
    assert(remove(file_path)==0);
    snprintf(capture,sizeof(capture),"%s/capture_0001",root); remove_capture(capture);
    snprintf(capture,sizeof(capture),"%s/capture_0002",root); remove_capture(capture);
    assert(remove_dir(root)==0);
    puts("asset dump tests passed");
    return 0;
}
