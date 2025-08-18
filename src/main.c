#include "tonc.h"
#include "Sound.h"
#include "title.h"
#include "EngineSound.h"
#include "car_front.h"
#include "car_front_smooth.h"

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

    // Prepare front sample from embedded raw (unsigned -> signed)
    static s8 carFront[4096];
    static s8 carFrontSmooth[4096];
    static bool inited=false;
    if(!inited){
        for(size_t i=0;i<audio_car_front_raw_len && i<sizeof(carFront);i++) carFront[i]=(s8)((int)audio_car_front_raw[i]-128);
        // build smoothed copy (unsigned->signed)
        for(size_t i=0;i<audio_car_front_smooth_raw_len && i<sizeof(carFrontSmooth);i++) carFrontSmooth[i]=(s8)((int)audio_car_front_smooth_raw[i]-128);
        inited=true;
    }
    // Rear is pitched-down a fifth (3/2 -> 2/3 frequency). We'll apply via targetFreq later
    // Use original longer sample with better loop points
    u32 loopStart = 1500;  // Start loop partway into sample to skip attack
    u32 loopLength = audio_car_front_raw_len - loopStart;  // Loop the rest (~1871 samples)
    EngineSound_init((const s8*)carFront, (const s8*)carFront,
                     (u32)audio_car_front_raw_len, loopStart, loopLength, 22050);
    
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
        
        // Remove note test controls

        // Reserve channel 3 for engine sound demo
        
        if(key_hit(KEY_SELECT)) {
            // Stop all channels
            for(int i = 0; i < SND_MAX_CHANNELS; i++) {
                sndChannel[i].data = 0;
            }
        }

        // Demo: press START to run a simple gradual pass-by
        // Pan selection via D-Pad: LEFT = hard left, UP = center, RIGHT = hard right
        // L/R: dynamic pan for duration of sweep (L: left->right, R: right->left)
        static u32 demoFrame=0; static bool demoOn=false;
        static int panSel = 1; // 0=left, 1=center, 2=right
        static int panDynamicMode = 0; // 0=none, 1=L->R, 2=R->L
        static bool nextFrontToBack = false; // false: back->front (default), true: front->back

        // D-Pad for static pan positions
        if(key_hit(KEY_LEFT)) { 
            panSel = 0; 
            panDynamicMode = 0;  // Reset dynamic mode
            EngineSound_set_pan(64,0); 
        }
        if(key_hit(KEY_RIGHT)) { 
            panSel = 2; 
            panDynamicMode = 0;  // Reset dynamic mode
            EngineSound_set_pan(0,64); 
        }
        
        // UP resets to center and stops dynamic panning
        if(key_hit(KEY_UP)) { 
            panSel = 1; 
            panDynamicMode = 0;  // Reset dynamic mode
            EngineSound_set_pan(64,64); 
        }
        
        // L/R trigger dynamic pan modes (press once, not hold)
        if(key_hit(KEY_L)) { 
            panDynamicMode = 1;  // Start L->R sweep
        }
        if(key_hit(KEY_R)) { 
            panDynamicMode = 2;  // Start R->L sweep
        }

        // B toggles direction for the NEXT sweep only
        if(key_hit(KEY_B)) nextFrontToBack = !nextFrontToBack;

        if(key_hit(KEY_START)) {
            if(!demoOn){
                // starting a new sweep: apply stored direction and current pan
                EngineSound_set_direction(nextFrontToBack);
                if(panDynamicMode==0){
                    if(panSel==0) EngineSound_set_pan(64,0); else if(panSel==1) EngineSound_set_pan(64,64); else EngineSound_set_pan(0,64);
                }
                demoFrame=0; demoOn=true;
            }else{
                // stopping current sweep
                demoOn=false; EngineSound_stop();
            }
        }

        if(demoOn){
            // If dynamic pan requested, move across over the sweep duration
            if(panDynamicMode!=0){
                const u32 total=360; u32 f = demoFrame<total?demoFrame:total; // clamp
                // t in [0,1]
                u32 t_num = f; u32 t_den = total; // integer form
                // left gain is either (1-t) or t; right is complement
                u32 left, right;
                if(panDynamicMode==1){ // L->R
                    left  = (u32)((64ULL*(t_den - t_num))/t_den);
                    right = 64 - left;
                }else{ // R->L
                    right = (u32)((64ULL*(t_den - t_num))/t_den);
                    left  = 64 - right;
                }
                EngineSound_set_pan(left, right);
            }
            EngineSound_demo_update_passby(demoFrame++, 360);
            if(demoFrame>360){ demoOn=false; EngineSound_stop(); }
        }

        // A toggles sweep direction: front->back vs back->front
        static bool dirFTB=false; // false=back->front default
        if(key_hit(KEY_A)) { dirFTB = !dirFTB; EngineSound_set_direction(dirFTB); }
    }
    
    return 0;
}