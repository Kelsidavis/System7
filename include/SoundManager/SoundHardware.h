/*
 * SoundHardware.h - Sound Hardware Abstraction Layer
 *
 * Provides a unified interface for audio hardware on different platforms.
 * Abstracts platform-specific audio APIs (ALSA, PulseAudio, CoreAudio,
 * WASAPI) behind a common interface for the Mac OS Sound Manager.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef _SOUNDHARDWARE_H_
#define _SOUNDHARDWARE_H_

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

#include "SoundTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform Audio API Types */
/* AudioAPIType is defined in SystemTypes.h as UInt32 */
#define AUDIO_API_DUMMY      0
#define AUDIO_API_ALSA       1
#define AUDIO_API_PULSE      2
#define AUDIO_API_COREAUDIO  3
#define AUDIO_API_WASAPI     4
#define AUDIO_API_AUTO       0xFF  /* Auto-detect based on platform */

/* Audio Device Types */
typedef enum {
    AUDIO_DEVICE_OUTPUT = 1,
    AUDIO_DEVICE_INPUT = 2,
    AUDIO_DEVICE_DUPLEX = 3
} AudioDeviceType;

/* Audio encoding types */
#define k16BitBigEndianFormat    1

/* Audio Format Description */
typedef struct {
    UInt32 sampleRate;
    UInt16 channels;
    UInt16 bitsPerSample;
    UInt32 bytesPerFrame;
    UInt32 bytesPerSecond;
    UInt16 encoding;
    Boolean bigEndian;
    Boolean signedSamples;
} AudioFormat;

/* Audio Device Information */
/* AudioDeviceInfo is forward declared in SystemTypes.h */
struct AudioDeviceInfo {
    char name[256];
    char description[256];
    AudioDeviceType type;
    Boolean isDefault;
};

/* Audio Stream Configuration */
typedef struct {
    AudioFormat format;
    UInt32 bufferFrames;
} AudioStreamConfig;

/* Audio Stream State */
typedef enum {
    AUDIO_STREAM_STOPPED = 0,
    AUDIO_STREAM_RUNNING = 1,
    AUDIO_STREAM_PAUSED = 2
} AudioStreamState;

/* Audio Stream Statistics */
typedef struct {
    UInt32 underruns;
    UInt32 overruns;
} AudioStreamStats;

/* Forward declarations */
typedef struct SoundHardware* SoundHardwarePtr;
typedef struct AudioStream* AudioStreamPtr;

/* Audio Stream Callbacks */
typedef void (*AudioOutputCallback)(void* userData, SInt16* buffer, UInt32 frameCount);
typedef void (*AudioInputCallback)(void* userData, const SInt16* buffer, UInt32 frameCount);
typedef void (*AudioDuplexCallback)(void* userData, const SInt16* inBuffer, SInt16* outBuffer, UInt32 frameCount);
typedef void (*AudioStreamCallback)(void* userData, AudioStreamPtr stream, UInt32 eventType);

/* Sound Hardware Structure */
typedef struct SoundHardware {
    AudioAPIType apiType;
    char apiName[256];
    Boolean initialized;
    UInt32 deviceCount;
    AudioDeviceInfo* devices;
    AudioDeviceInfo* defaultOutput;
    AudioDeviceInfo* defaultInput;
} SoundHardware;

/* Audio Stream Structure */
typedef struct AudioStream {
    SoundHardwarePtr hardware;
    AudioDeviceInfo* device;
    AudioStreamConfig config;
    AudioStreamState state;
} AudioStream;

/* Recorder Structure */
typedef struct AudioRecorder* RecorderPtr;


/* Device Query Functions */
AudioDeviceInfo* SoundHardwareFindDevice(SoundHardwarePtr hardware,
                                         const char* name,
                                         AudioDeviceType type);

/* Audio Stream Management */
OSErr AudioStreamOpen(SoundHardwarePtr hardware,
                     AudioStreamPtr* stream,
                     AudioDeviceInfo* device,
                     AudioStreamConfig* config);


/* Callback Management */
OSErr AudioStreamSetOutputCallback(AudioStreamPtr stream,
                                  AudioOutputCallback callback,
                                  void* userData);

OSErr AudioStreamSetInputCallback(AudioStreamPtr stream,
                                 AudioInputCallback callback,
                                 void* userData);

OSErr AudioStreamSetDuplexCallback(AudioStreamPtr stream,
                                  AudioDuplexCallback callback,
                                  void* userData);

OSErr AudioStreamSetStreamCallback(AudioStreamPtr stream,
                                  AudioStreamCallback callback,
                                  void* userData);

/* Buffer Management */
OSErr AudioStreamGetInputBuffer(AudioStreamPtr stream,
                               SInt16** buffer,
                               UInt32* frameCount);

OSErr AudioStreamGetOutputBuffer(AudioStreamPtr stream,
                                SInt16** buffer,
                                UInt32* frameCount);


/* Format Utilities */
OSErr AudioFormatGetBestMatch(AudioDeviceInfo* device,
                             AudioFormat* desired,
                             AudioFormat* best);


/* Conversion Functions */
void AudioConvertFormat(void* srcBuffer, AudioFormat* srcFormat,
                       void* dstBuffer, AudioFormat* dstFormat,
                       UInt32 frameCount);

void AudioConvertSampleRate(SInt16* srcBuffer, UInt32 srcFrames, UInt32 srcRate,
                           SInt16* dstBuffer, UInt32* dstFrames, UInt32 dstRate);

void AudioConvertChannels(SInt16* srcBuffer, UInt16 srcChannels,
                         SInt16* dstBuffer, UInt16 dstChannels,
                         UInt32 frameCount);

/* Platform-specific Hardware Implementations */
#ifdef PLATFORM_REMOVED_LINUX
#endif

#ifdef PLATFORM_REMOVED_APPLE
#endif

#ifdef PLATFORM_REMOVED_WIN32
#endif


/* Stream Event Types */
#define AUDIO_STREAM_EVENT_STARTED      1
#define AUDIO_STREAM_EVENT_STOPPED      2
#define AUDIO_STREAM_EVENT_PAUSED       3
#define AUDIO_STREAM_EVENT_RESUMED      4
#define AUDIO_STREAM_EVENT_ERROR        5
#define AUDIO_STREAM_EVENT_UNDERRUN     6
#define AUDIO_STREAM_EVENT_OVERRUN      7
#define AUDIO_STREAM_EVENT_DROPOUT      8

/* Error Codes */
#define AUDIO_ERROR_SUCCESS             0
#define AUDIO_ERROR_INVALID_PARAM      -1
#define AUDIO_ERROR_NO_DEVICE          -2
#define AUDIO_ERROR_DEVICE_BUSY        -3
#define AUDIO_ERROR_FORMAT_NOT_SUPPORTED -4
#define AUDIO_ERROR_BUFFER_TOO_SMALL   -5
#define AUDIO_ERROR_BUFFER_TOO_LARGE   -6
#define AUDIO_ERROR_MEMORY_ERROR       -7
#define AUDIO_ERROR_HARDWARE_ERROR     -8

#include "SystemTypes.h"
#define AUDIO_ERROR_NOT_INITIALIZED    -9
#define AUDIO_ERROR_ALREADY_RUNNING    -10
#define AUDIO_ERROR_NOT_RUNNING        -11

/* Standard Audio Formats */
extern const AudioFormat AUDIO_FORMAT_CD;          /* 44.1kHz, 16-bit, stereo */
extern const AudioFormat AUDIO_FORMAT_DAT;         /* 48kHz, 16-bit, stereo */
extern const AudioFormat AUDIO_FORMAT_MAC_22K;     /* 22.254kHz, 16-bit, stereo */
extern const AudioFormat AUDIO_FORMAT_MAC_11K;     /* 11.127kHz, 8-bit, mono */
extern const AudioFormat AUDIO_FORMAT_PHONE;       /* 8kHz, 8-bit, mono */

/* Hardware Capability Flags */
#define AUDIO_CAP_OUTPUT                0x01
#define AUDIO_CAP_INPUT                 0x02
#define AUDIO_CAP_DUPLEX                0x04
#define AUDIO_CAP_EXCLUSIVE             0x08
#define AUDIO_CAP_MMAP                  0x10
#define AUDIO_CAP_REALTIME              0x20
#define AUDIO_CAP_HARDWARE_VOLUME       0x40

#include "SystemTypes.h"
#define AUDIO_CAP_HARDWARE_MUTE         0x80

#include "SystemTypes.h"

/* Recording Device State */

/* Ptr is defined in MacTypes.h */


/* PC Speaker Functions (x86 platform) */
void PCSpkr_Beep(uint32_t frequency, uint32_t duration_ms);
int PCSpkr_Init(void);
void PCSpkr_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* _SOUNDHARDWARE_H_ */