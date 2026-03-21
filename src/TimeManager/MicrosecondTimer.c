/*
 * MicrosecondTimer.c - Microsecond timing utilities
 * Based on Inside Macintosh: Operating System Utilities (Time Manager)
 */

#include "SystemTypes.h"
#include "TimeManager/MicrosecondTimer.h"
#include "TimeManager/TimeBase.h"

/* Simple 64-bit division for 32-bit builds */
static uint64_t udiv64(uint64_t num, uint64_t den) {
    if (den == 0) return 0;
    uint64_t quot = 0;
    uint64_t bit = 1ULL << 63;
    while (bit && !(den & bit)) bit >>= 1;
    while (bit) {
        if (num >= den) {
            num -= den;
            quot |= bit;
        }
        den >>= 1;
        bit >>= 1;
    }
    return quot;
}

OSErr AddWideTime(UnsignedWide a, UnsignedWide b, UnsignedWide *out) {
    if (!out) return paramErr;
    
    uint64_t sum = ((uint64_t)a.hi << 32 | a.lo) + 
                   ((uint64_t)b.hi << 32 | b.lo);
    
    out->hi = (UInt32)(sum >> 32);
    out->lo = (UInt32)(sum & 0xFFFFFFFF);
    return noErr;
}

OSErr SubtractWideTime(UnsignedWide a, UnsignedWide b, UnsignedWide *out) {
    if (!out) return paramErr;
    
    uint64_t diff = ((uint64_t)a.hi << 32 | a.lo) - 
                    ((uint64_t)b.hi << 32 | b.lo);
    
    out->hi = (UInt32)(diff >> 32);
    out->lo = (UInt32)(diff & 0xFFFFFFFF);
    return noErr;
}

OSErr MicrosecondDelay(UInt32 microseconds) {
    if (microseconds == 0) return noErr;

    UnsignedWide start, now;
    Microseconds(&start);

    uint64_t targetUs = ((uint64_t)start.hi << 32 | start.lo) + microseconds;
    
    if (microseconds < 1000) {
        /* Busy wait for short delays */
        uint64_t startCycles = PlatformCounterNow();
        TimeBaseInfo info;
        GetTimeBaseInfo(&info);
        uint64_t targetCycles = startCycles +
            udiv64(microseconds * info.counterFrequency, MICROSECONDS_PER_SECOND);
        while (PlatformCounterNow() < targetCycles) {
            /* Spin */
        }
    } else {
        /* Poll Microseconds for longer delays */
        do {
            Microseconds(&now);
        } while (((uint64_t)now.hi << 32 | now.lo) < targetUs);
    }
    
    return noErr;
}

OSErr NanosecondDelay(uint64_t nanoseconds) {
    if (nanoseconds == 0) return noErr;
    
    if (nanoseconds < NANOSECONDS_PER_MICROSECOND) {
        /* Sub-microsecond: direct counter spin */
        TimeBaseInfo info;
        GetTimeBaseInfo(&info);
        uint64_t cycles = udiv64(nanoseconds * info.counterFrequency, NANOSECONDS_PER_SECOND);
        uint64_t target = PlatformCounterNow() + cycles;
        while (PlatformCounterNow() < target) {
            /* Spin */
        }
    } else {
        /* Use microsecond delay */
        return MicrosecondDelay((UInt32)udiv64(nanoseconds, NANOSECONDS_PER_MICROSECOND));
    }
    
    return noErr;
}

OSErr StartPerformanceTimer(UnsignedWide *start) {
    if (!start) return paramErr;
    Microseconds(start);
    return noErr;
}

OSErr EndPerformanceTimer(UnsignedWide start, UnsignedWide *elapsed) {
    if (!elapsed) return paramErr;
    
    UnsignedWide now;
    Microseconds(&now);
    
    return SubtractWideTime(now, start, elapsed);
}
