#ifndef SAPPY_ENGINE_H
#define SAPPY_ENGINE_H

#include <stdint.h>
#include "tonc.h"

// Based on the m4a/Sappy engine structure from Pokemon Ruby/Sapphire

// Sound mode flags
#define SAPPY_MODE_REVERB_VAL   0x0000007F
#define SAPPY_MODE_REVERB_SET   0x00000080
#define SAPPY_MODE_MAXCHN       0x00000F00
#define SAPPY_MODE_MAXCHN_SHIFT 8
#define SAPPY_MODE_MASVOL       0x0000F000
#define SAPPY_MODE_MASVOL_SHIFT 12
#define SAPPY_MODE_FREQ_13379   0x00040000  // Default frequency
#define SAPPY_MODE_FREQ_21024   0x00070000
#define SAPPY_MODE_FREQ         0x000F0000
#define SAPPY_MODE_FREQ_SHIFT   16
#define SAPPY_MODE_DA_BIT_8     0x00900000  // 8-bit samples
#define SAPPY_MODE_DA_BIT_SHIFT 20

// Channel status flags
#define SAPPY_CHANNEL_SF_START       0x80
#define SAPPY_CHANNEL_SF_STOP        0x40
#define SAPPY_CHANNEL_SF_LOOP        0x10
#define SAPPY_CHANNEL_SF_ENV         0x03
#define SAPPY_CHANNEL_SF_ENV_ATTACK  0x03
#define SAPPY_CHANNEL_SF_ENV_DECAY   0x02
#define SAPPY_CHANNEL_SF_ENV_SUSTAIN 0x01
#define SAPPY_CHANNEL_SF_ENV_RELEASE 0x00

// Maximum channels
#define SAPPY_MAX_DIRECTSOUND_CHANNELS 8

// Sample data structure
typedef struct {
    uint16_t type;
    uint16_t status;
    uint32_t freq;
    uint32_t loopStart;
    uint32_t size;      // number of samples
    int8_t data[1];     // sample data
} SappyWaveData;

// Instrument definition
typedef struct {
    uint8_t type;
    uint8_t key;
    uint8_t length;
    uint8_t pan_sweep;
    SappyWaveData *wave;
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
} SappyInstrument;

// Sound channel structure
typedef struct {
    uint8_t status;
    uint8_t type;
    uint8_t rightVolume;
    uint8_t leftVolume;
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
    uint8_t key;
    uint8_t envelopeVolume;
    uint8_t envelopeVolumeRight;
    uint8_t envelopeVolumeLeft;
    uint8_t velocity;
    uint8_t priority;
    uint32_t frequency;
    uint32_t currentPos;    // Current position in sample
    SappyWaveData *wave;
    uint16_t envelopeCounter;
    uint8_t envelopePhase;  // 0=attack, 1=decay, 2=sustain, 3=release
} SappyChannel;

// Main sound info structure
typedef struct {
    uint32_t ident;         // Should be 'Smsh' (0x68736D53)
    uint8_t pcmDmaCtr;
    uint8_t reverb;
    uint8_t maxChans;
    uint8_t masterVol;
    uint8_t freq;
    uint8_t mode;
    uint8_t cgbChans;
    uint8_t priority;
    SappyChannel channels[SAPPY_MAX_DIRECTSOUND_CHANNELS];
    int8_t *pcmBuffer;
    uint32_t pcmBufferSize;
    uint32_t samplesPerFrame;
    uint32_t sampleRate;
} SappySoundInfo;

// Function prototypes
void sappy_init(uint32_t sampleRate);
void sappy_vblank_intr(void);
void sappy_mix_channels(void);
void sappy_play_note(uint8_t channel, SappyInstrument *instrument, uint8_t key, uint8_t velocity);
void sappy_stop_note(uint8_t channel);
void sappy_set_master_volume(uint8_t volume);
void sappy_update_envelope(SappyChannel *chan);

// External data (defined in implementation)
extern SappySoundInfo gSappySoundInfo;

// Sample initialization
void sappy_init_samples(void);

#endif // SAPPY_ENGINE_H
