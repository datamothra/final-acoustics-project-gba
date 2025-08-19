#include "Prebaked.h"
#include "Sound.h"
#include "car_front.h"
#include "test_sine.h"
#include "pb_table.h"
#include "az16_table.h"
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

        // Start centered bucket for az16
        int azIdx = AZ16_COUNT/2;
        const s8* left = (const s8*)az16_L[azIdx];
        const s8* right = (const s8*)az16_R[azIdx];
        u32 len = AZ16_LEN; // use az16 length
        u32 inc = (SND_MIX_RATE_HZ << 12) / SND_MIX_RATE_HZ; if(!inc) inc = 1; // 1:1 playback

        // Left ear hard left
        sndChannel[PB_LEFT_CH].data = (s8*)left;
        sndChannel[PB_LEFT_CH].pos = 0;
        sndChannel[PB_LEFT_CH].inc = inc;
        sndChannel[PB_LEFT_CH].vol = 32;
        sndChannel[PB_LEFT_CH].panL = 64;
        sndChannel[PB_LEFT_CH].panR = 0;
        sndChannel[PB_LEFT_CH].length = len << 12;
        sndChannel[PB_LEFT_CH].loopLength = len << 12;

        // Right ear hard right
        sndChannel[PB_RIGHT_CH].data = (s8*)right;
        sndChannel[PB_RIGHT_CH].pos = 0;
        sndChannel[PB_RIGHT_CH].inc = inc;
        sndChannel[PB_RIGHT_CH].vol = 32;
        sndChannel[PB_RIGHT_CH].panL = 0;
        sndChannel[PB_RIGHT_CH].panR = 64;
        sndChannel[PB_RIGHT_CH].length = len << 12;
        sndChannel[PB_RIGHT_CH].loopLength = len << 12;
    } else {
        sndChannel[PB_LEFT_CH].data = 0;
        sndChannel[PB_RIGHT_CH].data = 0;
    }
}

void Prebaked_update_by_position(int x, int y, u32 volume0to64){
    if(!s_enabled) return;
    // Map X to az16 azimuth bucket
    int azIdx = (x < 120) ? ((120 - x) * (AZ16_COUNT/2) / 120) : ((x - 120) * (AZ16_COUNT/2) / 120 + AZ16_COUNT/2);
    if(azIdx < 0) azIdx = 0; if(azIdx >= AZ16_COUNT) azIdx = AZ16_COUNT-1;
    const s8* left = (const s8*)az16_L[azIdx];
    const s8* right = (const s8*)az16_R[azIdx];
    u32 len = AZ16_LEN;
    // If pointer changed, swap at loop boundary by resetting pos together
    if(sndChannel[PB_LEFT_CH].data != left || sndChannel[PB_RIGHT_CH].data != right){
        sndChannel[PB_LEFT_CH].data = (s8*)left;
        sndChannel[PB_RIGHT_CH].data = (s8*)right;
        sndChannel[PB_LEFT_CH].pos = 0;
        sndChannel[PB_RIGHT_CH].pos = 0;
        sndChannel[PB_LEFT_CH].length = len << 12;
        sndChannel[PB_RIGHT_CH].length = len << 12;
        sndChannel[PB_LEFT_CH].loopLength = len << 12;
        sndChannel[PB_RIGHT_CH].loopLength = len << 12;
    }
    
    // Volume control applies equally to both
    u32 vol = (volume0to64 > 64) ? 64 : volume0to64;
    sndChannel[PB_LEFT_CH].vol = vol;
    sndChannel[PB_RIGHT_CH].vol = vol;
}

void Prebaked_stop(void){
    s_enabled = false;
    sndChannel[PB_LEFT_CH].data = 0;
    sndChannel[PB_RIGHT_CH].data = 0;
}