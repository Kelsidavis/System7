/*
 * time.h - Minimal time handling for bare-metal ARM64
 */

#ifndef _TIME_H_
#define _TIME_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Time type - seconds since Unix epoch */
typedef long time_t;

/* Broken-down time structure */
struct tm {
    int tm_sec;      /* Seconds (0-59) */
    int tm_min;      /* Minutes (0-59) */
    int tm_hour;     /* Hours (0-23) */
    int tm_mday;     /* Day of month (1-31) */
    int tm_mon;      /* Month (0-11) */
    int tm_year;     /* Year since 1900 */
    int tm_wday;     /* Day of week (0-6, Sunday = 0) */
    int tm_yday;     /* Day of year (0-365) */
    int tm_isdst;    /* Daylight saving time flag */
};

/* Time functions */
time_t time(time_t *t);
time_t mktime(struct tm *tm);

#ifdef __cplusplus
}
#endif

#endif /* _TIME_H_ */
