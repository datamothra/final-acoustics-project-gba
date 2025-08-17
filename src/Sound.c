#include "Sound.h"
#include "tonc.h"
#include <string.h>  // For memset

// Based on Deku's sound tutorial
// https://stuij.github.io/deku-sound-tutorial/

// Globals
SOUND_CHANNEL sndChannel[SND_MAX_CHANNELS];
SOUND_VARS sndVars;

// Double buffer for audio mixing - 304 samples at 18157 Hz
// Size matches the frequency table entry for 18157 Hz
s8 sndMixBuffer[304 * 2] EWRAM_DATA;

// Initialize sound system
void SndInit(void) {
    s32 i;
    
    // Enable sound
    REG_SOUNDCNT_X = SSTAT_ENABLE;
    
    // Configure Direct Sound A: output to both L/R at 100% volume, reset FIFO
    REG_SOUNDCNT_H = SDS_DMG100 | SDS_A100 | SDS_AR | SDS_AL | SDS_ATMR0 | SDS_ARESET;
    
    // Clear the entire buffer
    i = 0;
    dma3_fill(sndMixBuffer, i, 304 * 2);
    
    // Initialize main sound variables
    sndVars.mixBufferSize = 304;  // For 18157 Hz
    sndVars.mixBufferBase = sndMixBuffer;
    sndVars.curMixBuffer = sndVars.mixBufferBase;
    sndVars.activeBuffer = 1;  // 1 so first swap will start DMA
    
    // Initialize channel structures
    for (i = 0; i < SND_MAX_CHANNELS; i++) {
        sndChannel[i].data = 0;
        sndChannel[i].pos = 0;
        sndChannel[i].inc = 0;
        sndChannel[i].vol = 0;
        sndChannel[i].length = 0;
        sndChannel[i].loopLength = 0;
    }
    
    // Start Timer 0 for 18157 Hz sample rate
    // Timer value from Deku's frequency table
    REG_TM0D = 64612;
    REG_TM0CNT = TM_ENABLE;
    
    // Set up DMA1 for Direct Sound A, but let VBlank start it
    REG_DMA1CNT = 0;
    REG_DMA1DAD = (u32)&REG_FIFO_A;
}

// VBlank interrupt handler - swaps buffers
void SndVSync(void) {
    if (sndVars.activeBuffer == 1) {  // buffer 1 just finished
        // Start playing buffer 0
        REG_DMA1CNT = 0;
        REG_DMA1SAD = (u32)sndVars.mixBufferBase;
        REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
        
        // Set current buffer pointer to buffer 1
        sndVars.curMixBuffer = sndVars.mixBufferBase + sndVars.mixBufferSize;
        sndVars.activeBuffer = 0;
    } else {  // buffer 0 just finished
        // DMA already points to buffer 1, don't reset it
        
        // Set current buffer pointer to buffer 0
        sndVars.curMixBuffer = sndVars.mixBufferBase;
        sndVars.activeBuffer = 1;
    }
}

// Mix one frame of audio
void SndMix(void) {
    s32 i, curChn;
    
    // 16-bit intermediate buffer to reduce quantization noise
    static s16 tempBuffer[304];
    
    // Zero the buffer
    memset(tempBuffer, 0, 304 * sizeof(s16));
    
    // Mix each channel
    for (curChn = 0; curChn < SND_MAX_CHANNELS; curChn++) {
        SOUND_CHANNEL *chnPtr = &sndChannel[curChn];
        
        // Check if channel is active
        if (chnPtr->data != 0) {
            // Mix this channel's data into the intermediate buffer
            for (i = 0; i < sndVars.mixBufferSize; i++) {
                // Mix a sample into the intermediate buffer
                tempBuffer[i] += (s16)chnPtr->data[chnPtr->pos >> 12] * (s16)chnPtr->vol;
                chnPtr->pos += chnPtr->inc;
                
                // Handle looping or stopping at the end
                if (chnPtr->pos >= chnPtr->length) {
                    // Check for loop
                    if (chnPtr->loopLength == 0) {
                        // No loop - disable channel and break
                        chnPtr->data = 0;
                        i = sndVars.mixBufferSize;
                    } else {
                        // Loop back
                        while (chnPtr->pos >= chnPtr->length) {
                            chnPtr->pos -= chnPtr->loopLength;
                        }
                    }
                }
            }
        }
    }
    
    // Downsample to 8-bit and copy to the playing buffer
    for (i = 0; i < sndVars.mixBufferSize; i++) {
        // >>6 for volume, >>2 for 4 channels to prevent overflow
        sndVars.curMixBuffer[i] = tempBuffer[i] >> 8;
    }
}
