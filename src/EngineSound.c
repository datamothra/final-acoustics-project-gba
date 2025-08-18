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
static u32 s_panL = 64, s_panR = 64;
static bool s_frontToBack = true;

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
    ch->inc = (targetFreqHz << 12) / 31536;  // Sappy standard rate
    if(ch->inc == 0) ch->inc = 1;
    ch->vol = (volume0to64 > 64) ? 64 : volume0to64;
}

void EngineSound_start(bool useFrontSample, u32 volume0to64, u32 targetFreqHz)
{
    SOUND_CHANNEL *ch = &sndChannel[ENG_CH];
    s_isFront = useFrontSample;
    ch->data = 0; // stop first
    ch->pos = 0;  // Start from beginning of sample, not loop point!
    ch->length = (s_length << 12);  // Use full length
    ch->loopLength = (s_loopLength ? (s_loopLength << 12) : 0);
    ch->data = s_isFront ? (s8*)s_frontData : (s8*)s_rearData;
    // Apply current pan
    ch->panL = s_panL;
    ch->panR = s_panR;
    set_inc_and_vol(ch, volume0to64, targetFreqHz);
    s_active = true;
}

void EngineSound_update(u32 volume0to64, u32 targetFreqHz)
{
    SOUND_CHANNEL *ch = &sndChannel[ENG_CH];
    if(!s_active || ch->data==0) return;
    set_inc_and_vol(ch, volume0to64, targetFreqHz);
    ch->panL = s_panL; ch->panR = s_panR;
}

void EngineSound_update_volume(u32 volume0to64)
{
    SOUND_CHANNEL *ch = &sndChannel[ENG_CH];
    if(!s_active || ch->data==0) return;
    // Only update volume, don't touch increment (pitch)
    ch->vol = (volume0to64 > 64) ? 64 : volume0to64;
    ch->panL = s_panL; ch->panR = s_panR;
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
    if(s_frontToBack)
        front = !front;  // invert mapping

    // Volume ramps up to midpoint then down (pan set externally at start)
    u32 vol = (frame <= half) ? (frame*64/half) : ((rangeT-frame)*64/half);
    if(vol>64) vol=64;

    // Rear: 2/3 rate; Front: native rate
    u32 hz = front ? s_baseHz : (s_baseHz*2)/3;

    if(frame == 0)
        EngineSound_start(front, vol, hz);
    else if(front != s_isFront)
        EngineSound_start(front, vol, hz);  // switch sample exactly at apex
    else
        EngineSound_update(vol, hz);
}

void EngineSound_set_direction(bool frontToBack)
{
    s_frontToBack = frontToBack;
}

void EngineSound_set_pan(u32 panLeft0to64, u32 panRight0to64)
{
    s_panL = (panLeft0to64>64)?64:panLeft0to64;
    s_panR = (panRight0to64>64)?64:panRight0to64;
}

void EngineSound_set_mix(u32 volFront0to64, u32 volRear0to64,
                         u32 frontFreqHz, u32 rearFreqHz)
{
    // Simple: choose the louder side and set front/rear accordingly
    bool front = volFront0to64 >= volRear0to64;
    u32 vol = front ? volFront0to64 : volRear0to64;
    u32 hz  = front ? frontFreqHz    : rearFreqHz;
    if(!s_active)
        EngineSound_start(front, vol, hz);
    else if(front != s_isFront)
        EngineSound_start(front, vol, hz);
    else
        EngineSound_update(vol, hz);
}


