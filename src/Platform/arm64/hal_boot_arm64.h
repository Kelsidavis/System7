/*
 * ARM64 Boot HAL Interface
 * Platform-specific boot functions for ARM64/AArch64
 */

#ifndef HAL_BOOT_ARM64_H
#define HAL_BOOT_ARM64_H

#include <stdint.h>

/* Early ARM64 boot entry from assembly - called with DTB pointer */
void arm64_boot_main(void *dtb_ptr);

/* Get the device tree blob address */
void *hal_get_dtb_address(void);

#endif /* HAL_BOOT_ARM64_H */
