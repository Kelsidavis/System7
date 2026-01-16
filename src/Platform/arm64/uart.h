/*
 * ARM64 UART Driver Interface
 * PL011 UART for Raspberry Pi 3/4/5
 */

#ifndef ARM64_UART_H
#define ARM64_UART_H

#include <stdbool.h>

/* Initialize UART hardware */
void uart_init(void);

/* Write single character */
void uart_putc(char c);

/* Read single character (returns -1 if none available) */
int uart_getc(void);

/* Write null-terminated string */
void uart_puts(const char *str);

/* Check if UART is available */
bool uart_is_available(void);

/* Check if data is available to read (non-blocking) */
bool uart_data_ready(void);

/* Flush UART output buffer */
void uart_flush(void);

#endif /* ARM64_UART_H */
