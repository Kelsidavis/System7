/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * SoundMgr_HAL.c - Hardware Abstraction Layer for Sound Manager
 * Provides platform-specific audio output and synthesis
 */

#include "SoundManager/SoundManager.h"
#include "SoundManager/SoundTypes.h"
#include "SoundManager/SoundSynthesis.h"
#include "SoundManager/SoundHardware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#else
#ifdef HAS_ALSA
#include <alsa/asoundlib.h>
#elif defined(HAS_PULSEAUDIO)
#include <pulse/simple.h>
#include <pulse/error.h>
#endif
#endif

#define SAMPLE_RATE 44100
#define CHANNELS 2
#define FRAMES_PER_BUFFER 512
#define MAX_CHANNELS 32
#define MIXING_BUFFER_SIZE (FRAMES_PER_BUFFER * CHANNELS)

/* Platform-specific audio context */
typedef struct {
    Boolean initialized;
    Boolean playing;
    pthread_t audioThread;
    pthread_mutex_t audioMutex;

    /* Mixing buffer */
    float mixBuffer[MIXING_BUFFER_SIZE];
    int16_t outputBuffer[MIXING_BUFFER_SIZE];

    /* Channel states */
    struct {
        Boolean active;
        SndChannelPtr channel;
        float volume;
        float pan;
        uint32_t position;
        uint32_t length;
        int16_t *sampleData;
        float phase;
        float frequency;
        WaveformType waveform;
    } channels[MAX_CHANNELS];

#ifdef __APPLE__
    AudioUnit outputUnit;
    AudioStreamBasicDescription audioFormat;
#else
#ifdef HAS_ALSA
    snd_pcm_t *pcmHandle;
    snd_pcm_hw_params_t *hwParams;
#elif defined(HAS_PULSEAUDIO)
    pa_simple *paStream;
    pa_sample_spec paSpec;
#endif
#endif
} HAL_AudioContext;

/* Global audio context */
static HAL_AudioContext gAudioContext = {0};

/* Forward declarations */
static void* SoundMgr_HAL_AudioThread(void *data);
static void SoundMgr_HAL_MixChannels(void);
static void SoundMgr_HAL_GenerateSamples(int channel, float *buffer, int frames);
static float SoundMgr_HAL_GenerateWaveform(WaveformType type, float phase);

#ifdef __APPLE__
static OSStatus SoundMgr_HAL_RenderCallback(void *inRefCon,
                                           AudioUnitRenderActionFlags *ioActionFlags,
                                           const AudioTimeStamp *inTimeStamp,
                                           UInt32 inBusNumber,
                                           UInt32 inNumberFrames,
                                           AudioBufferList *ioData);
#endif

/* Initialize Sound Manager HAL */
OSErr SoundMgr_HAL_Init(void) {
    if (gAudioContext.initialized) {
        return noErr;
    }

    pthread_mutex_init(&gAudioContext.audioMutex, NULL);

#ifdef __APPLE__
    /* Initialize Core Audio */
    AudioComponentDescription desc = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0
    };

    AudioComponent component = AudioComponentFindNext(NULL, &desc);
    if (!component) {
        return sndErr;
    }

    OSStatus status = AudioComponentInstanceNew(component, &gAudioContext.outputUnit);
    if (status != noErr) {
        return sndErr;
    }

    /* Set audio format */
    gAudioContext.audioFormat.mSampleRate = SAMPLE_RATE;
    gAudioContext.audioFormat.mFormatID = kAudioFormatLinearPCM;
    gAudioContext.audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger |
                                             kAudioFormatFlagIsPacked;
    gAudioContext.audioFormat.mBitsPerChannel = 16;
    gAudioContext.audioFormat.mChannelsPerFrame = CHANNELS;
    gAudioContext.audioFormat.mFramesPerPacket = 1;
    gAudioContext.audioFormat.mBytesPerFrame = CHANNELS * sizeof(int16_t);
    gAudioContext.audioFormat.mBytesPerPacket = gAudioContext.audioFormat.mBytesPerFrame;

    AudioUnitSetProperty(gAudioContext.outputUnit,
                        kAudioUnitProperty_StreamFormat,
                        kAudioUnitScope_Input,
                        0,
                        &gAudioContext.audioFormat,
                        sizeof(gAudioContext.audioFormat));

    /* Set render callback */
    AURenderCallbackStruct callback = {
        .inputProc = SoundMgr_HAL_RenderCallback,
        .inputProcRefCon = NULL
    };

    AudioUnitSetProperty(gAudioContext.outputUnit,
                        kAudioUnitProperty_SetRenderCallback,
                        kAudioUnitScope_Input,
                        0,
                        &callback,
                        sizeof(callback));

    /* Initialize audio unit */
    status = AudioUnitInitialize(gAudioContext.outputUnit);
    if (status != noErr) {
        return sndErr;
    }

#else
#ifdef HAS_ALSA
    /* Initialize ALSA */
    int err;

    err = snd_pcm_open(&gAudioContext.pcmHandle, "default",
                      SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot open audio device: %s\n", snd_strerror(err));
        return sndErr;
    }

    snd_pcm_hw_params_alloca(&gAudioContext.hwParams);
    snd_pcm_hw_params_any(gAudioContext.pcmHandle, gAudioContext.hwParams);

    snd_pcm_hw_params_set_access(gAudioContext.pcmHandle, gAudioContext.hwParams,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(gAudioContext.pcmHandle, gAudioContext.hwParams,
                                 SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(gAudioContext.pcmHandle, gAudioContext.hwParams,
                                   CHANNELS);

    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(gAudioContext.pcmHandle, gAudioContext.hwParams,
                                    &rate, 0);

    snd_pcm_hw_params_set_period_size(gAudioContext.pcmHandle, gAudioContext.hwParams,
                                      FRAMES_PER_BUFFER, 0);

    err = snd_pcm_hw_params(gAudioContext.pcmHandle, gAudioContext.hwParams);
    if (err < 0) {
        fprintf(stderr, "Cannot set hardware parameters: %s\n", snd_strerror(err));
        snd_pcm_close(gAudioContext.pcmHandle);
        return sndErr;
    }

    snd_pcm_prepare(gAudioContext.pcmHandle);

#elif defined(HAS_PULSEAUDIO)
    /* Initialize PulseAudio */
    gAudioContext.paSpec.format = PA_SAMPLE_S16LE;
    gAudioContext.paSpec.rate = SAMPLE_RATE;
    gAudioContext.paSpec.channels = CHANNELS;

    int error;
    gAudioContext.paStream = pa_simple_new(NULL,           /* server */
                                          "System7",       /* app name */
                                          PA_STREAM_PLAYBACK,
                                          NULL,           /* device */
                                          "Sound Manager", /* stream name */
                                          &gAudioContext.paSpec,
                                          NULL,           /* channel map */
                                          NULL,           /* buffer attributes */
                                          &error);

    if (!gAudioContext.paStream) {
        fprintf(stderr, "PulseAudio error: %s\n", pa_strerror(error));
        return sndErr;
    }
#endif
#endif

    gAudioContext.initialized = true;
    return noErr;
}

/* Cleanup Sound Manager HAL */
void SoundMgr_HAL_Cleanup(void) {
    if (!gAudioContext.initialized) {
        return;
    }

    SoundMgr_HAL_Stop();

#ifdef __APPLE__
    if (gAudioContext.outputUnit) {
        AudioOutputUnitStop(gAudioContext.outputUnit);
        AudioUnitUninitialize(gAudioContext.outputUnit);
        AudioComponentInstanceDispose(gAudioContext.outputUnit);
    }

#else
#ifdef HAS_ALSA
    if (gAudioContext.pcmHandle) {
        snd_pcm_drain(gAudioContext.pcmHandle);
        snd_pcm_close(gAudioContext.pcmHandle);
    }

#elif defined(HAS_PULSEAUDIO)
    if (gAudioContext.paStream) {
        pa_simple_drain(gAudioContext.paStream, NULL);
        pa_simple_free(gAudioContext.paStream);
    }
#endif
#endif

    pthread_mutex_destroy(&gAudioContext.audioMutex);
    gAudioContext.initialized = false;
}

/* Start audio playback */
OSErr SoundMgr_HAL_Start(void) {
    if (!gAudioContext.initialized || gAudioContext.playing) {
        return noErr;
    }

    gAudioContext.playing = true;

#ifdef __APPLE__
    OSStatus status = AudioOutputUnitStart(gAudioContext.outputUnit);
    if (status != noErr) {
        gAudioContext.playing = false;
        return sndErr;
    }

#else
    /* Start audio thread for other platforms */
    pthread_create(&gAudioContext.audioThread, NULL,
                  SoundMgr_HAL_AudioThread, NULL);
#endif

    return noErr;
}

/* Stop audio playback */
OSErr SoundMgr_HAL_Stop(void) {
    if (!gAudioContext.playing) {
        return noErr;
    }

    gAudioContext.playing = false;

#ifdef __APPLE__
    AudioOutputUnitStop(gAudioContext.outputUnit);

#else
    pthread_join(gAudioContext.audioThread, NULL);
#endif

    return noErr;
}

/* Allocate sound channel */
OSErr SoundMgr_HAL_AllocateChannel(SndChannelPtr *chan, short synth) {
    pthread_mutex_lock(&gAudioContext.audioMutex);

    /* Find free channel */
    int channelIndex = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!gAudioContext.channels[i].active) {
            channelIndex = i;
            break;
        }
    }

    if (channelIndex < 0) {
        pthread_mutex_unlock(&gAudioContext.audioMutex);
        return noChannelErr;
    }

    /* Allocate channel structure */
    SndChannelPtr newChan = (SndChannelPtr)calloc(1, sizeof(SndChannel));
    if (!newChan) {
        pthread_mutex_unlock(&gAudioContext.audioMutex);
        return memFullErr;
    }

    /* Initialize channel */
    newChan->qLength = stdQLength;
    newChan->qHead = 0;
    newChan->qTail = 0;
    newChan->busy = 0;
    newChan->wait = 0;
    newChan->pauseCount = 0;
    newChan->flags = 0;
    newChan->userInfo = 0;
    newChan->callBack = NULL;

    /* Store in HAL */
    gAudioContext.channels[channelIndex].active = true;
    gAudioContext.channels[channelIndex].channel = newChan;
    gAudioContext.channels[channelIndex].volume = 1.0f;
    gAudioContext.channels[channelIndex].pan = 0.5f;
    gAudioContext.channels[channelIndex].position = 0;
    gAudioContext.channels[channelIndex].phase = 0.0f;
    gAudioContext.channels[channelIndex].waveform = squareWave;

    *chan = newChan;

    pthread_mutex_unlock(&gAudioContext.audioMutex);
    return noErr;
}

/* Dispose sound channel */
OSErr SoundMgr_HAL_DisposeChannel(SndChannelPtr chan) {
    if (!chan) {
        return badChannel;
    }

    pthread_mutex_lock(&gAudioContext.audioMutex);

    /* Find and deactivate channel */
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (gAudioContext.channels[i].channel == chan) {
            gAudioContext.channels[i].active = false;
            gAudioContext.channels[i].channel = NULL;
            if (gAudioContext.channels[i].sampleData) {
                free(gAudioContext.channels[i].sampleData);
                gAudioContext.channels[i].sampleData = NULL;
            }
            break;
        }
    }

    free(chan);

    pthread_mutex_unlock(&gAudioContext.audioMutex);
    return noErr;
}

/* Play sound resource */
OSErr SoundMgr_HAL_PlayResource(Handle sndHandle, SndChannelPtr chan) {
    if (!sndHandle || !chan) {
        return paramErr;
    }

    pthread_mutex_lock(&gAudioContext.audioMutex);

    /* Find channel in HAL */
    int channelIndex = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (gAudioContext.channels[i].channel == chan) {
            channelIndex = i;
            break;
        }
    }

    if (channelIndex < 0) {
        pthread_mutex_unlock(&gAudioContext.audioMutex);
        return badChannel;
    }

    /* Parse sound resource (simplified) */
    /* In real implementation, would parse 'snd ' resource format */
    uint32_t dataSize = GetHandleSize(sndHandle);
    if (dataSize < 20) {
        pthread_mutex_unlock(&gAudioContext.audioMutex);
        return badFormat;
    }

    /* For now, treat as raw PCM data */
    HLock(sndHandle);

    /* Allocate sample buffer */
    if (gAudioContext.channels[channelIndex].sampleData) {
        free(gAudioContext.channels[channelIndex].sampleData);
    }

    gAudioContext.channels[channelIndex].sampleData = (int16_t*)malloc(dataSize);
    if (!gAudioContext.channels[channelIndex].sampleData) {
        HUnlock(sndHandle);
        pthread_mutex_unlock(&gAudioContext.audioMutex);
        return memFullErr;
    }

    memcpy(gAudioContext.channels[channelIndex].sampleData, *sndHandle, dataSize);
    gAudioContext.channels[channelIndex].length = dataSize / sizeof(int16_t);
    gAudioContext.channels[channelIndex].position = 0;

    HUnlock(sndHandle);

    pthread_mutex_unlock(&gAudioContext.audioMutex);

    /* Start playback if not already running */
    return SoundMgr_HAL_Start();
}

/* Send command to channel */
OSErr SoundMgr_HAL_SendCommand(SndChannelPtr chan, const SndCommand *cmd, Boolean noWait) {
    if (!chan || !cmd) {
        return paramErr;
    }

    pthread_mutex_lock(&gAudioContext.audioMutex);

    /* Find channel in HAL */
    int channelIndex = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (gAudioContext.channels[i].channel == chan) {
            channelIndex = i;
            break;
        }
    }

    if (channelIndex < 0) {
        pthread_mutex_unlock(&gAudioContext.audioMutex);
        return badChannel;
    }

    /* Process command */
    switch (cmd->cmd) {
        case nullCmd:
            /* No operation */
            break;

        case quietCmd:
            /* Stop sound immediately */
            gAudioContext.channels[channelIndex].position =
                gAudioContext.channels[channelIndex].length;
            break;

        case flushCmd:
            /* Flush command queue */
            chan->qHead = chan->qTail = 0;
            break;

        case freqCmd:
            /* Set frequency */
            gAudioContext.channels[channelIndex].frequency =
                (float)cmd->param2 / 1000.0f;
            break;

        case ampCmd:
            /* Set amplitude */
            gAudioContext.channels[channelIndex].volume =
                (float)cmd->param1 / 255.0f;
            break;

        case waveTableCmd:
            /* Set wave table */
            if (cmd->param1 >= 0 && cmd->param1 <= 3) {
                gAudioContext.channels[channelIndex].waveform = cmd->param1;
            }
            break;

        case volumeCmd:
            /* Set volume */
            {
                int16_t vol = (int16_t)cmd->param2;
                gAudioContext.channels[channelIndex].volume =
                    (float)(vol + 256) / 512.0f;
            }
            break;

        case soundCmd:
            /* Play sound */
            if (cmd->param2) {
                Handle sndHandle = (Handle)cmd->param2;
                /* Would parse and play sound resource */
            }
            break;

        default:
            /* Unknown command */
            break;
    }

    pthread_mutex_unlock(&gAudioContext.audioMutex);
    return noErr;
}

/* Audio thread for non-Mac platforms */
static void* SoundMgr_HAL_AudioThread(void *data) {
    while (gAudioContext.playing) {
        /* Generate audio */
        SoundMgr_HAL_MixChannels();

        /* Write to audio device */
#ifdef HAS_ALSA
        int frames = snd_pcm_writei(gAudioContext.pcmHandle,
                                    gAudioContext.outputBuffer,
                                    FRAMES_PER_BUFFER);
        if (frames < 0) {
            snd_pcm_prepare(gAudioContext.pcmHandle);
        }

#elif defined(HAS_PULSEAUDIO)
        int error;
        if (pa_simple_write(gAudioContext.paStream,
                           gAudioContext.outputBuffer,
                           FRAMES_PER_BUFFER * CHANNELS * sizeof(int16_t),
                           &error) < 0) {
            fprintf(stderr, "PulseAudio write error: %s\n", pa_strerror(error));
        }
#endif
    }

    return NULL;
}

#ifdef __APPLE__
/* Core Audio render callback */
static OSStatus SoundMgr_HAL_RenderCallback(void *inRefCon,
                                           AudioUnitRenderActionFlags *ioActionFlags,
                                           const AudioTimeStamp *inTimeStamp,
                                           UInt32 inBusNumber,
                                           UInt32 inNumberFrames,
                                           AudioBufferList *ioData) {
    int16_t *buffer = (int16_t*)ioData->mBuffers[0].mData;

    /* Generate audio */
    pthread_mutex_lock(&gAudioContext.audioMutex);

    /* Clear mix buffer */
    memset(gAudioContext.mixBuffer, 0, inNumberFrames * CHANNELS * sizeof(float));

    /* Mix all active channels */
    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        if (gAudioContext.channels[ch].active) {
            SoundMgr_HAL_GenerateSamples(ch, gAudioContext.mixBuffer, inNumberFrames);
        }
    }

    /* Convert float to int16 */
    for (int i = 0; i < inNumberFrames * CHANNELS; i++) {
        float sample = gAudioContext.mixBuffer[i];

        /* Clip */
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;

        buffer[i] = (int16_t)(sample * 32767.0f);
    }

    pthread_mutex_unlock(&gAudioContext.audioMutex);

    return noErr;
}
#endif

/* Mix all active channels */
static void SoundMgr_HAL_MixChannels(void) {
    pthread_mutex_lock(&gAudioContext.audioMutex);

    /* Clear mix buffer */
    memset(gAudioContext.mixBuffer, 0, sizeof(gAudioContext.mixBuffer));

    /* Mix all active channels */
    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        if (gAudioContext.channels[ch].active) {
            SoundMgr_HAL_GenerateSamples(ch, gAudioContext.mixBuffer, FRAMES_PER_BUFFER);
        }
    }

    /* Convert float to int16 */
    for (int i = 0; i < MIXING_BUFFER_SIZE; i++) {
        float sample = gAudioContext.mixBuffer[i];

        /* Clip */
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;

        gAudioContext.outputBuffer[i] = (int16_t)(sample * 32767.0f);
    }

    pthread_mutex_unlock(&gAudioContext.audioMutex);
}

/* Generate samples for a channel */
static void SoundMgr_HAL_GenerateSamples(int channel, float *buffer, int frames) {
    if (channel < 0 || channel >= MAX_CHANNELS) {
        return;
    }

    float volume = gAudioContext.channels[channel].volume;
    float pan = gAudioContext.channels[channel].pan;
    float leftGain = volume * (1.0f - pan);
    float rightGain = volume * pan;

    /* If we have sample data, play it */
    if (gAudioContext.channels[channel].sampleData &&
        gAudioContext.channels[channel].position < gAudioContext.channels[channel].length) {

        uint32_t pos = gAudioContext.channels[channel].position;
        uint32_t len = gAudioContext.channels[channel].length;
        int16_t *samples = gAudioContext.channels[channel].sampleData;

        for (int i = 0; i < frames && pos < len; i++) {
            float sample = (float)samples[pos] / 32768.0f;

            buffer[i * 2] += sample * leftGain;      /* Left channel */
            buffer[i * 2 + 1] += sample * rightGain; /* Right channel */

            pos++;
        }

        gAudioContext.channels[channel].position = pos;
    }
    /* Otherwise generate waveform */
    else if (gAudioContext.channels[channel].frequency > 0) {
        float freq = gAudioContext.channels[channel].frequency;
        float phase = gAudioContext.channels[channel].phase;
        float phaseIncrement = (2.0f * M_PI * freq) / SAMPLE_RATE;
        WaveformType waveform = gAudioContext.channels[channel].waveform;

        for (int i = 0; i < frames; i++) {
            float sample = SoundMgr_HAL_GenerateWaveform(waveform, phase);

            buffer[i * 2] += sample * leftGain;      /* Left channel */
            buffer[i * 2 + 1] += sample * rightGain; /* Right channel */

            phase += phaseIncrement;
            if (phase >= 2.0f * M_PI) {
                phase -= 2.0f * M_PI;
            }
        }

        gAudioContext.channels[channel].phase = phase;
    }
}

/* Generate waveform sample */
static float SoundMgr_HAL_GenerateWaveform(WaveformType type, float phase) {
    switch (type) {
        case sineWave:
            return sinf(phase);

        case squareWave:
            return (phase < M_PI) ? 1.0f : -1.0f;

        case triangleWave:
            {
                float t = phase / (2.0f * M_PI);
                if (t < 0.5f) {
                    return 4.0f * t - 1.0f;
                } else {
                    return 3.0f - 4.0f * t;
                }
            }

        case sawtoothWave:
            return 1.0f - (2.0f * phase / (2.0f * M_PI));

        default:
            return 0.0f;
    }
}

/* Get current CPU load */
int16_t SoundMgr_HAL_GetCPULoad(void) {
    /* TODO: Implement actual CPU load measurement */
    return 10; /* Return 10% for now */
}

/* Set global volume */
OSErr SoundMgr_HAL_SetVolume(int16_t volume) {
    float vol = (float)(volume + 256) / 512.0f;

    pthread_mutex_lock(&gAudioContext.audioMutex);

    /* Apply to all channels */
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (gAudioContext.channels[i].active) {
            gAudioContext.channels[i].volume = vol;
        }
    }

    pthread_mutex_unlock(&gAudioContext.audioMutex);
    return noErr;
}

/* Simple beep implementation */
OSErr SoundMgr_HAL_SysBeep(short duration) {
    SndChannelPtr chan = NULL;
    OSErr err;

    /* Allocate temporary channel */
    err = SoundMgr_HAL_AllocateChannel(&chan, squareWaveSynth);
    if (err != noErr) {
        return err;
    }

    /* Play beep tone (1000 Hz) */
    SndCommand cmd;

    /* Set frequency */
    cmd.cmd = freqCmd;
    cmd.param1 = 0;
    cmd.param2 = 1000; /* 1000 Hz */
    SoundMgr_HAL_SendCommand(chan, &cmd, true);

    /* Set amplitude */
    cmd.cmd = ampCmd;
    cmd.param1 = 200; /* ~80% volume */
    cmd.param2 = 0;
    SoundMgr_HAL_SendCommand(chan, &cmd, true);

    /* Start playback */
    SoundMgr_HAL_Start();

    /* Wait for duration */
    usleep(duration * 1000);

    /* Stop */
    cmd.cmd = quietCmd;
    cmd.param1 = 0;
    cmd.param2 = 0;
    SoundMgr_HAL_SendCommand(chan, &cmd, true);

    /* Clean up */
    SoundMgr_HAL_DisposeChannel(chan);

    return noErr;
}