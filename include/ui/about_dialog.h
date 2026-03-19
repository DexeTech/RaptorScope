/*=====================================================================
 *  RaptorScope - Custom About Dialog
 *  Owner-drawn dark-themed popup with banner image
 *  Fully self-contained: everything allocated on ShowAboutDialog(),
 *  freed on WM_DESTROY.  Zero file-scope state.
 *=====================================================================*/
#ifndef DC_ABOUT_DIALOG_H
#define DC_ABOUT_DIALOG_H

#include "ui/app.h"
#include "ui/about_banner.h"
#include <shellapi.h>
#include <math.h>

#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA 0x01
#endif

#define ABT_BANNER_W   VBAN_W
#define ABT_BANNER_H   VBAN_H
#define ABT_TEXT_W     240
#define ABT_PAD        20
#define ABT_DLG_W      (ABT_BANNER_W + ABT_TEXT_W + ABT_PAD * 3)
#define ABT_DLG_H      (ABT_BANNER_H + ABT_PAD * 2)
#define ABT_CLOSE_SZ   28
#define ABT_FADE_STEPS 12
#define ABT_FADE_MS    12

#define ABT_ZONE_NONE   0
#define ABT_ZONE_CLOSE  1
#define ABT_ZONE_URL    2

struct AboutState {
    HBITMAP  hBmpBanner;
    BYTE*    bannerBits;
    HFONT    hFontTitle;
    HFONT    hFontVer;
    HFONT    hFontBody;
    HFONT    hFontURL;
    RECT     rcClose;
    RECT     rcURL;
    int      hotZone;
    int      fadeAlpha;

    /* ADPCM audio playback */
    HWAVEOUT hWaveOut;
    WAVEHDR  waveHdr[2];
    short*   waveBuf[2];
    int      adpcmPos;       /* byte offset into about_audio_adpcm[] */
    int      adpcmPred;      /* IMA ADPCM predictor state */
    int      adpcmIndex;     /* IMA ADPCM step index */
    double   fadeGain;       /* audio fade-in gain (0→1 over 2s) */
    int      totalSamples;   /* samples generated so far */
    bool     audioRunning;
};

/* ── IMA ADPCM decoder + looping waveOut playback ── */
#include "ui/about_audio.h"

#define DRONE_SR     ABOUT_AUDIO_SR
#define DRONE_BUFSZ  4096

static const int ima_step_table[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,
    60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
    337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,
    1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,
    5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,
    16818,18500,20350,22385,24623,27086,29794,32767
};
static const int ima_index_table[16] = {
    -1,-1,-1,-1, 2,4,6,8, -1,-1,-1,-1, 2,4,6,8
};

static short adpcm_decode_nibble(int nibble, int* pred, int* idx) {
    int step = ima_step_table[*idx];
    int diff = step >> 3;
    if (nibble & 4) diff += step;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 8) *pred -= diff; else *pred += diff;
    if (*pred > 32767) *pred = 32767;
    if (*pred < -32768) *pred = -32768;
    *idx += ima_index_table[nibble & 7];
    if (*idx < 0) *idx = 0;
    if (*idx > 88) *idx = 88;
    return (short)*pred;
}

static void drone_fill_buffer(AboutState* s, short* buf, int n) {
    for (int i = 0; i < n; i++) {
        /* Decode one sample from ADPCM stream, loop at end */
        int byte_pos = s->adpcmPos / 2;
        int nibble_sel = s->adpcmPos & 1;  /* 0=low nibble, 1=high nibble */

        if (byte_pos >= ABOUT_AUDIO_BYTES) {
            /* Loop: reset ADPCM state and position */
            s->adpcmPos = 0;
            s->adpcmPred = 0;
            s->adpcmIndex = 0;
            byte_pos = 0;
            nibble_sel = 0;
        }

        int nibble;
        if (nibble_sel == 0)
            nibble = about_audio_adpcm[byte_pos] & 0x0F;
        else
            nibble = (about_audio_adpcm[byte_pos] >> 4) & 0x0F;
        s->adpcmPos++;

        short sample = adpcm_decode_nibble(nibble, &s->adpcmPred, &s->adpcmIndex);

        /* Fade-in over first 2 seconds */
        double fade = 1.0;
        if (s->totalSamples < DRONE_SR * 2) {
            double t = (double)s->totalSamples / (double)(DRONE_SR * 2);
            fade = t * t * (3.0 - 2.0 * t);  /* smoothstep */
        }
        buf[i] = (short)(sample * fade);
        s->totalSamples++;
    }
}

static void CALLBACK drone_wave_proc(HWAVEOUT hwo, UINT uMsg,
                                      DWORD_PTR dwInst, DWORD_PTR dw1, DWORD_PTR) {
    if (uMsg != WOM_DONE) return;
    AboutState* s = (AboutState*)dwInst;
    if (!s || !s->audioRunning) return;
    WAVEHDR* hdr = (WAVEHDR*)dw1;
    int idx = (hdr == &s->waveHdr[0]) ? 0 : 1;
    drone_fill_buffer(s, s->waveBuf[idx], DRONE_BUFSZ);
    waveOutWrite(s->hWaveOut, &s->waveHdr[idx], sizeof(WAVEHDR));
}

static void drone_start(AboutState* s) {
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = DRONE_SR;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = DRONE_SR * 2;

    if (waveOutOpen(&s->hWaveOut, WAVE_MAPPER, &wfx,
                    (DWORD_PTR)drone_wave_proc, (DWORD_PTR)s,
                    CALLBACK_FUNCTION) != MMSYSERR_NOERROR) return;

    s->adpcmPos = 0;
    s->adpcmPred = 0;
    s->adpcmIndex = 0;
    s->totalSamples = 0;
    s->audioRunning = true;

    for (int b = 0; b < 2; b++) {
        s->waveBuf[b] = (short*)calloc(DRONE_BUFSZ, sizeof(short));
        if (!s->waveBuf[b]) return;
        drone_fill_buffer(s, s->waveBuf[b], DRONE_BUFSZ);
        memset(&s->waveHdr[b], 0, sizeof(WAVEHDR));
        s->waveHdr[b].lpData = (LPSTR)s->waveBuf[b];
        s->waveHdr[b].dwBufferLength = DRONE_BUFSZ * sizeof(short);
        waveOutPrepareHeader(s->hWaveOut, &s->waveHdr[b], sizeof(WAVEHDR));
        waveOutWrite(s->hWaveOut, &s->waveHdr[b], sizeof(WAVEHDR));
    }
}

static void drone_stop(AboutState* s) {
    if (!s->hWaveOut) return;
    s->audioRunning = false;
    waveOutReset(s->hWaveOut);
    for (int b = 0; b < 2; b++) {
        if (s->waveBuf[b]) {
            waveOutUnprepareHeader(s->hWaveOut, &s->waveHdr[b], sizeof(WAVEHDR));
            free(s->waveBuf[b]);
            s->waveBuf[b] = 0;
        }
    }
    waveOutClose(s->hWaveOut);
    s->hWaveOut = 0;
}

static inline AboutState* Abt_Get(HWND hwnd) {
    return (AboutState*)(LONG_PTR)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
}

static void Abt_FillRoundRect(HDC hdc, const RECT* rc, int r, COLORREF col)
{
    HBRUSH hbr = CreateSolidBrush(col);
    HPEN   hp  = CreatePen(PS_SOLID, 1, col);
    HBRUSH ob  = (HBRUSH)SelectObject(hdc, hbr);
    HPEN   op  = (HPEN)SelectObject(hdc, hp);
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, r, r);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(hp);
    DeleteObject(hbr);
}

static int Abt_HitTest(AboutState* s, int x, int y)
{
    POINT pt = { x, y };
    if (PtInRect(&s->rcClose, pt)) return ABT_ZONE_CLOSE;
    if (PtInRect(&s->rcURL, pt))   return ABT_ZONE_URL;
    return ABT_ZONE_NONE;
}

static void Abt_Paint(HWND hwnd, AboutState* s)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, W, H);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(mem, hBmp);

    COLORREF bgCol = T.isDark ? RGB(22,22,26) : RGB(245,245,248);
    HBRUSH hBg = CreateSolidBrush(bgCol);
    FillRect(mem, &rc, hBg);
    DeleteObject(hBg);

    HPEN hBorder = CreatePen(PS_SOLID, 1, T.isDark ? RGB(60,60,70) : RGB(195,195,205));
    SelectObject(mem, (HBRUSH)GetStockObject(NULL_BRUSH));
    SelectObject(mem, hBorder);
    RoundRect(mem, 0, 0, W, H, 8, 8);
    DeleteObject(hBorder);

    int bx = ABT_PAD, by = (H - ABT_BANNER_H) / 2;
    if (s->hBmpBanner) {
        HDC hdcBmp = CreateCompatibleDC(mem);
        HBITMAP hOld2 = (HBITMAP)SelectObject(hdcBmp, s->hBmpBanner);
        BLENDFUNCTION bf;
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;
        AlphaBlend(mem, bx, by, ABT_BANNER_W, ABT_BANNER_H,
                   hdcBmp, 0, 0, ABT_BANNER_W, ABT_BANNER_H, bf);
        SelectObject(hdcBmp, hOld2);
        DeleteDC(hdcBmp);
    }

    int tx = bx + ABT_BANNER_W + ABT_PAD;
    int ty = by + 20;
    int tw = ABT_TEXT_W;
    SetBkMode(mem, TRANSPARENT);

    SetTextColor(mem, T.isDark ? RGB(230,230,235) : RGB(30,30,35));
    SelectObject(mem, s->hFontTitle);
    RECT r1 = { tx, ty, tx+tw, ty+30 };
    DrawTextA(mem, "RaptorScope", -1, &r1, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
    ty += 32;

    RECT ra = { tx, ty, tx+60, ty+3 };
    Abt_FillRoundRect(mem, &ra, 2, T.accent);
    ty += 14;

    SetTextColor(mem, T.isDark ? RGB(130,160,220) : RGB(40,80,170));
    SelectObject(mem, s->hFontVer);
    RECT r2 = { tx, ty, tx+tw, ty+20 };
    DrawTextA(mem, "Version 2.3.0  \xB7  March 2026", -1, &r2, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
    ty += 28;

    SetTextColor(mem, T.isDark ? RGB(170,170,175) : RGB(80,80,85));
    SelectObject(mem, s->hFontBody);
    const char* desc =
        "Browse, export, import & preview "
        "Dino Crisis .DAT archive files.\n\n"
        "Textures \xB7 Palettes \xB7 Audio\n"
        "3D Meshes \xB7 EMD Models\n"
        "Animations \xB7 Hex Editor\n\n"
        "Directed by Corey Nguyen\n"
        "Code & RE by Claude Opus 4.6\n"
        "(Anthropic LLM, March 2026)";
    RECT r3 = { tx, ty, tx+tw, ty+130 };
    DrawTextA(mem, desc, -1, &r3, DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
    ty += 132;

    HPEN hSep = CreatePen(PS_SOLID, 1, T.isDark ? RGB(50,50,56) : RGB(210,210,218));
    SelectObject(mem, hSep);
    MoveToEx(mem, tx, ty, NULL);
    LineTo(mem, tx+tw-10, ty);
    DeleteObject(hSep);
    ty += 14;

    COLORREF dimCol = T.isDark ? RGB(110,110,120) : RGB(130,130,140);
    SetTextColor(mem, dimCol);
    SelectObject(mem, s->hFontBody);
    RECT r4 = { tx, ty, tx+tw, ty+18 };
    DrawTextA(mem, "Built with Win32 + OpenGL", -1, &r4, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
    ty += 22;

    COLORREF urlCol = T.isDark ? RGB(80,150,240) : RGB(0,90,200);
    if (s->hotZone == ABT_ZONE_URL)
        urlCol = T.isDark ? RGB(120,180,255) : RGB(0,60,180);
    SetTextColor(mem, urlCol);
    SelectObject(mem, s->hFontURL);
    const char* url = "github.com/coreynguyen";
    RECT r5 = { tx, ty, tx+tw, ty+18 };
    DrawTextA(mem, url, -1, &r5, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);
    s->rcURL = r5;
    s->rcURL.right += 4;
    s->rcURL.bottom += 2;
    DrawTextA(mem, url, -1, &r5, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
    if (s->hotZone == ABT_ZONE_URL) {
        HPEN hUL = CreatePen(PS_SOLID, 1, urlCol);
        SelectObject(mem, hUL);
        MoveToEx(mem, r5.left, r5.bottom, NULL);
        LineTo(mem, r5.right, r5.bottom);
        DeleteObject(hUL);
    }

    int cx = W - ABT_CLOSE_SZ - 8, cy = 8;
    s->rcClose.left = cx;  s->rcClose.top = cy;
    s->rcClose.right = cx + ABT_CLOSE_SZ;  s->rcClose.bottom = cy + ABT_CLOSE_SZ;
    if (s->hotZone == ABT_ZONE_CLOSE) {
        RECT rb = s->rcClose;
        Abt_FillRoundRect(mem, &rb, 4, T.isDark ? RGB(180,50,50) : RGB(230,70,70));
    }
    COLORREF xCol = (s->hotZone == ABT_ZONE_CLOSE)
        ? RGB(255,255,255) : (T.isDark ? RGB(140,140,145) : RGB(120,120,125));
    HPEN hX = CreatePen(PS_SOLID, 2, xCol);
    SelectObject(mem, hX);
    int xm = cx+ABT_CLOSE_SZ/2, ym = cy+ABT_CLOSE_SZ/2, xs = 5;
    MoveToEx(mem, xm-xs, ym-xs, NULL); LineTo(mem, xm+xs+1, ym+xs+1);
    MoveToEx(mem, xm+xs, ym-xs, NULL); LineTo(mem, xm-xs-1, ym+xs+1);
    DeleteObject(hX);

    ty = H - ABT_PAD - 14;
    SetTextColor(mem, dimCol);
    SelectObject(mem, s->hFontBody);
    RECT r6 = { tx, ty, tx+tw, ty+16 };
    DrawTextA(mem, "Released into the public domain (Unlicense)", -1, &r6, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);

    BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    AboutState* s = Abt_Get(hwnd);
    switch (msg) {
    case WM_PAINT:
        if (s) Abt_Paint(hwnd, s);
        else { PAINTSTRUCT ps; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); }
        return 0;
    case WM_MOUSEMOVE:
        if (s) {
            int zone = Abt_HitTest(s, (short)LOWORD(lp), (short)HIWORD(lp));
            if (zone != s->hotZone) {
                s->hotZone = zone;
                InvalidateRect(hwnd, NULL, FALSE);
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                tme.dwHoverTime = 0;
                TrackMouseEvent(&tme);
            }
            SetCursor(LoadCursor(NULL, (zone==ABT_ZONE_URL) ? IDC_HAND : IDC_ARROW));
        }
        return 0;
    case WM_MOUSELEAVE:
        if (s && s->hotZone != ABT_ZONE_NONE) {
            s->hotZone = ABT_ZONE_NONE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (s) {
            int zone = Abt_HitTest(s, (short)LOWORD(lp), (short)HIWORD(lp));
            if (zone == ABT_ZONE_CLOSE)
                DestroyWindow(hwnd);
            else if (zone == ABT_ZONE_URL)
                ShellExecuteA(NULL, "open", "https://github.com/coreynguyen", NULL, NULL, SW_SHOWNORMAL);
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_RETURN || wp == VK_SPACE)
            DestroyWindow(hwnd);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE)
            DestroyWindow(hwnd);
        return 0;
    case WM_TIMER:
        if (wp == 1 && s) {
            if (s->fadeAlpha < 255) {
                s->fadeAlpha += (255 / ABT_FADE_STEPS);
                if (s->fadeAlpha > 255) s->fadeAlpha = 255;
                SetLayeredWindowAttributes(hwnd, 0, (BYTE)s->fadeAlpha, LWA_ALPHA);
                if (s->fadeAlpha >= 255) KillTimer(hwnd, 1);
            }
        }
        return 0;
    case WM_DESTROY:
        if (s) {
            drone_stop(s);
            if (s->hBmpBanner) DeleteObject(s->hBmpBanner);
            if (s->hFontTitle) DeleteObject(s->hFontTitle);
            if (s->hFontVer)   DeleteObject(s->hFontVer);
            if (s->hFontBody)  DeleteObject(s->hFontBody);
            if (s->hFontURL)   DeleteObject(s->hFontURL);
            free(s);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void ShowAboutDialog(HWND hParent)
{
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXA wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_DROPSHADOW;
        wc.lpfnWndProc   = AboutWndProc;
        wc.hInstance      = g_app.hInst;
        wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground  = NULL;
        wc.lpszClassName  = "DCAboutDlg";
        if (RegisterClassExA(&wc)) registered = TRUE;
    }

    AboutState* s = (AboutState*)calloc(1, sizeof(AboutState));
    if (!s) return;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth    = VBAN_W;
    bmi.bmiHeader.biHeight   = VBAN_H;
    bmi.bmiHeader.biPlanes   = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    HDC hdc = GetDC(NULL);
    s->hBmpBanner = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
                                     (void**)&s->bannerBits, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (s->hBmpBanner && s->bannerBits) {
        HRSRC hr = FindResource(g_app.hInst, MAKEINTRESOURCE(IDR_VBAN_DATA), RT_RCDATA);
        if (hr) {
            HGLOBAL hg = LoadResource(g_app.hInst, hr);
            if (hg) {
                const void* p = LockResource(hg);
                DWORD sz = SizeofResource(g_app.hInst, hr);
                if (p && sz == (DWORD)(VBAN_W * VBAN_H * 4))
                    memcpy(s->bannerBits, p, sz);
            }
        }
    }

    s->hFontTitle = CreateFontA(-22,0,0,0,FW_BOLD,0,0,0,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
    if (!s->hFontTitle) s->hFontTitle = CreateFontA(-22,0,0,0,FW_BOLD,0,0,0,
        DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,"Tahoma");
    s->hFontVer = CreateFontA(-13,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
    if (!s->hFontVer) s->hFontVer = CreateFontA(-13,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,"Tahoma");
    s->hFontBody = CreateFontA(-12,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
    if (!s->hFontBody) s->hFontBody = CreateFontA(-12,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,"Tahoma");
    s->hFontURL = CreateFontA(-11,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
    if (!s->hFontURL) s->hFontURL = CreateFontA(-11,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,"Tahoma");

    RECT rp;
    GetWindowRect(hParent, &rp);
    int px = rp.left + (rp.right - rp.left - ABT_DLG_W) / 2;
    int py = rp.top  + (rp.bottom - rp.top - ABT_DLG_H) / 2;

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        "DCAboutDlg", "About", WS_POPUP,
        px, py, ABT_DLG_W, ABT_DLG_H,
        hParent, NULL, g_app.hInst, NULL);
    if (!hwnd) { free(s); return; }

    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)s);

    s->fadeAlpha = 0;
    SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOWNA);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    SetTimer(hwnd, 1, ABT_FADE_MS, NULL);

    /* Start procedural ambient drone (fades in over 2s, loops forever) */
    drone_start(s);

    /* Disable parent to prevent click-through while about dialog is open */
    EnableWindow(hParent, FALSE);

    MSG m;
    while (IsWindow(hwnd) && GetMessageA(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }

    /* Re-enable parent */
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
}

#endif
