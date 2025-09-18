/*
 * File: SpeechOutput.h
 *
 * Contains: Speech output device control and routing for Speech Manager
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This header provides speech audio output functionality
 *              including device management, routing, and audio processing.
 */

#ifndef _SPEECHOUTPUT_H_
#define _SPEECHOUTPUT_H_

#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Audio Output Constants ===== */

/* Output device types */
typedef enum {
    kOutputDevice_Unknown       = 0,
    kOutputDevice_Speaker       = 1,  /* Built-in speaker */
    kOutputDevice_Headphones    = 2,  /* Headphones */
    kOutputDevice_LineOut       = 3,  /* Line output */
    kOutputDevice_USB           = 4,  /* USB audio device */
    kOutputDevice_Bluetooth     = 5,  /* Bluetooth audio */
    kOutputDevice_Network       = 6,  /* Network audio */
    kOutputDevice_File          = 7   /* File output */
} AudioOutputDeviceType;

/* Output quality levels */
typedef enum {
    kOutputQuality_Telephone    = 1,  /* 8kHz, 8-bit, mono */
    kOutputQuality_AM_Radio     = 2,  /* 11kHz, 8-bit, mono */
    kOutputQuality_FM_Radio     = 3,  /* 22kHz, 16-bit, stereo */
    kOutputQuality_CD           = 4,  /* 44.1kHz, 16-bit, stereo */
    kOutputQuality_DAT          = 5,  /* 48kHz, 16-bit, stereo */
    kOutputQuality_HighRes      = 6   /* 96kHz, 24-bit, stereo */
} AudioOutputQuality;

/* Output routing modes */
typedef enum {
    kRoutingMode_Automatic      = 0,  /* Automatic routing */
    kRoutingMode_Exclusive      = 1,  /* Exclusive device access */
    kRoutingMode_Mixed          = 2,  /* Mixed with other audio */
    kRoutingMode_Ducked         = 3,  /* Duck other audio */
    kRoutingMode_Priority       = 4   /* Priority over other audio */
} AudioRoutingMode;

/* Output processing flags */
typedef enum {
    kOutputFlag_Normalize       = (1 << 0),  /* Normalize audio levels */
    kOutputFlag_Compress        = (1 << 1),  /* Dynamic range compression */
    kOutputFlag_Equalize        = (1 << 2),  /* Apply equalization */
    kOutputFlag_SpatialAudio    = (1 << 3),  /* Spatial audio processing */
    kOutputFlag_NoiseGate       = (1 << 4),  /* Noise gate */
    kOutputFlag_Limiter         = (1 << 5),  /* Audio limiter */
    kOutputFlag_Echo            = (1 << 6),  /* Echo/reverb effects */
    kOutputFlag_Monitoring      = (1 << 7)   /* Audio monitoring */
} AudioOutputFlags;

/* ===== Audio Output Structures ===== */

/* Audio device information */
typedef struct AudioOutputDevice {
    char deviceName[64];            /* Device name */
    char deviceID[32];              /* Unique device identifier */
    AudioOutputDeviceType type;     /* Device type */
    bool isAvailable;               /* Whether device is available */
    bool isDefault;                 /* Whether device is default */
    long maxSampleRate;             /* Maximum sample rate */
    short maxChannels;              /* Maximum channels */
    short maxBitDepth;              /* Maximum bit depth */
    AudioOutputFlags supportedFlags; /* Supported processing flags */
    void *deviceSpecific;           /* Device-specific data */
} AudioOutputDevice;

/* Audio format specification */
typedef struct AudioOutputFormat {
    long sampleRate;                /* Sample rate in Hz */
    short channels;                 /* Number of channels */
    short bitsPerSample;            /* Bits per sample */
    bool isFloat;                   /* Whether samples are floating point */
    bool isBigEndian;               /* Byte order */
    bool isInterleaved;             /* Channel interleaving */
    long frameSize;                 /* Bytes per frame */
    void *formatExtensions;         /* Format-specific extensions */
} AudioOutputFormat;

/* Audio output configuration */
typedef struct AudioOutputConfig {
    char deviceID[32];              /* Target device ID */
    AudioOutputFormat format;       /* Audio format */
    AudioRoutingMode routingMode;   /* Routing mode */
    AudioOutputFlags flags;         /* Processing flags */
    Fixed volume;                   /* Output volume (0.0-1.0) */
    Fixed balance;                  /* Stereo balance (-1.0 to 1.0) */
    long bufferSize;                /* Buffer size in frames */
    long latency;                   /* Desired latency in ms */
    void *processingChain;          /* Audio processing chain */
} AudioOutputConfig;

/* Audio output statistics */
typedef struct AudioOutputStats {
    long totalFramesPlayed;         /* Total audio frames played */
    long totalBytes;                /* Total bytes output */
    long underrunCount;             /* Buffer underrun count */
    long overrunCount;              /* Buffer overrun count */
    double averageLatency;          /* Average latency in ms */
    double cpuUsage;                /* CPU usage percentage */
    long droppedFrames;             /* Dropped frames count */
    time_t sessionStartTime;        /* Session start time */
} AudioOutputStats;

/* ===== Audio Output Management ===== */

/* Output system initialization */
OSErr InitializeAudioOutput(void);
void CleanupAudioOutput(void);

/* Device enumeration */
OSErr CountAudioOutputDevices(short *deviceCount);
OSErr GetIndAudioOutputDevice(short index, AudioOutputDevice *device);
OSErr GetDefaultAudioOutputDevice(AudioOutputDevice *device);
OSErr SetDefaultAudioOutputDevice(const char *deviceID);

/* Device information */
OSErr GetAudioOutputDeviceInfo(const char *deviceID, AudioOutputDevice *device);
OSErr GetAudioOutputDeviceCapabilities(const char *deviceID, AudioOutputFormat **formats,
                                       short *formatCount, AudioOutputFlags *supportedFlags);

/* Device status */
bool IsAudioOutputDeviceAvailable(const char *deviceID);
OSErr GetAudioOutputDeviceStatus(const char *deviceID, bool *isActive, long *currentSampleRate,
                                 short *currentChannels);

/* ===== Audio Output Configuration ===== */

/* Output configuration */
OSErr CreateAudioOutputConfig(AudioOutputConfig **config);
OSErr DisposeAudioOutputConfig(AudioOutputConfig *config);
OSErr SetAudioOutputConfig(const AudioOutputConfig *config);
OSErr GetAudioOutputConfig(AudioOutputConfig *config);
OSErr ValidateAudioOutputConfig(const AudioOutputConfig *config, bool *isValid,
                                char **errorMessage);

/* Format management */
OSErr SetAudioOutputFormat(const AudioOutputFormat *format);
OSErr GetAudioOutputFormat(AudioOutputFormat *format);
OSErr GetBestAudioOutputFormat(const char *deviceID, AudioOutputQuality quality,
                               AudioOutputFormat *format);

/* Volume and balance control */
OSErr SetAudioOutputVolume(Fixed volume);
OSErr GetAudioOutputVolume(Fixed *volume);
OSErr SetAudioOutputBalance(Fixed balance);
OSErr GetAudioOutputBalance(Fixed *balance);
OSErr SetChannelVolume(short channel, Fixed volume);
OSErr GetChannelVolume(short channel, Fixed *volume);

/* ===== Audio Streaming ===== */

/* Stream management */
typedef struct AudioOutputStream AudioOutputStream;

OSErr CreateAudioOutputStream(const AudioOutputConfig *config, AudioOutputStream **stream);
OSErr DisposeAudioOutputStream(AudioOutputStream *stream);
OSErr OpenAudioOutputStream(AudioOutputStream *stream);
OSErr CloseAudioOutputStream(AudioOutputStream *stream);

/* Stream control */
OSErr StartAudioOutputStream(AudioOutputStream *stream);
OSErr StopAudioOutputStream(AudioOutputStream *stream);
OSErr PauseAudioOutputStream(AudioOutputStream *stream);
OSErr ResumeAudioOutputStream(AudioOutputStream *stream);
OSErr FlushAudioOutputStream(AudioOutputStream *stream);

/* Stream data */
OSErr WriteAudioData(AudioOutputStream *stream, const void *audioData, long dataSize,
                     long *framesWritten);
OSErr WriteAudioFrames(AudioOutputStream *stream, const void *audioFrames, long frameCount);
OSErr GetAudioStreamPosition(AudioOutputStream *stream, long *currentFrame, long *totalFrames);

/* Stream properties */
OSErr SetAudioStreamProperty(AudioOutputStream *stream, OSType property, const void *value,
                             long valueSize);
OSErr GetAudioStreamProperty(AudioOutputStream *stream, OSType property, void *value,
                             long *valueSize);

/* ===== Audio Processing ===== */

/* Audio effects and processing */
typedef struct AudioProcessor AudioProcessor;

OSErr CreateAudioProcessor(AudioOutputFlags processingFlags, AudioProcessor **processor);
OSErr DisposeAudioProcessor(AudioProcessor *processor);
OSErr ProcessAudioData(AudioProcessor *processor, void *audioData, long dataSize);

/* Built-in effects */
OSErr ApplyVolumeControl(void *audioData, long dataSize, const AudioOutputFormat *format,
                         Fixed volume);
OSErr ApplyNormalization(void *audioData, long dataSize, const AudioOutputFormat *format);
OSErr ApplyCompression(void *audioData, long dataSize, const AudioOutputFormat *format,
                       Fixed threshold, Fixed ratio);
OSErr ApplyEqualization(void *audioData, long dataSize, const AudioOutputFormat *format,
                        Fixed *bandGains, short bandCount);

/* Custom effects */
typedef OSErr (*AudioEffectProc)(void *audioData, long dataSize, const AudioOutputFormat *format,
                                  void *effectData, void *userData);

OSErr RegisterAudioEffect(OSType effectType, AudioEffectProc effectProc, void *userData);
OSErr ApplyCustomEffect(OSType effectType, void *audioData, long dataSize,
                        const AudioOutputFormat *format, void *effectData);

/* ===== Audio Routing ===== */

/* Routing control */
OSErr SetAudioRoutingMode(AudioRoutingMode mode);
OSErr GetAudioRoutingMode(AudioRoutingMode *mode);
OSErr RouteAudioToDevice(const char *deviceID);
OSErr GetCurrentAudioRoute(char *deviceID, long deviceIDSize);

/* Multi-device routing */
OSErr EnableMultiDeviceOutput(bool enable);
OSErr AddOutputDevice(const char *deviceID, Fixed volume);
OSErr RemoveOutputDevice(const char *deviceID);
OSErr GetActiveOutputDevices(char ***deviceIDs, short *deviceCount);

/* Routing policies */
OSErr SetAudioInterruptionPolicy(bool allowInterruptions);
OSErr SetAudioDuckingEnabled(bool enable);
OSErr SetAudioPriorityLevel(short priority);

/* ===== Audio Monitoring ===== */

/* Level monitoring */
OSErr EnableAudioLevelMonitoring(bool enable);
OSErr GetAudioLevels(Fixed *leftLevel, Fixed *rightLevel);
OSErr GetPeakLevels(Fixed *leftPeak, Fixed *rightPeak);
OSErr ResetPeakLevels(void);

/* Spectrum analysis */
OSErr EnableSpectrumAnalysis(bool enable);
OSErr GetAudioSpectrum(Fixed *spectrum, short bandCount);
OSErr SetSpectrumAnalysisParameters(short fftSize, short overlap);

/* Audio statistics */
OSErr GetAudioOutputStats(AudioOutputStats *stats);
OSErr ResetAudioOutputStats(void);

/* ===== Audio File Output ===== */

/* File output */
typedef enum {
    kAudioFileFormat_WAV        = 1,
    kAudioFileFormat_AIFF       = 2,
    kAudioFileFormat_MP3        = 3,
    kAudioFileFormat_AAC        = 4,
    kAudioFileFormat_FLAC       = 5,
    kAudioFileFormat_OGG        = 6
} AudioFileFormat;

OSErr StartAudioFileRecording(const char *filePath, AudioFileFormat format,
                              const AudioOutputFormat *audioFormat);
OSErr StopAudioFileRecording(void);
OSErr WriteAudioToFile(const char *filePath, const void *audioData, long dataSize,
                       const AudioOutputFormat *format, AudioFileFormat fileFormat);

/* ===== Audio Callbacks ===== */

/* Output callback */
typedef void (*AudioOutputProc)(AudioOutputStream *stream, void *audioData, long dataSize,
                                 void *userData);

/* Level callback */
typedef void (*AudioLevelProc)(Fixed leftLevel, Fixed rightLevel, Fixed leftPeak,
                                Fixed rightPeak, void *userData);

/* Device change callback */
typedef void (*AudioDeviceChangeProc)(const char *deviceID, bool deviceAdded, void *userData);

/* Buffer callback */
typedef void (*AudioBufferProc)(AudioOutputStream *stream, long bufferSize, long availableFrames,
                                 void *userData);

/* Callback registration */
OSErr SetAudioOutputCallback(AudioOutputStream *stream, AudioOutputProc callback, void *userData);
OSErr SetAudioLevelCallback(AudioLevelProc callback, void *userData);
OSErr SetAudioDeviceChangeCallback(AudioDeviceChangeProc callback, void *userData);
OSErr SetAudioBufferCallback(AudioOutputStream *stream, AudioBufferProc callback, void *userData);

/* ===== Platform Integration ===== */

/* Platform-specific audio support */
#ifdef _WIN32
OSErr InitializeDirectSoundOutput(void);
OSErr InitializeWASAPIOutput(void);
OSErr ConfigureWindowsAudioSession(void *sessionConfig);
#endif

#ifdef __APPLE__
OSErr InitializeCoreAudioOutput(void);
OSErr ConfigureAudioUnit(void *audioUnitConfig);
OSErr SetAudioSessionCategory(OSType category);
#endif

#ifdef __linux__
OSErr InitializeALSAOutput(void);
OSErr InitializePulseAudioOutput(void);
OSErr InitializeJACKOutput(void);
OSErr ConfigureLinuxAudioSystem(const char *configFile);
#endif

/* Cross-platform abstraction */
OSErr GetPlatformAudioInfo(char **platformName, char **driverVersion, long *capabilities);
OSErr SetPlatformAudioPreferences(const void *preferences);

/* ===== Audio Utilities ===== */

/* Format conversion */
OSErr ConvertAudioFormat(const void *inputData, long inputSize,
                         const AudioOutputFormat *inputFormat,
                         const AudioOutputFormat *outputFormat,
                         void **outputData, long *outputSize);

/* Sample rate conversion */
OSErr ResampleAudio(const void *inputData, long inputFrames,
                    const AudioOutputFormat *inputFormat, long outputSampleRate,
                    void **outputData, long *outputFrames);

/* Channel mapping */
OSErr MapAudioChannels(const void *inputData, long dataSize,
                       short inputChannels, short outputChannels,
                       void **outputData, long *outputSize);

/* Audio validation */
OSErr ValidateAudioData(const void *audioData, long dataSize,
                        const AudioOutputFormat *format, bool *isValid);
OSErr AnalyzeAudioData(const void *audioData, long dataSize,
                       const AudioOutputFormat *format, Fixed *rmsLevel, Fixed *peakLevel);

/* Timing utilities */
OSErr AudioFramesToTime(long frameCount, long sampleRate, long *timeMs);
OSErr AudioTimeToFrames(long timeMs, long sampleRate, long *frameCount);
OSErr GetCurrentAudioTime(long *timeMs);

/* ===== Audio Debugging ===== */

/* Debug information */
OSErr GetAudioOutputDebugInfo(char **debugInfo);
OSErr DumpAudioOutputState(FILE *output);
OSErr LogAudioActivity(const char *message);

/* Performance monitoring */
OSErr EnableAudioPerformanceMonitoring(bool enable);
OSErr GetAudioPerformanceData(double *cpuUsage, long *memoryUsage, long *bufferUsage);

/* Audio testing */
OSErr GenerateTestTone(double frequency, long durationMs, const AudioOutputFormat *format,
                       void **audioData, long *dataSize);
OSErr PlayTestTone(double frequency, long durationMs);
OSErr TestAudioOutputDevice(const char *deviceID, bool *isWorking, char **errorMessage);

#ifdef __cplusplus
}
#endif

#endif /* _SPEECHOUTPUT_H_ */