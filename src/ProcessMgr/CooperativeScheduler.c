/*
 * CooperativeScheduler.c - Cooperative Process Scheduler
 *
 * Implements cooperative multitasking with round-robin scheduling
 * and aging priority system. Integrates with Time Manager for
 * microsecond-precision sleep timers.
 */

#include "SystemTypes.h"
#include "ProcessMgr/ProcessTypes.h"
#include "TimeManager/TimeManager.h"
#include "EventManager/EventTypes.h"
#include "System71StdLib.h"
#include "ProcessMgr/ProcessLogging.h"

/* Process states */
#define PROC_FREE     0
#define PROC_READY    1
#define PROC_RUNNING  2
#define PROC_BLOCKED  3
#define PROC_SLEEPING 4

/* Process entry function type */
typedef void (*ProcEntry)(void* arg);

/* Process control block */
typedef struct ProcessCB {
    /* State and identity */
    UInt8 state;
    UInt8 priority;     /* Base priority 0-15 */
    UInt8 aging;        /* Aging bonus 0-15 */
    UInt8 flags;

    /* Process context */
    void* stackPtr;
    void* stackBase;
    Size stackSize;

    /* Entry point */
    ProcEntry entry;
    void* arg;
    Boolean neverStarted;

    /* Scheduling */
    struct ProcessCB* next;
    struct ProcessCB* prev;

    /* Sleep/wake management */
    TMTask wakeTimer;
    UInt32 wakeTime;

    /* Event blocking */
    EventMask eventMask;
    EventRecord* eventPtr;

    /* Process info */
    ProcessID pid;
    char name[32];
} ProcessCB;

/* Process table - 16 slots max */
#define MAX_PROCESSES 16
static ProcessCB gProcessTable[MAX_PROCESSES];
static ProcessCB* gCurrentProcess = NULL;
static ProcessCB* gReadyQueue = NULL;
static UInt32 gNextPID = 1;
static Boolean gSchedulerInitialized = false;

/* Forward declarations */
static void AddToReadyQueue(ProcessCB* proc);
static void RemoveFromReadyQueue(ProcessCB* proc);
static ProcessCB* SelectNextProcess(void);
static void WakeTimerCallback(TMTaskPtr tmTaskPtr);

/* Logging helpers */
#define PROC_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleSystem, kLogLevelDebug, "[PROC] " fmt, ##__VA_ARGS__)
#define PROC_LOG_TRACE(fmt, ...) serial_logf(kLogModuleSystem, kLogLevelTrace, "[PROC] " fmt, ##__VA_ARGS__)
#define PROC_LOG_WARN(fmt, ...)  serial_logf(kLogModuleSystem, kLogLevelWarn,  "[PROC] " fmt, ##__VA_ARGS__)
#define PROC_LOG_INFO(fmt, ...)  serial_logf(kLogModuleSystem, kLogLevelInfo, "[PROC] " fmt, ##__VA_ARGS__)

/* String functions from System71StdLib */
extern void* memset(void* s, int c, size_t n);
extern char* strcpy(char* dest, const char* src);
extern char* strncpy(char* dest, const char* src, size_t n);

/*
 * Proc_Init - Initialize cooperative scheduler
 */
OSErr Proc_Init(void) {
    if (gSchedulerInitialized) {
        return noErr;
    }

    /* Clear process table */
    memset(gProcessTable, 0, sizeof(gProcessTable));

    /* Create idle process (PID 0) */
    ProcessCB* idle = &gProcessTable[0];
    idle->state = PROC_READY;
    idle->priority = 0;
    idle->aging = 0;
    idle->pid = 0;
    strncpy(idle->name, "Idle", sizeof(idle->name) - 1);
    idle->name[sizeof(idle->name) - 1] = '\0';

    /* Set as current and only ready process */
    gCurrentProcess = idle;
    gReadyQueue = idle;
    idle->next = idle;
    idle->prev = idle;

    gSchedulerInitialized = true;
    PROC_LOG_DEBUG("Scheduler initialized\n");

    return noErr;
}

/*
 * Proc_New - Create new process
 */
ProcessID Proc_New(const char* name, void* entry, void* arg,
                   Size stackSize, UInt8 priority) {
    ProcessCB* proc = NULL;

    if (!gSchedulerInitialized) {
        Proc_Init();
    }

    /* Find free slot (skip idle at 0) */
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (gProcessTable[i].state == PROC_FREE) {
            proc = &gProcessTable[i];
            break;
        }
    }

    if (!proc) {
        PROC_LOG_WARN("No free process slots\n");
        return 0;
    }

    /* Initialize process */
    proc->state = PROC_READY;
    proc->priority = priority & 0x0F;
    proc->aging = 0;
    proc->flags = 0;

    proc->stackPtr = NULL;  /* No stack switching yet */
    proc->stackBase = NULL;
    proc->stackSize = stackSize;

    /* Store entry point for cooperative tasklet */
    proc->entry = (ProcEntry)entry;
    proc->arg = arg;
    proc->neverStarted = true;

    proc->pid = gNextPID++;
    strncpy(proc->name, name, 31);
    proc->name[31] = '\0';

    /* Clear wake timer */
    memset(&proc->wakeTimer, 0, sizeof(TMTask));
    proc->wakeTime = 0;

    /* Clear event blocking */
    proc->eventMask = 0;
    proc->eventPtr = NULL;

    /* Add to ready queue */
    AddToReadyQueue(proc);

    PROC_LOG_DEBUG("Created process %d '%s' pri=%d\n",
                   proc->pid, proc->name, proc->priority);

    return proc->pid;
}

/*
 * Proc_Yield - Yield CPU to next process
 */
void Proc_Yield(void) {
    ProcessCB* next;

    if (!gSchedulerInitialized || !gCurrentProcess) {
        return;
    }

    /* If current process still ready, age other processes */
    if (gCurrentProcess->state == PROC_RUNNING) {
        gCurrentProcess->state = PROC_READY;

        /* Age all other ready processes */
        ProcessCB* proc = gReadyQueue;
        if (proc) {
            do {
                if (proc != gCurrentProcess && proc->aging < 15) {
                    proc->aging++;
                }
                proc = proc->next;
            } while (proc != gReadyQueue);
        }
    }

    /* Select next process */
    next = SelectNextProcess();
    if (next && next != gCurrentProcess) {
        ProcessCB* prev = gCurrentProcess;

        /* Switch to new process */
        gCurrentProcess = next;
        next->state = PROC_RUNNING;
        next->aging = 0;  /* Reset aging on run */

        PROC_LOG_TRACE("Switch %d->%d\n", prev->pid, next->pid);

        /* First-time execution of tasklet */
        if (next->neverStarted && next->entry) {
            next->neverStarted = false;
            PROC_LOG_TRACE("Starting tasklet %d\n", next->pid);
            next->entry(next->arg);
            /* If entry returns, mark process as free */
            next->state = PROC_FREE;
            RemoveFromReadyQueue(next);
            /* Need to pick another process */
            Proc_Yield();
        }
    }
}

/*
 * Proc_Sleep - Sleep for microseconds using Time Manager
 */
void Proc_Sleep(UInt32 microseconds) {
    if (!gCurrentProcess || gCurrentProcess->pid == 0) {
        /* Can't sleep idle process */
        return;
    }

    /* Set up wake timer - critical: must set callback or timer never fires! */
    gCurrentProcess->wakeTimer.tmAddr = (Ptr)WakeTimerCallback;
    gCurrentProcess->wakeTimer.tmWakeUp = 0;
    gCurrentProcess->wakeTimer.tmReserved = 0;

    /* Calculate wake time */
    gCurrentProcess->wakeTime = microseconds;

    /* Install timer task */
    InsTime(&gCurrentProcess->wakeTimer);
    PrimeTime(&gCurrentProcess->wakeTimer, microseconds);

    /* Remove from ready queue and mark sleeping */
    RemoveFromReadyQueue(gCurrentProcess);
    gCurrentProcess->state = PROC_SLEEPING;

    PROC_LOG_TRACE("Process %d sleeping for %u us\n",
                   gCurrentProcess->pid, (unsigned int)microseconds);

    /* Yield to next process */
    Proc_Yield();
}

/*
 * Proc_BlockOnEvent - Block waiting for event
 */
void Proc_BlockOnEvent(EventMask mask, EventRecord* evt) {
    if (!gCurrentProcess || gCurrentProcess->pid == 0) {
        /* Can't block idle process - it must always be ready */
        if (gCurrentProcess && gCurrentProcess->pid == 0) {
            PROC_LOG_WARN("Idle process cannot block\n");
        }
        return;
    }

    /* Set up event blocking */
    gCurrentProcess->eventMask = mask;
    gCurrentProcess->eventPtr = evt;

    /* Remove from ready queue and mark blocked */
    RemoveFromReadyQueue(gCurrentProcess);
    gCurrentProcess->state = PROC_BLOCKED;

    PROC_LOG_TRACE("Process %d blocked on events 0x%08x\n",
                   gCurrentProcess->pid, mask);

    /* Yield to next process */
    Proc_Yield();
}

/*
 * Proc_Wake - Wake process by PID
 */
void Proc_Wake(ProcessID pid) {
    ProcessCB* proc = NULL;

    /* Find process */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (gProcessTable[i].pid == pid &&
            gProcessTable[i].state != PROC_FREE) {
            proc = &gProcessTable[i];
            break;
        }
    }

    if (!proc) {
        return;
    }

    /* Wake based on state */
    if (proc->state == PROC_SLEEPING) {
        /* Cancel wake timer */
        RmvTime(&proc->wakeTimer);
        proc->wakeTime = 0;

        /* Return to ready */
        proc->state = PROC_READY;
        AddToReadyQueue(proc);

        PROC_LOG_TRACE("Woke sleeping process %d\n", pid);
    }
    else if (proc->state == PROC_BLOCKED) {
        /* Clear event blocking */
        proc->eventMask = 0;
        proc->eventPtr = NULL;

        /* Return to ready */
        proc->state = PROC_READY;
        AddToReadyQueue(proc);

        PROC_LOG_TRACE("Woke blocked process %d\n", pid);
    }
}

/*
 * Proc_UnblockEvent - Unblock processes waiting for event
 */
void Proc_UnblockEvent(EventRecord* evt) {
    for (int i = 1; i < MAX_PROCESSES; i++) {
        ProcessCB* proc = &gProcessTable[i];

        if (proc->state == PROC_BLOCKED &&
            (proc->eventMask & (1 << evt->what))) {

            /* Copy event if buffer provided */
            if (proc->eventPtr) {
                *proc->eventPtr = *evt;
            }

            /* Wake process */
            proc->eventMask = 0;
            proc->eventPtr = NULL;
            proc->state = PROC_READY;
            AddToReadyQueue(proc);

            PROC_LOG_TRACE("Unblocked process %d for event %d\n",
                            proc->pid, evt->what);
        }
    }
}

/*
 * Proc_GetCurrent - Get current process ID
 */
ProcessID Proc_GetCurrent(void) {
    return gCurrentProcess ? gCurrentProcess->pid : 0;
}

/*
 * Queue management
 */
static void AddToReadyQueue(ProcessCB* proc) {
    if (!proc) return;

    if (!gReadyQueue) {
        /* First ready process */
        gReadyQueue = proc;
        proc->next = proc;
        proc->prev = proc;
    } else {
        /* Add to end of queue */
        proc->next = gReadyQueue;
        proc->prev = gReadyQueue->prev;
        gReadyQueue->prev->next = proc;
        gReadyQueue->prev = proc;
    }
}

static void RemoveFromReadyQueue(ProcessCB* proc) {
    if (!proc) return;

    /* Idle process (PID 0) must never leave ready queue */
    if (proc->pid == 0) {
        PROC_LOG_WARN("Attempt to remove idle from ready queue\n");
        return;
    }

    if (proc->next == proc) {
        /* Last non-idle process - keep idle in queue */
        if (proc->pid != 0) {
            /* Find idle process */
            ProcessCB* idle = &gProcessTable[0];
            gReadyQueue = idle;
            idle->next = idle;
            idle->prev = idle;
        }
    } else {
        /* Remove from queue */
        proc->prev->next = proc->next;
        proc->next->prev = proc->prev;

        if (gReadyQueue == proc) {
            gReadyQueue = proc->next;
        }
    }

    proc->next = NULL;
    proc->prev = NULL;
}

/*
 * SelectNextProcess - Choose next process to run
 */
static ProcessCB* SelectNextProcess(void) {
    ProcessCB* best = NULL;
    UInt8 bestScore = 0;

    if (!gReadyQueue) {
        /* No ready processes - shouldn't happen with idle */
        return gCurrentProcess;
    }

    /* Find highest priority + aging */
    ProcessCB* proc = gReadyQueue;
    do {
        UInt8 score = proc->priority + proc->aging;
        if (!best || score > bestScore) {
            best = proc;
            bestScore = score;
        }
        proc = proc->next;
    } while (proc != gReadyQueue);

    return best;
}

/*
 * WakeTimerCallback - Time Manager callback for waking sleeping process
 */
static void WakeTimerCallback(TMTaskPtr tmTaskPtr) {
    /* Find process that owns this timer */
    ProcessCB* proc = NULL;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (&gProcessTable[i].wakeTimer == tmTaskPtr) {
            proc = &gProcessTable[i];
            break;
        }
    }

    if (proc && proc->state == PROC_SLEEPING) {
        /* Wake process */
        proc->wakeTime = 0;
        proc->state = PROC_READY;
        AddToReadyQueue(proc);

        PROC_LOG_TRACE("Timer woke process %d\n", proc->pid);
    }
}

/*
 * Debugging functions
 */
void Proc_DumpTable(void) {
    PROC_LOG_INFO("\n=== Process Table ===\n");
    PROC_LOG_INFO("Current: %d\n", gCurrentProcess ? gCurrentProcess->pid : -1);

    for (int i = 0; i < MAX_PROCESSES; i++) {
        ProcessCB* proc = &gProcessTable[i];
        if (proc->state != PROC_FREE) {
            const char* stateStr = "?";
            switch (proc->state) {
                case PROC_READY: stateStr = "READY"; break;
                case PROC_RUNNING: stateStr = "RUN"; break;
                case PROC_BLOCKED: stateStr = "BLOCK"; break;
                case PROC_SLEEPING: stateStr = "SLEEP"; break;
            }

            PROC_LOG_INFO("Slot %d state=%s pid=%d pri=%d age=%d name='%s'\n",
                          i, stateStr, proc->pid, proc->priority,
                          proc->aging, proc->name);
        }
    }
    PROC_LOG_INFO("===================\n\n");
}
