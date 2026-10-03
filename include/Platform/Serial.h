/* Shared serial console interface. */
#ifndef SYSTEM7_PLATFORM_SERIAL_H
#define SYSTEM7_PLATFORM_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char* str);
int serial_data_ready(void);
char serial_getchar(void);
void serial_print_hex(uint32_t value);
void serial_printf(const char* fmt, ...)
    __attribute__((format(printf, 1, 2)));

#if defined(__arm__) || defined(__aarch64__)
void serial_set_pl011_base(uintptr_t base);
#endif

#endif /* SYSTEM7_PLATFORM_SERIAL_H */
