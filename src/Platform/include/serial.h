#ifndef HAL_SERIAL_H
#define HAL_SERIAL_H

#include <stdarg.h>
#include <stdint.h>

void serial_puts(const char* str);
void serial_printf(const char* fmt, ...);

#endif /* HAL_SERIAL_H */
