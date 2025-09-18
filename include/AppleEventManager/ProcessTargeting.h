/*
 * ProcessTargeting.h
 *
 * Process identification and targeting for Apple Events
 * Provides process discovery, addressing, and inter-application communication
 *
 * Based on Mac OS 7.1 Process Manager integration with modern extensions
 */

#ifndef PROCESS_TARGETING_H
#define PROCESS_TARGETING_H

#include "AppleEventManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Process Identification Types and Constants
 * ======================================================================== */

/* Process information structure */
typedef struct ProcessInfo {
    ProcessSerialNumber processSerialNumber;
    char* processName;
    char* processSignature;
    int32_t processID;
    bool isBackgroundOnly;
    bool acceptsHighLevelEvents;
    bool hasScriptingTerminology;
    bool isRunning;
    Size memoryUsage;
    char* executablePath;
} ProcessInfo;

/* Application addressing modes */
typedef enum {
    kAEAddressByPSN = 0,
    kAEAddressBySignature = 1,
    kAEAddressByName = 2,
    kAEAddressByPath = 3,
    kAEAddressByPID = 4,
    kAEAddressByURL = 5
} AEAddressingMode;

/* Target process constants */
#define kNoProcess {0, 0}
#define kSystemProcess {0, 1}
#define kCurrentProcess {0, 2}

/* ========================================================================
 * Process Discovery and Enumeration
 * ======================================================================== */

/* Process enumeration callback */
typedef bool (*ProcessEnumProc)(const ProcessInfo* processInfo, void* userData);

/* Process discovery functions */
OSErr AEGetProcessList(ProcessInfo** processList, int32_t* processCount);
OSErr AEEnumerateProcesses(ProcessEnumProc enumProc, void* userData);
OSErr AEFindProcessByName(const char* processName, ProcessSerialNumber* psn);
OSErr AEFindProcessBySignature(const char* signature, ProcessSerialNumber* psn);
OSErr AEFindProcessByPath(const char* executablePath, ProcessSerialNumber* psn);
OSErr AEFindProcessByPID(int32_t processID, ProcessSerialNumber* psn);

/* Process information */
OSErr AEGetProcessInfo(const ProcessSerialNumber* psn, ProcessInfo* processInfo);
OSErr AEGetCurrentProcess(ProcessSerialNumber* psn);
OSErr AEGetFrontProcess(ProcessSerialNumber* psn);
OSErr AESetFrontProcess(const ProcessSerialNumber* psn);

/* Process validation */
bool AEIsValidProcess(const ProcessSerialNumber* psn);
bool AEProcessExists(const ProcessSerialNumber* psn);
bool AEProcessAcceptsAppleEvents(const ProcessSerialNumber* psn);

/* ========================================================================
 * Address Descriptor Creation
 * ======================================================================== */

/* Basic address creation */
OSErr AECreateAddressDesc(AEAddressingMode mode, const void* addressData, Size dataSize, AEAddressDesc* result);
OSErr AECreateProcessAddressDesc(const ProcessSerialNumber* psn, AEAddressDesc* result);
OSErr AECreateApplicationAddressDesc(const char* signature, AEAddressDesc* result);
OSErr AECreateNameAddressDesc(const char* applicationName, AEAddressDesc* result);
OSErr AECreatePathAddressDesc(const char* applicationPath, AEAddressDesc* result);
OSErr AECreatePIDAddressDesc(int32_t processID, AEAddressDesc* result);

/* Network addressing */
OSErr AECreateMachineAddressDesc(const char* machineName, AEAddressDesc* result);
OSErr AECreateURLAddressDesc(const char* url, AEAddressDesc* result);

/* Address resolution */
OSErr AEResolveAddressDesc(const AEAddressDesc* addressDesc, ProcessSerialNumber* psn);
OSErr AEGetAddressInfo(const AEAddressDesc* addressDesc, AEAddressingMode* mode, void** addressData, Size* dataSize);

/* ========================================================================
 * Application Launch and Control
 * ======================================================================== */

/* Launch parameters */
typedef struct LaunchParams {
    const char* applicationPath;
    const char* signature;
    bool bringToFront;
    bool hideAfterLaunch;
    const char** documentPaths;
    int32_t documentCount;
    const char* workingDirectory;
    const char** environmentVariables;
    int32_t environmentCount;
} LaunchParams;

/* Application launching */
OSErr AELaunchApplication(const LaunchParams* launchParams, ProcessSerialNumber* launchedPSN);
OSErr AELaunchApplicationBySignature(const char* signature, ProcessSerialNumber* launchedPSN);
OSErr AELaunchApplicationByPath(const char* applicationPath, ProcessSerialNumber* launchedPSN);
OSErr AELaunchApplicationWithDocuments(const char* applicationPath, const char** documentPaths, int32_t documentCount, ProcessSerialNumber* launchedPSN);

/* Application termination */
OSErr AEQuitApplication(const ProcessSerialNumber* psn, bool forceful);
OSErr AEQuitAllApplications(bool askUser);

/* Application control */
OSErr AEBringApplicationToFront(const ProcessSerialNumber* psn);
OSErr AEHideApplication(const ProcessSerialNumber* psn);
OSErr AEShowApplication(const ProcessSerialNumber* psn);

/* ========================================================================
 * Inter-Process Communication
 * ======================================================================== */

/* IPC transport types */
typedef enum {
    kAETransportAppleEvents = 0,
    kAETransportPipes = 1,
    kAETransportSockets = 2,
    kAETransportDBus = 3,
    kAETransportXPC = 4
} AETransportType;

/* Transport configuration */
typedef struct AETransportConfig {
    AETransportType transportType;
    bool enableSecurity;
    bool enableEncryption;
    int32_t timeoutSeconds;
    Size maxMessageSize;
    void* transportSpecificData;
} AETransportConfig;

/* Transport management */
OSErr AESetTransportConfig(const AETransportConfig* config);
OSErr AEGetTransportConfig(AETransportConfig* config);

/* Message sending with transport selection */
OSErr AESendWithTransport(const AppleEvent* theAppleEvent, AppleEvent* reply, AESendMode sendMode, AESendPriority sendPriority, int32_t timeOutInTicks, AETransportType transport);

/* ========================================================================
 * Process Monitoring and Notification
 * ======================================================================== */

/* Process event types */
typedef enum {
    kAEProcessLaunched = 1,
    kAEProcessTerminated = 2,
    kAEProcessBecameFront = 3,
    kAEProcessResignedFront = 4,
    kAEProcessSuspended = 5,
    kAEProcessResumed = 6
} AEProcessEventType;

/* Process notification callback */
typedef void (*ProcessNotificationProc)(AEProcessEventType eventType, const ProcessSerialNumber* psn, void* userData);

/* Process monitoring */
OSErr AEInstallProcessNotification(ProcessNotificationProc notificationProc, void* userData);
OSErr AERemoveProcessNotification(ProcessNotificationProc notificationProc);

/* Process watching */
OSErr AEWatchProcess(const ProcessSerialNumber* psn, AEProcessEventType eventTypes);
OSErr AEUnwatchProcess(const ProcessSerialNumber* psn);
OSErr AEUnwatchAllProcesses(void);

/* ========================================================================
 * Security and Permissions
 * ======================================================================== */

/* Security context */
typedef struct AESecurityContext {
    bool requireAuthentication;
    bool allowUntrustedProcesses;
    char** trustedSignatures;
    int32_t trustedSignatureCount;
    char** blockedSignatures;
    int32_t blockedSignatureCount;
} AESecurityContext;

/* Security management */
OSErr AESetSecurityContext(const AESecurityContext* context);
OSErr AEGetSecurityContext(AESecurityContext* context);
OSErr AEValidateProcessAccess(const ProcessSerialNumber* psn, bool* isAllowed);

/* Permission checking */
bool AECanSendToProcess(const ProcessSerialNumber* psn);
bool AECanReceiveFromProcess(const ProcessSerialNumber* psn);
bool AECanLaunchApplication(const char* applicationPath);

/* ========================================================================
 * Remote Process Communication
 * ======================================================================== */

/* Machine addressing */
typedef struct MachineAddress {
    char* machineName;
    char* ipAddress;
    int32_t port;
    AETransportType transportType;
} MachineAddress;

/* Remote process discovery */
OSErr AEDiscoverRemoteProcesses(const MachineAddress* machine, ProcessInfo** processList, int32_t* processCount);
OSErr AECreateRemoteAddressDesc(const MachineAddress* machine, const ProcessSerialNumber* remotePSN, AEAddressDesc* result);

/* Network configuration */
OSErr AESetNetworkPort(int32_t port);
OSErr AEGetNetworkPort(int32_t* port);
OSErr AEEnableNetworkAccess(bool enable);
bool AEIsNetworkAccessEnabled(void);

/* ========================================================================
 * Process Manager Integration
 * ======================================================================== */

/* System integration */
OSErr AERegisterWithProcessManager(void);
OSErr AEUnregisterWithProcessManager(void);

/* Process events */
OSErr AEHandleProcessEvent(const void* processEvent);
OSErr AEPostProcessEvent(AEProcessEventType eventType, const ProcessSerialNumber* psn);

/* ========================================================================
 * Debugging and Diagnostics
 * ======================================================================== */

/* Process debugging */
typedef struct ProcessDiagnostics {
    ProcessSerialNumber psn;
    int32_t messagesSent;
    int32_t messagesReceived;
    int32_t messagesHandled;
    int32_t messagesFailed;
    int32_t averageResponseTime;
    bool isResponding;
    char* lastError;
} ProcessDiagnostics;

OSErr AEGetProcessDiagnostics(const ProcessSerialNumber* psn, ProcessDiagnostics* diagnostics);
OSErr AEResetProcessDiagnostics(const ProcessSerialNumber* psn);

/* System diagnostics */
OSErr AEDumpProcessTable(void);
OSErr AEValidateProcessTable(void);

#ifdef DEBUG
void AEPrintProcessInfo(const ProcessInfo* processInfo, const char* label);
void AEPrintAddressDesc(const AEAddressDesc* addressDesc, const char* label);
#endif

/* ========================================================================
 * Modern Platform Integration
 * ======================================================================== */

#ifdef __linux__
/* D-Bus integration */
OSErr AECreateDBusAddressDesc(const char* serviceName, const char* objectPath, AEAddressDesc* result);
OSErr AESendViaDBus(const AppleEvent* theAppleEvent, const char* serviceName, const char* objectPath, AppleEvent* reply);
#endif

#ifdef __APPLE__
/* XPC integration */
OSErr AECreateXPCAddressDesc(const char* serviceName, AEAddressDesc* result);
OSErr AESendViaXPC(const AppleEvent* theAppleEvent, const char* serviceName, AppleEvent* reply);
#endif

#ifdef _WIN32
/* COM integration */
OSErr AECreateCOMAddressDesc(const char* progID, AEAddressDesc* result);
OSErr AESendViaCOM(const AppleEvent* theAppleEvent, const char* progID, AppleEvent* reply);
#endif

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/* PSN utilities */
bool AEComparePSN(const ProcessSerialNumber* psn1, const ProcessSerialNumber* psn2);
void AECopyPSN(const ProcessSerialNumber* source, ProcessSerialNumber* dest);
bool AEIsNullPSN(const ProcessSerialNumber* psn);

/* Process name utilities */
OSErr AEGetProcessName(const ProcessSerialNumber* psn, char** processName);
OSErr AEGetProcessSignature(const ProcessSerialNumber* psn, char** signature);
OSErr AEGetProcessPath(const ProcessSerialNumber* psn, char** executablePath);

/* Memory management */
void AEDisposeProcessInfo(ProcessInfo* processInfo);
void AEDisposeProcessList(ProcessInfo* processList, int32_t processCount);

#ifdef __cplusplus
}
#endif

#endif /* PROCESS_TARGETING_H */