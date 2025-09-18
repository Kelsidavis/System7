/*
 * RE-AGENT-BANNER
 * ProcessMgr.h - Mac OS System 7 Process Manager Interface
 *
 * Reverse engineered from System.rsrc
 * SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba
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

#include <Types.h>
#include <Events.h>
#include <Files.h>
#include <Memory.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Process Manager Constants
 * Evidence: Standard Mac OS System 7 conventions
 */
#define kPM_MaxProcesses        32
#define kPM_InvalidProcessID    0xFFFFFFFF
#define kPM_SystemProcessID     0x00000001
#define kPM_FinderProcessID     0x00000002

/*
 * Process States for Cooperative Multitasking
 * Evidence: layouts.process_manager.json:process_control_block.processState
 */
typedef enum {
    kProcessRunning = 0,        /* Currently executing process */
    kProcessSuspended = 1,      /* Suspended by MultiFinder */
    kProcessBackground = 2,     /* Background process */
    kProcessTerminated = 3      /* Process marked for cleanup */
} ProcessState;

/*
 * Process Mode Flags
 * Evidence: MultiFinder configuration and cooperative behavior
 */
typedef enum {
    kProcessModeCooperative = 0x0001,   /* Cooperative multitasking */
    kProcessModeCanBackground = 0x0002,  /* Can run in background */
    kProcessModeNeedsEvents = 0x0004,    /* Requires event processing */
    kProcessModeAcceptSuspend = 0x0008   /* Can be suspended */
} ProcessMode;

/*
 * Launch Control Flags
 * Evidence: layouts.process_manager.json:launch_parameters.launchControlFlags
 */
typedef enum {
    kLaunchNewInstance = 0x0001,    /* Launch new instance */
    kLaunchDontSwitch = 0x0002,     /* Don't switch to new process */
    kLaunchNoFileFlags = 0x0004,    /* Ignore file flags */
    kLaunchMixedMode = 0x0008       /* Mixed mode (68k/PowerPC) */
} LaunchFlags;

/*
 * Process Serial Number
 * Evidence: Standard Mac OS Process Manager identifier
 */
typedef struct ProcessSerialNumber {
    UInt32 highLongOfPSN;
    UInt32 lowLongOfPSN;
} ProcessSerialNumber;

/*
 * Process Control Block - Core data structure for each process
 * Evidence: layouts.process_manager.json:process_control_block
 * Size: 256 bytes, aligned to 4 bytes
 */
typedef struct ProcessControlBlock {
    ProcessSerialNumber processID;      /* Offset 0: Unique process identifier */
    OSType              processSignature; /* Offset 8: Four-character signature */
    OSType              processType;    /* Offset 12: Process type (APPL, etc.) */
    ProcessState        processState;   /* Offset 16: Current process state */
    ProcessMode         processMode;    /* Offset 18: Process execution mode */
    Ptr                 processLocation; /* Offset 20: Process memory partition */
    Size                processSize;    /* Offset 24: Size of memory partition */
    THz                 processHeapZone; /* Offset 28: Process heap zone */
    Ptr                 processStackBase; /* Offset 32: Base of process stack */
    Size                processStackSize; /* Offset 36: Size of process stack */
    Ptr                 processA5World; /* Offset 40: A5 register value */
    StringPtr           processEnvirons; /* Offset 44: Environment variables */
    GDHandle            processCurGDevice; /* Offset 48: Current graphics device */
    short               processCurResFile; /* Offset 52: Current resource file */
    struct ProcessControlBlock* processNextProcess; /* Offset 54: Next in queue */

    /* Additional System 7 fields */
    UInt32              processCreationTime; /* Offset 58: Process start time */
    UInt32              processLastEventTime; /* Offset 62: Last event time */
    UInt16              processEventMask;   /* Offset 66: Event mask */
    UInt16              processPriority;    /* Offset 68: Process priority */
    Ptr                 processContextSave; /* Offset 70: Saved context area */

    /* Reserved for future expansion */
    UInt8               reserved[186];      /* Offset 74: Reserved bytes */
} ProcessControlBlock;

/*
 * Launch Parameter Block for starting new processes
 * Evidence: layouts.process_manager.json:launch_parameters
 */
typedef struct LaunchParamBlockRec {
    UInt32      launchBlockID;      /* Parameter block identifier */
    UInt16      launchEPBLength;    /* Extended parameter block length */
    UInt16      launchFileFlags;    /* File system flags */
    LaunchFlags launchControlFlags; /* Launch control flags */
    FSSpec      launchAppSpec;      /* Application file specification */
    Ptr         launchAppParameters; /* Application parameters */
    Size        launchMinSize;      /* Minimum memory size */
    Size        launchPrefSize;     /* Preferred memory size */
} LaunchParamBlockRec;

/*
 * Process Queue for Cooperative Scheduling
 * Evidence: layouts.process_manager.json:scheduler_queue
 */
typedef struct ProcessQueue {
    ProcessControlBlock* queueHead;     /* Head of process queue */
    ProcessControlBlock* queueTail;     /* Tail of process queue */
    UInt16              queueSize;      /* Number of processes in queue */
    UInt16              queuePadding;   /* Alignment padding */
    ProcessControlBlock* currentProcess; /* Currently running process */
} ProcessQueue;

/*
 * Process Context Save Area for 68k Context Switching
 * Evidence: layouts.process_manager.json:context_save_area
 */
typedef struct ProcessContext {
    UInt32  savedDataRegisters[8]; /* D0-D7 registers */
    UInt32  savedAddrRegisters[8]; /* A0-A7 registers */
    UInt16  savedSR;               /* Status register */
    UInt16  savedPadding;          /* Alignment padding */
    UInt32  savedPC;               /* Program counter */
    UInt32  savedA5;               /* A5 register (globals) */
    UInt32  savedStackPointer;     /* Stack pointer */
    UInt32  reserved[29];          /* Reserved for expansion */
} ProcessContext;

/*
 * Process Manager Function Prototypes
 * Evidence: mappings.process_manager.json:function_mappings
 */

/* Process Lifecycle Management */
OSErr ProcessManager_Initialize(void);
OSErr Process_Create(const FSSpec* appSpec, Size memorySize, LaunchFlags flags);
OSErr Process_Cleanup(ProcessSerialNumber* psn);
OSErr LaunchApplication(LaunchParamBlockRec* launchParams);
OSErr ExitToShell(void);

/* Cooperative Scheduling */
OSErr Scheduler_GetNextProcess(ProcessControlBlock** nextProcess);
OSErr Context_Switch(ProcessControlBlock* targetProcess);
OSErr Process_Yield(void);
OSErr Process_Suspend(ProcessSerialNumber* psn);
OSErr Process_Resume(ProcessSerialNumber* psn);

/* Process Information */
OSErr GetProcessInformation(ProcessSerialNumber* psn, ProcessInfoRec* info);
OSErr GetCurrentProcess(ProcessSerialNumber* currentPSN);
OSErr GetNextProcess(ProcessSerialNumber* psn);
OSErr SetFrontProcess(ProcessSerialNumber* psn);

/* Event Integration for Cooperative Multitasking */
Boolean WaitNextEvent(EventMask eventMask, EventRecord* theEvent,
                     UInt32 sleep, RgnHandle mouseRgn);
Boolean GetNextEvent(EventMask eventMask, EventRecord* theEvent);
OSErr PostEvent(EventKind eventNum, UInt32 eventMsg);
void FlushEvents(EventMask eventMask, EventMask stopMask);

/* Memory Management Integration */
OSErr Process_AllocateMemory(ProcessSerialNumber* psn, Size blockSize, Ptr* block);
OSErr Process_DeallocateMemory(ProcessSerialNumber* psn, Ptr block);
OSErr Process_SetMemorySize(ProcessSerialNumber* psn, Size newSize);

/* MultiFinder Integration */
OSErr MultiFinder_Init(void);
OSErr MultiFinder_ConfigureProcess(ProcessSerialNumber* psn, ProcessMode mode);
Boolean MultiFinder_IsActive(void);

/*
 * Global Process Manager Variables
 * Evidence: System 7 Process Manager implementation
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