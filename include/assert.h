#ifndef ASSERT_H
#define ASSERT_H

/* Minimal assert implementation for freestanding builds */
#include "Platform/Serial.h"

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
/* Use platform-appropriate output function */
#ifdef __aarch64__
#define assert(expr) ((void)((expr) || (uart_puts("[ASSERT] " #expr "\n"), 0)))
#else
#define assert(expr) ((void)((expr) || (serial_puts("[ASSERT] " #expr "\n"), 0)))
#endif
#endif

#endif
