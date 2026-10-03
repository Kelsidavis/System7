/*
 * File: SpeechSynthesis.h
 *
 * Contains: Speech synthesis engine integration for Speech Manager
 *
 *
 * Description: This header provides speech synthesis engine functionality
 *              including engine management, audio synthesis, and output control.
 */

#ifndef _SPEECHSYNTHESIS_H_
#define _SPEECHSYNTHESIS_H_

#include "SystemTypes.h"

#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Synthesis Engine Constants ===== */

/* Synthesis engine types */
#define kSynthEngineTypeFestival 0
#define kSynthEngineTypeEspeak   1
#define kSynthEngineTypeVoiceXML 2

/* Synthesis quality levels */
#define kSynthQualityLow    0
#define kSynthQualityMed    1
#define kSynthQualityHigh   2

/* Synthesis engine capabilities */
#define kSynthCapPolyphonic   0x0001
#define kSynthCapStreaming    0x0002
#define kSynthCapRealtime     0x0004
#define kSynthCapPhonetic     0x0008

/* Audio format specifications */
#define kAudioFormatPCM       0
#define kAudioFormatMP3       1
#define kAudioFormatWAV       2

/* ===== Synthesis Engine Type Definitions ===== */

/* Opaque handles for synthesis objects */
typedef long SynthEngineRef;
typedef long SynthEngineType;
typedef long SynthEngineCapabilities;
typedef long SynthesisState;
typedef long SynthesisProgress;
typedef short SynthQuality;
typedef long AudioFormatDescriptor;
typedef long EmotionalState;

/* Forward declarations for opaque structures */
typedef struct SynthEngineInfoStruct *SynthEngineInfo;
typedef struct SynthesisParametersStruct *SynthesisParameters;
typedef struct SynthesisResultStruct *SynthesisResult;

/* Callback types */
typedef void (*SynthesisProgressProc)(SynthEngineRef engine, long bytesProcessed, long totalBytes, void *userData);
typedef void (*SynthesisCompletionProc)(SynthEngineRef engine, void *result, void *userData);
typedef void (*SynthesisErrorProc)(SynthEngineRef engine, OSErr error, void *userData);
typedef void (*SynthesisAudioProc)(SynthEngineRef engine, const void *audioData, long audioLength, void *userData);

/* ===== Synthesis Engine Management ===== */

/* Engine initialization and cleanup */
OSErr InitializeSpeechSynthesis(void);
void CleanupSpeechSynthesis(void);


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


/* Engine statistics */
OSErr GetEngineStatistics(SynthEngineRef engine, long *totalSyntheses, long *totalBytes,
                          long *averageSpeed, long *errorCount);

/* ===== Platform Integration ===== */

/* Platform-specific engine support */
#ifdef PLATFORM_REMOVED_WIN32
#endif

#ifdef PLATFORM_REMOVED_APPLE
#endif

#ifdef PLATFORM_REMOVED_LINUX
#endif


#ifdef __cplusplus
}
#endif

#endif /* _SPEECHSYNTHESIS_H_ */
