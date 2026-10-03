/*
 * ProcessAPI.c - Standard Process Manager API
 *
 * Implements the standard Mac OS Process Manager API functions that
 * provide access to process information and control. These are wrapper
 * functions around the internal ProcessManager implementation.
 *
 * Based on Inside Macintosh: Processes
 */

#include "ProcessMgr/ProcessMgr.h"
#include "SystemTypes.h"
#include "System71StdLib.h"

/* Debug logging */
#define PROCAPI_DEBUG 0

#if PROCAPI_DEBUG
#define PROCAPI_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[ProcAPI] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define PROCAPI_LOG(...)
#endif

/*
 * GetCurrentProcess - Get the process serial number of the current process
 *
 * Returns the ProcessSerialNumber of the currently executing process.
 * This is typically the process that currently has CPU time or is
 * frontmost in the application switching environment.
 *
 * Parameters:
 *   currentPSN - Pointer to ProcessSerialNumber to receive current process ID
 *
 * Returns:
 *   noErr (0) on success
 *   procNotFound (-600) if no current process exists
 *
 * Based on Inside Macintosh: Processes, Chapter 2
 */
OSErr GetCurrentProcess(ProcessSerialNumber* currentPSN) {
    if (!currentPSN) {
        PROCAPI_LOG("GetCurrentProcess: NULL pointer\\n");
        return paramErr;
    }

    if (!gCurrentProcess) {
        PROCAPI_LOG("GetCurrentProcess: No current process\\n");
        currentPSN->highLongOfPSN = 0;
        currentPSN->lowLongOfPSN = 0;
        return procNotFound;
    }

    *currentPSN = gCurrentProcess->processID;

    PROCAPI_LOG("GetCurrentProcess: PSN {%lu, %lu}\\n",
                (unsigned long)currentPSN->highLongOfPSN,
                (unsigned long)currentPSN->lowLongOfPSN);

    return noErr;
}

/*
 * GetNextProcess - Get the next process in the process list
 *
 * Iterates through the list of all running processes. Pass a PSN with
 * both fields set to kNoProcess (0) to get the first process, then pass
 * each returned PSN to get the next process in the list.
 *
 * Parameters:
 *   psn - Pointer to ProcessSerialNumber
 *         On input: Previous PSN (or {0,0} for first process)
 *         On output: Next PSN in the process list
 *
 * Returns:
 *   noErr (0) on success
 *   procNotFound (-600) if no more processes exist
 *
 * Based on Inside Macintosh: Processes, Chapter 2
 */
OSErr GetNextProcess(ProcessSerialNumber* psn) {
    ProcessControlBlock* process;
    Boolean foundCurrent = false;

    if (!psn) {
        PROCAPI_LOG("GetNextProcess: NULL pointer\\n");
        return paramErr;
    }

    if (!gProcessQueue) {
        PROCAPI_LOG("GetNextProcess: No process queue\\n");
        return procNotFound;
    }

    /* Check if this is a request for the first process */
    if (psn->highLongOfPSN == kNoProcess && psn->lowLongOfPSN == kNoProcess) {
        /* Return the first process in the queue */
        if (gProcessQueue->queueHead) {
            *psn = gProcessQueue->queueHead->processID;
            PROCAPI_LOG("GetNextProcess: First process PSN {%lu, %lu}\\n",
                        (unsigned long)psn->highLongOfPSN,
                        (unsigned long)psn->lowLongOfPSN);
            return noErr;
        } else {
            PROCAPI_LOG("GetNextProcess: No processes in queue\\n");
            return procNotFound;
        }
    }

    /* Find the current process in the queue and return the next one */
    process = gProcessQueue->queueHead;
    while (process) {
        if (foundCurrent) {
            /* This is the next process after the one we were looking for */
            *psn = process->processID;
            PROCAPI_LOG("GetNextProcess: Next process PSN {%lu, %lu}\\n",
                        (unsigned long)psn->highLongOfPSN,
                        (unsigned long)psn->lowLongOfPSN);
            return noErr;
        }

        /* Check if this is the process we're looking for */
        if (process->processID.highLongOfPSN == psn->highLongOfPSN &&
            process->processID.lowLongOfPSN == psn->lowLongOfPSN) {
            foundCurrent = true;
        }

        process = process->processNextProcess;
    }

    /* Either the process wasn't found, or it was the last one */
    PROCAPI_LOG("GetNextProcess: No more processes\\n");
    return procNotFound;
}

/*
 * SetFrontProcess - Make a process the frontmost application
 *
 * Brings the specified process to the front, making it the active
 * application. This causes the process's windows to come to the front
 * and the process to receive events.
 *
 * Parameters:
 *   psn - Pointer to ProcessSerialNumber of process to bring to front
 *
 * Returns:
 *   noErr (0) on success
 *   procNotFound (-600) if process doesn't exist
 *   paramErr (-50) if psn is NULL
 *
 * Based on Inside Macintosh: Processes, Chapter 2
 */
OSErr SetFrontProcess(const ProcessSerialNumber* psn) {
    OSErr err;

    if (!psn) {
        PROCAPI_LOG("SetFrontProcess: NULL pointer\\n");
        return paramErr;
    }

    PROCAPI_LOG("SetFrontProcess: PSN {%lu, %lu}\\n",
                (unsigned long)psn->highLongOfPSN,
                (unsigned long)psn->lowLongOfPSN);

    /* Call the internal ProcessManager function */
    err = ProcessManager_SetFrontProcess(*psn);

    if (err != noErr) {
        PROCAPI_LOG("SetFrontProcess: Failed with error %d\\n", err);
    }

    return err;
}

/*
 * GetFrontProcess - Get the frontmost process
 *
 * Returns the ProcessSerialNumber of the frontmost (active) application.
 * This is the process that is currently receiving user input.
 *
 * Parameters:
 *   frontPSN - Pointer to ProcessSerialNumber to receive front process ID
 *
 * Returns:
 *   noErr (0) on success
 *   paramErr (-50) if frontPSN is NULL
 *
 * Based on Inside Macintosh: Processes, Chapter 2
 */
OSErr GetFrontProcess(ProcessSerialNumber* frontPSN) {
    if (!frontPSN) {
        PROCAPI_LOG("GetFrontProcess: NULL pointer\\n");
        return paramErr;
    }

    /* Get the front process from the internal ProcessManager */
    *frontPSN = ProcessManager_GetFrontProcess();

    PROCAPI_LOG("GetFrontProcess: PSN {%lu, %lu}\\n",
                (unsigned long)frontPSN->highLongOfPSN,
                (unsigned long)frontPSN->lowLongOfPSN);

    return noErr;
}

/*
 * SameProcess - Compare two process serial numbers
 *
 * Determines if two ProcessSerialNumbers refer to the same process.
 *
 * Parameters:
 *   psn1 - Pointer to first ProcessSerialNumber
 *   psn2 - Pointer to second ProcessSerialNumber
 *   result - Pointer to Boolean to receive comparison result
 *            true if processes are the same, false otherwise
 *
 * Returns:
 *   noErr (0) on success
 *   paramErr (-50) if any pointer is NULL
 *
 * Based on Inside Macintosh: Processes, Chapter 2
 */
OSErr SameProcess(const ProcessSerialNumber* psn1,
                  const ProcessSerialNumber* psn2,
                  Boolean* result) {
    if (!psn1 || !psn2 || !result) {
        PROCAPI_LOG("SameProcess: NULL pointer\\n");
        return paramErr;
    }

    *result = (psn1->highLongOfPSN == psn2->highLongOfPSN &&
               psn1->lowLongOfPSN == psn2->lowLongOfPSN);

    PROCAPI_LOG("SameProcess: {%lu,%lu} vs {%lu,%lu} = %s\\n",
                (unsigned long)psn1->highLongOfPSN,
                (unsigned long)psn1->lowLongOfPSN,
                (unsigned long)psn2->highLongOfPSN,
                (unsigned long)psn2->lowLongOfPSN,
                *result ? "same" : "different");

    return noErr;
}
