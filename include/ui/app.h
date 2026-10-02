/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  Win32 UI Framework
 *  Modeled on WBC Viewer dark theme approach
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_UI_APP_H
#define DC_UI_APP_H

#ifndef WINVER
#define WINVER 0x0501
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif
#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif
#ifndef COLOR_MENUBAR
#define COLOR_MENUBAR 30
#endif
#ifndef COLOR_MENUHILIGHT
#define COLOR_MENUHILIGHT 29
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <stdio.h>

#include "core/types.h"

struct DatArchive;
struct Mesh;

/*═══════════════════════════════════════════════════════════════════
 *  Theme  -  simple color struct (like WBC viewer)
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

struct AppTheme {
    COLORREF bg, panelBg, text, textDim, border, accent;
    COLORREF menuBg, menuHover, menuPress;
    COLORREF listBg, listSel, listText;
    COLORREF statusBg, statusText;
    COLORREF hexBg, hexAddr, hexByte, hexAscii;
    BOOL isDark;
};

extern AppTheme T;
extern HBRUSH g_hBrBg;
extern HBRUSH g_hBrPanel;

void LoadThemeApis(void);
BOOL DetectSystemDarkMode(void);
void ApplyTheme(BOOL dark);
void ApplyDarkToControl(HWND hwnd);  /* apply dark explorer theme to a control */

/*═══════════════════════════════════════════════════════════════════
 *  Custom Menu Bar
 *═══════════════════════════════════════════════════════════════════*/
#define MAX_MENUBAR_ITEMS 8
#define MENUBAR_H  26
#define STATUS_H   22

struct MenuBarItem { const char* label; HMENU hSub; RECT rc; };
extern MenuBarItem g_mbItems[MAX_MENUBAR_ITEMS];
extern int g_mbCount;
extern int g_mbHot;

LRESULT CALLBACK MenuBarProc(HWND, UINT, WPARAM, LPARAM);
void CreateAppMenus(void);

/*═══════════════════════════════════════════════════════════════════
 *  Panel IDs (no visible tabs  -  auto-switched)
 *═══════════════════════════════════════════════════════════════════*/
enum PanelID {
    PANEL_HEX     = 0,
    PANEL_IMAGE   = 1,
    PANEL_3D      = 2,
    PANEL_AUDIO   = 3,
    PANEL_PALETTE = 4,
    PANEL_VIDEO   = 5,
    PANEL_SAVE    = 6,
    PANEL_WEP     = 7,
    PANEL_SCD     = 8,
    PANEL_COUNT
};

/*═══════════════════════════════════════════════════════════════════
 *  Application State
 *═══════════════════════════════════════════════════════════════════*/
struct App {
    HINSTANCE   hInst;
    HWND        hMain;
    HWND        hMenuBar;
    HWND        hStatus;
    HWND        hTree;
    HWND        hPanelArea;

    HFONT       hFontUI;
    HFONT       hFontMono;

    DatArchive* archive;
    int         active_panel;
    bool        nav_hidden;   /* spacebar toggle: hide tree, 3D panel goes full width */
    u8*         raw_hex_buf;  /* owned buffer for raw hex view of non-DAT files */
    size_t      raw_hex_size;

    App();
    bool init(HINSTANCE hInstance, int nCmdShow);
    int  run();
    void shutdown();

    bool open_archive(const char* path);
    void close_archive();
    void build_tree();
    void set_status(const char* text);
    void switch_panel(int panel_id);
    int  selected_entry_idx();  /* get entry index from current tree selection */
};

extern App g_app;

/*═══════════════════════════════════════════════════════════════════
 *  Window Procedures
 *═══════════════════════════════════════════════════════════════════*/
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);

/*═══════════════════════════════════════════════════════════════════
 *  IDs
 *═══════════════════════════════════════════════════════════════════*/
enum {
    IDM_FILE_OPEN       = 1001,
    IDM_FILE_SAVE       = 1002,
    IDM_FILE_SAVE_AS    = 1003,
    IDM_FILE_CLOSE      = 1004,
    IDM_FILE_EXIT       = 1005,
    IDM_ENTRY_EXPORT    = 2001,
    IDM_ENTRY_EXPORT_ALL= 2002,
    IDM_ENTRY_IMPORT    = 2003,
    IDM_ENTRY_REPLACE   = 2004,
    IDM_ENTRY_DELETE    = 2005,
    IDM_ENTRY_ADD       = 2006,
    IDM_EXPORT_OBJ      = 2101,
    IDM_EXPORT_WAV      = 2102,
    IDM_EXPORT_PNG      = 2103,
    IDM_EXPORT_TGA      = 2104,
    IDM_EXPORT_DEC      = 2105,
    IDM_EXPORT_SMD      = 2106,
    IDM_EXPORT_ATLAS    = 2107,
    IDM_EXPORT_COLL     = 2108,
    IDM_EXPORT_GLB      = 2109,
    IDM_VIEW_HEX        = 3001,
    IDM_VIEW_3D         = 3002,
    IDM_VIEW_IMAGE      = 3003,
    IDM_VIEW_AUDIO      = 3004,
    IDM_VIEW_PALETTE    = 3005,
    IDM_VIEW_FIT_SCREEN = 3010,
    IDM_VIEW_SCD        = 3011,
    IDM_HELP_ABOUT      = 9001,
    IDC_ENTRY_LIST      = 4001,
    IDC_MENUBAR         = 4002,
    IDC_PANEL_AREA      = 4003,
    IDC_STATUS_BAR      = 4004
};

bool ui_open_file(HWND parent, char* path, int max_len,
                  const char* filter, const char* title);
bool ui_save_file(HWND parent, char* path, int max_len,
                  const char* filter, const char* title,
                  const char* default_ext);

#endif /* DC_UI_APP_H */
