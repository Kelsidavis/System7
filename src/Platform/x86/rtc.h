#ifndef X86_RTC_H
#define X86_RTC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_datetime_t;

void rtc_init(void);
bool rtc_read_datetime(rtc_datetime_t *out);

#endif /* X86_RTC_H */
