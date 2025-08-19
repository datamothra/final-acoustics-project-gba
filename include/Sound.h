#ifndef SOUND_H
#define SOUND_H

#include "tonc.h"
// Global mixer rate constant so all modules agree (matches Sappy common rate)
#ifndef SND_MIX_RATE_HZ
#define SND_MIX_RATE_HZ 18157
#endif

// Compile-time toggle for per-ear 2-band HRTF. Set to 0 for Sappy-style ILD-only.
#ifndef HRTF_ENABLE
#define HRTF_ENABLE 0
#endif
// Based on Deku's sound tutorial Day 2 structure
// https://stuij.github.io/deku-sound-tutorial/

#define SND_MAX_CHANNELS 4

typedef struct _SOUND_CHANNEL {
    s8 *data;
    u32 pos;        // 12-bit fixed point position
    u32 inc;        // 12-bit fixed point increment  
    u32 vol;        // Volume (0-64)
    u32 panL;       // Left gain (0-64)
    u32 panR;       // Right gain (0-64)
    u32 length;     // 12-bit fixed point length
    u32 loopLength; // 12-bit fixed point loop length (0 = no loop)
    // Simple 2-band per-ear HRTF (first-order low-pass states and gains)
    s32 lowStateL;   // IIR low state for left ear
    s32 lowStateR;   // IIR low state for right ear
    u16 lpAlpha;     // low-pass alpha in 4.12 fixed (0..4096)
    u16 gainLowL;    // 0..64
    u16 gainHighL;   // 0..64
    u16 gainLowR;    // 0..64
    u16 gainHighR;   // 0..64
} SOUND_CHANNEL;

typedef struct _SOUND_VARS {
    s8 *mixBufferBase;   // Left buffer base
    s8 *curMixBuffer;    // Left current buffer
    s8 *mixBufferBaseR;  // Right buffer base
    s8 *curMixBufferR;   // Right current buffer
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
