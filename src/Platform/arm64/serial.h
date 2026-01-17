/*
 * ARM64 Serial Port Interface
 * Compatibility layer mapping System7 serial to ARM64 UART
 */

#ifndef ARM64_SERIAL_H
#define ARM64_SERIAL_H

#include <stdint.h>

/* Initialize serial port */
void serial_init(void);

/* Write single character */
void serial_putchar(char c);

/* Write null-terminated string */
void serial_puts(const char *str);

/* Read character (blocking) */
char serial_getchar(void);

/* Check if data is available */
int serial_data_ready(void);

/* Print hex value */
void serial_print_hex(uint32_t value);

/* Printf-style output */
void serial_printf(const char *fmt, ...);

/* System7 compatible string output */
void Serial_WriteString(const char *str);

#endif /* ARM64_SERIAL_H */
