#ifndef REDRIVER2_3DS_SDL_SHIM_H
#define REDRIVER2_3DS_SDL_SHIM_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_Thread SDL_Thread;
typedef int (*SDL_ThreadFunction)(void* data);

struct SDL_Window { int unused; };
struct SDL_Texture { int unused; };
struct SDL_mutex { int unused; };
struct SDL_Thread { int unused; };

typedef struct SDL_Rect
{
    int x;
    int y;
    int w;
    int h;
} SDL_Rect;

typedef struct SDL_Surface
{
    void* pixels;
    int w;
    int h;
    int pitch;
} SDL_Surface;

#define SDL_MESSAGEBOX_ERROR 0x00000010
#define SDL_MESSAGEBOX_INFORMATION 0x00000040
#define SDL_WINDOW_SHOWN 0x00000004
#define SDL_INIT_TIMER 0x00000001

static inline int SDL_ShowSimpleMessageBox(Uint32 flags, const char* title, const char* message, SDL_Window* window)
{
    (void)flags;
    (void)window;
    printf("%s: %s\n", title ? title : "SDL", message ? message : "");
    return 0;
}

static inline SDL_mutex* SDL_CreateMutex(void)
{
    return (SDL_mutex*)calloc(1, sizeof(SDL_mutex));
}

static inline void SDL_DestroyMutex(SDL_mutex* mutex)
{
    free(mutex);
}

static inline int SDL_LockMutex(SDL_mutex* mutex)
{
    (void)mutex;
    return 0;
}

static inline int SDL_UnlockMutex(SDL_mutex* mutex)
{
    (void)mutex;
    return 0;
}

static inline SDL_Thread* SDL_CreateThread(SDL_ThreadFunction fn, const char* name, void* data)
{
    (void)name;

    if (fn)
        fn(data);

    return (SDL_Thread*)calloc(1, sizeof(SDL_Thread));
}

static inline void SDL_WaitThread(SDL_Thread* thread, int* status)
{
    if (status)
        *status = 0;

    free(thread);
}

static inline const char* SDL_GetError(void)
{
    return "SDL 3DS shim";
}

static inline void SDL_Delay(Uint32 ms)
{
    (void)ms;
}

#define SDL_malloc malloc
#define SDL_free free

#endif
