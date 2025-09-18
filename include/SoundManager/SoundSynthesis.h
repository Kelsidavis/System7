/*
 * SoundSynthesis.h - Sound Synthesis and MIDI Support
 *
 * Provides sound synthesis, wave table support, and MIDI capabilities
 * for the Mac OS Sound Manager. Includes square wave, sampled, and
 * wave table synthesizers with complete MIDI integration.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef _SOUNDSYNTHESIS_H_
#define _SOUNDSYNTHESIS_H_

#include <stdint.h>
#include <stdbool.h>
#include "SoundTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Synthesizer State Structure */
typedef struct SynthesizerState {
    int16_t         synthType;          /* Synthesizer type */
    bool            active;             /* Synthesizer active */
    bool            busy;               /* Synthesizer busy */
    uint32_t        sampleRate;         /* Current sample rate */
    uint16_t        channels;           /* Number of channels */
    uint16_t        bitDepth;           /* Bit depth */
    void*           privateData;        /* Private synthesizer data */
} SynthesizerState;

typedef struct SynthesizerState* SynthesizerPtr;

/* Wave Table Entry */
typedef struct WaveTableEntry {
    uint32_t        waveLength;         /* Length of wave in samples */
    int16_t*        waveData;           /* Wave sample data */
    uint32_t        loopStart;          /* Loop start point */
    uint32_t        loopEnd;            /* Loop end point */
    bool            looping;            /* Looping enabled */
    uint8_t         baseFreq;           /* Base frequency */
} WaveTableEntry;

/* Wave Table */
typedef struct WaveTable {
    uint16_t            numWaves;       /* Number of waves */
    WaveTableEntry*     waves;          /* Wave entries */
    uint32_t            version;        /* Wave table version */
} WaveTable;

/* Square Wave Synthesizer State */
typedef struct SquareWaveSynth {
    SynthesizerState    base;           /* Base synthesizer */
    uint16_t            frequency;      /* Current frequency */
    uint16_t            amplitude;      /* Current amplitude */
    uint16_t            timbre;         /* Timbre setting */
    uint32_t            phase;          /* Current phase */
    uint32_t            phaseIncrement; /* Phase increment per sample */
    bool                gate;           /* Gate on/off */
} SquareWaveSynth;

/* Sampled Sound Synthesizer State */
typedef struct SampledSynth {
    SynthesizerState    base;           /* Base synthesizer */
    SoundHeader*        soundHeader;    /* Current sound header */
    uint8_t*            sampleData;     /* Sample data pointer */
    uint32_t            sampleLength;   /* Sample length */
    uint32_t            sampleRate;     /* Original sample rate */
    uint32_t            currentPos;     /* Current playback position */
    uint32_t            loopStart;      /* Loop start position */
    uint32_t            loopEnd;        /* Loop end position */
    bool                looping;        /* Looping enabled */
    int16_t             amplitude;      /* Playback amplitude */
    Fixed               playbackRate;   /* Playback rate multiplier */
    uint8_t             encoding;       /* Sample encoding */
    uint16_t            interpolation;  /* Interpolation mode */
} SampledSynth;

/* Wave Table Synthesizer State */
typedef struct WaveTableSynth {
    SynthesizerState    base;           /* Base synthesizer */
    WaveTable*          waveTable;      /* Current wave table */
    uint16_t            currentWave;    /* Current wave index */
    uint32_t            currentPos;     /* Current position in wave */
    uint16_t            frequency;      /* Target frequency */
    uint16_t            amplitude;      /* Current amplitude */
    uint32_t            phaseIncrement; /* Phase increment */
    bool                looping;        /* Looping enabled */
} WaveTableSynth;

/* MIDI Voice State */
typedef struct MIDIVoice {
    bool                active;         /* Voice active */
    uint8_t             channel;        /* MIDI channel */
    uint8_t             note;           /* MIDI note number */
    uint8_t             velocity;       /* Note velocity */
    uint16_t            frequency;      /* Calculated frequency */
    uint16_t            amplitude;      /* Calculated amplitude */
    uint32_t            attackTime;     /* Attack time */
    uint32_t            decayTime;      /* Decay time */
    uint16_t            sustainLevel;   /* Sustain level */
    uint32_t            releaseTime;    /* Release time */
    uint32_t            currentTime;    /* Current envelope time */
    uint16_t            currentAmp;     /* Current envelope amplitude */
    uint8_t             envelopePhase;  /* Current envelope phase */
    uint16_t            pitchBend;      /* Pitch bend value */
    uint8_t             program;        /* Current program */
    WaveTableEntry*     wave;           /* Current wave */
    uint32_t            wavePos;        /* Position in wave */
} MIDIVoice;

/* MIDI Synthesizer State */
typedef struct MIDISynth {
    SynthesizerState    base;           /* Base synthesizer */
    MIDIVoice           voices[32];     /* MIDI voices */
    uint16_t            maxVoices;      /* Maximum polyphony */
    uint16_t            activeVoices;   /* Currently active voices */
    WaveTable*          waveTable;      /* Wave table for voices */
    uint8_t             channelProgram[16]; /* Program per channel */
    uint16_t            channelPitchBend[16]; /* Pitch bend per channel */
    uint8_t             channelVolume[16];   /* Volume per channel */
    uint8_t             channelPan[16];      /* Pan per channel */
    bool                channelMute[16];     /* Mute per channel */
    bool                sustainPedal[16];    /* Sustain pedal per channel */
} MIDISynth;

/* Mixer Channel State */
typedef struct MixerChannel {
    bool                active;         /* Channel active */
    SynthesizerPtr      synthesizer;    /* Associated synthesizer */
    int16_t*            inputBuffer;    /* Input buffer */
    uint32_t            inputFrames;    /* Input frame count */
    uint16_t            volume;         /* Channel volume (0-256) */
    int16_t             pan;            /* Pan (-128 to +127) */
    bool                muted;          /* Channel muted */
    bool                solo;           /* Channel soloed */
    uint32_t            fadeFrames;     /* Fade frame count */
    uint16_t            fadeStart;      /* Fade start volume */
    uint16_t            fadeEnd;        /* Fade end volume */
    uint32_t            fadePos;        /* Current fade position */
} MixerChannel;

/* Audio Mixer State */
typedef struct Mixer {
    MixerChannel        channels[32];   /* Mixer channels */
    uint16_t            numChannels;    /* Number of channels */
    uint16_t            activeChannels; /* Active channel count */
    int16_t*            mixBuffer;      /* Mix buffer */
    uint32_t            bufferFrames;   /* Buffer frame count */
    uint32_t            sampleRate;     /* Output sample rate */
    uint16_t            outputChannels; /* Output channel count */
    uint16_t            masterVolume;   /* Master volume */
    bool                masterMute;     /* Master mute */
    uint32_t            cpuLoad;        /* Current CPU load */
} Mixer;

typedef struct Mixer* MixerPtr;

/* Synthesizer Management Functions */
OSErr SynthInit(SynthesizerPtr* synth, int16_t synthType, uint32_t sampleRate);
OSErr SynthDispose(SynthesizerPtr synth);
OSErr SynthReset(SynthesizerPtr synth);

/* Square Wave Synthesizer */
OSErr SquareWaveInit(SquareWaveSynth* synth, uint32_t sampleRate);
OSErr SquareWaveSetFrequency(SquareWaveSynth* synth, uint16_t frequency);
OSErr SquareWaveSetAmplitude(SquareWaveSynth* synth, uint16_t amplitude);
OSErr SquareWaveSetTimbre(SquareWaveSynth* synth, uint16_t timbre);
OSErr SquareWaveGate(SquareWaveSynth* synth, bool gateOn);
uint32_t SquareWaveGenerate(SquareWaveSynth* synth, int16_t* buffer, uint32_t frameCount);

/* Sampled Sound Synthesizer */
OSErr SampledSynthInit(SampledSynth* synth, uint32_t sampleRate);
OSErr SampledSynthLoadSound(SampledSynth* synth, SoundHeader* header);
OSErr SampledSynthSetAmplitude(SampledSynth* synth, int16_t amplitude);
OSErr SampledSynthSetRate(SampledSynth* synth, Fixed rate);
OSErr SampledSynthPlay(SampledSynth* synth, bool looping);
OSErr SampledSynthStop(SampledSynth* synth);
uint32_t SampledSynthGenerate(SampledSynth* synth, int16_t* buffer, uint32_t frameCount);

/* Wave Table Synthesizer */
OSErr WaveTableInit(WaveTableSynth* synth, uint32_t sampleRate);
OSErr WaveTableLoadTable(WaveTableSynth* synth, WaveTable* table);
OSErr WaveTableSetWave(WaveTableSynth* synth, uint16_t waveIndex);
OSErr WaveTableSetFrequency(WaveTableSynth* synth, uint16_t frequency);
OSErr WaveTableSetAmplitude(WaveTableSynth* synth, uint16_t amplitude);
uint32_t WaveTableGenerate(WaveTableSynth* synth, int16_t* buffer, uint32_t frameCount);

/* Wave Table Management */
OSErr WaveTableCreate(WaveTable** table, uint16_t numWaves);
OSErr WaveTableDispose(WaveTable* table);
OSErr WaveTableAddWave(WaveTable* table, uint16_t index,
                       int16_t* waveData, uint32_t length,
                       uint32_t loopStart, uint32_t loopEnd,
                       uint8_t baseFreq);

/* MIDI Synthesizer */
OSErr MIDISynthInit(MIDISynth* synth, uint32_t sampleRate, uint16_t maxVoices);
OSErr MIDISynthDispose(MIDISynth* synth);
OSErr MIDISynthLoadWaveTable(MIDISynth* synth, WaveTable* table);

/* MIDI Message Processing */
OSErr MIDISynthNoteOn(MIDISynth* synth, uint8_t channel, uint8_t note, uint8_t velocity);
OSErr MIDISynthNoteOff(MIDISynth* synth, uint8_t channel, uint8_t note, uint8_t velocity);
OSErr MIDISynthProgramChange(MIDISynth* synth, uint8_t channel, uint8_t program);
OSErr MIDISynthPitchBend(MIDISynth* synth, uint8_t channel, uint16_t bend);
OSErr MIDISynthControlChange(MIDISynth* synth, uint8_t channel, uint8_t controller, uint8_t value);
OSErr MIDISynthSystemReset(MIDISynth* synth);

/* MIDI Voice Management */
OSErr MIDISynthAllocateVoice(MIDISynth* synth, uint8_t channel, uint8_t note, MIDIVoice** voice);
OSErr MIDISynthReleaseVoice(MIDISynth* synth, MIDIVoice* voice);
uint32_t MIDISynthGenerate(MIDISynth* synth, int16_t* buffer, uint32_t frameCount);

/* Audio Mixer */
OSErr MixerInit(MixerPtr* mixer, uint16_t numChannels, uint32_t sampleRate);
OSErr MixerDispose(MixerPtr mixer);
OSErr MixerAddChannel(MixerPtr mixer, SynthesizerPtr synth, uint16_t* channelIndex);
OSErr MixerRemoveChannel(MixerPtr mixer, uint16_t channelIndex);

/* Mixer Channel Control */
OSErr MixerSetChannelVolume(MixerPtr mixer, uint16_t channel, uint16_t volume);
OSErr MixerSetChannelPan(MixerPtr mixer, uint16_t channel, int16_t pan);
OSErr MixerSetChannelMute(MixerPtr mixer, uint16_t channel, bool muted);
OSErr MixerSetChannelSolo(MixerPtr mixer, uint16_t channel, bool solo);
OSErr MixerFadeChannel(MixerPtr mixer, uint16_t channel,
                       uint16_t startVol, uint16_t endVol, uint32_t fadeTime);

/* Mixer Master Control */
OSErr MixerSetMasterVolume(MixerPtr mixer, uint16_t volume);
OSErr MixerSetMasterMute(MixerPtr mixer, bool muted);
uint32_t MixerProcess(MixerPtr mixer, int16_t* outputBuffer, uint32_t frameCount);

/* MIDI Constants */
#define MIDI_NOTE_OFF           0x80
#define MIDI_NOTE_ON            0x90
#define MIDI_POLY_PRESSURE      0xA0
#define MIDI_CONTROL_CHANGE     0xB0
#define MIDI_PROGRAM_CHANGE     0xC0
#define MIDI_CHANNEL_PRESSURE   0xD0
#define MIDI_PITCH_BEND         0xE0
#define MIDI_SYSTEM_EXCLUSIVE   0xF0

/* MIDI Control Change Numbers */
#define MIDI_CC_BANK_SELECT_MSB     0
#define MIDI_CC_MODULATION_WHEEL    1
#define MIDI_CC_BREATH_CONTROLLER   2
#define MIDI_CC_FOOT_CONTROLLER     4
#define MIDI_CC_PORTAMENTO_TIME     5
#define MIDI_CC_DATA_ENTRY_MSB      6
#define MIDI_CC_VOLUME              7
#define MIDI_CC_BALANCE             8
#define MIDI_CC_PAN                 10
#define MIDI_CC_EXPRESSION          11
#define MIDI_CC_BANK_SELECT_LSB     32
#define MIDI_CC_DATA_ENTRY_LSB      38
#define MIDI_CC_SUSTAIN_PEDAL       64
#define MIDI_CC_PORTAMENTO          65
#define MIDI_CC_SOSTENUTO           66
#define MIDI_CC_SOFT_PEDAL          67
#define MIDI_CC_LEGATO_FOOTSWITCH   68
#define MIDI_CC_HOLD_2              69
#define MIDI_CC_SOUND_VARIATION     70
#define MIDI_CC_RESONANCE           71
#define MIDI_CC_SOUND_RELEASE_TIME  72
#define MIDI_CC_SOUND_ATTACK_TIME   73
#define MIDI_CC_SOUND_BRIGHTNESS    74
#define MIDI_CC_REVERB_LEVEL        91
#define MIDI_CC_TREMOLO_LEVEL       92
#define MIDI_CC_CHORUS_LEVEL        93
#define MIDI_CC_CELESTE_LEVEL       94
#define MIDI_CC_PHASER_LEVEL        95
#define MIDI_CC_ALL_SOUND_OFF       120
#define MIDI_CC_ALL_CONTROLLERS_OFF 121
#define MIDI_CC_LOCAL_KEYBOARD      122
#define MIDI_CC_ALL_NOTES_OFF       123

/* Note Frequency Table (A4 = 440 Hz) */
extern const uint16_t MIDI_NOTE_FREQUENCIES[128];

/* General MIDI Program Names */
extern const char* GM_PROGRAM_NAMES[128];

/* Envelope Phases */
typedef enum {
    ENV_PHASE_OFF = 0,
    ENV_PHASE_ATTACK = 1,
    ENV_PHASE_DECAY = 2,
    ENV_PHASE_SUSTAIN = 3,
    ENV_PHASE_RELEASE = 4
} EnvelopePhase;

/* Interpolation Modes */
typedef enum {
    INTERP_NONE = 0,            /* No interpolation (nearest) */
    INTERP_LINEAR = 1,          /* Linear interpolation */
    INTERP_CUBIC = 2,           /* Cubic interpolation */
    INTERP_SINC = 3             /* Sinc interpolation */
} InterpolationMode;

/* Synthesis Utility Functions */
uint16_t NoteToFrequency(uint8_t note);
uint8_t FrequencyToNote(uint16_t frequency);
int16_t LinearInterpolate(int16_t sample1, int16_t sample2, uint32_t fraction);
int16_t CubicInterpolate(int16_t s0, int16_t s1, int16_t s2, int16_t s3, uint32_t fraction);

/* Envelope Generator Functions */
uint16_t EnvelopeProcess(MIDIVoice* voice);
void EnvelopeNoteOn(MIDIVoice* voice);
void EnvelopeNoteOff(MIDIVoice* voice);

/* Audio Processing Utilities */
void ApplyVolume(int16_t* buffer, uint32_t frameCount, uint16_t volume);
void ApplyPan(int16_t* leftBuffer, int16_t* rightBuffer, uint32_t frameCount, int16_t pan);
void MixBuffers(int16_t* dest, const int16_t* src, uint32_t frameCount, uint16_t volume);
void ConvertSampleFormat(void* src, void* dest, uint32_t samples,
                        AudioEncodingType srcFormat, AudioEncodingType destFormat);

#ifdef __cplusplus
}
#endif

#endif /* _SOUNDSYNTHESIS_H_ */