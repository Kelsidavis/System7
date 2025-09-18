/*
 * File: SpeechOutput.c
 *
 * Contains: Speech output device control and routing implementation
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This file implements speech audio output functionality
 *              including device management, routing, and audio processing.
 */

#include "../include/SpeechManager/SpeechOutput.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

/* ===== Audio Output Implementation ===== */

/* Maximum limits */
#define MAX_OUTPUT_DEVICES 32
#define MAX_OUTPUT_STREAMS 16
#define DEFAULT_BUFFER_SIZE 4096
#define DEFAULT_SAMPLE_RATE 22050
#define MAX_AUDIO_CHANNELS 8

/* Audio output stream structure */
struct AudioOutputStream {
    bool isOpen;
    bool isActive;
    char deviceID[32];
    AudioOutputFormat format;
    AudioOutputConfig config;

    /* Audio buffers */
    void *audioBuffer;
    long bufferSize;
    long bufferFrames;
    long currentFrame;
    long totalFrames;

    /* Synchronization */
    pthread_mutex_t streamMutex;
    pthread_cond_t bufferCondition;

    /* Callbacks */
    AudioOutputProc outputCallback;
    void *outputUserData;
    AudioBufferProc bufferCallback;
    void *bufferUserData;

    /* Statistics */
    AudioOutputStats stats;

    /* Platform-specific data */
    void *platformData;
};

/* Audio output manager state */
typedef struct AudioOutputManager {
    bool initialized;
    AudioOutputDevice devices[MAX_OUTPUT_DEVICES];
    short deviceCount;
    char defaultDeviceID[32];

    AudioOutputStream streams[MAX_OUTPUT_STREAMS];
    short streamCount;

    /* Global settings */
    AudioOutputConfig defaultConfig;
    AudioRoutingMode routingMode;
    Fixed masterVolume;
    Fixed masterBalance;
    bool levelMonitoringEnabled;
    bool spectrumAnalysisEnabled;

    /* Callbacks */
    AudioLevelProc levelCallback;
    void *levelUserData;
    AudioDeviceChangeProc deviceChangeCallback;
    void *deviceChangeUserData;

    /* Statistics */
    AudioOutputStats globalStats;

    /* Synchronization */
    pthread_mutex_t managerMutex;
} AudioOutputManager;

static AudioOutputManager gAudioManager = {
    .initialized = false,
    .deviceCount = 0,
    .streamCount = 0,
    .routingMode = kRoutingMode_Automatic,
    .masterVolume = 0x00010000,  /* 1.0 */
    .masterBalance = 0x00000000, /* 0.0 (center) */
    .levelMonitoringEnabled = false,
    .spectrumAnalysisEnabled = false
};

/* Built-in audio devices */
static const AudioOutputDevice kBuiltinDevices[] = {
    {
        .deviceName = "Default Audio Device",
        .deviceID = "default",
        .type = kOutputDevice_Speaker,
        .isAvailable = true,
        .isDefault = true,
        .maxSampleRate = 48000,
        .maxChannels = 2,
        .maxBitDepth = 16,
        .supportedFlags = kOutputFlag_Normalize | kOutputFlag_Compress,
        .deviceSpecific = NULL
    },
    {
        .deviceName = "System Speaker",
        .deviceID = "speaker",
        .type = kOutputDevice_Speaker,
        .isAvailable = true,
        .isDefault = false,
        .maxSampleRate = 22050,
        .maxChannels = 1,
        .maxBitDepth = 16,
        .supportedFlags = kOutputFlag_Normalize,
        .deviceSpecific = NULL
    }
};

#define BUILTIN_DEVICE_COUNT (sizeof(kBuiltinDevices) / sizeof(AudioOutputDevice))

/* ===== Internal Functions ===== */

/*
 * InitializeDefaultConfig
 * Initializes default audio output configuration
 */
static void InitializeDefaultConfig(AudioOutputConfig *config) {
    memset(config, 0, sizeof(AudioOutputConfig));

    strcpy(config->deviceID, "default");
    config->format.sampleRate = DEFAULT_SAMPLE_RATE;
    config->format.channels = 1;
    config->format.bitsPerSample = 16;
    config->format.isFloat = false;
    config->format.isBigEndian = false;
    config->format.isInterleaved = true;
    config->format.frameSize = 2; /* 16-bit mono */

    config->routingMode = kRoutingMode_Automatic;
    config->flags = kOutputFlag_Normalize;
    config->volume = 0x00010000; /* 1.0 */
    config->balance = 0x00000000; /* 0.0 (center) */
    config->bufferSize = DEFAULT_BUFFER_SIZE;
    config->latency = 100; /* 100ms */
}

/*
 * FindOutputDevice
 * Finds an output device by ID
 */
static AudioOutputDevice *FindOutputDevice(const char *deviceID) {
    if (!deviceID) return NULL;

    for (short i = 0; i < gAudioManager.deviceCount; i++) {
        if (strcmp(gAudioManager.devices[i].deviceID, deviceID) == 0) {
            return &gAudioManager.devices[i];
        }
    }
    return NULL;
}

/*
 * FindOutputStream
 * Finds an output stream by reference
 */
static AudioOutputStream *FindOutputStream(AudioOutputStream *stream) {
    if (!stream) return NULL;

    for (short i = 0; i < gAudioManager.streamCount; i++) {
        if (&gAudioManager.streams[i] == stream) {
            return stream;
        }
    }
    return NULL;
}

/*
 * ApplyVolumeToBuffer
 * Applies volume control to audio buffer
 */
static void ApplyVolumeToBuffer(void *audioData, long frameCount,
                                const AudioOutputFormat *format, Fixed volume) {
    if (!audioData || frameCount <= 0 || !format) return;

    double volumeDouble = (double)volume / 65536.0; /* Convert Fixed to double */

    if (format->bitsPerSample == 16) {
        short *samples = (short *)audioData;
        long sampleCount = frameCount * format->channels;

        for (long i = 0; i < sampleCount; i++) {
            samples[i] = (short)(samples[i] * volumeDouble);
        }
    } else if (format->bitsPerSample == 8) {
        unsigned char *samples = (unsigned char *)audioData;
        long sampleCount = frameCount * format->channels;

        for (long i = 0; i < sampleCount; i++) {
            int sample = (samples[i] - 128) * volumeDouble + 128;
            samples[i] = (unsigned char)((sample < 0) ? 0 : (sample > 255) ? 255 : sample);
        }
    }
}

/*
 * ProcessAudioBuffer
 * Processes audio buffer with effects and routing
 */
static OSErr ProcessAudioBuffer(AudioOutputStream *stream, void *audioData, long frameCount) {
    if (!stream || !audioData || frameCount <= 0) {
        return paramErr;
    }

    /* Apply volume control */
    ApplyVolumeToBuffer(audioData, frameCount, &stream->format, stream->config.volume);

    /* Apply master volume */
    ApplyVolumeToBuffer(audioData, frameCount, &stream->format, gAudioManager.masterVolume);

    /* Apply normalization if enabled */
    if (stream->config.flags & kOutputFlag_Normalize) {
        /* Simple normalization - find peak and scale */
        if (stream->format.bitsPerSample == 16) {
            short *samples = (short *)audioData;
            long sampleCount = frameCount * stream->format.channels;
            short peak = 0;

            /* Find peak */
            for (long i = 0; i < sampleCount; i++) {
                short absSample = abs(samples[i]);
                if (absSample > peak) peak = absSample;
            }

            /* Normalize if needed */
            if (peak > 0 && peak < 16384) { /* Only if peak is less than 50% */
                double scale = 16384.0 / peak;
                for (long i = 0; i < sampleCount; i++) {
                    samples[i] = (short)(samples[i] * scale);
                }
            }
        }
    }

    /* Update statistics */
    stream->stats.totalFramesPlayed += frameCount;
    stream->stats.totalBytes += frameCount * stream->format.frameSize;
    stream->currentFrame += frameCount;

    return noErr;
}

/*
 * NotifyLevelCallback
 * Notifies level monitoring callback
 */
static void NotifyLevelCallback(Fixed leftLevel, Fixed rightLevel) {
    if (gAudioManager.levelCallback && gAudioManager.levelMonitoringEnabled) {
        gAudioManager.levelCallback(leftLevel, rightLevel, leftLevel, rightLevel,
                                     gAudioManager.levelUserData);
    }
}

/* ===== Public API Implementation ===== */

/*
 * InitializeAudioOutput
 * Initializes the audio output system
 */
OSErr InitializeAudioOutput(void) {
    if (gAudioManager.initialized) {
        return noErr;
    }

    /* Initialize manager state */
    memset(&gAudioManager, 0, sizeof(AudioOutputManager));

    /* Initialize mutex */
    int result = pthread_mutex_init(&gAudioManager.managerMutex, NULL);
    if (result != 0) {
        return memFullErr;
    }

    /* Set up built-in devices */
    for (int i = 0; i < BUILTIN_DEVICE_COUNT; i++) {
        gAudioManager.devices[i] = kBuiltinDevices[i];
        gAudioManager.deviceCount++;
    }

    /* Set default device */
    strcpy(gAudioManager.defaultDeviceID, "default");

    /* Initialize default configuration */
    InitializeDefaultConfig(&gAudioManager.defaultConfig);

    /* Set default values */
    gAudioManager.routingMode = kRoutingMode_Automatic;
    gAudioManager.masterVolume = 0x00010000; /* 1.0 */
    gAudioManager.masterBalance = 0x00000000; /* 0.0 */

    gAudioManager.initialized = true;
    return noErr;
}

/*
 * CleanupAudioOutput
 * Cleans up audio output resources
 */
void CleanupAudioOutput(void) {
    if (!gAudioManager.initialized) {
        return;
    }

    pthread_mutex_lock(&gAudioManager.managerMutex);

    /* Close all active streams */
    for (short i = 0; i < gAudioManager.streamCount; i++) {
        AudioOutputStream *stream = &gAudioManager.streams[i];
        if (stream->isOpen) {
            CloseAudioOutputStream(stream);
        }
        if (stream->audioBuffer) {
            free(stream->audioBuffer);
        }
        pthread_mutex_destroy(&stream->streamMutex);
        pthread_cond_destroy(&stream->bufferCondition);
    }

    gAudioManager.initialized = false;
    gAudioManager.deviceCount = 0;
    gAudioManager.streamCount = 0;

    pthread_mutex_unlock(&gAudioManager.managerMutex);
    pthread_mutex_destroy(&gAudioManager.managerMutex);
}

/*
 * CountAudioOutputDevices
 * Returns the number of available audio output devices
 */
OSErr CountAudioOutputDevices(short *deviceCount) {
    if (!deviceCount) {
        return paramErr;
    }

    if (!gAudioManager.initialized) {
        OSErr err = InitializeAudioOutput();
        if (err != noErr) {
            return err;
        }
    }

    *deviceCount = gAudioManager.deviceCount;
    return noErr;
}

/*
 * GetIndAudioOutputDevice
 * Gets audio output device information by index
 */
OSErr GetIndAudioOutputDevice(short index, AudioOutputDevice *device) {
    if (!device || index < 1) {
        return paramErr;
    }

    if (!gAudioManager.initialized) {
        OSErr err = InitializeAudioOutput();
        if (err != noErr) {
            return err;
        }
    }

    if (index > gAudioManager.deviceCount) {
        return paramErr;
    }

    *device = gAudioManager.devices[index - 1];
    return noErr;
}

/*
 * GetDefaultAudioOutputDevice
 * Gets the default audio output device
 */
OSErr GetDefaultAudioOutputDevice(AudioOutputDevice *device) {
    if (!device) {
        return paramErr;
    }

    if (!gAudioManager.initialized) {
        OSErr err = InitializeAudioOutput();
        if (err != noErr) {
            return err;
        }
    }

    AudioOutputDevice *defaultDevice = FindOutputDevice(gAudioManager.defaultDeviceID);
    if (!defaultDevice) {
        return resNotFound;
    }

    *device = *defaultDevice;
    return noErr;
}

/*
 * CreateAudioOutputStream
 * Creates a new audio output stream
 */
OSErr CreateAudioOutputStream(const AudioOutputConfig *config, AudioOutputStream **stream) {
    if (!config || !stream) {
        return paramErr;
    }

    if (!gAudioManager.initialized) {
        OSErr err = InitializeAudioOutput();
        if (err != noErr) {
            return err;
        }
    }

    pthread_mutex_lock(&gAudioManager.managerMutex);

    if (gAudioManager.streamCount >= MAX_OUTPUT_STREAMS) {
        pthread_mutex_unlock(&gAudioManager.managerMutex);
        return memFullErr;
    }

    /* Allocate stream */
    AudioOutputStream *newStream = &gAudioManager.streams[gAudioManager.streamCount];
    memset(newStream, 0, sizeof(AudioOutputStream));

    /* Initialize stream */
    newStream->config = *config;
    strcpy(newStream->deviceID, config->deviceID);
    newStream->format = config->format;

    /* Calculate frame size */
    newStream->format.frameSize = (newStream->format.bitsPerSample / 8) * newStream->format.channels;

    /* Allocate audio buffer */
    newStream->bufferFrames = config->bufferSize;
    newStream->bufferSize = newStream->bufferFrames * newStream->format.frameSize;
    newStream->audioBuffer = malloc(newStream->bufferSize);
    if (!newStream->audioBuffer) {
        pthread_mutex_unlock(&gAudioManager.managerMutex);
        return memFullErr;
    }

    /* Initialize synchronization */
    pthread_mutex_init(&newStream->streamMutex, NULL);
    pthread_cond_init(&newStream->bufferCondition, NULL);

    /* Initialize statistics */
    newStream->stats.sessionStartTime = time(NULL);

    gAudioManager.streamCount++;
    *stream = newStream;

    pthread_mutex_unlock(&gAudioManager.managerMutex);
    return noErr;
}

/*
 * DisposeAudioOutputStream
 * Disposes of an audio output stream
 */
OSErr DisposeAudioOutputStream(AudioOutputStream *stream) {
    if (!stream) {
        return paramErr;
    }

    AudioOutputStream *foundStream = FindOutputStream(stream);
    if (!foundStream) {
        return paramErr;
    }

    pthread_mutex_lock(&gAudioManager.managerMutex);

    /* Close stream if open */
    if (foundStream->isOpen) {
        CloseAudioOutputStream(foundStream);
    }

    /* Free resources */
    if (foundStream->audioBuffer) {
        free(foundStream->audioBuffer);
    }

    pthread_mutex_destroy(&foundStream->streamMutex);
    pthread_cond_destroy(&foundStream->bufferCondition);

    /* Remove from array */
    for (short i = 0; i < gAudioManager.streamCount; i++) {
        if (&gAudioManager.streams[i] == foundStream) {
            memmove(&gAudioManager.streams[i], &gAudioManager.streams[i + 1],
                    (gAudioManager.streamCount - i - 1) * sizeof(AudioOutputStream));
            gAudioManager.streamCount--;
            break;
        }
    }

    pthread_mutex_unlock(&gAudioManager.managerMutex);
    return noErr;
}

/*
 * OpenAudioOutputStream
 * Opens an audio output stream
 */
OSErr OpenAudioOutputStream(AudioOutputStream *stream) {
    if (!stream) {
        return paramErr;
    }

    AudioOutputStream *foundStream = FindOutputStream(stream);
    if (!foundStream) {
        return paramErr;
    }

    pthread_mutex_lock(&foundStream->streamMutex);

    if (foundStream->isOpen) {
        pthread_mutex_unlock(&foundStream->streamMutex);
        return noErr; /* Already open */
    }

    /* Verify device exists */
    AudioOutputDevice *device = FindOutputDevice(foundStream->deviceID);
    if (!device || !device->isAvailable) {
        pthread_mutex_unlock(&foundStream->streamMutex);
        return resNotFound;
    }

    /* In a real implementation, this would open the platform audio device */
    foundStream->isOpen = true;
    foundStream->currentFrame = 0;
    foundStream->totalFrames = 0;

    pthread_mutex_unlock(&foundStream->streamMutex);
    return noErr;
}

/*
 * CloseAudioOutputStream
 * Closes an audio output stream
 */
OSErr CloseAudioOutputStream(AudioOutputStream *stream) {
    if (!stream) {
        return paramErr;
    }

    AudioOutputStream *foundStream = FindOutputStream(stream);
    if (!foundStream) {
        return paramErr;
    }

    pthread_mutex_lock(&foundStream->streamMutex);

    if (!foundStream->isOpen) {
        pthread_mutex_unlock(&foundStream->streamMutex);
        return noErr; /* Already closed */
    }

    /* Stop if active */
    if (foundStream->isActive) {
        foundStream->isActive = false;
    }

    /* In a real implementation, this would close the platform audio device */
    foundStream->isOpen = false;

    pthread_mutex_unlock(&foundStream->streamMutex);
    return noErr;
}

/*
 * WriteAudioData
 * Writes audio data to an output stream
 */
OSErr WriteAudioData(AudioOutputStream *stream, const void *audioData, long dataSize,
                     long *framesWritten) {
    if (!stream || !audioData || dataSize <= 0) {
        return paramErr;
    }

    AudioOutputStream *foundStream = FindOutputStream(stream);
    if (!foundStream || !foundStream->isOpen) {
        return synthNotReady;
    }

    pthread_mutex_lock(&foundStream->streamMutex);

    /* Calculate frames */
    long frameCount = dataSize / foundStream->format.frameSize;

    /* Copy data to internal buffer for processing */
    void *processBuffer = malloc(dataSize);
    if (!processBuffer) {
        pthread_mutex_unlock(&foundStream->streamMutex);
        return memFullErr;
    }

    memcpy(processBuffer, audioData, dataSize);

    /* Process audio */
    OSErr err = ProcessAudioBuffer(foundStream, processBuffer, frameCount);

    if (err == noErr) {
        /* In a real implementation, this would write to the audio device */
        /* For now, we just simulate the write */

        if (framesWritten) {
            *framesWritten = frameCount;
        }

        /* Notify callback */
        if (foundStream->outputCallback) {
            foundStream->outputCallback(foundStream, processBuffer, dataSize,
                                        foundStream->outputUserData);
        }

        /* Update level monitoring */
        if (gAudioManager.levelMonitoringEnabled) {
            /* Calculate simple RMS level */
            if (foundStream->format.bitsPerSample == 16) {
                short *samples = (short *)processBuffer;
                long sampleCount = frameCount * foundStream->format.channels;
                long sum = 0;

                for (long i = 0; i < sampleCount; i++) {
                    sum += abs(samples[i]);
                }

                Fixed avgLevel = (Fixed)((sum / sampleCount) * 2); /* Convert to Fixed */
                NotifyLevelCallback(avgLevel, avgLevel);
            }
        }
    }

    free(processBuffer);
    pthread_mutex_unlock(&foundStream->streamMutex);

    return err;
}

/*
 * SetAudioOutputVolume
 * Sets the master output volume
 */
OSErr SetAudioOutputVolume(Fixed volume) {
    if (!gAudioManager.initialized) {
        OSErr err = InitializeAudioOutput();
        if (err != noErr) {
            return err;
        }
    }

    pthread_mutex_lock(&gAudioManager.managerMutex);
    gAudioManager.masterVolume = volume;
    pthread_mutex_unlock(&gAudioManager.managerMutex);

    return noErr;
}

/*
 * GetAudioOutputVolume
 * Gets the master output volume
 */
OSErr GetAudioOutputVolume(Fixed *volume) {
    if (!volume) {
        return paramErr;
    }

    if (!gAudioManager.initialized) {
        OSErr err = InitializeAudioOutput();
        if (err != noErr) {
            return err;
        }
    }

    pthread_mutex_lock(&gAudioManager.managerMutex);
    *volume = gAudioManager.masterVolume;
    pthread_mutex_unlock(&gAudioManager.managerMutex);

    return noErr;
}

/*
 * EnableAudioLevelMonitoring
 * Enables or disables audio level monitoring
 */
OSErr EnableAudioLevelMonitoring(bool enable) {
    if (!gAudioManager.initialized) {
        OSErr err = InitializeAudioOutput();
        if (err != noErr) {
            return err;
        }
    }

    pthread_mutex_lock(&gAudioManager.managerMutex);
    gAudioManager.levelMonitoringEnabled = enable;
    pthread_mutex_unlock(&gAudioManager.managerMutex);

    return noErr;
}

/*
 * SetAudioLevelCallback
 * Sets the audio level monitoring callback
 */
OSErr SetAudioLevelCallback(AudioLevelProc callback, void *userData) {
    if (!gAudioManager.initialized) {
        OSErr err = InitializeAudioOutput();
        if (err != noErr) {
            return err;
        }
    }

    pthread_mutex_lock(&gAudioManager.managerMutex);
    gAudioManager.levelCallback = callback;
    gAudioManager.levelUserData = userData;
    pthread_mutex_unlock(&gAudioManager.managerMutex);

    return noErr;
}

/*
 * ApplyVolumeControl
 * Applies volume control to audio data
 */
OSErr ApplyVolumeControl(void *audioData, long dataSize, const AudioOutputFormat *format,
                         Fixed volume) {
    if (!audioData || dataSize <= 0 || !format) {
        return paramErr;
    }

    long frameCount = dataSize / format->frameSize;
    ApplyVolumeToBuffer(audioData, frameCount, format, volume);

    return noErr;
}

/*
 * GenerateTestTone
 * Generates a test tone for audio testing
 */
OSErr GenerateTestTone(double frequency, long durationMs, const AudioOutputFormat *format,
                       void **audioData, long *dataSize) {
    if (frequency <= 0 || durationMs <= 0 || !format || !audioData || !dataSize) {
        return paramErr;
    }

    long frameCount = (format->sampleRate * durationMs) / 1000;
    *dataSize = frameCount * format->frameSize;
    *audioData = malloc(*dataSize);

    if (!*audioData) {
        return memFullErr;
    }

    if (format->bitsPerSample == 16) {
        short *samples = (short *)*audioData;
        double phaseStep = 2.0 * M_PI * frequency / format->sampleRate;

        for (long i = 0; i < frameCount; i++) {
            short sampleValue = (short)(sin(i * phaseStep) * 8192); /* 25% amplitude */
            for (short c = 0; c < format->channels; c++) {
                samples[i * format->channels + c] = sampleValue;
            }
        }
    }

    return noErr;
}

/* Module cleanup */
static void __attribute__((destructor)) SpeechOutputCleanup(void) {
    CleanupAudioOutput();
}