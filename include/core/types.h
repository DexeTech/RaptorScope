/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  Core Type Definitions
 *  Pre-C++11 compatible POD types for MinGW / MSVC targeting XP+
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_CORE_TYPES_H
#define DC_CORE_TYPES_H

/* Fixed-width integers  -  use stdint where available, else define */
#ifdef _MSC_VER
  #if _MSC_VER < 1600      /* VS2008 and earlier lack <stdint.h> */
    typedef signed   __int8   int8_t;
    typedef unsigned __int8   uint8_t;
    typedef signed   __int16  int16_t;
    typedef unsigned __int16  uint16_t;
    typedef signed   __int32  int32_t;
    typedef unsigned __int32  uint32_t;
    typedef signed   __int64  int64_t;
    typedef unsigned __int64  uint64_t;
  #else
    #include <stdint.h>
  #endif
#else
  #include <stdint.h>
#endif

#include <stddef.h>   /* size_t */
#include <stdlib.h>   /* malloc, realloc, free, calloc */
#include <string.h>   /* memcpy, memset */

/*─── Convenience aliases ─────────────────────────────────────────*/
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef float     f32;
typedef double    f64;

/*─── Simple dynamic buffer (replaces std::vector<uint8_t>) ──────*/
struct Buffer {
    u8*    data;
    size_t size;
    size_t capacity;

    Buffer() : data(0), size(0), capacity(0) {}
    ~Buffer() { free(data); }

    void reserve(size_t cap) {
        if (cap <= capacity) return;
        size_t newcap = capacity ? capacity : 64;
        while (newcap < cap) newcap *= 2;
        u8* p = (u8*)realloc(data, newcap);
        if (p) { data = p; capacity = newcap; }
    }
    void push(u8 byte) {
        if (size >= capacity) reserve(size + 1);
        data[size++] = byte;
    }
    void append(const u8* src, size_t len) {
        reserve(size + len);
        memcpy(data + size, src, len);
        size += len;
    }
    void clear() { size = 0; }
    void resize(size_t n, u8 fill = 0) {
        reserve(n);
        if (n > size) memset(data + size, fill, n - size);
        size = n;
    }

private:
    Buffer(const Buffer&);             /* no copy */
    Buffer& operator=(const Buffer&);
};

/*─── Simple dynamic array template ──────────────────────────────*/
template<typename T>
struct Array {
    T*     items;
    size_t count;
    size_t capacity;

    Array() : items(0), count(0), capacity(0) {}
    ~Array() { free(items); }

    void reserve(size_t cap) {
        if (cap <= capacity) return;
        size_t newcap = capacity ? capacity : 8;
        while (newcap < cap) newcap *= 2;
        T* p = (T*)realloc(items, newcap * sizeof(T));
        if (p) { items = p; capacity = newcap; }
    }
    void push(const T& val) {
        if (count >= capacity) reserve(count + 1);
        items[count++] = val;
    }
    T& operator[](size_t i) { return items[i]; }
    const T& operator[](size_t i) const { return items[i]; }
    void clear() { count = 0; }

private:
    Array(const Array&);
    Array& operator=(const Array&);
};

/*─── Simple non-owning string view ──────────────────────────────*/
struct StrView {
    const char* ptr;
    size_t      len;
    StrView() : ptr(""), len(0) {}
    StrView(const char* s) : ptr(s), len(s ? strlen(s) : 0) {}
    StrView(const char* s, size_t n) : ptr(s), len(n) {}
};

/*─── Fixed-size string for entry names, paths, etc ──────────────*/
struct Str256 {
    char buf[256];
    Str256() { buf[0] = '\0'; }
    Str256(const char* s) { set(s); }
    void set(const char* s) {
        if (!s) { buf[0] = '\0'; return; }
        size_t n = strlen(s);
        if (n > 255) n = 255;
        memcpy(buf, s, n);
        buf[n] = '\0';
    }
    const char* c_str() const { return buf; }
    size_t length() const { return strlen(buf); }
};

/*─── Color types ────────────────────────────────────────────────*/
struct RGBA8 {
    u8 r, g, b, a;
    RGBA8() : r(0), g(0), b(0), a(255) {}
    RGBA8(u8 r_, u8 g_, u8 b_, u8 a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}
};

struct Vec3 {
    f32 x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}
};

struct Vec2 {
    f32 x, y;
    Vec2() : x(0), y(0) {}
    Vec2(f32 x_, f32 y_) : x(x_), y(y_) {}
};

/*─── Math helpers ───────────────────────────────────────────────*/
#ifndef DC_PI
#define DC_PI 3.14159265358979323846f
#endif

inline f32 dc_clamp(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
inline int dc_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
inline u32 dc_align_sector(u32 size) {
    return (size + 0x7FF) & ~0x7FFu;
}

/*─── Read helpers (little-endian) ───────────────────────────────*/
inline u16 rd_u16(const u8* p) { return (u16)p[0] | ((u16)p[1] << 8); }
inline s16 rd_s16(const u8* p) { return (s16)rd_u16(p); }
inline u32 rd_u32(const u8* p) {
    return (u32)p[0] | ((u32)p[1]<<8) | ((u32)p[2]<<16) | ((u32)p[3]<<24);
}
inline s32 rd_s32(const u8* p) { return (s32)rd_u32(p); }

/*─── Write helpers (little-endian) ──────────────────────────────*/
inline void wr_u16(u8* p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v>>8); }
inline void wr_u32(u8* p, u32 v) {
    p[0]=(u8)v; p[1]=(u8)(v>>8); p[2]=(u8)(v>>16); p[3]=(u8)(v>>24);
}

#endif /* DC_CORE_TYPES_H */
