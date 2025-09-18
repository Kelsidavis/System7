/*
 * File: SpeechChannels.h
 *
 * Contains: Speech channel management and control for Speech Manager
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This header provides speech channel functionality
 *              including channel lifecycle, properties, and control.
 */

#ifndef _SPEECHCHANNELS_H_
#define _SPEECHCHANNELS_H_

#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Speech Channel Constants ===== */

/* Channel states */
typedef enum {
    kChannelState_Closed        = 0,
    kChannelState_Open          = 1,
    kChannelState_Speaking      = 2,
    kChannelState_Paused        = 3,
    kChannelState_Stopping      = 4,
    kChannelState_Error         = 5
} SpeechChannelState;

/* Channel types */
typedef enum {
    kChannelType_Default        = 0,  /* Default speech channel */
    kChannelType_Private        = 1,  /* Private channel (SpeakString) */
    kChannelType_Shared         = 2,  /* Shared system channel */
    kChannelType_Background     = 3,  /* Background speech channel */
    kChannelType_Priority       = 4   /* Priority speech channel */
} SpeechChannelType;

/* Channel priorities */
typedef enum {
    kChannelPriority_Low        = 1,
    kChannelPriority_Normal     = 2,
    kChannelPriority_High       = 3,
    kChannelPriority_Critical   = 4,
    kChannelPriority_Emergency  = 5
} SpeechChannelPriority;

/* Channel flags */
typedef enum {
    kChannelFlag_AutoDispose    = (1 << 0),  /* Auto-dispose when done */
    kChannelFlag_Exclusive      = (1 << 1),  /* Exclusive access */
    kChannelFlag_Interrupt      = (1 << 2),  /* Can interrupt others */
    kChannelFlag_Background     = (1 << 3),  /* Background processing */
    kChannelFlag_Queue          = (1 << 4),  /* Queue multiple texts */
    kChannelFlag_Callbacks      = (1 << 5),  /* Enable callbacks */
    kChannelFlag_Streaming      = (1 << 6),  /* Streaming mode */
    kChannelFlag_Monitoring     = (1 << 7)   /* Enable monitoring */
} SpeechChannelFlags;

/* ===== Speech Channel Structures ===== */

/* Channel information */
typedef struct SpeechChannelInfo {
    SpeechChannel channel;          /* Channel reference */
    SpeechChannelType type;         /* Channel type */
    SpeechChannelState state;       /* Current state */
    SpeechChannelPriority priority; /* Channel priority */
    SpeechChannelFlags flags;       /* Channel flags */
    VoiceSpec currentVoice;         /* Current voice */
    Fixed currentRate;              /* Current speech rate */
    Fixed currentPitch;             /* Current pitch */
    Fixed currentVolume;            /* Current volume */
    long totalBytesSpoken;          /* Total bytes processed */
    long totalWordsSpoken;          /* Total words spoken */
    time_t creationTime;            /* Channel creation time */
    time_t lastActiveTime;          /* Last activity time */
    void *userData;                 /* User data */
} SpeechChannelInfo;

/* Channel configuration */
typedef struct SpeechChannelConfig {
    SpeechChannelType type;         /* Desired channel type */
    SpeechChannelPriority priority; /* Channel priority */
    SpeechChannelFlags flags;       /* Channel flags */
    VoiceSpec *preferredVoice;      /* Preferred voice (can be NULL) */
    Fixed initialRate;              /* Initial speech rate */
    Fixed initialPitch;             /* Initial pitch */
    Fixed initialVolume;            /* Initial volume */
    long bufferSize;                /* Audio buffer size */
    long queueSize;                 /* Text queue size */
    void *userData;                 /* User data */
} SpeechChannelConfig;

/* Channel statistics */
typedef struct SpeechChannelStats {
    long totalSyntheses;            /* Total synthesis operations */
    long totalBytes;                /* Total bytes processed */
    long totalWords;                /* Total words spoken */
    long totalTime;                 /* Total active time (ms) */
    long errorCount;                /* Number of errors */
    long queuedItems;               /* Items in queue */
    long averageRate;               /* Average synthesis rate */
    double cpuUsage;                /* CPU usage percentage */
    long memoryUsage;               /* Memory usage in bytes */
} SpeechChannelStats;

/* ===== Channel Management ===== */

/* Channel creation and disposal */
OSErr NewSpeechChannelWithConfig(const SpeechChannelConfig *config, SpeechChannel *chan);
OSErr CloneSpeechChannel(SpeechChannel sourceChannel, SpeechChannel *newChannel);

/* Channel information */
OSErr GetSpeechChannelInfo(SpeechChannel chan, SpeechChannelInfo *info);
OSErr SetSpeechChannelInfo(SpeechChannel chan, const SpeechChannelInfo *info);

/* Channel state management */
OSErr GetSpeechChannelState(SpeechChannel chan, SpeechChannelState *state);
OSErr OpenSpeechChannel(SpeechChannel chan);
OSErr CloseSpeechChannel(SpeechChannel chan);
OSErr ResetSpeechChannel(SpeechChannel chan);

/* ===== Channel Properties ===== */

/* Voice management for channels */
OSErr SetSpeechChannelVoice(SpeechChannel chan, const VoiceSpec *voice);
OSErr GetSpeechChannelVoice(SpeechChannel chan, VoiceSpec *voice);

/* Rate control */
OSErr SetSpeechChannelRate(SpeechChannel chan, Fixed rate);
OSErr GetSpeechChannelRate(SpeechChannel chan, Fixed *rate);

/* Pitch control */
OSErr SetSpeechChannelPitch(SpeechChannel chan, Fixed pitch);
OSErr GetSpeechChannelPitch(SpeechChannel chan, Fixed *pitch);

/* Volume control */
OSErr SetSpeechChannelVolume(SpeechChannel chan, Fixed volume);
OSErr GetSpeechChannelVolume(SpeechChannel chan, Fixed *volume);

/* Channel priority */
OSErr SetSpeechChannelPriority(SpeechChannel chan, SpeechChannelPriority priority);
OSErr GetSpeechChannelPriority(SpeechChannel chan, SpeechChannelPriority *priority);

/* Channel flags */
OSErr SetSpeechChannelFlags(SpeechChannel chan, SpeechChannelFlags flags);
OSErr GetSpeechChannelFlags(SpeechChannel chan, SpeechChannelFlags *flags);

/* ===== Channel Information and Control ===== */

/* Generic channel information */
OSErr SetSpeechChannelInfo(SpeechChannel chan, OSType selector, void *speechInfo);
OSErr GetSpeechChannelInfo(SpeechChannel chan, OSType selector, void *speechInfo);

/* Channel status */
bool IsSpeechChannelBusy(SpeechChannel chan);
bool IsSpeechChannelPaused(SpeechChannel chan);
OSErr GetSpeechChannelBytesLeft(SpeechChannel chan, long *bytesLeft);

/* Channel dictionary support */
OSErr SetSpeechChannelDictionary(SpeechChannel chan, void *dictionary);
OSErr GetSpeechChannelDictionary(SpeechChannel chan, void **dictionary);

/* ===== Channel Text Processing ===== */

/* Text speaking */
OSErr SpeakText(SpeechChannel chan, void *textBuf, long textBytes);
OSErr SpeakBuffer(SpeechChannel chan, void *textBuf, long textBytes, long controlFlags);

/* Text queueing */
OSErr QueueSpeechText(SpeechChannel chan, void *textBuf, long textBytes);
OSErr ClearSpeechQueue(SpeechChannel chan);
OSErr GetSpeechQueueLength(SpeechChannel chan, long *queueLength);

/* Text streaming */
OSErr BeginSpeechStream(SpeechChannel chan);
OSErr WriteSpeechStream(SpeechChannel chan, void *textBuf, long textBytes);
OSErr EndSpeechStream(SpeechChannel chan);

/* ===== Channel Control ===== */

/* Speech control */
OSErr StopSpeech(SpeechChannel chan);
OSErr StopSpeechAt(SpeechChannel chan, long whereToStop);
OSErr PauseSpeechAt(SpeechChannel chan, long whereToPause);
OSErr ContinueSpeech(SpeechChannel chan);

/* Channel synchronization */
OSErr WaitForSpeechCompletion(SpeechChannel chan, long timeoutMs);
OSErr FlushSpeechChannel(SpeechChannel chan);

/* ===== Channel Monitoring ===== */

/* Channel statistics */
OSErr GetSpeechChannelStats(SpeechChannel chan, SpeechChannelStats *stats);
OSErr ResetSpeechChannelStats(SpeechChannel chan);

/* Performance monitoring */
OSErr EnableSpeechChannelMonitoring(SpeechChannel chan, bool enable);
OSErr GetSpeechChannelPerformance(SpeechChannel chan, double *cpuUsage, long *memoryUsage);

/* Event monitoring */
typedef enum {
    kChannelEvent_StateChanged  = 1,
    kChannelEvent_TextStarted   = 2,
    kChannelEvent_TextCompleted = 3,
    kChannelEvent_WordStarted   = 4,
    kChannelEvent_PhonemeStarted = 5,
    kChannelEvent_Error         = 6,
    kChannelEvent_QueueChanged  = 7
} SpeechChannelEventType;

/* Event callback */
typedef void (*SpeechChannelEventProc)(SpeechChannel chan, SpeechChannelEventType eventType,
                                        void *eventData, void *userData);

OSErr SetSpeechChannelEventCallback(SpeechChannel chan, SpeechChannelEventProc callback,
                                    void *userData);

/* ===== System Channel Management ===== */

/* System-wide channel operations */
OSErr CountActiveSpeechChannels(short *channelCount);
OSErr GetActiveSpeechChannels(SpeechChannel **channels, short *channelCount);
bool IsSpeechSystemBusy(void);

/* Channel enumeration */
OSErr EnumerateSpeechChannels(bool (*callback)(SpeechChannel chan, void *userData), void *userData);

/* System channel control */
OSErr StopAllSpeechChannels(void);
OSErr PauseAllSpeechChannels(void);
OSErr ResumeAllSpeechChannels(void);

/* Channel priority management */
OSErr SetSystemChannelPriorities(bool enablePriorities);
OSErr InterruptLowerPriorityChannels(SpeechChannelPriority minimumPriority);

/* ===== Channel Resource Management ===== */

/* Memory management */
OSErr SetSpeechChannelMemoryLimit(SpeechChannel chan, long memoryLimit);
OSErr GetSpeechChannelMemoryUsage(SpeechChannel chan, long *memoryUsage);

/* Buffer management */
OSErr SetSpeechChannelBufferSize(SpeechChannel chan, long bufferSize);
OSErr GetSpeechChannelBufferSize(SpeechChannel chan, long *bufferSize);
OSErr FlushSpeechChannelBuffers(SpeechChannel chan);

/* Resource cleanup */
OSErr CleanupIdleChannels(long maxIdleTime);
OSErr OptimizeSpeechChannelMemory(SpeechChannel chan);

/* ===== Channel Threading ===== */

/* Thread safety */
OSErr LockSpeechChannel(SpeechChannel chan);
OSErr UnlockSpeechChannel(SpeechChannel chan);
OSErr SetSpeechChannelThreadSafe(SpeechChannel chan, bool threadSafe);

/* Background processing */
OSErr SetSpeechChannelBackgroundMode(SpeechChannel chan, bool backgroundMode);
OSErr GetSpeechChannelBackgroundMode(SpeechChannel chan, bool *backgroundMode);

/* ===== Channel Configuration ===== */

/* Default configuration */
OSErr GetDefaultChannelConfig(SpeechChannelConfig *config);
OSErr SetDefaultChannelConfig(const SpeechChannelConfig *config);

/* Channel presets */
OSErr SaveChannelPreset(SpeechChannel chan, const char *presetName);
OSErr LoadChannelPreset(SpeechChannel chan, const char *presetName);
OSErr DeleteChannelPreset(const char *presetName);

/* Configuration validation */
OSErr ValidateChannelConfig(const SpeechChannelConfig *config, bool *isValid,
                            char **errorMessage);

/* ===== Channel Debugging ===== */

/* Debug information */
OSErr GetSpeechChannelDebugInfo(SpeechChannel chan, char **debugInfo);
OSErr DumpSpeechChannelState(SpeechChannel chan, FILE *output);

/* Channel tracing */
OSErr EnableSpeechChannelTracing(SpeechChannel chan, bool enable);
OSErr GetSpeechChannelTrace(SpeechChannel chan, char **traceData, long *traceSize);

/* Error reporting */
OSErr GetSpeechChannelLastError(SpeechChannel chan, OSErr *error, char **errorMessage);
OSErr ClearSpeechChannelErrors(SpeechChannel chan);

/* ===== Channel Utilities ===== */

/* Channel comparison */
bool AreSpeechChannelsEqual(SpeechChannel chan1, SpeechChannel chan2);
int CompareSpeechChannels(SpeechChannel chan1, SpeechChannel chan2);

/* Channel validation */
bool IsValidSpeechChannel(SpeechChannel chan);
OSErr ValidateSpeechChannel(SpeechChannel chan);

/* Channel conversion */
OSErr SpeechChannelToString(SpeechChannel chan, char *string, long stringSize);
OSErr StringToSpeechChannel(const char *string, SpeechChannel *chan);

#ifdef __cplusplus
}
#endif

#endif /* _SPEECHCHANNELS_H_ */