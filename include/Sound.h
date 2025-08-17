#ifndef SOUND_H
#define SOUND_H

#include "tonc.h"

// Based on Deku's sound tutorial Day 2 structure
// https://stuij.github.io/deku-sound-tutorial/

#define SND_MAX_CHANNELS 4

typedef struct _SOUND_CHANNEL {
    s8 *data;
    u32 pos;        // 12-bit fixed point position
    u32 inc;        // 12-bit fixed point increment  
    u32 vol;        // Volume (0-64)
    u32 length;     // 12-bit fixed point length
    u32 loopLength; // 12-bit fixed point loop length (0 = no loop)
} SOUND_CHANNEL;

typedef struct _SOUND_VARS {
    s8 *mixBufferBase;
    s8 *curMixBuffer;
    u32 mixBufferSize;
    u8 activeBuffer;
} SOUND_VARS;

extern SOUND_CHANNEL sndChannel[SND_MAX_CHANNELS];
extern SOUND_VARS sndVars;

// Function prototypes
void SndInit(void);
void SndVSync(void);  // VBlank handler
void SndMix(void);    // Mix one frame of audio

#endif // SOUND_H
