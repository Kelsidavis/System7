/*
 * TimerTasks.c - Deferred task execution queue
 * Based on Inside Macintosh: Operating System Utilities (Time Manager)
 */

#include "SystemTypes.h"
#include "TimeManager/TimeManager.h"
#include "TimeManager/TimeManagerPriv.h"
#include "TimeManager/TimeBase.h"

#define TM_DEFERRED_QUEUE_SIZE 256

typedef struct {
    TMTask *task;
    UInt32  gen;
} DeferredEntry;

/* SPSC ring buffer */
static DeferredEntry gDeferredQueue[TM_DEFERRED_QUEUE_SIZE];
static volatile UInt32 gDeferredHead = 0;  /* ISR writes */
static volatile UInt32 gDeferredTail = 0;  /* Main reads */

/* Core_GetTaskGeneration declared in TimeManagerPriv.h */

void InitDeferredQueue(void) {
    gDeferredHead = 0;
    gDeferredTail = 0;
}

void ShutdownDeferredQueue(void) {
    gDeferredHead = 0;
    gDeferredTail = 0;
}

void EnqueueDeferred(TMTask *task, UInt32 gen) {
    if (!task) return;

    UInt32 next = (gDeferredHead + 1) % TM_DEFERRED_QUEUE_SIZE;
    if (next == gDeferredTail) {
        /* Queue full, drop */
        return;
    }

    gDeferredQueue[gDeferredHead].task = task;
    gDeferredQueue[gDeferredHead].gen = gen;
    gDeferredHead = next;
}

void TimeManager_DrainDeferred(UInt32 maxTasks, UInt32 maxMicros) {
    if (maxTasks == 0) return;

    UnsignedWide start;
    Microseconds(&start);
    UInt64 startUS = ((UInt64)start.hi << 32) | start.lo;

    UInt32 count = 0;
    while (gDeferredTail != gDeferredHead && count < maxTasks) {
        /* Check time limit */
        if (maxMicros > 0 && count > 0) {
            UnsignedWide now;
            Microseconds(&now);
            UInt64 nowUS = ((UInt64)now.hi << 32) | now.lo;
            if (nowUS - startUS >= maxMicros) {
                break;
            }
        }

        /* Dequeue entry */
        DeferredEntry entry = gDeferredQueue[gDeferredTail];
        gDeferredTail = (gDeferredTail + 1) % TM_DEFERRED_QUEUE_SIZE;

        /* Check generation to prevent stale callbacks */
        UInt32 currentGen = Core_GetTaskGeneration(entry.task);
        if (entry.task && entry.task->tmAddr && entry.gen == currentGen) {
            /* Generation matches, safe to invoke */
            typedef void (*TMTaskProc)(TMTask*);
            TMTaskProc taskProc = (TMTaskProc)__builtin_assume_aligned(
                entry.task->tmAddr, _Alignof(TMTaskProc));
            taskProc(entry.task);
        }
        /* else: task was cancelled or reused, skip callback */

        count++;
    }
}

#ifdef TM_SELFTEST
/* Self-test code */
extern void serial_puts(const char *s);

static volatile UInt32 gTestCounter = 0;
static volatile UInt32 gTestPeriodic = 0;

static void test_oneshot(TMTask *task) {
    (void)task;
    gTestCounter++;
    serial_puts("[TM_TEST] One-shot fired\n");
}

static void test_periodic(TMTask *task) {
    (void)task;
    gTestPeriodic++;
    if (gTestPeriodic >= 5) {
        CancelTime(task);
        serial_puts("[TM_TEST] Periodic stopped after 5 fires\n");
    }
}

void TimeManager_RunSelfTest(void) {
    serial_puts("[TM_TEST] Starting self-test...\n");

    TMTask oneshot1 = {0};
    TMTask oneshot2 = {0};
    TMTask periodic = {0};

    /* Schedule one-shots */
    InsTime(&oneshot1);
    oneshot1.tmAddr = (void*)test_oneshot;
    PrimeTime(&oneshot1, 1000);  /* 1ms */

    InsTime(&oneshot2);
    oneshot2.tmAddr = (void*)test_oneshot;
    PrimeTime(&oneshot2, 3000);  /* 3ms */

    /* Schedule periodic */
    InsTime(&periodic);
    periodic.tmAddr = (void*)test_periodic;
    periodic.qType = 0x0001; /* TM_FLAG_PERIODIC */
    PrimeTime(&periodic, 2000);  /* 2ms period */

    /* Run for ~15ms */
    UnsignedWide testStart;
    Microseconds(&testStart);
    UInt64 testStartUS = ((UInt64)testStart.hi << 32) | testStart.lo;

    while (1) {
        TimeManager_TimerISR();
        TimeManager_DrainDeferred(16, 1000);

        UnsignedWide now;
        Microseconds(&now);
        UInt64 nowUS = ((UInt64)now.hi << 32) | now.lo;

        if (nowUS - testStartUS >= 15000) {
            break;
        }
    }

    /* Check results */
    if (gTestCounter == 2 && gTestPeriodic == 5) {
        serial_puts("[TM_TEST] PASS - All tests completed\n");
    } else {
        serial_puts("[TM_TEST] FAIL - Unexpected counts\n");
    }

    /* Cleanup */
    RmvTime(&oneshot1);
    RmvTime(&oneshot2);
    RmvTime(&periodic);
}
#endif /* TM_SELFTEST */
