/*
 * SoundManager.h - Mac OS Sound Manager API
 *
 * Complete implementation of the Mac OS Sound Manager for System 7.1
 * providing audio playback, recording, synthesis, and device management.
 *
 * This implementation preserves exact Mac OS Sound Manager behavior while
 * providing portable abstractions for modern audio systems.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef _SOUNDMANAGER_H_
#define _SOUNDMANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include "SoundTypes.h"
#include "SoundSynthesis.h"
#include "SoundHardware.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sound Manager Version */
#define kSoundManagerVersion    0x0300      /* Version 3.0 */

/* Sound Manager Error Codes */
typedef enum {
    noErr = 0,
    notEnoughHardwareErr = -201,            /* Insufficient hardware */
    queueFull = -203,                       /* Sound queue is full */
    resProblem = -204,                      /* Resource problem */
    badChannel = -205,                      /* Invalid channel reference */
    badFormat = -206,                       /* Unsupported audio format */
    notEnoughBufferSpace = -207,            /* Buffer space exhausted */
    badFileFormat = -208,                   /* Invalid file format */
    channelBusy = -209,                     /* Channel is busy */
    buffersTooSmall = -210,                 /* Buffers are too small */
    channelNotBusy = -211,                  /* Channel is not busy */
    noSynthFound = -212,                    /* No synthesizer found */
    synthNotReady = -213,                   /* Synthesizer not ready */
    synthOpenFailed = -214,                 /* Failed to open synthesizer */
    siNoSoundInHardware = -220,             /* No sound input hardware */
    siBadSoundInDevice = -221,              /* Bad sound input device */
    siNoBufferSpecified = -222,             /* No buffer specified */
    siInvalidCompression = -223,            /* Invalid compression type */
    siHardDriveTooSlow = -224,              /* Hard drive too slow */
    siInvalidSampleRate = -225,             /* Invalid sample rate */
    siInvalidSampleSize = -226,             /* Invalid sample size */
    siDeviceBusyErr = -227,                 /* Sound input device busy */
    siBadDeviceName = -228,                 /* Bad device name */
    siBadRefNum = -229,                     /* Bad reference number */
    siInputDeviceErr = -230,                /* Sound input device error */
    siUnknownInfoType = -231,               /* Unknown info type */
    siUnknownQuality = -232,                /* Unknown quality */
    paramErr = -50,                         /* Parameter error */
    memFullErr = -108                       /* Memory full */
} SoundManagerError;

/* Sound Manager Command Opcodes */
typedef enum {
    nullCmd = 0,                    /* Do nothing */
    quietCmd = 3,                   /* Stop all sounds */
    flushCmd = 4,                   /* Flush command queue */
    reInitCmd = 5,                  /* Reinitialize channel */
    waitCmd = 10,                   /* Wait for specified time */
    pauseCmd = 11,                  /* Pause channel */
    resumeCmd = 12,                 /* Resume channel */
    callBackCmd = 13,               /* Execute callback */
    syncCmd = 14,                   /* Synchronize channels */
    availableCmd = 24,              /* Get available channels */
    versionCmd = 25,                /* Get Sound Manager version */
    totalLoadCmd = 26,              /* Get total CPU load */
    loadCmd = 27,                   /* Get channel CPU load */
    freqDurationCmd = 40,           /* Play frequency for duration */
    restCmd = 41,                   /* Rest for specified time */
    freqCmd = 42,                   /* Set frequency */
    ampCmd = 43,                    /* Set amplitude */
    timbreCmd = 44,                 /* Set timbre */
    getAmpCmd = 45,                 /* Get amplitude */
    volumeCmd = 46,                 /* Set volume */
    getVolumeCmd = 47,              /* Get volume */
    waveTableCmd = 60,              /* Set wave table */
    phaseCmd = 61,                  /* Set phase */
    soundCmd = 80,                  /* Play sound resource */
    bufferCmd = 81,                 /* Play from buffer */
    rateCmd = 82,                   /* Set sample rate */
    continueCmd = 83,               /* Continue playback */
    doubleBufferCmd = 84,           /* Double buffer playback */
    getRateCmd = 85,                /* Get sample rate */
    rateMultiplierCmd = 86,         /* Set rate multiplier */
    getRateMultiplierCmd = 87,      /* Get rate multiplier */
    sizeCmd = 90,                   /* Set buffer size */
    convertCmd = 91,                /* Convert audio format */
    pommeCmd = 92,                  /* Pomme command */
    skinCmd = 93,                   /* Skin command */
    volumeRampCmd = 94,             /* Volume ramp */
    getActuallRateCmd = 95,         /* Get actual sample rate */
    scaleCmd = 96,                  /* Scale audio */
    tempoCmd = 97,                  /* Set tempo */
    stereoCmd = 98,                 /* Stereo pan */
    makeStereoCmd = 99,             /* Make stereo */
    bassCmd = 100,                  /* Bass control */
    trebleCmd = 101,                /* Treble control */
    synthCmd = 102,                 /* Synthesizer command */
    synthAttackReleaseCmd = 103,    /* Attack/release envelope */
    midiCmd = 104,                  /* MIDI command */
    setupMIDICmd = 105              /* Setup MIDI */
} SoundManagerCommand;

/* Channel Initialization Flags */
typedef enum {
    initChan0 = 0x0004,             /* Initialize channel 0 */
    initChan1 = 0x0008,             /* Initialize channel 1 */
    initChan2 = 0x0010,             /* Initialize channel 2 */
    initChan3 = 0x0020,             /* Initialize channel 3 */
    initStereo = 0x00C0,            /* Stereo channel pair */
    initMono = 0x0080,              /* Mono channel */
    initNoInterp = 0x0004,          /* No interpolation */
    initNoDrop = 0x0008,            /* Don't drop samples */
    initMACE3 = 0x0010,             /* MACE 3:1 compression */
    initMACE6 = 0x0020,             /* MACE 6:1 compression */
    initPanMask = 0x0003,           /* Pan mask */
    initSRateMask = 0x0030,         /* Sample rate mask */
    initStereoMask = 0x00C0,        /* Stereo/mono mask */
    initCompMask = 0x00FF           /* Compression mask */
} ChannelInitFlags;

/* Synthesizer Types */
typedef enum {
    squareWaveSynth = 1,            /* Square wave synthesizer */
    waveTableSynth = 3,             /* Wave table synthesizer */
    sampledSynth = 5,               /* Sampled sound synthesizer */
    MIDISynthIn = 7,                /* MIDI input */
    MIDISynthOut = 9,               /* MIDI output */
    internalSynth = 11              /* Internal synthesizer */
} SynthesizerType;

/* Volume Control Constants */
#define kFullVolume         0x0100  /* Maximum volume */
#define kNoVolume           0x0000  /* Silence */
#define kDefaultVolume      0x0100  /* Default volume level */

/* Sample Rate Constants (Fixed Point) */
#define rate22khz           0x56220000UL    /* 22.254 kHz */
#define rate11khz           0x2B110000UL    /* 11.127 kHz */
#define rate44khz           0xAC440000UL    /* 44.100 kHz */
#define rate48khz           0xBB800000UL    /* 48.000 kHz */

/* Sound Manager Command Structure */
typedef struct SndCommand {
    uint16_t    cmd;                /* Command opcode */
    int16_t     param1;             /* First parameter */
    int32_t     param2;             /* Second parameter */
} SndCommand;

/* Sound Channel Callback Function */
typedef void (*SndCallBackProcPtr)(SndChannelPtr chan, SndCommand *cmd);

/* Sound Manager Global State */
typedef struct {
    bool                initialized;        /* Initialization flag */
    uint16_t           version;            /* Sound Manager version */
    uint32_t           channelCount;       /* Number of active channels */
    SndChannelPtr      channelList;        /* List of sound channels */
    SoundHardwarePtr   hardware;           /* Hardware abstraction */
    SynthesizerPtr     synthesizer;        /* Software synthesizer */
    MixerPtr           mixer;              /* Audio mixer */
    RecorderPtr        recorder;           /* Audio recorder */
    uint16_t           globalVolume;       /* System volume level */
    bool               muted;              /* Global mute state */
    uint32_t           cpuLoad;            /* Current CPU load */
} SoundManagerGlobals;

/* Sound Manager API Functions */

/* Initialization and Cleanup */
OSErr SoundManagerInit(void);
OSErr SoundManagerShutdown(void);

/* Channel Management */
OSErr SndNewChannel(SndChannelPtr *chan,
                   int16_t synth,
                   int32_t init,
                   SndCallBackProcPtr userRoutine);

OSErr SndDisposeChannel(SndChannelPtr chan, Boolean quietNow);

OSErr SndChannelStatus(SndChannelPtr chan,
                      int16_t theLength,
                      SCStatus *theStatus);

/* Sound Playback */
OSErr SndPlay(SndChannelPtr chan, SndListHandle sndHandle, Boolean async);

OSErr SndStartFilePlay(SndChannelPtr chan,
                      int16_t fRefNum,
                      int16_t resNum,
                      int32_t bufferSize,
                      void *theBuffer,
                      AudioSelectionPtr theSelection,
                      FilePlayCompletionUPP theCompletion,
                      Boolean async);

OSErr SndPauseFilePlay(SndChannelPtr chan);
OSErr SndStopFilePlay(SndChannelPtr chan, Boolean quietNow);

/* Command Processing */
OSErr SndDoCommand(SndChannelPtr chan,
                  const SndCommand *cmd,
                  Boolean noWait);

OSErr SndDoImmediate(SndChannelPtr chan, const SndCommand *cmd);

/* Sound Control */
OSErr SndControl(int16_t id, SndCommand *cmd);

/* Volume and Settings */
void GetSysBeepVolume(int32_t *level);
OSErr SetSysBeepVolume(int32_t level);
void GetDefaultOutputVolume(int32_t *level);
OSErr SetDefaultOutputVolume(int32_t level);
void GetSoundHeaderOffset(SndListHandle sndHandle, int32_t *offset);
UnsignedFixed GetCompressionInfo(int16_t compressionID,
                                OSType format,
                                int16_t numChannels,
                                int16_t sampleSize,
                                CompressionInfoPtr cp);

/* Sound Recording */
OSErr SndRecord(ModalFilterProcPtr filterProc,
               Point corner,
               OSType quality,
               SndListHandle *sndHandle);

OSErr SndRecordToFile(ModalFilterProcPtr filterProc,
                     Point corner,
                     OSType quality,
                     int16_t fRefNum);

/* Sound Input Management */
OSErr SPBOpenDevice(const Str255 deviceName,
                   int16_t permission,
                   int32_t *inRefNum);

OSErr SPBCloseDevice(int32_t inRefNum);

OSErr SPBRecord(SPBPtr inParamPtr, Boolean asynchFlag);
OSErr SPBRecordToFile(SPBPtr inParamPtr, Boolean asynchFlag);
OSErr SPBPauseRecording(int32_t inRefNum);
OSErr SPBResumeRecording(int32_t inRefNum);
OSErr SPBStopRecording(int32_t inRefNum);

OSErr SPBGetRecordingStatus(int32_t inRefNum,
                           int16_t *recordingStatus,
                           int16_t *meterLevel,
                           uint32_t *totalSamplesToRecord,
                           uint32_t *numberOfSamplesRecorded,
                           uint32_t *totalMsecsToRecord,
                           uint32_t *numberOfMsecsRecorded);

OSErr SPBGetDeviceInfo(int32_t inRefNum,
                      OSType infoType,
                      void *infoData);

OSErr SPBSetDeviceInfo(int32_t inRefNum,
                      OSType infoType,
                      void *infoData);

OSErr SPBMillisecondsToBytes(int32_t inRefNum,
                           int32_t *milliseconds);

OSErr SPBBytesToMilliseconds(int32_t inRefNum,
                           int32_t *byteCount);

/* Sound Manager Information */
NumVersion SndSoundManagerVersion(void);
OSErr SndManagerStatus(int16_t theLength, SMStatus *theStatus);

/* Legacy Sound Manager 1.0 Compatibility */
void StartSound(const void *synthRec,
               int32_t numBytes,
               SoundCompletionUPP completionRtn);
void StopSound(void);
Boolean SoundDone(void);
void GetSoundVol(int16_t *level);
void SetSoundVol(int16_t level);

/* Utility Functions */
OSErr GetSoundPreference(OSType theType,
                        Str255 name,
                        Handle settings);

OSErr SetSoundPreference(OSType theType,
                        const Str255 name,
                        Handle settings);

/* Constants for GetSoundPreference/SetSoundPreference */
#define kSoundPrefsType         FOUR_CHAR_CODE('sout')
#define kGeneralSoundPrefs      FOUR_CHAR_CODE('genr')
#define kSoundEffectsPrefs      FOUR_CHAR_CODE('sfx ')
#define kSpeechPrefs           FOUR_CHAR_CODE('spch')

/* MIDI Support */
OSErr MIDISignIn(OSType clientID,
                int32_t refCon,
                Handle icon,
                Str255 name);

OSErr MIDISignOut(OSType clientID);

OSErr MIDIGetClients(OSType *clientIDs, int16_t *actualCount);
OSErr MIDIGetClientName(OSType clientID, Str255 name);

OSErr MIDIOpenPort(OSType clientID,
                  Str255 name,
                  MIDIPortDirectionFlags direction,
                  MIDIPortParams *params,
                  int32_t *portRefNum);

OSErr MIDIClosePort(int32_t portRefNum);

OSErr MIDIConnectPort(int32_t srcPortRefNum,
                     int32_t dstPortRefNum,
                     OSType connType);

OSErr MIDIDisconnectPort(int32_t srcPortRefNum, int32_t dstPortRefNum);

OSErr MIDISendData(int32_t portRefNum,
                  MIDIPacketListPtr packetList);

/* Internal Functions - Do Not Call Directly */
void _SoundManagerInterrupt(void);
OSErr _SoundManagerProcessCommands(void);
void _SoundManagerMixerCallback(void *userData,
                               int16_t *buffer,
                               uint32_t frameCount);

#ifdef __cplusplus
}
#endif

#endif /* _SOUNDMANAGER_H_ */