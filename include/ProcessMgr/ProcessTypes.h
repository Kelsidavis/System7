#ifndef PROCESS_TYPES_H
#define PROCESS_TYPES_H

/*
 * ProcessTypes.h - Process Manager Type Definitions
 *
 * Core types for cooperative process scheduling and management.
 */

#include "SystemTypes.h"

/* Process ID type */
typedef UInt32 ProcessID;

/* Process Manager error codes */
enum {
    procNotFound = -600,
    memFragErr = -601,
    appModeErr = -602,
    protocolErr = -603,
    hardwareConfigErr = -604,
    appMemFullErr = -605,
    appIsDaemon = -606,
    bufferIsSmall = -607,
    noOutstandingHLE = -608,
    connectionInvalid = -609,
    noUserInteractionAllowed = -610
};

/* Process Manager function prototypes */

/* Scheduler functions */
OSErr Proc_Init(void);
ProcessID Proc_New(const char* name, void* entry, void* arg,
                   Size stackSize, UInt8 priority);
void Proc_Yield(void);
void Proc_Sleep(UInt32 microseconds);
void Proc_BlockOnEvent(EventMask mask, EventRecord* evt);
void Proc_Wake(ProcessID pid);
void Proc_UnblockEvent(EventRecord* evt);
ProcessID Proc_GetCurrent(void);
void Proc_DumpTable(void);

/* Event integration functions */
void Event_InitQueue(void);
UInt16 Event_QueueCount(void);
void Event_DumpQueue(void);

/* Process-aware event functions */
Boolean Proc_GetNextEvent(EventMask mask, EventRecord* evt);
Boolean Proc_EventAvail(EventMask mask, EventRecord* evt);
OSErr Proc_PostEvent(EventKind what, UInt32 message);

/* When ENABLE_PROCESS_COOP is defined, these override canonical APIs */

#endif /* PROCESS_TYPES_H */