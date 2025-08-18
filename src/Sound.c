#include "Sound.h"
#include "tonc.h"
#include <string.h>  // For memset

// Based on Deku's sound tutorial
// https://stuij.github.io/deku-sound-tutorial/

// Globals
SOUND_CHANNEL sndChannel[SND_MAX_CHANNELS];
SOUND_VARS sndVars;

// Double buffer for audio mixing (stereo) - 304 samples at 18157 Hz
// Size matches the frequency table entry for 18157 Hz
s8 sndMixBufferL[304 * 2] EWRAM_DATA;
s8 sndMixBufferR[304 * 2] EWRAM_DATA;

// Initialize sound system
void SndInit(void) {
    s32 i;
    
    // Enable sound
    REG_SOUNDCNT_X = SSTAT_ENABLE;
    
    // Configure Direct Sound A (left) and B (right), 100% volume, reset FIFOs
    // Route A to left only, B to right only
    REG_SOUNDCNT_H = SDS_DMG100 | SDS_A100 | SDS_B100 | SDS_AL | SDS_BR | SDS_ATMR0 | SDS_BTMR0 | SDS_ARESET | SDS_BRESET;
    
    // Clear the entire buffer
    i = 0;
    dma3_fill(sndMixBufferL, i, 304 * 2);
    dma3_fill(sndMixBufferR, i, 304 * 2);
    
    // Initialize main sound variables
    sndVars.mixBufferSize = 304;  // For 18157 Hz
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
        sndChannel[i].length = 0;
        sndChannel[i].loopLength = 0;
    }
    
    // Start Timer 0 for 18157 Hz sample rate
    // Timer value from Deku's frequency table
    REG_TM0D = 64612;
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
        // Start playing buffer 0 (left and right)
        REG_DMA1CNT = 0; REG_DMA2CNT = 0;
        REG_DMA1SAD = (u32)sndVars.mixBufferBase;
        REG_DMA2SAD = (u32)sndVars.mixBufferBaseR;
        REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
        REG_DMA2CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
        
        // Set current buffer pointer to buffer 1
        sndVars.curMixBuffer = sndVars.mixBufferBase + sndVars.mixBufferSize;
        sndVars.curMixBufferR = sndVars.mixBufferBaseR + sndVars.mixBufferSize;
        sndVars.activeBuffer = 0;
    } else {  // buffer 0 just finished
        // DMA already points to buffer 1, don't reset it
        
        // Set current buffer pointer to buffer 0
        sndVars.curMixBuffer = sndVars.mixBufferBase;
        sndVars.curMixBufferR = sndVars.mixBufferBaseR;
        sndVars.activeBuffer = 1;
    }
}

// Mix one frame of audio
void SndMix(void) {
    s32 i, curChn;
    
    // 16-bit intermediate buffers to reduce quantization noise
    static s16 tempBufferL[304];
    static s16 tempBufferR[304];
    
    // Zero the buffer
    memset(tempBufferL, 0, 304 * sizeof(s16));
    memset(tempBufferR, 0, 304 * sizeof(s16));
    
    // Mix each channel
    for (curChn = 0; curChn < SND_MAX_CHANNELS; curChn++) {
        SOUND_CHANNEL *chnPtr = &sndChannel[curChn];
        
        // Check if channel is active
        if (chnPtr->data != 0) {
            // Mix this channel's data into the intermediate buffer
            for (i = 0; i < sndVars.mixBufferSize; i++) {
                // Mix a sample into the intermediate buffers with simple ILD panning
                s16 v = (s16)chnPtr->data[chnPtr->pos >> 12];
                // Default pan if not set
                u32 panL = chnPtr->panL ? chnPtr->panL : 64;
                u32 panR = chnPtr->panR ? chnPtr->panR : 64;
                tempBufferL[i] += (v * (s16)chnPtr->vol * (s16)panL) / 64;
                tempBufferR[i] += (v * (s16)chnPtr->vol * (s16)panR) / 64;
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
    
    // Downsample to 8-bit and copy to the playing buffers
    for (i = 0; i < sndVars.mixBufferSize; i++) {
        s32 l = tempBufferL[i] >> 8;
        s32 r = tempBufferR[i] >> 8;
        if(l>127) l=127; if(l<-128) l=-128;
        if(r>127) r=127; if(r<-128) r=-128;
        sndVars.curMixBuffer[i]  = (s8)l;
        sndVars.curMixBufferR[i] = (s8)r;
    }
}
