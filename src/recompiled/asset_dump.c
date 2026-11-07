/* CGB graphics snapshot export. PNG uses standard uncompressed DEFLATE blocks,
 * avoiding an extra runtime library or a second stb implementation. */
#include "asset_dump.h"
#include "ppu.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define make_dir(p) mkdir(p, 0755)
#endif
#define PATH_SIZE 1024

static void put32(unsigned char* p, uint32_t v) {
    p[0]=(unsigned char)(v>>24); p[1]=(unsigned char)(v>>16);
    p[2]=(unsigned char)(v>>8); p[3]=(unsigned char)v;
}
static uint32_t crc32_bytes(const unsigned char* p, size_t n, uint32_t crc) {
    while (n--) {
        crc ^= *p++;
        for (int i=0;i<8;i++) crc=(crc>>1)^((crc&1)?0xedb88320u:0);
    }
    return crc;
}
static bool chunk(FILE* f, const char* type, const unsigned char* data, size_t n) {
    unsigned char word[4]; put32(word,(uint32_t)n);
    if (fwrite(word,1,4,f)!=4 || fwrite(type,1,4,f)!=4) return false;
    if (n && fwrite(data,1,n,f)!=n) return false;
    uint32_t crc=crc32_bytes((const unsigned char*)type,4,0xffffffffu);
    put32(word,crc32_bytes(data,n,crc)^0xffffffffu);
    return fwrite(word,1,4,f)==4;
}
static bool write_png(const char* path, const unsigned char* rgba, int w, int h) {
    size_t stride=(size_t)w*4, raw_size=(stride+1)*(size_t)h;
    size_t blocks=(raw_size+65534)/65535;
    unsigned char* raw=(unsigned char*)malloc(raw_size);
    unsigned char* z=(unsigned char*)malloc(raw_size+blocks*5+6);
    if (!raw || !z) { free(raw); free(z); return false; }
    for (int y=0;y<h;y++) {
        raw[(stride+1)*y]=0;
        memcpy(raw+(stride+1)*y+1,rgba+stride*y,stride);
    }
    z[0]=0x78; z[1]=0x01; size_t pos=2, offset=0;
    while (offset<raw_size) {
        size_t n=raw_size-offset; if (n>65535) n=65535;
        unsigned complement=(unsigned)n^0xffffu;
        z[pos++]=(offset+n==raw_size)?1:0;
        z[pos++]=(unsigned char)n; z[pos++]=(unsigned char)(n>>8);
        z[pos++]=(unsigned char)complement; z[pos++]=(unsigned char)(complement>>8);
        memcpy(z+pos,raw+offset,n); pos+=n; offset+=n;
    }
    uint32_t a=1,b=0;
    for (size_t i=0;i<raw_size;i++) { a=(a+raw[i])%65521; b=(b+a)%65521; }
    put32(z+pos,(b<<16)|a); pos+=4; free(raw);
    FILE* f=fopen(path,"wb");
    if (!f) { free(z); return false; }
    unsigned char ihdr[13]={0}; put32(ihdr,(uint32_t)w); put32(ihdr+4,(uint32_t)h);
    ihdr[8]=8; ihdr[9]=6; /* 8-bit RGBA */
    bool ok=fwrite("\211PNG\r\n\032\n",1,8,f)==8 &&
        chunk(f,"IHDR",ihdr,13) && chunk(f,"IDAT",z,pos) && chunk(f,"IEND",NULL,0);
    if (fclose(f)!=0) ok=false;
    free(z); return ok;
}
static void color(unsigned char* out, uint16_t value, bool transparent) {
    out[0]=(unsigned char)((value&31)*255/31);
    out[1]=(unsigned char)(((value>>5)&31)*255/31);
    out[2]=(unsigned char)(((value>>10)&31)*255/31);
    out[3]=transparent?0:255;
}
static void tile_pixel(const GBContext* ctx, unsigned char* out, int bank,
                       int tile, int x, int y, int palette, bool obj) {
    const GBPPU* ppu=(const GBPPU*)ctx->ppu;
    const unsigned char* row=ctx->vram+bank*0x2000+tile*16+y*2;
    unsigned c=((row[0]>>(7-x))&1)|(((row[1]>>(7-x))&1)<<1);
    const unsigned char* ram=(obj?ppu->obj_palette_ram:ppu->bg_palette_ram)+palette*8+c*2;
    color(out,(uint16_t)(ram[0]|(ram[1]<<8)),obj && c==0);
}
static bool save_image(const char* dir, const char* name, const unsigned char* image, int w,int h) {
    char path[PATH_SIZE];
    int n=snprintf(path,sizeof(path),"%s/%s",dir,name);
    return n>0 && (size_t)n<sizeof(path) && write_png(path,image,w,h);
}
static bool write_manifest(const GBContext* ctx, const char* dir) {
    char path[PATH_SIZE]; snprintf(path,sizeof(path),"%s/manifest.txt",dir);
    FILE* f=fopen(path,"wb"); if (!f) return false;
    const unsigned char* io=ctx->io;
    fprintf(f,
        "Resident Evil Gaiden CGB graphics capture\n"
        "Only graphics currently loaded in VRAM are included. The game streams graphics;\n"
        "capture different rooms, battles, dialogues and menus for a broader collection.\n"
        "Captures are references, not an automatically generated replacement pack.\n\n"
        "tiles_bank{0,1}_{bg,obj}_palette{0..7}.png: 128x192 RGBA, 384 tiles per sheet.\n"
        "16 columns x 24 rows; each tile is 8x8. Tile N is at ((N %% 16)*8,(N / 16)*8).\n"
        "Physical tile N maps to VRAM $8000 + N*16 in the named bank.\n"
        "All eight CGB palettes are exported for each bank, including unused combinations.\n"
        "OBJ sheets have transparent color index 0; BG sheets are opaque.\n"
        "map_9800.png and map_9c00.png: 256x256 full 32x32 tile maps, opaque.\n"
        "Maps use LCDC signed/unsigned tile addressing and CGB bank/palette/XY flip attributes.\n"
        "They show the complete map, without scrolling, window placement or sprite compositing.\n"
        "framebuffer.png: 160x144 native CGB RGB555 frame, with no HD/lighting/postprocess.\n"
        "The frame and VRAM reflect capture time; raster palette changes cannot be reconstructed\n"
        "in the full maps from one final palette snapshot.\n\n"
        "LCDC=%02X SCX=%u SCY=%u WX=%u WY=%u\n"
        "BG map=$%04X Window map=$%04X Tile addressing=%s\n\n"
        "HD PACK AUTHORING (relative to hd_pack/):\n"
        "backgrounds/battle*.png: battle background stretched over the viewport.\n"
        "monsters/zombie*.png: battle monster scaled into a square (70%% viewport height).\n"
        "monsters/monster*.png: fallback battle monster prefix.\n"
        "portraits/barry*.png, portraits/leon*.png, portraits/lucia*.png: loaded portrait names.\n"
        "Current renderer selects the first matching battle/zombie prefix and Barry portrait;\n"
        "suffixes are not scene IDs and Leon/Lucia are not automatically selected.\n"
        "PNG input dimensions are unrestricted by the loader. Use a square transparent image\n"
        "for monsters/portraits; match the viewport aspect ratio for battle backgrounds.\n"
        "Tile sheets and map_*.png names are NOT supported automatic tile replacements.\n"
        "Use them to identify/crop/repaint art, then save the authored image under a supported name.\n",
        io[0x40],io[0x43],io[0x42],io[0x4b],io[0x4a],
        (io[0x40]&8)?0x9c00:0x9800,(io[0x40]&0x40)?0x9c00:0x9800,
        (io[0x40]&0x10)?"unsigned $8000":"signed $9000");
    bool ok=!ferror(f); if (fclose(f)!=0) ok=false; return ok;
}
bool asset_dump_capture(const GBContext* ctx, const char* output_dir,
                        char* status, size_t status_size) {
    const char* error="Invalid graphics capture arguments";
    char dir[PATH_SIZE]={0}; unsigned char* image=NULL;
    if (!ctx || !ctx->vram || !ctx->io || !ctx->ppu || !output_dir || !output_dir[0]) goto fail;
    if (strlen(output_dir)>PATH_SIZE-100) { error="Graphics output path is too long"; goto fail; }
    if (make_dir(output_dir)!=0 && errno!=EEXIST) { error="Cannot create graphics output directory"; goto fail; }
    bool created=false;
    for (unsigned i=1;i<=999999;i++) {
        snprintf(dir,sizeof(dir),"%s/capture_%04u",output_dir,i);
        if (make_dir(dir)==0) { created=true; break; }
        if (errno!=EEXIST) break;
    }
    if (!created) { error="Cannot create a new graphics capture directory"; goto fail; }
    image=(unsigned char*)malloc(256*256*4);
    if (!image) { error="Out of memory while capturing graphics"; goto fail; }
    error="Failed writing graphics capture (partial files may remain)";
    for (int bank=0;bank<2;bank++) for (int obj=0;obj<2;obj++) for (int p=0;p<8;p++) {
        for (int t=0;t<384;t++) for (int y=0;y<8;y++) for (int x=0;x<8;x++) {
            unsigned char* pixel=image+(((t/16)*8+y)*128+(t%16)*8+x)*4;
            tile_pixel(ctx,pixel,bank,t,x,y,p,obj!=0);
        }
        char name[64]; snprintf(name,sizeof(name),"tiles_bank%d_%s_palette%d.png",bank,obj?"obj":"bg",p);
        if (!save_image(dir,name,image,128,192)) goto fail;
    }
    for (int map=0;map<2;map++) {
        int start=0x1800+map*0x400;
        for (int t=0;t<1024;t++) {
            int tile=ctx->vram[start+t];
            unsigned attr=ctx->vram[0x2000+start+t];
            if (!(ctx->io[0x40]&0x10)) tile=256+(int)(int8_t)tile;
            for (int y=0;y<8;y++) for (int x=0;x<8;x++) {
                unsigned char* pixel=image+(((t/32)*8+y)*256+(t%32)*8+x)*4;
                tile_pixel(ctx,pixel,(attr>>3)&1,tile,(attr&0x20)?7-x:x,(attr&0x40)?7-y:y,attr&7,false);
            }
        }
        if (!save_image(dir,map?"map_9c00.png":"map_9800.png",image,256,256)) goto fail;
    }
    const GBPPU* ppu=(const GBPPU*)ctx->ppu;
    for (int i=0;i<160*144;i++) color(image+i*4,ppu->color_framebuffer[i],false);
    if (!save_image(dir,"framebuffer.png",image,160,144) || !write_manifest(ctx,dir)) goto fail;
    free(image);
    if (status && status_size) snprintf(status,status_size,"Graphics captured: %s",dir);
    return true;
fail:
    free(image);
    if (status && status_size) snprintf(status,status_size,"%s",error);
    return false;
}
