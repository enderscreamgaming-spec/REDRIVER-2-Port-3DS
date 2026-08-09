#include <3ds.h>
#include <string.h>

#include "audio/PsyX_SPUAL.h"

static const int SPU_MEM_SIZE = 2 * 1024 * 1024;
static const int SPU_VOICE_COUNT = 24;
static const int MAX_DECODED_SAMPLES = 262144;

static u_char s_spuMem[SPU_MEM_SIZE];
static u_int s_writeAddr = 0;
static u_int s_allocAddr = 0;
static int s_mute = 0;
static int s_reverb = 0;
static u_int s_reverbVoice = 0;
static int s_ndspReady = 0;
static int s_csndReady = 0;
static int s_loggedFirstVoice = 0;

extern "C" void R2_3DS_Trace(const char* fmt, ...);

struct VoiceState
{
    SpuVoiceAttr attr;
    ndspWaveBuf waveBuf;
    short* pcm;
    int sampleCount;
};

static VoiceState s_voices[SPU_VOICE_COUNT];

static int ClampInt(int value, int lo, int hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static int VoiceBit(int voice)
{
    return 1 << voice;
}

static float VoiceGain(short volume)
{
    int v = volume;
    if (v < 0)
        v = -v;
    v = ClampInt(v, 0, 16384);
    return (float)v / 16384.0f;
}

static float VoiceRate(const VoiceState* voice)
{
    int pitch = voice->attr.pitch;
    if (pitch <= 0)
        pitch = 4096;

    float rate = 44100.0f * (float)pitch / 4096.0f;
    if (rate < 2000.0f)
        rate = 2000.0f;
    if (rate > 96000.0f)
        rate = 96000.0f;
    return rate;
}

static int CsndChannel(int voice)
{
    return 8 + voice;
}

static void VoiceVolPan(const VoiceState* voice, float* vol, float* pan)
{
    const float left = VoiceGain(voice->attr.volume.left);
    const float right = VoiceGain(voice->attr.volume.right);
    const float sum = left + right;

    if (vol)
        *vol = sum > 0.0f ? (sum * 0.5f) : 0.0f;
    if (pan)
        *pan = sum > 0.0f ? (right - left) / sum : 0.0f;
}

static void ApplyVoiceParams(int voice)
{
    if (voice < 0 || voice >= SPU_VOICE_COUNT)
        return;

    VoiceState* state = &s_voices[voice];

    if (s_csndReady)
    {
        float vol = 0.0f;
        float pan = 0.0f;
        VoiceVolPan(state, &vol, &pan);
        CSND_SetVol(CsndChannel(voice), CSND_VOL(vol, pan), 0);
        CSND_SetTimer(CsndChannel(voice), CSND_TIMER((u32)VoiceRate(state)));
        CSND_UpdateInfo(false);
    }

    if (!s_ndspReady)
        return;

    float mix[12] = {};
    mix[0] = VoiceGain(state->attr.volume.left);
    mix[1] = VoiceGain(state->attr.volume.right);

    ndspChnSetFormat(voice, NDSP_FORMAT_MONO_PCM16);
    ndspChnSetInterp(voice, NDSP_INTERP_LINEAR);
    ndspChnSetRate(voice, VoiceRate(state));
    ndspChnSetMix(voice, mix);
    ndspChnSetPaused(voice, state->attr.pitch == 0);
}

static int CountAdpcmSamples(const u_char* data, int bytes, int* loopStart, int* loopLength)
{
    int samples = 0;
    int localLoopStart = 0;
    int localLoopLength = 0;

    for (int pos = 0; pos + 15 < bytes && samples < MAX_DECODED_SAMPLES; pos += 16)
    {
        const u_char flags = data[pos + 1];
        const int blockStart = samples;
        samples += 28;

        if (flags & (1 << 2))
            localLoopStart = blockStart;

        if (flags & (1 << 0))
        {
            if (flags & (1 << 1))
                localLoopLength = samples - localLoopStart;
            break;
        }
    }

    if (loopStart)
        *loopStart = localLoopStart;
    if (loopLength)
        *loopLength = localLoopLength;

    return ClampInt(samples, 0, MAX_DECODED_SAMPLES);
}

static int DecodeAdpcm(const u_char* data, int bytes, short* out, int maxSamples)
{
    static const int k0[5] = { 0, 60, 115, 98, 122 };
    static const int k1[5] = { 0, 0, -52, -55, -60 };
    int prev1 = 0;
    int prev2 = 0;
    int outCount = 0;

    for (int pos = 0; pos + 15 < bytes && outCount < maxSamples; pos += 16)
    {
        const u_char param = data[pos];
        const u_char flags = data[pos + 1];
        const int shift = param & 0x0F;
        int filter = (param >> 4) & 0x0F;
        if (filter > 4)
            filter = 0;

        for (int i = 0; i < 14 && outCount < maxSamples; i++)
        {
            const u_char packed = data[pos + 2 + i];
            for (int half = 0; half < 2 && outCount < maxSamples; half++)
            {
                int nibble = half == 0 ? (packed & 0x0F) : ((packed >> 4) & 0x0F);
                if (nibble >= 8)
                    nibble -= 16;

                int sample = nibble << 12;
                if (shift > 0)
                    sample >>= shift;

                sample += (prev1 * k0[filter] + prev2 * k1[filter] + 32) >> 6;
                sample = ClampInt(sample, -32768, 32767);
                out[outCount++] = (short)sample;
                prev2 = prev1;
                prev1 = sample;
            }
        }

        if (flags & (1 << 0))
            break;
    }

    return outCount;
}

static void FreeVoiceBuffer(int voice)
{
    if (voice < 0 || voice >= SPU_VOICE_COUNT)
        return;

    if (s_ndspReady)
        ndspChnWaveBufClear(voice);
    if (s_csndReady)
    {
        CSND_SetPlayState(CsndChannel(voice), 0);
        CSND_UpdateInfo(false);
    }

    if (s_voices[voice].pcm)
    {
        linearFree(s_voices[voice].pcm);
        s_voices[voice].pcm = NULL;
    }

    memset(&s_voices[voice].waveBuf, 0, sizeof(s_voices[voice].waveBuf));
    s_voices[voice].sampleCount = 0;
}

static void PlayVoice(int voice)
{
    if ((!s_ndspReady && !s_csndReady) || voice < 0 || voice >= SPU_VOICE_COUNT || s_mute)
        return;

    VoiceState* state = &s_voices[voice];
    const u_int addr = state->attr.addr;
    if (addr >= SPU_MEM_SIZE)
        return;

    int loopStart = 0;
    int loopLength = 0;
    const int available = SPU_MEM_SIZE - (int)addr;
    int sampleCount = CountAdpcmSamples(&s_spuMem[addr], available, &loopStart, &loopLength);
    if (sampleCount <= 0)
        return;

    FreeVoiceBuffer(voice);

    short* pcm = (short*)linearAlloc(sampleCount * sizeof(short));
    if (!pcm)
        return;

    sampleCount = DecodeAdpcm(&s_spuMem[addr], available, pcm, sampleCount);
    if (sampleCount <= 0)
    {
        linearFree(pcm);
        return;
    }

    state->pcm = pcm;
    state->sampleCount = sampleCount;
    DSP_FlushDataCache(pcm, sampleCount * sizeof(short));

    memset(&state->waveBuf, 0, sizeof(state->waveBuf));
    state->waveBuf.data_pcm16 = pcm;
    state->waveBuf.nsamples = sampleCount;
    state->waveBuf.looping = loopLength > 0;

    if (s_ndspReady)
    {
        ndspChnReset(voice);
        ApplyVoiceParams(voice);
        ndspChnWaveBufAdd(voice, &state->waveBuf);
    }
    else if (s_csndReady)
    {
        float vol = 0.0f;
        float pan = 0.0f;
        VoiceVolPan(state, &vol, &pan);
        const u32 flags = SOUND_FORMAT_16BIT | SOUND_LINEAR_INTERP | (loopLength > 0 ? SOUND_REPEAT : SOUND_ONE_SHOT);
        const u32 bytes = sampleCount * sizeof(short);

        GSPGPU_FlushDataCache(pcm, bytes);
        CSND_SetPlayState(CsndChannel(voice), 0);
        CSND_UpdateInfo(false);
        csndPlaySound(CsndChannel(voice), flags, (u32)VoiceRate(state), vol, pan, pcm, NULL, bytes);
    }

    if (!s_loggedFirstVoice)
    {
        R2_3DS_Trace("SPU: first %s voice=%d addr=%u samples=%d loop=%d", s_ndspReady ? "NDSP" : "CSND", voice, addr, sampleCount, loopLength > 0);
        s_loggedFirstVoice = 1;
    }
}

extern "C" int PsyX_SPUAL_InitSound()
{
    const int audioAlreadyReady = s_ndspReady || s_csndReady;

    memset(s_spuMem, 0, sizeof(s_spuMem));
    memset(s_voices, 0, sizeof(s_voices));
    s_writeAddr = 0;
    s_allocAddr = 0;
    s_mute = 0;

    if (!audioAlreadyReady)
    {
        Result rc = ndspInit();
        s_ndspReady = R_SUCCEEDED(rc);
        if (!s_ndspReady)
        {
            R2_3DS_Trace("SPU: ndspInit failed rc=0x%08lx", (unsigned long)rc);
            rc = csndInit();
            s_csndReady = R_SUCCEEDED(rc);
            if (s_csndReady)
                R2_3DS_Trace("SPU: csndInit fallback ok");
            else
                R2_3DS_Trace("SPU: csndInit failed rc=0x%08lx", (unsigned long)rc);
        }

        if (s_ndspReady)
            R2_3DS_Trace("SPU: ndspInit ok");
    }

    if (s_ndspReady)
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    for (int i = 0; i < SPU_VOICE_COUNT; i++)
    {
        if (s_ndspReady)
            ndspChnReset(i);
        if (s_csndReady)
            CSND_SetPlayState(CsndChannel(i), 0);

        s_voices[i].attr.voice = VoiceBit(i);
        s_voices[i].attr.pitch = 4096;
        s_voices[i].attr.volume.left = 0x3000;
        s_voices[i].attr.volume.right = 0x3000;
        ApplyVoiceParams(i);
    }

    return 1;
}

extern "C" void PsyX_SPUAL_ShutdownSound()
{
    for (int i = 0; i < SPU_VOICE_COUNT; i++)
        FreeVoiceBuffer(i);

    if (s_ndspReady)
    {
        ndspExit();
        s_ndspReady = 0;
    }

    if (s_csndReady)
    {
        csndExit();
        s_csndReady = 0;
    }
}

extern "C" int PsyX_SPUAL_Alloc(int size)
{
    int addr = (int)s_allocAddr;
    s_allocAddr += (size + 7) & ~7;

    if (s_allocAddr > SPU_MEM_SIZE)
        return -1;

    return addr;
}

extern "C" int PsyX_SPUAL_InitAlloc(int num, char* top)
{
    (void)num;
    (void)top;
    s_allocAddr = 0;
    return 0;
}

extern "C" void PsyX_SPUAL_Free(u_int addr)
{
    (void)addr;
}

extern "C" u_int PsyX_SPUAL_Write(u_char* addr, u_int size)
{
    if (!addr || s_writeAddr >= SPU_MEM_SIZE)
        return 0;

    if (s_writeAddr + size > SPU_MEM_SIZE)
        size = SPU_MEM_SIZE - s_writeAddr;

    memcpy(&s_spuMem[s_writeAddr], addr, size);
    s_writeAddr += size;
    return size;
}

extern "C" u_int PsyX_SPUAL_Read(u_char* addr, u_int size)
{
    if (!addr || s_writeAddr >= SPU_MEM_SIZE)
        return 0;

    if (s_writeAddr + size > SPU_MEM_SIZE)
        size = SPU_MEM_SIZE - s_writeAddr;

    memcpy(addr, &s_spuMem[s_writeAddr], size);
    s_writeAddr += size;
    return size;
}

extern "C" u_int PsyX_SPUAL_SetTransferStartAddr(u_int addr)
{
    u_int old = s_writeAddr;
    s_writeAddr = addr < SPU_MEM_SIZE ? addr : 0;
    return old;
}

extern "C" void PsyX_SPUAL_GetVoiceVolume(int vNum, short* volL, short* volR)
{
    if (vNum < 0 || vNum >= SPU_VOICE_COUNT)
        return;

    if (volL)
        *volL = s_voices[vNum].attr.volume.left;
    if (volR)
        *volR = s_voices[vNum].attr.volume.right;
}

extern "C" void PsyX_SPUAL_GetVoicePitch(int vNum, u_short* pitch)
{
    if (vNum < 0 || vNum >= SPU_VOICE_COUNT || !pitch)
        return;

    *pitch = s_voices[vNum].attr.pitch;
}

extern "C" void PsyX_SPUAL_SetVoiceAttr(SpuVoiceAttr* psxAttrib)
{
    if (!psxAttrib)
        return;

    for (int i = 0; i < SPU_VOICE_COUNT; i++)
    {
        if ((psxAttrib->voice & VoiceBit(i)) == 0)
            continue;

        VoiceState* state = &s_voices[i];
        state->attr.voice = VoiceBit(i);

        if (psxAttrib->mask == 0)
        {
            state->attr = *psxAttrib;
            state->attr.voice = VoiceBit(i);
        }
        else
        {
            if (psxAttrib->mask & SPU_VOICE_VOLL)
                state->attr.volume.left = psxAttrib->volume.left;
            if (psxAttrib->mask & SPU_VOICE_VOLR)
                state->attr.volume.right = psxAttrib->volume.right;
            if (psxAttrib->mask & SPU_VOICE_PITCH)
                state->attr.pitch = psxAttrib->pitch;
            if (psxAttrib->mask & SPU_VOICE_WDSA)
                state->attr.addr = psxAttrib->addr;
            if (psxAttrib->mask & SPU_VOICE_LSAX)
                state->attr.loop_addr = psxAttrib->loop_addr;
        }

        ApplyVoiceParams(i);
    }
}

extern "C" void PsyX_SPUAL_SetKey(int on_off, u_int voice_bit)
{
    for (int i = 0; i < SPU_VOICE_COUNT; i++)
    {
        if ((voice_bit & VoiceBit(i)) == 0)
            continue;

        if (on_off)
            PlayVoice(i);
        else
            FreeVoiceBuffer(i);
    }
}

extern "C" int PsyX_SPUAL_GetKeyStatus(u_int voice_bit)
{
    if (!s_ndspReady && !s_csndReady)
        return 0;

    for (int i = 0; i < SPU_VOICE_COUNT; i++)
    {
        if (voice_bit != (u_int)VoiceBit(i))
            continue;

        if (s_ndspReady)
        {
            return ndspChnIsPlaying(i) ||
                s_voices[i].waveBuf.status == NDSP_WBUF_QUEUED ||
                s_voices[i].waveBuf.status == NDSP_WBUF_PLAYING;
        }

        u8 playing = 0;
        CSND_UpdateInfo(false);
        csndIsPlaying(CsndChannel(i), &playing);
        return playing != 0;
    }

    return 0;
}

extern "C" void PsyX_SPUAL_GetAllKeysStatus(char* status)
{
    if (!status)
        return;

    for (int i = 0; i < SPU_VOICE_COUNT; i++)
    {
        if (s_ndspReady)
        {
            status[i] = ndspChnIsPlaying(i) ||
                s_voices[i].waveBuf.status == NDSP_WBUF_QUEUED ||
                s_voices[i].waveBuf.status == NDSP_WBUF_PLAYING;
        }
        else if (s_csndReady)
        {
            u8 playing = 0;
            csndIsPlaying(CsndChannel(i), &playing);
            status[i] = playing != 0;
        }
        else
        {
            status[i] = 0;
        }
    }
}

extern "C" int PsyX_SPUAL_SetMute(int on_off)
{
    int old = s_mute;
    s_mute = on_off;
    if (s_mute)
    {
        for (int i = 0; i < SPU_VOICE_COUNT; i++)
            FreeVoiceBuffer(i);
    }
    return old;
}

extern "C" int PsyX_SPUAL_SetReverb(int on_off)
{
    int old = s_reverb;
    s_reverb = on_off;
    return old;
}

extern "C" int PsyX_SPUAL_GetReverbState()
{
    return s_reverb;
}

extern "C" u_int PsyX_SPUAL_SetReverbVoice(int on_off, u_int voice_bit)
{
    u_int old = s_reverbVoice;
    s_reverbVoice = on_off ? (s_reverbVoice | voice_bit) : (s_reverbVoice & ~voice_bit);
    return old;
}

extern "C" u_int PsyX_SPUAL_GetReverbVoice()
{
    return s_reverbVoice;
}
