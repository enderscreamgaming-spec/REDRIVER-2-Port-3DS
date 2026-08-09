#include <3ds.h>
#include <string.h>

#include "psx/types.h"
#include "psx/libpad.h"
#include "PsyX/PsyX_public.h"

extern "C" int g_padCommEnable;

int g_cfg_controllerToSlotMapping[MAX_CONTROLLERS] = { -1, -1 };

static u_char* s_padData[MAX_CONTROLLERS] = { NULL, NULL };

static unsigned char StickToPsxAxis(s16 value)
{
    if (value > -10 && value < 10)
        value = 0;

    int mapped = 128 + (value * 127) / 156;

    if (mapped < 0)
        mapped = 0;
    if (mapped > 255)
        mapped = 255;

    return (unsigned char)mapped;
}

extern "C" void PsyX_Pad_InitPad(int slot, u_char* padData)
{
    if (slot < 0 || slot >= MAX_CONTROLLERS)
        return;

    s_padData[slot] = padData;

    if (!padData)
        return;

    LPPADRAW pad = (LPPADRAW)padData;
    pad->status = 0;
    pad->id = slot == 0 ? 0x73 : 0xFF;
    pad->buttons[0] = 0xFF;
    pad->buttons[1] = 0xFF;
    pad->analog[0] = 128;
    pad->analog[1] = 128;
    pad->analog[2] = 128;
    pad->analog[3] = 128;
}

extern "C" int PsyX_Pad_GetStatus(int mtap, int slot)
{
    (void)mtap;
    return slot == 0 && s_padData[0] != NULL;
}

extern "C" void PsyX_Pad_Vibrate(int mtap, int slot, unsigned char* table, int len)
{
    (void)mtap;
    (void)slot;
    (void)table;
    (void)len;
}

extern "C" void PsyX_Pad_InternalPadUpdates()
{
    if (g_padCommEnable == 0 || !s_padData[0])
        return;

    hidScanInput();

    u32 held = hidKeysHeld();
    circlePosition circle;
    circlePosition cstick;

    hidCircleRead(&circle);
    memset(&cstick, 0, sizeof(cstick));
    hidCstickRead(&cstick);

    u_short buttons = 0xFFFF;

    // PS1 mapping:
    // B/Circle Pad = accelerate/steer, Y = brake, A = circle action, X = triangle action.
    // L/R map to L1/R1. New 3DS ZL/ZR map to L2/R2. C-stick maps to right analog.
    if (held & KEY_SELECT)
        buttons &= ~0x0001;
    if (held & KEY_START)
        buttons &= ~0x0008;
    if (held & KEY_DUP)
        buttons &= ~0x0010;
    if (held & KEY_DRIGHT)
        buttons &= ~0x0020;
    if (held & KEY_DDOWN)
        buttons &= ~0x0040;
    if (held & KEY_DLEFT)
        buttons &= ~0x0080;
    if (held & KEY_L)
        buttons &= ~0x0400;
    if (held & KEY_R)
        buttons &= ~0x0800;

#if defined(KEY_ZL)
    if (held & KEY_ZL)
        buttons &= ~0x0100;
#endif

#if defined(KEY_ZR)
    if (held & KEY_ZR)
        buttons &= ~0x0200;
#endif

    if (held & KEY_X)
        buttons &= ~0x1000;
    if (held & KEY_A)
        buttons &= ~0x2000;
    if (held & KEY_B)
        buttons &= ~0x4000;
    if (held & KEY_Y)
        buttons &= ~0x8000;

    LPPADRAW pad = (LPPADRAW)s_padData[0];
    pad->status = 0;
    pad->id = 0x73;
    *(u_short*)pad->buttons = buttons;
    pad->analog[0] = StickToPsxAxis(cstick.dx);
    pad->analog[1] = StickToPsxAxis(-cstick.dy);
    pad->analog[2] = StickToPsxAxis(circle.dx);
    pad->analog[3] = StickToPsxAxis(-circle.dy);
}
