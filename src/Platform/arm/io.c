/*
 * ARM I/O Operations HAL
 * Memory-mapped I/O abstractions for ARM-based systems
 * Replaces x86 port I/O operations with MMIO equivalents
 */

#include <stdint.h>
#include "Platform/include/io.h"
#include "mmio.h"

/*
 * ARM doesn't have port-based I/O like x86
 * All peripheral access is memory-mapped
 * These functions provide compatibility layer
 */

/* Note: On ARM, port I/O operations don't apply
 * Device drivers should use direct MMIO access
 * These stubs are here for compatibility only
 */

uint8_t hal_inb(uint16_t port) {
    /* Port I/O not available on ARM - return 0 */
    (void)port;
    return 0;
}

void hal_outb(uint16_t port, uint8_t value) {
    /* Port I/O not available on ARM */
    (void)port;
    (void)value;
}

uint16_t hal_inw(uint16_t port) {
    /* Port I/O not available on ARM - return 0 */
    (void)port;
    return 0;
}

void hal_outw(uint16_t port, uint16_t value) {
    /* Port I/O not available on ARM */
    (void)port;
    (void)value;
}

uint32_t hal_inl(uint16_t port) {
    /* Port I/O not available on ARM - return 0 */
    (void)port;
    return 0;
}

void hal_outl(uint16_t port, uint32_t value) {
    /* Port I/O not available on ARM */
    (void)port;
    (void)value;
}

/*
 * ARM-specific I/O delay
 * Used for synchronization with slow peripherals
 */
void hal_io_delay(uint32_t cycles) {
    /* Simple delay loop - not cycle-accurate but sufficient */
    for (uint32_t i = 0; i < cycles; i++) {
        __asm__ __volatile__("nop");
    }
}

/*
 * Flush I/O buffers (if applicable)
 * On ARM with strongly-ordered memory, mostly a no-op
 */
void hal_io_flush(void) {
    /* Data synchronization barrier */
    __asm__ __volatile__("dsb sy" ::: "memory");
}

/* Stub for uart_flush - called by debug logging throughout the codebase */
void uart_flush(void) {
    /* No-op on ARM until UART driver is integrated */
}

/* Stub for uart_puts - called by serial_puts() in System71StdLib.c */
void uart_puts(const char* s) {
    (void)s;
    /* No-op on ARM until UART driver is integrated */
}
