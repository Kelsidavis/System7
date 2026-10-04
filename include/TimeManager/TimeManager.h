#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* We extend TMTask with private scheduling fields via a parallel structure */

/* Flags */
#define TM_FLAG_PERIODIC 0x0001

/* Errors */
#define tmNoErr      0
#define tmParamErr  -50
#define tmQueueFull -201
#define tmNotActive -202

/* Lifecycle */
OSErr InitTimeManager(void);
void  ShutdownTimeManager(void);

/* Task management */
OSErr InsTime  (TMTaskPtr task);
OSErr RmvTime  (TMTaskPtr task);
OSErr PrimeTime(TMTaskPtr task, UInt32 microseconds);
OSErr CancelTime(TMTaskPtr task);

/* Deferred draining */
void TimeManager_DrainDeferred(UInt32 maxTasks, UInt32 maxMicros);

/* ISR entry */
void TimeManager_TimerISR(void);

/* Queries */
UInt32 TimeManager_GetResolutionNS(void);
UInt32 TimeManager_GetOverheadUS(void);
UInt32 TimeManager_GetActiveCount(void);

#ifdef __cplusplus
}
#endif
#endif /* TIMEMANAGER_H */
