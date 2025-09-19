/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * SoundMIDI.c
 *
 * MIDI support for Sound Manager
 * Provides MIDI playback, General MIDI instruments, and MIDI file parsing
 *
 * Based on Mac OS 7.1 Sound Manager MIDI capabilities
 */

#include "SoundManager/SoundManager.h"
#include "SoundManager/SoundSynthesis.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* MIDI Constants */
#define MIDI_NOTE_OFF           0x80
#define MIDI_NOTE_ON            0x90
#define MIDI_KEY_PRESSURE       0xA0
#define MIDI_CONTROL_CHANGE     0xB0
#define MIDI_PROGRAM_CHANGE     0xC0
#define MIDI_CHANNEL_PRESSURE   0xD0
#define MIDI_PITCH_WHEEL        0xE0
#define MIDI_SYSEX              0xF0
#define MIDI_TIME_CODE          0xF1
#define MIDI_SONG_POSITION      0xF2
#define MIDI_SONG_SELECT        0xF3
#define MIDI_TUNE_REQUEST       0xF6
#define MIDI_END_SYSEX          0xF7
#define MIDI_TIMING_CLOCK       0xF8
#define MIDI_START              0xFA
#define MIDI_CONTINUE           0xFB
#define MIDI_STOP               0xFC
#define MIDI_ACTIVE_SENSING     0xFE
#define MIDI_SYSTEM_RESET       0xFF

/* MIDI Controllers */
#define MIDI_CC_MODULATION      1
#define MIDI_CC_BREATH          2
#define MIDI_CC_VOLUME          7
#define MIDI_CC_PAN             10
#define MIDI_CC_EXPRESSION      11
#define MIDI_CC_SUSTAIN         64
#define MIDI_CC_PORTAMENTO      65
#define MIDI_CC_SOSTENUTO       66
#define MIDI_CC_SOFT_PEDAL      67
#define MIDI_CC_ALL_SOUND_OFF   120
#define MIDI_CC_RESET_ALL       121
#define MIDI_CC_ALL_NOTES_OFF   123

/* MIDI Channel state */
typedef struct MIDIChannel {
    uint8_t program;            /* Current program (instrument) */
    uint8_t volume;             /* Channel volume */
    uint8_t pan;                /* Pan position */
    uint8_t expression;         /* Expression level */
    uint16_t pitchBend;         /* Pitch bend value */
    Boolean sustain;            /* Sustain pedal state */
    Boolean sostenuto;          /* Sostenuto pedal state */
    Boolean soft;               /* Soft pedal state */

    /* Active notes */
    struct {
        uint8_t note;
        uint8_t velocity;
        Boolean active;
        uint32_t startTime;
    } notes[128];

    /* Synthesizer for this channel */
    SynthesizerPtr synthesizer;
} MIDIChannel;

/* MIDI Manager state */
typedef struct MIDIManager {
    Boolean initialized;
    MIDIChannel channels[16];

    /* MIDI file playback */
    struct {
        uint8_t* data;
        uint32_t size;
        uint32_t position;
        uint16_t format;
        uint16_t numTracks;
        uint16_t division;
        uint32_t tempo;         /* Microseconds per quarter note */
        Boolean playing;
        uint32_t currentTick;
    } file;

    /* Timing */
    uint32_t sampleRate;
    uint32_t samplesPerTick;
} MIDIManager;

static MIDIManager g_midiManager = {0};

/* Forward declarations */
static OSErr ProcessMIDIMessage(uint8_t* message, uint32_t length);
static OSErr ProcessChannelMessage(uint8_t status, uint8_t data1, uint8_t data2);
static OSErr ProcessSystemMessage(uint8_t status, uint8_t* data, uint32_t length);
static uint32_t ReadVariableLength(uint8_t* data, uint32_t* position);
static OSErr ParseMIDIFile(uint8_t* data, uint32_t size);

/* ========================================================================
 * MIDI Initialization
 * ======================================================================== */

OSErr MIDIManagerInit(uint32_t sampleRate) {
    if (g_midiManager.initialized) {
        return noErr;
    }

    memset(&g_midiManager, 0, sizeof(MIDIManager));
    g_midiManager.sampleRate = sampleRate;

    /* Initialize channels */
    for (int i = 0; i < 16; i++) {
        MIDIChannel* channel = &g_midiManager.channels[i];

        channel->program = 0;        /* Acoustic Grand Piano */
        channel->volume = 100;
        channel->pan = 64;            /* Center */
        channel->expression = 127;
        channel->pitchBend = 0x2000;  /* Center (no bend) */
        channel->sustain = false;
        channel->sostenuto = false;
        channel->soft = false;

        /* Clear notes */
        memset(channel->notes, 0, sizeof(channel->notes));

        /* Create synthesizer for channel */
        OSErr err = SynthInit(&channel->synthesizer, sampledSynth, sampleRate);
        if (err != noErr) {
            /* Clean up previously created synthesizers */
            for (int j = 0; j < i; j++) {
                SynthDispose(g_midiManager.channels[j].synthesizer);
            }
            return err;
        }
    }

    /* Set channel 9 (index 9) for drums */
    g_midiManager.channels[9].program = 128;  /* Drum kit marker */

    g_midiManager.initialized = true;
    return noErr;
}

void MIDIManagerShutdown(void) {
    if (!g_midiManager.initialized) {
        return;
    }

    /* Stop playback */
    MIDIStopPlayback();

    /* Dispose synthesizers */
    for (int i = 0; i < 16; i++) {
        if (g_midiManager.channels[i].synthesizer) {
            SynthDispose(g_midiManager.channels[i].synthesizer);
        }
    }

    /* Free MIDI file data */
    if (g_midiManager.file.data) {
        free(g_midiManager.file.data);
    }

    g_midiManager.initialized = false;
}

/* ========================================================================
 * MIDI Message Processing
 * ======================================================================== */

OSErr MIDISendMessage(uint8_t* message, uint32_t length) {
    if (!g_midiManager.initialized || !message || length == 0) {
        return paramErr;
    }

    return ProcessMIDIMessage(message, length);
}

static OSErr ProcessMIDIMessage(uint8_t* message, uint32_t length) {
    uint8_t status = message[0];

    /* Check for channel message */
    if ((status & 0xF0) < 0xF0) {
        uint8_t data1 = (length > 1) ? message[1] : 0;
        uint8_t data2 = (length > 2) ? message[2] : 0;
        return ProcessChannelMessage(status, data1, data2);
    }

    /* System message */
    return ProcessSystemMessage(status, message + 1, length - 1);
}

static OSErr ProcessChannelMessage(uint8_t status, uint8_t data1, uint8_t data2) {
    uint8_t messageType = status & 0xF0;
    uint8_t channel = status & 0x0F;

    if (channel >= 16) {
        return paramErr;
    }

    MIDIChannel* chan = &g_midiManager.channels[channel];

    switch (messageType) {
        case MIDI_NOTE_OFF:
            return MIDINoteOff(channel, data1, data2);

        case MIDI_NOTE_ON:
            if (data2 == 0) {
                /* Note on with velocity 0 is note off */
                return MIDINoteOff(channel, data1, 64);
            }
            return MIDINoteOn(channel, data1, data2);

        case MIDI_CONTROL_CHANGE:
            return MIDIControlChange(channel, data1, data2);

        case MIDI_PROGRAM_CHANGE:
            return MIDIProgramChange(channel, data1);

        case MIDI_PITCH_WHEEL:
            return MIDIPitchBend(channel, (data2 << 7) | data1);

        default:
            /* Unsupported message type */
            return noErr;
    }
}

static OSErr ProcessSystemMessage(uint8_t status, uint8_t* data, uint32_t length) {
    switch (status) {
        case MIDI_TIMING_CLOCK:
            /* Timing clock tick */
            if (g_midiManager.file.playing) {
                g_midiManager.file.currentTick++;
            }
            break;

        case MIDI_START:
            MIDIStartPlayback();
            break;

        case MIDI_STOP:
            MIDIStopPlayback();
            break;

        case MIDI_SYSTEM_RESET:
            MIDIReset();
            break;

        default:
            /* Ignore other system messages */
            break;
    }

    return noErr;
}

/* ========================================================================
 * MIDI Note Control
 * ======================================================================== */

OSErr MIDINoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (channel >= 16 || note >= 128) {
        return paramErr;
    }

    MIDIChannel* chan = &g_midiManager.channels[channel];

    /* Mark note as active */
    chan->notes[note].note = note;
    chan->notes[note].velocity = velocity;
    chan->notes[note].active = true;
    chan->notes[note].startTime = 0;  /* Would use actual time */

    /* Play note on synthesizer */
    if (chan->synthesizer) {
        SynthPlayNote(chan->synthesizer, note, velocity, 0);
    }

    return noErr;
}

OSErr MIDINoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (channel >= 16 || note >= 128) {
        return paramErr;
    }

    MIDIChannel* chan = &g_midiManager.channels[channel];

    /* Check sustain pedal */
    if (!chan->sustain) {
        /* Mark note as inactive */
        chan->notes[note].active = false;

        /* Stop note on synthesizer */
        if (chan->synthesizer) {
            SynthStopNote(chan->synthesizer, note);
        }
    } else {
        /* Note will be released when sustain is released */
        chan->notes[note].velocity = 0;  /* Mark for release */
    }

    return noErr;
}

OSErr MIDIAllNotesOff(uint8_t channel) {
    if (channel >= 16) {
        return paramErr;
    }

    MIDIChannel* chan = &g_midiManager.channels[channel];

    /* Stop all notes */
    for (int i = 0; i < 128; i++) {
        if (chan->notes[i].active) {
            MIDINoteOff(channel, i, 0);
        }
    }

    return noErr;
}

/* ========================================================================
 * MIDI Control Changes
 * ======================================================================== */

OSErr MIDIControlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    if (channel >= 16) {
        return paramErr;
    }

    MIDIChannel* chan = &g_midiManager.channels[channel];

    switch (controller) {
        case MIDI_CC_VOLUME:
            chan->volume = value;
            if (chan->synthesizer) {
                double volume = value / 127.0;
                SynthSetParameter(chan->synthesizer, kSynthParamAmplitude, volume);
            }
            break;

        case MIDI_CC_PAN:
            chan->pan = value;
            break;

        case MIDI_CC_EXPRESSION:
            chan->expression = value;
            break;

        case MIDI_CC_SUSTAIN:
            chan->sustain = (value >= 64);
            if (!chan->sustain) {
                /* Release all sustained notes */
                for (int i = 0; i < 128; i++) {
                    if (chan->notes[i].active && chan->notes[i].velocity == 0) {
                        MIDINoteOff(channel, i, 0);
                    }
                }
            }
            break;

        case MIDI_CC_ALL_SOUND_OFF:
        case MIDI_CC_ALL_NOTES_OFF:
            MIDIAllNotesOff(channel);
            break;

        case MIDI_CC_RESET_ALL:
            /* Reset all controllers */
            chan->volume = 100;
            chan->pan = 64;
            chan->expression = 127;
            chan->pitchBend = 0x2000;
            chan->sustain = false;
            chan->sostenuto = false;
            chan->soft = false;
            MIDIAllNotesOff(channel);
            break;
    }

    return noErr;
}

OSErr MIDIProgramChange(uint8_t channel, uint8_t program) {
    if (channel >= 16 || program >= 128) {
        return paramErr;
    }

    MIDIChannel* chan = &g_midiManager.channels[channel];
    chan->program = program;

    /* Configure synthesizer for new instrument */
    if (chan->synthesizer) {
        /* Set waveform based on program */
        WaveformType waveform = kWaveformSine;
        if (program < 8) {
            waveform = kWaveformSine;      /* Piano */
        } else if (program < 16) {
            waveform = kWaveformSquare;    /* Chromatic percussion */
        } else if (program < 24) {
            waveform = kWaveformSawtooth;  /* Organ */
        } else if (program < 32) {
            waveform = kWaveformTriangle;  /* Guitar */
        }

        SynthSetParameter(chan->synthesizer, kSynthParamWaveform, waveform);
    }

    return noErr;
}

OSErr MIDIPitchBend(uint8_t channel, uint16_t value) {
    if (channel >= 16) {
        return paramErr;
    }

    MIDIChannel* chan = &g_midiManager.channels[channel];
    chan->pitchBend = value;

    /* Apply pitch bend to synthesizer */
    if (chan->synthesizer) {
        /* Convert 14-bit pitch bend to semitones (-2 to +2) */
        double bend = ((double)value - 0x2000) / 0x2000 * 2.0;
        /* Would apply to frequency */
    }

    return noErr;
}

/* ========================================================================
 * MIDI File Playback
 * ======================================================================== */

OSErr MIDILoadFile(const char* filePath) {
    FILE* file = fopen(filePath, "rb");
    if (!file) {
        return fnfErr;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Allocate buffer */
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(file);
        return memFullErr;
    }

    /* Read file */
    size_t bytesRead = fread(data, 1, size, file);
    fclose(file);

    if (bytesRead != size) {
        free(data);
        return ioErr;
    }

    /* Parse MIDI file */
    OSErr err = ParseMIDIFile(data, size);
    if (err != noErr) {
        free(data);
        return err;
    }

    /* Store file data */
    if (g_midiManager.file.data) {
        free(g_midiManager.file.data);
    }
    g_midiManager.file.data = data;
    g_midiManager.file.size = size;
    g_midiManager.file.position = 0;

    return noErr;
}

static OSErr ParseMIDIFile(uint8_t* data, uint32_t size) {
    if (size < 14) {
        return paramErr;
    }

    /* Check header */
    if (memcmp(data, "MThd", 4) != 0) {
        return paramErr;
    }

    /* Parse header chunk */
    uint32_t headerSize = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    if (headerSize < 6) {
        return paramErr;
    }

    g_midiManager.file.format = (data[8] << 8) | data[9];
    g_midiManager.file.numTracks = (data[10] << 8) | data[11];
    g_midiManager.file.division = (data[12] << 8) | data[13];

    /* Set default tempo (120 BPM) */
    g_midiManager.file.tempo = 500000;  /* Microseconds per quarter note */

    /* Calculate samples per tick */
    if (g_midiManager.file.division & 0x8000) {
        /* SMPTE time code */
        int fps = -(int8_t)(g_midiManager.file.division >> 8);
        int tpf = g_midiManager.file.division & 0xFF;
        g_midiManager.file.samplesPerTick = g_midiManager.sampleRate / (fps * tpf);
    } else {
        /* Ticks per quarter note */
        uint32_t ticksPerQuarter = g_midiManager.file.division;
        uint32_t microsecondsPerTick = g_midiManager.file.tempo / ticksPerQuarter;
        g_midiManager.file.samplesPerTick = (g_midiManager.sampleRate * microsecondsPerTick) / 1000000;
    }

    return noErr;
}

OSErr MIDIStartPlayback(void) {
    if (!g_midiManager.file.data) {
        return noDataErr;
    }

    g_midiManager.file.playing = true;
    g_midiManager.file.currentTick = 0;
    g_midiManager.file.position = 14;  /* Skip header */

    return noErr;
}

OSErr MIDIStopPlayback(void) {
    g_midiManager.file.playing = false;

    /* Stop all notes */
    for (int i = 0; i < 16; i++) {
        MIDIAllNotesOff(i);
    }

    return noErr;
}

Boolean MIDIIsPlaying(void) {
    return g_midiManager.file.playing;
}

/* ========================================================================
 * MIDI Reset
 * ======================================================================== */

OSErr MIDIReset(void) {
    /* Reset all channels */
    for (int i = 0; i < 16; i++) {
        MIDIControlChange(i, MIDI_CC_RESET_ALL, 0);
        MIDIProgramChange(i, (i == 9) ? 128 : 0);  /* Channel 10 is drums */
    }

    /* Stop playback */
    MIDIStopPlayback();

    return noErr;
}

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

static uint32_t ReadVariableLength(uint8_t* data, uint32_t* position) {
    uint32_t value = 0;
    uint8_t byte;

    do {
        byte = data[(*position)++];
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);

    return value;
}

const char* MIDIGetProgramName(uint8_t program) {
    if (program >= 128) {
        return "Drum Kit";
    }

    /* General MIDI program names */
    static const char* programNames[128] = {
        "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
        "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
        /* ... rest of GM instruments ... */
    };

    return programNames[program];
}