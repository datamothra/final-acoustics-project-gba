#include "Sound.h"
#include "tonc.h"
#include <string.h>  // For memset

// Based on Deku's sound tutorial
// https://stuij.github.io/deku-sound-tutorial/

// Globals
SOUND_CHANNEL sndChannel[SND_MAX_CHANNELS];
SOUND_VARS sndVars;

// Mixer configuration - Sappy standard rate 31536 Hz (rate 9)
#define MIX_RATE_HZ 31536
#define BUFFER_SIZE 526  // 31536/60 ≈ 525.6 samples per frame

// Double buffer for audio mixing (stereo)
s8 sndMixBufferL[BUFFER_SIZE * 2] EWRAM_DATA;
s8 sndMixBufferR[BUFFER_SIZE * 2] EWRAM_DATA;

// Initialize sound system
void SndInit(void) {
    s32 i;
    
    // Enable sound
    REG_SOUNDCNT_X = SSTAT_ENABLE;
    // Set sound bias per GBATEK/Sappy practice
    REG_SOUNDBIAS = 0x0200;
    
    // Configure Direct Sound A (left) and B (right), 100% volume, reset FIFOs
    // Route A to left only, B to right only
    REG_SOUNDCNT_H = SDS_DMG100 | SDS_A100 | SDS_B100 | SDS_AL | SDS_BR | SDS_ATMR0 | SDS_BTMR0 | SDS_ARESET | SDS_BRESET;
    
    // Clear the entire buffers
    i = 0;
    dma3_fill(sndMixBufferL, i, sizeof(sndMixBufferL));
    dma3_fill(sndMixBufferR, i, sizeof(sndMixBufferR));
    
    // Initialize main sound variables
    sndVars.mixBufferSize = BUFFER_SIZE;
    sndVars.mixBufferBase = sndMixBufferL;
    sndVars.curMixBuffer = sndVars.mixBufferBase;
    sndVars.mixBufferBaseR = sndMixBufferR;
    sndVars.curMixBufferR = sndMixBufferR;
    sndVars.activeBuffer = 1;  // 1 so first swap will start DMA
    
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
    
    // Start Timer 0 for ~32000 Hz sample rate
    REG_TM0D = 65536 - (16777216 / MIX_RATE_HZ);
    REG_TM0CNT = TM_ENABLE;
    
    // Set up DMA1 for Direct Sound A (left), DMA2 for Direct Sound B (right), but let VBlank start them
    REG_DMA1CNT = 0;
    REG_DMA1DAD = (u32)&REG_FIFO_A;
    REG_DMA2CNT = 0;
    REG_DMA2DAD = (u32)&REG_FIFO_B;
}

// VBlank interrupt handler - swaps buffers
void SndVSync(void) {
    if (sndVars.activeBuffer == 1) {  // buffer 1 just finished
        // Restart DMA for buffer 0 (Sappy-style: restart each buffer swap)
        REG_DMA1CNT = 0; 
        REG_DMA2CNT = 0;
        REG_DMA1SAD = (u32)sndVars.mixBufferBase;
        REG_DMA2SAD = (u32)sndVars.mixBufferBaseR;
        REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
        REG_DMA2CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
        
        // Set current buffer pointer to buffer 1
        sndVars.curMixBuffer = sndVars.mixBufferBase + sndVars.mixBufferSize;
        sndVars.curMixBufferR = sndVars.mixBufferBaseR + sndVars.mixBufferSize;
        sndVars.activeBuffer = 0;
    } else {  // buffer 0 just finished
        // Restart DMA for buffer 1
        REG_DMA1CNT = 0;
        REG_DMA2CNT = 0;
        REG_DMA1SAD = (u32)(sndVars.mixBufferBase + sndVars.mixBufferSize);
        REG_DMA2SAD = (u32)(sndVars.mixBufferBaseR + sndVars.mixBufferSize);
        REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
        REG_DMA2CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
        
        // Set current buffer pointer to buffer 0
        sndVars.curMixBuffer = sndVars.mixBufferBase;
        sndVars.curMixBufferR = sndVars.mixBufferBaseR;
        sndVars.activeBuffer = 1;
    }
}

// Mix one frame of audio
void SndMix(void) {
    s32 i, curChn;
    u32 activeCount = 0;
    
    // 32-bit intermediate buffers for headroom (then scale down)
    static s32 tempBufferL[BUFFER_SIZE];
    static s32 tempBufferR[BUFFER_SIZE];
    
    // Zero the buffer
    memset(tempBufferL, 0, sndVars.mixBufferSize * sizeof(s32));
    memset(tempBufferR, 0, sndVars.mixBufferSize * sizeof(s32));
    
    // Mix each channel
    for (curChn = 0; curChn < SND_MAX_CHANNELS; curChn++) {
        SOUND_CHANNEL *chnPtr = &sndChannel[curChn];
        
        // Check if channel is active
        if (chnPtr->data != 0) {
            activeCount++;
            // Mix this channel's data into the intermediate buffer
            for (i = 0; i < sndVars.mixBufferSize; i++) {
                // Mix a sample into the intermediate buffers with simple ILD panning
                // Linear interpolation (Sappy-style: no wrap on interpolation)
                u32 p = chnPtr->pos;
                u32 idx = p >> 12;
                u32 frac = p & 0xFFF; // 12-bit fraction
                
                // Get current sample
                s32 s0 = (s32)((s16)chnPtr->data[idx]);
                // For interpolation, check if we can safely read next sample
                s32 s1 = s0;  // Default to current if at end
                if (idx + 1 < (chnPtr->length >> 12)) {
                    s1 = (s32)((s16)chnPtr->data[idx + 1]);
                }
                s32 v = s0 + (((s1 - s0) * (s32)frac) >> 12);
                tempBufferL[i] += (v * (s32)chnPtr->vol * (s32)chnPtr->panL) / 64;
                tempBufferR[i] += (v * (s32)chnPtr->vol * (s32)chnPtr->panR) / 64;
                chnPtr->pos += chnPtr->inc;
                
                // Handle looping or stopping at the end
                if (chnPtr->pos >= chnPtr->length) {
                    // Check for loop
                    if (chnPtr->loopLength == 0) {
                        // No loop - disable channel and break
                        chnPtr->data = 0;
                        i = sndVars.mixBufferSize;
                    } else {
                        // Loop back - subtract loop length to maintain phase
                        while (chnPtr->pos >= chnPtr->length) {
                            chnPtr->pos -= chnPtr->loopLength;
                        }
                    }
                }
            }
        }
    }
    
    // Choose dynamic scaling based on number of active channels to use full range
    // 1 ch => >>6, 2 ch => >>7, 3+ ch => >>8
    u32 shift = (activeCount <= 1) ? 6 : (activeCount == 2 ? 7 : 8);

    // Downsample to 8-bit with rounding and copy to the playing buffers
    for (i = 0; i < sndVars.mixBufferSize; i++) {
        s32 lAcc = tempBufferL[i];
        s32 rAcc = tempBufferR[i];
        // Round-to-nearest before shifting
        if(lAcc >= 0) lAcc += (1 << (shift-1)); else lAcc -= (1 << (shift-1));
        if(rAcc >= 0) rAcc += (1 << (shift-1)); else rAcc -= (1 << (shift-1));

        s32 l = lAcc >> shift;
        s32 r = rAcc >> shift;
        if(l > 127) { l = 127; }
        if(l < -128) { l = -128; }
        if(r > 127) { r = 127; }
        if(r < -128) { r = -128; }
        sndVars.curMixBuffer[i]  = (s8)l;
        sndVars.curMixBufferR[i] = (s8)r;
    }
}
