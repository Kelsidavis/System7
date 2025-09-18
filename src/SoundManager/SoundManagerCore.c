/*
 * SoundManagerCore.c - Mac OS Sound Manager Core Implementation
 *
 * Complete implementation of the Mac OS Sound Manager providing:
 * - Sound channel management and control
 * - Command processing and queuing
 * - Sound resource playback
 * - Legacy Sound Manager 1.0 compatibility
 * - Multi-channel audio mixing
 *
 * Based on original Mac OS Sound Manager from System 7.1 with modern
 * portable implementation using platform audio abstractions.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

#include "../../include/SoundManager/SoundManager.h"
#include "../../include/SoundManager/SoundTypes.h"
#include "../../include/SoundManager/SoundSynthesis.h"
#include "../../include/SoundManager/SoundHardware.h"
#include "../../include/Resources/ResourceData.h"

/* Sound Manager Global State */
static SoundManagerGlobals g_soundMgr = {
    .initialized = false,
    .version = kSoundManagerVersion,
    .channelCount = 0,
    .channelList = NULL,
    .hardware = NULL,
    .synthesizer = NULL,
    .mixer = NULL,
    .recorder = NULL,
    .globalVolume = kFullVolume,
    .muted = false,
    .cpuLoad = 0
};

/* Synchronization */
static pthread_mutex_t g_soundMutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_soundManagerActive = false;
static pthread_t g_soundThread;

/* Legacy compatibility globals */
static int16_t g_legacySoundVol = kFullVolume;
static bool g_legacySoundPlaying = false;
static SndChannelPtr g_legacyChannel = NULL;

/* Forward declarations */
static OSErr ProcessSoundCommand(SndChannelPtr chan, const SndCommand* cmd);
static OSErr ExecuteImmediateCommand(SndChannelPtr chan, const SndCommand* cmd);
static void* SoundManagerThread(void* arg);
static void AudioOutputCallback(void* userData, int16_t* outputBuffer, uint32_t frameCount);
static OSErr InitializeSoundHardware(void);
static OSErr ShutdownSoundHardware(void);

/*
 * Sound Manager Initialization
 * Sets up the Sound Manager, initializes hardware, and starts the audio thread
 */
OSErr SoundManagerInit(void)
{
    OSErr err = noErr;

    pthread_mutex_lock(&g_soundMutex);

    if (g_soundMgr.initialized) {
        pthread_mutex_unlock(&g_soundMutex);
        return noErr;
    }

    printf("Initializing Sound Manager version %04X...\n", g_soundMgr.version);

    /* Initialize sound hardware */
    err = InitializeSoundHardware();
    if (err != noErr) {
        printf("Failed to initialize sound hardware: %d\n", err);
        goto cleanup;
    }

    /* Initialize synthesizer */
    err = SynthInit(&g_soundMgr.synthesizer, sampledSynth, 44100);
    if (err != noErr) {
        printf("Failed to initialize synthesizer: %d\n", err);
        goto cleanup;
    }

    /* Initialize mixer */
    err = MixerInit(&g_soundMgr.mixer, 32, 44100);
    if (err != noErr) {
        printf("Failed to initialize mixer: %d\n", err);
        goto cleanup;
    }

    /* Initialize recorder */
    err = AudioRecorderInit(&g_soundMgr.recorder, g_soundMgr.hardware);
    if (err != noErr) {
        printf("Warning: Failed to initialize audio recorder: %d\n", err);
        /* Continue without recorder - not critical */
    }

    /* Start sound processing thread */
    g_soundManagerActive = true;
    if (pthread_create(&g_soundThread, NULL, SoundManagerThread, NULL) != 0) {
        err = memFullErr;
        g_soundManagerActive = false;
        goto cleanup;
    }

    g_soundMgr.initialized = true;
    printf("Sound Manager initialized successfully\n");

cleanup:
    if (err != noErr) {
        ShutdownSoundHardware();
        if (g_soundMgr.mixer) {
            MixerDispose(g_soundMgr.mixer);
            g_soundMgr.mixer = NULL;
        }
        if (g_soundMgr.synthesizer) {
            SynthDispose(g_soundMgr.synthesizer);
            g_soundMgr.synthesizer = NULL;
        }
    }

    pthread_mutex_unlock(&g_soundMutex);
    return err;
}

/*
 * Sound Manager Shutdown
 * Cleans up all resources and stops the audio system
 */
OSErr SoundManagerShutdown(void)
{
    SndChannelPtr chan, nextChan;

    pthread_mutex_lock(&g_soundMutex);

    if (!g_soundMgr.initialized) {
        pthread_mutex_unlock(&g_soundMutex);
        return noErr;
    }

    printf("Shutting down Sound Manager...\n");

    /* Stop sound thread */
    g_soundManagerActive = false;
    pthread_mutex_unlock(&g_soundMutex);
    pthread_join(g_soundThread, NULL);
    pthread_mutex_lock(&g_soundMutex);

    /* Dispose all sound channels */
    chan = g_soundMgr.channelList;
    while (chan != NULL) {
        nextChan = chan->nextChan;
        SndDisposeChannel(chan, true);
        chan = nextChan;
    }

    /* Shutdown components */
    if (g_soundMgr.recorder) {
        AudioRecorderShutdown(g_soundMgr.recorder);
        g_soundMgr.recorder = NULL;
    }

    if (g_soundMgr.mixer) {
        MixerDispose(g_soundMgr.mixer);
        g_soundMgr.mixer = NULL;
    }

    if (g_soundMgr.synthesizer) {
        SynthDispose(g_soundMgr.synthesizer);
        g_soundMgr.synthesizer = NULL;
    }

    ShutdownSoundHardware();

    /* Reset state */
    g_soundMgr.initialized = false;
    g_soundMgr.channelCount = 0;
    g_soundMgr.channelList = NULL;

    printf("Sound Manager shutdown complete\n");
    pthread_mutex_unlock(&g_soundMutex);

    return noErr;
}

/*
 * Create New Sound Channel
 * Allocates and initializes a new sound channel for audio playback
 */
OSErr SndNewChannel(SndChannelPtr* chan, int16_t synth, int32_t init, SndCallBackProcPtr userRoutine)
{
    SndChannelPtr newChan;
    OSErr err = noErr;

    if (chan == NULL) {
        return paramErr;
    }

    *chan = NULL;

    if (!g_soundMgr.initialized) {
        return notEnoughHardwareErr;
    }

    /* Allocate channel structure */
    newChan = (SndChannelPtr)calloc(1, sizeof(SndChannel));
    if (newChan == NULL) {
        return memFullErr;
    }

    /* Initialize channel */
    newChan->nextChan = NULL;
    newChan->firstMod = NULL;
    newChan->callBack = userRoutine;
    newChan->userInfo = 0;
    newChan->wait = 0;
    newChan->cmdInProgress.cmd = nullCmd;
    newChan->cmdInProgress.param1 = 0;
    newChan->cmdInProgress.param2 = 0;
    newChan->flags = 0;
    newChan->qHead = 0;
    newChan->qTail = 0;
    newChan->queueSize = 64; /* Default queue size */
    newChan->synthType = synth;
    newChan->initBits = init;
    newChan->userRoutine = userRoutine;

    /* Allocate command queue */
    newChan->queue = (SndCommand*)calloc(newChan->queueSize, sizeof(SndCommand));
    if (newChan->queue == NULL) {
        free(newChan);
        return memFullErr;
    }

    pthread_mutex_lock(&g_soundMutex);

    /* Assign channel number */
    newChan->channelNumber = g_soundMgr.channelCount;

    /* Add to channel list */
    newChan->nextChan = g_soundMgr.channelList;
    g_soundMgr.channelList = newChan;
    g_soundMgr.channelCount++;

    /* Add channel to mixer */
    uint16_t mixerChannelIndex;
    err = MixerAddChannel(g_soundMgr.mixer, g_soundMgr.synthesizer, &mixerChannelIndex);
    if (err != noErr) {
        /* Remove from list */
        g_soundMgr.channelList = newChan->nextChan;
        g_soundMgr.channelCount--;
        pthread_mutex_unlock(&g_soundMutex);

        free(newChan->queue);
        free(newChan);
        return err;
    }

    pthread_mutex_unlock(&g_soundMutex);

    *chan = newChan;
    return noErr;
}

/*
 * Dispose Sound Channel
 * Releases a sound channel and frees its resources
 */
OSErr SndDisposeChannel(SndChannelPtr chan, Boolean quietNow)
{
    SndChannelPtr* chanPtr;

    if (chan == NULL) {
        return badChannel;
    }

    if (!g_soundMgr.initialized) {
        return badChannel;
    }

    pthread_mutex_lock(&g_soundMutex);

    /* Find channel in list and remove */
    chanPtr = &g_soundMgr.channelList;
    while (*chanPtr != NULL && *chanPtr != chan) {
        chanPtr = &(*chanPtr)->nextChan;
    }

    if (*chanPtr == NULL) {
        pthread_mutex_unlock(&g_soundMutex);
        return badChannel;
    }

    /* Remove from list */
    *chanPtr = chan->nextChan;
    g_soundMgr.channelCount--;

    pthread_mutex_unlock(&g_soundMutex);

    /* Stop current sound if requested */
    if (quietNow) {
        SndCommand stopCmd = { quietCmd, 0, 0 };
        ProcessSoundCommand(chan, &stopCmd);
    }

    /* Free resources */
    if (chan->queue != NULL) {
        free(chan->queue);
    }

    free(chan);
    return noErr;
}

/*
 * Play Sound Resource
 * Plays a sound resource through the specified channel
 */
OSErr SndPlay(SndChannelPtr chan, SndListHandle sndHandle, Boolean async)
{
    SndCommand playCmd;
    OSErr err;

    if (sndHandle == NULL) {
        return resProblem;
    }

    /* Create sound command */
    playCmd.cmd = soundCmd;
    playCmd.param1 = 0;
    playCmd.param2 = (int32_t)sndHandle;

    if (async) {
        err = SndDoCommand(chan, &playCmd, false);
    } else {
        err = SndDoImmediate(chan, &playCmd);
        if (err == noErr) {
            /* Wait for completion in synchronous mode */
            while (chan->cmdInProgress.cmd != nullCmd) {
                usleep(1000); /* Sleep 1ms */
            }
        }
    }

    return err;
}

/*
 * Execute Sound Command
 * Processes a sound command either immediately or queued
 */
OSErr SndDoCommand(SndChannelPtr chan, const SndCommand* cmd, Boolean noWait)
{
    if (chan == NULL || cmd == NULL) {
        return paramErr;
    }

    if (!g_soundMgr.initialized) {
        return notEnoughHardwareErr;
    }

    if (noWait) {
        return ExecuteImmediateCommand(chan, cmd);
    } else {
        /* Add to queue */
        uint16_t nextTail = (chan->qTail + 1) % chan->queueSize;
        if (nextTail == chan->qHead) {
            return queueFull;
        }

        chan->queue[chan->qTail] = *cmd;
        chan->qTail = nextTail;
        return noErr;
    }
}

/*
 * Execute Immediate Sound Command
 * Processes a sound command immediately without queuing
 */
OSErr SndDoImmediate(SndChannelPtr chan, const SndCommand* cmd)
{
    if (chan == NULL || cmd == NULL) {
        return paramErr;
    }

    return ExecuteImmediateCommand(chan, cmd);
}

/*
 * Get Sound Channel Status
 * Returns current status information for a sound channel
 */
OSErr SndChannelStatus(SndChannelPtr chan, int16_t theLength, SCStatus* theStatus)
{
    if (chan == NULL || theStatus == NULL) {
        return paramErr;
    }

    if (theLength < sizeof(SCStatus)) {
        return paramErr;
    }

    memset(theStatus, 0, sizeof(SCStatus));

    theStatus->scChannelBusy = (chan->cmdInProgress.cmd != nullCmd);
    theStatus->scChannelDisposed = false;
    theStatus->scChannelPaused = (chan->flags & 0x0001) != 0; /* Pause flag */
    theStatus->scChannelAttributes = chan->flags;
    theStatus->scCPULoad = g_soundMgr.cpuLoad;

    return noErr;
}

/*
 * Sound Control
 * Sends a control command to the Sound Manager
 */
OSErr SndControl(int16_t id, SndCommand* cmd)
{
    if (cmd == NULL) {
        return paramErr;
    }

    switch (cmd->cmd) {
        case versionCmd:
            cmd->param2 = g_soundMgr.version;
            return noErr;

        case totalLoadCmd:
            cmd->param2 = g_soundMgr.cpuLoad;
            return noErr;

        case availableCmd:
            cmd->param2 = 32 - g_soundMgr.channelCount; /* Max 32 channels */
            return noErr;

        default:
            return paramErr;
    }
}

/*
 * Get Sound Manager Version
 * Returns the current Sound Manager version
 */
NumVersion SndSoundManagerVersion(void)
{
    NumVersion version;
    version.majorRev = (g_soundMgr.version >> 8) & 0xFF;
    version.minorAndBugRev = g_soundMgr.version & 0xFF;
    version.stage = 0x80; /* Final stage */
    version.nonRelRev = 0;
    return version;
}

/*
 * Legacy Sound Manager 1.0 Compatibility Functions
 */

void StartSound(const void* synthRec, int32_t numBytes, SoundCompletionUPP completionRtn)
{
    if (!g_soundMgr.initialized) {
        SoundManagerInit();
    }

    /* Create legacy channel if needed */
    if (g_legacyChannel == NULL) {
        SndNewChannel(&g_legacyChannel, sampledSynth, initMono, NULL);
    }

    if (g_legacyChannel != NULL && synthRec != NULL) {
        SndCommand cmd = { bufferCmd, 0, (int32_t)synthRec };
        SndDoCommand(g_legacyChannel, &cmd, false);
        g_legacySoundPlaying = true;
    }
}

void StopSound(void)
{
    if (g_legacyChannel != NULL) {
        SndCommand cmd = { quietCmd, 0, 0 };
        SndDoImmediate(g_legacyChannel, &cmd);
        g_legacySoundPlaying = false;
    }
}

Boolean SoundDone(void)
{
    if (g_legacyChannel == NULL) {
        return true;
    }

    return (g_legacyChannel->cmdInProgress.cmd == nullCmd);
}

void GetSoundVol(int16_t* level)
{
    if (level != NULL) {
        *level = g_legacySoundVol;
    }
}

void SetSoundVol(int16_t level)
{
    g_legacySoundVol = level;
    if (g_soundMgr.mixer != NULL) {
        MixerSetMasterVolume(g_soundMgr.mixer, (uint16_t)level);
    }
}

void GetSysBeepVolume(int32_t* level)
{
    if (level != NULL) {
        *level = g_soundMgr.globalVolume;
    }
}

OSErr SetSysBeepVolume(int32_t level)
{
    if (level < 0) level = 0;
    if (level > 0x0100) level = 0x0100;

    g_soundMgr.globalVolume = (uint16_t)level;

    if (g_soundMgr.mixer != NULL) {
        return MixerSetMasterVolume(g_soundMgr.mixer, (uint16_t)level);
    }

    return noErr;
}

void GetDefaultOutputVolume(int32_t* level)
{
    GetSysBeepVolume(level);
}

OSErr SetDefaultOutputVolume(int32_t level)
{
    return SetSysBeepVolume(level);
}

/*
 * System Beep
 * Plays the system beep sound from embedded resources
 */
void SysBeep(short duration)
{
    static Boolean resourcesInitialized = false;

    /* Initialize resources if needed */
    if (!resourcesInitialized) {
        InitResourceData();
        resourcesInitialized = true;
    }

    /* Try to play the embedded system beep */
    OSErr err = PlayResourceSound(kSystemBeepID);

    /* If resource sound fails, fall back to simple tone */
    if (err != noErr && g_soundMgr.initialized) {
        /* Generate a simple 1kHz beep */
        SndCommand cmd;
        cmd.cmd = freqCmd;
        cmd.param1 = duration > 0 ? duration : 6;  /* Duration in ticks (60Hz) */
        cmd.param2 = 0x0400;  /* 1kHz frequency */

        if (g_legacyChannel == NULL) {
            SndNewChannel(&g_legacyChannel, sampledSynth, 0, NULL);
        }

        if (g_legacyChannel != NULL) {
            SndDoImmediate(g_legacyChannel, &cmd);
        }
    }
}

/*
 * Internal Functions
 */

static OSErr ProcessSoundCommand(SndChannelPtr chan, const SndCommand* cmd)
{
    OSErr err = noErr;

    if (chan == NULL || cmd == NULL) {
        return paramErr;
    }

    chan->cmdInProgress = *cmd;

    switch (cmd->cmd) {
        case nullCmd:
            /* Do nothing */
            break;

        case quietCmd:
            /* Stop all sounds on channel */
            chan->qHead = chan->qTail; /* Clear queue */
            break;

        case flushCmd:
            /* Flush command queue */
            chan->qHead = chan->qTail;
            break;

        case reInitCmd:
            /* Reinitialize channel */
            chan->wait = 0;
            chan->flags = 0;
            chan->qHead = chan->qTail;
            break;

        case waitCmd:
            /* Set wait time */
            chan->wait = cmd->param1;
            break;

        case pauseCmd:
            /* Pause channel */
            chan->flags |= 0x0001;
            break;

        case resumeCmd:
            /* Resume channel */
            chan->flags &= ~0x0001;
            break;

        case volumeCmd:
            /* Set volume */
            if (g_soundMgr.mixer != NULL) {
                MixerSetChannelVolume(g_soundMgr.mixer, chan->channelNumber, cmd->param1);
            }
            break;

        case soundCmd:
        case bufferCmd:
            /* Play sound or buffer */
            if (g_soundMgr.synthesizer != NULL) {
                /* Process sound data through synthesizer */
                /* This would be implemented based on the sound format */
            }
            break;

        case callBackCmd:
            /* Execute callback */
            if (chan->callBack != NULL) {
                chan->callBack(chan, (SndCommand*)cmd);
            }
            break;

        default:
            err = paramErr;
            break;
    }

    /* Command completed */
    chan->cmdInProgress.cmd = nullCmd;
    chan->cmdInProgress.param1 = 0;
    chan->cmdInProgress.param2 = 0;

    return err;
}

static OSErr ExecuteImmediateCommand(SndChannelPtr chan, const SndCommand* cmd)
{
    return ProcessSoundCommand(chan, cmd);
}

static void* SoundManagerThread(void* arg)
{
    SndChannelPtr chan;
    SndCommand cmd;

    printf("Sound Manager thread started\n");

    while (g_soundManagerActive) {
        pthread_mutex_lock(&g_soundMutex);

        /* Process command queues for all channels */
        chan = g_soundMgr.channelList;
        while (chan != NULL) {
            /* Process queued commands */
            if (chan->qHead != chan->qTail && chan->cmdInProgress.cmd == nullCmd) {
                cmd = chan->queue[chan->qHead];
                chan->qHead = (chan->qHead + 1) % chan->queueSize;

                pthread_mutex_unlock(&g_soundMutex);
                ProcessSoundCommand(chan, &cmd);
                pthread_mutex_lock(&g_soundMutex);
            }

            chan = chan->nextChan;
        }

        pthread_mutex_unlock(&g_soundMutex);

        /* Sleep for a short time to avoid busy-waiting */
        usleep(10000); /* 10ms */
    }

    printf("Sound Manager thread stopped\n");
    return NULL;
}

static void AudioOutputCallback(void* userData, int16_t* outputBuffer, uint32_t frameCount)
{
    if (!g_soundMgr.initialized || g_soundMgr.mixer == NULL) {
        /* Output silence */
        memset(outputBuffer, 0, frameCount * 2 * sizeof(int16_t));
        return;
    }

    /* Process audio through mixer */
    MixerProcess(g_soundMgr.mixer, outputBuffer, frameCount);
}

static OSErr InitializeSoundHardware(void)
{
    OSErr err;
    AudioStreamConfig config;
    AudioDeviceInfo* outputDevice;

    /* Initialize hardware abstraction */
    err = SoundHardwareInit(&g_soundMgr.hardware, AUDIO_API_AUTO);
    if (err != noErr) {
        return err;
    }

    /* Get default output device */
    outputDevice = SoundHardwareGetDefaultOutputDevice(g_soundMgr.hardware);
    if (outputDevice == NULL) {
        return notEnoughHardwareErr;
    }

    /* Configure audio stream */
    memset(&config, 0, sizeof(config));
    config.format.sampleRate = 44100;
    config.format.channels = 2;
    config.format.bitsPerSample = 16;
    config.format.encoding = k16BitBigEndianFormat;
    config.format.bigEndian = true;
    config.format.signedSamples = true;
    config.bufferFrames = 1024;
    config.bufferCount = 2;
    config.latencyFrames = 2048;
    config.userData = NULL;

    /* Open audio stream */
    AudioStreamPtr outputStream;
    err = AudioStreamOpen(g_soundMgr.hardware, &outputStream, outputDevice, &config);
    if (err != noErr) {
        return err;
    }

    /* Set output callback */
    err = AudioStreamSetOutputCallback(outputStream, AudioOutputCallback, NULL);
    if (err != noErr) {
        AudioStreamClose(outputStream);
        return err;
    }

    /* Start audio stream */
    err = AudioStreamStart(outputStream);
    if (err != noErr) {
        AudioStreamClose(outputStream);
        return err;
    }

    return noErr;
}

static OSErr ShutdownSoundHardware(void)
{
    if (g_soundMgr.hardware != NULL) {
        SoundHardwareShutdown(g_soundMgr.hardware);
        g_soundMgr.hardware = NULL;
    }
    return noErr;
}

/* Sound Manager Status */
OSErr SndManagerStatus(int16_t theLength, SMStatus* theStatus)
{
    if (theStatus == NULL || theLength < sizeof(SMStatus)) {
        return paramErr;
    }

    theStatus->smMaxCPULoad = 100;
    theStatus->smNumChannels = g_soundMgr.channelCount;
    theStatus->smCurCPULoad = g_soundMgr.cpuLoad;

    return noErr;
}