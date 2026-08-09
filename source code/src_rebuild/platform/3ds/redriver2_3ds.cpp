#include <3ds.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "platform.h"
#include "driver2.h"
#include "C/main.h"
#include "C/system.h"
#include "C/fmvplay.h"
#include "C/time.h"

#include "PsyX/PsyX_globals.h"
#include "PsyX/PsyX_public.h"

#ifndef REDRIVER2_3DS_SMOKE_TEST
#define REDRIVER2_3DS_SMOKE_TEST 0
#endif

#ifndef REDRIVER2_3DS_DRAW_DISTANCE
#define REDRIVER2_3DS_DRAW_DISTANCE 34
#endif

extern void DeinitStringMng();
extern int gDrawDistance;
extern int gFastLoadingScreens;
extern int gEnableDlights;

extern "C" void R2_3DS_Trace(const char* fmt, ...);

static void EnableHardwarePerfMode()
{
    osSetSpeedupEnable(true);

    Result cpuLimitResult = APT_SetAppCpuTimeLimit(80);
    if (R_FAILED(cpuLimitResult))
        R2_3DS_Trace("launcher: cpu time limit request failed rc=0x%08lx", cpuLimitResult);
    else
        R2_3DS_Trace("launcher: cpu time limit set to 80%%");

    R2_3DS_Trace("launcher: New 3DS speedup requested");
}

extern "C" void R2_3DS_Trace(const char* fmt, ...)
{
    char line[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    line[sizeof(line) - 1] = 0;

    printf("[3DS] %s\n", line);

    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/redriver2", 0777);

    FILE* fp = fopen("sdmc:/3ds/redriver2/debug.log", "ab");
    if (fp)
    {
        fprintf(fp, "%s\n", line);
        fclose(fp);
    }
}

static void FixSlashes(char* path)
{
    for (char* p = path; *p; p++)
    {
        if (*p == '\\')
            *p = '/';
    }
}

static int HasDataFile(const char* relativePath)
{
    char path[MAX_GAME_PATH];
    snprintf(path, sizeof(path), "%s%s", gDataFolder, relativePath);
    path[sizeof(path) - 1] = 0;
    FixSlashes(path);

    FILE* fp = fopen(path, "rb");
    if (!fp)
        return 0;

    fclose(fp);
    return 1;
}

static int ValidateDataFiles()
{
    static const char* requiredFiles[] =
    {
        "DATA\\FEFONT.BNK",
        "GFX\\FONT2.FNT",
        "GFX\\LOADCHIC.TIM",
        "GFX\\LOADHAVA.TIM",
        "GFX\\LOADVEGA.TIM",
        "GFX\\LOADRIO.TIM",
        "LANG\\EN_GAME.LTXT",
        "LANG\\EN_MISSION.LTXT",
        "LEVELS\\CHICAGO.LEV",
        "LEVELS\\HAVANA.LEV",
        "LEVELS\\VEGAS.LEV",
        "LEVELS\\RIO.LEV",
        NULL
    };
    static const char* optionalFiles[] =
    {
        "GFX\\HQ\\digits.tga",
        "GFX\\HQ\\fefont.fn2",
        "GFX\\HQ\\fefont.tga",
        "GFX\\HQ\\font2.fn2",
        "GFX\\HQ\\font2.tga",
        NULL
    };

    int missing = 0;
    printf("Data: %s\n", gDataFolder);

    for (int i = 0; requiredFiles[i]; i++)
    {
        if (!HasDataFile(requiredFiles[i]))
        {
            printf("Missing: %s\n", requiredFiles[i]);
            missing++;
        }
    }

    if (!HasDataFile("GFX\\SPLASH1N.TIM") && !HasDataFile("GFX\\SPLASH1P.TIM"))
    {
        printf("Missing: GFX\\SPLASH1N.TIM or GFX\\SPLASH1P.TIM\n");
        missing++;
    }

    for (int i = 0; optionalFiles[i]; i++)
    {
        if (!HasDataFile(optionalFiles[i]))
            printf("Optional missing: %s\n", optionalFiles[i]);
    }

    if (missing)
    {
        printf("\nCopy the full DRIVER2 data to:\n");
        printf("sdmc:/3ds/redriver2/DRIVER2/\n");
        return 0;
    }

    return 1;
}

static void WaitForStart()
{
    printf("\nPress START to exit.\n");

    while (aptMainLoop())
    {
        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        gspWaitForVBlank();
    }
}

#if REDRIVER2_3DS_SMOKE_TEST
static unsigned char ClampByte(int value)
{
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return (unsigned char)value;
}

static void PutTopPixel(u8* fb, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    if (!fb || x < 0 || y < 0 || x >= 400 || y >= 240)
        return;

    const int offset = 3 * (y + x * 240);
    fb[offset + 0] = b;
    fb[offset + 1] = g;
    fb[offset + 2] = r;
}

static void RunSmokeTest()
{
    int frame = 0;
    printf("3DS smoke test\n");
    printf("Circle Pad moves the square.\n");
    printf("Press START to exit.\n");

    while (aptMainLoop())
    {
        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        circlePosition circle;
        hidCircleRead(&circle);

        u16 fbWidth = 0;
        u16 fbHeight = 0;
        u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fbWidth, &fbHeight);

        for (int y = 0; y < 240; y++)
        {
            for (int x = 0; x < 400; x++)
            {
                unsigned char r = (unsigned char)((x + frame) & 0xFF);
                unsigned char g = (unsigned char)((y * 2) & 0xFF);
                unsigned char b = (unsigned char)(((x + y) / 2) & 0xFF);
                PutTopPixel(fb, x, y, r, g, b);
            }
        }

        const int squareX = 192 + (circle.dx * 96) / 156;
        const int squareY = 112 - (circle.dy * 64) / 156;
        for (int y = -12; y <= 12; y++)
        {
            for (int x = -12; x <= 12; x++)
            {
                PutTopPixel(fb, squareX + x, squareY + y,
                    ClampByte(255 - x * 3), ClampByte(220 - y * 3), 40);
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
        frame++;
    }
}
#endif

#if !USE_CRT_MALLOC
char g_Overlay_buffer[0x50000];
char g_Frontend_buffer[0x60000];
char g_Other_buffer[0x50000];
char g_Other_buffer2[0x50000];
OTTYPE g_OT1[OTSIZE];
OTTYPE g_OT2[OTSIZE];
char g_PrimTab1[PRIMTAB_SIZE];
char g_PrimTab2[PRIMTAB_SIZE];
char g_SBank_buffer[0x50000];
char g_Replay_buffer[0x50000];
#endif

int main(int argc, char** argv)
{
    R2_3DS_Trace("launcher: main enter argc=%d", argc);
    EnableHardwarePerfMode();

#if USE_CRT_MALLOC
    _overlay_buffer = (char*)malloc(0x50000);
    _frontend_buffer = (char*)malloc(0x60000);
    _other_buffer = (char*)malloc(0x50000);
    _other_buffer2 = (char*)malloc(0x50000);
    _OT1 = (OTTYPE*)malloc(OTSIZE * sizeof(OTTYPE));
    _OT2 = (OTTYPE*)malloc(OTSIZE * sizeof(OTTYPE));
    _primTab1 = (char*)malloc(PRIMTAB_SIZE);
    _primTab2 = (char*)malloc(PRIMTAB_SIZE);
    _sbank_buffer = (char*)malloc(0x80000);
    _replay_buffer = (char*)malloc(0x50000);
#else
    _overlay_buffer = g_Overlay_buffer;
    _frontend_buffer = g_Frontend_buffer;
    _other_buffer = g_Other_buffer;
    _other_buffer2 = g_Other_buffer2;
    _OT1 = g_OT1;
    _OT2 = g_OT2;
    _primTab1 = g_PrimTab1;
    _primTab2 = g_PrimTab2;
    _sbank_buffer = g_SBank_buffer;
    _replay_buffer = g_Replay_buffer;
#endif

    strcpy(gDataFolder, "sdmc:/3ds/redriver2/DRIVER2/");
    gNoFMV = 1;
    gDrawDistance = REDRIVER2_3DS_DRAW_DISTANCE;
    gFastLoadingScreens = 1;
    gEnableDlights = 0;

    R2_3DS_Trace("launcher: data folder %s", gDataFolder);
    R2_3DS_Trace("launcher: PsyX_Initialise begin");
    PsyX_Initialise((char*)"REDRIVER2 3DS", 400, 240, 0);
    R2_3DS_Trace("launcher: PsyX_Initialise done");

    char versionInfo[32];
    GetTimeStamp(versionInfo);
    PsyX_Log_Info("%s %s (%s)\n", GAME_VERSION, versionInfo, BUILD_CONFIGURATION_STRING);

#if REDRIVER2_3DS_SMOKE_TEST
    RunSmokeTest();
    PsyX_Shutdown();
    return 0;
#endif

    if (!ValidateDataFiles())
    {
        R2_3DS_Trace("launcher: data validation failed");
        WaitForStart();
        PsyX_Shutdown();
        return 1;
    }

    R2_3DS_Trace("launcher: data validation ok");
    R2_3DS_Trace("launcher: redriver2_main begin");
    redriver2_main(argc, argv);
    R2_3DS_Trace("launcher: redriver2_main returned");

    DeinitStringMng();
    PsyX_Shutdown();

    return 0;
}
