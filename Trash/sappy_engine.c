#include "sappy_engine.h"
#include "tonc.h"

// Global sound info
SappySoundInfo gSappySoundInfo;

// Double buffer for audio mixing
static int8_t mixBuffer[2][736];  // 736 samples per frame at 44100 Hz
static uint8_t currentBuffer = 0;

// Initialize the Sappy-style sound engine
void sappy_init(uint32_t sampleRate) {
    // Set identity marker
    gSappySoundInfo.ident = 0x68736D53; // 'Smsh'
    
    // Configure sound parameters
    gSappySoundInfo.maxChans = SAPPY_MAX_DIRECTSOUND_CHANNELS;
    gSappySoundInfo.masterVol = 15;  // Max volume
    gSappySoundInfo.sampleRate = sampleRate;
    
    // Calculate samples per frame (60 Hz)
    gSappySoundInfo.samplesPerFrame = sampleRate / 60;
    
    // Initialize channels
    for (int i = 0; i < SAPPY_MAX_DIRECTSOUND_CHANNELS; i++) {
        gSappySoundInfo.channels[i].status = 0;
        gSappySoundInfo.channels[i].envelopeVolume = 0;
    }
    
    // Set up sound hardware
    REG_SOUNDCNT_X = SSTAT_ENABLE;
    
    // Configure for Direct Sound A at 100% volume, output to both L and R
    REG_SOUNDCNT_L = SDMG_BUILD(0, 0, 7, 7);  // DMG channels off, max master volume
    REG_SOUNDCNT_H = SDS_DMG100 | SDS_A100 | SDS_AR | SDS_AL | SDS_ATMR0 | SDS_ARESET;
    
    // Set up Timer 0 for sample rate
    uint32_t timerValue = 65536 - (16777216 / sampleRate);
    REG_TM0CNT = 0;
    REG_TM0D = timerValue;
    REG_TM0CNT = TM_ENABLE;
    
    // Set up DMA 1 for Direct Sound A
    REG_DMA1CNT = 0;
    REG_DMA1SAD = (uint32_t)mixBuffer[0];
    REG_DMA1DAD = (uint32_t)&REG_FIFO_A;
    REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
    
    gSappySoundInfo.pcmBuffer = mixBuffer[0];
    gSappySoundInfo.pcmBufferSize = sizeof(mixBuffer[0]);
}

// VBlank interrupt handler - switches buffers and triggers mixing
void sappy_vblank_intr(void) {
    // Switch buffers
    currentBuffer = 1 - currentBuffer;
    
    // Update DMA source to new buffer
    REG_DMA1CNT = 0;
    REG_DMA1SAD = (uint32_t)mixBuffer[currentBuffer];
    REG_DMA1DAD = (uint32_t)&REG_FIFO_A;
    REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
    
    // Mix audio for the next frame
    sappy_mix_channels();
}

// Mix all active channels into the PCM buffer
void sappy_mix_channels(void) {
    int8_t *outBuffer = mixBuffer[1 - currentBuffer];
    int16_t sample;
    
    // Clear the output buffer
    for (uint32_t i = 0; i < gSappySoundInfo.samplesPerFrame; i++) {
        outBuffer[i] = 0;
    }
    
    // Mix each active channel
    for (int ch = 0; ch < SAPPY_MAX_DIRECTSOUND_CHANNELS; ch++) {
        SappyChannel *chan = &gSappySoundInfo.channels[ch];
        
        if (!(chan->status & SAPPY_CHANNEL_SF_START) || !chan->wave) {
            continue;
        }
        
        // Update envelope
        sappy_update_envelope(chan);
        
        // Check if channel should stop
        if (chan->envelopeVolume == 0 && chan->envelopePhase == 3) {
            chan->status = 0;
            continue;
        }
        
        // Mix samples
        for (uint32_t i = 0; i < gSappySoundInfo.samplesPerFrame; i++) {
            // Get sample with linear interpolation
            uint32_t pos = chan->currentPos >> 8;  // Fixed point position
            
            if (pos >= chan->wave->size) {
                if (chan->status & SAPPY_CHANNEL_SF_LOOP) {
                    chan->currentPos = chan->wave->loopStart << 8;
                    pos = chan->wave->loopStart;
                } else {
                    chan->status &= ~SAPPY_CHANNEL_SF_START;
                    break;
                }
            }
            
            // Get sample and apply volume
            int16_t sampleValue = chan->wave->data[pos];
            sampleValue = (sampleValue * chan->envelopeVolume) >> 8;
            
            // Apply panning (simple stereo for now)
            int16_t leftSample = (sampleValue * chan->leftVolume) >> 7;
            int16_t rightSample = (sampleValue * chan->rightVolume) >> 7;
            sample = (leftSample + rightSample) >> 1;
            
            // Mix into output buffer with master volume
            int16_t mixed = outBuffer[i] + ((sample * gSappySoundInfo.masterVol) >> 4);
            
            // Clamp to 8-bit range
            if (mixed > 127) mixed = 127;
            if (mixed < -128) mixed = -128;
            outBuffer[i] = mixed;
            
            // Advance position based on frequency
            chan->currentPos += (chan->frequency >> 10);  // Simplified frequency calculation
        }
    }
}

// Update ADSR envelope for a channel
void sappy_update_envelope(SappyChannel *chan) {
    switch (chan->envelopePhase) {
        case 0:  // Attack
            if (chan->attack == 0) {
                chan->envelopeVolume = 255;
                chan->envelopePhase = 1;
            } else {
                chan->envelopeVolume += (256 / chan->attack);
                if (chan->envelopeVolume >= 255) {
                    chan->envelopeVolume = 255;
                    chan->envelopePhase = 1;
                }
            }
            break;
            
        case 1:  // Decay
            if (chan->decay == 0) {
                chan->envelopeVolume = chan->sustain;
                chan->envelopePhase = 2;
            } else {
                int16_t decayRate = (chan->envelopeVolume - chan->sustain) / chan->decay;
                if (decayRate < 1) decayRate = 1;
                chan->envelopeVolume -= decayRate;
                if (chan->envelopeVolume <= chan->sustain) {
                    chan->envelopeVolume = chan->sustain;
                    chan->envelopePhase = 2;
                }
            }
            break;
            
        case 2:  // Sustain
            // Stay at sustain level until note off
            if (!(chan->status & SAPPY_CHANNEL_SF_START)) {
                chan->envelopePhase = 3;
            }
            break;
            
        case 3:  // Release
            if (chan->release == 0) {
                chan->envelopeVolume = 0;
            } else {
                int16_t releaseRate = chan->envelopeVolume / chan->release;
                if (releaseRate < 1) releaseRate = 1;
                chan->envelopeVolume -= releaseRate;
                if (chan->envelopeVolume <= 0) {
                    chan->envelopeVolume = 0;
                }
            }
            break;
    }
}

// Play a note on a channel
void sappy_play_note(uint8_t channel, SappyInstrument *instrument, uint8_t key, uint8_t velocity) {
    if (channel >= SAPPY_MAX_DIRECTSOUND_CHANNELS || !instrument || !instrument->wave) {
        return;
    }
    
    SappyChannel *chan = &gSappySoundInfo.channels[channel];
    
    // Set up channel
    chan->status = SAPPY_CHANNEL_SF_START | SAPPY_CHANNEL_SF_ENV_ATTACK;
    chan->wave = instrument->wave;
    chan->key = key;
    chan->velocity = velocity;
    
    // Set ADSR envelope
    chan->attack = instrument->attack;
    chan->decay = instrument->decay;
    chan->sustain = instrument->sustain;
    chan->release = instrument->release;
    
    // Reset envelope
    chan->envelopeVolume = 0;
    chan->envelopePhase = 0;
    chan->envelopeCounter = 0;
    
    // Set volume and panning
    chan->leftVolume = 64;   // Center pan
    chan->rightVolume = 64;
    
    // Calculate frequency from MIDI key
    // Simplified: base frequency * 2^((key - 60) / 12)
    uint32_t baseFreq = instrument->wave->freq;
    int32_t keyDiff = key - 60;  // Middle C = 60
    chan->frequency = baseFreq;
    
    // Adjust frequency for key difference (simplified)
    if (keyDiff > 0) {
        for (int i = 0; i < keyDiff; i++) {
            chan->frequency = (chan->frequency * 10595) >> 13;  // Multiply by semitone ratio
        }
    } else if (keyDiff < 0) {
        for (int i = 0; i < -keyDiff; i++) {
            chan->frequency = (chan->frequency * 15564) >> 14;  // Divide by semitone ratio
        }
    }
    
    // Reset sample position
    chan->currentPos = 0;
    
    // Set loop flag if needed
    if (instrument->wave->loopStart < instrument->wave->size) {
        chan->status |= SAPPY_CHANNEL_SF_LOOP;
    }
}

// Stop a note on a channel
void sappy_stop_note(uint8_t channel) {
    if (channel >= SAPPY_MAX_DIRECTSOUND_CHANNELS) {
        return;
    }
    
    SappyChannel *chan = &gSappySoundInfo.channels[channel];
    
    // Trigger release phase
    chan->status &= ~SAPPY_CHANNEL_SF_START;
    chan->envelopePhase = 3;
}

// Set master volume
void sappy_set_master_volume(uint8_t volume) {
    if (volume > 15) volume = 15;
    gSappySoundInfo.masterVol = volume;
}
