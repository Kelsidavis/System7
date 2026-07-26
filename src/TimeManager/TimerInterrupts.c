/*
 * TimerInterrupts.c - Timer interrupt handling
 * Based on Inside Macintosh: Operating System Utilities (Time Manager)
 */

#include "SystemTypes.h"
#include "TimeManager/TimeManager.h"
#include "TimeManager/TimeManagerPriv.h"
#include "TimeManager/TimeBase.h"

/* Simple 64-bit division helper (duplicated for freestanding) */
static uint64_t udiv64(uint64_t num, uint64_t den) {
    /* Restoring division. The divisor must be shifted LEFT to align with the
     * dividend before iterating; without that the quotient saturates near the
     * divisor's own magnitude (100/3 returned 3). Kept in sync with the copy in
     * TimeManager/TimeBase.c. */
    if (den == 0) return 0;
    if (num < den) return 0;
    uint64_t quot = 0;
    int shift = 0;
    while (den <= (num >> 1) && shift < 63) {
        den <<= 1;
        shift++;
    }
    while (shift >= 0) {
        if (num >= den) {
            num -= den;
            quot |= (1ULL << shift);
        }
        den >>= 1;
        shift--;
    }
    return quot;
}

/* Hardware timer state (simulated) */
static struct {
    UInt64 nextDeadlineUS;
    Boolean armed;
} gTimerState = {0};

void ProgramNextTimerInterrupt(UInt64 absDeadlineUS) {
    if (absDeadlineUS == 0) {
        /* Disarm timer */
        gTimerState.armed = false;
        gTimerState.nextDeadlineUS = 0;
    } else {
        /* Compute delta and cap to 1 second */
        UnsignedWide now;
        Microseconds(&now);
        UInt64 nowUS = ((UInt64)now.hi << 32) | now.lo;

        int64_t deltaUS = (int64_t)(absDeadlineUS - nowUS);
        if (deltaUS < 0) deltaUS = 0;

        /* Resolution-aware clamping */
        UInt64 resolutionNS = GetTimerResolution();
        UInt64 resolutionUS = udiv64(resolutionNS, 1000);  /* Exact ns to µs */
        if (resolutionUS < 1) resolutionUS = 1;

        /* If delta is below measurable resolution, fire on next ISR pass */
        if ((UInt64)deltaUS < resolutionUS) {
            deltaUS = 0; /* Fire immediately on next ISR */
        }

        /* Cap to 1 second maximum */
        if (deltaUS > MICROSECONDS_PER_SECOND) {
            deltaUS = MICROSECONDS_PER_SECOND;
        }

        gTimerState.nextDeadlineUS = nowUS + deltaUS;
        gTimerState.armed = true;
    }
}

void TimeManager_TimerISR(void) {
    if (!gTimerState.armed) return;
    
    UnsignedWide now;
    Microseconds(&now);
    UInt64 nowUS = ((UInt64)now.hi << 32) | now.lo;
    
    /* Check if timer expired */
    if ((int64_t)(gTimerState.nextDeadlineUS - nowUS) <= 0) {
        gTimerState.armed = false;
        Core_ExpireDue(nowUS);
    }
}
