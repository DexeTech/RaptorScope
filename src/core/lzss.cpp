/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  LZSS Compression (Capcom variant)
 *═══════════════════════════════════════════════════════════════════*/
#include "core/lzss.h"

bool lzss_decompress(const u8* src, size_t src_size, Buffer& dst)
{
    dst.clear();
    dst.reserve(src_size * 4);   /* typical expansion ratio */

    u32 flag = 1;
    size_t i = 0;

    while (i < src_size) {
        if (flag == 1) {
            if (i >= src_size) break;
            flag = src[i] | 0x100u;
            i++;
        }
        if (i >= src_size) break;

        u8 ch = src[i]; i++;

        if (flag & 1) {
            dst.push(ch);
        } else {
            if (i >= src_size) break;
            u8 t = src[i]; i++;
            u32 jump = ((u32)(t & 0x0F) << 8) | ch;
            u32 sz   = (t >> 4) + 2;
            size_t sp = dst.size - jump;
            for (u32 j = 0; j < sz; j++) {
                size_t idx = sp + j;
                if (idx < dst.size)
                    dst.push(dst.data[idx]);
                else
                    dst.push(0);
            }
        }
        flag >>= 1;
    }
    return dst.size > 0;
}

bool lzss_compress(const u8* src, size_t src_size, Buffer& dst)
{
    dst.clear();
    if (src_size == 0) return false;
    dst.reserve(src_size + src_size / 8 + 16);

    /* Hash chain match finder — same algorithm validated in Python
       against all 4 DAT files (37/37 entries round-trip pass). */
    const int HASH_SIZE = 1 << 14;
    const int HASH_MASK = HASH_SIZE - 1;
    int* head = (int*)malloc(HASH_SIZE * sizeof(int));
    int* prev = (int*)malloc(src_size * sizeof(int));
    if (!head || !prev) { free(head); free(prev); return false; }
    for (int i = 0; i < HASH_SIZE; i++) head[i] = -1;
    for (size_t i = 0; i < src_size; i++) prev[i] = -1;

    #define H3(p) (((p)+2 < (int)src_size) ? \
        (((int)src[p] << 5) ^ ((int)src[(p)+1] << 3) ^ (int)src[(p)+2]) & HASH_MASK : 0)

    size_t i = 0;
    while (i < src_size) {
        size_t flag_pos = dst.size;
        dst.push(0);
        u8 flag = 0;

        for (int bit = 0; bit < 8; bit++) {
            if (i >= src_size) break;

            int best_len = 1;
            int best_off = 0;
            int max_off  = (int)i;
            if (max_off > 4095) max_off = 4095;
            int max_len  = 17;
            if (i + max_len > src_size) max_len = (int)(src_size - i);

            /* Hash chain search */
            if (i + 2 < src_size) {
                int h = H3((int)i);
                int cp = head[h];
                int steps = 0;
                while (cp >= 0 && steps < 128) {
                    int d = (int)i - cp;
                    if (d > max_off) break;
                    int ml = 0;
                    while (ml < max_len && src[i + ml] == src[cp + ml]) ml++;
                    if (ml > best_len) { best_len = ml; best_off = d; }
                    if (best_len >= 17) break;
                    cp = prev[cp]; steps++;
                }
            }

            if (best_len >= 2) {
                dst.push((u8)(best_off & 0xFF));
                dst.push((u8)(((best_off >> 8) & 0x0F) |
                              ((best_len - 2) << 4)));
                /* Update hash chains for skipped positions */
                for (int k = 0; k < best_len; k++) {
                    if (i + k < src_size) {
                        int h = H3((int)(i + k));
                        prev[i + k] = head[h]; head[h] = (int)(i + k);
                    }
                }
                i += best_len;
            } else {
                flag |= (1 << bit);
                if (i + 2 < src_size) {
                    int h = H3((int)i);
                    prev[i] = head[h]; head[h] = (int)i;
                }
                dst.push(src[i]);
                i++;
            }
        }
        dst.data[flag_pos] = flag;
    }

    #undef H3
    free(head); free(prev);
    return dst.size > 0;
}
