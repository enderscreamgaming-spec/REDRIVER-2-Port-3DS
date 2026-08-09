#ifndef REDRIVER2_3DS_AL_SHIM_H
#define REDRIVER2_3DS_AL_SHIM_H

typedef unsigned int ALuint;
typedef int ALenum;
typedef int ALsizei;
typedef int ALint;
typedef float ALfloat;
typedef char ALboolean;
typedef void ALvoid;

#define AL_NONE 0
#define AL_FALSE 0
#define AL_TRUE 1
#define AL_FORMAT_MONO8 0x1100
#define AL_FORMAT_MONO16 0x1101
#define AL_FORMAT_STEREO8 0x1102
#define AL_FORMAT_STEREO16 0x1103
#define AL_LOOPING 0x1007
#define AL_BUFFER 0x1009
#define AL_GAIN 0x100A
#define AL_PITCH 0x1003
#define AL_SOURCE_STATE 0x1010
#define AL_STOPPED 0x1014
#define AL_SOURCE_RELATIVE 0x202
#define AL_SOURCE_RESAMPLER_SOFT 0x1210
#define AL_LOOP_POINTS_SOFT 0x2015
#define AL_SAMPLE_LENGTH_SOFT 0x2009

static inline void alGenSources(ALsizei n, ALuint* sources)
{
    for (ALsizei i = 0; i < n; i++)
        sources[i] = (ALuint)(i + 1);
}

static inline void alDeleteSources(ALsizei n, const ALuint* sources)
{
    (void)n;
    (void)sources;
}

static inline void alGenBuffers(ALsizei n, ALuint* buffers)
{
    for (ALsizei i = 0; i < n; i++)
        buffers[i] = (ALuint)(i + 1);
}

static inline void alDeleteBuffers(ALsizei n, const ALuint* buffers)
{
    (void)n;
    (void)buffers;
}

static inline void alBufferData(ALuint buffer, ALenum format, const ALvoid* data, ALsizei size, ALsizei freq)
{
    (void)buffer;
    (void)format;
    (void)data;
    (void)size;
    (void)freq;
}

static inline void alBufferiv(ALuint buffer, ALenum param, const ALint* values)
{
    (void)buffer;
    (void)param;
    (void)values;
}

static inline void alGetBufferi(ALuint buffer, ALenum param, ALint* value)
{
    (void)buffer;
    (void)param;
    if (value)
        *value = 0;
}

static inline void alSourcei(ALuint source, ALenum param, ALint value)
{
    (void)source;
    (void)param;
    (void)value;
}

static inline void alSourcef(ALuint source, ALenum param, ALfloat value)
{
    (void)source;
    (void)param;
    (void)value;
}

static inline void alSourcePlay(ALuint source)
{
    (void)source;
}

static inline void alSourcePause(ALuint source)
{
    (void)source;
}

static inline void alSourceStop(ALuint source)
{
    (void)source;
}

static inline void alGetSourcei(ALuint source, ALenum param, ALint* value)
{
    (void)source;
    (void)param;
    if (value)
        *value = AL_STOPPED;
}

#endif
