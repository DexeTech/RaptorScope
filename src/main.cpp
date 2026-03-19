/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  WinMain Entry Point
 *  Dino Crisis (PC) .DAT Archive Multi-Tool
 *═══════════════════════════════════════════════════════════════════*/
#include "ui/app.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrev;
    (void)lpCmdLine;

    /* Initialize common controls (needed for trackbar/slider) */
    INITCOMMONCONTROLSEX icx;
    icx.dwSize = sizeof(icx);
    icx.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icx);

    if (!g_app.init(hInstance, nCmdShow))
        return 1;

    int ret = g_app.run();
    g_app.shutdown();
    return ret;
}
