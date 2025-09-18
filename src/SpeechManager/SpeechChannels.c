/*
 * File: SpeechChannels.c
 *
 * Contains: Speech channel management and control implementation
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This file implements speech channel functionality
 *              including channel lifecycle, properties, and control.
 */

#include "../include/SpeechManager/SpeechChannels.h"
#include "../include/SpeechManager/SpeechSynthesis.h"
#include "../include/SpeechManager/TextToSpeech.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* ===== Speech Channel Implementation ===== */

/* Maximum number of channels */
#define MAX_SPEECH_CHANNELS 64
#define DEFAULT_BUFFER_SIZE 8192
#define DEFAULT_QUEUE_SIZE 16

/* Speech channel record structure */
typedef struct SpeechChannelRecord {
    /* Channel identification */
    long channelID;
    SpeechChannelType type;
    SpeechChannelState state;
    SpeechChannelPriority priority;
    SpeechChannelFlags flags;

    /* Voice and synthesis */
    VoiceSpec currentVoice;
    void *synthEngine;  /* SynthEngineRef */

    /* Speech parameters */
    Fixed currentRate;
    Fixed currentPitch;
    Fixed currentVolume;

    /* Text processing */
    void *textProcessor;  /* TextProcessingContext */
    char *textBuffer;
    long textBufferSize;
    long textBufferUsed;

    /* Audio output */
    void *audioBuffer;
    long audioBufferSize;
    long audioBufferUsed;

    /* Statistics and monitoring */
    SpeechChannelStats stats;
    time_t creationTime;
    time_t lastActiveTime;

    /* Callbacks */
    SpeechTextDoneProcPtr textDoneCallback;
    SpeechDoneProcPtr speechDoneCallback;
    SpeechSyncProcPtr syncCallback;
    SpeechErrorProcPtr errorCallback;
    SpeechPhonemeProcPtr phonemeCallback;
    SpeechWordProcPtr wordCallback;
    SpeechChannelEventProc eventCallback;
    void *callbackUserData;

    /* Threading and synchronization */
    pthread_mutex_t channelMutex;
    pthread_cond_t stateCondition;
    bool threadSafe;

    /* Resource management */
    void *dictionary;
    long memoryLimit;
    long currentMemoryUsage;

    /* Error tracking */
    OSErr lastError;
    char *lastErrorMessage;

    /* User data */
    void *userData;

    /* Internal state */
    bool isValid;
    long refCount;
} SpeechChannelRecord;

/* Channel manager state */
typedef struct ChannelManagerState {
    bool initialized;
    SpeechChannelRecord channels[MAX_SPEECH_CHANNELS];
    short channelCount;
    long nextChannelID;
    SpeechChannelConfig defaultConfig;
    bool systemBusy;
    pthread_mutex_t managerMutex;
} ChannelManagerState;

static ChannelManagerState gChannelManager = {
    .initialized = false,
    .channelCount = 0,
    .nextChannelID = 1,
    .systemBusy = false
};

/* ===== Internal Functions ===== */

/*
 * InitializeChannelManager
 * Initializes the channel management system
 */
static OSErr InitializeChannelManager(void) {
    if (gChannelManager.initialized) {
        return noErr;
    }

    /* Initialize manager state */
    memset(&gChannelManager, 0, sizeof(ChannelManagerState));

    /* Set up default configuration */
    gChannelManager.defaultConfig.type = kChannelType_Default;
    gChannelManager.defaultConfig.priority = kChannelPriority_Normal;
    gChannelManager.defaultConfig.flags = kChannelFlag_Callbacks;
    gChannelManager.defaultConfig.preferredVoice = NULL;
    gChannelManager.defaultConfig.initialRate = 0x00010000;     /* 1.0 */
    gChannelManager.defaultConfig.initialPitch = 0x00010000;    /* 1.0 */
    gChannelManager.defaultConfig.initialVolume = 0x00010000;   /* 1.0 */
    gChannelManager.defaultConfig.bufferSize = DEFAULT_BUFFER_SIZE;
    gChannelManager.defaultConfig.queueSize = DEFAULT_QUEUE_SIZE;

    gChannelManager.nextChannelID = 1;
    gChannelManager.channelCount = 0;

    /* Initialize mutex */
    int result = pthread_mutex_init(&gChannelManager.managerMutex, NULL);
    if (result != 0) {
        return memFullErr;
    }

    gChannelManager.initialized = true;
    return noErr;
}

/*
 * CleanupChannelManager
 * Cleans up channel management resources
 */
static void CleanupChannelManager(void) {
    if (!gChannelManager.initialized) {
        return;
    }

    pthread_mutex_lock(&gChannelManager.managerMutex);

    /* Clean up all channels */
    for (short i = 0; i < gChannelManager.channelCount; i++) {
        SpeechChannelRecord *chan = &gChannelManager.channels[i];
        if (chan->isValid) {
            DisposeSpeechChannel((SpeechChannel)chan);
        }
    }

    gChannelManager.initialized = false;
    gChannelManager.channelCount = 0;

    pthread_mutex_unlock(&gChannelManager.managerMutex);
    pthread_mutex_destroy(&gChannelManager.managerMutex);
}

/*
 * FindChannelRecord
 * Finds a channel record by channel reference
 */
static SpeechChannelRecord *FindChannelRecord(SpeechChannel chan) {
    if (!chan) return NULL;

    SpeechChannelRecord *record = (SpeechChannelRecord *)chan;

    /* Validate that this is actually a channel record */
    for (short i = 0; i < gChannelManager.channelCount; i++) {
        if (&gChannelManager.channels[i] == record && record->isValid) {
            return record;
        }
    }

    return NULL;
}

/*
 * AllocateChannelRecord
 * Allocates a new channel record
 */
static SpeechChannelRecord *AllocateChannelRecord(void) {
    if (gChannelManager.channelCount >= MAX_SPEECH_CHANNELS) {
        return NULL;
    }

    SpeechChannelRecord *record = &gChannelManager.channels[gChannelManager.channelCount];
    memset(record, 0, sizeof(SpeechChannelRecord));

    /* Initialize channel record */
    record->channelID = gChannelManager.nextChannelID++;
    record->state = kChannelState_Closed;
    record->isValid = true;
    record->refCount = 1;
    record->creationTime = time(NULL);
    record->lastActiveTime = record->creationTime;

    /* Initialize mutex and condition */
    pthread_mutex_init(&record->channelMutex, NULL);
    pthread_cond_init(&record->stateCondition, NULL);

    gChannelManager.channelCount++;
    return record;
}

/*
 * DeallocateChannelRecord
 * Deallocates a channel record
 */
static void DeallocateChannelRecord(SpeechChannelRecord *record) {
    if (!record || !record->isValid) {
        return;
    }

    /* Clean up resources */
    if (record->textBuffer) {
        free(record->textBuffer);
    }
    if (record->audioBuffer) {
        free(record->audioBuffer);
    }
    if (record->lastErrorMessage) {
        free(record->lastErrorMessage);
    }

    /* Clean up threading objects */
    pthread_mutex_destroy(&record->channelMutex);
    pthread_cond_destroy(&record->stateCondition);

    record->isValid = false;
    record->refCount = 0;

    /* Compact channel array */
    for (short i = 0; i < gChannelManager.channelCount; i++) {
        if (&gChannelManager.channels[i] == record) {
            memmove(&gChannelManager.channels[i], &gChannelManager.channels[i + 1],
                    (gChannelManager.channelCount - i - 1) * sizeof(SpeechChannelRecord));
            gChannelManager.channelCount--;
            break;
        }
    }
}

/*
 * SetChannelState
 * Sets channel state with proper synchronization
 */
static void SetChannelState(SpeechChannelRecord *record, SpeechChannelState newState) {
    if (!record) return;

    pthread_mutex_lock(&record->channelMutex);
    if (record->state != newState) {
        record->state = newState;
        record->lastActiveTime = time(NULL);

        /* Notify event callback */
        if (record->eventCallback) {
            record->eventCallback((SpeechChannel)record, kChannelEvent_StateChanged,
                                  &newState, record->callbackUserData);
        }

        pthread_cond_broadcast(&record->stateCondition);
    }
    pthread_mutex_unlock(&record->channelMutex);
}

/*
 * UpdateSystemBusyState
 * Updates system-wide busy state
 */
static void UpdateSystemBusyState(void) {
    bool anyBusy = false;

    for (short i = 0; i < gChannelManager.channelCount; i++) {
        SpeechChannelRecord *record = &gChannelManager.channels[i];
        if (record->isValid &&
            (record->state == kChannelState_Speaking || record->state == kChannelState_Paused)) {
            anyBusy = true;
            break;
        }
    }

    gChannelManager.systemBusy = anyBusy;
}

/* ===== Public API Implementation ===== */

/*
 * NewSpeechChannel
 * Creates a new speech channel
 */
OSErr NewSpeechChannel(VoiceSpec *voice, SpeechChannel *chan) {
    if (!chan) {
        return paramErr;
    }

    /* Initialize channel manager if needed */
    OSErr err = InitializeChannelManager();
    if (err != noErr) {
        return err;
    }

    pthread_mutex_lock(&gChannelManager.managerMutex);

    /* Allocate new channel record */
    SpeechChannelRecord *record = AllocateChannelRecord();
    if (!record) {
        pthread_mutex_unlock(&gChannelManager.managerMutex);
        return memFullErr;
    }

    /* Apply default configuration */
    record->type = gChannelManager.defaultConfig.type;
    record->priority = gChannelManager.defaultConfig.priority;
    record->flags = gChannelManager.defaultConfig.flags;
    record->currentRate = gChannelManager.defaultConfig.initialRate;
    record->currentPitch = gChannelManager.defaultConfig.initialPitch;
    record->currentVolume = gChannelManager.defaultConfig.initialVolume;

    /* Set voice if provided */
    if (voice) {
        record->currentVoice = *voice;
    } else {
        /* Use default voice */
        VoiceSpec defaultVoice;
        err = GetDefaultVoice(&defaultVoice);
        if (err == noErr) {
            record->currentVoice = defaultVoice;
        }
    }

    /* Allocate text buffer */
    record->textBufferSize = gChannelManager.defaultConfig.bufferSize;
    record->textBuffer = malloc(record->textBufferSize);
    if (!record->textBuffer) {
        DeallocateChannelRecord(record);
        pthread_mutex_unlock(&gChannelManager.managerMutex);
        return memFullErr;
    }

    /* Open the channel */
    SetChannelState(record, kChannelState_Open);

    *chan = (SpeechChannel)record;

    pthread_mutex_unlock(&gChannelManager.managerMutex);
    return noErr;
}

/*
 * DisposeSpeechChannel
 * Disposes of a speech channel
 */
OSErr DisposeSpeechChannel(SpeechChannel chan) {
    if (!chan) {
        return paramErr;
    }

    pthread_mutex_lock(&gChannelManager.managerMutex);

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        pthread_mutex_unlock(&gChannelManager.managerMutex);
        return paramErr;
    }

    /* Stop any active speech */
    if (record->state == kChannelState_Speaking || record->state == kChannelState_Paused) {
        StopSpeech(chan);
    }

    /* Close the channel */
    SetChannelState(record, kChannelState_Closed);

    /* Deallocate the record */
    DeallocateChannelRecord(record);

    /* Update system state */
    UpdateSystemBusyState();

    pthread_mutex_unlock(&gChannelManager.managerMutex);
    return noErr;
}

/*
 * SpeakText
 * Speaks text through a speech channel
 */
OSErr SpeakText(SpeechChannel chan, void *textBuf, long textBytes) {
    if (!chan || !textBuf || textBytes <= 0) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record || record->state == kChannelState_Closed) {
        return synthNotReady;
    }

    pthread_mutex_lock(&record->channelMutex);

    /* Stop any current speech */
    if (record->state == kChannelState_Speaking) {
        SetChannelState(record, kChannelState_Stopping);
        /* In a real implementation, this would stop the synthesis engine */
    }

    /* Copy text to buffer */
    if (textBytes > record->textBufferSize) {
        char *newBuffer = realloc(record->textBuffer, textBytes + 1);
        if (!newBuffer) {
            pthread_mutex_unlock(&record->channelMutex);
            return memFullErr;
        }
        record->textBuffer = newBuffer;
        record->textBufferSize = textBytes + 1;
    }

    memcpy(record->textBuffer, textBuf, textBytes);
    record->textBuffer[textBytes] = '\0';
    record->textBufferUsed = textBytes;

    /* Update statistics */
    record->stats.totalSyntheses++;
    record->stats.totalBytes += textBytes;
    record->lastActiveTime = time(NULL);

    /* Start speaking */
    SetChannelState(record, kChannelState_Speaking);
    UpdateSystemBusyState();

    /* Notify text started event */
    if (record->eventCallback) {
        record->eventCallback(chan, kChannelEvent_TextStarted, textBuf,
                              record->callbackUserData);
    }

    /* In a real implementation, this would start the synthesis engine */
    /* For now, we'll simulate immediate completion */
    SetChannelState(record, kChannelState_Open);
    UpdateSystemBusyState();

    /* Notify completion */
    if (record->speechDoneCallback) {
        record->speechDoneCallback(chan, (long)record->callbackUserData);
    }

    if (record->eventCallback) {
        record->eventCallback(chan, kChannelEvent_TextCompleted, NULL,
                              record->callbackUserData);
    }

    pthread_mutex_unlock(&record->channelMutex);
    return noErr;
}

/*
 * StopSpeech
 * Stops speech on a channel
 */
OSErr StopSpeech(SpeechChannel chan) {
    if (!chan) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);

    if (record->state == kChannelState_Speaking || record->state == kChannelState_Paused) {
        SetChannelState(record, kChannelState_Stopping);

        /* In a real implementation, this would stop the synthesis engine */

        SetChannelState(record, kChannelState_Open);
        UpdateSystemBusyState();
    }

    pthread_mutex_unlock(&record->channelMutex);
    return noErr;
}

/*
 * PauseSpeechAt
 * Pauses speech at a specific point
 */
OSErr PauseSpeechAt(SpeechChannel chan, long whereToPause) {
    if (!chan) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);

    if (record->state == kChannelState_Speaking) {
        SetChannelState(record, kChannelState_Paused);
        /* In a real implementation, this would pause the synthesis engine */
    }

    pthread_mutex_unlock(&record->channelMutex);
    return noErr;
}

/*
 * ContinueSpeech
 * Continues paused speech
 */
OSErr ContinueSpeech(SpeechChannel chan) {
    if (!chan) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);

    if (record->state == kChannelState_Paused) {
        SetChannelState(record, kChannelState_Speaking);
        UpdateSystemBusyState();
        /* In a real implementation, this would resume the synthesis engine */
    }

    pthread_mutex_unlock(&record->channelMutex);
    return noErr;
}

/*
 * IsSpeechChannelBusy
 * Checks if a speech channel is busy
 */
bool IsSpeechChannelBusy(SpeechChannel chan) {
    if (!chan) {
        return false;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return false;
    }

    return (record->state == kChannelState_Speaking || record->state == kChannelState_Paused);
}

/*
 * IsSpeechSystemBusy
 * Checks if any speech channel is busy system-wide
 */
bool IsSpeechSystemBusy(void) {
    if (!gChannelManager.initialized) {
        return false;
    }

    return gChannelManager.systemBusy;
}

/*
 * SetSpeechChannelRate
 * Sets the speech rate for a channel
 */
OSErr SetSpeechChannelRate(SpeechChannel chan, Fixed rate) {
    if (!chan) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);
    record->currentRate = rate;
    pthread_mutex_unlock(&record->channelMutex);

    return noErr;
}

/*
 * GetSpeechChannelRate
 * Gets the speech rate for a channel
 */
OSErr GetSpeechChannelRate(SpeechChannel chan, Fixed *rate) {
    if (!chan || !rate) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);
    *rate = record->currentRate;
    pthread_mutex_unlock(&record->channelMutex);

    return noErr;
}

/*
 * SetSpeechChannelPitch
 * Sets the speech pitch for a channel
 */
OSErr SetSpeechChannelPitch(SpeechChannel chan, Fixed pitch) {
    if (!chan) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);
    record->currentPitch = pitch;
    pthread_mutex_unlock(&record->channelMutex);

    return noErr;
}

/*
 * GetSpeechChannelPitch
 * Gets the speech pitch for a channel
 */
OSErr GetSpeechChannelPitch(SpeechChannel chan, Fixed *pitch) {
    if (!chan || !pitch) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);
    *pitch = record->currentPitch;
    pthread_mutex_unlock(&record->channelMutex);

    return noErr;
}

/*
 * SetSpeechChannelInfo
 * Sets speech channel information using selector
 */
OSErr SetSpeechChannelInfo(SpeechChannel chan, OSType selector, void *speechInfo) {
    if (!chan || !speechInfo) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);

    OSErr err = noErr;
    switch (selector) {
        case soRate:
            record->currentRate = *(Fixed *)speechInfo;
            break;

        case soPitchBase:
            record->currentPitch = *(Fixed *)speechInfo;
            break;

        case soVolume:
            record->currentVolume = *(Fixed *)speechInfo;
            break;

        case soRefCon:
            record->callbackUserData = speechInfo;
            break;

        case soTextDoneCallBack:
            record->textDoneCallback = (SpeechTextDoneProcPtr)speechInfo;
            break;

        case soSpeechDoneCallBack:
            record->speechDoneCallback = (SpeechDoneProcPtr)speechInfo;
            break;

        case soSyncCallBack:
            record->syncCallback = (SpeechSyncProcPtr)speechInfo;
            break;

        case soErrorCallBack:
            record->errorCallback = (SpeechErrorProcPtr)speechInfo;
            break;

        case soPhonemeCallBack:
            record->phonemeCallback = (SpeechPhonemeProcPtr)speechInfo;
            break;

        case soWordCallBack:
            record->wordCallback = (SpeechWordProcPtr)speechInfo;
            break;

        default:
            err = paramErr;
            break;
    }

    pthread_mutex_unlock(&record->channelMutex);
    return err;
}

/*
 * GetSpeechChannelInfo
 * Gets speech channel information using selector
 */
OSErr GetSpeechChannelInfo(SpeechChannel chan, OSType selector, void *speechInfo) {
    if (!chan || !speechInfo) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);

    OSErr err = noErr;
    switch (selector) {
        case soStatus: {
            SpeechStatusInfo *status = (SpeechStatusInfo *)speechInfo;
            status->outputBusy = (record->state == kChannelState_Speaking);
            status->outputPaused = (record->state == kChannelState_Paused);
            status->inputBytesLeft = record->textBufferUsed;
            status->phonemeCode = 0; /* Current phoneme - would be set by synthesis engine */
            break;
        }

        case soRate:
            *(Fixed *)speechInfo = record->currentRate;
            break;

        case soPitchBase:
            *(Fixed *)speechInfo = record->currentPitch;
            break;

        case soVolume:
            *(Fixed *)speechInfo = record->currentVolume;
            break;

        case soCurrentVoice:
            *(VoiceSpec *)speechInfo = record->currentVoice;
            break;

        case soRefCon:
            *(void **)speechInfo = record->callbackUserData;
            break;

        default:
            err = paramErr;
            break;
    }

    pthread_mutex_unlock(&record->channelMutex);
    return err;
}

/*
 * SetSpeechChannelDictionary
 * Sets dictionary for a speech channel
 */
OSErr SetSpeechChannelDictionary(SpeechChannel chan, void *dictionary) {
    if (!chan) {
        return paramErr;
    }

    SpeechChannelRecord *record = FindChannelRecord(chan);
    if (!record) {
        return paramErr;
    }

    pthread_mutex_lock(&record->channelMutex);
    record->dictionary = dictionary;
    pthread_mutex_unlock(&record->channelMutex);

    return noErr;
}

/* Module cleanup */
static void __attribute__((destructor)) SpeechChannelsCleanup(void) {
    CleanupChannelManager();
}