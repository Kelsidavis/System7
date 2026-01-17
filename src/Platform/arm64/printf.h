/*
 * ARM64 Printf Interface
 * Minimal snprintf for kernel
 */

#ifndef ARM64_PRINTF_H
#define ARM64_PRINTF_H

#include <stddef.h>

/* Printf-style formatting */
int snprintf(char *str, size_t size, const char *format, ...);

#endif /* ARM64_PRINTF_H */
