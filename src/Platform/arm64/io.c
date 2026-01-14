/*
 * ARM64 I/O Stubs
 * Port I/O is not applicable on ARM64 - these are no-op stubs
 */

#include <stdint.h>
#include "Platform/include/io.h"

/*
 * Port I/O functions - ARM64 doesn't have x86-style port I/O
 * These are no-op stubs for compatibility
 */

void hal_outb(uint16_t port, uint8_t val) {
    (void)port;
    (void)val;
    /* No-op on ARM64 */
}

void hal_outw(uint16_t port, uint16_t val) {
    (void)port;
    (void)val;
    /* No-op on ARM64 */
}

void hal_outl(uint16_t port, uint32_t val) {
    (void)port;
    (void)val;
    /* No-op on ARM64 */
}

uint8_t hal_inb(uint16_t port) {
    (void)port;
    return 0;
}

uint16_t hal_inw(uint16_t port) {
    (void)port;
    return 0;
}

uint32_t hal_inl(uint16_t port) {
    (void)port;
    return 0;
}

/*
 * I/O delay - used for timing on x86
 * On ARM64, we don't need this but provide it for compatibility
 */
void hal_io_wait(void) {
    /* Small delay - could use ARM timer if needed */
    for (volatile int i = 0; i < 100; i++);
}
