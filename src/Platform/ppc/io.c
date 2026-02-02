/*
 * io.c - PowerPC HAL I/O stubs
 *
 * Memory-mapped I/O handling will be implemented as the PowerPC bring-up
 * progresses. For now the routines are no-ops so higher level subsystems can
 * link successfully.
 */

#include "Platform/include/io.h"

void hal_outb(uint16_t port, uint8_t value) {
    (void)port;
    (void)value;
}

uint8_t hal_inb(uint16_t port) {
    (void)port;
    return 0;
}

void hal_outw(uint16_t port, uint16_t value) {
    (void)port;
    (void)value;
}

uint16_t hal_inw(uint16_t port) {
    (void)port;
    return 0;
}

void hal_outl(uint16_t port, uint32_t value) {
    (void)port;
    (void)value;
}

uint32_t hal_inl(uint16_t port) {
    (void)port;
    return 0;
}

/* Stub for uart_flush - called by debug logging throughout the codebase */
void uart_flush(void) {
    /* No-op on PowerPC until UART driver is integrated */
}
