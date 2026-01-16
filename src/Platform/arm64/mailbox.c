/*
 * ARM64 Mailbox Driver
 * VideoCore mailbox interface for Raspberry Pi 3/4/5
 * Used for GPU communication, framebuffer setup, hardware info
 */

#include <stdint.h>
#include <stdbool.h>
#include "mmio.h"

/* Mailbox registers
 * Base addresses:
 *   Pi 3: 0x3F00B880
 *   Pi 4/5: 0xFE00B880
 */

#define MAILBOX_READ        0x00
#define MAILBOX_STATUS      0x18
#define MAILBOX_WRITE       0x20

/* Status register bits */
#define MAILBOX_FULL        0x80000000
#define MAILBOX_EMPTY       0x40000000

/* Mailbox channels */
#define MBOX_CH_POWER       0
#define MBOX_CH_FB          1
#define MBOX_CH_VUART       2
#define MBOX_CH_VCHIQ       3
#define MBOX_CH_LEDS        4
#define MBOX_CH_BTNS        5
#define MBOX_CH_TOUCH       6
#define MBOX_CH_COUNT       7
#define MBOX_CH_PROP        8  /* Property tags ARM->VC */
#define MBOX_CH_PROP_VC     9  /* Property tags VC->ARM */

/* Property tag IDs */
#define MBOX_TAG_GET_BOARD_MODEL        0x00010001
#define MBOX_TAG_GET_BOARD_REVISION     0x00010002
#define MBOX_TAG_GET_BOARD_MAC          0x00010003
#define MBOX_TAG_GET_BOARD_SERIAL       0x00010004
#define MBOX_TAG_GET_ARM_MEMORY         0x00010005
#define MBOX_TAG_GET_VC_MEMORY          0x00010006
#define MBOX_TAG_GET_CLOCKS             0x00010007

#define MBOX_TAG_ALLOCATE_BUFFER        0x00040001
#define MBOX_TAG_RELEASE_BUFFER         0x00048001
#define MBOX_TAG_BLANK_SCREEN           0x00040002

#define MBOX_TAG_GET_PHYSICAL_SIZE      0x00040003
#define MBOX_TAG_TEST_PHYSICAL_SIZE     0x00044003
#define MBOX_TAG_SET_PHYSICAL_SIZE      0x00048003
#define MBOX_TAG_GET_VIRTUAL_SIZE       0x00040004
#define MBOX_TAG_TEST_VIRTUAL_SIZE      0x00044004
#define MBOX_TAG_SET_VIRTUAL_SIZE       0x00048004
#define MBOX_TAG_GET_DEPTH              0x00040005
#define MBOX_TAG_TEST_DEPTH             0x00044005
#define MBOX_TAG_SET_DEPTH              0x00048005
#define MBOX_TAG_GET_PIXEL_ORDER        0x00040006
#define MBOX_TAG_TEST_PIXEL_ORDER       0x00044006
#define MBOX_TAG_SET_PIXEL_ORDER        0x00048006
#define MBOX_TAG_GET_PITCH              0x00040008

/* Response codes */
#define MBOX_REQUEST                    0x00000000
#define MBOX_RESPONSE_SUCCESS           0x80000000
#define MBOX_RESPONSE_ERROR             0x80000001

/* Mailbox base address */
static uint64_t mailbox_base = 0;

/* Timeout for mailbox operations (iterations, not time) */
#define MAILBOX_TIMEOUT     1000000

/* Mailbox buffer - must be 16-byte aligned
 * Exposed globally for framebuffer driver */
uint32_t __attribute__((aligned(16))) mailbox_buffer[256];

/*
 * Detect mailbox base address
 */
static bool mailbox_detect_base(void) {
    /* Try Pi 4/5 address */
    uint64_t test_base = 0xFE00B880;
    uint32_t status = mmio_read32(test_base + MAILBOX_STATUS);

    /* If we can read the status register, assume it's valid */
    if ((status & (MAILBOX_FULL | MAILBOX_EMPTY)) != 0) {
        mailbox_base = test_base;
        return true;
    }

    /* Try Pi 3 address */
    test_base = 0x3F00B880;
    status = mmio_read32(test_base + MAILBOX_STATUS);
    if ((status & (MAILBOX_FULL | MAILBOX_EMPTY)) != 0) {
        mailbox_base = test_base;
        return true;
    }

    return false;
}

/*
 * Initialize mailbox
 */
bool mailbox_init(void) {
    return mailbox_detect_base();
}

/*
 * Write to mailbox
 */
static bool mailbox_write(uint32_t channel, uint32_t data) {
    if (!mailbox_base) return false;
    if (channel > 15) return false;

    /* Wait for mailbox to be not full (with timeout) */
    uint32_t timeout = MAILBOX_TIMEOUT;
    while (mmio_read32(mailbox_base + MAILBOX_STATUS) & MAILBOX_FULL) {
        if (--timeout == 0) return false;
    }

    /* Write data with channel in low 4 bits */
    mmio_write32(mailbox_base + MAILBOX_WRITE, (data & 0xFFFFFFF0) | (channel & 0xF));

    return true;
}

/*
 * Read from mailbox
 * Returns true on success, stores result in *result
 */
static bool mailbox_read(uint32_t channel, uint32_t *result) {
    if (!mailbox_base) return false;

    uint32_t timeout = MAILBOX_TIMEOUT;
    while (timeout > 0) {
        /* Wait for mailbox to be not empty (with timeout) */
        uint32_t wait = MAILBOX_TIMEOUT;
        while (mmio_read32(mailbox_base + MAILBOX_STATUS) & MAILBOX_EMPTY) {
            if (--wait == 0) return false;
        }

        /* Read data */
        uint32_t data = mmio_read32(mailbox_base + MAILBOX_READ);

        /* Check if it's for our channel */
        if ((data & 0xF) == channel) {
            *result = data & 0xFFFFFFF0;
            return true;
        }
        timeout--;
    }
    return false;
}

/*
 * Call mailbox with property tags
 * Returns true on success
 */
bool mailbox_call(uint32_t channel) {
    if (!mailbox_base) return false;

    /* Convert buffer address to GPU bus address
     * For Pi 3: Physical address | 0xC0000000
     * For Pi 4/5: Physical address (identity mapped) */
    uint64_t addr = (uint64_t)mailbox_buffer;

    /* Detect Pi version by base address */
    if (mailbox_base == 0x3F00B880) {
        /* Pi 3 - use VC bus address */
        addr |= 0xC0000000;
    }
    /* Pi 4/5 use physical address directly */

    /* Ensure data is written to memory */
    __asm__ volatile("dsb sy" ::: "memory");

    /* Write to mailbox */
    if (!mailbox_write(channel, (uint32_t)addr)) {
        return false;
    }

    /* Read response */
    uint32_t response;
    if (!mailbox_read(channel, &response)) {
        return false;
    }

    /* Ensure data is read from memory */
    __asm__ volatile("dsb sy" ::: "memory");

    /* Check if response matches our buffer address */
    if (response != (uint32_t)addr) {
        return false;
    }

    /* Check response code */
    return mailbox_buffer[1] == MBOX_RESPONSE_SUCCESS;
}

/*
 * Get board model
 */
bool mailbox_get_board_model(uint32_t *model) {
    mailbox_buffer[0] = 8 * 4;  /* Buffer size */
    mailbox_buffer[1] = MBOX_REQUEST;
    mailbox_buffer[2] = MBOX_TAG_GET_BOARD_MODEL;
    mailbox_buffer[3] = 4;      /* Value buffer size */
    mailbox_buffer[4] = 0;      /* Request */
    mailbox_buffer[5] = 0;      /* Model will be returned here */
    mailbox_buffer[6] = 0;      /* End tag */
    mailbox_buffer[7] = 0;

    if (!mailbox_call(MBOX_CH_PROP)) {
        return false;
    }

    *model = mailbox_buffer[5];
    return true;
}

/*
 * Get board revision
 */
bool mailbox_get_board_revision(uint32_t *revision) {
    mailbox_buffer[0] = 8 * 4;
    mailbox_buffer[1] = MBOX_REQUEST;
    mailbox_buffer[2] = MBOX_TAG_GET_BOARD_REVISION;
    mailbox_buffer[3] = 4;
    mailbox_buffer[4] = 0;
    mailbox_buffer[5] = 0;
    mailbox_buffer[6] = 0;
    mailbox_buffer[7] = 0;

    if (!mailbox_call(MBOX_CH_PROP)) {
        return false;
    }

    *revision = mailbox_buffer[5];
    return true;
}

/*
 * Get ARM memory region
 */
bool mailbox_get_arm_memory(uint32_t *base, uint32_t *size) {
    mailbox_buffer[0] = 9 * 4;
    mailbox_buffer[1] = MBOX_REQUEST;
    mailbox_buffer[2] = MBOX_TAG_GET_ARM_MEMORY;
    mailbox_buffer[3] = 8;
    mailbox_buffer[4] = 0;
    mailbox_buffer[5] = 0;
    mailbox_buffer[6] = 0;
    mailbox_buffer[7] = 0;
    mailbox_buffer[8] = 0;

    if (!mailbox_call(MBOX_CH_PROP)) {
        return false;
    }

    *base = mailbox_buffer[5];
    *size = mailbox_buffer[6];
    return true;
}

/*
 * Power off device using VideoCore firmware
 * Device ID 0 = SD card, 1 = UART0, 2 = UART1, 3 = USB HCD
 * For system power off, we use the GET_POWER_STATE/SET_POWER_STATE for device 0
 * Or use the hardware reset function
 */
bool mailbox_power_off(void) {
    /* Use firmware power off - tag 0x00028001 (SET_POWER_STATE) */
    /* Request to power off all devices then halt */
#define MBOX_TAG_SET_POWER_STATE    0x00028001
#define MBOX_TAG_GET_POWER_STATE    0x00020001

    /* First, attempt a clean shutdown sequence */
    /* Power off USB (device 3) */
    mailbox_buffer[0] = 8 * 4;
    mailbox_buffer[1] = MBOX_REQUEST;
    mailbox_buffer[2] = MBOX_TAG_SET_POWER_STATE;
    mailbox_buffer[3] = 8;      /* Value buffer size */
    mailbox_buffer[4] = 8;      /* Request: size of value */
    mailbox_buffer[5] = 3;      /* Device ID: USB HCD */
    mailbox_buffer[6] = 0;      /* State: off, don't wait */
    mailbox_buffer[7] = 0;      /* End tag */

    bool usb_powered_off = mailbox_call(MBOX_CH_PROP);

    /* Note: There's no actual "power off the whole board" mailbox tag.
     * The best we can do is halt the CPU and let the watchdog
     * or user power cycle the device. Return whether USB power-off succeeded. */
    return usb_powered_off;
}

/*
 * Check if mailbox is available
 */
bool mailbox_is_available(void) {
    return mailbox_base != 0;
}
