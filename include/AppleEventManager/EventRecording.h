/*
 * EventRecording.h
 *
 * Event recording and playback for automation
 * Provides comprehensive Apple Event recording, scripting, and automation capabilities
 *
 * Based on Mac OS 7.1 recording architecture with modern automation extensions
 */

#ifndef EVENT_RECORDING_H
#define EVENT_RECORDING_H

#include "AppleEventManager.h"
#include "AppleScript.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Recording Types and Constants
 * ======================================================================== */

/* Recording session handle */
typedef void* AERecordingSession;

/* Recording modes */
typedef enum {
    kAERecordUserActions = 0x00000001,
    kAERecordSystemEvents = 0x00000002,
    kAERecordApplicationEvents = 0x00000004,
    kAERecordMenuSelections = 0x00000008,
    kAERecordKeystrokes = 0x00000010,
    kAERecordMouseClicks = 0x00000020,
    kAERecordFileOperations = 0x00000040,
    kAERecordNetworkEvents = 0x00000080,
    kAERecordAllEvents = 0xFFFFFFFF
} AERecordingMode;

/* Recording filters */
typedef enum {
    kAEFilterNone = 0,
    kAEFilterByApplication = 1,
    kAEFilterByEventClass = 2,
    kAEFilterByTime = 3,
    kAEFilterByUserAction = 4
} AERecordingFilter;

/* Playback modes */
typedef enum {
    kAEPlaybackNormal = 0,
    kAEPlaybackFast = 1,
    kAEPlaybackSlow = 2,
    kAEPlaybackStep = 3,
    kAEPlaybackDebug = 4
} AEPlaybackMode;

/* Recording event types */
typedef enum {
    kAERecordedAppleEvent = 1,
    kAERecordedUserInterface = 2,
    kAERecordedSystemCall = 3,
    kAERecordedFileSystem = 4,
    kAERecordedNetwork = 5,
    kAERecordedCustom = 6
} AERecordedEventType;

/* ========================================================================
 * Recording Session Management
 * ======================================================================== */

/* Recording session creation and control */
OSErr AECreateRecordingSession(AERecordingSession* session);
OSErr AEDisposeRecordingSession(AERecordingSession session);

/* Recording control */
OSErr AEStartRecording(AERecordingSession session, AERecordingMode mode);
OSErr AEStopRecording(AERecordingSession session);
OSErr AEPauseRecording(AERecordingSession session);
OSErr AEResumeRecording(AERecordingSession session);

/* Recording state */
bool AEIsRecording(AERecordingSession session);
bool AEIsRecordingPaused(AERecordingSession session);
OSErr AEGetRecordingMode(AERecordingSession session, AERecordingMode* mode);
OSErr AESetRecordingMode(AERecordingSession session, AERecordingMode mode);

/* ========================================================================
 * Recording Configuration
 * ======================================================================== */

/* Recording options */
typedef struct AERecordingOptions {
    AERecordingMode recordingMode;
    AERecordingFilter filterType;
    void* filterData;
    bool includeTimestamps;
    bool includeUserContext;
    bool compressEvents;
    int32_t maxRecordingSize;
    int32_t maxRecordingTime;
    char* outputFormat;
} AERecordingOptions;

/* Configuration functions */
OSErr AESetRecordingOptions(AERecordingSession session, const AERecordingOptions* options);
OSErr AEGetRecordingOptions(AERecordingSession session, AERecordingOptions* options);

/* Filtering */
OSErr AESetRecordingFilter(AERecordingSession session, AERecordingFilter filterType, const void* filterData, Size filterSize);
OSErr AERemoveRecordingFilter(AERecordingSession session);

/* Application-specific recording */
OSErr AERecordOnlyApplication(AERecordingSession session, const ProcessSerialNumber* psn);
OSErr AERecordAllApplications(AERecordingSession session);
OSErr AEExcludeApplication(AERecordingSession session, const ProcessSerialNumber* psn);

/* ========================================================================
 * Event Recording Functions
 * ======================================================================== */

/* Manual event recording */
OSErr AERecordEvent(AERecordingSession session, const AppleEvent* theEvent, AERecordedEventType eventType);
OSErr AERecordUserAction(AERecordingSession session, const char* actionDescription, const void* actionData, Size dataSize);
OSErr AERecordSystemEvent(AERecordingSession session, const char* systemCall, const void* parameters, Size paramSize);

/* Automatic event interception */
OSErr AEInstallRecordingHook(AERecordingSession session);
OSErr AERemoveRecordingHook(AERecordingSession session);

/* Custom event recording */
typedef OSErr (*AERecordingCallbackProc)(AERecordingSession session, AERecordedEventType eventType, const void* eventData, Size eventSize, void* userData);
OSErr AESetRecordingCallback(AERecordingSession session, AERecordingCallbackProc callback, void* userData);

/* ========================================================================
 * Recording Storage and Retrieval
 * ======================================================================== */

/* Recording data structure */
typedef struct AERecordedEvent {
    AERecordedEventType eventType;
    int64_t timestamp;
    ProcessSerialNumber sourceProcess;
    ProcessSerialNumber targetProcess;
    AppleEvent event;
    char* description;
    void* customData;
    Size customDataSize;
} AERecordedEvent;

/* Recording access */
OSErr AEGetRecordingEventCount(AERecordingSession session, int32_t* eventCount);
OSErr AEGetRecordedEvent(AERecordingSession session, int32_t eventIndex, AERecordedEvent* recordedEvent);
OSErr AEGetRecordedEventRange(AERecordingSession session, int32_t startIndex, int32_t endIndex, AERecordedEvent** events, int32_t* eventCount);

/* Recording enumeration */
typedef bool (*AERecordedEventEnumProc)(const AERecordedEvent* recordedEvent, int32_t eventIndex, void* userData);
OSErr AEEnumerateRecordedEvents(AERecordingSession session, AERecordedEventEnumProc enumProc, void* userData);

/* Recording modification */
OSErr AEInsertRecordedEvent(AERecordingSession session, int32_t insertIndex, const AERecordedEvent* recordedEvent);
OSErr AEDeleteRecordedEvent(AERecordingSession session, int32_t eventIndex);
OSErr AEModifyRecordedEvent(AERecordingSession session, int32_t eventIndex, const AERecordedEvent* newEvent);

/* ========================================================================
 * Recording Persistence
 * ======================================================================== */

/* File formats */
#define kAERecordingFormatBinary "AEBinary"
#define kAERecordingFormatXML "AEXML"
#define kAERecordingFormatJSON "AEJSON"
#define kAERecordingFormatAppleScript "AppleScript"
#define kAERecordingFormatJavaScript "JavaScript"
#define kAERecordingFormatPython "Python"

/* Save and load */
OSErr AESaveRecording(AERecordingSession session, const char* filename, const char* format);
OSErr AELoadRecording(const char* filename, AERecordingSession* session);

/* Recording metadata */
typedef struct AERecordingMetadata {
    char* title;
    char* description;
    char* author;
    int64_t creationDate;
    int64_t modificationDate;
    char* version;
    char* targetApplication;
    char* requiredVersion;
    int32_t eventCount;
    int64_t totalDuration;
} AERecordingMetadata;

OSErr AESetRecordingMetadata(AERecordingSession session, const AERecordingMetadata* metadata);
OSErr AEGetRecordingMetadata(AERecordingSession session, AERecordingMetadata* metadata);

/* ========================================================================
 * Recording Playback
 * ======================================================================== */

/* Playback session */
typedef void* AEPlaybackSession;

/* Playback control */
OSErr AECreatePlaybackSession(AERecordingSession recording, AEPlaybackSession* playback);
OSErr AEDisposePlaybackSession(AEPlaybackSession playback);

OSErr AEStartPlayback(AEPlaybackSession playback, AEPlaybackMode mode);
OSErr AEStopPlayback(AEPlaybackSession playback);
OSErr AEPausePlayback(AEPlaybackSession playback);
OSErr AEResumePlayback(AEPlaybackSession playback);

/* Playback navigation */
OSErr AEPlaybackStepForward(AEPlaybackSession playback);
OSErr AEPlaybackStepBackward(AEPlaybackSession playback);
OSErr AEPlaybackGotoEvent(AEPlaybackSession playback, int32_t eventIndex);
OSErr AEPlaybackGotoTime(AEPlaybackSession playback, int64_t timestamp);

/* Playback state */
bool AEIsPlaybackActive(AEPlaybackSession playback);
bool AEIsPlaybackPaused(AEPlaybackSession playback);
OSErr AEGetPlaybackPosition(AEPlaybackSession playback, int32_t* currentEventIndex, int64_t* currentTime);
OSErr AEGetPlaybackMode(AEPlaybackSession playback, AEPlaybackMode* mode);

/* ========================================================================
 * Playback Options and Control
 * ======================================================================== */

/* Playback configuration */
typedef struct AEPlaybackOptions {
    AEPlaybackMode playbackMode;
    double speedMultiplier;
    bool respectTimestamps;
    bool interactiveMode;
    bool continueOnError;
    bool targetSpecificApplication;
    ProcessSerialNumber targetPSN;
    int32_t maxErrorCount;
} AEPlaybackOptions;

OSErr AESetPlaybackOptions(AEPlaybackSession playback, const AEPlaybackOptions* options);
OSErr AEGetPlaybackOptions(AEPlaybackSession playback, AEPlaybackOptions* options);

/* Playback callbacks */
typedef void (*AEPlaybackEventProc)(AEPlaybackSession playback, const AERecordedEvent* event, OSErr result, void* userData);
typedef void (*AEPlaybackErrorProc)(AEPlaybackSession playback, OSErr error, const AERecordedEvent* event, void* userData);
typedef void (*AEPlaybackCompletionProc)(AEPlaybackSession playback, OSErr finalResult, void* userData);

OSErr AESetPlaybackEventCallback(AEPlaybackSession playback, AEPlaybackEventProc callback, void* userData);
OSErr AESetPlaybackErrorCallback(AEPlaybackSession playback, AEPlaybackErrorProc callback, void* userData);
OSErr AESetPlaybackCompletionCallback(AEPlaybackSession playback, AEPlaybackCompletionProc callback, void* userData);

/* ========================================================================
 * Script Generation from Recordings
 * ======================================================================== */

/* Script generation options */
typedef struct AEScriptGenerationOptions {
    char* scriptLanguage;
    bool includeComments;
    bool optimizeForReadability;
    bool optimizeForPerformance;
    bool includeErrorHandling;
    bool includeVariables;
    char* indentationStyle;
    int32_t indentationSize;
} AEScriptGenerationOptions;

/* Script generation */
OSErr AEGenerateScriptFromRecording(AERecordingSession session, const AEScriptGenerationOptions* options, char** scriptText, Size* scriptSize);
OSErr AEGenerateAppleScriptFromRecording(AERecordingSession session, OSAScript* script);
OSErr AEGenerateJavaScriptFromRecording(AERecordingSession session, char** scriptText, Size* scriptSize);
OSErr AEGeneratePythonFromRecording(AERecordingSession session, char** scriptText, Size* scriptSize);

/* Script optimization */
OSErr AEOptimizeRecordingForScript(AERecordingSession session, const char* targetLanguage);
OSErr AESimplifyRecording(AERecordingSession session, bool removeRedundantEvents);

/* ========================================================================
 * Macro and Template System
 * ======================================================================== */

/* Macro creation */
typedef struct AEMacro {
    char* macroName;
    char* description;
    AERecordingSession recording;
    char* parameters;
    int32_t parameterCount;
} AEMacro;

OSErr AECreateMacroFromRecording(AERecordingSession session, const char* macroName, const char* description, AEMacro** macro);
OSErr AEDisposeMacro(AEMacro* macro);

/* Macro library */
OSErr AESaveMacro(const AEMacro* macro, const char* filename);
OSErr AELoadMacro(const char* filename, AEMacro** macro);
OSErr AEGetMacroLibrary(AEMacro*** macros, int32_t* macroCount);

/* Macro execution */
OSErr AEExecuteMacro(const AEMacro* macro, const char** parameters, int32_t parameterCount);
OSErr AEExecuteMacroByName(const char* macroName, const char** parameters, int32_t parameterCount);

/* ========================================================================
 * Advanced Recording Features
 * ======================================================================== */

/* Conditional recording */
typedef bool (*AERecordingConditionProc)(const AppleEvent* event, AERecordedEventType eventType, void* userData);
OSErr AESetRecordingCondition(AERecordingSession session, AERecordingConditionProc condition, void* userData);

/* Recording compression and optimization */
OSErr AECompressRecording(AERecordingSession session);
OSErr AEOptimizeRecording(AERecordingSession session, bool removeRedundantEvents, bool combineSequentialEvents);

/* Recording analysis */
typedef struct AERecordingAnalysis {
    int32_t totalEvents;
    int32_t uniqueEventTypes;
    int32_t totalDuration;
    int32_t averageEventDuration;
    int32_t applicationsInvolved;
    char** applicationList;
    Size recordingSize;
    Size compressedSize;
} AERecordingAnalysis;

OSErr AEAnalyzeRecording(AERecordingSession session, AERecordingAnalysis* analysis);

/* ========================================================================
 * Integration with AppleScript
 * ======================================================================== */

/* AppleScript recording integration */
OSErr AEStartAppleScriptRecording(OSAComponentInstance scriptingComponent, AERecordingSession* session);
OSErr AEStopAppleScriptRecording(AERecordingSession session, OSAScript* resultScript);

/* Script event interception */
OSErr AEInstallScriptRecordingHook(OSAComponentInstance scriptingComponent, AERecordingSession session);
OSErr AERemoveScriptRecordingHook(OSAComponentInstance scriptingComponent);

/* ========================================================================
 * Modern Automation Integration
 * ======================================================================== */

/* Workflow integration */
OSErr AEExportToAutomator(AERecordingSession session, const char* workflowPath);
OSErr AEImportFromAutomator(const char* workflowPath, AERecordingSession* session);

/* Shell script integration */
OSErr AEExportToShellScript(AERecordingSession session, const char* scriptPath, const char* shellType);
OSErr AEExecuteAsShellCommand(AERecordingSession session, char** command);

/* Task scheduler integration */
OSErr AEScheduleRecordingPlayback(AERecordingSession session, const char* schedule, const char* taskName);
OSErr AEUnscheduleRecordingPlayback(const char* taskName);

/* ========================================================================
 * Recording Statistics and Monitoring
 * ======================================================================== */

typedef struct AERecordingStats {
    int32_t totalRecordingSessions;
    int32_t activeRecordingSessions;
    int32_t totalEventsRecorded;
    int32_t totalPlaybackSessions;
    int32_t eventsPlayedBack;
    int32_t recordingErrors;
    int32_t playbackErrors;
    Size totalRecordingSize;
} AERecordingStats;

OSErr AEGetRecordingStats(AERecordingStats* stats);
void AEResetRecordingStats(void);

/* ========================================================================
 * Debug and Development Support
 * ======================================================================== */

#ifdef DEBUG
/* Recording debugging */
void AEPrintRecordedEvent(const AERecordedEvent* event, const char* label);
void AEPrintRecording(AERecordingSession session, const char* label);
OSErr AEValidateRecording(AERecordingSession session);

/* Playback debugging */
void AESetPlaybackDebugMode(AEPlaybackSession playback, bool enabled);
OSErr AEGetPlaybackTrace(AEPlaybackSession playback, char** traceOutput, Size* traceSize);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EVENT_RECORDING_H */