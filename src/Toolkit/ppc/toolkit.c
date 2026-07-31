/* Placeholder PPC toolkit source (clean-room compatibility layer)
 * This file implements minimal Gestalt and trap dispatcher stubs.
 * It is intentionally small and contains no Apple proprietary code.
 * Build this with a PPC big-endian cross-toolchain and link as a flat
 * binary to produce TOOLKIT_PPC.ROM.
 */

#include <stdint.h>

// Simple Gestalt selectors we respond to
#define GESTALT_OS_TYPE 0x0001

// Minimal PRAM area (filled by loader)
extern void* g_pram_ptr;
extern uint32_t g_pram_size;

// Return fake Gestalt values
uint32_t gestalt(uint32_t selector) {
    switch (selector) {
        case GESTALT_OS_TYPE:
            return 0x09320000; /* pretend OS 9.2.0 encoded value; apps can probe */
        default:
            return 0;
    }
}

// Minimal trap dispatcher (very small surface)
void trap_dispatch(uint32_t trap_num) {
    // In a real toolkit this would dispatch to many traps.
    // Here we only implement a tiny set for compatibility smoke tests.
    (void)trap_num;
}

// Entry point for toolkit (optional)
void toolkit_init(void) {
    // No-op for placeholder
}
