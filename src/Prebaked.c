#include "Prebaked.h"
#include "Sound.h"
#include "car_front.h"
#include "test_sine.h"
#include <string.h>

// B-mode: True stereo test using two hard-panned channels with different tones

#define PB_LEFT_CH  1
#define PB_RIGHT_CH 2

static bool s_enabled = false;

void Prebaked_init(const s8* baseData, u32 lengthSamples, u32 loopStart, u32 loopLength, u32 baseHz){
    (void)baseData; (void)lengthSamples; (void)loopStart; (void)loopLength; (void)baseHz;
    
    (void)baseData; (void)lengthSamples; (void)loopStart; (void)loopLength; (void)baseHz;
}

void Prebaked_enable(bool enable){
    s_enabled = enable;
    
    if(s_enabled){
        // Clear other channels
        sndChannel[0].data = 0;
        sndChannel[3].data = 0;

        u32 inc = (SND_MIX_RATE_HZ << 12) / SND_MIX_RATE_HZ; if(!inc) inc = 1; // 1:1 at mixer rate

        // Left: 440 Hz sine, hard left
        sndChannel[PB_LEFT_CH].data = (s8*)test_sine_left;
        sndChannel[PB_LEFT_CH].pos = 0;
        sndChannel[PB_LEFT_CH].inc = inc;
        sndChannel[PB_LEFT_CH].vol = 32;
        sndChannel[PB_LEFT_CH].panL = 64;
        sndChannel[PB_LEFT_CH].panR = 0;
        sndChannel[PB_LEFT_CH].length = TEST_SINE_LEN_LEFT << 12;
        sndChannel[PB_LEFT_CH].loopLength = TEST_SINE_LEN_LEFT << 12;

        // Right: 660 Hz sine, hard right
        sndChannel[PB_RIGHT_CH].data = (s8*)test_sine_right;
        sndChannel[PB_RIGHT_CH].pos = 0;
        sndChannel[PB_RIGHT_CH].inc = inc;
        sndChannel[PB_RIGHT_CH].vol = 32;
        sndChannel[PB_RIGHT_CH].panL = 0;
        sndChannel[PB_RIGHT_CH].panR = 64;
        sndChannel[PB_RIGHT_CH].length = TEST_SINE_LEN_RIGHT << 12;
        sndChannel[PB_RIGHT_CH].loopLength = TEST_SINE_LEN_RIGHT << 12;
    } else {
        sndChannel[PB_LEFT_CH].data = 0;
        sndChannel[PB_RIGHT_CH].data = 0;
    }
}

void Prebaked_update_by_position(int x, int y, u32 volume0to64){
    if(!s_enabled) return;
    
    // Volume control applies equally to both
    u32 vol = (volume0to64 > 64) ? 64 : volume0to64;
    sndChannel[PB_LEFT_CH].vol = vol;
    sndChannel[PB_RIGHT_CH].vol = vol;

    // Optional: pan crossfade test (commented out for pure stereo)
    (void)x; (void)y;
}

void Prebaked_stop(void){
    s_enabled = false;
    sndChannel[PB_LEFT_CH].data = 0;
    sndChannel[PB_RIGHT_CH].data = 0;
}