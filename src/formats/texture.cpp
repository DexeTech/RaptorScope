/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  Texture Rendering & Swizzle
 *═══════════════════════════════════════════════════════════════════*/
#include "formats/texture.h"
#include <string.h>
#include <stdlib.h>

/*─── VRAM Deswizzle ─────────────────────────────────────────────*/
/* Row-major block order: for each by, iterate bx (standard PS1 VRAM) */
void deswizzle_8bpp(const u8* src, size_t src_size,
                    u8* dst, int vram_w, int vram_h)
{
    int pw = vram_w * 2;       /* byte width */
    int bw = 64, bh = 32;     /* block size in bytes/rows */
    int bx_count = pw / bw;
    int full_by  = vram_h / bh;
    int remain_h = vram_h % bh;

    memset(dst, 0, pw * vram_h);
    size_t si = 0;

    for (int by = 0; by < full_by; by++) {
        for (int bx = 0; bx < bx_count; bx++) {
            for (int y = 0; y < bh; y++) {
                size_t di = (size_t)(by * bh + y) * pw + bx * bw;
                size_t end = si + bw;
                if (end <= src_size && di + bw <= (size_t)(pw * vram_h))
                    memcpy(dst + di, src + si, bw);
                si += bw;
            }
        }
    }
    if (remain_h > 0) {
        for (int bx = 0; bx < bx_count; bx++) {
            for (int y = 0; y < remain_h; y++) {
                size_t di = (size_t)(full_by * bh + y) * pw + bx * bw;
                size_t end = si + bw;
                if (end <= src_size && di + bw <= (size_t)(pw * vram_h))
                    memcpy(dst + di, src + si, bw);
                si += bw;
            }
        }
    }
}

/* Remain-first block order: remainder rows appear FIRST in source,
   then full blocks in row-major order (bx inner, by outer).
   Used by Dino Crisis item sprite banks. */
void deswizzle_8bpp_remfirst(const u8* src, size_t src_size,
                              u8* dst, int vram_w, int vram_h)
{
    int pw = vram_w * 2;
    int bw = 64, bh = 32;
    int bx_count = pw / bw;
    int full_by  = vram_h / bh;
    int remain_h = vram_h % bh;

    memset(dst, 0, pw * vram_h);
    size_t si = 0;

    /* 1. Remainder rows first in source → placed at bottom of output */
    if (remain_h > 0) {
        for (int bx = 0; bx < bx_count; bx++) {
            for (int y = 0; y < remain_h; y++) {
                size_t di = (size_t)(full_by * bh + y) * pw + bx * bw;
                if (si + bw <= src_size && di + bw <= (size_t)(pw * vram_h))
                    memcpy(dst + di, src + si, bw);
                si += bw;
            }
        }
    }

    /* 2. Full blocks in row-major order → placed top-down */
    for (int by = 0; by < full_by; by++) {
        for (int bx = 0; bx < bx_count; bx++) {
            for (int y = 0; y < bh; y++) {
                size_t di = (size_t)(by * bh + y) * pw + bx * bw;
                if (si + bw <= src_size && di + bw <= (size_t)(pw * vram_h))
                    memcpy(dst + di, src + si, bw);
                si += bw;
            }
        }
    }
}

void swizzle_8bpp(const u8* src, size_t src_size,
                  u8* dst, int vram_w, int vram_h)
{
    int pw = vram_w * 2;
    int bw = 64, bh = 32;
    int bx_count = pw / bw;
    int full_by  = vram_h / bh;
    int remain_h = vram_h % bh;

    size_t di = 0;
    for (int by = 0; by < full_by; by++) {
        for (int bx = 0; bx < bx_count; bx++) {
            for (int y = 0; y < bh; y++) {
                size_t si2 = (size_t)(by * bh + y) * pw + bx * bw;
                if (si2 + bw <= src_size && di + bw <= src_size)
                    memcpy(dst + di, src + si2, bw);
                di += bw;
            }
        }
    }
    if (remain_h > 0) {
        for (int bx = 0; bx < bx_count; bx++) {
            for (int y = 0; y < remain_h; y++) {
                size_t si2 = (size_t)(full_by * bh + y) * pw + bx * bw;
                if (si2 + bw <= src_size && di + bw <= src_size)
                    memcpy(dst + di, src + si2, bw);
                di += bw;
            }
        }
    }
}

/*─── 8bpp Render ────────────────────────────────────────────────*/
void render_8bpp(const u8* pixels, size_t pix_size,
                 const RGBA8* palette,
                 int vram_w, int vram_h,
                 u8* out_rgba, bool do_deswizzle,
                 int pal_row)
{
    int pw = vram_w * 2;  /* pixel width = byte width for 8bpp */
    size_t total = (size_t)pw * vram_h;

    /* Deswizzle if needed */
    u8* linear = 0;
    if (do_deswizzle && pix_size > 0) {
        linear = (u8*)malloc(total);
        if (!linear) return;
        deswizzle_8bpp(pixels, pix_size, linear, vram_w, vram_h);
    } else {
        linear = (u8*)pixels;  /* use directly */
    }

    /* Render to RGBA */
    for (int y = 0; y < vram_h; y++) {
        for (int x = 0; x < pw; x++) {
            size_t si = (size_t)y * pw + x;
            size_t di = ((size_t)y * pw + x) * 4;
            u8 idx = (si < total && si < pix_size) ? linear[si] : 0;
            RGBA8 c = palette[pal_row * 256 + idx];
            out_rgba[di]     = c.r;
            out_rgba[di + 1] = c.g;
            out_rgba[di + 2] = c.b;
            out_rgba[di + 3] = c.a;
        }
    }

    if (do_deswizzle && linear != pixels)
        free(linear);
}

/*─── 4bpp Render ────────────────────────────────────────────────*/
void render_4bpp(const u8* pixels, size_t pix_size,
                 const RGBA8* palette,
                 int vram_w, int vram_h,
                 u8* out_rgba, bool do_deswizzle,
                 int pal_row, int sub_pal)
{
    int byte_w = vram_w * 2;
    int pixel_w = byte_w * 2;  /* 4bpp: 2 pixels per byte */
    size_t total_bytes = (size_t)byte_w * vram_h;

    u8* linear = 0;
    if (do_deswizzle && pix_size > 0) {
        linear = (u8*)malloc(total_bytes);
        if (!linear) return;
        deswizzle_8bpp(pixels, pix_size, linear, vram_w, vram_h);
    } else {
        linear = (u8*)pixels;
    }

    int pal_base = pal_row * 256 + sub_pal * 16;

    for (int y = 0; y < vram_h; y++) {
        for (int bx = 0; bx < byte_w; bx++) {
            size_t si = (size_t)y * byte_w + bx;
            u8 byte_val = (si < total_bytes) ? linear[si] : 0;
            u8 lo = byte_val & 0x0F;
            u8 hi = (byte_val >> 4) & 0x0F;

            int px = bx * 2;
            size_t di0 = ((size_t)y * pixel_w + px) * 4;
            size_t di1 = ((size_t)y * pixel_w + px + 1) * 4;

            RGBA8 c0 = palette[pal_base + lo];
            RGBA8 c1 = palette[pal_base + hi];

            out_rgba[di0]     = c0.r; out_rgba[di0+1] = c0.g;
            out_rgba[di0+2]   = c0.b; out_rgba[di0+3] = c0.a;
            out_rgba[di1]     = c1.r; out_rgba[di1+1] = c1.g;
            out_rgba[di1+2]   = c1.b; out_rgba[di1+3] = c1.a;
        }
    }

    if (do_deswizzle && linear != pixels)
        free(linear);
}

/*─── 16bpp Render ───────────────────────────────────────────────*/
void render_16bpp(const u8* pixels, int w, int h, u8* out_rgba)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t si = ((size_t)y * w + x) * 2;
            u16 pix = rd_u16(pixels + si);
            RGBA8 c = bgr555_to_rgba(pix);
            size_t di = ((size_t)y * w + x) * 4;
            out_rgba[di]     = c.r;
            out_rgba[di + 1] = c.g;
            out_rgba[di + 2] = c.b;
            out_rgba[di + 3] = c.a;
        }
    }
}

/*─── General render dispatch ────────────────────────────────────*/
void texture_pixel_dims(int vram_w, int vram_h, int bpp,
                        int& out_w, int& out_h)
{
    out_h = vram_h;
    switch (bpp) {
        case 4:  out_w = vram_w * 4; break;
        case 8:  out_w = vram_w * 2; break;
        case 16: out_w = vram_w;     break;
        default: out_w = vram_w * 2; break;
    }
}

void render_texture(const u8* pixels, size_t pix_size,
                    const u8* pal_data, size_t pal_size,
                    int vram_w, int vram_h, int bpp,
                    u8* out_rgba, bool do_deswizzle,
                    int pal_row, int sub_pal)
{
    RGBA8 palette[4096];   /* up to 16 palette rows of 256 */
    for (int i = 0; i < 4096; i++) palette[i] = RGBA8(0, 0, 0, 0);
    if (pal_data && pal_size > 0) {
        int np = (int)(pal_size / 512);
        if (np < 1) np = 1;
        if (np > 16) np = 16;
        parse_palette(pal_data, pal_size, palette, np);
    }

    switch (bpp) {
        case 4:
            render_4bpp(pixels, pix_size, palette, vram_w, vram_h,
                        out_rgba, do_deswizzle, pal_row, sub_pal);
            break;
        case 16:
            render_16bpp(pixels, vram_w, vram_h, out_rgba);
            break;
        case 8:
        default:
            render_8bpp(pixels, pix_size, palette, vram_w, vram_h,
                        out_rgba, do_deswizzle, pal_row);
            break;
    }
}

/*─── Linear 8bpp Render (no VRAM block swizzle) ────────────────*/
/* If src_stride < pixel_w, data is stored as sequential vertical
   column strips of src_stride width (e.g. item sprite bank:
   src_stride=64, pixel_w=128 → two 64-wide columns stacked). */
void render_linear_8bpp(const u8* pixels, size_t pix_size,
                        const u8* pal_data, size_t pal_size,
                        int pixel_w, int pixel_h,
                        u8* out_rgba, int src_stride)
{
    /* Decode palette */
    RGBA8 palette[256];
    for (int i = 0; i < 256; i++) palette[i] = RGBA8(0, 0, 0, 0);
    if (pal_data && pal_size >= 2) {
        int ncolors = (int)(pal_size / 2);
        if (ncolors > 256) ncolors = 256;
        for (int i = 0; i < ncolors; i++) {
            u16 c = rd_u16(pal_data + i * 2);
            palette[i].r = (u8)((c & 0x1F) << 3);
            palette[i].g = (u8)(((c >> 5) & 0x1F) << 3);
            palette[i].b = (u8)(((c >> 10) & 0x1F) << 3);
            palette[i].a = (i == 0) ? 0 : ((c == 0) ? 0 : 255);
        }
    }

    if (src_stride <= 0) src_stride = pixel_w;

    for (int y = 0; y < pixel_h; y++) {
        for (int x = 0; x < pixel_w; x++) {
            size_t si;
            if (src_stride >= pixel_w) {
                /* Simple linear: src_stride == pixel_w */
                si = (size_t)y * pixel_w + x;
            } else {
                /* Column-strip layout: data stored as sequential
                   vertical columns of src_stride width */
                int col = x / src_stride;
                int lx  = x % src_stride;
                si = (size_t)col * pixel_h * src_stride
                   + (size_t)y * src_stride + lx;
            }
            size_t di = ((size_t)y * pixel_w + x) * 4;
            u8 idx = (si < pix_size) ? pixels[si] : 0;
            RGBA8 c = palette[idx];
            out_rgba[di]     = c.r;
            out_rgba[di + 1] = c.g;
            out_rgba[di + 2] = c.b;
            out_rgba[di + 3] = c.a;
        }
    }
}

/*─── Background extraction stub ─────────────────────────────────*/
bool is_background_data(const u8* dec_data, size_t dec_size)
{
    if (dec_size < 0x800) return false;
    /* Check for tile map signature at typical offset */
    /* Detailed heuristic matches Python is_lzss0_background() */
    u32 first = rd_u32(dec_data);
    if (first < 0x80000000u || first > 0x80200000u) return false;
    return true;  /* simplified check */
}

bool extract_background(const u8* dec_data, size_t dec_size,
                        Background& out)
{
    out.valid = false;
    /* TODO: Full background extraction matching Python */
    /* extract_lzss0_background() logic */
    (void)dec_data; (void)dec_size;
    return false;
}

/*─── 4× Texture Upscale (Scale2x cascade + alpha smoothing) ────*/

/* Pixel access helper (clamped edges) */
static inline void px_get(const u8* img, int w, int h, int x, int y, u8 out[4]) {
    if (x < 0) x = 0; if (x >= w) x = w - 1;
    if (y < 0) y = 0; if (y >= h) y = h - 1;
    const u8* p = img + ((size_t)y * w + x) * 4;
    out[0] = p[0]; out[1] = p[1]; out[2] = p[2]; out[3] = p[3];
}

static inline bool px_eq(const u8 a[4], const u8 b[4]) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

static inline bool px_neq(const u8 a[4], const u8 b[4]) {
    return !px_eq(a, b);
}

static inline void px_set(u8* img, int w, int x, int y, const u8 c[4]) {
    u8* p = img + ((size_t)y * w + x) * 4;
    p[0] = c[0]; p[1] = c[1]; p[2] = c[2]; p[3] = c[3];
}

/* Scale2x (EPX): 1 pixel → 2×2 block.
   For each pixel P with neighbors B(up), D(left), F(right), H(down):
     E0 = D==B && D!=H && B!=F ? D : P
     E1 = B==F && B!=D && F!=H ? F : P
     E2 = D==H && D!=B && H!=F ? D : P
     E3 = H==F && H!=D && F!=B ? F : P  */
static u8* scale2x(const u8* src, int w, int h) {
    int w2 = w * 2, h2 = h * 2;
    u8* dst = (u8*)calloc((size_t)w2 * h2, 4);
    if (!dst) return 0;

    u8 P[4], B[4], D[4], F[4], H[4];

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            px_get(src, w, h, x, y, P);
            px_get(src, w, h, x, y-1, B);
            px_get(src, w, h, x-1, y, D);
            px_get(src, w, h, x+1, y, F);
            px_get(src, w, h, x, y+1, H);

            u8 E0[4], E1[4], E2[4], E3[4];
            for (int c = 0; c < 4; c++) E0[c]=E1[c]=E2[c]=E3[c]=P[c];

            if (px_eq(D,B) && px_neq(D,H) && px_neq(B,F))
                { E0[0]=D[0]; E0[1]=D[1]; E0[2]=D[2]; E0[3]=D[3]; }
            if (px_eq(B,F) && px_neq(B,D) && px_neq(F,H))
                { E1[0]=F[0]; E1[1]=F[1]; E1[2]=F[2]; E1[3]=F[3]; }
            if (px_eq(D,H) && px_neq(D,B) && px_neq(H,F))
                { E2[0]=D[0]; E2[1]=D[1]; E2[2]=D[2]; E2[3]=D[3]; }
            if (px_eq(H,F) && px_neq(H,D) && px_neq(F,B))
                { E3[0]=F[0]; E3[1]=F[1]; E3[2]=F[2]; E3[3]=F[3]; }

            px_set(dst, w2, x*2,   y*2,   E0);
            px_set(dst, w2, x*2+1, y*2,   E1);
            px_set(dst, w2, x*2,   y*2+1, E2);
            px_set(dst, w2, x*2+1, y*2+1, E3);
        }
    }
    return dst;
}

/* Smooth alpha: 3×3 box blur on the alpha channel only.
   This creates anti-aliased transitions at transparency boundaries
   while keeping opaque/transparent interiors crisp. Only affects
   pixels near alpha edges (where neighbors have different alpha). */
static void smooth_alpha(u8* rgba, int w, int h) {
    /* Allocate temp alpha buffer */
    u8* tmp = (u8*)malloc((size_t)w * h);
    if (!tmp) return;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            u8 center_a = rgba[((size_t)y * w + x) * 4 + 3];

            /* Check if this pixel is near an alpha edge */
            bool is_edge = false;
            int sum = 0, count = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0) nx = 0; if (nx >= w) nx = w - 1;
                    if (ny < 0) ny = 0; if (ny >= h) ny = h - 1;
                    u8 na = rgba[((size_t)ny * w + nx) * 4 + 3];
                    sum += na;
                    count++;
                    if (na != center_a) is_edge = true;
                }
            }

            if (is_edge) {
                /* Smooth: weighted average (center 4× + neighbors 1×) */
                tmp[y * w + x] = (u8)((center_a * 4 + (sum - center_a)) / (count + 3));
            } else {
                tmp[y * w + x] = center_a;
            }
        }
    }

    /* Write back smoothed alpha */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            rgba[((size_t)y * w + x) * 4 + 3] = tmp[y * w + x];

    free(tmp);
}

u8* upscale_4x_smooth(const u8* rgba, int w, int h,
                       int& out_w, int& out_h)
{
    /* Pass 1: Scale2x (1× → 2×) */
    u8* s2 = scale2x(rgba, w, h);
    if (!s2) { out_w = w; out_h = h; return 0; }
    int w2 = w * 2, h2 = h * 2;

    /* Pass 2: Scale2x again (2× → 4×) */
    u8* s4 = scale2x(s2, w2, h2);
    free(s2);
    if (!s4) { out_w = w; out_h = h; return 0; }
    int w4 = w * 4, h4 = h * 4;

    /* Smooth alpha edges for clean transparency boundaries */
    smooth_alpha(s4, w4, h4);

    out_w = w4;
    out_h = h4;
    return s4;
}
