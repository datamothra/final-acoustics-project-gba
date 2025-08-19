#include "tonc.h"
#include "Sound.h"
#include "title.h"
#include "EngineSound.h"
#include "car_front.h"
#include "car_front_smooth.h"
#include "Prebaked.h"

// A-mode pitch tuning
#define FRONT_SAMPLE_HZ 26000
#define BACK_SAMPLE_HZ  ((FRONT_SAMPLE_HZ * 2) / 3)

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
    // Loop the entire sample for natural engine sound
    u32 loopStart = 0;  // Loop from beginning
    u32 loopLength = audio_car_front_raw_len;  // Loop entire sample (3371 samples)
    EngineSound_init((const s8*)carFront, (const s8*)carFront,
                     (u32)audio_car_front_raw_len, loopStart, loopLength, FRONT_SAMPLE_HZ);
    
    // Enable interrupts
    REG_IME = 1;
    
    // Manual control mode variables
    static bool manualMode = false;
    static int manualX = 120;  // Center of screen (0-240)
    static int manualY = 80;   // Center of screen (0-160)
    // Saved pixels for lightweight dot redraws (avoid full-screen DMA every frame)
    static u16 savedCenter[9];
    static bool centerSaved = false;
    static u16 savedGreen[9];
    static int savedGreenX = -1, savedGreenY = -1;
    
    // Prebaked mode flag
    static bool pbActive = false;

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
            // Reset to center
            if(manualMode) {
                manualX = 120;
                manualY = 80;
            }
            EngineSound_set_pan(64, 64);
            EngineSound_stop();
        }

        // Demo mode removed

        // B toggles prebaked HRTF mode at any time (enters manual control for B-mode)
        if(key_hit(KEY_B)){
            if(!pbActive){
                // Enter B-mode - make sure A-mode is fully stopped
                EngineSound_stop();
                manualMode = false;
                
                // Clean up any existing visuals
                if(centerSaved){
                    int idx=0; for(int dy=-2; dy<=2; dy++) for(int dx=-2; dx<=2; dx++){
                        int px=120+dx, py=80+dy; ((u16*)MEM_VRAM)[py*240+px] = savedCenter[idx++];
                    }
                    centerSaved=false;
                }
                if(savedGreenX>=0){
                    int idx=0; for(int dy=-1; dy<=1; dy++) for(int dx=-1; dx<=1; dx++){
                        int px=savedGreenX+dx, py=savedGreenY+dy; ((u16*)MEM_VRAM)[py*240+px] = savedGreen[idx++];
                    }
                    savedGreenX=-1; savedGreenY=-1;
                }
                
                // Initialize prebaked system (parameters ignored, uses prebaked assets)
                static bool prebaked_initialized = false;
                if(!prebaked_initialized) {
                    Prebaked_init(NULL, 0, 0, 0, 0);
                    prebaked_initialized = true;
                }
                
                // Start prebaked playback
                Prebaked_enable(true);
                Prebaked_update_by_position(120, 80, 64);
                
                // Set up visual mode
                manualMode = true;
                manualX = 120;
                manualY = 80;
                
                // Draw larger head dot (5x5) to represent head width
                int idx=0; for(int dy=-2; dy<=2; dy++) for(int dx=-2; dx<=2; dx++){
                    int px=120+dx, py=80+dy; savedCenter[idx] = ((u16*)MEM_VRAM)[py*240+px]; ((u16*)MEM_VRAM)[py*240+px] = RGB15(31,31,31); idx++; }
                centerSaved=true;
                idx=0; for(int dy=-1; dy<=1; dy++) for(int dx=-1; dx<=1; dx++){
                    int px=manualX+dx, py=manualY+dy; savedGreen[idx] = ((u16*)MEM_VRAM)[py*240+px]; ((u16*)MEM_VRAM)[py*240+px] = RGB15(0,31,0); idx++; }
                savedGreenX=manualX; savedGreenY=manualY;
                pbActive = true;
            }else{
                // Exit B-mode  
                Prebaked_stop();
                pbActive = false;
                manualMode = false;
                
                // Restore visuals
                if(centerSaved){
                    int idx=0; for(int dy=-2; dy<=2; dy++) for(int dx=-2; dx<=2; dx++){
                        int px=120+dx, py=80+dy; ((u16*)MEM_VRAM)[py*240+px] = savedCenter[idx++];
                    }
                    centerSaved=false;
                }
                if(savedGreenX>=0){
                    int idx=0; for(int dy=-1; dy<=1; dy++) for(int dx=-1; dx<=1; dx++){
                        int px=savedGreenX+dx, py=savedGreenY+dy; ((u16*)MEM_VRAM)[py*240+px] = savedGreen[idx++];
                    }
                    savedGreenX=-1; savedGreenY=-1;
                }
            }
        }

        // A toggles A-mode manual (live engine). If B-mode active, disable it and re-init A-mode
        if(key_hit(KEY_A)) {
            if(pbActive){ Prebaked_stop(); Prebaked_enable(false); pbActive=false; }
            manualMode = !manualMode;
            if(manualMode) {
                // Entering manual mode - ensure engine is clean
                EngineSound_stop();
                // Reset position to center
                manualX = 120;
                manualY = 80;
                // Draw center white dot once and save underlying pixels
                int idx=0;
                for(int dy=-1; dy<=1; dy++){
                    for(int dx=-1; dx<=1; dx++){
                        int px=120+dx, py=80+dy;
                        savedCenter[idx] = ((u16*)MEM_VRAM)[py*240+px];
                        ((u16*)MEM_VRAM)[py*240+px] = RGB15(31,31,31);
                        idx++;
                    }
                }
                centerSaved = true;
                // Draw initial green dot and save underlying
                idx=0;
                for(int dy=-1; dy<=1; dy++){
                    for(int dx=-1; dx<=1; dx++){
                        int px=manualX+dx, py=manualY+dy;
                        savedGreen[idx] = ((u16*)MEM_VRAM)[py*240+px];
                        ((u16*)MEM_VRAM)[py*240+px] = RGB15(0,31,0);
                        idx++;
                    }
                }
                savedGreenX = manualX;
                savedGreenY = manualY;
                // Immediately start live engine centered (avoid needing movement to start)
                EngineSound_set_pan(64,64);
                EngineSound_start(true, 64, FRONT_SAMPLE_HZ);
            } else {
                // Exiting manual mode - stop engine
                EngineSound_stop();
                // Restore center dot if drawn
                if(centerSaved){
                    int idx=0;
                    for(int dy=-1; dy<=1; dy++){
                        for(int dx=-1; dx<=1; dx++){
                            int px=120+dx, py=80+dy;
                            ((u16*)MEM_VRAM)[py*240+px] = savedCenter[idx++];
                        }
                    }
                    centerSaved=false;
                }
                // Restore last green dot area
                if(savedGreenX>=0){
                    int idx=0;
                    for(int dy=-1; dy<=1; dy++){
                        for(int dx=-1; dx<=1; dx++){
                            int px=savedGreenX+dx, py=savedGreenY+dy;
                            ((u16*)MEM_VRAM)[py*240+px] = savedGreen[idx++];
                        }
                    }
                    savedGreenX=-1; savedGreenY=-1;
                }
            }
        }
        
        // Manual control mode
        if(manualMode) {
            // D-Pad direct movement (no momentum)
            // Store previous position to detect if we're actually moving
            static int prevX = 120;
            static int prevY = 80;
            
            if(key_is_down(KEY_LEFT))  manualX -= 1;
            if(key_is_down(KEY_RIGHT)) manualX += 1;
            if(key_is_down(KEY_UP))    manualY -= 1;
            if(key_is_down(KEY_DOWN))  manualY += 1;
            
            // Clamp position
            if(manualX < 0) { manualX = 0; }
            if(manualX > 239) { manualX = 239; }
            if(manualY < 0) { manualY = 0; }
            if(manualY > 159) { manualY = 159; }
            
            // Check if position actually changed significantly
            int dx = (manualX > prevX) ? (manualX - prevX) : (prevX - manualX);
            int dy = (manualY > prevY) ? (manualY - prevY) : (prevY - manualY);
            bool positionChanged = (dx >= 1) || (dy >= 1);
            
            // Calculate audio parameters from position
            // X-axis: panning (0=left, 120=center, 240=right)
            u32 panL = (manualX <= 120) ? 64 : (64 * (240 - manualX) / 120);
            u32 panR = (manualX >= 120) ? 64 : (64 * manualX / 120);
            
            // Volume attenuation by distance from head center (white dot at 120,80)
            // Farther left/right also reduces volume (radial falloff)
            // Use a cheap Chebyshev norm approximation to avoid sqrt
            int absXc = (manualX >= 120) ? (manualX - 120) : (120 - manualX);
            int absYc = (manualY >= 80)  ? (manualY - 80)  : (80  - manualY);
            u32 nx = (absXc * 64) / 120;  // 0..64
            u32 ny = (absYc * 64) / 80;   // 0..64
            u32 r  = (nx > ny) ? nx : ny; // 0..64
            u32 volume = (r >= 64) ? 0 : (64 - r);
            
            if(pbActive){
                // B-mode: prebaked stereo with front/back rotation and ILD only
                Prebaked_update_by_position(manualX, manualY, volume);
                // Update green dot visuals when moved
                if(positionChanged){
                    if(savedGreenX>=0){
                        int idx=0;
                        for(int dy=-1; dy<=1; dy++){
                            for(int dx=-1; dx<=1; dx++){
                                int px=savedGreenX+dx, py=savedGreenY+dy;
                                ((u16*)MEM_VRAM)[py*240+px] = savedGreen[idx++];
                            }
                        }
                    }
                    int idx2=0;
                    for(int dy=-1; dy<=1; dy++){
                        for(int dx=-1; dx<=1; dx++){
                            int px=manualX+dx, py=manualY+dy;
                            savedGreen[idx2] = ((u16*)MEM_VRAM)[py*240+px];
                            ((u16*)MEM_VRAM)[py*240+px] = RGB15(0,31,0);
                            idx2++;
                        }
                    }
                    savedGreenX=manualX; savedGreenY=manualY;
                    prevX = manualX; prevY = manualY;
                }
            } else {
                // Live engine path (unchanged vs A-mode)
                // Sound state management
                static bool wasPlaying = false;
                static bool wasFront = true;  // Start with front sample
                
                // Update panning (ILD)
                EngineSound_set_pan(panL, panR);
                
                // Only touch the sound engine when something actually changes
                if(volume > 0) {
                    if(!wasPlaying) {
                        bool useFront = (manualY < 80);
                        u32 targetHz = useFront ? FRONT_SAMPLE_HZ : BACK_SAMPLE_HZ;
                        EngineSound_start(useFront, volume, targetHz);
                        wasPlaying = true;
                        wasFront = useFront;
                        prevX = manualX;
                        prevY = manualY;
                    } else {
                        bool shouldBeFront = (manualY < 70);
                        bool shouldBeBack = (manualY > 90);
                        if((wasFront && shouldBeBack) || (!wasFront && shouldBeFront)) {
                            wasFront = !wasFront;
                            u32 targetHz = wasFront ? FRONT_SAMPLE_HZ : BACK_SAMPLE_HZ;
                            EngineSound_start(wasFront, volume, targetHz);
                            prevX = manualX;
                            prevY = manualY;
                        } else if(positionChanged) {
                            EngineSound_update_volume(volume);
                            prevX = manualX;
                            prevY = manualY;
                            if(savedGreenX>=0){
                                int idx=0;
                                for(int dy=-1; dy<=1; dy++){
                                    for(int dx=-1; dx<=1; dx++){
                                        int px=savedGreenX+dx, py=savedGreenY+dy;
                                        ((u16*)MEM_VRAM)[py*240+px] = savedGreen[idx++];
                                    }
                                }
                            }
                            int idx2=0;
                            for(int dy=-1; dy<=1; dy++){
                                for(int dx=-1; dx<=1; dx++){
                                    int px=manualX+dx, py=manualY+dy;
                                    savedGreen[idx2] = ((u16*)MEM_VRAM)[py*240+px];
                                    ((u16*)MEM_VRAM)[py*240+px] = RGB15(0,31,0);
                                    idx2++;
                                }
                            }
                            savedGreenX=manualX; savedGreenY=manualY;
                        }
                    }
                } else {
                    if(wasPlaying) {
                        EngineSound_stop();
                        wasPlaying = false;
                    }
                }
            }
            
            // Visual redraws happen only on movement; nothing to do here when static
        } else {
            // Non-manual mode: keep only B-mode toggle
            if(key_hit(KEY_B)){
                static bool pb=false; pb=!pb;
                Prebaked_enable(pb);
                if(pb){
                    Prebaked_init((const s8*)carFront, (u32)audio_car_front_raw_len, 0, (u32)audio_car_front_raw_len, 22050);
                    Prebaked_update_by_position(120,80,64);
                }else{
                    Prebaked_stop();
                }
            }
        }
    }
    
    return 0;
}