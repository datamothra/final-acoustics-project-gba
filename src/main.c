#include "tonc.h"
#include "sappy_engine.h"

// External functions from sappy_samples.c
extern SappyInstrument gPianoInstrument;
extern SappyInstrument gOrganInstrument;
extern SappyInstrument gDrumInstrument;
extern void sappy_init_samples(void);

// Background image data (we'll keep using the converted image)
extern const unsigned short titleBitmap[];
extern const unsigned short titlePal[];

// Current instrument selection
static int currentInstrument = 0;
static SappyInstrument* instruments[3];

// VBlank interrupt handler
void vblank_handler(void) {
    // Update sound mixer
    sappy_vblank_intr();
}

// Initialize background
void init_background(void) {
    // Set video mode to Mode 3 (240x160, 16-bit bitmap)
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;
    
    // For Mode 3, we'd need to copy bitmap data directly to VRAM
    // For now, let's use Mode 0 with the tile-based background
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
    
    // Set up BG0 for 256x256 tilemap
    REG_BG0CNT = BG_CBB(0) | BG_SBB(31) | BG_8BPP | BG_SIZE0;
    
    // Note: We'd need to properly convert and load the background here
    // For now, we'll just clear the screen
    pal_bg_mem[0] = RGB15(0, 0, 0);    // Black background
    pal_bg_mem[1] = RGB15(31, 31, 31); // White text
}

// Draw simple UI text
void draw_ui(void) {
    // Clear text area
    for(int i = 0; i < 32*20; i++) {
        se_mem[31][i] = 0;
    }
    
    // Simple text display (would need a proper font system)
    // For now, just indicate the mode
}

int main(void) {
    // Initialize interrupts
    irq_init(NULL);
    irq_add(II_VBLANK, vblank_handler);
    
    // Initialize background
    init_background();
    
    // Initialize Sappy sound engine
    sappy_init(22050);  // 22.05 kHz sample rate
    sappy_init_samples();
    
    // Set up instrument array
    instruments[0] = &gPianoInstrument;
    instruments[1] = &gOrganInstrument;
    instruments[2] = &gDrumInstrument;
    
    // Main game loop
    int notePressed[8] = {0};  // Track which notes are pressed
    
    while(1) {
        // Wait for VBlank
        VBlankIntrWait();
        
        // Read keys
        key_poll();
        
        // Switch instruments with L/R
        if(key_hit(KEY_L)) {
            currentInstrument = (currentInstrument - 1 + 3) % 3;
        }
        if(key_hit(KEY_R)) {
            currentInstrument = (currentInstrument + 1) % 3;
        }
        
        // Play notes with buttons (C major scale)
        // A = C (60), B = D (62), X = E (64), Y = F (65)
        // D-pad: G (67), A (69), B (71), C (72)
        
        if(key_hit(KEY_A) && !notePressed[0]) {
            sappy_play_note(0, instruments[currentInstrument], 60, 127);  // C
            notePressed[0] = 1;
        }
        if(key_released(KEY_A) && notePressed[0]) {
            sappy_stop_note(0);
            notePressed[0] = 0;
        }
        
        if(key_hit(KEY_B) && !notePressed[1]) {
            sappy_play_note(1, instruments[currentInstrument], 62, 127);  // D
            notePressed[1] = 1;
        }
        if(key_released(KEY_B) && notePressed[1]) {
            sappy_stop_note(1);
            notePressed[1] = 0;
        }
        
        if(key_hit(KEY_LEFT) && !notePressed[2]) {
            sappy_play_note(2, instruments[currentInstrument], 64, 127);  // E
            notePressed[2] = 1;
        }
        if(key_released(KEY_LEFT) && notePressed[2]) {
            sappy_stop_note(2);
            notePressed[2] = 0;
        }
        
        if(key_hit(KEY_RIGHT) && !notePressed[3]) {
            sappy_play_note(3, instruments[currentInstrument], 65, 127);  // F
            notePressed[3] = 1;
        }
        if(key_released(KEY_RIGHT) && notePressed[3]) {
            sappy_stop_note(3);
            notePressed[3] = 0;
        }
        
        if(key_hit(KEY_UP) && !notePressed[4]) {
            sappy_play_note(4, instruments[currentInstrument], 67, 127);  // G
            notePressed[4] = 1;
        }
        if(key_released(KEY_UP) && notePressed[4]) {
            sappy_stop_note(4);
            notePressed[4] = 0;
        }
        
        if(key_hit(KEY_DOWN) && !notePressed[5]) {
            sappy_play_note(5, instruments[currentInstrument], 69, 127);  // A
            notePressed[5] = 1;
        }
        if(key_released(KEY_DOWN) && notePressed[5]) {
            sappy_stop_note(5);
            notePressed[5] = 0;
        }
        
        if(key_hit(KEY_START) && !notePressed[6]) {
            sappy_play_note(6, instruments[currentInstrument], 71, 127);  // B
            notePressed[6] = 1;
        }
        if(key_released(KEY_START) && notePressed[6]) {
            sappy_stop_note(6);
            notePressed[6] = 0;
        }
        
        if(key_hit(KEY_SELECT) && !notePressed[7]) {
            sappy_play_note(7, instruments[currentInstrument], 72, 127);  // High C
            notePressed[7] = 1;
        }
        if(key_released(KEY_SELECT) && notePressed[7]) {
            sappy_stop_note(7);
            notePressed[7] = 0;
        }
    }
    
    return 0;
}
