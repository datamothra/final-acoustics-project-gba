// Ultra-simple stereo test to isolate the issue
#include "tonc.h"

// Simple white noise data
static const s8 noise_left[256] = {
    10, -20, 30, -40, 50, -60, 70, -80, 15, -25, 35, -45, 55, -65, 75, -85,
    12, -22, 32, -42, 52, -62, 72, -82, 17, -27, 37, -47, 57, -67, 77, -87,
    // ... repeating pattern for left
};

static const s8 noise_right[256] = {
    -15, 25, -35, 45, -55, 65, -75, 85, -10, 20, -30, 40, -50, 60, -70, 80,
    -17, 27, -37, 47, -57, 67, -77, 87, -12, 22, -32, 42, -52, 62, -72, 82,
    // ... different pattern for right
};

// Double buffers for each channel
static s8 bufferL[2][256] EWRAM_DATA;
static s8 bufferR[2][256] EWRAM_DATA;
static int activeBuffer = 0;

void SimpleStereo_Init(void) {
    // Copy noise to buffers
    for(int b = 0; b < 2; b++) {
        for(int i = 0; i < 256; i++) {
            bufferL[b][i] = noise_left[i % 256];
            bufferR[b][i] = noise_right[i % 256];
        }
    }
    
    // Set up sound registers
    REG_SOUNDCNT_X = SSTAT_ENABLE;
    REG_SOUNDCNT_L = 0;
    REG_SOUNDCNT_H = SSQR_RESET | SDMG_BUILD(3, 3, 2, 0);
    REG_SOUNDBIAS = 0x0200;
    
    // Timer 0 for sample rate
    REG_TM0D = 65536 - (16777216 / 18157);  // ~18kHz
    REG_TM0CNT = TM_ENABLE;
    
    // Initial DMA setup
    REG_DMA1SAD = (u32)bufferL[0];
    REG_DMA1DAD = (u32)&REG_FIFO_A;
    REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
    
    REG_DMA2SAD = (u32)bufferR[0];
    REG_DMA2DAD = (u32)&REG_FIFO_B;
    REG_DMA2CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
}

void SimpleStereo_VBlank(void) {
    // Swap buffers
    activeBuffer = 1 - activeBuffer;
    
    // Update DMA sources
    REG_DMA1CNT = 0;
    REG_DMA2CNT = 0;
    
    REG_DMA1SAD = (u32)bufferL[activeBuffer];
    REG_DMA2SAD = (u32)bufferR[activeBuffer];
    
    REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
    REG_DMA2CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
}

void SimpleStereo_Stop(void) {
    REG_DMA1CNT = 0;
    REG_DMA2CNT = 0;
}
