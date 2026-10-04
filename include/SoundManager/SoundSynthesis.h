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

#include "SystemTypes.h"

#include "SoundTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wave Table Management */
OSErr WaveTableAddWave(WaveTable* table, UInt16 index,
                       SInt16* waveData, UInt32 length,
                       UInt32 loopStart, UInt32 loopEnd,
                       UInt8 baseFreq);


/* Mixer Channel Control */
OSErr MixerFadeChannel(MixerPtr mixer, UInt16 channel,
                       UInt16 startVol, UInt16 endVol, UInt32 fadeTime);


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
extern const UInt16 MIDI_NOTE_FREQUENCIES[128];

/* General MIDI Program Names */
extern const char* GM_PROGRAM_NAMES[128];

/* Audio Processing Utilities */
void ConvertSampleFormat(void* src, void* dest, UInt32 samples,
                        AudioEncodingType srcFormat, AudioEncodingType destFormat);

#ifdef __cplusplus
}
#endif

#endif /* _SOUNDSYNTHESIS_H_ */
