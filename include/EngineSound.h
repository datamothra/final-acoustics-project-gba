#ifndef ENGINE_SOUND_H
#define ENGINE_SOUND_H

#include "tonc.h"

// Minimal engine sound controller on top of Deku Day-2 style mixer

void EngineSound_init(const s8 *rearData,
                      const s8 *frontData,
                      u32 sampleLength,      // in samples
                      u32 loopStart,         // in samples
                      u32 loopLength,        // in samples (0=no loop)
                      u32 baseSampleRateHz); // nominal sample rate of sample data

// Start playback if not active. If already active and the sample side changes,
// it switches to that side and restarts.
void EngineSound_start(bool useFrontSample,
                       u32 volume0to64,
                       u32 targetFreqHz);

// Update volume/pitch without re-triggering
void EngineSound_update(u32 volume0to64, u32 targetFreqHz);

// Set per-source panning gains (0-64). Applies to both rear and front loops.
void EngineSound_set_pan(u32 panLeft0to64, u32 panRight0to64);

// Set independent front/rear mix (volumes 0-64, frequencies in Hz)
void EngineSound_set_mix(u32 volFront0to64, u32 volRear0to64,
                         u32 frontFreqHz, u32 rearFreqHz);

void EngineSound_stop(void);

// Optional demo update: simple approach/pass-by/away envelope
// rangeT is total frames of the demo; frame is current frame 0..rangeT
void EngineSound_demo_update_passby(u32 frame, u32 rangeT);

#endif // ENGINE_SOUND_H


