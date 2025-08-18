#include "EngineSound.h"
#include "Sound.h"

// Simple state
static const s8 *s_rearData = NULL;
static const s8 *s_frontData = NULL;
static u32 s_length = 0;     // samples
static u32 s_loopStart = 0;  // samples
static u32 s_loopLength = 0; // samples
static u32 s_baseHz = 18157; // default
static bool s_isFront = true;
static bool s_active = false;

// Use channel 0 for engine
#define ENG_CH 3

void EngineSound_init(const s8 *rearData,
                      const s8 *frontData,
                      u32 sampleLength,
                      u32 loopStart,
                      u32 loopLength,
                      u32 baseSampleRateHz)
{
    s_rearData = rearData;
    s_frontData = frontData;
    s_length = sampleLength;
    s_loopStart = loopStart;
    s_loopLength = loopLength;
    s_baseHz = baseSampleRateHz ? baseSampleRateHz : 18157;
}

static void set_inc_and_vol(SOUND_CHANNEL *ch, u32 volume0to64, u32 targetFreqHz)
{
    // increment in 12.12 fixed: (targetHz << 12) / mixHz
    ch->inc = (targetFreqHz << 12) / 18157;
    if(ch->inc == 0) ch->inc = 1;
    ch->vol = (volume0to64 > 64) ? 64 : volume0to64;
}

void EngineSound_start(bool useFrontSample, u32 volume0to64, u32 targetFreqHz)
{
    SOUND_CHANNEL *ch = &sndChannel[ENG_CH];
    s_isFront = useFrontSample;
    ch->data = 0; // stop first
    ch->pos = 0;
    ch->length = (s_length << 12);
    ch->loopLength = (s_loopLength ? (s_loopLength << 12) : 0);
    ch->data = s_isFront ? (s8*)s_frontData : (s8*)s_rearData;
    // Slight pan depending on side: front centered, rear slightly behind (bias left then right)
    ch->panL = s_isFront ? 64 : 48;
    ch->panR = s_isFront ? 64 : 48;
    set_inc_and_vol(ch, volume0to64, targetFreqHz);
    s_active = true;
}

void EngineSound_update(u32 volume0to64, u32 targetFreqHz)
{
    SOUND_CHANNEL *ch = &sndChannel[ENG_CH];
    if(!s_active || ch->data==0) return;
    set_inc_and_vol(ch, volume0to64, targetFreqHz);
}

void EngineSound_stop(void)
{
    sndChannel[ENG_CH].data = 0;
    s_active = false;
}

void EngineSound_demo_update_passby(u32 frame, u32 rangeT)
{
    if(rangeT == 0) return;
    if(frame >= rangeT)
    {
        EngineSound_stop();
        return;
    }

    u32 half = rangeT/2;
    bool front = frame >= half;

    // Volume ramps up to midpoint then down
    u32 vol = (frame <= half) ? (frame*64/half) : ((rangeT-frame)*64/half);
    if(vol>64) vol=64;

    // Natural playback of provided sample rate; rear pitched down a fifth (2/3)
    u32 natHz = s_baseHz;
    u32 hz = front ? natHz : (natHz*2)/3;

    if(frame == 0)
        EngineSound_start(front, vol, hz);
    else if(front != s_isFront)
        EngineSound_start(front, vol, hz);  // restart on side switch
    else
        EngineSound_update(vol, hz);
}


