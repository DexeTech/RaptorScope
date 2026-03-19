/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  UV Editor Modal
 *  Displays UV layout overlaid on texture atlas, grouped by tpage
 *  (material). Pan/zoom viewport, per-material visibility toggles,
 *  dark-themed, fully self-contained.
 *
 *  Follows the about_dialog.h pattern: everything allocated on
 *  ShowUVEditor(), freed on WM_DESTROY.  Zero file-scope state.
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_UV_EDITOR_H
#define DC_UV_EDITOR_H

#include "ui/app.h"
#include "formats/mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ─── Layout constants ─────────────────────────────────────────── */
#define UVE_DEF_W       960
#define UVE_DEF_H       640
#define UVE_MIN_W       560
#define UVE_MIN_H       400
#define UVE_TITLE_H     30
#define UVE_STATUS_H    22
#define UVE_SIDEBAR_W   186
#define UVE_CLOSE_SZ    24
#define UVE_MAT_ROW_H   28
#define UVE_MAT_PAD     6
#define UVE_CHECK_SZ    14
#define UVE_MAX_MATS    64
#define UVE_FADE_STEPS  10
#define UVE_FADE_MS     14

/* ─── Hit zones ────────────────────────────────────────────────── */
#define UVE_ZONE_NONE    0
#define UVE_ZONE_CLOSE   1
#define UVE_ZONE_MAT     2   /* hot_mat_idx valid */
#define UVE_ZONE_BTN_ALL 3
#define UVE_ZONE_BTN_NON 4
#define UVE_ZONE_VP      5
#define UVE_ZONE_MAXIMIZE 6
#define UVE_ZONE_MAT_SOLO 7  /* solo icon on material row */

/* ─── Material wireframe palette ───────────────────────────────── */
static const COLORREF g_uvMatColors[] = {
    RGB(  0,230,230), RGB(255,100,220), RGB(240,230, 40),
    RGB(100,240,100), RGB(255,160, 50), RGB(100,180,255),
    RGB(255,110,110), RGB(190,150,255), RGB(  0,200,150),
    RGB(255,200,100), RGB(180,220, 80), RGB(220,130,180),
    RGB(130,210,250), RGB(230,180,130), RGB(160,255,200),
    RGB(255,160,160),
};
#define UVE_NUM_PAL  (sizeof(g_uvMatColors)/sizeof(g_uvMatColors[0]))

/* ─── Per-material group ───────────────────────────────────────── */
struct UVMatGroup {
    u16      tpage;
    int      first_tri;      /* index into sorted array */
    int      count;
    bool     visible;
    COLORREF color;
    bool     in_range;       /* tpage UVs map within loaded texture */
    float    uv_min_x, uv_min_y, uv_max_x, uv_max_y; /* UV pixel bbox */
};

/* ─── Editor state ─────────────────────────────────────────────── */
struct UVEditorState {
    HWND     hwnd;
    HFONT    hFont, hFontBold, hFontSmall;
    int      win_w, win_h;

    /* Texture */
    HBITMAP  hTexDib;
    void*    texDibBits;
    int      tex_w, tex_h;
    u16      tex_vram_x;      /* VRAM halfword X of texture entry */
    u16      tex_vram_y;      /* VRAM Y of texture entry */
    int      tex_bpp;         /* texture BPP as rendered (4 or 8) */
    bool     has_tex;

    /* Multi-palette atlas data (owned copy) */
    u8*      atlas_rgba;      /* full RGBA atlas: atlas_w × (slice_h × num_pal_rows) */
    int      atlas_w;         /* pixel width of full atlas (before crop) */
    int      atlas_full_h;    /* total atlas height = slice_h * num_pal_rows */
    int      slice_h;         /* height of one palette row slice */
    int      num_pal_rows;    /* number of CLUT rows in atlas */
    int      num_sub_pals;    /* sub-palette slices per CLUT row (1 if 8bpp only) */
    int      cur_pal_row;     /* currently displayed palette row */
    int      crop_x, crop_w;  /* pixel X crop region */

    /* Mesh data (owned copy, sorted by tpage) */
    MeshTri* tris;
    int      tri_count;
    int*     sort_map;        /* sort_map[sorted_i] = original index */

    /* Material groups */
    UVMatGroup mats[UVE_MAX_MATS];
    int      mat_count;
    int      total_visible_tris;

    /* Viewport transform: screen = texel * zoom + pan */
    double   zoom;
    double   pan_x, pan_y;

    /* Drag state */
    bool     dragging;
    int      drag_btn;        /* 0=LMB, 1=RMB, 2=MMB */
    int      drag_sx, drag_sy;
    double   drag_pan_x, drag_pan_y;

    /* Selection state */
    bool*    sel_mask;         /* external ptr: sel_mask[original_idx] */
    int      sel_mask_count;   /* size of sel_mask array */
    bool     sel_dragging;     /* LMB rectangle drag in progress */
    int      sel_start_x, sel_start_y;  /* rect start (screen) */
    int      sel_cur_x, sel_cur_y;      /* rect current (screen) */
    int      sel_mode;         /* 0=replace, 1=add(ctrl), 2=deselect(ctrl+alt) */
    HWND     sel_parent;       /* parent 3D panel HWND for live repaint */

    /* UI state */
    int      hot_zone;
    int      hot_mat_idx;
    bool     show_checker;
    bool     show_fill;       /* semi-transparent fill on UV faces */
    bool     show_texture;    /* texture backdrop visible */
    bool     show_uv_wire;    /* UV wireframe visible */
    int      sidebar_scroll;
    int      fadeAlpha;
    int      solo_mat;        /* -1 = off, 0..N = solo'd material idx */
    bool     maximized;
    RECT     restore_rect;    /* saved window rect for maximize/restore */

    /* Cursor readout */
    float    cursor_u, cursor_v;
    bool     cursor_in_vp;
};

/* ─── Helpers ──────────────────────────────────────────────────── */
static inline UVEditorState* Uve_Get(HWND h) {
    return (UVEditorState*)(LONG_PTR)GetWindowLongPtrA(h, GWLP_USERDATA);
}

static inline void Uve_VpRect(UVEditorState* s, RECT* r) {
    r->left = 0;  r->top = UVE_TITLE_H;
    r->right = s->win_w - UVE_SIDEBAR_W;
    r->bottom = s->win_h - UVE_STATUS_H;
}

static inline void Uve_SideRect(UVEditorState* s, RECT* r) {
    r->left = s->win_w - UVE_SIDEBAR_W;  r->top = UVE_TITLE_H;
    r->right = s->win_w;
    r->bottom = s->win_h - UVE_STATUS_H;
}

static inline void Uve_StatusRect(UVEditorState* s, RECT* r) {
    r->left = 0;  r->top = s->win_h - UVE_STATUS_H;
    r->right = s->win_w;  r->bottom = s->win_h;
}

/* ─── Rebuild texture DIB from atlas for given palette row ──────── */
static void Uve_RebuildTexDib(UVEditorState* s, int pal_row) {
    if (!s->atlas_rgba || s->num_pal_rows < 1) return;
    if (pal_row < 0) pal_row = 0;
    if (pal_row >= s->num_pal_rows) pal_row = s->num_pal_rows - 1;
    s->cur_pal_row = pal_row;

    int cw = (s->crop_w > 0) ? s->crop_w : s->atlas_w;
    int cx = (s->crop_w > 0) ? s->crop_x : 0;
    int sh = s->slice_h;
    s->tex_w = cw; s->tex_h = sh;

    /* Delete old DIB */
    if (s->hTexDib) { DeleteObject(s->hTexDib); s->hTexDib = 0; s->texDibBits = 0; }

    BITMAPINFO bmi; memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth    = cw;
    bmi.bmiHeader.biHeight   = -sh;
    bmi.bmiHeader.biPlanes   = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    HDC hdc = GetDC(NULL);
    s->hTexDib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &s->texDibBits, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (!s->hTexDib || !s->texDibBits) { s->has_tex = false; return; }

    u8* dst = (u8*)s->texDibBits;
    /* Atlas layout: slices are [row0_sub0, row0_sub1, ..., row1_sub0, ...].
       For the UV editor default, show sub=0 (8bpp pixel-doubled) for each row. */
    int nsp = s->num_sub_pals > 0 ? s->num_sub_pals : 1;
    size_t row_y_off = (size_t)pal_row * nsp * sh * s->atlas_w * 4;
    for (int y = 0; y < sh; y++) {
        const u8* src_line = s->atlas_rgba + row_y_off + ((size_t)y * s->atlas_w + cx) * 4;
        u8* dst_line = dst + (size_t)y * cw * 4;
        for (int x = 0; x < cw; x++) {
            u8 r = src_line[x*4+0], g = src_line[x*4+1];
            u8 b = src_line[x*4+2], a = src_line[x*4+3];
            dst_line[x*4+0] = (u8)((b * a + 127) / 255);
            dst_line[x*4+1] = (u8)((g * a + 127) / 255);
            dst_line[x*4+2] = (u8)((r * a + 127) / 255);
            dst_line[x*4+3] = a;
        }
    }
    s->has_tex = true;
}

/* ─── UV pixel coords from tri face ────────────────────────────── */
static inline void Uve_TriUV(const UVEditorState* s, const MeshTri& t, int vi,
                             float* ou, float* ov) {
    /* Tpage TX/TY/TP decode */
    int tx = t.tpage & 0xF;
    int ty = (t.tpage >> 4) & 1;
    int tp = (t.tpage >> 7) & 3;
    int face_ppw = (tp == 0) ? 4 : (tp == 2) ? 1 : 2;
    int tex_ppw  = (s->tex_bpp == 4) ? 4 : (s->tex_bpp == 16) ? 1 : 2;
    /* Pixel offset in rendered texture */
    float u_off = (float)((tx * 64 - (int)s->tex_vram_x) * tex_ppw);
    /* UV scale: face page is 256 face-pixels, which covers
       256*(tex_ppw/face_ppw) pixels in our rendered texture */
    float u_scl = (float)tex_ppw / (float)face_ppw;
    float v_off = (float)(ty * 256 - (int)s->tex_vram_y);
    *ou = u_off + (float)t.uv[vi][0] * u_scl;
    *ov = v_off + (float)t.uv[vi][1];
}

/* ─── Tri sort compare (by tpage) ──────────────────────────────── */
static int Uve_CmpTri(const void* a, const void* b) {
    return (int)((const MeshTri*)a)->tpage - (int)((const MeshTri*)b)->tpage;
}

/* ─── Build material groups from sorted tris ───────────────────── */
static void Uve_BuildMats(UVEditorState* s) {
    s->mat_count = 0;
    if (!s->tris || s->tri_count == 0) return;
    int ci = 0;
    while (ci < s->tri_count && s->mat_count < UVE_MAX_MATS) {
        u16 tp = s->tris[ci].tpage;
        int start = ci;
        while (ci < s->tri_count && s->tris[ci].tpage == tp) ci++;
        UVMatGroup& g = s->mats[s->mat_count];
        g.tpage     = tp;
        g.first_tri = start;
        g.count     = ci - start;
        g.visible   = true;
        g.color     = g_uvMatColors[s->mat_count % UVE_NUM_PAL];
        g.in_range  = true; /* updated by Uve_ComputeMatRanges */
        g.uv_min_x = g.uv_min_y = g.uv_max_x = g.uv_max_y = 0;
        s->mat_count++;
    }
}

/* ─── Compute UV bounding box per material and flag in-range ──── */
static void Uve_ComputeMatRanges(UVEditorState* s) {
    float tw = (s->tex_w > 0) ? (float)s->tex_w : 256.0f;
    float th = (s->tex_h > 0) ? (float)s->tex_h : 512.0f;
    for (int mi = 0; mi < s->mat_count; mi++) {
        UVMatGroup& g = s->mats[mi];
        g.uv_min_x = 1e9f; g.uv_min_y = 1e9f;
        g.uv_max_x = -1e9f; g.uv_max_y = -1e9f;
        for (int ti = g.first_tri; ti < g.first_tri + g.count; ti++) {
            for (int vi = 0; vi < 3; vi++) {
                float u, v;
                Uve_TriUV(s, s->tris[ti], vi, &u, &v);
                if (u < g.uv_min_x) g.uv_min_x = u;
                if (v < g.uv_min_y) g.uv_min_y = v;
                if (u > g.uv_max_x) g.uv_max_x = u;
                if (v > g.uv_max_y) g.uv_max_y = v;
            }
        }
        /* In range if UV bbox overlaps texture rect [0, tw) x [0, th) */
        float margin = 8.0f; /* small tolerance */
        g.in_range = (g.uv_max_x > -margin && g.uv_min_x < tw + margin &&
                      g.uv_max_y > -margin && g.uv_min_y < th + margin);
        /* Auto-hide out-of-range groups */
        if (!g.in_range) g.visible = false;
    }
}

/* ─── Effective visibility (respects solo mode) ───────────────── */
static inline bool Uve_MatVisible(UVEditorState* s, int mi) {
    if (s->solo_mat >= 0) return (mi == s->solo_mat);
    return s->mats[mi].visible;
}

/* ─── Fit UV space into viewport ───────────────────────────────── */
static void Uve_FitView(UVEditorState* s) {
    RECT vp; Uve_VpRect(s, &vp);
    int vw = vp.right - vp.left, vh = vp.bottom - vp.top;
    if (vw < 1 || vh < 1) return;
    /* UV space = texture dimensions (or 256x512 fallback) */
    double tw = (s->tex_w > 0) ? s->tex_w : 256.0;
    double th = (s->tex_h > 0) ? s->tex_h : 512.0;
    double zx = (vw - 20.0) / tw, zy = (vh - 20.0) / th;
    s->zoom = (zx < zy) ? zx : zy;
    if (s->zoom < 0.05) s->zoom = 0.05;
    s->pan_x = (vw - tw * s->zoom) * 0.5;
    s->pan_y = (vh - th * s->zoom) * 0.5;
}

/* ─── Zoom toward point ────────────────────────────────────────── */
static void Uve_ZoomAt(UVEditorState* s, int sx, int sy, double factor) {
    RECT vp; Uve_VpRect(s, &vp);
    double lx = sx - vp.left, ly = sy - vp.top;
    double old_z = s->zoom;
    s->zoom *= factor;
    if (s->zoom < 0.05)  s->zoom = 0.05;
    if (s->zoom > 64.0)  s->zoom = 64.0;
    s->pan_x = lx - (lx - s->pan_x) * (s->zoom / old_z);
    s->pan_y = ly - (ly - s->pan_y) * (s->zoom / old_z);
}

/* ─── Hit test ─────────────────────────────────────────────────── */
static int Uve_HitTest(UVEditorState* s, int x, int y, int* out_mat) {
    *out_mat = -1;
    /* Close button */
    int cx = s->win_w - UVE_CLOSE_SZ - 4, cy = 3;
    if (x >= cx && x < cx + UVE_CLOSE_SZ && y >= cy && y < cy + UVE_CLOSE_SZ)
        return UVE_ZONE_CLOSE;
    /* Maximize button (left of close) */
    int mx2 = cx - UVE_CLOSE_SZ - 2;
    if (x >= mx2 && x < mx2 + UVE_CLOSE_SZ && y >= cy && y < cy + UVE_CLOSE_SZ)
        return UVE_ZONE_MAXIMIZE;
    /* Sidebar material rows */
    RECT side; Uve_SideRect(s, &side);
    if (x >= side.left && x < side.right && y >= side.top && y < side.bottom) {
        /* Content starts 24px below side.top (matching paint's content.top) */
        int content_top = side.top + 24;
        if (y < content_top) return UVE_ZONE_NONE; /* header area */
        int ry = y - content_top + s->sidebar_scroll;
        /* Buttons at bottom of material list */
        int list_h = s->mat_count * UVE_MAT_ROW_H;
        int btn_y = list_h + 10;
        if (ry >= btn_y && ry < btn_y + 22) {
            int bx = x - side.left;
            if (bx >= UVE_MAT_PAD && bx < UVE_MAT_PAD + 40)
                return UVE_ZONE_BTN_ALL;
            if (bx >= UVE_MAT_PAD + 48 && bx < UVE_MAT_PAD + 88)
                return UVE_ZONE_BTN_NON;
        }
        /* Material row */
        int mi = ry / UVE_MAT_ROW_H;
        if (mi >= 0 && mi < s->mat_count) {
            *out_mat = mi;
            /* Solo icon area: right side of row */
            int solo_x = side.right - UVE_MAT_PAD - 14;
            if (x >= solo_x && x < solo_x + 14)
                return UVE_ZONE_MAT_SOLO;
            return UVE_ZONE_MAT;
        }
        return UVE_ZONE_NONE;
    }
    /* Viewport */
    RECT vp; Uve_VpRect(s, &vp);
    if (x >= vp.left && x < vp.right && y >= vp.top && y < vp.bottom)
        return UVE_ZONE_VP;
    return UVE_ZONE_NONE;
}

/* ─── Count visible tris ───────────────────────────────────────── */
static void Uve_CountVisible(UVEditorState* s) {
    s->total_visible_tris = 0;
    for (int i = 0; i < s->mat_count; i++)
        if (Uve_MatVisible(s, i)) s->total_visible_tris += s->mats[i].count;
}

/* ─── Selection helpers ───────────────────────────────────────── */
static int Uve_SelCount(UVEditorState* s) {
    if (!s->sel_mask) return 0;
    int c = 0;
    for (int i = 0; i < s->sel_mask_count; i++)
        if (s->sel_mask[i]) c++;
    return c;
}

/* 2D cross product sign */
static inline float Uve_Cross2D(float ax, float ay, float bx, float by) {
    return ax * by - ay * bx;
}

/* Point in triangle (UV pixel space) */
static bool Uve_PtInTri(float px, float py,
                        float x0, float y0, float x1, float y1, float x2, float y2) {
    float d1 = Uve_Cross2D(x1-x0, y1-y0, px-x0, py-y0);
    float d2 = Uve_Cross2D(x2-x1, y2-y1, px-x1, py-y1);
    float d3 = Uve_Cross2D(x0-x2, y0-y2, px-x2, py-y2);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

/* Check if any vertex of triangle is inside rect (screen coords) */
static bool Uve_TriInRect(UVEditorState* s, const MeshTri& t,
                          int rx0, int ry0, int rx1, int ry1) {
    RECT vp; Uve_VpRect(s, &vp);
    for (int vi = 0; vi < 3; vi++) {
        float u, v; Uve_TriUV(s, t, vi, &u, &v);
        int sx = vp.left + (int)(u * s->zoom + s->pan_x);
        int sy = vp.top  + (int)(v * s->zoom + s->pan_y);
        if (sx >= rx0 && sx <= rx1 && sy >= ry0 && sy <= ry1)
            return true;
    }
    return false;
}

/* ═══ RENDERING ════════════════════════════════════════════════════ */

static void Uve_DrawChecker(HDC hdc, int x0, int y0, int x1, int y1,
                            int sz, COLORREF c1, COLORREF c2) {
    HBRUSH hb1 = CreateSolidBrush(c1), hb2 = CreateSolidBrush(c2);
    for (int cy = y0; cy < y1; cy += sz) {
        for (int cx = x0; cx < x1; cx += sz) {
            RECT r = { cx, cy, cx+sz < x1 ? cx+sz : x1, cy+sz < y1 ? cy+sz : y1 };
            int which = ((cx - x0) / sz + (cy - y0) / sz) & 1;
            FillRect(hdc, &r, which ? hb2 : hb1);
        }
    }
    DeleteObject(hb1); DeleteObject(hb2);
}

static void Uve_PaintViewport(HDC buf, UVEditorState* s) {
    RECT vp; Uve_VpRect(s, &vp);
    int vw = vp.right - vp.left, vh = vp.bottom - vp.top;
    if (vw < 1 || vh < 1) return;

    /* Clip to viewport */
    HRGN hClip = CreateRectRgn(vp.left, vp.top, vp.right, vp.bottom);
    SelectClipRgn(buf, hClip);

    /* Background */
    COLORREF bgCol = T.isDark ? RGB(24,24,30) : RGB(220,220,228);
    HBRUSH hBg = CreateSolidBrush(bgCol);
    FillRect(buf, &vp, hBg);
    DeleteObject(hBg);

    /* Texture region in screen coords */
    double tw = (s->tex_w > 0) ? s->tex_w : 256.0;
    double th = (s->tex_h > 0) ? s->tex_h : 512.0;
    int tx0 = vp.left + (int)s->pan_x;
    int ty0 = vp.top  + (int)s->pan_y;
    int tx1 = tx0 + (int)(tw * s->zoom);
    int ty1 = ty0 + (int)(th * s->zoom);

    /* Checkerboard behind texture area */
    if (s->show_checker) {
        int cksz = (int)(8.0 * s->zoom);
        if (cksz < 2) cksz = 2; if (cksz > 64) cksz = 64;
        COLORREF ck1 = T.isDark ? RGB(40,40,48) : RGB(200,200,210);
        COLORREF ck2 = T.isDark ? RGB(55,55,64) : RGB(235,235,240);
        int cx0 = tx0 > vp.left ? tx0 : vp.left;
        int cy0 = ty0 > vp.top  ? ty0 : vp.top;
        int cx1 = tx1 < vp.right  ? tx1 : vp.right;
        int cy1 = ty1 < vp.bottom ? ty1 : vp.bottom;
        if (cx0 < cx1 && cy0 < cy1)
            Uve_DrawChecker(buf, cx0, cy0, cx1, cy1, cksz, ck1, ck2);
    }

    /* Blit texture with alpha */
    if (s->show_texture && s->has_tex && s->hTexDib) {
        HDC hdcTex = CreateCompatibleDC(buf);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcTex, s->hTexDib);
        SetStretchBltMode(buf, COLORONCOLOR);
        BLENDFUNCTION bf; memset(&bf, 0, sizeof(bf));
        bf.BlendOp = AC_SRC_OVER;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;
        AlphaBlend(buf, tx0, ty0, tx1-tx0, ty1-ty0,
                   hdcTex, 0, 0, s->tex_w, s->tex_h, bf);
        SelectObject(hdcTex, hOld);
        DeleteDC(hdcTex);
    }

    /* Texture border */
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, T.isDark ? RGB(80,80,96) : RGB(150,150,170));
    HPEN hOldPen = (HPEN)SelectObject(buf, hBorderPen);
    SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
    Rectangle(buf, tx0, ty0, tx1, ty1);
    SelectObject(buf, hOldPen);
    DeleteObject(hBorderPen);

    /* ── UV wireframe ─────────────────────────────────────────── */
    if (s->show_uv_wire)
    for (int mi = 0; mi < s->mat_count; mi++) {
        UVMatGroup& g = s->mats[mi];
        if (!Uve_MatVisible(s, mi)) continue;
        HPEN hPen = CreatePen(PS_SOLID, 1, g.color);
        SelectObject(buf, hPen);

        for (int ti = g.first_tri; ti < g.first_tri + g.count; ti++) {
            const MeshTri& tri = s->tris[ti];
            POINT pts[4];
            for (int vi = 0; vi < 3; vi++) {
                float u, v; Uve_TriUV(s, tri, vi, &u, &v);
                pts[vi].x = vp.left + (int)(u * s->zoom + s->pan_x);
                pts[vi].y = vp.top  + (int)(v * s->zoom + s->pan_y);
            }
            pts[3] = pts[0]; /* close the triangle */

            /* Optional semi-transparent fill */
            if (s->show_fill) {
                /* Approximate: draw with 25% opacity using hatched brush */
                /* GDI doesn't have alpha, use colored hatching instead */
            }

            MoveToEx(buf, pts[0].x, pts[0].y, NULL);
            LineTo(buf, pts[1].x, pts[1].y);
            LineTo(buf, pts[2].x, pts[2].y);
            LineTo(buf, pts[3].x, pts[3].y);
        }
        SelectObject(buf, hOldPen);
        DeleteObject(hPen);
    }

    /* UV vertex dots at high zoom */
    if (s->show_uv_wire && s->zoom >= 4.0) {
        for (int mi = 0; mi < s->mat_count; mi++) {
            UVMatGroup& g = s->mats[mi];
            if (!Uve_MatVisible(s, mi)) continue;
            for (int ti = g.first_tri; ti < g.first_tri + g.count; ti++) {
                const MeshTri& tri = s->tris[ti];
                for (int vi = 0; vi < 3; vi++) {
                    float u, v; Uve_TriUV(s, tri, vi, &u, &v);
                    int sx = vp.left + (int)(u * s->zoom + s->pan_x);
                    int sy = vp.top  + (int)(v * s->zoom + s->pan_y);
                    RECT dot = { sx-2, sy-2, sx+3, sy+3 };
                    HBRUSH hd = CreateSolidBrush(g.color);
                    FillRect(buf, &dot, hd);
                    DeleteObject(hd);
                }
            }
        }
    }

    /* ── Selection overlay (red triangles) ────────────────────── */
    if (s->sel_mask) {
        /* Pass 1: hatched red fill for selected tris */
        HBRUSH hSelBr = CreateHatchBrush(HS_DIAGCROSS, RGB(255, 50, 40));
        HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
        SelectObject(buf, hNullPen);
        SetBkMode(buf, TRANSPARENT);
        HBRUSH hOldBr2 = (HBRUSH)SelectObject(buf, hSelBr);

        for (int ti = 0; ti < s->tri_count; ti++) {
            int orig = s->sort_map ? s->sort_map[ti] : ti;
            if (orig < 0 || orig >= s->sel_mask_count) continue;
            if (!s->sel_mask[orig]) continue;

            const MeshTri& tri = s->tris[ti];
            POINT pts[3];
            for (int vi = 0; vi < 3; vi++) {
                float u, v; Uve_TriUV(s, tri, vi, &u, &v);
                pts[vi].x = vp.left + (int)(u * s->zoom + s->pan_x);
                pts[vi].y = vp.top  + (int)(v * s->zoom + s->pan_y);
            }
            Polygon(buf, pts, 3);
        }
        SelectObject(buf, hOldBr2);
        DeleteObject(hSelBr);
        DeleteObject(hNullPen);

        /* Pass 2: bright red outline */
        HPEN hSelOut = CreatePen(PS_SOLID, 2, RGB(255, 50, 30));
        SelectObject(buf, hSelOut);
        SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
        for (int ti = 0; ti < s->tri_count; ti++) {
            int orig = s->sort_map ? s->sort_map[ti] : ti;
            if (orig < 0 || orig >= s->sel_mask_count) continue;
            if (!s->sel_mask[orig]) continue;

            const MeshTri& tri = s->tris[ti];
            POINT pts[4];
            for (int vi = 0; vi < 3; vi++) {
                float u, v; Uve_TriUV(s, tri, vi, &u, &v);
                pts[vi].x = vp.left + (int)(u * s->zoom + s->pan_x);
                pts[vi].y = vp.top  + (int)(v * s->zoom + s->pan_y);
            }
            pts[3] = pts[0];
            MoveToEx(buf, pts[0].x, pts[0].y, NULL);
            LineTo(buf, pts[1].x, pts[1].y);
            LineTo(buf, pts[2].x, pts[2].y);
            LineTo(buf, pts[3].x, pts[3].y);
        }
        DeleteObject(hSelOut);
    }

    /* ── Selection rectangle ──────────────────────────────────── */
    if (s->sel_dragging) {
        HPEN hRPen = CreatePen(PS_DOT, 1, RGB(255, 100, 80));
        SelectObject(buf, hRPen);
        SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
        int rx0 = s->sel_start_x < s->sel_cur_x ? s->sel_start_x : s->sel_cur_x;
        int ry0 = s->sel_start_y < s->sel_cur_y ? s->sel_start_y : s->sel_cur_y;
        int rx1 = s->sel_start_x > s->sel_cur_x ? s->sel_start_x : s->sel_cur_x;
        int ry1 = s->sel_start_y > s->sel_cur_y ? s->sel_start_y : s->sel_cur_y;
        Rectangle(buf, rx0, ry0, rx1, ry1);
        DeleteObject(hRPen);
    }

    SelectClipRgn(buf, NULL);
    DeleteObject(hClip);
}

static void Uve_PaintSidebar(HDC buf, UVEditorState* s) {
    RECT side; Uve_SideRect(s, &side);
    COLORREF sbBg = T.isDark ? RGB(30,30,36) : RGB(240,240,244);
    HBRUSH hSbBg = CreateSolidBrush(sbBg);
    FillRect(buf, &side, hSbBg);
    DeleteObject(hSbBg);

    /* Separator line */
    HPEN hSep = CreatePen(PS_SOLID, 1, T.isDark ? RGB(55,55,65) : RGB(195,195,205));
    HPEN hOld = (HPEN)SelectObject(buf, hSep);
    MoveToEx(buf, side.left, side.top, NULL);
    LineTo(buf, side.left, side.bottom);
    SelectObject(buf, hOld);
    DeleteObject(hSep);

    /* Section header */
    SetBkMode(buf, TRANSPARENT);
    SelectObject(buf, s->hFontBold);
    SetTextColor(buf, T.isDark ? RGB(180,180,190) : RGB(50,50,60));
    RECT rh = { side.left + UVE_MAT_PAD, side.top + 4,
                side.right - UVE_MAT_PAD, side.top + 20 };
    DrawTextA(buf, "Materials (TPage)", -1, &rh, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);

    /* Clip to sidebar content area */
    RECT content = { side.left+1, side.top + 24, side.right, side.bottom };
    HRGN hClip = CreateRectRgn(content.left, content.top, content.right, content.bottom);
    SelectClipRgn(buf, hClip);

    int base_y = content.top - s->sidebar_scroll;
    SelectObject(buf, s->hFont);

    /* Material rows */
    for (int mi = 0; mi < s->mat_count; mi++) {
        UVMatGroup& g = s->mats[mi];
        int ry = base_y + mi * UVE_MAT_ROW_H;
        if (ry + UVE_MAT_ROW_H < content.top || ry > content.bottom)
            continue;

        int lx = side.left + UVE_MAT_PAD;
        bool is_solo = (s->solo_mat == mi);
        bool eff_vis = Uve_MatVisible(s, mi);

        /* Hover highlight */
        if (s->hot_zone == UVE_ZONE_MAT && s->hot_mat_idx == mi) {
            RECT rHov = { side.left+1, ry, side.right, ry + UVE_MAT_ROW_H };
            HBRUSH hHov = CreateSolidBrush(T.isDark ? RGB(45,45,55) : RGB(225,225,235));
            FillRect(buf, &rHov, hHov);
            DeleteObject(hHov);
        }
        /* Solo highlight */
        if (is_solo) {
            RECT rSol = { side.left+1, ry, side.right, ry + UVE_MAT_ROW_H };
            HBRUSH hSol = CreateSolidBrush(T.isDark ? RGB(40,50,65) : RGB(215,225,245));
            FillRect(buf, &rSol, hSol);
            DeleteObject(hSol);
        }

        /* Checkbox */
        int ckx = lx, cky = ry + (UVE_MAT_ROW_H - UVE_CHECK_SZ) / 2;
        RECT rcCk = { ckx, cky, ckx + UVE_CHECK_SZ, cky + UVE_CHECK_SZ };
        COLORREF ckCol = (!g.in_range) ? (T.isDark ? RGB(50,40,40) : RGB(210,200,200)) :
                         (g.visible ? g.color : (T.isDark ? RGB(50,50,58) : RGB(200,200,208)));
        HBRUSH hCkBg = CreateSolidBrush(ckCol);
        FillRect(buf, &rcCk, hCkBg);
        DeleteObject(hCkBg);
        /* Checkbox border */
        HPEN hCkPen = CreatePen(PS_SOLID, 1, T.isDark ? RGB(90,90,100) : RGB(160,160,170));
        SelectObject(buf, hCkPen);
        SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
        Rectangle(buf, ckx, cky, ckx + UVE_CHECK_SZ, cky + UVE_CHECK_SZ);
        DeleteObject(hCkPen);
        /* Checkmark */
        if (g.visible) {
            HPEN hChk = CreatePen(PS_SOLID, 2,
                T.isDark ? RGB(20,20,24) : RGB(255,255,255));
            SelectObject(buf, hChk);
            MoveToEx(buf, ckx+3, cky+7, NULL);
            LineTo(buf, ckx+6, cky+10);
            LineTo(buf, ckx+11, cky+3);
            DeleteObject(hChk);
        }

        /* Label: "0x0040 (124)" or "0x0040 (124) [empty]" */
        char label[80];
        if (!g.in_range)
            _snprintf(label, 79, "0x%04X  (%d) empty", g.tpage, g.count);
        else
            _snprintf(label, 79, "0x%04X  (%d)", g.tpage, g.count);
        COLORREF lblCol;
        if (!g.in_range)
            lblCol = T.isDark ? RGB(80,65,65) : RGB(170,155,155);
        else if (!eff_vis)
            lblCol = T.isDark ? RGB(100,100,110) : RGB(150,150,160);
        else
            lblCol = T.isDark ? RGB(200,200,210) : RGB(40,40,50);
        SetTextColor(buf, lblCol);
        RECT rl = { lx + UVE_CHECK_SZ + 6, ry + 2,
                    side.right - UVE_MAT_PAD - 18, ry + UVE_MAT_ROW_H };
        DrawTextA(buf, label, -1, &rl, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX|DT_VCENTER);

        /* Solo icon ("S") — right side of row */
        int solo_x = side.right - UVE_MAT_PAD - 14;
        int solo_y = ry + (UVE_MAT_ROW_H - 14) / 2;
        if (g.in_range) {
            bool solo_hot = (s->hot_zone == UVE_ZONE_MAT_SOLO && s->hot_mat_idx == mi);
            COLORREF sbg = is_solo ? (T.isDark ? RGB(70,110,170) : RGB(140,175,225)) :
                           solo_hot ? (T.isDark ? RGB(55,60,72) : RGB(210,215,228)) :
                                      (T.isDark ? RGB(38,38,46) : RGB(232,232,240));
            HBRUSH hSB = CreateSolidBrush(sbg);
            RECT rS = { solo_x, solo_y, solo_x + 14, solo_y + 14 };
            FillRect(buf, &rS, hSB); DeleteObject(hSB);
            HPEN hSP = CreatePen(PS_SOLID, 1, T.isDark ? RGB(70,70,82) : RGB(180,180,192));
            SelectObject(buf, hSP);
            SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
            Rectangle(buf, rS.left, rS.top, rS.right, rS.bottom);
            DeleteObject(hSP);
            SetTextColor(buf, is_solo ? RGB(255,255,255) :
                (T.isDark ? RGB(130,130,145) : RGB(100,100,115)));
            SelectObject(buf, s->hFontSmall);
            DrawTextA(buf, "S", -1, &rS, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            SelectObject(buf, s->hFont);
        }
    }

    /* Buttons: [All] [None] below last material */
    int btn_y = base_y + s->mat_count * UVE_MAT_ROW_H + 10;
    if (btn_y < content.bottom) {
        int bx = side.left + UVE_MAT_PAD;
        /* "All" button */
        RECT bAll = { bx, btn_y, bx + 40, btn_y + 20 };
        COLORREF abg = (s->hot_zone == UVE_ZONE_BTN_ALL) ?
            (T.isDark ? RGB(55,80,120) : RGB(200,215,240)) :
            (T.isDark ? RGB(42,42,52) : RGB(228,228,236));
        HBRUSH hBA = CreateSolidBrush(abg);
        FillRect(buf, &bAll, hBA); DeleteObject(hBA);
        HPEN hBP = CreatePen(PS_SOLID, 1, T.isDark ? RGB(70,70,80) : RGB(180,180,190));
        SelectObject(buf, hBP);
        SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
        Rectangle(buf, bAll.left, bAll.top, bAll.right, bAll.bottom);
        DeleteObject(hBP);
        SetTextColor(buf, T.isDark ? RGB(170,180,200) : RGB(50,60,80));
        SelectObject(buf, s->hFontSmall);
        DrawTextA(buf, "All", -1, &bAll, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);

        /* "None" button */
        RECT bNon = { bx + 48, btn_y, bx + 92, btn_y + 20 };
        COLORREF nbg = (s->hot_zone == UVE_ZONE_BTN_NON) ?
            (T.isDark ? RGB(55,80,120) : RGB(200,215,240)) :
            (T.isDark ? RGB(42,42,52) : RGB(228,228,236));
        HBRUSH hBN = CreateSolidBrush(nbg);
        FillRect(buf, &bNon, hBN); DeleteObject(hBN);
        hBP = CreatePen(PS_SOLID, 1, T.isDark ? RGB(70,70,80) : RGB(180,180,190));
        SelectObject(buf, hBP);
        Rectangle(buf, bNon.left, bNon.top, bNon.right, bNon.bottom);
        DeleteObject(hBP);
        DrawTextA(buf, "None", -1, &bNon, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    }

    /* Summary below buttons */
    int sum_y = btn_y + 28;
    if (sum_y < content.bottom - 14) {
        char sum[160];
        if (s->solo_mat >= 0)
            _snprintf(sum, 159, "%d / %d tris shown\n%d materials  SOLO: #%d",
                      s->total_visible_tris, s->tri_count, s->mat_count, s->solo_mat + 1);
        else
            _snprintf(sum, 159, "%d / %d tris shown\n%d materials",
                      s->total_visible_tris, s->tri_count, s->mat_count);
        SetTextColor(buf, T.isDark ? RGB(110,110,125) : RGB(130,130,145));
        SelectObject(buf, s->hFontSmall);
        RECT rs = { side.left + UVE_MAT_PAD, sum_y,
                    side.right - UVE_MAT_PAD, sum_y + 30 };
        DrawTextA(buf, sum, -1, &rs, DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
    }

    /* ── Display Toggles ──────────────────────────────────────── */
    int tog_y = sum_y + 38;
    if (tog_y < content.bottom - 80) {
        int lx = side.left + UVE_MAT_PAD;
        /* Separator */
        HPEN hTSep = CreatePen(PS_SOLID, 1, T.isDark ? RGB(50,50,60) : RGB(205,205,215));
        SelectObject(buf, hTSep);
        MoveToEx(buf, lx, tog_y, NULL);
        LineTo(buf, side.right - UVE_MAT_PAD, tog_y);
        DeleteObject(hTSep);
        tog_y += 8;

        SelectObject(buf, s->hFontBold);
        SetTextColor(buf, T.isDark ? RGB(160,160,175) : RGB(60,60,75));
        RECT rth = { lx, tog_y, side.right - UVE_MAT_PAD, tog_y + 16 };
        DrawTextA(buf, "Display", -1, &rth, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
        tog_y += 20;

        /* Toggle rows: [indicator] [key] label */
        SelectObject(buf, s->hFontSmall);
        struct { const char* key; const char* label; bool on; } toggles[] = {
            { "T", "Texture",      s->show_texture },
            { "W", "UV Wireframe", s->show_uv_wire },
            { "C", "Checkerboard", s->show_checker },
        };
        for (int ti = 0; ti < 3; ti++) {
            int ty = tog_y + ti * 18;
            if (ty > content.bottom - 14) break;
            /* On/off dot */
            COLORREF dotC = toggles[ti].on ?
                (T.isDark ? RGB(80,210,120) : RGB(40,170,70)) :
                (T.isDark ? RGB(70,70,80) : RGB(180,180,190));
            HBRUSH hDot = CreateSolidBrush(dotC);
            RECT rd = { lx, ty + 4, lx + 6, ty + 10 };
            FillRect(buf, &rd, hDot); DeleteObject(hDot);
            /* Key badge */
            COLORREF kbg = T.isDark ? RGB(50,55,65) : RGB(215,218,225);
            HBRUSH hKb = CreateSolidBrush(kbg);
            RECT rk = { lx + 10, ty, lx + 24, ty + 15 };
            FillRect(buf, &rk, hKb); DeleteObject(hKb);
            HPEN hKp = CreatePen(PS_SOLID, 1, T.isDark ? RGB(75,75,88) : RGB(185,185,198));
            SelectObject(buf, hKp);
            SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
            Rectangle(buf, rk.left, rk.top, rk.right, rk.bottom);
            DeleteObject(hKp);
            SetTextColor(buf, T.isDark ? RGB(180,185,200) : RGB(60,60,75));
            DrawTextA(buf, toggles[ti].key, -1, &rk,
                      DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            /* Label */
            SetTextColor(buf, toggles[ti].on ?
                (T.isDark ? RGB(190,190,200) : RGB(40,40,55)) :
                (T.isDark ? RGB(95,95,108) : RGB(150,150,162)));
            RECT rlb = { lx + 28, ty, side.right - UVE_MAT_PAD, ty + 15 };
            DrawTextA(buf, toggles[ti].label, -1, &rlb,
                      DT_LEFT|DT_SINGLELINE|DT_NOPREFIX|DT_VCENTER);
        }
    }

    /* ── Key Legend ────────────────────────────────────────────── */
    int leg_y = tog_y + 62;
    if (leg_y < content.bottom - 60) {
        int lx = side.left + UVE_MAT_PAD;
        /* Separator */
        HPEN hLSep = CreatePen(PS_SOLID, 1, T.isDark ? RGB(50,50,60) : RGB(205,205,215));
        SelectObject(buf, hLSep);
        MoveToEx(buf, lx, leg_y, NULL);
        LineTo(buf, side.right - UVE_MAT_PAD, leg_y);
        DeleteObject(hLSep);
        leg_y += 8;

        SelectObject(buf, s->hFontBold);
        SetTextColor(buf, T.isDark ? RGB(160,160,175) : RGB(60,60,75));
        RECT rlh = { lx, leg_y, side.right - UVE_MAT_PAD, leg_y + 16 };
        DrawTextA(buf, "Controls", -1, &rlh, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
        leg_y += 20;

        SelectObject(buf, s->hFontSmall);
        COLORREF keyDim = T.isDark ? RGB(105,105,120) : RGB(130,130,148);
        COLORREF keyBrt = T.isDark ? RGB(155,155,170) : RGB(70,70,85);
        struct { const char* key; const char* desc; } keys[] = {
            { "F",     "Fit view" },
            { "A / N", "All / None" },
            { "1-9",   "Toggle material" },
            { "S",     "Solo hovered mat" },
            { "M",     "Maximize/Restore" },
            { "Scroll","Zoom" },
            { "Drag",  "Pan" },
            { "Esc",   "Close" },
        };
        for (int ki = 0; ki < 8; ki++) {
            int ky = leg_y + ki * 15;
            if (ky > content.bottom - 10) break;
            SetTextColor(buf, keyBrt);
            RECT rkl = { lx, ky, lx + 42, ky + 14 };
            DrawTextA(buf, keys[ki].key, -1, &rkl,
                      DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
            SetTextColor(buf, keyDim);
            RECT rdl = { lx + 44, ky, side.right - UVE_MAT_PAD, ky + 14 };
            DrawTextA(buf, keys[ki].desc, -1, &rdl,
                      DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
        }
    }

    SelectClipRgn(buf, NULL);
    DeleteObject(hClip);
}

static void Uve_PaintTitle(HDC buf, UVEditorState* s) {
    RECT rt = { 0, 0, s->win_w, UVE_TITLE_H };
    COLORREF tbg = T.isDark ? RGB(26,26,32) : RGB(238,238,244);
    HBRUSH hTb = CreateSolidBrush(tbg);
    FillRect(buf, &rt, hTb); DeleteObject(hTb);

    /* Separator */
    HPEN hSep = CreatePen(PS_SOLID, 1, T.isDark ? RGB(55,55,65) : RGB(200,200,210));
    HPEN hOld = (HPEN)SelectObject(buf, hSep);
    MoveToEx(buf, 0, UVE_TITLE_H-1, NULL);
    LineTo(buf, s->win_w, UVE_TITLE_H-1);
    SelectObject(buf, hOld); DeleteObject(hSep);

    /* Title text (with solo indicator) */
    SetBkMode(buf, TRANSPARENT);
    SelectObject(buf, s->hFontBold);
    SetTextColor(buf, T.isDark ? RGB(200,200,210) : RGB(40,40,50));
    RECT rl = { 10, 0, s->win_w - UVE_CLOSE_SZ*2 - 16, UVE_TITLE_H };
    char title[80] = "UV Editor";
    if (s->solo_mat >= 0)
        _snprintf(title, 79, "UV Editor  [Solo: 0x%04X]", s->mats[s->solo_mat].tpage);
    DrawTextA(buf, title, -1, &rl, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);

    /* Close button */
    int cx = s->win_w - UVE_CLOSE_SZ - 4, cy = 3;
    if (s->hot_zone == UVE_ZONE_CLOSE) {
        RECT rcb = { cx, cy, cx + UVE_CLOSE_SZ, cy + UVE_CLOSE_SZ };
        HBRUSH hH = CreateSolidBrush(T.isDark ? RGB(180,50,50) : RGB(230,70,70));
        FillRect(buf, &rcb, hH); DeleteObject(hH);
    }
    COLORREF xc = (s->hot_zone == UVE_ZONE_CLOSE) ? RGB(255,255,255) :
        (T.isDark ? RGB(140,140,148) : RGB(100,100,110));
    HPEN hX = CreatePen(PS_SOLID, 2, xc);
    SelectObject(buf, hX);
    int xm = cx + UVE_CLOSE_SZ/2, ym = cy + UVE_CLOSE_SZ/2, xs = 5;
    MoveToEx(buf, xm-xs, ym-xs, NULL); LineTo(buf, xm+xs+1, ym+xs+1);
    MoveToEx(buf, xm+xs, ym-xs, NULL); LineTo(buf, xm-xs-1, ym+xs+1);
    DeleteObject(hX);

    /* Maximize/Restore button (left of close) */
    int mx2 = cx - UVE_CLOSE_SZ - 2;
    if (s->hot_zone == UVE_ZONE_MAXIMIZE) {
        RECT rmb = { mx2, cy, mx2 + UVE_CLOSE_SZ, cy + UVE_CLOSE_SZ };
        HBRUSH hMH = CreateSolidBrush(T.isDark ? RGB(55,55,70) : RGB(210,210,225));
        FillRect(buf, &rmb, hMH); DeleteObject(hMH);
    }
    COLORREF mc = (s->hot_zone == UVE_ZONE_MAXIMIZE) ?
        (T.isDark ? RGB(220,220,230) : RGB(50,50,60)) :
        (T.isDark ? RGB(140,140,148) : RGB(100,100,110));
    HPEN hMP = CreatePen(PS_SOLID, 1, mc);
    SelectObject(buf, hMP);
    if (s->maximized) {
        /* Restore icon: two overlapping squares */
        int bx = mx2 + 5, by = cy + 4;
        /* Back square (offset) */
        MoveToEx(buf, bx+2, by, NULL);
        LineTo(buf, bx+14, by); LineTo(buf, bx+14, by+10);
        /* Front square */
        SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
        Rectangle(buf, bx, by+3, bx+12, by+14);
    } else {
        /* Maximize icon: single rectangle */
        SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
        Rectangle(buf, mx2+4, cy+4, mx2+UVE_CLOSE_SZ-4, cy+UVE_CLOSE_SZ-4);
    }
    DeleteObject(hMP);
}

static void Uve_PaintStatus(HDC buf, UVEditorState* s) {
    RECT rs; Uve_StatusRect(s, &rs);
    COLORREF sbg = T.isDark ? RGB(26,26,32) : RGB(238,238,244);
    HBRUSH hSb = CreateSolidBrush(sbg);
    FillRect(buf, &rs, hSb); DeleteObject(hSb);

    /* Top separator */
    HPEN hSep = CreatePen(PS_SOLID, 1, T.isDark ? RGB(55,55,65) : RGB(200,200,210));
    HPEN hOld = (HPEN)SelectObject(buf, hSep);
    MoveToEx(buf, 0, rs.top, NULL); LineTo(buf, s->win_w, rs.top);
    SelectObject(buf, hOld); DeleteObject(hSep);

    SetBkMode(buf, TRANSPARENT);
    SelectObject(buf, s->hFontSmall);
    SetTextColor(buf, T.isDark ? RGB(130,130,145) : RGB(100,100,115));

    char status[256];
    char pal_info[64] = "";
    if (s->num_pal_rows > 1)
        _snprintf(pal_info, 63, "  PAL: %d/%d [P]", s->cur_pal_row + 1, s->num_pal_rows);
    int sc = Uve_SelCount(s);
    if (s->cursor_in_vp) {
        _snprintf(status, 255,
            "  UV: (%.1f, %.1f)    Zoom: %.0f%%    Sel: %d%s    Tex: %s  Wire: %s  Checker: %s",
            s->cursor_u, s->cursor_v, s->zoom * 100.0, sc, pal_info,
            s->show_texture  ? "ON" : "off",
            s->show_uv_wire  ? "ON" : "off",
            s->show_checker  ? "ON" : "off");
    } else {
        _snprintf(status, 255,
            "  Zoom: %.0f%%    Sel: %d%s    Tex: %s  Wire: %s  Checker: %s",
            s->zoom * 100.0, sc, pal_info,
            s->show_texture  ? "ON" : "off",
            s->show_uv_wire  ? "ON" : "off",
            s->show_checker  ? "ON" : "off");
    }
    RECT rl = { rs.left + 4, rs.top + 2, rs.right - 4, rs.bottom };
    DrawTextA(buf, status, -1, &rl, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX|DT_VCENTER);
}

/* ─── Toggle maximize / restore ───────────────────────────────── */
static void Uve_ToggleMaximize(UVEditorState* s) {
    if (s->maximized) {
        /* Restore */
        SetWindowPos(s->hwnd, NULL, s->restore_rect.left, s->restore_rect.top,
                     s->restore_rect.right - s->restore_rect.left,
                     s->restore_rect.bottom - s->restore_rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        s->maximized = false;
    } else {
        /* Save current rect, then maximize to work area */
        GetWindowRect(s->hwnd, &s->restore_rect);
        HMONITOR hMon = MonitorFromWindow(s->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi; mi.cbSize = sizeof(mi);
        GetMonitorInfoA(hMon, &mi);
        RECT wa = mi.rcWork;
        SetWindowPos(s->hwnd, NULL, wa.left, wa.top,
                     wa.right - wa.left, wa.bottom - wa.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        s->maximized = true;
    }
    RECT cr; GetClientRect(s->hwnd, &cr);
    s->win_w = cr.right; s->win_h = cr.bottom;
    Uve_FitView(s);
    InvalidateRect(s->hwnd, NULL, FALSE);
}

static void Uve_Paint(HWND hwnd, UVEditorState* s) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    /* Always use GetClientRect for actual dimensions */
    RECT cr; GetClientRect(hwnd, &cr);
    s->win_w = cr.right; s->win_h = cr.bottom;
    int W = s->win_w, H = s->win_h;

    HDC buf = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, W, H);
    HBITMAP hOld = (HBITMAP)SelectObject(buf, hBmp);

    Uve_PaintTitle(buf, s);
    Uve_PaintViewport(buf, s);
    Uve_PaintSidebar(buf, s);
    Uve_PaintStatus(buf, s);

    /* Outer border */
    HPEN hBdr = CreatePen(PS_SOLID, 1, T.isDark ? RGB(60,60,70) : RGB(180,180,195));
    SelectObject(buf, hBdr);
    SelectObject(buf, (HBRUSH)GetStockObject(NULL_BRUSH));
    Rectangle(buf, 0, 0, W, H);
    DeleteObject(hBdr);

    BitBlt(hdc, 0, 0, W, H, buf, 0, 0, SRCCOPY);
    SelectObject(buf, hOld);
    DeleteObject(hBmp);
    DeleteDC(buf);
    EndPaint(hwnd, &ps);
}

/* ═══ WINDOW PROC ══════════════════════════════════════════════════ */
static LRESULT CALLBACK UVEditorProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UVEditorState* s = Uve_Get(hwnd);
    switch (msg) {

    case WM_PAINT:
        if (s) Uve_Paint(hwnd, s);
        else { PAINTSTRUCT ps; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SETCURSOR:
        /* We handle cursor shape in WM_MOUSEMOVE — prevent default reset */
        if (LOWORD(lp) == HTCLIENT) return TRUE;
        break;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = UVE_MIN_W;
        mmi->ptMinTrackSize.y = UVE_MIN_H;
    } return 0;

    case WM_SIZE:
        if (s) {
            RECT cr; GetClientRect(hwnd, &cr);
            s->win_w = cr.right; s->win_h = cr.bottom;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_MOUSEMOVE: {
        if (!s) return 0;
        int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);

        /* Pan drag (RMB/MMB) */
        if (s->dragging) {
            s->pan_x = s->drag_pan_x + (mx - s->drag_sx);
            s->pan_y = s->drag_pan_y + (my - s->drag_sy);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        /* Selection rect drag (LMB) */
        if (s->sel_dragging) {
            s->sel_cur_x = mx; s->sel_cur_y = my;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        /* Edge resize cursor */
        int W = s->win_w, H = s->win_h;
        int bdr = 5;
        bool el = mx < bdr, er = mx >= W-bdr, et = my < bdr, eb = my >= H-bdr;
        if ((et && el) || (eb && er)) SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
        else if ((et && er) || (eb && el)) SetCursor(LoadCursor(NULL, IDC_SIZENESW));
        else if (el || er) SetCursor(LoadCursor(NULL, IDC_SIZEWE));
        else if (et || eb) SetCursor(LoadCursor(NULL, IDC_SIZENS));
        else {
            /* Normal hot tracking */
            int mat = -1;
            int zone = Uve_HitTest(s, mx, my, &mat);
            SetCursor(LoadCursor(NULL, (zone == UVE_ZONE_CLOSE || zone == UVE_ZONE_MAT
                    || zone == UVE_ZONE_BTN_ALL || zone == UVE_ZONE_BTN_NON
                    || zone == UVE_ZONE_MAXIMIZE || zone == UVE_ZONE_MAT_SOLO)
                    ? IDC_HAND : (zone == UVE_ZONE_VP ? IDC_CROSS : IDC_ARROW)));
        }

        /* Cursor UV readout */
        RECT vp; Uve_VpRect(s, &vp);
        if (mx >= vp.left && mx < vp.right && my >= vp.top && my < vp.bottom) {
            s->cursor_u = (float)((mx - vp.left - s->pan_x) / s->zoom);
            s->cursor_v = (float)((my - vp.top  - s->pan_y) / s->zoom);
            s->cursor_in_vp = true;
        } else {
            s->cursor_in_vp = false;
        }

        /* Hot tracking */
        int mat = -1;
        int zone = Uve_HitTest(s, mx, my, &mat);
        if (zone != s->hot_zone || mat != s->hot_mat_idx) {
            s->hot_zone = zone; s->hot_mat_idx = mat;
            InvalidateRect(hwnd, NULL, FALSE);
        }

        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme); tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd; tme.dwHoverTime = 0;
        TrackMouseEvent(&tme);
    } return 0;

    case WM_MOUSELEAVE:
        if (s) {
            s->hot_zone = UVE_ZONE_NONE; s->hot_mat_idx = -1;
            s->cursor_in_vp = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        if (!s) return 0;
        int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
        int W = s->win_w, H = s->win_h;
        int bdr = 5;
        bool el = mx < bdr, er = mx >= W-bdr, et = my < bdr, eb = my >= H-bdr;

        /* Edge resize — dispatch to system handler */
        WPARAM nc = 0;
        if (et && el) nc = HTTOPLEFT;    else if (et && er) nc = HTTOPRIGHT;
        else if (eb && el) nc = HTBOTTOMLEFT; else if (eb && er) nc = HTBOTTOMRIGHT;
        else if (el) nc = HTLEFT;   else if (er) nc = HTRIGHT;
        else if (et) nc = HTTOP;    else if (eb) nc = HTBOTTOM;
        if (nc) {
            ReleaseCapture();
            SendMessageA(hwnd, WM_NCLBUTTONDOWN, nc, 0);
            return 0;
        }

        /* Title bar drag (but not close/maximize button) */
        if (my < UVE_TITLE_H) {
            int cx = W - UVE_CLOSE_SZ - 4;
            if (mx >= cx && mx < cx + UVE_CLOSE_SZ && my >= 3 && my < 3 + UVE_CLOSE_SZ) {
                DestroyWindow(hwnd); return 0;
            }
            /* Maximize button (left of close) */
            int mx2 = cx - UVE_CLOSE_SZ - 2;
            if (mx >= mx2 && mx < mx2 + UVE_CLOSE_SZ && my >= 3 && my < 3 + UVE_CLOSE_SZ) {
                Uve_ToggleMaximize(s); return 0;
            }
            if (!s->maximized) {
                ReleaseCapture();
                SendMessageA(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;
        }

        /* Client area clicks */
        int mat = -1;
        int zone = Uve_HitTest(s, mx, my, &mat);
        if (zone == UVE_ZONE_CLOSE) { DestroyWindow(hwnd); return 0; }
        if (zone == UVE_ZONE_MAT && mat >= 0 && mat < s->mat_count) {
            s->mats[mat].visible = !s->mats[mat].visible;
            Uve_CountVisible(s);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (zone == UVE_ZONE_BTN_ALL) {
            for (int i = 0; i < s->mat_count; i++) s->mats[i].visible = true;
            s->solo_mat = -1;
            Uve_CountVisible(s);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (zone == UVE_ZONE_BTN_NON) {
            for (int i = 0; i < s->mat_count; i++) s->mats[i].visible = false;
            s->solo_mat = -1;
            Uve_CountVisible(s);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (zone == UVE_ZONE_MAT_SOLO && mat >= 0 && mat < s->mat_count) {
            /* Toggle solo: click same = unsolo, different = solo that one */
            if (s->solo_mat == mat)
                s->solo_mat = -1;
            else
                s->solo_mat = mat;
            Uve_CountVisible(s);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (zone == UVE_ZONE_MAXIMIZE) {
            Uve_ToggleMaximize(s);
            return 0;
        }
        /* Viewport: LMB = selection (click or rect drag) */
        if (zone == UVE_ZONE_VP) {
            /* Determine mode from modifiers */
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt  = (GetKeyState(VK_MENU)    & 0x8000) != 0;
            if (ctrl && alt)      s->sel_mode = 2; /* deselect */
            else if (ctrl)        s->sel_mode = 1; /* add */
            else                  s->sel_mode = 0; /* replace */

            SetCapture(hwnd);
            s->sel_dragging = true;
            s->sel_start_x = mx; s->sel_start_y = my;
            s->sel_cur_x = mx; s->sel_cur_y = my;
        }
    } return 0;

    case WM_LBUTTONDBLCLK: {
        if (!s) return 0;
        int my2 = (short)HIWORD(lp);
        if (my2 < UVE_TITLE_H) {
            Uve_ToggleMaximize(s);
        }
    } return 0;

    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        if (!s) return 0;
        int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
        int mat = -1;
        int zone = Uve_HitTest(s, mx, my, &mat);
        if (zone == UVE_ZONE_VP) {
            SetCapture(hwnd);
            s->dragging = true;
            s->drag_sx = mx; s->drag_sy = my;
            s->drag_pan_x = s->pan_x; s->drag_pan_y = s->pan_y;
        }
    } return 0;

    case WM_LBUTTONUP: {
        if (!s) { ReleaseCapture(); return 0; }
        if (s->sel_dragging) {
            s->sel_dragging = false;
            ReleaseCapture();

            if (!s->sel_mask) {
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
            int dx = mx - s->sel_start_x, dy = my - s->sel_start_y;
            bool is_click = (dx*dx + dy*dy) < 16; /* < 4px movement = click */

            /* Clear existing selection unless additive/deselect mode */
            if (s->sel_mode == 0) {
                memset(s->sel_mask, 0, s->sel_mask_count * sizeof(bool));
            }

            RECT vp; Uve_VpRect(s, &vp);

            if (is_click) {
                /* Point select: find tri under cursor */
                float cu = (float)((s->sel_start_x - vp.left - s->pan_x) / s->zoom);
                float cv = (float)((s->sel_start_y - vp.top  - s->pan_y) / s->zoom);
                int best = -1;
                for (int mi = 0; mi < s->mat_count; mi++) {
                    UVMatGroup& g = s->mats[mi];
                    if (!Uve_MatVisible(s, mi)) continue;
                    for (int ti = g.first_tri; ti < g.first_tri + g.count; ti++) {
                        float u0,v0,u1,v1,u2,v2;
                        Uve_TriUV(s, s->tris[ti], 0, &u0, &v0);
                        Uve_TriUV(s, s->tris[ti], 1, &u1, &v1);
                        Uve_TriUV(s, s->tris[ti], 2, &u2, &v2);
                        if (Uve_PtInTri(cu, cv, u0, v0, u1, v1, u2, v2))
                            best = ti; /* last match = topmost drawn */
                    }
                }
                if (best >= 0) {
                    int orig = s->sort_map ? s->sort_map[best] : best;
                    if (orig >= 0 && orig < s->sel_mask_count) {
                        if (s->sel_mode == 2)
                            s->sel_mask[orig] = false;  /* deselect */
                        else
                            s->sel_mask[orig] = true;   /* select/add */
                    }
                }
            } else {
                /* Rectangle select */
                int rx0 = s->sel_start_x < mx ? s->sel_start_x : mx;
                int ry0 = s->sel_start_y < my ? s->sel_start_y : my;
                int rx1 = s->sel_start_x > mx ? s->sel_start_x : mx;
                int ry1 = s->sel_start_y > my ? s->sel_start_y : my;
                for (int mi = 0; mi < s->mat_count; mi++) {
                    UVMatGroup& g = s->mats[mi];
                    if (!Uve_MatVisible(s, mi)) continue;
                    for (int ti = g.first_tri; ti < g.first_tri + g.count; ti++) {
                        if (Uve_TriInRect(s, s->tris[ti], rx0, ry0, rx1, ry1)) {
                            int orig = s->sort_map ? s->sort_map[ti] : ti;
                            if (orig >= 0 && orig < s->sel_mask_count) {
                                if (s->sel_mode == 2)
                                    s->sel_mask[orig] = false;
                                else
                                    s->sel_mask[orig] = true;
                            }
                        }
                    }
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
            /* Live-sync: repaint the 3D panel so selection overlay updates */
            if (s->sel_parent) InvalidateRect(s->sel_parent, NULL, FALSE);
        }
    } return 0;

    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        if (s && s->dragging) { s->dragging = false; }
        ReleaseCapture();
        return 0;

    case WM_MOUSEWHEEL: {
        if (!s) return 0;
        int delta = (short)HIWORD(wp);
        POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
        ScreenToClient(hwnd, &pt);
        double factor = (delta > 0) ? 1.15 : (1.0 / 1.15);

        /* Sidebar scroll with Ctrl or when cursor is over sidebar */
        RECT side; Uve_SideRect(s, &side);
        if (pt.x >= side.left && pt.x < side.right) {
            s->sidebar_scroll -= delta / 4;
            if (s->sidebar_scroll < 0) s->sidebar_scroll = 0;
            int max_scroll = s->mat_count * UVE_MAT_ROW_H - (side.bottom - side.top - 24) + 280;
            if (max_scroll < 0) max_scroll = 0;
            if (s->sidebar_scroll > max_scroll) s->sidebar_scroll = max_scroll;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        Uve_ZoomAt(s, pt.x, pt.y, factor);
        InvalidateRect(hwnd, NULL, FALSE);
    } return 0;

    case WM_KEYDOWN:
        if (!s) return 0;
        switch (wp) {
        case VK_ESCAPE:
            /* If selection exists, clear it first; else close */
            if (s->sel_mask && Uve_SelCount(s) > 0) {
                memset(s->sel_mask, 0, s->sel_mask_count * sizeof(bool));
                InvalidateRect(hwnd, NULL, FALSE);
                if (s->sel_parent) InvalidateRect(s->sel_parent, NULL, FALSE);
            } else {
                DestroyWindow(hwnd);
            }
            return 0;
        case 'C': s->show_checker = !s->show_checker;
                  InvalidateRect(hwnd, NULL, FALSE); return 0;
        case 'T': s->show_texture = !s->show_texture;
                  InvalidateRect(hwnd, NULL, FALSE); return 0;
        case 'W': s->show_uv_wire = !s->show_uv_wire;
                  InvalidateRect(hwnd, NULL, FALSE); return 0;
        case 'F': Uve_FitView(s);
                  InvalidateRect(hwnd, NULL, FALSE); return 0;
        case 'P': {
            if (s->num_pal_rows > 1) {
                int next = (GetKeyState(VK_SHIFT) & 0x8000)
                    ? (s->cur_pal_row - 1 + s->num_pal_rows) % s->num_pal_rows
                    : (s->cur_pal_row + 1) % s->num_pal_rows;
                Uve_RebuildTexDib(s, next);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } return 0;
        case 'A': for (int i = 0; i < s->mat_count; i++) s->mats[i].visible = true;
                  s->solo_mat = -1;
                  Uve_CountVisible(s);
                  InvalidateRect(hwnd, NULL, FALSE); return 0;
        case 'N': for (int i = 0; i < s->mat_count; i++) s->mats[i].visible = false;
                  s->solo_mat = -1;
                  Uve_CountVisible(s);
                  InvalidateRect(hwnd, NULL, FALSE); return 0;
        case 'M': Uve_ToggleMaximize(s); return 0;
        case 'S': {
            /* Solo: if hovering a material, solo that; else toggle off */
            if (s->hot_zone == UVE_ZONE_MAT || s->hot_zone == UVE_ZONE_MAT_SOLO) {
                if (s->hot_mat_idx >= 0 && s->hot_mat_idx < s->mat_count) {
                    s->solo_mat = (s->solo_mat == s->hot_mat_idx) ? -1 : s->hot_mat_idx;
                    Uve_CountVisible(s);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            } else if (s->solo_mat >= 0) {
                s->solo_mat = -1;
                Uve_CountVisible(s);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } return 0;
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9': {
            int mi = (int)(wp - '1');
            if (mi >= 0 && mi < s->mat_count) {
                s->mats[mi].visible = !s->mats[mi].visible;
                Uve_CountVisible(s);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } return 0;
        }
        return 0;

    case WM_TIMER:
        return 0;

    case WM_DESTROY:
        if (s) {
            if (s->hTexDib) DeleteObject(s->hTexDib);
            if (s->hFont)     DeleteObject(s->hFont);
            if (s->hFontBold) DeleteObject(s->hFontBold);
            if (s->hFontSmall)DeleteObject(s->hFontSmall);
            free(s->atlas_rgba);
            free(s->tris);
            free(s->sort_map);
            free(s);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ═══ PUBLIC ENTRY POINT ═══════════════════════════════════════════ */

/* Show the UV Editor modal.
   tris/tri_count = mesh face data (copied internally).
   tex_rgba/tw/th = RGBA texture atlas (top-down, may be NULL).
   vram_x = texture VRAM X in halfwords, bpp = bits per pixel.
   sel_mask = external bool array for tri selection (may be NULL).
   sel_mask_count = size of sel_mask array.
   hParent = owner window. */
static void ShowUVEditor(HWND hParent,
                         const MeshTri* tris, int tri_count,
                         const u8* tex_rgba, int tw, int th,
                         u16 vram_x, int bpp, u16 vram_y = 0,
                         bool* sel_mask = 0, int sel_mask_count = 0,
                         const u8* full_atlas = 0, int atlas_w = 0, int atlas_h = 0,
                         int num_pal_rows = 1, int init_pal_row = 0,
                         int crop_x = 0, int crop_w = 0,
                         int num_sub_pals = 1)
{
    if (!tris || tri_count < 1) return;

    /* Register window class once */
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXA wc; memset(&wc, 0, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_DBLCLKS;
        wc.lpfnWndProc   = UVEditorProc;
        wc.hInstance     = g_app.hInst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = "DCUVEditor";
        if (RegisterClassExA(&wc)) registered = TRUE;
    }

    /* Allocate state */
    UVEditorState* s = (UVEditorState*)calloc(1, sizeof(UVEditorState));
    if (!s) return;

    s->show_checker  = true;
    s->show_fill     = false;
    s->show_texture  = true;
    s->show_uv_wire  = true;
    s->hot_zone      = UVE_ZONE_NONE;
    s->hot_mat_idx  = -1;
    s->solo_mat     = -1;
    s->maximized    = false;
    s->win_w = UVE_DEF_W;
    s->win_h = UVE_DEF_H;

    /* Selection state (external pointer, not owned) */
    s->sel_mask = sel_mask;
    s->sel_mask_count = sel_mask_count;
    s->sel_dragging = false;
    s->sel_mode = 0;
    s->sel_parent = hParent;

    /* Copy tris, build index map, then sort both together */
    s->tris = (MeshTri*)malloc(tri_count * sizeof(MeshTri));
    s->sort_map = (int*)malloc(tri_count * sizeof(int));
    if (!s->tris || !s->sort_map) {
        free(s->tris); free(s->sort_map); free(s); return;
    }
    memcpy(s->tris, tris, tri_count * sizeof(MeshTri));
    for (int i = 0; i < tri_count; i++) s->sort_map[i] = i;
    s->tri_count = tri_count;

    /* Sort by tpage, keeping sort_map in sync */
    /* Simple insertion sort (stable, small N) */
    for (int i = 1; i < tri_count; i++) {
        MeshTri tmpT = s->tris[i];
        int tmpM = s->sort_map[i];
        int j = i - 1;
        while (j >= 0 && s->tris[j].tpage > tmpT.tpage) {
            s->tris[j+1] = s->tris[j];
            s->sort_map[j+1] = s->sort_map[j];
            j--;
        }
        s->tris[j+1] = tmpT;
        s->sort_map[j+1] = tmpM;
    }

    Uve_BuildMats(s);
    Uve_CountVisible(s);

    /* Create HBITMAP from RGBA texture (convert RGBA top-down → BGRA DIB) */
    s->tex_w = tw; s->tex_h = th; s->tex_vram_x = vram_x; s->tex_vram_y = vram_y; s->tex_bpp = bpp;
    s->num_pal_rows = (num_pal_rows > 1 && full_atlas && atlas_w > 0 && atlas_h > 0) ? num_pal_rows : 1;
    s->num_sub_pals = num_sub_pals;
    s->cur_pal_row = init_pal_row;
    s->crop_x = crop_x;
    s->crop_w = crop_w;
    s->atlas_rgba = 0;
    s->atlas_w = atlas_w;
    s->atlas_full_h = atlas_h;
    { int total_v = s->num_pal_rows * num_sub_pals;
      s->slice_h = (total_v > 1) ? atlas_h / total_v : atlas_h; }

    if (s->num_pal_rows > 1 && full_atlas) {
        /* Store full atlas for palette cycling */
        size_t asz = (size_t)atlas_w * atlas_h * 4;
        s->atlas_rgba = (u8*)malloc(asz);
        if (s->atlas_rgba) memcpy(s->atlas_rgba, full_atlas, asz);
        /* Build initial DIB from the requested palette row */
        Uve_RebuildTexDib(s, init_pal_row);
    } else if (tex_rgba && tw > 0 && th > 0) {
        /* Single palette fallback — use the pre-cropped texture directly */
        BITMAPINFO bmi; memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth    = tw;
        bmi.bmiHeader.biHeight   = -th; /* top-down */
        bmi.bmiHeader.biPlanes   = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        HDC hdc = GetDC(NULL);
        s->hTexDib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
                                      &s->texDibBits, NULL, 0);
        ReleaseDC(NULL, hdc);
        if (s->hTexDib && s->texDibBits) {
            /* Swizzle RGBA → premultiplied BGRA for AlphaBlend */
            u8* dst = (u8*)s->texDibBits;
            for (int i = 0; i < tw * th; i++) {
                u8 r = tex_rgba[i*4+0], g = tex_rgba[i*4+1];
                u8 b = tex_rgba[i*4+2], a = tex_rgba[i*4+3];
                dst[i*4+0] = (u8)((b * a + 127) / 255); /* B */
                dst[i*4+1] = (u8)((g * a + 127) / 255); /* G */
                dst[i*4+2] = (u8)((r * a + 127) / 255); /* R */
                dst[i*4+3] = a;
            }
            s->has_tex = true;
        }
    }

    /* Compute UV ranges and flag out-of-range materials */
    Uve_ComputeMatRanges(s);
    Uve_CountVisible(s);

    /* Fonts */
    s->hFont = CreateFontA(-13,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
    if (!s->hFont) s->hFont = CreateFontA(-13,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,"Tahoma");
    s->hFontBold = CreateFontA(-13,0,0,0,FW_BOLD,0,0,0,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
    if (!s->hFontBold) s->hFontBold = CreateFontA(-13,0,0,0,FW_BOLD,0,0,0,
        DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,"Tahoma");
    s->hFontSmall = CreateFontA(-11,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
    if (!s->hFontSmall) s->hFontSmall = CreateFontA(-11,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,"Tahoma");

    /* Center on parent */
    RECT rp; GetWindowRect(hParent, &rp);
    int px = rp.left + (rp.right - rp.left - UVE_DEF_W) / 2;
    int py = rp.top  + (rp.bottom - rp.top - UVE_DEF_H) / 2;

    HWND hwnd = CreateWindowExA(
        WS_EX_TOPMOST,
        "DCUVEditor", "UV Editor",
        WS_POPUP | WS_CLIPCHILDREN,
        px, py, UVE_DEF_W, UVE_DEF_H,
        hParent, NULL, g_app.hInst, NULL);
    if (!hwnd) {
        free(s->tris); if (s->hTexDib) DeleteObject(s->hTexDib);
        DeleteObject(s->hFont); DeleteObject(s->hFontBold); DeleteObject(s->hFontSmall);
        free(s); return;
    }

    s->hwnd = hwnd;
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)s);

    /* Initial view: fit texture */
    Uve_FitView(s);

    /* Show immediately (no fade — WS_EX_LAYERED causes DPI coord issues) */
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    /* Modal message loop */
    MSG m;
    while (IsWindow(hwnd) && GetMessageA(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
}

#endif /* DC_UV_EDITOR_H */
