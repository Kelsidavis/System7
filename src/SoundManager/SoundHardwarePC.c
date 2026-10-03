/*
 * SoundHardwarePC.c - PC Speaker hardware driver for bare-metal x86
 *
 * Provides basic audio output using the PC speaker (I/O port 0x61)
 * and Programmable Interval Timer (PIT) channel 2.
 *
 * This is a minimal implementation for SysBeep() support.
 */

#include <stdint.h>
#include <stdbool.h>
#include "SoundManager/PCSpkr.h"
#include "System71StdLib.h"
#include "SoundManager/SoundLogging.h"

/* I/O port access functions (implemented in platform code) */
#include "Platform/include/io.h"

#define inb(port) hal_inb(port)
#define outb(port, value) hal_outb(port, value)

/* PC Speaker hardware ports */
#define PC_SPEAKER_PORT     0x61    /* PC speaker control */
#define PIT_CHANNEL_2       0x42    /* PIT channel 2 data port */
#define PIT_COMMAND         0x43    /* PIT command port */

/* PIT command register bits */
#define PIT_CHANNEL_2_SEL   0xB6    /* Channel 2, square wave, binary mode */

/* PC speaker control bits */
#define SPEAKER_GATE        0x01    /* PIT channel 2 gate enable */
#define SPEAKER_DATA        0x02    /* Speaker data (connect to PIT output) */

/* PIT base frequency (1.193182 MHz) */
#define PIT_BASE_FREQ       1193182

/*
 * PCSpkr_SetFrequency - Set PC speaker frequency using PIT channel 2
 *
 * @param frequency - Frequency in Hz (0 to disable)
 */
static void PCSpkr_SetFrequency(uint32_t frequency) {
    if (frequency == 0) {
        /* Disable speaker */
        uint8_t tmp = inb(PC_SPEAKER_PORT);
        outb(PC_SPEAKER_PORT, tmp & ~(SPEAKER_GATE | SPEAKER_DATA));
        return;
    }

    /* Calculate PIT divisor for desired frequency */
    uint32_t divisor = PIT_BASE_FREQ / frequency;
    if (divisor > 65535) divisor = 65535;
    if (divisor < 1) divisor = 1;

    /* Program PIT channel 2 for square wave */
    outb(PIT_COMMAND, PIT_CHANNEL_2_SEL);
    outb(PIT_CHANNEL_2, divisor & 0xFF);        /* Low byte */
    outb(PIT_CHANNEL_2, (divisor >> 8) & 0xFF); /* High byte */

    /* Enable speaker (connect to PIT output) */
    uint8_t tmp = inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, tmp | SPEAKER_GATE | SPEAKER_DATA);
}

/*
 * PCSpkr_Beep - Generate a beep tone for specified duration
 *
 * @param frequency - Tone frequency in Hz
 * @param duration_ms - Duration in milliseconds
 */
/* Simple CPU-based delay (calibrated for modern CPUs) */
static void delay_ms(uint32_t ms) {
    /* Approximate delay loop for modern multi-GHz CPUs
     * Each ms needs many more iterations on fast processors */
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 100000; j++) {  /* Increased from 1000 to 100000 */
            /* Busy wait - volatile prevents optimization */
        }
    }
}

void PCSpkr_Beep(uint32_t frequency, uint32_t duration_ms) {

    SND_LOG_DEBUG("PCSpkr_Beep: freq=%lu Hz, duration=%lu ms\n", (unsigned long)frequency, (unsigned long)duration_ms);

    /* Start tone */
    PCSpkr_SetFrequency(frequency);

    /* Simple busy-wait delay (doesn't rely on timer interrupts) */
    delay_ms(duration_ms);

    /* Stop tone */
    PCSpkr_SetFrequency(0);
}

/*
 * PCSpkr_Init - Initialize PC speaker hardware
 *
 * Returns 0 on success
 */
int PCSpkr_Init(void) {

    /* Ensure speaker is off */
    PCSpkr_SetFrequency(0);

    serial_puts("PCSpkr_Init: PC speaker initialized\n");
    return 0;
}

/*
 * PCSpkr_Shutdown - Shut down PC speaker hardware
 */
void PCSpkr_Shutdown(void) {
    /* Ensure speaker is off */
    PCSpkr_SetFrequency(0);
}
