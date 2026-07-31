/* Placeholder 68k toolkit source (clean-room compatibility layer)
 * Similar to PPC placeholder but intended for 68k BE builds. Compile with
 * a 68k cross-toolchain and link as a flat binary to produce TOOLKIT_68K_BE.ROM.
 */

#include <stdint.h>

#define GESTALT_OS_TYPE 0x0001

extern void* g_pram_ptr;
extern uint32_t g_pram_size;

uint32_t gestalt(uint32_t selector) {
    switch (selector) {
        case GESTALT_OS_TYPE:
            return 0x07000000; /* pretend System 7.x-ish */
        default:
            return 0;
    }
}

void trap_dispatch(uint32_t trap_num) {
    (void)trap_num;
}

void toolkit_init(void) {
}
