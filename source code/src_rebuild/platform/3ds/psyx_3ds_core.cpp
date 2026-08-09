#include <3ds.h>
#include <libcd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "PsyX/PsyX_globals.h"
#include "PsyX/PsyX_public.h"
#include "PsyX/PsyX_render.h"
#include "PsyX_main.h"
#include "gpu/PsyX_GPU.h"
#include "pad/PsyX_pad.h"

extern int GR_InitialiseRender(char* windowName, int width, int height, int fullscreen);
extern int GR_InitialisePSX();
extern void GR_Shutdown();
extern void GR_BeginScene();
extern void GR_EndScene();

bool g_3dsGfxReady = false;

int g_swapInterval = 1;
int g_enableSwapInterval = 1;
int g_skipSwapInterval = 0;
int g_cfg_swapInterval = 1;
int g_activeKeyboardControllers = 0x1;
unsigned int g_swapTime = 0;

PsyXKeyboardMapping g_cfg_keyboardMapping = {};
PsyXControllerMapping g_cfg_controllerMapping = {};
GameOnTextInputHandler g_cfg_gameOnTextInput = NULL;
GameDebugKeysHandlerFunc g_dbg_gameDebugKeys = NULL;
GameDebugMouseHandlerFunc g_dbg_gameDebugMouse = NULL;

int g_dbg_polygonSelected = 0;
int g_vmode = -1;

extern void (*vsync_callback)(void);

static int s_vblankCount = 0;
static char s_sceneBegun = 0;

static void PsyX_LogV(const char* prefix, const char* fmt, va_list args)
{
    if (prefix)
        fputs(prefix, stdout);

    vprintf(fmt, args);
    fflush(stdout);
}

extern "C" void PsyX_Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PsyX_LogV(NULL, fmt, args);
    va_end(args);
}

extern "C" void PsyX_Log_Info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PsyX_LogV("[info] ", fmt, args);
    va_end(args);
}

extern "C" void PsyX_Log_Warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PsyX_LogV("[warn] ", fmt, args);
    va_end(args);
}

extern "C" void PsyX_Log_Error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PsyX_LogV("[error] ", fmt, args);
    va_end(args);
}

extern "C" void PsyX_Log_Success(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PsyX_LogV("[ok] ", fmt, args);
    va_end(args);
}

extern "C" void PsyX_Initialise(char* windowName, int screenWidth, int screenHeight, int fullscreen)
{
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    g_3dsGfxReady = true;

    GR_InitialiseRender(windowName, screenWidth, screenHeight, fullscreen);
    GR_InitialisePSX();
    ClearSplits();

    PsyX_Log_Info("Initialised %s at %dx%d\n", windowName, screenWidth, screenHeight);
}

extern "C" void PsyX_Shutdown(void)
{
    GR_Shutdown();

    if (g_3dsGfxReady)
    {
        gfxExit();
        g_3dsGfxReady = false;
    }
}

extern "C" void PsyX_GetScreenSize(int* screenWidth, int* screenHeight)
{
    if (screenWidth)
        *screenWidth = g_windowWidth;
    if (screenHeight)
        *screenHeight = g_windowHeight;
}

extern "C" void PsyX_SetCursorPosition(int x, int y)
{
    (void)x;
    (void)y;
}

extern "C" void PsyX_SetCursorRelative(int enable)
{
    (void)enable;
}

extern "C" char PsyX_BeginScene(void)
{
    if (s_sceneBegun)
        return 0;

    GR_BeginScene();

    if (activeDrawEnv.isbg)
        GR_Clear(activeDrawEnv.clip.x, activeDrawEnv.clip.y, activeDrawEnv.clip.w, activeDrawEnv.clip.h, activeDrawEnv.r0, activeDrawEnv.g0, activeDrawEnv.b0);

    s_sceneBegun = 1;
    return 1;
}

extern "C" void PsyX_EndScene(void)
{
    if (!s_sceneBegun)
        return;

    s_sceneBegun = 0;
    GR_EndScene();
    GR_StoreFrameBuffer(activeDispEnv.disp.x, activeDispEnv.disp.y, activeDispEnv.disp.w, activeDispEnv.disp.h);
    GR_SwapWindow();
}

extern "C" void PsyX_UpdateInput(void)
{
    PsyX_Pad_InternalPadUpdates();
}

extern "C" int PsyX_LookupKeyboardMapping(const char* str, int default_value)
{
    (void)str;
    return default_value;
}

extern "C" int PsyX_LookupGameControllerMapping(const char* str, int default_value)
{
    (void)str;
    return default_value;
}

extern "C" void PsyX_GetPSXWidescreenMappedViewport(RECT16* rect)
{
    rect->x = activeDispEnv.disp.x;
    rect->y = activeDispEnv.disp.y;
    rect->w = activeDispEnv.disp.w;
    rect->h = activeDispEnv.disp.h;
}

extern "C" void PsyX_WaitForTimestep(int count)
{
    for (int i = 0; i < count; i++)
    {
        gspWaitForVBlank();
        s_vblankCount++;

        if (vsync_callback)
            vsync_callback();

        PsyX_CDFS_StepSpooler();
    }
}

extern "C" void PsyX_EnableSwapInterval(int enable)
{
    g_enableSwapInterval = enable;
}

extern "C" void PsyX_SetSwapInterval(int interval)
{
    g_swapInterval = interval;
}

extern "C" int PsyX_Sys_SetVMode(int mode)
{
    int old = g_vmode;
    g_vmode = mode;
    return old;
}

extern "C" int PsyX_Sys_GetVBlankCount()
{
    return s_vblankCount;
}
