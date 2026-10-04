#ifndef SYSTEM71_DATETIME_H
#define SYSTEM71_DATETIME_H

#include "SystemTypes.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Seconds between the Mac epoch (1904) and the Unix epoch (1970). */
#define MAC_UNIX_EPOCH_OFFSET 2082844800UL

UInt32 DateTime_Current(void);
UInt32 DateTime_FromUnix(time_t unixTime);
time_t DateTime_ToUnix(UInt32 macTime);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM71_DATETIME_H */
