/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  Color Conversion & Palette
 *  BGR555 ↔ RGBA, palette decode/encode
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_CORE_COLOR_H
#define DC_CORE_COLOR_H

#include "types.h"

/*─── BGR555 ↔ RGBA ──────────────────────────────────────────────*/
RGBA8 bgr555_to_rgba(u16 pixel);
u16   rgba_to_bgr555(u8 r, u8 g, u8 b, u8 a = 255);

/* Legacy: returns R,G,B only (a=255 implied) */
inline void decode_bgr555(u16 pixel, u8& r, u8& g, u8& b) {
    r = (u8)((pixel & 0x1F) << 3);
    g = (u8)(((pixel >> 5) & 0x1F) << 3);
    b = (u8)(((pixel >> 10) & 0x1F) << 3);
}

inline u16 encode_bgr555(u8 r, u8 g, u8 b, u8 stp = 0) {
    return (u16)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | (stp << 15));
}

/*─── Palette (256-color BGR555 CLUT) ────────────────────────────*/
/* Parse raw palette bytes into RGBA8 array.
   out must point to at least 256 RGBA8 entries per palette row.
   num_palettes = number of 512-byte palette rows to decode. */
void parse_palette(const u8* pal_data, size_t pal_size,
                   RGBA8* out, int num_palettes = 1);

/* Build 512 bytes of BGR555 from 256 RGBA8 entries. */
void build_palette(const RGBA8* colors, u8* out_512);

#endif /* DC_CORE_COLOR_H */
