/*
 * rtc.c - CMOS/RTC access for x86
 */

#include "rtc.h"
#include "Platform/include/io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define RTC_REG_SECONDS 0x00
#define RTC_REG_MINUTES 0x02
#define RTC_REG_HOURS   0x04
#define RTC_REG_DAY     0x07
#define RTC_REG_MONTH   0x08
#define RTC_REG_YEAR    0x09
#define RTC_REG_STATUS_A 0x0A
#define RTC_REG_STATUS_B 0x0B

static inline uint8_t cmos_read(uint8_t reg) {
    hal_outb(CMOS_ADDR, reg);
    return hal_inb(CMOS_DATA);
}

static inline void cmos_write(uint8_t reg, uint8_t value) {
    hal_outb(CMOS_ADDR, reg);
    hal_outb(CMOS_DATA, value);
}

static bool rtc_updating(void) {
    return (cmos_read(RTC_REG_STATUS_A) & 0x80) != 0;
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)((bcd & 0x0F) + ((bcd / 16) * 10));
}

void rtc_init(void) {
    uint8_t status_b = cmos_read(RTC_REG_STATUS_B);
    /* Set 24-hour mode and binary if not already set */
    status_b |= 0x02; /* 24-hour */
    status_b |= 0x04; /* Binary */
    cmos_write(RTC_REG_STATUS_B, status_b);
}

bool rtc_read_datetime(rtc_datetime_t *out) {
    if (!out) {
        return false;
    }

    /* Wait for update to complete */
    for (int i = 0; i < 1000 && rtc_updating(); i++) {
        /* spin */
    }

    uint8_t status_b = cmos_read(RTC_REG_STATUS_B);
    bool bcd = (status_b & 0x04) == 0;

    uint8_t sec = cmos_read(RTC_REG_SECONDS);
    uint8_t min = cmos_read(RTC_REG_MINUTES);
    uint8_t hour = cmos_read(RTC_REG_HOURS);
    uint8_t day = cmos_read(RTC_REG_DAY);
    uint8_t month = cmos_read(RTC_REG_MONTH);
    uint8_t year = cmos_read(RTC_REG_YEAR);

    if (bcd) {
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
        day = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year = bcd_to_bin(year);
    }

    out->second = sec;
    out->minute = min;
    out->hour = hour;
    out->day = day;
    out->month = month;
    out->year = (uint16_t)(2000 + year);
    return true;
}
