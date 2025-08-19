#include "Sound.h"
#include "tonc.h"
#include <string.h>  // For memset

// Based on Deku's sound tutorial and Sappy documentation

// Globals
SOUND_CHANNEL sndChannel[SND_MAX_CHANNELS];
SOUND_VARS sndVars;

// Mixer configuration - Sappy common rate 18157 Hz (per sappy.txt)
#define MIX_RATE_HZ SND_MIX_RATE_HZ
#define BUFFER_SIZE 303  // 18157/60 ≈ 302.6; use 303

// Double buffer for audio mixing (stereo)
s8 sndMixBufferL[BUFFER_SIZE * 2] EWRAM_DATA ALIGN(4);
s8 sndMixBufferR[BUFFER_SIZE * 2] EWRAM_DATA ALIGN(4);

// Intermediate 32-bit accumulators in EWRAM (large); keep code in IWRAM
static s32 tempBufferL[BUFFER_SIZE] EWRAM_DATA;
static s32 tempBufferR[BUFFER_SIZE] EWRAM_DATA;

// Initialize sound system
void SndInit(void) {
    s32 i;
    
    // Enable sound
    REG_SOUNDCNT_X = SSTAT_ENABLE;
    // Set sound bias per Sappy standard
    REG_SOUNDBIAS = 0x0200;
    
    // Configure Direct Sound A (left) and B (right), 100% volume, reset FIFOs
    REG_SOUNDCNT_H = SDS_DMG100 | SDS_A100 | SDS_B100 | SDS_AL | SDS_BR | SDS_ATMR0 | SDS_BTMR0 | SDS_ARESET | SDS_BRESET;
    
    // Clear buffers
    memset(sndMixBufferL, 0, sizeof(sndMixBufferL));
    memset(sndMixBufferR, 0, sizeof(sndMixBufferR));
    
    // Initialize main sound variables
    sndVars.mixBufferSize = BUFFER_SIZE;
    sndVars.mixBufferBase = sndMixBufferL;
    sndVars.curMixBuffer = sndVars.mixBufferBase;
    sndVars.mixBufferBaseR = sndMixBufferR;
    sndVars.curMixBufferR = sndMixBufferR;
    // Start with activeBuffer=1 so first VBlank arms buffer 0 to play, and we mix into buffer 1
    sndVars.activeBuffer = 1;
    
    // Initialize channel structures
    for (i = 0; i < SND_MAX_CHANNELS; i++) {
        sndChannel[i].data = 0;
        sndChannel[i].pos = 0;
        sndChannel[i].inc = 0;
        sndChannel[i].vol = 0;
        sndChannel[i].panL = 64;
        sndChannel[i].panR = 64;
        sndChannel[i].length = 0;
        sndChannel[i].loopLength = 0;
    }
    
    // Start Timer 0 for sample rate
    REG_TM0D = 65536 - (16777216 / MIX_RATE_HZ);
    REG_TM0CNT = TM_ENABLE;
    
    // Prepare DMA channels: will be re-initialized each VBlank with correct buffer half
    REG_DMA1CNT = 0; REG_DMA2CNT = 0;
    REG_DMA1DAD = (u32)&REG_FIFO_A;
    REG_DMA2DAD = (u32)&REG_FIFO_B;
}

// VBlank interrupt handler - restart DMA for the buffer that will play next and swap mix pointers
IWRAM_CODE void SndVSync(void) {
    if (sndVars.activeBuffer == 1) {
        // buffer 1 was playing; switch to buffer 0 for output
        REG_DMA1CNT = 0; REG_DMA2CNT = 0;
        REG_DMA1SAD = (u32)sndVars.mixBufferBase;
        REG_DMA2SAD = (u32)sndVars.mixBufferBaseR;
        REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | 4 | DMA_ENABLE;
        REG_DMA2CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | 4 | DMA_ENABLE;
        // Mix into buffer 1 next
        sndVars.curMixBuffer  = sndVars.mixBufferBase  + sndVars.mixBufferSize;
        sndVars.curMixBufferR = sndVars.mixBufferBaseR + sndVars.mixBufferSize;
        sndVars.activeBuffer = 0;
    } else {
        // buffer 0 was playing; switch to buffer 1 for output
        REG_DMA1CNT = 0; REG_DMA2CNT = 0;
        REG_DMA1SAD = (u32)(sndVars.mixBufferBase  + sndVars.mixBufferSize);
        REG_DMA2SAD = (u32)(sndVars.mixBufferBaseR + sndVars.mixBufferSize);
        REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | 4 | DMA_ENABLE;
        REG_DMA2CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | 4 | DMA_ENABLE;
        // Mix into buffer 0 next
        sndVars.curMixBuffer  = sndVars.mixBufferBase;
        sndVars.curMixBufferR = sndVars.mixBufferBaseR;
        sndVars.activeBuffer = 1;
    }
}

// Mix one frame of audio (interpolated, 32-bit headroom, dynamic downscale)
IWRAM_CODE void SndMix(void) {
    s32 i, curChn;
    u32 activeCount = 0;

    // Zero accumulators
    memset(tempBufferL, 0, sndVars.mixBufferSize * sizeof(s32));
    memset(tempBufferR, 0, sndVars.mixBufferSize * sizeof(s32));

    // Mix each channel with linear interpolation
    for (curChn = 0; curChn < SND_MAX_CHANNELS; curChn++) {
        SOUND_CHANNEL *chnPtr = &sndChannel[curChn];
        if (chnPtr->data == 0) continue;
        activeCount++;

        for (i = 0; i < sndVars.mixBufferSize; i++) {
            u32 p = chnPtr->pos;
            u32 idx = p >> 12; 
            u32 frac = p & 0xFFF;
            u32 endSamp = (chnPtr->length >> 12);
            if (endSamp == 0) break;
            if (idx >= endSamp) idx = endSamp - 1;

            s32 s0 = (s32)((s16)chnPtr->data[idx]);
            s32 s1;
            if (idx + 1 < endSamp)
                s1 = (s32)((s16)chnPtr->data[idx + 1]);
            else
                s1 = (s32)((s16)chnPtr->data[0]);  // seamless interpolation across loop
            s32 v = s0 + (((s1 - s0) * (s32)frac) >> 12);

            // ILD pan
            tempBufferL[i] += (v * (s32)chnPtr->vol * (s32)chnPtr->panL);
            tempBufferR[i] += (v * (s32)chnPtr->vol * (s32)chnPtr->panR);

            chnPtr->pos += chnPtr->inc;

            // Loop / stop
            if (chnPtr->pos >= chnPtr->length) {
                if (chnPtr->loopLength == 0) {
                    chnPtr->data = 0;
                    break;
                } else {
                    while (chnPtr->pos >= chnPtr->length) {
                        chnPtr->pos -= chnPtr->loopLength;
                    }
                }
            }
        }
    }

    // Dynamic scale: 1ch => >>12, 2ch => >>13, 3+ => >>14 (since we multiplied by vol(<=64) and pan(<=64))
    u32 shift = (activeCount <= 1) ? 12 : (activeCount == 2 ? 13 : 14);

    // Downmix to 8-bit with rounding and clamp
    for (i = 0; i < sndVars.mixBufferSize; i++) {
        s32 lAcc = tempBufferL[i];
        s32 rAcc = tempBufferR[i];
        // Round-to-nearest before shifting
        if (lAcc >= 0) lAcc += (1 << (shift-1)); else lAcc -= (1 << (shift-1));
        if (rAcc >= 0) rAcc += (1 << (shift-1)); else rAcc -= (1 << (shift-1));

        s32 l = lAcc >> shift;
        s32 r = rAcc >> shift;
        if (l > 127) l = 127;
        if (l < -128) l = -128;
        if (r > 127) r = 127;
        if (r < -128) r = -128;
        sndVars.curMixBuffer[i]  = (s8)l;
        sndVars.curMixBufferR[i] = (s8)r;
    }
}