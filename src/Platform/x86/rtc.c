/*
 * rtc.c - CMOS/RTC access for x86
 *
 * This code is deliberately READ-ONLY.
 *
 * rtc_init() used to write CMOS Status Register B to force 24-hour and binary
 * mode. CMOS is battery-backed, so that was a permanent modification to the
 * host machine's RTC configuration that outlived the boot. Nearly every PC BIOS
 * keeps the RTC in BCD; after we flipped the DM bit to binary, the firmware
 * would read the counters back on the next power-up still expecting BCD, decode
 * garbage (or an invalid BCD nibble), and typically respond by declaring a CMOS
 * error and resetting the stored date/time and settings to defaults. From the
 * outside that looks exactly like the machine clearing its clock because we
 * booted this OS on it.
 *
 * The write was also unnecessary: the read path already inspects Status
 * Register B to discover the format. It now handles every combination the
 * hardware may legitimately be left in - BCD or binary, 12- or 24-hour - and
 * touches nothing.
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

#define STATUS_A_UPDATE_IN_PROGRESS 0x80
#define STATUS_B_24_HOUR            0x02  /* set = 24-hour, clear = 12-hour */
#define STATUS_B_BINARY             0x04  /* set = binary,  clear = BCD     */

#define HOURS_PM_FLAG               0x80  /* in 12-hour mode only */

static inline uint8_t cmos_read(uint8_t reg) {
    /* Bit 7 of the index port is the NMI mask. We leave it clear, matching the
     * state firmware hands us; we never disable NMI, so there is nothing to
     * preserve. Note the index port is write-only on essentially all chipsets,
     * so the current mask cannot be read back and restored anyway. */
    hal_outb(CMOS_ADDR, reg);
    return hal_inb(CMOS_DATA);
}

static bool rtc_updating(void) {
    return (cmos_read(RTC_REG_STATUS_A) & STATUS_A_UPDATE_IN_PROGRESS) != 0;
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)((bcd & 0x0F) + ((bcd >> 4) * 10));
}

void rtc_init(void) {
    /* Intentionally empty.
     *
     * Do not write CMOS here. Whatever format the firmware left the RTC in is
     * the format the firmware expects to read back after we are gone, and
     * rtc_read_datetime() adapts to all of them. See the file header. */
}

/* Read the six time registers once. Caller is responsible for update-in-progress
 * handling and for confirming the values are stable. */
static void rtc_read_raw(rtc_datetime_t *out) {
    out->second = cmos_read(RTC_REG_SECONDS);
    out->minute = cmos_read(RTC_REG_MINUTES);
    out->hour   = cmos_read(RTC_REG_HOURS);
    out->day    = cmos_read(RTC_REG_DAY);
    out->month  = cmos_read(RTC_REG_MONTH);
    out->year   = cmos_read(RTC_REG_YEAR);
}

bool rtc_read_datetime(rtc_datetime_t *out) {
    if (!out) {
        return false;
    }

    /* Wait for any in-progress update to finish, then read twice and require
     * both passes to agree. The registers are not latched, so a read that
     * straddles an update tick can mix old and new fields - most visibly at a
     * minute or hour rollover. */
    rtc_datetime_t first, second;
    int attempts = 0;

    do {
        for (int i = 0; i < 100000 && rtc_updating(); i++) {
            /* spin until the update flag clears */
        }
        rtc_read_raw(&first);
        rtc_read_raw(&second);

        if (first.second == second.second && first.minute == second.minute &&
            first.hour == second.hour && first.day == second.day &&
            first.month == second.month && first.year == second.year) {
            break;
        }
    } while (++attempts < 10);

    uint8_t status_b = cmos_read(RTC_REG_STATUS_B);
    bool is_bcd      = (status_b & STATUS_B_BINARY) == 0;
    bool is_24_hour  = (status_b & STATUS_B_24_HOUR) != 0;

    uint8_t sec   = (uint8_t)first.second;
    uint8_t min   = (uint8_t)first.minute;
    uint8_t hour  = (uint8_t)first.hour;
    uint8_t day   = (uint8_t)first.day;
    uint8_t month = (uint8_t)first.month;
    uint8_t year  = (uint8_t)first.year;

    /* In 12-hour mode the top bit of the hours register is the PM flag, not
     * part of the value. It must come off before any BCD conversion, or the
     * conversion decodes a bogus tens digit. */
    bool is_pm = false;
    if (!is_24_hour) {
        is_pm = (hour & HOURS_PM_FLAG) != 0;
        hour &= (uint8_t)~HOURS_PM_FLAG;
    }

    if (is_bcd) {
        sec   = bcd_to_bin(sec);
        min   = bcd_to_bin(min);
        hour  = bcd_to_bin(hour);
        day   = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year  = bcd_to_bin(year);
    }

    if (!is_24_hour) {
        /* 12-hour clock runs 1..12: noon and midnight are the special cases. */
        if (is_pm) {
            if (hour != 12) hour = (uint8_t)(hour + 12);
        } else {
            if (hour == 12) hour = 0;
        }
    }

    out->second = sec;
    out->minute = min;
    out->hour   = hour;
    out->day    = day;
    out->month  = month;
    out->year   = (uint16_t)(2000 + year);
    return true;
}
