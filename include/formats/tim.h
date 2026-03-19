/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  PS1 TIM / IMD Image Format
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_FORMATS_TIM_H
#define DC_FORMATS_TIM_H

#include "core/types.h"
#include "core/color.h"
#include <stdlib.h>

struct TimImage {
    int    width, height;
    int    bpp;          /* 4, 8, 16, or 24 */
    RGBA8* rgba;         /* width * height pixels (owned) */
    bool   valid;

    /* CLUT info */
    bool   has_clut;
    int    clut_colors;
    int    clut_x, clut_y;
    RGBA8  clut[256];    /* max 256 palette entries */

    /* Pixel area VRAM coords */
    int    px_x, px_y;

    TimImage() : width(0), height(0), bpp(0), rgba(0), valid(false),
                 has_clut(false), clut_colors(0), clut_x(0), clut_y(0),
                 px_x(0), px_y(0) {}
    ~TimImage() { free(rgba); }

private:
    TimImage(const TimImage&);
    TimImage& operator=(const TimImage&);
};

/* Parse TIM or IMD image from file data.
   Supports 4, 8, 16, 24 bpp. */
bool parse_tim_image(const u8* data, size_t size, TimImage& out);

/*─── Dino Crisis standalone IMD (16bpp BGR555 image) ────────────
 *  Layout:
 *    0x00  u32  hdr_size    = 0x10 (16)
 *    0x04  u32  bpp_flag    = 2    (-> 16bpp BGR555)
 *    0x08  u32  data_size   = hdr_size + w*h*2 - some_offset (informational)
 *    0x0C  u32  reserved    = 0
 *    0x10  u16  width       (pixels)
 *    0x12  u16  height      (pixels)
 *    0x14  u8[] BGR555 pixel data  (width * height * 2 bytes, LE)
 *           Pixel 0x8000 = transparent (PS1 STP flag set, RGB=0).
 *           All other non-zero STP pixels treated as opaque here.
 *─────────────────────────────────────────────────────────────── */
struct ImdImage {
    int    width, height;
    RGBA8* rgba;    /* width * height RGBA8 pixels (owned); index=y*w+x */
    bool   valid;

    ImdImage() : width(0), height(0), rgba(0), valid(false) {}
    ~ImdImage() { free(rgba); }

private:
    ImdImage(const ImdImage&);
    ImdImage& operator=(const ImdImage&);
};

/* Detect whether data starts with a Dino Crisis IMD header.
   Returns true if hdr_size==0x10, bpp_flag==2, and pixel block fits. */
bool is_imd_image(const u8* data, size_t size);

/* Parse a standalone Dino Crisis .IMD file into an ImdImage.
   Handles transparency: 0x8000 -> alpha=0, all other values -> alpha=255. */
bool parse_imd_image(const u8* data, size_t size, ImdImage& out);

/* Save RGBA image as BMP (no external dependencies). */
bool save_bmp(const char* path, const RGBA8* pixels, int w, int h);

/* Save RGBA image as TGA (uncompressed). */
bool save_tga(const char* path, const RGBA8* pixels, int w, int h);

#endif /* DC_FORMATS_TIM_H */
