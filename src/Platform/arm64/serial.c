/*
 * ARM64 Serial Port Compatibility Layer
 * Maps System7 serial functions to ARM64 UART driver
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "uart.h"
#include "serial.h"

/*
 * serial_init - Initialize serial port
 * For ARM64, UART is already initialized by arm64_boot_main
 */
void serial_init(void) {
    /* UART already initialized, nothing to do */
}

/*
 * serial_putchar - Write single character
 */
void serial_putchar(char c) {
    uart_putc(c);
}

/*
 * serial_puts - Write null-terminated string
 */
void serial_puts(const char *str) {
    uart_puts(str);
}

/*
 * serial_getchar - Read character (blocking)
 */
char serial_getchar(void) {
    int c;
    do {
        c = uart_getc();
    } while (c < 0);
    return (char)c;
}

/*
 * serial_data_ready - Check if data is available
 */
int serial_data_ready(void) {
    return uart_data_ready() ? 1 : 0;
}

/*
 * serial_print_hex - Print hex value
 */
void serial_print_hex(uint32_t value) {
    const char *hex = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 7; i >= 0; i--) {
        uart_putc(hex[(value >> (i * 4)) & 0xF]);
    }
}

/*
 * Simple printf implementation for serial output
 */
static void print_decimal(uint32_t value) {
    char buf[12];
    int i = 0;

    if (value == 0) {
        uart_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

static void print_hex_value(uint32_t value, int width) {
    const char *hex = "0123456789abcdef";
    int started = 0;

    for (int i = 7; i >= 0; i--) {
        int digit = (value >> (i * 4)) & 0xF;
        if (digit != 0 || started || i < width) {
            uart_putc(hex[digit]);
            started = 1;
        }
    }

    if (!started) {
        uart_putc('0');
    }
}

void serial_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;

            /* Handle format specifiers */
            switch (*fmt) {
                case 'd':
                case 'i': {
                    int val = va_arg(args, int);
                    if (val < 0) {
                        uart_putc('-');
                        val = -val;
                    }
                    print_decimal((uint32_t)val);
                    break;
                }
                case 'u':
                    print_decimal(va_arg(args, uint32_t));
                    break;
                case 'x':
                    print_hex_value(va_arg(args, uint32_t), 1);
                    break;
                case 'X':
                    print_hex_value(va_arg(args, uint32_t), 1);
                    break;
                case 'p':
                    uart_puts("0x");
                    print_hex_value((uint32_t)(uintptr_t)va_arg(args, void*), 8);
                    break;
                case 's': {
                    const char *s = va_arg(args, const char*);
                    if (s) {
                        uart_puts(s);
                    } else {
                        uart_puts("(null)");
                    }
                    break;
                }
                case 'c':
                    uart_putc((char)va_arg(args, int));
                    break;
                case '%':
                    uart_putc('%');
                    break;
                case '0':
                    /* Skip width specifiers for now */
                    while (*fmt >= '0' && *fmt <= '9') fmt++;
                    fmt--; /* Back up one to let main loop handle */
                    break;
                default:
                    uart_putc('%');
                    uart_putc(*fmt);
                    break;
            }
        } else {
            uart_putc(*fmt);
        }
        fmt++;
    }

    va_end(args);
}

/*
 * Serial_WriteString - System7 compatible string output
 */
void Serial_WriteString(const char *str) {
    uart_puts(str);
}
