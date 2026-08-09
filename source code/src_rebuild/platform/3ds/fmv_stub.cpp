#include "psx/types.h"
#include "C/fmvplay.h"
#include "PsyX/PsyX_globals.h"

int FMV_main(RENDER_ARGS* args)
{
    (void)args;
    PsyX_Log_Warning("FMV playback is disabled on 3DS scaffold.\n");
    return 0;
}
