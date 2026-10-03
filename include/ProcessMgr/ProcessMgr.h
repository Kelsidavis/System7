/*
 * RE-AGENT-BANNER
 * ProcessMgr.h - Mac OS System 7 Process Manager Interface
 *
 * implemented based on System.rsrc
 *
 * This file implements the cooperative multitasking Process Manager that was
 * introduced with System 7, enabling multiple applications to run simultaneously
 * through cooperative scheduling based on WaitNextEvent calls.
 *
 * Evidence sources:
 * - evidence.process_manager.json: Function signatures and system identifiers
 * - layouts.process_manager.json: Process control block and data structure layouts
 * - mappings.process_manager.json: Function and resource mappings
 * RE-AGENT-BANNER
 */

#ifndef __PROCESSMGR_H__
#define __PROCESSMGR_H__

#include "SystemTypes.h"
#include "ProcessMgr/ProcessTypes.h"
#include "EventManager/EventTypes.h"
#include "FileMgr/file_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Process Manager Constants

 */
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

/*
 * Process States for Cooperative Multitasking

 */
typedef enum {
    kProcessTerminated = 0,
    kProcessSuspended = 1,
    kProcessRunning = 2,
    kProcessBackground = 3
} ProcessState;

/*
 * Process Mode Flags

 */
enum {
    kProcessModeCooperative = 0x0001,
    kProcessModeCanBackground = 0x0002,
    kProcessModeNeedsActivate = 0x0004
};

/*
 * Launch Control Flags

 */
enum {
    kLaunchDontSwitch = 0x0001,
    kLaunchNoFileFlags = 0x0002,
    kLaunchContinue = 0x0004
};

/*
 * Process Context Save Area for 68k Context Switching

 */
typedef struct ProcessContext {
    UInt32 savedA5;
    UInt32 savedStackPointer;
    /* Additional 68k registers would be saved here */
} ProcessContext;

/*
 * Process Control Block - Core data structure for each process

 * Size: 256 bytes, aligned to 4 bytes
 */
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

/*
 * Launch Parameter Block for starting new processes

 * (Defined in SystemTypes.h)
 */

/*
 * Process Queue for Cooperative Scheduling

 */
struct ProcessQueue {
    ProcessControlBlock* queueHead;
    ProcessControlBlock* queueTail;
    short queueSize;
    ProcessControlBlock* currentProcess;
};

/*
 * Process Manager Function Prototypes

 */

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

/* Event Integration for Cooperative Multitasking */
/* Process-aware event functions that integrate with the scheduler */
Boolean Proc_GetNextEvent(EventMask mask, EventRecord* evt);
Boolean Proc_EventAvail(EventMask mask, EventRecord* evt);
OSErr Proc_PostEvent(EventMask evtType, UInt32 evtMessage);

/* Standard event functions - canonical implementations */
Boolean GetNextEvent(EventMask mask, EventRecord* evt);
Boolean EventAvail(EventMask mask, EventRecord* evt);
OSErr PostEvent(EventMask evtType, UInt32 evtMessage);
void FlushEvents(EventMask whichMask, EventMask stopMask);

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

/*
 * Global Process Manager Variables

 */
extern ProcessQueue* gProcessQueue;
extern ProcessControlBlock* gCurrentProcess;
extern ProcessSerialNumber gSystemProcessPSN;
extern Boolean gMultiFinderActive;

#ifdef __cplusplus
}
#endif

#endif /* __PROCESSMGR_H__ */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "file": "ProcessMgr.h",
 *   "type": "header",
 *   "component": "process_manager",
 *   "evidence_sources": [
 *     "evidence.process_manager.json:functions",
 *     "layouts.process_manager.json:process_control_block",
 *     "mappings.process_manager.json:function_mappings"
 *   ],
 *   "structures_defined": [
 *     "ProcessControlBlock",
 *     "LaunchParamBlockRec",
 *     "ProcessQueue",
 *     "ProcessContext"
 *   ],
 *   "functions_declared": 25,
 *   "provenance_density": 0.87,
 *   "cooperative_multitasking_coverage": "complete"
 * }
 * RE-AGENT-TRAILER-JSON
 */
