/*
 * File: SpeechOutput_Stub.c
 *
 * Contains: Speech output stub implementation that connects to SoundManager
 *
 * Written by: Claude Code (Portable Implementation)
 *
 *
 * Description: This file provides a minimal speech audio output implementation
 *              that forwards audio data directly to SoundManager for playback.
 */

#include <string.h>

#include "SystemTypes.h"
#include "SpeechManager/SpeechOutput.h"

/* Forward declaration of SoundManager function */
extern OSErr SoundManager_PlayPCM(const uint8_t* data,
                                  uint32_t sizeBytes,
                                  uint32_t sampleRate,
                                  uint8_t channels,
                                  uint8_t bitsPerSample);

/* ===== Audio Output Implementation ===== */

/* Global audio output state */
static struct {
    Boolean initialized;
    AudioOutputStream currentStream;
    Fixed masterVolume;
    uint32_t currentSampleRate;
    uint8_t currentChannels;
    uint8_t currentBitsPerSample;
} gAudioOutput = {
    .initialized = false,
    .currentStream = 0,
    .masterVolume = 0x00010000,  /* 1.0 in fixed point */
    .currentSampleRate = 22050,
    .currentChannels = 2,
    .currentBitsPerSample = 16
};

/*
 * InitializeAudioOutput
 * Initializes the audio output system
 */
OSErr InitializeAudioOutput(void) {
    if (gAudioOutput.initialized) {
        return noErr;
    }

    gAudioOutput.initialized = true;
    return noErr;
}

/*
 * CleanupAudioOutput
 * Cleans up audio output resources
 */
void CleanupAudioOutput(void) {
    if (!gAudioOutput.initialized) {
        return;
    }

    gAudioOutput.initialized = false;
    gAudioOutput.currentStream = 0;
}

/*
 * CountAudioOutputDevices
 * Returns the number of available audio output devices (stub)
 */
OSErr CountAudioOutputDevices(short *deviceCount) {
    if (!deviceCount) {
        return paramErr;
    }

    /* Report one audio device (SoundManager backend) */
    *deviceCount = 1;
    return noErr;
}

/*
 * GetIndAudioOutputDevice
 * Gets the specified audio output device (stub)
 */
OSErr GetIndAudioOutputDevice(short index, AudioOutputDevice *device) {
    if (!device || index < 1 || index > 1) {
        return paramErr;
    }

    *device = 1;  /* Default device */
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

    *device = 1;  /* Default device */
    return noErr;
}

/*
 * SetDefaultAudioOutputDevice
 * Sets the default audio output device (stub)
 */
OSErr SetDefaultAudioOutputDevice(const char *deviceID) {
    if (!deviceID) {
        return paramErr;
    }

    return noErr;
}

/*
 * GetAudioOutputDeviceInfo
 * Gets information about audio output device (stub)
 */
OSErr GetAudioOutputDeviceInfo(const char *deviceID, AudioOutputDevice *device) {
    if (!deviceID || !device) {
        return paramErr;
    }

    *device = 1;
    return noErr;
}

/*
 * CreateAudioOutputStream
 * Creates an audio output stream
 */
OSErr CreateAudioOutputStream(const AudioOutputConfig *config, AudioOutputStream **stream) {
    if (!stream) {
        return paramErr;
    }

    /* Allocate opaque stream handle */
    gAudioOutput.currentStream = 1;
    *stream = &gAudioOutput.currentStream;

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

    gAudioOutput.currentStream = 0;
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

    return noErr;
}

/*
 * StartAudioOutputStream
 * Starts audio playback on the stream
 */
OSErr StartAudioOutputStream(AudioOutputStream *stream) {
    if (!stream) {
        return paramErr;
    }

    return noErr;
}

/*
 * StopAudioOutputStream
 * Stops audio playback on the stream
 */
OSErr StopAudioOutputStream(AudioOutputStream *stream) {
    if (!stream) {
        return paramErr;
    }

    return noErr;
}

/*
 * PauseAudioOutputStream
 * Pauses audio playback on the stream
 */
OSErr PauseAudioOutputStream(AudioOutputStream *stream) {
    if (!stream) {
        return paramErr;
    }

    return noErr;
}

/*
 * ResumeAudioOutputStream
 * Resumes audio playback on the stream
 */
OSErr ResumeAudioOutputStream(AudioOutputStream *stream) {
    if (!stream) {
        return paramErr;
    }

    return noErr;
}

/*
 * FlushAudioOutputStream
 * Flushes pending audio data in the stream
 */
OSErr FlushAudioOutputStream(AudioOutputStream *stream) {
    if (!stream) {
        return paramErr;
    }

    return noErr;
}

/*
 * WriteAudioData
 * Writes audio data to the stream (KEY INTEGRATION POINT)
 * Forwards audio to SoundManager_PlayPCM for actual playback
 */
OSErr WriteAudioData(AudioOutputStream *stream, const void *audioData, long dataSize,
                     long *framesWritten) {
    if (!stream || !audioData || dataSize <= 0) {
        return paramErr;
    }

    /* Forward to SoundManager for playback */
    OSErr err = SoundManager_PlayPCM((const uint8_t*)audioData,
                                     (uint32_t)dataSize,
                                     gAudioOutput.currentSampleRate,
                                     gAudioOutput.currentChannels,
                                     gAudioOutput.currentBitsPerSample);

    /* Calculate frames written based on format */
    if (framesWritten) {
        long bytesPerFrame = (gAudioOutput.currentBitsPerSample / 8) * gAudioOutput.currentChannels;
        *framesWritten = (bytesPerFrame > 0) ? (dataSize / bytesPerFrame) : 0;
    }

    return err;
}

/*
 * WriteAudioFrames
 * Writes audio frames to the stream
 */
OSErr WriteAudioFrames(AudioOutputStream *stream, const void *audioFrames, long frameCount) {
    if (!stream || !audioFrames || frameCount <= 0) {
        return paramErr;
    }

    /* Calculate data size from frame count */
    long bytesPerFrame = (gAudioOutput.currentBitsPerSample / 8) * gAudioOutput.currentChannels;
    long dataSize = frameCount * bytesPerFrame;

    return WriteAudioData(stream, audioFrames, dataSize, NULL);
}

/*
 * GetAudioStreamPosition
 * Gets the current position in the audio stream
 */
OSErr GetAudioStreamPosition(AudioOutputStream *stream, long *currentFrame, long *totalFrames) {
    if (!stream) {
        return paramErr;
    }

    if (currentFrame) *currentFrame = 0;
    if (totalFrames) *totalFrames = 0;

    return noErr;
}

/*
 * SetAudioOutputVolume
 * Sets the master output volume
 */
OSErr SetAudioOutputVolume(Fixed volume) {
    if (volume < 0 || volume > 0x00010000) {
        return paramErr;
    }

    gAudioOutput.masterVolume = volume;
    return noErr;
}

/*
 * GetAudioOutputVolume
 * Gets the current output volume
 */
OSErr GetAudioOutputVolume(Fixed *volume) {
    if (!volume) {
        return paramErr;
    }

    *volume = gAudioOutput.masterVolume;
    return noErr;
}

/*
 * Stub implementations for less critical functions
 */

OSErr GetAudioOutputDeviceCapabilities(const char *deviceID, AudioOutputFormat **formats,
                                       short *formatCount, AudioOutputFlags *supportedFlags) {
    (void)deviceID;
    if (formats) *formats = NULL;
    if (formatCount) *formatCount = 0;
    if (supportedFlags) *supportedFlags = 0;
    return noErr;
}

Boolean IsAudioOutputDeviceAvailable(const char *deviceID) {
    return true;
}

OSErr GetAudioOutputDeviceStatus(const char *deviceID, Boolean *isActive, long *currentSampleRate,
                                 short *currentChannels) {
    if (isActive) *isActive = true;
    if (currentSampleRate) *currentSampleRate = 22050;
    if (currentChannels) *currentChannels = 2;
    return noErr;
}

OSErr CreateAudioOutputConfig(AudioOutputConfig **config) {
    if (!config) return paramErr;
    *config = 0;  /* Opaque */
    return noErr;
}

OSErr DisposeAudioOutputConfig(AudioOutputConfig *config) {
    return noErr;
}

OSErr SetAudioOutputConfig(const AudioOutputConfig *config) {
    return noErr;
}

OSErr GetAudioOutputConfig(AudioOutputConfig *config) {
    return noErr;
}

OSErr ValidateAudioOutputConfig(const AudioOutputConfig *config, Boolean *isValid,
                                char **errorMessage) {
    if (isValid) *isValid = true;
    return noErr;
}

OSErr SetAudioOutputFormat(const AudioOutputFormat *format) {
    return noErr;
}

OSErr GetAudioOutputFormat(AudioOutputFormat *format) {
    return noErr;
}

OSErr GetBestAudioOutputFormat(const char *deviceID, AudioOutputQuality quality,
                               AudioOutputFormat *format) {
    return noErr;
}

OSErr SetAudioOutputBalance(Fixed balance) {
    return noErr;
}

OSErr GetAudioOutputBalance(Fixed *balance) {
    if (balance) *balance = 0;
    return noErr;
}

OSErr SetChannelVolume(short channel, Fixed volume) {
    return noErr;
}

OSErr GetChannelVolume(short channel, Fixed *volume) {
    if (volume) *volume = gAudioOutput.masterVolume;
    return noErr;
}

OSErr SetAudioStreamProperty(AudioOutputStream *stream, OSType property, const void *value,
                             long valueSize) {
    return noErr;
}

OSErr GetAudioStreamProperty(AudioOutputStream *stream, OSType property, void *value,
                             long *valueSize) {
    (void)stream; (void)property; (void)value;
    if (valueSize) *valueSize = 0;
    return noErr;
}

OSErr CreateAudioProcessor(AudioOutputFlags processingFlags, AudioProcessor **processor) {
    if (!processor) return paramErr;
    *processor = 0;
    return noErr;
}

OSErr DisposeAudioProcessor(AudioProcessor *processor) {
    return noErr;
}

OSErr ProcessAudioData(AudioProcessor *processor, void *audioData, long dataSize) {
    return noErr;
}

OSErr ApplyVolumeControl(void *audioData, long dataSize, const AudioOutputFormat *format,
                         Fixed volume) {
    return noErr;
}

OSErr ApplyNormalization(void *audioData, long dataSize, const AudioOutputFormat *format) {
    return noErr;
}

OSErr ApplyCompression(void *audioData, long dataSize, const AudioOutputFormat *format,
                       Fixed threshold, Fixed ratio) {
    return noErr;
}

OSErr ApplyEqualization(void *audioData, long dataSize, const AudioOutputFormat *format,
                        Fixed *bandGains, short bandCount) {
    return noErr;
}

OSErr RegisterAudioEffect(OSType effectType, AudioEffectProc effectProc, void *userData) {
    return noErr;
}

OSErr ApplyCustomEffect(OSType effectType, void *audioData, long dataSize,
                        const AudioOutputFormat *format, void *effectData) {
    return noErr;
}

OSErr SetAudioRoutingMode(AudioRoutingMode mode) {
    return noErr;
}

OSErr GetAudioRoutingMode(AudioRoutingMode *mode) {
    if (mode) *mode = kRoutingMode_Automatic;
    return noErr;
}

OSErr RouteAudioToDevice(const char *deviceID) {
    return noErr;
}

OSErr GetCurrentAudioRoute(char *deviceID, long deviceIDSize) {
    if (deviceID && deviceIDSize > 0) {
        strncpy(deviceID, "default", deviceIDSize - 1);
        deviceID[deviceIDSize - 1] = '\0';
    }
    return noErr;
}

OSErr EnableMultiDeviceOutput(Boolean enable) {
    return noErr;
}

OSErr AddOutputDevice(const char *deviceID, Fixed volume) {
    return noErr;
}

OSErr RemoveOutputDevice(const char *deviceID) {
    return noErr;
}

OSErr GetActiveOutputDevices(char ***deviceIDs, short *deviceCount) {
    if (deviceCount) *deviceCount = 0;
    return noErr;
}

OSErr SetAudioInterruptionPolicy(Boolean allowInterruptions) {
    return noErr;
}

OSErr SetAudioDuckingEnabled(Boolean enable) {
    return noErr;
}

OSErr SetAudioPriorityLevel(short priority) {
    return noErr;
}

OSErr EnableAudioLevelMonitoring(Boolean enable) {
    return noErr;
}

OSErr GetAudioLevels(Fixed *leftLevel, Fixed *rightLevel) {
    if (leftLevel) *leftLevel = 0;
    if (rightLevel) *rightLevel = 0;
    return noErr;
}

OSErr GetPeakLevels(Fixed *leftPeak, Fixed *rightPeak) {
    if (leftPeak) *leftPeak = 0;
    if (rightPeak) *rightPeak = 0;
    return noErr;
}

OSErr ResetPeakLevels(void) {
    return noErr;
}

OSErr EnableSpectrumAnalysis(Boolean enable) {
    return noErr;
}

OSErr GetAudioSpectrum(Fixed *spectrum, short bandCount) {
    return noErr;
}

OSErr SetSpectrumAnalysisParameters(short fftSize, short overlap) {
    return noErr;
}

OSErr GetAudioOutputStats(AudioOutputStats *stats) {
    return noErr;
}

OSErr ResetAudioOutputStats(void) {
    return noErr;
}
