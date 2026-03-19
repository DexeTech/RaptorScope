/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  PS1 TIM / IMD Image Format
 *═══════════════════════════════════════════════════════════════════*/
#include "formats/tim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool parse_tim_image(const u8* data, size_t size, TimImage& out)
{
    out.width = 0; out.height = 0; out.bpp = 0;
    if (out.rgba) { free(out.rgba); out.rgba = 0; }
    out.valid = false; out.has_clut = false;
    out.clut_colors = 0; out.clut_x = 0; out.clut_y = 0;
    out.px_x = 0; out.px_y = 0;

    if (size < 8 || data[0] != 0x10)
        return false;

    u32 flags = rd_u32(data + 4);
    int pmode     = flags & 0x7;
    int has_clut  = (flags >> 3) & 1;
    size_t off = 8;

    /* Parse CLUT */
    out.has_clut = (has_clut != 0);
    if (has_clut) {
        if (off + 12 > size) return false;
        u32 clut_len = rd_u32(data + off);
        out.clut_x = rd_u16(data + off + 4);
        out.clut_y = rd_u16(data + off + 6);
        u16 clut_w = rd_u16(data + off + 8);
        u16 clut_h = rd_u16(data + off + 10);
        out.clut_colors = clut_w * clut_h;
        if (out.clut_colors > 256) out.clut_colors = 256;

        for (int i = 0; i < out.clut_colors; i++) {
            size_t ci = off + 12 + (size_t)i * 2;
            if (ci + 2 > size) break;
            u16 c = rd_u16(data + ci);
            u8 r = (u8)((c & 0x1F) << 3);
            u8 g = (u8)(((c >> 5) & 0x1F) << 3);
            u8 b = (u8)(((c >> 10) & 0x1F) << 3);
            u8 a = (c == 0) ? 0 : 255;
            out.clut[i] = RGBA8(r, g, b, a);
        }
        off += clut_len;
    }

    /* Parse pixel data */
    if (off + 12 > size) return false;
    /* u32 pix_len = rd_u32(data + off); */
    out.px_x = rd_u16(data + off + 4);
    out.px_y = rd_u16(data + off + 6);
    u16 pix_w_hw = rd_u16(data + off + 8);
    u16 pix_h    = rd_u16(data + off + 10);
    const u8* pix = data + off + 12;
    size_t pix_avail = (off + 12 < size) ? size - off - 12 : 0;

    int actual_w = 0;
    switch (pmode) {
        case 0: actual_w = pix_w_hw * 4; break;    /* 4bpp */
        case 1: actual_w = pix_w_hw * 2; break;    /* 8bpp */
        case 2: actual_w = pix_w_hw;     break;    /* 16bpp */
        case 3: actual_w = (pix_w_hw * 2) / 3; break; /* 24bpp */
        default: return false;
    }

    out.width  = actual_w;
    out.height = pix_h;
    out.bpp = (pmode == 0) ? 4 : (pmode == 1) ? 8 : (pmode == 2) ? 16 : 24;

    int w = actual_w, h = pix_h;
    out.rgba = (RGBA8*)calloc(w * h, sizeof(RGBA8));
    if (!out.rgba) return false;

    if (pmode == 2) {
        /* 16bpp direct color */
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                size_t si = ((size_t)y * pix_w_hw + x) * 2;
                if (si + 1 >= pix_avail) continue;
                u16 c = rd_u16(pix + si);
                out.rgba[y * w + x] = RGBA8(
                    (u8)((c & 0x1F) << 3),
                    (u8)(((c >> 5) & 0x1F) << 3),
                    (u8)(((c >> 10) & 0x1F) << 3), 255);
            }
    } else if (pmode == 0) {
        /* 4bpp indexed */
        int byte_w = pix_w_hw * 2;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                size_t bi = (size_t)y * byte_w + x / 2;
                if (bi >= pix_avail) continue;
                int ci = (x % 2 == 0) ? (pix[bi] & 0xF) : ((pix[bi] >> 4) & 0xF);
                if (ci < out.clut_colors)
                    out.rgba[y * w + x] = out.clut[ci];
            }
    } else if (pmode == 1) {
        /* 8bpp indexed */
        int byte_w = pix_w_hw * 2;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                size_t bi = (size_t)y * byte_w + x;
                if (bi >= pix_avail) continue;
                int ci = pix[bi];
                if (ci < out.clut_colors)
                    out.rgba[y * w + x] = out.clut[ci];
            }
    } else if (pmode == 3) {
        /* 24bpp */
        int byte_w = pix_w_hw * 2;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                size_t bi = (size_t)y * byte_w + (size_t)x * 3;
                if (bi + 2 >= pix_avail) continue;
                out.rgba[y * w + x] = RGBA8(pix[bi], pix[bi+1], pix[bi+2], 255);
            }
    }

    out.valid = true;
    return true;
}

/*─── BMP export (bottom-up, BGRA) ───────────────────────────────*/
bool save_bmp(const char* path, const RGBA8* pixels, int w, int h)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    u32 row_bytes = ((u32)w * 3 + 3) & ~3u;
    u32 img_size  = row_bytes * (u32)h;
    u32 file_size = 54 + img_size;

    u8 hdr[54];
    memset(hdr, 0, 54);
    hdr[0] = 'B'; hdr[1] = 'M';
    wr_u32(hdr + 2,  file_size);
    wr_u32(hdr + 10, 54);
    wr_u32(hdr + 14, 40);
    wr_u32(hdr + 18, (u32)w);
    wr_u32(hdr + 22, (u32)h);
    wr_u16(hdr + 26, 1);
    wr_u16(hdr + 28, 24);
    wr_u32(hdr + 34, img_size);
    fwrite(hdr, 1, 54, f);

    u8* row = (u8*)malloc(row_bytes);
    for (int y = h - 1; y >= 0; y--) {
        memset(row, 0, row_bytes);
        for (int x = 0; x < w; x++) {
            const RGBA8& c = pixels[y * w + x];
            row[x * 3]     = c.b;
            row[x * 3 + 1] = c.g;
            row[x * 3 + 2] = c.r;
        }
        fwrite(row, 1, row_bytes, f);
    }
    free(row);
    fclose(f);
    return true;
}

/*═══════════════════════════════════════════════════════════════════
 *  Dino Crisis Standalone IMD (16bpp BGR555)
 *═══════════════════════════════════════════════════════════════════*/
bool is_imd_image(const u8* data, size_t size)
{
    if (size < 0x14) return false;
    u32 hdr_size = rd_u32(data + 0x00);
    u32 bpp_flag = rd_u32(data + 0x04);
    if (hdr_size != 0x10) return false;
    if (bpp_flag  != 0x02) return false;
    /* Verify that w/h sub-header and pixel block fit within size */
    u16 w = rd_u16(data + 0x10);
    u16 h = rd_u16(data + 0x12);
    if (w == 0 || h == 0) return false;
    size_t expected = 0x14 + (size_t)w * h * 2;
    if (expected > size) return false;
    return true;
}

bool parse_imd_image(const u8* data, size_t size, ImdImage& out)
{
    out.valid = false;
    if (out.rgba) { free(out.rgba); out.rgba = 0; }
    out.width = 0; out.height = 0;

    if (!is_imd_image(data, size)) return false;

    int w = (int)rd_u16(data + 0x10);
    int h = (int)rd_u16(data + 0x12);
    out.width  = w;
    out.height = h;

    out.rgba = (RGBA8*)malloc((size_t)w * h * sizeof(RGBA8));
    if (!out.rgba) return false;

    const u8* pix = data + 0x14;
    for (int i = 0; i < w * h; i++) {
        u16 c = rd_u16(pix + i * 2);
        u8 r = (u8)((c & 0x1F) << 3);
        u8 g = (u8)(((c >> 5)  & 0x1F) << 3);
        u8 b = (u8)(((c >> 10) & 0x1F) << 3);
        /* 0x8000 = STP=1, RGB=0: the PS1 transparent-black sentinel */
        u8 a = (c == 0x8000) ? 0 : 255;
        out.rgba[i] = RGBA8(r, g, b, a);
    }

    out.valid = true;
    return true;
}
bool save_tga(const char* path, const RGBA8* pixels, int w, int h)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    u8 hdr[18];
    memset(hdr, 0, 18);
    hdr[2]  = 2;         /* uncompressed true-color */
    wr_u16(hdr + 12, (u16)w);
    wr_u16(hdr + 14, (u16)h);
    hdr[16] = 32;        /* 32bpp */
    hdr[17] = 0x28;      /* top-down, 8-bit alpha */
    fwrite(hdr, 1, 18, f);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const RGBA8& c = pixels[y * w + x];
            u8 px[4] = { c.b, c.g, c.r, c.a };
            fwrite(px, 1, 4, f);
        }
    }
    fclose(f);
    return true;
}
