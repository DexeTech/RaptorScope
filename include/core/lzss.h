/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  LZSS Compression (Capcom variant)
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_CORE_LZSS_H
#define DC_CORE_LZSS_H

#include "types.h"

/* Decompress LZSS data.  Caller owns returned Buffer. */
bool lzss_decompress(const u8* src, size_t src_size, Buffer& dst);

/* Compress data to LZSS.  Caller owns returned Buffer. */
bool lzss_compress(const u8* src, size_t src_size, Buffer& dst);

#endif /* DC_CORE_LZSS_H */
