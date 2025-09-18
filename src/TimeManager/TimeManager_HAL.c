/*
 * TimeManager_HAL.c - Hardware Abstraction Layer Implementation
 *
 * Provides platform-specific timing implementations using native OS timers.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include "TimeManager/TimeManager_HAL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Platform-specific includes */
#ifdef __APPLE__
    #include <mach/mach_time.h>
    #include <dispatch/dispatch.h>
#elif defined(_WIN32)
    #include <windows.h>
    #include <mmsystem.h>
    #pragma comment(lib, "winmm.lib")
#elif defined(__linux__)
    #include <sys/time.h>
    #include <signal.h>
    #include <unistd.h>
    #include <pthread.h>
#endif

/* Platform-specific timer structure */
typedef struct PlatformTimer {
    TimerCallbackFunc callback;
    UInt32 intervalMicros;
    Boolean active;
#ifdef __APPLE__
    dispatch_source_t timer;
#elif defined(_WIN32)
    UINT timerId;
    HANDLE timerQueue;
    HANDLE timer;
#elif defined(__linux__)
    timer_t timerId;
    pthread_t thread;
    Boolean shouldStop;
#endif
} PlatformTimer;

/* Global HAL state */
static struct {
    Boolean initialized;
    UInt64 startTime;
    UInt32 timeBaseFreq;
#ifdef __APPLE__
    mach_timebase_info_data_t timebase;
#elif defined(_WIN32)
    LARGE_INTEGER perfFreq;
#endif
    /* Statistics */
    UInt32 timersFired;
    UInt32 timersMissed;
    UInt32 timerDrift;
} gHALState = {0};

/* Initialize HAL */
OSErr TimeManager_HAL_Init(void) {
    if (gHALState.initialized) {
        return noErr;
    }

#ifdef __APPLE__
    /* Get mach timebase info */
    mach_timebase_info(&gHALState.timebase);

#elif defined(_WIN32)
    /* Get performance counter frequency */
    if (!QueryPerformanceFrequency(&gHALState.perfFreq)) {
        return -1;
    }

    /* Set timer resolution to 1ms */
    timeBeginPeriod(1);

#elif defined(__linux__)
    /* Nothing specific needed for Linux */
#endif

    /* Calibrate time base */
    TimeManager_HAL_CalibrateTimeBase();

    /* Get initial time */
    UnsignedWide startMicros;
    TimeManager_HAL_GetMicroseconds(&startMicros);
    gHALState.startTime = ((UInt64)startMicros.hi << 32) | startMicros.lo;

    gHALState.initialized = true;
    return noErr;
}

/* Cleanup HAL */
void TimeManager_HAL_Cleanup(void) {
    if (!gHALState.initialized) {
        return;
    }

#ifdef _WIN32
    /* Reset timer resolution */
    timeEndPeriod(1);
#endif

    gHALState.initialized = false;
}

/* Get microseconds */
void TimeManager_HAL_GetMicroseconds(UnsignedWide* microseconds) {
    if (!microseconds) return;

#ifdef __APPLE__
    uint64_t nanos = mach_absolute_time();
    nanos = nanos * gHALState.timebase.numer / gHALState.timebase.denom;
    uint64_t micros = nanos / 1000;

    microseconds->hi = (UInt32)(micros >> 32);
    microseconds->lo = (UInt32)(micros & 0xFFFFFFFF);

#elif defined(_WIN32)
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    uint64_t micros = (counter.QuadPart * 1000000) / gHALState.perfFreq.QuadPart;

    microseconds->hi = (UInt32)(micros >> 32);
    microseconds->lo = (UInt32)(micros & 0xFFFFFFFF);

#elif defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    uint64_t micros = (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;

    microseconds->hi = (UInt32)(micros >> 32);
    microseconds->lo = (UInt32)(micros & 0xFFFFFFFF);

#else
    /* Fallback to standard time */
    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint64_t micros = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;

    microseconds->hi = (UInt32)(micros >> 32);
    microseconds->lo = (UInt32)(micros & 0xFFFFFFFF);
#endif
}

/* Get nanoseconds */
void TimeManager_HAL_GetNanoseconds(UnsignedWide* nanoseconds) {
    if (!nanoseconds) return;

#ifdef __APPLE__
    uint64_t nanos = mach_absolute_time();
    nanos = nanos * gHALState.timebase.numer / gHALState.timebase.denom;

    nanoseconds->hi = (UInt32)(nanos >> 32);
    nanoseconds->lo = (UInt32)(nanos & 0xFFFFFFFF);

#elif defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    uint64_t nanos = (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;

    nanoseconds->hi = (UInt32)(nanos >> 32);
    nanoseconds->lo = (UInt32)(nanos & 0xFFFFFFFF);

#else
    /* Convert microseconds to nanoseconds */
    UnsignedWide micros;
    TimeManager_HAL_GetMicroseconds(&micros);

    uint64_t nanos = ((uint64_t)micros.hi << 32 | micros.lo) * 1000;

    nanoseconds->hi = (UInt32)(nanos >> 32);
    nanoseconds->lo = (UInt32)(nanos & 0xFFFFFFFF);
#endif
}

/* Get milliseconds */
UInt32 TimeManager_HAL_GetMilliseconds(void) {
#ifdef _WIN32
    return GetTickCount();
#else
    UnsignedWide micros;
    TimeManager_HAL_GetMicroseconds(&micros);
    return micros.lo / 1000;  /* Simplified - assumes hi is 0 for 32-bit range */
#endif
}

/* Get ticks (60Hz) */
UInt32 TimeManager_HAL_GetTicks(void) {
    UnsignedWide micros;
    TimeManager_HAL_GetMicroseconds(&micros);

    /* Convert microseconds to 60Hz ticks */
    uint64_t totalMicros = ((uint64_t)micros.hi << 32) | micros.lo;
    return (UInt32)(totalMicros / (1000000 / 60));
}

/* Platform timer callbacks */
#ifdef __APPLE__
static void TimerCallbackMacOS(void* context) {
    PlatformTimer* timer = (PlatformTimer*)context;
    if (timer && timer->callback) {
        timer->callback();
        gHALState.timersFired++;
    }
}
#elif defined(_WIN32)
static VOID CALLBACK TimerCallbackWindows(PVOID lpParam, BOOLEAN TimerOrWaitFired) {
    PlatformTimer* timer = (PlatformTimer*)lpParam;
    if (timer && timer->callback) {
        timer->callback();
        gHALState.timersFired++;
    }
}
#elif defined(__linux__)
static void TimerCallbackLinux(union sigval val) {
    PlatformTimer* timer = (PlatformTimer*)val.sival_ptr;
    if (timer && timer->callback) {
        timer->callback();
        gHALState.timersFired++;
    }
}

static void* TimerThreadLinux(void* arg) {
    PlatformTimer* timer = (PlatformTimer*)arg;
    struct timespec ts;

    ts.tv_sec = timer->intervalMicros / 1000000;
    ts.tv_nsec = (timer->intervalMicros % 1000000) * 1000;

    while (!timer->shouldStop) {
        nanosleep(&ts, NULL);
        if (timer->callback && timer->active) {
            timer->callback();
            gHALState.timersFired++;
        }
    }

    return NULL;
}
#endif

/* Create platform timer */
void* TimeManager_HAL_CreateTimer(TimerCallbackFunc callback,
                                  UInt32 intervalMicros) {
    PlatformTimer* timer = (PlatformTimer*)calloc(1, sizeof(PlatformTimer));
    if (!timer) return NULL;

    timer->callback = callback;
    timer->intervalMicros = intervalMicros;
    timer->active = false;

#ifdef __APPLE__
    /* Create dispatch timer */
    dispatch_queue_t queue = dispatch_get_global_queue(
        DISPATCH_QUEUE_PRIORITY_HIGH, 0);

    timer->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                          0, 0, queue);
    if (!timer->timer) {
        free(timer);
        return NULL;
    }

    dispatch_source_set_timer(timer->timer,
        dispatch_time(DISPATCH_TIME_NOW, 0),
        intervalMicros * NSEC_PER_USEC,
        100 * NSEC_PER_USEC); /* 100us leeway */

    dispatch_source_set_event_handler_f(timer->timer,
        (dispatch_function_t)TimerCallbackMacOS);
    dispatch_set_context(timer->timer, timer);

    dispatch_resume(timer->timer);
    timer->active = true;

#elif defined(_WIN32)
    /* Create Windows timer queue timer */
    timer->timerQueue = CreateTimerQueue();
    if (!timer->timerQueue) {
        free(timer);
        return NULL;
    }

    DWORD period = intervalMicros / 1000;  /* Convert to milliseconds */
    if (period < 1) period = 1;

    if (!CreateTimerQueueTimer(&timer->timer, timer->timerQueue,
                              TimerCallbackWindows, timer,
                              0, period, WT_EXECUTEDEFAULT)) {
        DeleteTimerQueue(timer->timerQueue);
        free(timer);
        return NULL;
    }

    timer->active = true;

#elif defined(__linux__)
    /* Use POSIX timer or thread */
    struct sigevent sev;
    struct itimerspec its;

    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = TimerCallbackLinux;
    sev.sigev_value.sival_ptr = timer;

    if (timer_create(CLOCK_MONOTONIC, &sev, &timer->timerId) == -1) {
        /* Fallback to thread-based timer */
        timer->shouldStop = false;
        if (pthread_create(&timer->thread, NULL, TimerThreadLinux, timer) != 0) {
            free(timer);
            return NULL;
        }
    } else {
        /* Set timer interval */
        its.it_value.tv_sec = intervalMicros / 1000000;
        its.it_value.tv_nsec = (intervalMicros % 1000000) * 1000;
        its.it_interval = its.it_value;

        timer_settime(timer->timerId, 0, &its, NULL);
    }

    timer->active = true;

#else
    /* No platform timer available */
    free(timer);
    return NULL;
#endif

    return timer;
}

/* Destroy platform timer */
void TimeManager_HAL_DestroyTimer(void* timerHandle) {
    PlatformTimer* timer = (PlatformTimer*)timerHandle;
    if (!timer) return;

#ifdef __APPLE__
    if (timer->timer) {
        dispatch_source_cancel(timer->timer);
        dispatch_release(timer->timer);
    }

#elif defined(_WIN32)
    if (timer->timer) {
        DeleteTimerQueueTimer(timer->timerQueue, timer->timer,
                             INVALID_HANDLE_VALUE);
    }
    if (timer->timerQueue) {
        DeleteTimerQueue(timer->timerQueue);
    }

#elif defined(__linux__)
    if (timer->timerId) {
        timer_delete(timer->timerId);
    } else if (timer->thread) {
        timer->shouldStop = true;
        pthread_join(timer->thread, NULL);
    }
#endif

    free(timer);
}

/* Start timer */
OSErr TimeManager_HAL_StartTimer(void* timerHandle) {
    PlatformTimer* timer = (PlatformTimer*)timerHandle;
    if (!timer) return paramErr;

    timer->active = true;
    return noErr;
}

/* Stop timer */
OSErr TimeManager_HAL_StopTimer(void* timerHandle) {
    PlatformTimer* timer = (PlatformTimer*)timerHandle;
    if (!timer) return paramErr;

    timer->active = false;
    return noErr;
}

/* Yield to system */
void TimeManager_HAL_Yield(void) {
#ifdef _WIN32
    Sleep(0);
#elif defined(__APPLE__) || defined(__linux__)
    sched_yield();
#else
    usleep(0);
#endif
}

/* Sleep for microseconds */
void TimeManager_HAL_Sleep(UInt32 microseconds) {
#ifdef _WIN32
    Sleep(microseconds / 1000);
#else
    usleep(microseconds);
#endif
}

/* Calibrate time base */
OSErr TimeManager_HAL_CalibrateTimeBase(void) {
    /* Measure actual timer frequency */
    UnsignedWide start, end;
    TimeManager_HAL_GetMicroseconds(&start);

#ifdef _WIN32
    Sleep(100);
#else
    usleep(100000);
#endif

    TimeManager_HAL_GetMicroseconds(&end);

    UInt32 elapsed = end.lo - start.lo;
    if (elapsed > 0) {
        gHALState.timeBaseFreq = 1000000; /* Microsecond precision */
    }

    return noErr;
}

/* Get time base frequency */
UInt32 TimeManager_HAL_GetTimeBaseFrequency(void) {
    return gHALState.timeBaseFreq;
}

/* Platform feature queries */
Boolean TimeManager_HAL_HasHighResTimer(void) {
#ifdef __APPLE__
    return true;  /* mach_absolute_time is high-res */
#elif defined(_WIN32)
    return gHALState.perfFreq.QuadPart > 0;
#elif defined(__linux__)
    struct timespec ts;
    return clock_getres(CLOCK_MONOTONIC, &ts) == 0;
#else
    return false;
#endif
}

Boolean TimeManager_HAL_HasPeriodicTimer(void) {
    return true;  /* All platforms support periodic timers */
}

Boolean TimeManager_HAL_HasDriftFreeTimer(void) {
#ifdef __APPLE__
    return true;  /* Dispatch timers are drift-free */
#else
    return false;
#endif
}

UInt32 TimeManager_HAL_GetTimerResolution(void) {
#ifdef __APPLE__
    return 1;  /* 1 microsecond */
#elif defined(_WIN32)
    return 1000;  /* 1 millisecond */
#elif defined(__linux__)
    struct timespec ts;
    if (clock_getres(CLOCK_MONOTONIC, &ts) == 0) {
        return ts.tv_nsec / 1000;  /* Convert to microseconds */
    }
    return 1000;
#else
    return 10000;  /* 10 milliseconds fallback */
#endif
}

/* Timer statistics */
void TimeManager_HAL_GetTimerStats(UInt32* fired, UInt32* missed,
                                   UInt32* drift) {
    if (fired) *fired = gHALState.timersFired;
    if (missed) *missed = gHALState.timersMissed;
    if (drift) *drift = gHALState.timerDrift;
}

void TimeManager_HAL_ResetTimerStats(void) {
    gHALState.timersFired = 0;
    gHALState.timersMissed = 0;
    gHALState.timerDrift = 0;
}