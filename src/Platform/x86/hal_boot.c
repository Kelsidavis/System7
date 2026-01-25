/*
 * hal_boot.c - x86 Boot HAL Implementation
 *
 * Platform-specific boot initialization for x86.
 */

#include "Platform/include/boot.h"
#include "Platform/include/serial.h"

/*
 * hal_boot_init - Initialize platform-specific boot components
 *
 * For x86, most initialization is handled in kernel_main.
 * This function can be extended to handle x86-specific early init.
 */
void hal_boot_init(void *boot_arg) {
    (void)boot_arg;
    /* No early x86-specific initialization needed yet */
    /* Serial port and other hardware init happens in kernel_main */
}

int hal_get_framebuffer_info(hal_framebuffer_info_t *info) {
    (void)info;
    return -1;
}

uint32_t hal_get_memory_size(void) {
    return 0;
}

int hal_platform_init(void) {
    return 0;
}

void hal_platform_shutdown(void) {
}

int hal_framebuffer_present(void) {
    return 0;
}
