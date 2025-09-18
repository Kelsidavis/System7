/*
 * TimerTypes.h
 *
 * Timer Task and Time Data Structures for System 7.1 Time Manager
 *
 * This file defines the data structures used by the Time Manager,
 * converted from the original 68k assembly definitions.
 */

#ifndef TIMERTYPES_H
#define TIMERTYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Forward Declarations ===== */
typedef struct TMTask TMTask;
typedef TMTask *TMTaskPtr;

/* ===== Timer Procedure Types ===== */

/**
 * Timer Completion Procedure
 *
 * Called when a timer task expires. The original implementation passes:
 * - A0: Address of the service routine itself
 * - A1: Pointer to the expired TMTask structure
 *
 * In the portable version:
 * @param tmTaskPtr - Pointer to the expired TMTask structure
 */
typedef void (*TimerProcPtr)(TMTaskPtr tmTaskPtr);

/* ===== TMTask Structure ===== */

/**
 * Time Manager Task Record
 *
 * This structure represents a timer task, based on the original
 * TMTask record from SysEqu.a. The structure supports both standard
 * and extended timer tasks.
 */
struct TMTask {
    /* Queue link fields (compatible with Mac OS queue structures) */
    TMTaskPtr       qLink;          /* Pointer to next task in queue */
    uint16_t        qType;          /* Queue type and flags */

    /* Timer task fields */
    TimerProcPtr    tmAddr;         /* Timer completion procedure (NULL = no procedure) */
    int32_t         tmCount;        /* Delay time or remaining time */
    uint32_t        tmWakeUp;       /* Wake-up time for extended tasks (0 = not extended) */
    uint32_t        tmReserved;     /* Reserved field */

    /* Implementation-specific fields (not in original) */
    bool            isActive;       /* True if task is currently active */
    bool            isExtended;     /* True if this is an extended task */
    uint32_t        originalCount;  /* Original delay time for reference */
    void           *userData;       /* User data pointer for convenience */
};

/* ===== Time Manager Private Data ===== */

/**
 * Time Manager Private Storage
 *
 * This structure contains the Time Manager's internal state,
 * based on the TimeMgrPrivate record from the original implementation.
 */
typedef struct TimeMgrPrivate {
    /* Active task queue management */
    TMTaskPtr       activePtr;      /* Pointer to soonest active request */
    uint8_t         timerAdjust;    /* Number of VIA ticks used loading timer */
    uint8_t         timerLowSave;   /* Low byte of VIA timer from last FreezeTime */
    uint16_t        retryAdjust;    /* Number of VIA ticks for underflow retry */
    uint32_t        currentTime;    /* Number of virtual ticks since boot */
    uint32_t        backLog;        /* Number of virtual ticks of ready tasks */

    /* Microsecond counter (order-dependent fields) */
    uint32_t        highUSecs;      /* High 32 bits of microsecond count */
    uint32_t        lowUSecs;       /* Low 32 bits of microsecond count */
    uint16_t        fractUSecs;     /* 16 bit fractional microsecond count */
    uint16_t        curTimeThresh;  /* CurrentTime threshold for updating μsec count */

    /* Modern implementation fields */
    bool            initialized;    /* True if Time Manager is initialized */
    void           *timerThread;    /* Platform-specific timer thread handle */
    void           *timerMutex;     /* Platform-specific mutex for thread safety */
    uint64_t        startTime;      /* System start time for time base calculations */
} TimeMgrPrivate;

/* ===== Time Base Information ===== */

/**
 * Time Base Information Record
 *
 * Contains information about the system time base, used by
 * high-resolution timing functions.
 */
typedef struct TimeBaseInfo {
    uint32_t        numerator;      /* Numerator for time base conversion */
    uint32_t        denominator;    /* Denominator for time base conversion */
    uint32_t        minDelta;       /* Minimum measurable time difference */
    uint32_t        maxDelta;       /* Maximum single measurement time */
} TimeBaseInfo;

/* ===== Time Conversion Structures ===== */

/**
 * 64-bit Time Value
 *
 * Used for high-precision time measurements and microsecond counters.
 */
typedef union UnsignedWide {
    struct {
        uint32_t    hi;             /* High 32 bits */
        uint32_t    lo;             /* Low 32 bits */
    } parts;
    uint64_t        value;          /* Full 64-bit value */
} UnsignedWide;

/**
 * Fixed-Point Time Value
 *
 * Used for fractional time calculations in the Time Manager.
 */
typedef struct FixedTime {
    uint32_t        integer;        /* Integer part */
    uint16_t        fraction;       /* Fractional part (16-bit) */
} FixedTime;

/* ===== Task State Constants ===== */

/* QType flag bits */
#define QTASK_ACTIVE_FLAG       0x8000      /* Task is currently active */
#define QTASK_EXTENDED_FLAG     0x4000      /* Extended TMTask record */

/* QType base values */
#define QTASK_TIMER_TYPE        1           /* Standard timer task type */
#define QTASK_EXTENDED_TYPE     2           /* Extended timer task type */

/* ===== Timing Constants ===== */

/* Time conversion factors */
#define MICROSECONDS_PER_SECOND     1000000
#define MILLISECONDS_PER_SECOND     1000
#define MICROSECONDS_PER_MILLISEC   1000

/* Maximum timer values */
#define MAX_MILLISECONDS_DELAY      86400000    /* ~1 day in milliseconds */
#define MAX_MICROSECONDS_DELAY      2147483647  /* Maximum positive int32_t */

/* ===== Utility Macros ===== */

/**
 * Check if a TMTask is active
 */
#define IS_TMTASK_ACTIVE(task)      ((task)->qType & QTASK_ACTIVE_FLAG)

/**
 * Check if a TMTask is extended
 */
#define IS_TMTASK_EXTENDED(task)    ((task)->qType & QTASK_EXTENDED_FLAG)

/**
 * Set TMTask active flag
 */
#define SET_TMTASK_ACTIVE(task)     ((task)->qType |= QTASK_ACTIVE_FLAG)

/**
 * Clear TMTask active flag
 */
#define CLEAR_TMTASK_ACTIVE(task)   ((task)->qType &= ~QTASK_ACTIVE_FLAG)

/**
 * Set TMTask extended flag
 */
#define SET_TMTASK_EXTENDED(task)   ((task)->qType |= QTASK_EXTENDED_FLAG)

/**
 * Clear TMTask extended flag
 */
#define CLEAR_TMTASK_EXTENDED(task) ((task)->qType &= ~QTASK_EXTENDED_FLAG)

/**
 * Convert milliseconds to microseconds (with overflow check)
 */
#define MS_TO_US(ms)                ((ms) <= MAX_MILLISECONDS_DELAY / 1000 ? \
                                     (ms) * MICROSECONDS_PER_MILLISEC : MAX_MICROSECONDS_DELAY)

/**
 * Convert microseconds to milliseconds (with rounding)
 */
#define US_TO_MS(us)                (((us) + MICROSECONDS_PER_MILLISEC / 2) / MICROSECONDS_PER_MILLISEC)

#ifdef __cplusplus
}
#endif

#endif /* TIMERTYPES_H */