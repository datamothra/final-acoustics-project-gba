#include "tonc.h"
#include "Sound.h"

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

// External background data (if exists)
extern const unsigned short titleBitmap[];
extern const unsigned short titlePal[];

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
    // Using mode 3 for simplicity (240x160 16-bit bitmap)
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;
    
    // If we have the background image data, copy it
    // The title.bmp should have been converted to a C array
    // For now, just fill with a color to show something
    u16 *vram = (u16*)MEM_VRAM;
    for(int i = 0; i < 240*160; i++) {
        vram[i] = CLR_NAVY;  // Dark blue background
    }
    
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
        
        // Mix audio for next frame
        SndMix();
        
        // Handle input - play notes on button press
        if(key_hit(KEY_A)) {
            // Play middle C (261 Hz)
            sndChannel[0].data = 0;  // Stop first
            sndChannel[0].pos = 0;
            sndChannel[0].inc = (261 << 12) / 18157;  // Frequency to increment
            sndChannel[0].vol = 64;  // Max volume
            sndChannel[0].length = 256 << 12;  // Sample length
            sndChannel[0].loopLength = 256 << 12;  // Loop the whole sample
            sndChannel[0].data = (s8*)sineWaveData;  // Start playing
        }
        
        if(key_hit(KEY_B)) {
            // Play D (293 Hz)
            sndChannel[1].data = 0;
            sndChannel[1].pos = 0;
            sndChannel[1].inc = (293 << 12) / 18157;
            sndChannel[1].vol = 64;
            sndChannel[1].length = 256 << 12;
            sndChannel[1].loopLength = 256 << 12;
            sndChannel[1].data = (s8*)sineWaveData;
        }
        
        if(key_hit(KEY_LEFT)) {
            // Play E (329 Hz)
            sndChannel[2].data = 0;
            sndChannel[2].pos = 0;
            sndChannel[2].inc = (329 << 12) / 18157;
            sndChannel[2].vol = 64;
            sndChannel[2].length = 256 << 12;
            sndChannel[2].loopLength = 256 << 12;
            sndChannel[2].data = (s8*)sineWaveData;
        }
        
        if(key_hit(KEY_RIGHT)) {
            // Play F (349 Hz)
            sndChannel[3].data = 0;
            sndChannel[3].pos = 0;
            sndChannel[3].inc = (349 << 12) / 18157;
            sndChannel[3].vol = 64;
            sndChannel[3].length = 256 << 12;
            sndChannel[3].loopLength = 256 << 12;
            sndChannel[3].data = (s8*)sineWaveData;
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