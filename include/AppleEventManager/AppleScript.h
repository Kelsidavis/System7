/*
 * AppleScript.h
 *
 * AppleScript integration and execution framework
 * Provides script compilation, execution, and integration with Apple Events
 *
 * Based on Mac OS 7.1 AppleScript support with modern extensions
 */

#ifndef APPLE_SCRIPT_H
#define APPLE_SCRIPT_H

#include "AppleEventManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * AppleScript Core Types and Constants
 * ======================================================================== */

/* Script types */
typedef void* OSAScript;
typedef void* OSAComponent;
typedef uint32_t OSAComponentInstance;

/* Script execution modes */
typedef enum {
    kOSAModeNull = 0x00000000,
    kOSAModePreventGetSource = 0x00000001,
    kOSAModeNeverInteract = 0x00000010,
    kOSAModeCanInteract = 0x00000020,
    kOSAModeAlwaysInteract = 0x00000030,
    kOSAModeCantSwitchLayer = 0x00000040,
    kOSAModeDontReconnect = 0x00000080,
    kOSAModeDoRecord = 0x00001000,
    kOSAModeCompileIntoContext = 0x00000002,
    kOSAModeAugmentContext = 0x00000004,
    kOSAModeDisplayForHumans = 0x00000008
} OSAExecutionMode;

/* Script states */
typedef enum {
    kOSAScriptNotCompiled = 0,
    kOSAScriptCompiled = 1,
    kOSAScriptExecuted = 2,
    kOSAScriptError = 3
} OSAScriptState;

/* Script error information */
typedef struct OSAScriptError {
    OSErr errorNumber;
    char* errorMessage;
    int32_t errorLine;
    int32_t errorColumn;
    char* errorRange;
} OSAScriptError;

/* ========================================================================
 * AppleScript Engine Management
 * ======================================================================== */

/* Engine initialization and cleanup */
OSErr OSAInit(void);
void OSACleanup(void);

/* Component management */
OSErr OSAOpenDefaultComponent(OSAComponentInstance* scriptingComponent);
OSErr OSAOpenComponent(OSAComponent component, OSAComponentInstance* scriptingComponent);
OSErr OSACloseComponent(OSAComponentInstance scriptingComponent);

/* Component information */
OSErr OSAGetComponentInfo(OSAComponentInstance scriptingComponent, char** componentName, char** componentVersion);
OSErr OSAGetComponentCapabilities(OSAComponentInstance scriptingComponent, uint32_t* capabilities);

/* ========================================================================
 * Script Compilation Functions
 * ======================================================================== */

/* Basic compilation */
OSErr OSACompile(OSAComponentInstance scriptingComponent, const char* sourceText, OSAExecutionMode modeFlags, OSAScript* resultScript);
OSErr OSACompileExecute(OSAComponentInstance scriptingComponent, const char* sourceText, OSAScript contextScript, OSAExecutionMode modeFlags, AEDesc* result);

/* Compilation with source descriptors */
OSErr OSACompileFromDesc(OSAComponentInstance scriptingComponent, const AEDesc* sourceDesc, OSAExecutionMode modeFlags, OSAScript* resultScript);
OSErr OSACompileFromFile(OSAComponentInstance scriptingComponent, const char* filename, OSAExecutionMode modeFlags, OSAScript* resultScript);

/* Compilation validation */
OSErr OSAValidateScript(OSAComponentInstance scriptingComponent, OSAScript script, bool* isValid);
OSErr OSAGetScriptState(OSAScript script, OSAScriptState* state);

/* ========================================================================
 * Script Execution Functions
 * ======================================================================== */

/* Basic execution */
OSErr OSAExecute(OSAComponentInstance scriptingComponent, OSAScript script, OSAScript contextScript, OSAExecutionMode modeFlags, AEDesc* result);
OSErr OSAExecuteEvent(OSAComponentInstance scriptingComponent, const AppleEvent* theAppleEvent, OSAScript contextScript, OSAExecutionMode modeFlags, AppleEvent* reply);

/* Execution control */
OSErr OSAStop(OSAComponentInstance scriptingComponent);
OSErr OSAStopScript(OSAScript script);
OSErr OSAResumeScript(OSAScript script);

/* Script context management */
OSErr OSACreateContext(OSAComponentInstance scriptingComponent, OSAScript* contextScript);
OSErr OSASetDefaultContext(OSAComponentInstance scriptingComponent, OSAScript contextScript);
OSErr OSAGetDefaultContext(OSAComponentInstance scriptingComponent, OSAScript* contextScript);

/* ========================================================================
 * Script Source and Storage Functions
 * ======================================================================== */

/* Source code management */
OSErr OSAGetSource(OSAComponentInstance scriptingComponent, OSAScript script, DescType desiredType, AEDesc* resultSource);
OSErr OSAGetSourceText(OSAScript script, char** sourceText, Size* sourceSize);

/* Script storage */
OSErr OSAStore(OSAComponentInstance scriptingComponent, OSAScript script, DescType desiredType, OSAExecutionMode modeFlags, AEDesc* resultData);
OSErr OSALoad(OSAComponentInstance scriptingComponent, const AEDesc* scriptData, OSAExecutionMode modeFlags, OSAScript* resultScript);

/* File operations */
OSErr OSASaveScriptToFile(OSAScript script, const char* filename);
OSErr OSALoadScriptFromFile(OSAComponentInstance scriptingComponent, const char* filename, OSAScript* resultScript);

/* ========================================================================
 * Script Variable and Property Management
 * ======================================================================== */

/* Variable operations */
OSErr OSAGetVariable(OSAScript script, const char* variableName, AEDesc* result);
OSErr OSASetVariable(OSAScript script, const char* variableName, const AEDesc* value);
OSErr OSAGetVariableNames(OSAScript script, char*** variableNames, int32_t* count);

/* Property operations */
OSErr OSAGetProperty(OSAScript script, const char* propertyName, AEDesc* result);
OSErr OSASetProperty(OSAScript script, const char* propertyName, const AEDesc* value);
OSErr OSAGetPropertyNames(OSAScript script, char*** propertyNames, int32_t* count);

/* Handler operations */
OSErr OSAGetHandlerNames(OSAScript script, char*** handlerNames, int32_t* count);
OSErr OSACallHandler(OSAScript script, const char* handlerName, const AEDescList* parameters, AEDesc* result);

/* ========================================================================
 * Script Error Handling
 * ======================================================================== */

/* Error information */
OSErr OSAScriptError(OSAComponentInstance scriptingComponent, DescType selector, AEDesc* result);
OSErr OSAGetScriptError(OSAScript script, OSAScriptError* errorInfo);
void OSADisposeScriptError(OSAScriptError* errorInfo);

/* Error formatting */
OSErr OSAFormatErrorMessage(const OSAScriptError* errorInfo, char** formattedMessage);
OSErr OSAGetErrorRange(const OSAScriptError* errorInfo, int32_t* startPos, int32_t* endPos);

/* ========================================================================
 * Script Debugging Support
 * ======================================================================== */

/* Debugging capabilities */
typedef enum {
    kOSADebugStepInto = 1,
    kOSADebugStepOver = 2,
    kOSADebugStepOut = 3,
    kOSADebugContinue = 4,
    kOSADebugStop = 5
} OSADebugCommand;

/* Breakpoint management */
OSErr OSASetBreakpoint(OSAScript script, int32_t lineNumber, bool enabled);
OSErr OSARemoveBreakpoint(OSAScript script, int32_t lineNumber);
OSErr OSAGetBreakpoints(OSAScript script, int32_t** lineNumbers, int32_t* count);

/* Debug execution */
OSErr OSADebugExecute(OSAComponentInstance scriptingComponent, OSAScript script, OSADebugCommand command, AEDesc* result);
OSErr OSAGetCallStack(OSAScript script, char*** callStack, int32_t* depth);

/* Variable inspection during debugging */
OSErr OSAGetLocalVariables(OSAScript script, int32_t stackLevel, char*** variableNames, AEDesc** values, int32_t* count);

/* ========================================================================
 * Script Recording and Playback
 * ======================================================================== */

/* Recording capabilities */
OSErr OSAStartRecording(OSAComponentInstance scriptingComponent, OSAScript* recordingScript);
OSErr OSAStopRecording(OSAComponentInstance scriptingComponent, OSAScript recordingScript);
OSErr OSAIsRecording(OSAComponentInstance scriptingComponent, bool* isRecording);

/* Recording options */
typedef enum {
    kOSARecordAll = 0x00000001,
    kOSARecordUserActions = 0x00000002,
    kOSARecordSystemEvents = 0x00000004,
    kOSARecordNetworkEvents = 0x00000008
} OSARecordingOptions;

OSErr OSASetRecordingOptions(OSAComponentInstance scriptingComponent, OSARecordingOptions options);
OSErr OSAGetRecordingOptions(OSAComponentInstance scriptingComponent, OSARecordingOptions* options);

/* ========================================================================
 * Script Library and Module Support
 * ======================================================================== */

/* Script libraries */
OSErr OSALoadLibrary(OSAComponentInstance scriptingComponent, const char* libraryPath, OSAScript* libraryScript);
OSErr OSAUnloadLibrary(OSAScript libraryScript);
OSErr OSAGetLoadedLibraries(OSAComponentInstance scriptingComponent, OSAScript** libraries, int32_t* count);

/* Module system */
OSErr OSAImportModule(OSAScript script, const char* moduleName, const char* modulePath);
OSErr OSAExportModule(OSAScript script, const char* moduleName, const char* outputPath);

/* ========================================================================
 * AppleScript Integration with Apple Events
 * ======================================================================== */

/* Event to script conversion */
OSErr OSADoEvent(OSAComponentInstance scriptingComponent, const AppleEvent* theAppleEvent, OSAScript contextScript, OSAExecutionMode modeFlags, AppleEvent* reply);
OSErr OSAMakeContext(OSAComponentInstance scriptingComponent, const AEDesc* contextName, OSAScript parentContext, OSAScript* resultContext);

/* Script-generated events */
OSErr OSAExecuteAsAppleEvent(OSAComponentInstance scriptingComponent, OSAScript script, const ProcessSerialNumber* target, AppleEvent* reply);

/* Event handler scripts */
OSErr OSAInstallEventHandler(OSAComponentInstance scriptingComponent, AEEventClass eventClass, AEEventID eventID, OSAScript handlerScript, int32_t refcon, bool isSysHandler);
OSErr OSARemoveEventHandler(AEEventClass eventClass, AEEventID eventID, OSAScript handlerScript, bool isSysHandler);

/* ========================================================================
 * Script Memory Management
 * ======================================================================== */

/* Script lifecycle */
OSErr OSADisposeScript(OSAScript script);
OSErr OSADuplicateScript(OSAScript sourceScript, OSAScript* destScript);

/* Memory monitoring */
typedef struct OSAMemoryInfo {
    Size totalMemoryUsed;
    Size scriptDataSize;
    Size contextDataSize;
    int32_t scriptCount;
    int32_t contextCount;
} OSAMemoryInfo;

OSErr OSAGetMemoryInfo(OSAComponentInstance scriptingComponent, OSAMemoryInfo* memInfo);

/* Garbage collection */
OSErr OSACollectGarbage(OSAComponentInstance scriptingComponent);
OSErr OSASetGarbageCollectionMode(OSAComponentInstance scriptingComponent, bool autoGC, int32_t gcThreshold);

/* ========================================================================
 * Advanced AppleScript Features
 * ======================================================================== */

/* Script optimization */
OSErr OSAOptimizeScript(OSAScript script, uint32_t optimizationLevel);
OSErr OSAGetOptimizationInfo(OSAScript script, uint32_t* optimizationLevel, bool* isOptimized);

/* Script analysis */
OSErr OSAAnalyzeScript(OSAScript script, uint32_t analysisOptions, AEDesc* analysisResult);

/* Internationalization */
OSErr OSASetScriptLocale(OSAScript script, const char* localeIdentifier);
OSErr OSAGetScriptLocale(OSAScript script, char** localeIdentifier);

#ifdef __cplusplus
}
#endif

#endif /* APPLE_SCRIPT_H */