/*
/* QElemPtr defined in MacTypes.h */
 * TimeManager.h - System 7.1 Time Manager Interface
 *
 * The Time Manager provides high-resolution timing services for Mac OS
 * applications. It manages timer tasks, executes them at specified times,
 * and provides VBL (Vertical Blanking) synchronization.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

#include "../DeskManager/Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Time Manager Constants */
#define tmType          1       /* Queue type for Time Manager tasks */
#define vblType         16      /* Queue type for VBL tasks */

/* Timer task states */

/* Time units */

/* 64-bit wide integer for extended timing */

/* Timer procedure pointer types */

/* QElem and QElemPtr are defined in SystemTypes.h */

/* Queue header structure */
//      /* Queue flags */
//     volatile QElemPtr qHead;    /* First queue element */
//     volatile QElemPtr qTail;    /* Last queue element */
// } QHdr, *QHdrPtr;
// 
// /* Classic Time Manager task record (22 bytes) */
//       /* Queue link (offset 0) */
//     short           qType;      /* Queue type (offset 4) */
//     TimerProcPtr    tmAddr;     /* Task procedure (offset 6) */
//     long            tmCount;    /* Timer count in milliseconds (offset 10) */
//     long            tmWakeUp;   /* Wake-up time (offset 14) */
//     long            tmReserved; /* Reserved (offset 18) */
// } TMTask, *TMTaskPtr;
// 
// /* Extended Time Manager task record (58 bytes) */
//       /* Queue link */
//     short           qType;      /* Queue type */
//     TimerUPP        tmAddr;     /* Task procedure */
//     long            tmCount;    /* Timer count */
//     long            tmWakeUp;   /* Wake-up time */
//     long            tmReserved; /* Reserved */
//     long            tmRefCon;   /* Reference constant */
//     UnsignedWide    tmWakeUpTime; /* 64-bit wake-up time */
//     long            tmReserved2[6]; /* Reserved */
// } ExtendedTimerRec, *ExtendedTimerPtr;
// 
// /* VBL task record */
//       /* Queue link */
//     short           qType;      /* Queue type (vblType) */
//     ProcPtr         vblAddr;    /* VBL procedure */
//     short           vblCount;   /* VBL count */
//     short           vblPhase;   /* VBL phase */
// } VBLTask, *VBLTaskPtr;
// 
// /* Time Manager API Functions */
// 
// /* Initialization */
// OSErr InitTimeManager(void);
// void ShutdownTimeManager(void);
// 
// /* Classic Time Manager functions */
// OSErr InsTime(QElemPtr tmTaskPtr);
// OSErr RmvTime(QElemPtr tmTaskPtr);
// OSErr PrimeTime(QElemPtr tmTaskPtr, long count);
// 
// /* Extended Time Manager functions */
// OSErr InstallTimeTask(QElemPtr tmTaskPtr);
// OSErr InstallXTimeTask(QElemPtr tmTaskPtr);
// OSErr RemoveTimeTask(QElemPtr tmTaskPtr);
// OSErr PrimeTimeTask(QElemPtr tmTaskPtr, long count);
// OSErr SetTimeBaseZero(void);
// 
// /* Microsecond timing functions */
// void Microseconds(UnsignedWide* microTickCount);
// OSErr MicrosecondsToAbsolute(UnsignedWide* microSeconds);
// OSErr AbsoluteToMicroseconds(UnsignedWide* absolute);
// OSErr NanosecondsToAbsolute(UnsignedWide* nanoSeconds);
// OSErr AbsoluteToNanoseconds(UnsignedWide* absolute);
// 
// /* Utility functions */
// UInt32 TickCount(void);
// UInt32 GetTicks(void);
// void Delay(long numTicks, long* finalTicks);
// OSErr GetCurrentTime(UnsignedWide* currentTime);
// 
// /* Time conversion utilities */
// long MillisecondsToTicks(long milliseconds);
// long TicksToMilliseconds(long ticks);
// long MicrosecondsToTicks(long microseconds);
// long TicksToMicroseconds(long ticks);
// 
// /* VBL task management */
// OSErr VInstall(QElemPtr vblTaskPtr);
// OSErr VRemove(QElemPtr vblTaskPtr);
// OSErr SetInterruptMask(short mask);
// OSErr GetInterruptMask(short* mask);
// 
// /* Timer UPP creation and disposal */
// TimerUPP NewTimerUPP(TimerProcPtr userRoutine);
// void DisposeTimerUPP(TimerUPP userUPP);
// void InvokeTimerUPP(TMTaskPtr tmTaskPtr, TimerUPP userUPP);
// 
// /* Drift-free timer functions */
// OSErr DriftFreeInstallTimeTask(ExtendedTimerPtr tmTaskPtr);
// OSErr DriftFreePrimeTimeTask(ExtendedTimerPtr tmTaskPtr,
//                              UnsignedWide* period);
// 
// /* Time Manager information */
// OSErr GetTimeManagerVersion(short* version);
// OSErr GetTimeManagerFeatures(long* features);
// Boolean TimeManagerAvailable(void);
// 
// /* Debugging and statistics */
// long GetActiveTimerCount(void);
// OSErr GetTimerInfo(QElemPtr tmTaskPtr, long* timeRemaining);
// void DumpTimerQueue(void);
// 
// #ifdef __cplusplus
// }
// #endif
// 
// #endif /* TIMEMANAGER_H */