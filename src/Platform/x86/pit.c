/*
 * pit.c - Programmable Interval Timer (8253/8254)
 */

#include "pit.h"
#include "Platform/include/io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182u

static uint16_t g_pit_divisor = 0;

void pit_init_hz(uint32_t hz) {
    if (hz == 0) {
        return;
    }

    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor == 0) {
        divisor = 1;
    } else if (divisor > 0xFFFF) {
        divisor = 0xFFFF;
    }

    g_pit_divisor = (uint16_t)divisor;

    /* Channel 0, lobyte/hibyte, mode 2 (rate generator), binary */
    hal_outb(PIT_COMMAND, 0x34);
    hal_outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    hal_outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint16_t pit_get_divisor(void) {
    return g_pit_divisor;
}
