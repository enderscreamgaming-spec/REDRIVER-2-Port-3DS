#ifdef __3DS__
#include <3ds.h>
#endif

#include "driver2.h"
#include "state.h"
#include "main.h"
#include "glaunch.h"
#include "Frontend/FEmain.h"

#ifdef __3DS__
extern "C" void R2_3DS_Trace(const char* fmt, ...);
#endif

//-------------------------------------------

typedef void (*StateFn)(void*);

StateFn gStates[] = {
	NULL,
	State_InitFrontEnd,
	State_FrontEnd,
	State_GameStart,
	State_LaunchGame,
	State_MissionLadder,
	State_GameInit,
	State_GameLoop,
	State_GameComplete,
	State_FMVPlay,
};

StateFn gCurrentState = NULL;
void* gCurrentStateParam = NULL;

#ifdef __3DS__
static const char* GetStateName(GameStates state)
{
	switch (state)
	{
	case STATE_NONE: return "STATE_NONE";
	case STATE_INITFRONTEND: return "STATE_INITFRONTEND";
	case STATE_FRONTEND: return "STATE_FRONTEND";
	case STATE_GAMESTART: return "STATE_GAMESTART";
	case STATE_GAMELAUNCH: return "STATE_GAMELAUNCH";
	case STATE_LADDER: return "STATE_LADDER";
	case STATE_GAMEINIT: return "STATE_GAMEINIT";
	case STATE_GAMELOOP: return "STATE_GAMELOOP";
	case STATE_GAMECOMPLETE: return "STATE_GAMECOMPLETE";
	case STATE_FMVPLAY: return "STATE_FMVPLAY";
	default: return "STATE_UNKNOWN";
	}
}
#endif

#ifdef __EMSCRIPTEN__
void emStateFunc()
{
	StateFn stateFn = gCurrentState;

	if (!stateFn)
		return;

	stateFn(gCurrentStateParam);
}
#endif

// the main loop of the game
void DoStateLoop()
{
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(emStateFunc, 0, 1);
#else
#ifdef __3DS__
	R2_3DS_Trace("state: loop enter");
#endif
	do
	{
#ifdef __3DS__
		if (!aptMainLoop())
		{
			R2_3DS_Trace("state: aptMainLoop exit");
			break;
		}
#endif

		StateFn stateFn = gCurrentState;

		if (!stateFn)
		{
#ifdef __3DS__
			R2_3DS_Trace("state: null state exit");
#endif
			break;
		}

		stateFn(gCurrentStateParam);
	} while (true);
#ifdef __3DS__
	R2_3DS_Trace("state: loop leave");
#endif
#endif
}

void SetState(GameStates newState, void* param)
{
#ifdef __3DS__
	R2_3DS_Trace("state: SetState %s param=%p", GetStateName(newState), param);
#endif
	gCurrentState = gStates[newState];
	gCurrentStateParam = param;
}
