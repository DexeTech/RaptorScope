/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  Color Conversion & Palette
 *═══════════════════════════════════════════════════════════════════*/
#include "core/color.h"

RGBA8 bgr555_to_rgba(u16 pixel)
{
    u8 r = (u8)(( pixel        & 0x1F) << 3);  r |= r >> 5;
    u8 g = (u8)(((pixel >>  5) & 0x1F) << 3);  g |= g >> 5;
    u8 b = (u8)(((pixel >> 10) & 0x1F) << 3);  b |= b >> 5;
    /* Always return opaque.  Alpha is determined in parse_palette based on:
       - 0x0000 → transparent (PSX mask)
       - STP bit (bit 15) → semi-transparent (alpha=128) or transparent if black
       - STP=0 + non-zero → opaque
       bgr555_to_rgba is also used for direct 16bpp pixels and BMP export
       where STP doesn't apply, so we always return alpha=255 here. */
    return RGBA8(r, g, b, 255);
}

u16 rgba_to_bgr555(u8 r, u8 g, u8 b, u8 a)
{
    if (a < 128 && r == 0 && g == 0 && b == 0)
        return 0x0000;  /* transparent black */
    u8 stp = 0;
    if (a < 255) stp = 1;
    return (u16)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | (stp << 15));
}

void parse_palette(const u8* pal_data, size_t pal_size,
                   RGBA8* out, int num_palettes)
{
    for (int p = 0; p < num_palettes; p++) {
        for (int c = 0; c < 256; c++) {
            size_t off = (size_t)p * 512 + (size_t)c * 2;
            if (off + 2 <= pal_size) {
                u16 pix = rd_u16(pal_data + off);
                out[p * 256 + c] = bgr555_to_rgba(pix);
                /* PS1 transparency at palette level:
                   - 0x0000: masked pixel (never drawn on PSX) → alpha = 0
                   - 0x8000: STP=1 + black. On opaque primitives this draws as
                     black; on semi-transparent primitives it blends (≈ invisible).
                     Context-dependent — atlas builder handles this per-sub-palette
                     by checking if the sub-palette is a decal overlay.
                   - STP=1 + color: per-pixel semi-transparency enable flag.
                     Only triggers blending for semi-transparent primitives.
                     Atlas builder applies alpha=128 only for decal sub-palettes. */
                if (pix == 0x0000)
                    out[p * 256 + c].a = 0;
                /* else: alpha stays 255 (opaque), including 0x8000 */
            } else {
                out[p * 256 + c] = RGBA8(0, 0, 0, 255);
            }
        }
    }
}

void build_palette(const RGBA8* colors, u8* out_512)
{
    for (int i = 0; i < 256; i++) {
        u16 v = rgba_to_bgr555(colors[i].r, colors[i].g, colors[i].b, colors[i].a);
        wr_u16(out_512 + i * 2, v);
    }
}
