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

#include <stdint.h>
#include <stdbool.h>
#include "SoundTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform Audio API Types */
typedef enum {
    AUDIO_API_AUTO = 0,         /* Auto-detect best API */
    AUDIO_API_ALSA = 1,         /* ALSA (Linux) */
    AUDIO_API_PULSE = 2,        /* PulseAudio (Linux/Unix) */
    AUDIO_API_COREAUDIO = 3,    /* CoreAudio (macOS) */
    AUDIO_API_WASAPI = 4,       /* WASAPI (Windows) */
    AUDIO_API_DSOUND = 5,       /* DirectSound (Windows) */
    AUDIO_API_OSS = 6,          /* OSS (Unix) */
    AUDIO_API_JACK = 7,         /* JACK (Linux/Unix) */
    AUDIO_API_DUMMY = 8         /* Dummy/Null driver */
} AudioAPIType;

/* Audio Device Types */
typedef enum {
    AUDIO_DEVICE_OUTPUT = 1,    /* Audio output device */
    AUDIO_DEVICE_INPUT = 2,     /* Audio input device */
    AUDIO_DEVICE_DUPLEX = 3     /* Full duplex device */
} AudioDeviceType;

/* Audio Format Description */
typedef struct AudioFormat {
    uint32_t        sampleRate;     /* Sample rate (Hz) */
    uint16_t        channels;       /* Number of channels */
    uint16_t        bitsPerSample;  /* Bits per sample */
    uint16_t        bytesPerFrame;  /* Bytes per frame */
    uint32_t        bytesPerSecond; /* Bytes per second */
    AudioEncodingType encoding;     /* Sample encoding type */
    bool            bigEndian;      /* Big endian byte order */
    bool            signedSamples;  /* Signed sample format */
} AudioFormat;

/* Audio Device Information */
typedef struct AudioDeviceInfo {
    char            name[256];          /* Device name */
    char            description[512];   /* Device description */
    AudioDeviceType type;               /* Device type */
    uint32_t        maxSampleRate;      /* Maximum sample rate */
    uint32_t        minSampleRate;      /* Minimum sample rate */
    uint16_t        maxChannels;        /* Maximum channels */
    uint16_t        minChannels;        /* Minimum channels */
    uint16_t        maxBitsPerSample;   /* Maximum bits per sample */
    uint16_t        minBitsPerSample;   /* Minimum bits per sample */
    uint32_t        maxBufferSize;      /* Maximum buffer size */
    uint32_t        minBufferSize;      /* Minimum buffer size */
    bool            isDefault;          /* Default device */
    void*           platformData;       /* Platform-specific data */
} AudioDeviceInfo;

/* Audio Stream Configuration */
typedef struct AudioStreamConfig {
    AudioFormat     format;             /* Audio format */
    uint32_t        bufferFrames;       /* Buffer size in frames */
    uint32_t        bufferCount;        /* Number of buffers */
    uint32_t        latencyFrames;      /* Desired latency in frames */
    bool            exclusive;          /* Exclusive mode */
    void*           userData;           /* User data for callbacks */
} AudioStreamConfig;

/* Audio Stream State */
typedef enum {
    AUDIO_STREAM_CLOSED = 0,    /* Stream is closed */
    AUDIO_STREAM_STOPPED = 1,   /* Stream is stopped */
    AUDIO_STREAM_RUNNING = 2,   /* Stream is running */
    AUDIO_STREAM_PAUSED = 3,    /* Stream is paused */
    AUDIO_STREAM_ERROR = 4      /* Stream has error */
} AudioStreamState;

/* Audio Stream Statistics */
typedef struct AudioStreamStats {
    uint64_t        framesProcessed;    /* Total frames processed */
    uint64_t        underruns;          /* Output buffer underruns */
    uint64_t        overruns;           /* Input buffer overruns */
    uint32_t        currentLatency;     /* Current latency in frames */
    double          cpuLoad;            /* Current CPU load (0.0-1.0) */
    uint32_t        dropouts;           /* Audio dropouts */
} AudioStreamStats;

/* Forward declarations */
struct SoundHardware;
struct AudioStream;

typedef struct SoundHardware*   SoundHardwarePtr;
typedef struct AudioStream*     AudioStreamPtr;

/* Audio Stream Callbacks */
typedef void (*AudioOutputCallback)(void* userData,
                                   int16_t* outputBuffer,
                                   uint32_t frameCount);

typedef void (*AudioInputCallback)(void* userData,
                                  const int16_t* inputBuffer,
                                  uint32_t frameCount);

typedef void (*AudioDuplexCallback)(void* userData,
                                   const int16_t* inputBuffer,
                                   int16_t* outputBuffer,
                                   uint32_t frameCount);

typedef void (*AudioStreamCallback)(AudioStreamPtr stream,
                                   uint32_t event,
                                   void* userData);

/* Sound Hardware Structure */
typedef struct SoundHardware {
    AudioAPIType        apiType;            /* Audio API in use */
    char                apiName[64];        /* API name */
    uint32_t            deviceCount;        /* Number of devices */
    AudioDeviceInfo*    devices;            /* Available devices */
    AudioDeviceInfo*    defaultOutput;     /* Default output device */
    AudioDeviceInfo*    defaultInput;      /* Default input device */
    bool                initialized;        /* Hardware initialized */
    void*               privateData;        /* Private platform data */

    /* Function pointers for platform-specific operations */
    OSErr (*init)(struct SoundHardware* hw);
    OSErr (*shutdown)(struct SoundHardware* hw);
    OSErr (*enumDevices)(struct SoundHardware* hw);
    OSErr (*openStream)(struct SoundHardware* hw,
                       AudioStreamPtr* stream,
                       AudioDeviceInfo* device,
                       AudioStreamConfig* config);
} SoundHardware;

/* Audio Stream Structure */
typedef struct AudioStream {
    SoundHardwarePtr    hardware;           /* Parent hardware */
    AudioDeviceInfo*    device;             /* Associated device */
    AudioStreamConfig   config;             /* Stream configuration */
    AudioStreamState    state;              /* Current state */
    AudioStreamStats    stats;              /* Stream statistics */

    /* Callback functions */
    AudioOutputCallback outputCallback;     /* Output callback */
    AudioInputCallback  inputCallback;      /* Input callback */
    AudioDuplexCallback duplexCallback;     /* Duplex callback */
    AudioStreamCallback streamCallback;     /* Stream event callback */
    void*               callbackUserData;   /* Callback user data */

    /* Buffer management */
    int16_t*            inputBuffer;        /* Input buffer */
    int16_t*            outputBuffer;       /* Output buffer */
    uint32_t            bufferFrames;       /* Buffer size in frames */
    uint32_t            bufferCount;        /* Number of buffers */
    volatile uint32_t   bufferIndex;        /* Current buffer index */

    /* Synchronization */
    void*               mutex;              /* Stream mutex */
    void*               condition;          /* Condition variable */
    volatile bool       stopRequested;     /* Stop requested flag */

    void*               platformData;       /* Platform-specific data */
} AudioStream;

/* Hardware Management Functions */
OSErr SoundHardwareInit(SoundHardwarePtr* hardware, AudioAPIType apiType);
OSErr SoundHardwareShutdown(SoundHardwarePtr hardware);
OSErr SoundHardwareEnumerateDevices(SoundHardwarePtr hardware);
OSErr SoundHardwareRefreshDevices(SoundHardwarePtr hardware);

/* Device Query Functions */
uint32_t SoundHardwareGetDeviceCount(SoundHardwarePtr hardware);
AudioDeviceInfo* SoundHardwareGetDevice(SoundHardwarePtr hardware, uint32_t index);
AudioDeviceInfo* SoundHardwareGetDefaultOutputDevice(SoundHardwarePtr hardware);
AudioDeviceInfo* SoundHardwareGetDefaultInputDevice(SoundHardwarePtr hardware);
AudioDeviceInfo* SoundHardwareFindDevice(SoundHardwarePtr hardware,
                                         const char* name,
                                         AudioDeviceType type);

/* Audio Stream Management */
OSErr AudioStreamOpen(SoundHardwarePtr hardware,
                     AudioStreamPtr* stream,
                     AudioDeviceInfo* device,
                     AudioStreamConfig* config);

OSErr AudioStreamClose(AudioStreamPtr stream);
OSErr AudioStreamStart(AudioStreamPtr stream);
OSErr AudioStreamStop(AudioStreamPtr stream);
OSErr AudioStreamPause(AudioStreamPtr stream);
OSErr AudioStreamResume(AudioStreamPtr stream);

/* Stream Configuration */
OSErr AudioStreamSetFormat(AudioStreamPtr stream, AudioFormat* format);
OSErr AudioStreamGetFormat(AudioStreamPtr stream, AudioFormat* format);
OSErr AudioStreamSetBufferSize(AudioStreamPtr stream, uint32_t bufferFrames);
OSErr AudioStreamGetBufferSize(AudioStreamPtr stream, uint32_t* bufferFrames);

/* Stream Information */
AudioStreamState AudioStreamGetState(AudioStreamPtr stream);
OSErr AudioStreamGetStats(AudioStreamPtr stream, AudioStreamStats* stats);
OSErr AudioStreamGetLatency(AudioStreamPtr stream, uint32_t* latencyFrames);

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
                               int16_t** buffer,
                               uint32_t* frameCount);

OSErr AudioStreamGetOutputBuffer(AudioStreamPtr stream,
                                int16_t** buffer,
                                uint32_t* frameCount);

OSErr AudioStreamReleaseBuffer(AudioStreamPtr stream);

/* Volume Control */
OSErr AudioStreamSetVolume(AudioStreamPtr stream, float volume);
OSErr AudioStreamGetVolume(AudioStreamPtr stream, float* volume);
OSErr AudioStreamSetMute(AudioStreamPtr stream, bool muted);
OSErr AudioStreamGetMute(AudioStreamPtr stream, bool* muted);

/* Format Utilities */
bool AudioFormatIsSupported(AudioDeviceInfo* device, AudioFormat* format);
OSErr AudioFormatGetBestMatch(AudioDeviceInfo* device,
                             AudioFormat* desired,
                             AudioFormat* best);

uint32_t AudioFormatGetBytesPerFrame(AudioFormat* format);
uint32_t AudioFormatGetBytesPerSecond(AudioFormat* format);
uint32_t AudioFormatFramesToBytes(AudioFormat* format, uint32_t frames);
uint32_t AudioFormatBytesToFrames(AudioFormat* format, uint32_t bytes);

/* Conversion Functions */
void AudioConvertFormat(void* srcBuffer, AudioFormat* srcFormat,
                       void* dstBuffer, AudioFormat* dstFormat,
                       uint32_t frameCount);

void AudioConvertSampleRate(int16_t* srcBuffer, uint32_t srcFrames, uint32_t srcRate,
                           int16_t* dstBuffer, uint32_t* dstFrames, uint32_t dstRate);

void AudioConvertChannels(int16_t* srcBuffer, uint16_t srcChannels,
                         int16_t* dstBuffer, uint16_t dstChannels,
                         uint32_t frameCount);

/* Platform-specific Hardware Implementations */
#ifdef __linux__
OSErr SoundHardwareInitALSA(SoundHardwarePtr hardware);
OSErr SoundHardwareInitPulse(SoundHardwarePtr hardware);
OSErr SoundHardwareInitOSS(SoundHardwarePtr hardware);
OSErr SoundHardwareInitJACK(SoundHardwarePtr hardware);
#endif

#ifdef __APPLE__
OSErr SoundHardwareInitCoreAudio(SoundHardwarePtr hardware);
#endif

#ifdef _WIN32
OSErr SoundHardwareInitWASAPI(SoundHardwarePtr hardware);
OSErr SoundHardwareInitDirectSound(SoundHardwarePtr hardware);
#endif

/* Dummy/Null driver for testing */
OSErr SoundHardwareInitDummy(SoundHardwarePtr hardware);

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
#define AUDIO_CAP_HARDWARE_MUTE         0x80

/* Recording Device State */
typedef struct RecorderState {
    bool                initialized;        /* Recorder initialized */
    AudioStreamPtr      inputStream;        /* Input audio stream */
    AudioFormat         format;             /* Recording format */
    int16_t*            recordBuffer;       /* Recording buffer */
    uint32_t            bufferFrames;       /* Buffer size in frames */
    uint32_t            recordedFrames;     /* Frames recorded */
    bool                recording;          /* Currently recording */
    bool                paused;             /* Recording paused */
    void*               userData;           /* User data */
    void                (*callback)(void* userData, int16_t* buffer, uint32_t frames);
} RecorderState;

typedef struct RecorderState* RecorderPtr;

/* Audio Recording Functions */
OSErr AudioRecorderInit(RecorderPtr* recorder, SoundHardwarePtr hardware);
OSErr AudioRecorderShutdown(RecorderPtr recorder);
OSErr AudioRecorderSetFormat(RecorderPtr recorder, AudioFormat* format);
OSErr AudioRecorderStart(RecorderPtr recorder);
OSErr AudioRecorderStop(RecorderPtr recorder);
OSErr AudioRecorderPause(RecorderPtr recorder);
OSErr AudioRecorderResume(RecorderPtr recorder);
OSErr AudioRecorderGetData(RecorderPtr recorder, int16_t** buffer, uint32_t* frameCount);

#ifdef __cplusplus
}
#endif

#endif /* _SOUNDHARDWARE_H_ */