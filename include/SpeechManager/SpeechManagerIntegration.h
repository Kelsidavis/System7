/*
 * File: SpeechManagerIntegration.h
 *
 * Contains: Complete Speech Manager integration header for System 7.1 Portable
 *
 * Written by: Claude Code (Portable Implementation)
 *
 *
 * Description: This header provides complete Speech Manager integration
 *              including all components and modern speech synthesis support.
 */

#ifndef _SPEECHMANAGERINTEGRATION_H_
#define _SPEECHMANAGERINTEGRATION_H_

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

/* Include all Speech Manager components */
#include "SpeechManager.h"
#include "VoiceManager.h"
#include "TextToSpeech.h"
#include "SpeechSynthesis.h"
#include "VoiceResources.h"
#include "SpeechChannels.h"
#include "PronunciationEngine.h"
#include "SpeechOutput.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Speech Manager Integration ===== */

/*
 * Speech Manager Complete API
 *
 * This implementation provides a complete, portable implementation of the
 * Macintosh Speech Manager for System 7.1, with modern enhancements for
 * cross-platform accessibility and speech synthesis.
 *
 * Key Features:
 * - Complete Mac OS Speech Manager API compatibility
 * - Modern speech synthesis engine integration
 * - Cross-platform audio output support
 * - Advanced text processing and phoneme conversion
 * - Voice resource management and caching
 * - Real-time speech channel management
 * - Accessibility framework integration
 * - Neural text-to-speech support
 * - Multi-language speech synthesis
 * - Cloud-based speech services integration
 */

/* ===== Integration Constants ===== */

/* Speech Manager version */
#define kSpeechManagerIntegrationVersion 0x01008000  /* Version 1.0.8 */

/* Integration feature flags */

/* ===== Quick Start Functions ===== */

/*
 * Quick initialization for common use cases
 */

/* Simple speech synthesis */
OSErr SpeechQuickStart(void);
OSErr SpeechQuickSpeak(const char *text);
OSErr SpeechQuickSpeakWithVoice(const char *text, const char *voiceName);
OSErr SpeechQuickStop(void);
void SpeechQuickCleanup(void);

/* Voice selection helpers */
OSErr SpeechSelectBestVoice(const char *language, VoiceGender preferredGender, VoiceSpec *voice);
OSErr SpeechGetVoiceNames(char ***voiceNames, short *voiceCount);
OSErr SpeechSetDefaultVoiceByName(const char *voiceName);

/* Text processing helpers */
OSErr SpeechProcessText(const char *inputText, char **processedText);
OSErr SpeechConvertToPhonemes(const char *text, char **phonemes);
OSErr SpeechEstimateDuration(const char *text, long *durationMs);

/* ===== Integration Configuration ===== */

/* Speech Manager configuration */

/* Configuration management */
OSErr CreateSpeechManagerConfiguration(SpeechManagerConfiguration **config);
OSErr DisposeSpeechManagerConfiguration(SpeechManagerConfiguration *config);
OSErr LoadSpeechManagerConfiguration(const char *configFile, SpeechManagerConfiguration **config);
OSErr SaveSpeechManagerConfiguration(const SpeechManagerConfiguration *config, const char *configFile);

/* Apply configuration */
OSErr ApplySpeechManagerConfiguration(const SpeechManagerConfiguration *config);
OSErr GetCurrentSpeechManagerConfiguration(SpeechManagerConfiguration *config);
OSErr ResetSpeechManagerConfiguration(void);

/* ===== Platform Integration ===== */

/* Platform-specific initialization */
#ifdef PLATFORM_REMOVED_WIN32
OSErr InitializeSpeechManagerWindows(void);
OSErr ConfigureSAPIIntegration(Boolean enable);
OSErr ConfigureWindowsSpeechPlatform(void);
#endif

#ifdef PLATFORM_REMOVED_APPLE
OSErr InitializeSpeechManagerMacOS(void);
OSErr ConfigureAVSpeechIntegration(Boolean enable);
OSErr ConfigureAccessibilityIntegration(Boolean enable);
#endif

#ifdef PLATFORM_REMOVED_LINUX
OSErr InitializeSpeechManagerLinux(void);
OSErr ConfigureESpeakIntegration(Boolean enable);
OSErr ConfigureFestivalIntegration(Boolean enable);
OSErr ConfigureSpeechDispatcherIntegration(Boolean enable);
#endif

/* Cross-platform features */
OSErr EnableCloudSpeechServices(const char *apiKey, const char *serviceProvider);
OSErr ConfigureNeuralTTSEngine(const char *modelPath);
OSErr SetupAccessibilitySupport(Boolean enableScreenReader, Boolean enableVoiceOver);

/* ===== Advanced Features ===== */

/* SSML support */
OSErr SpeechSynthesizeSSML(const char *ssmlText, SpeechChannel chan);
OSErr SpeechValidateSSML(const char *ssmlText, Boolean *isValid, char **errorMessage);
OSErr SpeechConvertTextToSSML(const char *plainText, char **ssmlOutput);

/* Multi-voice synthesis */
OSErr SpeechBeginMultiVoiceSession(void);
OSErr SpeechAddVoiceToSession(const VoiceSpec *voice, const char *text);
OSErr SpeechEndMultiVoiceSession(void);

/* Real-time speech modification */
OSErr SpeechSetRealtimeParameters(SpeechChannel chan, Fixed rate, Fixed pitch, Fixed volume);
OSErr SpeechApplyRealtimeEffects(SpeechChannel chan, OSType effectType, void *effectParams);

/* Speech analysis */
OSErr SpeechAnalyzeText(const char *text, long *wordCount, long *sentenceCount,
                        long *estimatedDuration, short *complexity);
OSErr SpeechGetProsodicAnalysis(const char *text, void **prosodicData);

/* ===== Accessibility Integration ===== */

/* Screen reader support */
OSErr SpeechRegisterScreenReaderCallbacks(void *callbackStruct);
OSErr SpeechAnnounceSystemEvent(const char *eventDescription, short priority);
OSErr SpeechSpeakUIElement(const char *elementType, const char *elementText, const char *context);

/* Voice-over support */
OSErr SpeechEnableVoiceOverMode(Boolean enable);
OSErr SpeechSetVoiceOverParameters(Fixed rate, Fixed pitch, Boolean enableSounds);
OSErr SpeechSpeakWithVoiceOverNavigation(const char *text, const char *navigationHint);

/* Accessibility preferences */
OSErr SpeechLoadAccessibilityPreferences(void);
OSErr SpeechSaveAccessibilityPreferences(void);
OSErr SpeechGetAccessibilityStatus(Boolean *isEnabled, char **currentVoice, Fixed *currentRate);

/* ===== Callback Integration ===== */

/* Unified callback structure */

/* Callback management */
OSErr SpeechSetUnifiedCallbacks(SpeechChannel chan, const SpeechCallbacks *callbacks);
OSErr SpeechGetUnifiedCallbacks(SpeechChannel chan, SpeechCallbacks *callbacks);
OSErr SpeechClearAllCallbacks(SpeechChannel chan);

/* Global event callbacks */

OSErr SpeechRegisterSystemEventCallback(SpeechSystemEventProc callback, void *userData);

/* ===== Performance and Monitoring ===== */

/* Performance monitoring */

OSErr SpeechGetPerformanceMetrics(SpeechPerformanceMetrics *metrics);
OSErr SpeechResetPerformanceCounters(void);
OSErr SpeechEnablePerformanceLogging(Boolean enable, const char *logFile);

/* Resource monitoring */
OSErr SpeechGetResourceUsage(long *voiceMemoryMB, long *audioBufferMB, long *cacheMemoryMB);
OSErr SpeechOptimizeResourceUsage(void);
OSErr SpeechSetResourceLimits(long maxVoiceMemoryMB, long maxCacheMemoryMB);

/* ===== Error Handling and Debugging ===== */

/* Enhanced error handling */

OSErr SpeechGetLastError(SpeechErrorDetails *errorDetails);
OSErr SpeechClearErrorHistory(void);
OSErr SpeechSetErrorHandler(SpeechErrorProcPtr errorHandler, void *userData);

/* Debug support */
OSErr SpeechEnableDebugMode(Boolean enable);
OSErr SpeechSetDebugLevel(short debugLevel);
OSErr SpeechDumpSystemState(const char *outputFile);
OSErr SpeechValidateSystemIntegrity(Boolean *isValid, char **issues);

/* ===== Utility Functions ===== */

/* System information */
OSErr SpeechGetSystemInfo(char **platformName, char **speechEngines, long *capabilities);
OSErr SpeechGetVersionInfo(long *majorVersion, long *minorVersion, char **buildInfo);
OSErr SpeechGetSupportedLanguages(short **languageCodes, char ***languageNames, short *languageCount);

/* File format support */
OSErr SpeechSaveToAudioFile(SpeechChannel chan, const char *text, const char *filePath,
                            AudioFileFormat format);
OSErr SpeechLoadVoiceFromFile(const char *voiceFile, VoiceSpec *voice);
OSErr SpeechExportVoiceSettings(const VoiceSpec *voice, const char *settingsFile);

/* Text utilities */
OSErr SpeechValidateTextForSpeech(const char *text, Boolean *isValid, char **issues);
OSErr SpeechEstimateFileSize(const char *text, AudioFileFormat format, long *estimatedBytes);
OSErr SpeechCalculateChecksum(const char *text, const VoiceSpec *voice, long *checksum);

/* ===== Migration and Compatibility ===== */

/* Legacy compatibility */
OSErr SpeechEnableLegacyMode(Boolean enable);
OSErr SpeechConvertLegacyVoices(const char *legacyVoiceDir, const char *modernVoiceDir);
OSErr SpeechImportMacOSVoices(const char *systemVoiceDir);

/* Modern feature migration */
OSErr SpeechUpgradeToModernAPI(Boolean preserveCompatibility);
OSErr SpeechEnableNeuralFeatures(Boolean enable);
OSErr SpeechMigrateToCloudServices(const char *serviceConfig);

/* ===== Constants for Integration ===== */

/* Predefined configurations */
extern const SpeechManagerConfiguration kSpeechConfig_Default;
extern const SpeechManagerConfiguration kSpeechConfig_HighQuality;
extern const SpeechManagerConfiguration kSpeechConfig_LowLatency;
extern const SpeechManagerConfiguration kSpeechConfig_Accessibility;
extern const SpeechManagerConfiguration kSpeechConfig_MultiLanguage;

/* Common voice specifications */
extern const VoiceSpec kVoice_DefaultSystem;
extern const VoiceSpec kVoice_HighQualityMale;
extern const VoiceSpec kVoice_HighQualityFemale;
extern const VoiceSpec kVoice_AccessibilityOptimized;

/* Language codes */
#define kLanguage_English_US    0
#define kLanguage_English_UK    1
#define kLanguage_French        2
#define kLanguage_German        3
#define kLanguage_Spanish       4
#define kLanguage_Italian       5
#define kLanguage_Japanese      6
#define kLanguage_Chinese       7

#ifdef __cplusplus
}
#endif

#endif /* _SPEECHMANAGERINTEGRATION_H_ */