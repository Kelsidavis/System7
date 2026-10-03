#ifndef TIMEBASE_H
#define TIMEBASE_H

#include "SystemTypes.h"

typedef struct {
    UInt64  counterFrequency; /* Hz */
    UInt32  resolutionNs;     /* nominal ns per tick */
    UInt32  overheadUs;       /* measured Microseconds() overhead */
} TimeBaseInfo;

#ifdef __cplusplus
extern "C" {
#endif

OSErr   InitTimeBase(void);
void    ShutdownTimeBase(void);
uint64_t GetTimerResolution(void);
UInt32   GetTimerOverhead(void);
OSErr    GetTimeBaseInfo(TimeBaseInfo *info);

/* Platform accessors used by TimeBase/Microseconds */
OSErr    InitPlatformTimer(void);
void     ShutdownPlatformTimer(void);
OSErr    GetPlatformTime(UnsignedWide *timeValue);
uint64_t PlatformCounterNow(void);

/* Classic trap */
void Microseconds(UnsignedWide *microTickCount);
UInt32 TickCount(void);

/* Conversions */
OSErr AbsoluteToNanoseconds(UnsignedWide absolute, UnsignedWide *duration);
OSErr NanosecondsToAbsolute(UnsignedWide duration, UnsignedWide *absolute);

#ifdef __cplusplus
}
#endif
#endif /* TIMEBASE_H */
