/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  Texture Rendering & Swizzle
 *  Deswizzle 8bpp, render 4/8/16bpp to RGBA bitmap
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_FORMATS_TEXTURE_H
#define DC_FORMATS_TEXTURE_H

#include "core/types.h"
#include "core/color.h"
#include "formats/mesh.h"
#include <stdlib.h>

/*─── VRAM Deswizzle (Capcom PC block-tiled format) ──────────────*/

/* Deswizzle 8bpp VRAM from 32x32 halfword block tiled format.
   vram_w/vram_h in VRAM halfwords.  Byte width = vram_w * 2.
   dst must be at least vram_w * 2 * vram_h bytes. */
void deswizzle_8bpp(const u8* src, size_t src_size,
                    u8* dst, int vram_w, int vram_h);
void deswizzle_8bpp_remfirst(const u8* src, size_t src_size,
                              u8* dst, int vram_w, int vram_h);

/* Re-pack linear pixels into block tiled format. */
void swizzle_8bpp(const u8* src, size_t src_size,
                  u8* dst, int vram_w, int vram_h);

/*─── Render to RGBA bitmap ──────────────────────────────────────*/

/* Render 8bpp indexed texture to RGBA.
   pixel_w = vram_w * 2 (byte width = pixel width for 8bpp).
   palette must have 256 RGBA8 entries.
   out_rgba must be pixel_w * vram_h * 4 bytes. */
void render_8bpp(const u8* pixels, size_t pix_size,
                 const RGBA8* palette,
                 int vram_w, int vram_h,
                 u8* out_rgba, bool do_deswizzle = true,
                 int pal_row = 0);

/* Render 4bpp indexed texture to RGBA.
   pixel_w = vram_w * 4 (each byte = 2 pixels).
   palette must have at least (sub_pal+1)*16 entries. */
void render_4bpp(const u8* pixels, size_t pix_size,
                 const RGBA8* palette,
                 int vram_w, int vram_h,
                 u8* out_rgba, bool do_deswizzle = true,
                 int pal_row = 0, int sub_pal = 0);

/* Render 16bpp direct-color texture to RGBA.
   Each pixel is a BGR555 u16.  out_rgba = w * h * 4 bytes. */
void render_16bpp(const u8* pixels, int w, int h, u8* out_rgba);

/* General-purpose render: picks 4/8/16bpp based on bpp parameter. */
void render_texture(const u8* pixels, size_t pix_size,
                    const u8* pal_data, size_t pal_size,
                    int vram_w, int vram_h, int bpp,
                    u8* out_rgba, bool do_deswizzle = true,
                    int pal_row = 0, int sub_pal = 0);

/* Render linear 8bpp indexed pixels to RGBA (no VRAM deswizzle, no
   halfword width convention).  pixel_w and pixel_h are in actual pixels.
   pal_data is raw BGR555 CLUT, pal_size in bytes.
   src_stride: source byte stride per row (if < pixel_w, data is stored
   as column strips of src_stride width).  Pass 0 for src_stride == pixel_w.
   out_rgba must be pixel_w * pixel_h * 4 bytes. */
void render_linear_8bpp(const u8* pixels, size_t pix_size,
                        const u8* pal_data, size_t pal_size,
                        int pixel_w, int pixel_h,
                        u8* out_rgba, int src_stride = 0);

/* Calculate rendered pixel dimensions for a given vram_w/h and bpp.
   Returns pixel width and height. */
void texture_pixel_dims(int vram_w, int vram_h, int bpp,
                        int& out_w, int& out_h);

/*─── Render as sampled by faces ─────────────────────────────────*/

/* Render a texture the way the game's faces sample it: every texel a face
   covers is decoded with that face's colour depth (4/8bpp) and CLUT, looked
   up in the archive's palette entries.  Texels no face covers use the most
   common setting of their texture page and are drawn dimmed.
   pixels = decompressed, still swizzled texture data at VRAM (vram_x,
   vram_y) of vram_w x vram_h halfwords.  Output is 4 px per halfword when
   any 4bpp face samples the texture (8bpp texels doubled), else 2.
   *out_rgba is malloc'd (caller frees).  Returns false when no face
   samples this texture through a known palette. */
bool render_texture_by_faces(const u8* pixels, size_t pix_size,
                             int vram_x, int vram_y, int vram_w, int vram_h,
                             const TexFaceUse* faces, int n_faces,
                             const DatArchive& archive,
                             u8** out_rgba, int* out_w, int* out_h);

/*─── Background extraction (LZSS0 compressed) ──────────────────*/

struct BgTile {
    u8   pixels[256];  /* 16x16 8bpp tile data */
    u16  pal_idx;
};

struct Background {
    int    width, height;   /* in pixels */
    int    tile_cols, tile_rows;
    RGBA8* rgba;            /* width * height RGBA pixels (owned) */
    bool   valid;

    Background() : width(0), height(0), tile_cols(0), tile_rows(0),
                   rgba(0), valid(false) {}
    ~Background() { free(rgba); }
};

bool extract_background(const u8* dec_data, size_t dec_size,
                        Background& out);
bool is_background_data(const u8* dec_data, size_t dec_size);

/*─── 4× texture upscale (Scale2x cascade) with alpha smoothing ──*/

/* Upscale RGBA image 4× using two passes of Scale2x (EPX variant).
   Produces clean pixel-art-aware edges without bilinear blur.
   Alpha channel is dilated at boundaries to create smooth transitions.
   Returns malloc'd buffer (caller owns).  out_w = w*4, out_h = h*4. */
u8* upscale_4x_smooth(const u8* rgba, int w, int h,
                       int& out_w, int& out_h);

#endif /* DC_FORMATS_TEXTURE_H */
