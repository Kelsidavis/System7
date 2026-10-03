/* Process Manager interfaces and shared process data structures. */

#ifndef __PROCESSMGR_H__
#define __PROCESSMGR_H__

#include "SystemTypes.h"
#include "ProcessMgr/ProcessTypes.h"
#include "EventManager/EventTypes.h"
#include "FileManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Process Manager constants. */
#define kPM_MaxProcesses        32
#define kPM_InvalidProcessID    0xFFFFFFFF
#define kPM_SystemProcessID     0x00000001
#define kPM_FinderProcessID     0x00000002

/*
 * AppFile structure for GetAppFiles/CountAppFiles
 * Used to pass file information to applications at launch
 */
typedef struct AppFile {
    SInt16 vRefNum;     /* Volume reference number */
    OSType fType;       /* File type */
    SInt16 versNum;     /* Version number (unused in System 7) */
    Str255 fName;       /* Filename */
} AppFile;

/* Process states for cooperative scheduling. */
typedef enum {
    kProcessTerminated = 0,
    kProcessSuspended = 1,
    kProcessRunning = 2,
    kProcessBackground = 3
} ProcessState;

/* Process mode flags. */
enum {
    kProcessModeCooperative = 0x0001,
    kProcessModeCanBackground = 0x0002,
    kProcessModeNeedsActivate = 0x0004
};

/* Launch control flags. */
enum {
    kLaunchDontSwitch = 0x0001,
    kLaunchNoFileFlags = 0x0002,
    kLaunchContinue = 0x0004
};

/* Minimal context saved by the current process-switching implementation. */
typedef struct ProcessContext {
    UInt32 savedA5;
    UInt32 savedStackPointer;
    /* Additional 68k registers would be saved here */
} ProcessContext;

/* Process control block. */
struct ProcessControlBlock {
    ProcessSerialNumber processID;
    OSType processSignature;
    OSType processType;
    ProcessState processState;
    UInt32 processMode;
    Ptr processLocation;
    Size processSize;
    THz processHeapZone;
    Ptr processStackBase;
    Size processStackSize;
    Ptr processA5World;
    UInt32 processCreationTime;
    UInt32 processLastEventTime;
    EventMask processEventMask;
    short processPriority;
    Ptr processContextSave;
    struct ProcessControlBlock* processNextProcess;
};

/* Process queue for cooperative scheduling. */
struct ProcessQueue {
    ProcessControlBlock* queueHead;
    ProcessControlBlock* queueTail;
    short queueSize;
    ProcessControlBlock* currentProcess;
};

/* Process Lifecycle Management */
OSErr ProcessManager_Initialize(void);
OSErr Process_Create(const void* appSpec, Size memorySize, LaunchFlags flags);
OSErr Process_Cleanup(ProcessSerialNumber* psn);
OSErr LaunchApplication(LaunchParamBlockRec* launchParams);
OSErr ExitToShell(void);

/* Cooperative Scheduling */
OSErr Scheduler_GetNextProcess(ProcessControlBlock** nextProcess);
OSErr Context_Switch(ProcessControlBlock* targetProcess);

/* Process Information */
OSErr GetCurrentProcess(ProcessSerialNumber* currentPSN);
OSErr GetNextProcess(ProcessSerialNumber* psn);
OSErr SetFrontProcess(const ProcessSerialNumber* psn);
OSErr GetFrontProcess(ProcessSerialNumber* frontPSN);
OSErr SameProcess(const ProcessSerialNumber* psn1, const ProcessSerialNumber* psn2, Boolean* result);

/* Keyboard modifier state */
UInt16 GetCurrentModifiers(void);


/* MultiFinder Integration */
OSErr MultiFinder_Init(void);

/* Application Switcher Integration */
ProcessSerialNumber ProcessManager_GetFrontProcess(void);
OSErr ProcessManager_SetFrontProcess(ProcessSerialNumber psn);
ProcessQueue* ProcessManager_GetProcessQueue(void);

/* Application File Management - System 7 */
void GetAppParms(Str255 apName, SInt16* apRefNum, Handle* apParam);
void CountAppFiles(SInt16* message, SInt16* count);
OSErr GetAppFiles(SInt16 index, AppFile* theFile);
void ClrAppFiles(SInt16 index);

/* Global Process Manager state. */
extern ProcessQueue* gProcessQueue;
extern ProcessControlBlock* gCurrentProcess;
extern ProcessSerialNumber gSystemProcessPSN;
extern Boolean gMultiFinderActive;

#ifdef __cplusplus
}
#endif

#endif /* __PROCESSMGR_H__ */
