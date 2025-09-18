/*
 * File: SpeechSynthesis.h
 *
 * Contains: Speech synthesis engine integration for Speech Manager
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This header provides speech synthesis engine functionality
 *              including engine management, audio synthesis, and output control.
 */

#ifndef _SPEECHSYNTHESIS_H_
#define _SPEECHSYNTHESIS_H_

#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Synthesis Engine Constants ===== */

/* Synthesis engine types */
typedef enum {
    kSynthEngine_Unknown    = 0,
    kSynthEngine_System     = 1,  /* System built-in engine */
    kSynthEngine_SAPI       = 2,  /* Microsoft SAPI */
    kSynthEngine_Festival   = 3,  /* Festival Speech Synthesis */
    kSynthEngine_eSpeak     = 4,  /* eSpeak/eSpeak-NG */
    kSynthEngine_Flite      = 5,  /* CMU Flite */
    kSynthEngine_Neural     = 6,  /* Neural TTS engines */
    kSynthEngine_Cloud      = 7   /* Cloud-based engines */
} SynthEngineType;

/* Synthesis quality levels */
typedef enum {
    kSynthQuality_Fastest   = 1,  /* Fastest synthesis, lowest quality */
    kSynthQuality_Fast      = 2,  /* Fast synthesis, moderate quality */
    kSynthQuality_Normal    = 3,  /* Normal synthesis, good quality */
    kSynthQuality_High      = 4,  /* High quality synthesis */
    kSynthQuality_Highest   = 5   /* Highest quality, slowest synthesis */
} SynthQuality;

/* Synthesis engine capabilities */
typedef enum {
    kSynthCap_TextToSpeech      = (1 << 0),   /* Basic TTS capability */
    kSynthCap_PhonemeSupport    = (1 << 1),   /* Phoneme input support */
    kSynthCap_SSML              = (1 << 2),   /* SSML markup support */
    kSynthCap_Emotions          = (1 << 3),   /* Emotional speech */
    kSynthCap_MultiVoice        = (1 << 4),   /* Multiple voice support */
    kSynthCap_RateControl       = (1 << 5),   /* Speech rate control */
    kSynthCap_PitchControl      = (1 << 6),   /* Pitch control */
    kSynthCap_VolumeControl     = (1 << 7),   /* Volume control */
    kSynthCap_Callbacks         = (1 << 8),   /* Progress callbacks */
    kSynthCap_Streaming         = (1 << 9),   /* Streaming synthesis */
    kSynthCap_AudioFormats      = (1 << 10),  /* Multiple audio formats */
    kSynthCap_RealTime          = (1 << 11)   /* Real-time synthesis */
} SynthEngineCapabilities;

/* Audio format specifications */
typedef enum {
    kAudioFormat_PCM16          = 1,  /* 16-bit PCM */
    kAudioFormat_PCM24          = 2,  /* 24-bit PCM */
    kAudioFormat_PCM32          = 3,  /* 32-bit PCM */
    kAudioFormat_Float32        = 4,  /* 32-bit float */
    kAudioFormat_MP3            = 5,  /* MP3 compressed */
    kAudioFormat_AAC            = 6,  /* AAC compressed */
    kAudioFormat_OGG            = 7   /* OGG Vorbis */
} AudioFormat;

/* ===== Synthesis Engine Structures ===== */

/* Audio format descriptor */
typedef struct AudioFormatDescriptor {
    AudioFormat format;
    long sampleRate;
    short channels;
    short bitsPerSample;
    long bytesPerFrame;
    long framesPerPacket;
    long bytesPerPacket;
    bool isInterleaved;
    void *formatSpecific;
} AudioFormatDescriptor;

/* Synthesis engine information */
typedef struct SynthEngineInfo {
    SynthEngineType type;
    OSType creator;
    OSType subType;
    long version;
    char name[64];
    char manufacturer[64];
    char description[256];
    SynthEngineCapabilities capabilities;
    SynthQuality maxQuality;
    AudioFormatDescriptor *supportedFormats;
    short formatCount;
    void *engineSpecific;
} SynthEngineInfo;

/* Synthesis parameters */
typedef struct SynthesisParameters {
    Fixed rate;                 /* Speech rate (1.0 = normal) */
    Fixed pitch;                /* Base pitch (1.0 = normal) */
    Fixed volume;               /* Volume level (1.0 = normal) */
    SynthQuality quality;       /* Synthesis quality level */
    AudioFormatDescriptor audioFormat; /* Output audio format */
    bool useCallbacks;          /* Enable progress callbacks */
    bool streamingMode;         /* Enable streaming synthesis */
    void *engineParameters;     /* Engine-specific parameters */
} SynthesisParameters;

/* Synthesis progress information */
typedef struct SynthesisProgress {
    long totalBytes;            /* Total bytes to synthesize */
    long processedBytes;        /* Bytes processed so far */
    long totalWords;            /* Total words to synthesize */
    long processedWords;        /* Words processed so far */
    long estimatedTimeRemaining; /* Estimated time in milliseconds */
    short currentPhoneme;       /* Current phoneme being synthesized */
    long currentWordPosition;   /* Position of current word */
    void *userData;             /* User data for callbacks */
} SynthesisProgress;

/* Synthesis result */
typedef struct SynthesisResult {
    void *audioData;            /* Synthesized audio data */
    long audioDataSize;         /* Size of audio data in bytes */
    AudioFormatDescriptor audioFormat; /* Format of audio data */
    long durationMs;            /* Duration in milliseconds */
    OSErr synthesisError;       /* Error code if synthesis failed */
    char *errorMessage;         /* Human-readable error message */
    void *metadata;             /* Additional metadata */
} SynthesisResult;

/* ===== Synthesis Engine Management ===== */

/* Engine opaque handle */
typedef struct SynthEngineRef *SynthEngineRef;

/* Engine initialization and cleanup */
OSErr InitializeSpeechSynthesis(void);
void CleanupSpeechSynthesis(void);

/* Engine enumeration */
OSErr CountSynthEngines(short *engineCount);
OSErr GetIndSynthEngine(short index, SynthEngineRef *engine);
OSErr GetSynthEngineInfo(SynthEngineRef engine, SynthEngineInfo *info);

/* Engine selection */
OSErr FindBestSynthEngine(SynthEngineCapabilities requiredCaps, SynthEngineRef *engine);
OSErr FindSynthEngineByType(SynthEngineType type, SynthEngineRef *engine);
OSErr FindSynthEngineByName(const char *engineName, SynthEngineRef *engine);

/* Engine lifecycle */
OSErr CreateSynthEngine(SynthEngineType type, SynthEngineRef *engine);
OSErr DisposeSynthEngine(SynthEngineRef engine);
OSErr OpenSynthEngine(SynthEngineRef engine);
OSErr CloseSynthEngine(SynthEngineRef engine);

/* ===== Synthesis Operations ===== */

/* Text synthesis */
OSErr SynthesizeText(SynthEngineRef engine, const char *text, long textLength,
                     const SynthesisParameters *params, SynthesisResult **result);
OSErr SynthesizeTextToFile(SynthEngineRef engine, const char *text, long textLength,
                           const SynthesisParameters *params, const char *outputFile);
OSErr SynthesizeTextStreaming(SynthEngineRef engine, const char *text, long textLength,
                              const SynthesisParameters *params, void *streamContext);

/* Phoneme synthesis */
OSErr SynthesizePhonemes(SynthEngineRef engine, const char *phonemes, long phonemeLength,
                         const SynthesisParameters *params, SynthesisResult **result);

/* SSML synthesis */
OSErr SynthesizeSSML(SynthEngineRef engine, const char *ssmlText, long ssmlLength,
                     const SynthesisParameters *params, SynthesisResult **result);

/* Result management */
OSErr DisposeSynthesisResult(SynthesisResult *result);
OSErr CopySynthesisResult(const SynthesisResult *source, SynthesisResult **dest);

/* ===== Synthesis Control ===== */

/* Synthesis state */
typedef enum {
    kSynthState_Idle        = 0,
    kSynthState_Preparing   = 1,
    kSynthState_Synthesizing = 2,
    kSynthState_Paused      = 3,
    kSynthState_Stopping    = 4,
    kSynthState_Error       = 5
} SynthesisState;

/* Synthesis control */
OSErr StartSynthesis(SynthEngineRef engine);
OSErr PauseSynthesis(SynthEngineRef engine);
OSErr ResumeSynthesis(SynthEngineRef engine);
OSErr StopSynthesis(SynthEngineRef engine);
OSErr GetSynthesisState(SynthEngineRef engine, SynthesisState *state);

/* Synthesis monitoring */
OSErr GetSynthesisProgress(SynthEngineRef engine, SynthesisProgress *progress);
bool IsSynthesisActive(SynthEngineRef engine);
OSErr WaitForSynthesisCompletion(SynthEngineRef engine, long timeoutMs);

/* ===== Synthesis Parameters ===== */

/* Parameter management */
OSErr SetSynthesisParameters(SynthEngineRef engine, const SynthesisParameters *params);
OSErr GetSynthesisParameters(SynthEngineRef engine, SynthesisParameters *params);
OSErr ResetSynthesisParameters(SynthEngineRef engine);

/* Individual parameter control */
OSErr SetSynthesisRate(SynthEngineRef engine, Fixed rate);
OSErr GetSynthesisRate(SynthEngineRef engine, Fixed *rate);
OSErr SetSynthesisPitch(SynthEngineRef engine, Fixed pitch);
OSErr GetSynthesisPitch(SynthEngineRef engine, Fixed *pitch);
OSErr SetSynthesisVolume(SynthEngineRef engine, Fixed volume);
OSErr GetSynthesisVolume(SynthEngineRef engine, Fixed *volume);
OSErr SetSynthesisQuality(SynthEngineRef engine, SynthQuality quality);
OSErr GetSynthesisQuality(SynthEngineRef engine, SynthQuality *quality);

/* Audio format control */
OSErr SetSynthesisAudioFormat(SynthEngineRef engine, const AudioFormatDescriptor *format);
OSErr GetSynthesisAudioFormat(SynthEngineRef engine, AudioFormatDescriptor *format);
OSErr GetSupportedAudioFormats(SynthEngineRef engine, AudioFormatDescriptor **formats, short *count);

/* ===== Synthesis Callbacks ===== */

/* Synthesis progress callback */
typedef void (*SynthesisProgressProc)(SynthEngineRef engine, const SynthesisProgress *progress, void *userData);

/* Synthesis completion callback */
typedef void (*SynthesisCompletionProc)(SynthEngineRef engine, const SynthesisResult *result, void *userData);

/* Synthesis error callback */
typedef void (*SynthesisErrorProc)(SynthEngineRef engine, OSErr error, const char *errorMessage, void *userData);

/* Audio output callback */
typedef void (*SynthesisAudioProc)(SynthEngineRef engine, const void *audioData, long audioSize,
                                    const AudioFormatDescriptor *format, void *userData);

/* Callback registration */
OSErr SetSynthesisProgressCallback(SynthEngineRef engine, SynthesisProgressProc callback, void *userData);
OSErr SetSynthesisCompletionCallback(SynthEngineRef engine, SynthesisCompletionProc callback, void *userData);
OSErr SetSynthesisErrorCallback(SynthEngineRef engine, SynthesisErrorProc callback, void *userData);
OSErr SetSynthesisAudioCallback(SynthEngineRef engine, SynthesisAudioProc callback, void *userData);

/* ===== Voice Engine Integration ===== */

/* Voice loading for engines */
OSErr LoadVoiceForEngine(SynthEngineRef engine, const VoiceSpec *voice);
OSErr UnloadVoiceFromEngine(SynthEngineRef engine, const VoiceSpec *voice);
OSErr SetEngineVoice(SynthEngineRef engine, const VoiceSpec *voice);
OSErr GetEngineVoice(SynthEngineRef engine, VoiceSpec *voice);

/* Voice compatibility */
OSErr CheckVoiceEngineCompatibility(const VoiceSpec *voice, SynthEngineRef engine, bool *compatible);
OSErr GetCompatibleEnginesForVoice(const VoiceSpec *voice, SynthEngineRef **engines, short *count);

/* ===== Engine-Specific Features ===== */

/* Engine configuration */
OSErr SetEngineProperty(SynthEngineRef engine, OSType property, const void *value, long valueSize);
OSErr GetEngineProperty(SynthEngineRef engine, OSType property, void *value, long *valueSize);
OSErr GetEnginePropertyInfo(SynthEngineRef engine, OSType property, OSType *dataType, long *dataSize);

/* Engine capabilities testing */
bool EngineSupportsCapability(SynthEngineRef engine, SynthEngineCapabilities capability);
OSErr GetEngineCapabilities(SynthEngineRef engine, SynthEngineCapabilities *capabilities);

/* Engine statistics */
OSErr GetEngineStatistics(SynthEngineRef engine, long *totalSyntheses, long *totalBytes,
                          long *averageSpeed, long *errorCount);
OSErr ResetEngineStatistics(SynthEngineRef engine);

/* ===== Advanced Synthesis Features ===== */

/* Emotional synthesis */
typedef enum {
    kEmotion_Neutral    = 0,
    kEmotion_Happy      = 1,
    kEmotion_Sad        = 2,
    kEmotion_Angry      = 3,
    kEmotion_Excited    = 4,
    kEmotion_Calm       = 5,
    kEmotion_Whisper    = 6,
    kEmotion_Shout      = 7
} EmotionalState;

OSErr SetSynthesisEmotion(SynthEngineRef engine, EmotionalState emotion, Fixed intensity);
OSErr GetSynthesisEmotion(SynthEngineRef engine, EmotionalState *emotion, Fixed *intensity);

/* Multi-voice synthesis */
OSErr BeginMultiVoiceSynthesis(SynthEngineRef engine);
OSErr AddVoiceToSynthesis(SynthEngineRef engine, const VoiceSpec *voice, const char *text, long textLength);
OSErr EndMultiVoiceSynthesis(SynthEngineRef engine, SynthesisResult **result);

/* Synthesis caching */
OSErr EnableSynthesisCache(SynthEngineRef engine, bool enable);
OSErr ClearSynthesisCache(SynthEngineRef engine);
OSErr GetCacheStatistics(SynthEngineRef engine, long *cacheHits, long *cacheMisses, long *cacheSize);

/* ===== Platform Integration ===== */

/* Platform-specific engine support */
#ifdef _WIN32
OSErr InitializeSAPIEngine(SynthEngineRef *engine);
OSErr ConfigureSAPIEngine(SynthEngineRef engine, void *sapiConfig);
#endif

#ifdef __APPLE__
OSErr InitializeAVSpeechEngine(SynthEngineRef *engine);
OSErr ConfigureAVSpeechEngine(SynthEngineRef engine, void *avConfig);
#endif

#ifdef __linux__
OSErr InitializeESpeakEngine(SynthEngineRef *engine);
OSErr InitializeFestivalEngine(SynthEngineRef *engine);
OSErr ConfigureLinuxEngine(SynthEngineRef engine, const char *configFile);
#endif

/* Network engines */
OSErr InitializeCloudEngine(SynthEngineRef *engine, const char *apiKey, const char *endpoint);
OSErr SetCloudEngineCredentials(SynthEngineRef engine, const char *apiKey, const char *secret);

#ifdef __cplusplus
}
#endif

#endif /* _SPEECHSYNTHESIS_H_ */