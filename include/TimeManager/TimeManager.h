/*
 * TimeManager.h
 *
 * Main Time Manager API for System 7.1 - Portable C Implementation
 *
 * This file provides the main Time Manager interface, converted from the original
 * 68k assembly implementation in OS/TimeMgr/TimeMgr.a
 *
 * The Time Manager provides high-precision timing services, task scheduling,
 * and time-based operations essential for System 7.1 operation.
 */

#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "TimerTypes.h"
#include "MicrosecondTimer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Time Manager Constants ===== */

/* Timer resolution and scaling constants from original implementation */
#define TICKS_PER_SEC           783360      /* VIA Timer clock rate */
#define TICK_SCALE              4           /* Internal time is VIA ticks >> TickScale */
#define USECS_INC               0xFFF2E035  /* 65522.8758μsec (16.16 fixed point) */
#define THRESH_INC              3208        /* = 3208 internal ticks */

/* Time conversion multipliers (from original 68k constants) */
#define MS_TO_INTERNAL          0xC3D70A3E  /* msec to internal time multiplier */
#define US_TO_INTERNAL          0x0C88A47F  /* μsec to internal time multiplier */
#define INTERNAL_TO_MS          0x053A8FE6  /* internal time to msec multiplier */
#define INTERNAL_TO_US          0xA36610BC  /* internal time to μsec multiplier */

/* Fraction bits for time conversions */
#define MS_TO_INT_FRACT_BITS    26          /* fraction bits in ms conversion */
#define US_TO_INT_FRACT_BITS    32          /* fraction bits in μs conversion */
#define INT_TO_MS_FRACT_BITS    32          /* fraction bits in ms output */
#define INT_TO_US_FRACT_BITS    27          /* fraction bits in μs output */

/* Task flags */
#define TASK_ACTIVE_BIT         7           /* high bit of QType is active flag */
#define EXTENDED_TMTASK_BIT     6           /* indicates extended TmTask record */
#define T2_INT_BIT              5           /* VIA Timer 2 interrupt bit */

/* Error codes */
#define NO_ERR                  0           /* no error */

/* Maximum timer values */
#define MAX_TIMER_RANGE         0x0000FFFF  /* max range of VIA timer */

/* ===== Time Manager Core Functions ===== */

/**
 * Initialize the Time Manager
 *
 * Sets up the Time Manager's global data structures, timer interrupt handling,
 * and runtime calibration for processor-speed independence.
 *
 * Must be called during system initialization before any timing services are used.
 *
 * @return OSErr - noErr on success
 */
OSErr InitTimeMgr(void);

/**
 * Shutdown the Time Manager
 *
 * Cleans up Time Manager resources and stops all active timers.
 * Called during system shutdown.
 */
void ShutdownTimeMgr(void);

/**
 * Install Time Manager Task
 *
 * Initializes a Time Manager Task structure for use. In the original implementation,
 * this just marked the task as inactive. The actual queue management is handled
 * by PrimeTime.
 *
 * @param tmTaskPtr - Pointer to TMTask structure to initialize
 * @return OSErr - noErr on success
 */
OSErr InsTime(TMTaskPtr tmTaskPtr);

/**
 * Install Extended Time Manager Task
 *
 * Like InsTime, but for extended TMTask structures that support drift-free
 * fixed-frequency timing.
 *
 * @param tmTaskPtr - Pointer to extended TMTask structure to initialize
 * @return OSErr - noErr on success
 */
OSErr InsXTime(TMTaskPtr tmTaskPtr);

/**
 * Remove Time Manager Task
 *
 * Removes a Time Manager Task from active management. If the task was active
 * and had time remaining, that time is returned in the tmCount field in
 * negated microseconds or positive milliseconds.
 *
 * @param tmTaskPtr - Pointer to TMTask structure to remove
 * @return OSErr - noErr on success
 */
OSErr RmvTime(TMTaskPtr tmTaskPtr);

/**
 * Prime Time Manager Task
 *
 * Schedules a Time Manager Task to execute after a specified delay.
 * The delay can be specified in milliseconds (positive values) or
 * microseconds (negative values - negated).
 *
 * @param tmTaskPtr - Pointer to TMTask structure to schedule
 * @param count - Delay time: positive = milliseconds, negative = negated microseconds
 * @return OSErr - noErr on success
 */
OSErr PrimeTime(TMTaskPtr tmTaskPtr, int32_t count);

/* ===== Time Manager Internal Functions ===== */

/**
 * Freeze Time Manager operations
 *
 * Disables interrupts and reads the current VIA timer state.
 * Used internally to provide atomic access to the timer queue.
 *
 * @param savedSR - Pointer to store saved interrupt state
 * @param timerLow - Pointer to store VIA timer low byte
 */
void FreezeTime(uint16_t *savedSR, uint8_t *timerLow);

/**
 * Thaw Time Manager operations
 *
 * Restarts timer operation and restores interrupt state.
 * Used internally after manipulating the timer queue.
 *
 * @param savedSR - Saved interrupt state to restore
 * @param timerLow - VIA timer low byte from FreezeTime
 */
void ThawTime(uint16_t savedSR, uint8_t timerLow);

/**
 * Timer 2 Interrupt Handler
 *
 * Services timer interrupts and executes completion routines for expired tasks.
 * This is the main timer interrupt service routine.
 */
void Timer2Int(void);

/**
 * Multiply and Merge utility
 *
 * Performs 32-bit by 32-bit multiplication with 64-bit intermediate result,
 * then merges selected bits to produce final 32-bit result.
 * Used for high-precision time conversions.
 *
 * @param multiplicand - First operand
 * @param multiplier - Second operand
 * @param mergeMask - Mask for selecting result bits
 * @return uint32_t - Merged result
 */
uint32_t MultAndMerge(uint32_t multiplicand, uint32_t multiplier, uint32_t mergeMask);

/* ===== Time Manager Status Functions ===== */

/**
 * Get current Time Manager status
 *
 * @return bool - true if Time Manager is initialized and running
 */
bool IsTimeMgrActive(void);

/**
 * Get number of active timer tasks
 *
 * @return int32_t - Number of tasks currently in the timer queue
 */
int32_t GetActiveTaskCount(void);

/**
 * Get current backlog time
 *
 * @return uint32_t - Current backlog in internal time units
 */
uint32_t GetBacklogTime(void);

/* ===== Time Conversion Utilities ===== */

/**
 * Convert milliseconds to internal time format
 *
 * @param milliseconds - Time in milliseconds
 * @return uint32_t - Time in internal format
 */
uint32_t MillisecondsToInternal(uint32_t milliseconds);

/**
 * Convert microseconds to internal time format
 *
 * @param microseconds - Time in microseconds
 * @return uint32_t - Time in internal format
 */
uint32_t MicrosecondsToInternal(uint32_t microseconds);

/**
 * Convert internal time to milliseconds
 *
 * @param internal - Time in internal format
 * @return uint32_t - Time in milliseconds
 */
uint32_t InternalToMilliseconds(uint32_t internal);

/**
 * Convert internal time to microseconds
 *
 * @param internal - Time in internal format
 * @return uint32_t - Time in microseconds
 */
uint32_t InternalToMicroseconds(uint32_t internal);

#ifdef __cplusplus
}
#endif

#endif /* TIMEMANAGER_H */