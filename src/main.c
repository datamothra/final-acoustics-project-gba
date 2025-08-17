#include "tonc.h"
#include "Sound.h"
#include "title.h"

// Simple sine wave sample for testing (256 samples, one period)
const s8 sineWaveData[] = {
    0, 3, 6, 9, 12, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46,
    49, 52, 54, 57, 60, 63, 65, 68, 71, 73, 76, 78, 81, 83, 86, 88,
    90, 92, 95, 97, 99, 101, 103, 105, 107, 108, 110, 112, 113, 115, 116, 118,
    119, 121, 122, 123, 124, 125, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 126, 125, 124,
    123, 122, 121, 119, 118, 116, 115, 113, 112, 110, 108, 107, 105, 103, 101, 99,
    97, 95, 92, 90, 88, 86, 83, 81, 78, 76, 73, 71, 68, 65, 63, 60,
    57, 54, 52, 49, 46, 43, 40, 37, 34, 31, 28, 25, 22, 19, 16, 12,
    9, 6, 3, 0, -3, -6, -9, -12, -16, -19, -22, -25, -28, -31, -34, -37,
    -40, -43, -46, -49, -52, -54, -57, -60, -63, -65, -68, -71, -73, -76, -78, -81,
    -83, -86, -88, -90, -92, -95, -97, -99, -101, -103, -105, -107, -108, -110, -112, -113,
    -115, -116, -118, -119, -121, -122, -123, -124, -125, -126, -127, -127, -128, -128, -128, -128,
    -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127,
    -127, -126, -125, -124, -123, -122, -121, -119, -118, -116, -115, -113, -112, -110, -108, -107,
    -105, -103, -101, -99, -97, -95, -92, -90, -88, -86, -83, -81, -78, -76, -73, -71,
    -68, -65, -63, -60, -57, -54, -52, -49, -46, -43, -40, -37, -34, -31, -28, -25,
    -22, -19, -16, -12, -9, -6, -3
};

// External background data provided by grit

// VBlank interrupt handler
void vblank_handler(void) {
    // Call sound VSync to swap buffers
    SndVSync();
}

int main(void) {
    // Initialize interrupts
    irq_init(NULL);
    irq_add(II_VBLANK, vblank_handler);
    
    // Try to set up background image if it exists
    // Using mode 3 for 240x160 16-bit bitmap
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

    // Copy converted linear bitmap (Mode 3): titleBitmapLen is bytes, dma3_cpy expects bytes
    u16 *vram = (u16*)MEM_VRAM;
    dma3_cpy(vram, titleBitmap, titleBitmapLen);
    
    // Initialize sound system
    SndInit();
    
    // Enable interrupts
    REG_IME = 1;
    
    // Main loop
    while(1) {
        // Wait for VBlank
        VBlankIntrWait();
        
        // Update input
        key_poll();
        
        // Mix audio for next frame (Deku Day 2 pattern)
        SndMix();
        
        // Handle input - notes play while held
        // Channel 0: A - C (261 Hz)
        if(key_is_down(KEY_A)) {
            if(sndChannel[0].data == 0) {
                sndChannel[0].pos = 0;
                sndChannel[0].inc = (261 * (256 << 12)) / 18157;
                sndChannel[0].vol = 64;
                sndChannel[0].length = 256 << 12;
                sndChannel[0].loopLength = 256 << 12;
                sndChannel[0].data = (s8*)sineWaveData;
            }
        } else if(sndChannel[0].data != 0) {
            sndChannel[0].data = 0; // stop when released
        }

        // Channel 1: B - D (293 Hz)
        if(key_is_down(KEY_B)) {
            if(sndChannel[1].data == 0) {
                sndChannel[1].pos = 0;
                sndChannel[1].inc = (293 * (256 << 12)) / 18157;
                sndChannel[1].vol = 64;
                sndChannel[1].length = 256 << 12;
                sndChannel[1].loopLength = 256 << 12;
                sndChannel[1].data = (s8*)sineWaveData;
            }
        } else if(sndChannel[1].data != 0) {
            sndChannel[1].data = 0;
        }

        // Channel 2: LEFT - E (329 Hz)
        if(key_is_down(KEY_LEFT)) {
            if(sndChannel[2].data == 0) {
                sndChannel[2].pos = 0;
                sndChannel[2].inc = (329 * (256 << 12)) / 18157;
                sndChannel[2].vol = 64;
                sndChannel[2].length = 256 << 12;
                sndChannel[2].loopLength = 256 << 12;
                sndChannel[2].data = (s8*)sineWaveData;
            }
        } else if(sndChannel[2].data != 0) {
            sndChannel[2].data = 0;
        }

        // Channel 3: RIGHT - F (349 Hz)
        if(key_is_down(KEY_RIGHT)) {
            if(sndChannel[3].data == 0) {
                sndChannel[3].pos = 0;
                sndChannel[3].inc = (349 * (256 << 12)) / 18157;
                sndChannel[3].vol = 64;
                sndChannel[3].length = 256 << 12;
                sndChannel[3].loopLength = 256 << 12;
                sndChannel[3].data = (s8*)sineWaveData;
            }
        } else if(sndChannel[3].data != 0) {
            sndChannel[3].data = 0;
        }
        
        if(key_hit(KEY_SELECT)) {
            // Stop all channels
            for(int i = 0; i < SND_MAX_CHANNELS; i++) {
                sndChannel[i].data = 0;
            }
        }
    }
    
    return 0;
}