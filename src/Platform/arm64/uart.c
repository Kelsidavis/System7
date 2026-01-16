/*
 * ARM64 PL011 UART Driver
 * Primary UART for Raspberry Pi 3/4/5
 */

#include <stdint.h>
#include <stdbool.h>
#include "mmio.h"

/* PL011 UART registers for Raspberry Pi
 * Base addresses vary by model:
 *   Pi 3: 0x3F201000 (BCM2837)
 *   Pi 4/5: 0xFE201000 (BCM2711/BCM2712)
 */

/* UART register offsets from base */
#define UART_DR     0x00  /* Data Register */
#define UART_FR     0x18  /* Flag Register */
#define UART_IBRD   0x24  /* Integer Baud Rate Divisor */
#define UART_FBRD   0x28  /* Fractional Baud Rate Divisor */
#define UART_LCRH   0x2C  /* Line Control Register */
#define UART_CR     0x30  /* Control Register */
#define UART_IMSC   0x38  /* Interrupt Mask Set/Clear */
#define UART_ICR    0x44  /* Interrupt Clear Register */

/* Flag Register bits */
#define FR_TXFF     (1 << 5)  /* Transmit FIFO full */
#define FR_RXFE     (1 << 4)  /* Receive FIFO empty */
#define FR_BUSY     (1 << 3)  /* UART busy */

/* Line Control Register bits */
#define LCRH_FEN    (1 << 4)  /* Enable FIFOs */
#define LCRH_WLEN_8 (3 << 5)  /* 8-bit word length */

/* Control Register bits */
#define CR_UARTEN   (1 << 0)  /* UART enable */
#define CR_TXE      (1 << 8)  /* Transmit enable */
#define CR_RXE      (1 << 9)  /* Receive enable */

/* UART base address - will be set by init */
static uint64_t uart_base = 0;

/*
 * Detect and set UART base address
 * Returns true if UART detected
 */
static bool uart_detect_base(void) {
    /* Try Pi 4/5 address first (BCM2711/BCM2712) */
    uint64_t test_base = 0xFE201000;
    uint32_t cr = mmio_read32(test_base + UART_CR);

    /* Check if UART is enabled (bit 0 should be set by firmware) */
    if (cr & CR_UARTEN) {
        uart_base = test_base;
        return true;
    }

    /* Try Pi 3 address (BCM2837) */
    test_base = 0x3F201000;
    cr = mmio_read32(test_base + UART_CR);
    if (cr & CR_UARTEN) {
        uart_base = test_base;
        return true;
    }

    return false;
}

/*
 * Initialize UART
 * Called early in boot process
 */
void uart_init(void) {
    /* Detect UART base address */
    if (!uart_detect_base()) {
        /* No UART found - continue without serial output */
        return;
    }

    /* Disable UART during configuration */
    mmio_write32(uart_base + UART_CR, 0);

    /* Clear all interrupts */
    mmio_write32(uart_base + UART_ICR, 0x7FF);

    /* Set baud rate to 115200
     * With UART clock at 48MHz:
     *   Divisor = 48000000 / (16 * 115200) = 26.0416...
     *   IBRD = 26
     *   FBRD = (int)((0.0416... * 64) + 0.5) = 3
     */
    mmio_write32(uart_base + UART_IBRD, 26);
    mmio_write32(uart_base + UART_FBRD, 3);

    /* Set line control: 8N1, enable FIFOs */
    mmio_write32(uart_base + UART_LCRH, LCRH_WLEN_8 | LCRH_FEN);

    /* Disable all interrupts */
    mmio_write32(uart_base + UART_IMSC, 0);

    /* Enable UART, TX, and RX */
    mmio_write32(uart_base + UART_CR, CR_UARTEN | CR_TXE | CR_RXE);
}

/*
 * Write a single character to UART
 */
void uart_putc(char c) {
    if (!uart_base) return;

    /* Wait for TX FIFO to have space */
    while (mmio_read32(uart_base + UART_FR) & FR_TXFF) {
        /* Spin wait */
    }

    /* Write character */
    mmio_write32(uart_base + UART_DR, (uint32_t)c);
}

/*
 * Read a character from UART
 * Returns -1 if no data available
 */
int uart_getc(void) {
    if (!uart_base) return -1;

    /* Check if RX FIFO is empty */
    if (mmio_read32(uart_base + UART_FR) & FR_RXFE) {
        return -1;
    }

    /* Read character */
    return (int)(mmio_read32(uart_base + UART_DR) & 0xFF);
}

/*
 * Check if data is available to read
 */
bool uart_data_ready(void) {
    if (!uart_base) return false;

    /* Check if RX FIFO is NOT empty */
    return (mmio_read32(uart_base + UART_FR) & FR_RXFE) == 0;
}

/*
 * Write a string to UART
 */
void uart_puts(const char *str) {
    if (!str) return;

    while (*str) {
        if (*str == '\n') {
            uart_putc('\r');  /* Add CR for LF */
        }
        uart_putc(*str++);
    }
}

/*
 * Check if UART is available
 */
bool uart_is_available(void) {
    return uart_base != 0;
}

/*
 * Flush UART output
 */
void uart_flush(void) {
    if (!uart_base) return;

    /* Wait for UART to finish transmitting */
    while (mmio_read32(uart_base + UART_FR) & FR_BUSY) {
        /* Spin wait */
    }
}
