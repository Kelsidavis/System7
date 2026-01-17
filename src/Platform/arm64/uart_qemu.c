/*
 * ARM64 UART Driver for QEMU
 * PL011 UART with QEMU virt machine addresses
 */

#include <stdint.h>
#include <stdbool.h>
#include "mmio.h"
#include "uart.h"

/* PL011 UART registers for QEMU virt machine
 * Base address: 0x09000000
 */
#define UART_BASE   0x09000000

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

/*
 * Initialize UART for QEMU
 */
void uart_init(void) {
    /* Disable UART during configuration */
    mmio_write32(UART_BASE + UART_CR, 0);

    /* Clear all interrupts */
    mmio_write32(UART_BASE + UART_ICR, 0x7FF);

    /* Set baud rate to 115200
     * With UART clock at 24MHz (QEMU default):
     *   Divisor = 24000000 / (16 * 115200) = 13.0208...
     *   IBRD = 13
     *   FBRD = (int)((0.0208... * 64) + 0.5) = 1
     */
    mmio_write32(UART_BASE + UART_IBRD, 13);
    mmio_write32(UART_BASE + UART_FBRD, 1);

    /* Set line control: 8N1, enable FIFOs */
    mmio_write32(UART_BASE + UART_LCRH, LCRH_WLEN_8 | LCRH_FEN);

    /* Disable all interrupts */
    mmio_write32(UART_BASE + UART_IMSC, 0);

    /* Enable UART, TX, and RX */
    mmio_write32(UART_BASE + UART_CR, CR_UARTEN | CR_TXE | CR_RXE);
}

/*
 * Write a single character to UART
 */
void uart_putc(char c) {
    /* Wait for TX FIFO to have space */
    while (mmio_read32(UART_BASE + UART_FR) & FR_TXFF) {
        /* Spin wait */
    }

    /* Write character */
    mmio_write32(UART_BASE + UART_DR, (uint32_t)c);
}

/*
 * Read a character from UART
 * Returns -1 if no data available
 */
int uart_getc(void) {
    /* Check if RX FIFO is empty */
    if (mmio_read32(UART_BASE + UART_FR) & FR_RXFE) {
        return -1;
    }

    /* Read character */
    return (int)(mmio_read32(UART_BASE + UART_DR) & 0xFF);
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
    return true;  /* Always available in QEMU */
}

/*
 * Check if data is available to read (non-blocking)
 */
bool uart_data_ready(void) {
    /* Check if RX FIFO has data (not empty) */
    return (mmio_read32(UART_BASE + UART_FR) & FR_RXFE) == 0;
}

/*
 * Flush UART output
 */
void uart_flush(void) {
    /* Wait for UART to finish transmitting */
    while (mmio_read32(UART_BASE + UART_FR) & FR_BUSY) {
        /* Spin wait */
    }
}
