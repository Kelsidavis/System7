/*
 * TimeBase.c - Time base calibration and conversion
 * Based on Inside Macintosh: Operating System Utilities (Time Manager)
 */

#include "SystemTypes.h"
#include "System71StdLib.h"   /* udiv64 - no libgcc in the freestanding build */
#include "TimeManager/TimeBase.h"
#include "TimeManager/TimeManager.h"
#ifdef __i386__
#include "Platform/include/io.h"  /* hal_inb/hal_outb for PIT calibration */
#endif

/* Simple 64-bit division for 32-bit builds */
/* Time base state */
static struct {
    uint64_t bootCounter;
    uint64_t counterFreq;     /* Hz */
    uint64_t nsPerCount_32_32; /* 32.32 fixed point */
    uint32_t usPerCount_16_16; /* 16.16 fixed point */
    uint32_t resolutionNs;
    uint32_t overheadUs;
    Boolean  initialized;
} gTimeBase = {0};

/* Forward declarations */
UInt32 TickCount(void);

/* External functions */
extern void serial_puts(const char* s);

#ifdef __i386__
/* x86 CPUID support for frequency detection */
static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                        uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}
#endif

/* Accurate frequency calibration */
static void CalibrateFrequencyAccurate(void) {
    uint64_t freq = 0;

#ifdef __i386__
    /* Try CPUID leaf 0x15 for TSC frequency */
    uint32_t max_leaf, eax, ebx, ecx, edx;
    cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);

    if (max_leaf >= 0x15) {
        cpuid(0x15, 0, &eax, &ebx, &ecx, &edx);
        if (eax != 0 && ebx != 0) {
            /* TSC frequency = core_crystal_hz * ebx / eax */
            if (ecx != 0) {
                freq = udiv64(((uint64_t)ecx * ebx), eax);
                serial_puts("[TimeBase] TSC frequency from CPUID 0x15\n");
            }
        }
    }

    /* Fallback to CPUID leaf 0x16 */
    if (freq == 0 && max_leaf >= 0x16) {
        cpuid(0x16, 0, &eax, &ebx, &ecx, &edx);
        if (eax != 0) {
            freq = (uint64_t)eax * 1000000; /* Base MHz to Hz */
            serial_puts("[TimeBase] TSC frequency from CPUID 0x16\n");
        }
    }
#endif

#ifdef __aarch64__
    /* Read CNTFRQ_EL0 for timer frequency */
    uint64_t cntfrq;
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(cntfrq));
    if (cntfrq != 0) {
        freq = cntfrq;
        serial_puts("[TimeBase] Timer frequency from CNTFRQ_EL0\n");
    }
#endif

#ifdef __i386__
    /* Calibrate against PIT channel 2.
     *
     * CPUID leaves 0x15 and 0x16 only exist from Skylake onwards, so every
     * older machine - a Pentium 3 or a Core 2, precisely the hardware this
     * project targets - falls straight past both. What used to catch them was a
     * circular fallback that calibrated the time base against TickCount(), even
     * though TickCount() is derived from Microseconds(), which needs the very
     * time base being calibrated. It could never advance, so it always timed out
     * and fabricated 1 MHz. The real TSC runs at 1-3 GHz, so every derived
     * measurement was then wrong by three orders of magnitude, silently: not
     * just TickCount, but Time Manager delays and every timeout expressed in
     * ticks.
     *
     * PIT channel 2 avoids the circularity entirely. It is driven by a fixed
     * 1.193182 MHz crystal, is present on every x86, and can be gated by hand
     * without interrupts - so it works this early in boot and needs nothing
     * already calibrated. */
    if (freq == 0) {
        serial_puts("[TimeBase] Calibrating TSC against PIT channel 2...\n");

        const uint32_t PIT_HZ = 1193182u;
        const uint16_t count  = 59659u;   /* ~50ms: 0.05 * 1193182 */

        /* Port 0x61: bit 0 gates channel 2 on, bit 1 would route it to the
         * speaker - keep that clear so calibration stays silent. */
        uint8_t port61 = hal_inb(0x61);
        hal_outb(0x61, (uint8_t)((port61 & ~0x02) | 0x01));

        /* Channel 2, lobyte/hibyte, mode 0 (interrupt on terminal count) */
        hal_outb(0x43, 0xB0);
        hal_outb(0x42, (uint8_t)(count & 0xFF));
        hal_outb(0x42, (uint8_t)(count >> 8));

        /* Restart the counter by toggling its gate */
        port61 = hal_inb(0x61);
        hal_outb(0x61, (uint8_t)(port61 & ~0x01));
        hal_outb(0x61, (uint8_t)(port61 | 0x01));

        uint64_t startCounter = PlatformCounterNow();

        /* Bit 5 of port 0x61 is channel 2's output: it goes high at terminal
         * count. Bound the wait so a machine that never raises it cannot hang
         * the boot. */
        uint32_t guard = 0;
        while (!(hal_inb(0x61) & 0x20)) {
            if (++guard > 100000000u) {
                serial_puts("[TimeBase] PIT calibration timed out\n");
                break;
            }
        }

        uint64_t endCounter = PlatformCounterNow();

        /* Leave the gate as we found it */
        hal_outb(0x61, (uint8_t)(port61 & ~0x01));

        if (guard <= 100000000u && endCounter > startCounter) {
            uint64_t delta = endCounter - startCounter;
            /* freq = delta / (count / PIT_HZ) = delta * PIT_HZ / count.
             * Divide first to keep the intermediate away from 64-bit overflow. */
            freq = udiv64(delta, count) * PIT_HZ;
            serial_puts("[TimeBase] TSC frequency calibrated from PIT\n");
        }
    }
#endif

    /* Last resort: calibrate against TickCount (60 Hz).
     * Retained only as a final fallback for platforms with no PIT; it is
     * self-referential and expected to time out where it is reached. */
    if (freq == 0) {
        serial_puts("[TimeBase] Calibrating against TickCount...\n");

        /* Wait for tick boundary with timeout */
        UInt32 startTick = TickCount();
        uint64_t timeoutStart = PlatformCounterNow();
        const uint64_t TIMEOUT_CYCLES = 1000000000;  /* ~1s at 1GHz */

        while (TickCount() == startTick) {
            if (PlatformCounterNow() - timeoutStart > TIMEOUT_CYCLES) {
                serial_puts("[TimeBase] TickCount boundary timeout\n");
                freq = 1000000;  /* Default 1MHz */
                break;
            }
        }

        if (freq == 0) {
            startTick = TickCount();
            uint64_t startCounter = PlatformCounterNow();

            /* Wait N ticks (30 = ~500ms) with timeout */
            const UInt32 N = 30;
            UInt32 endTick = startTick + N;
            while (TickCount() < endTick) {
                if (PlatformCounterNow() - startCounter > TIMEOUT_CYCLES * 2) {
                    serial_puts("[TimeBase] TickCount wait timeout\n");
                    freq = 1000000;  /* Default 1MHz */
                    break;
                }
            }

            if (freq == 0) {
                uint64_t endCounter = PlatformCounterNow();
                uint64_t deltaCounter = endCounter - startCounter;

                /* Sanity check: at least some cycles passed */
                if (deltaCounter > 0) {
                    /* Reorder to avoid overflow: divide first, then multiply */
                    freq = udiv64(deltaCounter, N) * 60;
                    serial_puts("[TimeBase] Calibrated frequency from TickCount\n");
                } else {
                    freq = 1000000;  /* Default if no cycles measured */
                }
            }
        }
    }

    /* Report the calibrated rate unconditionally. On unfamiliar hardware a
     * wrong time base is invisible in behaviour but poisons every timeout in
     * the system, so the number itself needs to be on the wire. */
    {
        uint32_t mhz = (uint32_t)udiv64(freq, 1000000u);
        char buf[16];
        int i = 0;
        if (mhz == 0) {
            buf[i++] = '0';
        } else {
            char tmp[12];
            int n = 0;
            while (mhz > 0 && n < 11) { tmp[n++] = (char)('0' + (mhz % 10)); mhz /= 10; }
            while (n > 0) buf[i++] = tmp[--n];
        }
        buf[i] = '\0';
        serial_puts("[TimeBase] counter frequency = ");
        serial_puts(buf);
        serial_puts(" MHz\n");
    }

    /* Validate and clamp frequency */
    if (freq < 1000000) {
        freq = 1000000;  /* Min 1MHz */
        serial_puts("[TimeBase] Clamped to minimum 1MHz\n");
    } else if (freq > 10000000000ULL) {
        freq = 10000000000ULL;  /* Max 10GHz */
        serial_puts("[TimeBase] Clamped to maximum 10GHz\n");
    }

    gTimeBase.counterFreq = freq;
}

OSErr InitTimeBase(void) {
    OSErr err = InitPlatformTimer();
    if (err != noErr) return err;

    /* Get boot counter early */
    gTimeBase.bootCounter = PlatformCounterNow();

    /* Accurate frequency calibration */
    CalibrateFrequencyAccurate();
    
    /* Compute conversion factors using software division */
    gTimeBase.nsPerCount_32_32 = udiv64((NANOSECONDS_PER_SECOND << 32), gTimeBase.counterFreq);
    gTimeBase.usPerCount_16_16 = (uint32_t)udiv64((MICROSECONDS_PER_SECOND << 16), gTimeBase.counterFreq);

    /* Resolution is max of 1ns or actual timer resolution */
    uint32_t actualRes = (uint32_t)udiv64(NANOSECONDS_PER_SECOND, gTimeBase.counterFreq);
    gTimeBase.resolutionNs = actualRes > 0 ? actualRes : 1;

    /* Measure overhead empirically */
    {
        UnsignedWide times[16];
        uint32_t deltas[15];
        const int K = 16;

        /* Warm up */
        for (int i = 0; i < 4; i++) {
            Microseconds(&times[0]);
        }

        /* Measure K consecutive calls */
        for (int i = 0; i < K; i++) {
            Microseconds(&times[i]);
        }

        /* Calculate deltas */
        uint32_t sum = 0;
        for (int i = 1; i < K; i++) {
            uint32_t delta = times[i].lo - times[i-1].lo;
            deltas[i-1] = delta;
            sum += delta;
        }

        /* Use median to reduce outliers */
        /* Simple selection sort on K-1 delta entries (deltas[0..K-2]) */
        for (int i = 0; i < K-2; i++) {
            for (int j = i+1; j < K-1; j++) {
                if (deltas[i] > deltas[j]) {
                    uint32_t tmp = deltas[i];
                    deltas[i] = deltas[j];
                    deltas[j] = tmp;
                }
            }
        }

        gTimeBase.overheadUs = deltas[(K-1)/2]; /* Median */
        if (gTimeBase.overheadUs == 0) {
            gTimeBase.overheadUs = 1; /* Minimum 1us */
        }
    }

    gTimeBase.initialized = true;
    
    return noErr;
}

void ShutdownTimeBase(void) {
    gTimeBase.initialized = false;
    ShutdownPlatformTimer();
}

uint64_t GetTimerResolution(void) {
    return gTimeBase.resolutionNs;
}

UInt32 GetTimerOverhead(void) {
    return gTimeBase.overheadUs;
}

OSErr GetTimeBaseInfo(TimeBaseInfo *info) {
    if (!info) return paramErr;
    if (!gTimeBase.initialized) return tmNotActive;
    
    info->counterFrequency = gTimeBase.counterFreq;
    info->resolutionNs = gTimeBase.resolutionNs;
    info->overheadUs = gTimeBase.overheadUs;
    return noErr;
}

void Microseconds(UnsignedWide *microTickCount) {
    if (!microTickCount) return;
    if (!gTimeBase.initialized) {
        microTickCount->hi = 0;
        microTickCount->lo = 0;
        return;
    }

    uint64_t now = PlatformCounterNow();
    uint64_t delta = now - gTimeBase.bootCounter;

    /* Convert to microseconds using 16.16 fixed point */
    uint64_t us = (delta * gTimeBase.usPerCount_16_16) >> 16;

    microTickCount->hi = (UInt32)(us >> 32);
    microTickCount->lo = (UInt32)(us & 0xFFFFFFFF);
}

/**
 * TickCount - Returns 60 Hz ticks since boot (Classic Mac OS API)
 *
 * Implements the classic Mac OS TickCount() function which returns the number
 * of ticks (1/60th second intervals) since system startup. Uses Microseconds()
 * as the time source for accuracy and consistency with Time Manager.
 *
 * @return Number of ticks since boot (wraps at 2^32)
 */
UInt32 TickCount(void) {
    static UInt64 s_bootUS = 0;     /* microseconds at first call (boot epoch) */
    static UInt32 s_last   = 0;     /* last value returned, for monotonicity */

    UnsignedWide now;
    Microseconds(&now);
    UInt64 nowUS = ((UInt64)now.hi << 32) | now.lo;

    if (s_bootUS == 0) {
        /* First call: establish boot epoch so we start from ~0 ticks */
        s_bootUS = nowUS;
    }

    /* 1 tick = 1/60 s ≈ 16,666.666… µs. Use 16667 for integer stability */
    const UInt64 usPerTick = 16667ULL;
    UInt64 deltaUS = nowUS - s_bootUS;
    UInt32 ticks = (UInt32)udiv64(deltaUS, usPerTick);   /* wraps naturally at 2^32 */

    /* Guard against tiny timer regressions (shouldn't happen, but cheap insurance) */
    if (ticks < s_last) ticks = s_last;
    s_last = ticks;
    return ticks;
}

OSErr AbsoluteToNanoseconds(UnsignedWide absolute, UnsignedWide *duration) {
    if (!duration) return paramErr;
    
    uint64_t counts = ((uint64_t)absolute.hi << 32) | absolute.lo;
    uint64_t ns = (counts * gTimeBase.nsPerCount_32_32) >> 32;
    
    duration->hi = (UInt32)(ns >> 32);
    duration->lo = (UInt32)(ns & 0xFFFFFFFF);
    return noErr;
}

OSErr NanosecondsToAbsolute(UnsignedWide duration, UnsignedWide *absolute) {
    if (!absolute) return paramErr;

    uint64_t ns = ((uint64_t)duration.hi << 32) | duration.lo;
    uint64_t counts = udiv64((ns << 32), gTimeBase.nsPerCount_32_32);
    
    absolute->hi = (UInt32)(counts >> 32);
    absolute->lo = (UInt32)(counts & 0xFFFFFFFF);
    return noErr;
}
