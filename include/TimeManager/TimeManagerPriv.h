#ifndef TIMEMANAGER_PRIV_H
#define TIMEMANAGER_PRIV_H

#include "SystemTypes.h"
#include "TimeManager/TimeManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Core internal functions */
OSErr Core_Initialize(void);
void Core_Shutdown(void);
OSErr Core_InsertTask(TMTaskPtr task);
OSErr Core_RemoveTask(TMTaskPtr task);
OSErr Core_PrimeTask(TMTaskPtr task, UInt32 microseconds);
OSErr Core_CancelTask(TMTaskPtr task);
UInt32 Core_GetActiveCount(void);
UInt32 Core_GetTaskGeneration(TMTaskPtr task);

/* Core exposes this to ISR */
void Core_ExpireDue(UInt64 nowUS);

/* ISR exposes this to Core */
void ProgramNextTimerInterrupt(UInt64 absDeadlineUS);

/* Deferred queue management */
void InitDeferredQueue(void);
void ShutdownDeferredQueue(void);
void EnqueueDeferred(TMTaskPtr task, UInt32 gen);

#ifdef TM_SELFTEST
void TimeManager_RunSelfTest(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* TIMEMANAGER_PRIV_H */
