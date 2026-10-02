/*
 * ARM Bootloader HAL Implementation
 * Raspberry Pi boot initialization
 */

#include <stdint.h>
#include <stddef.h>
#include "System71StdLib.h"
#include "Platform/include/boot.h"
#include "hardware_detect.h"
#include "mmio.h"
#include "device_tree.h"

/* Forward declarations of platform-specific initialization */
extern void device_tree_init(void *dtb_ptr);
extern void device_tree_dump(void);
extern uint32_t device_tree_get_memory_size(void);
extern void hardware_report_info(void);
extern int arm_framebuffer_init(void);
extern int arm_framebuffer_get_info(hal_framebuffer_info_t *info);
extern int arm_framebuffer_present(void);
extern int arm_platform_timer_init(void);
extern int usb_controller_init(void);
extern int usb_controller_enumerate(void);

/* Multiboot-compatible structure for ARM (simplified) */
typedef struct {
    uint32_t size;
    uint32_t reserved;
    uint32_t memory_size;
    char board_model[256];
} arm_boot_info_t;

/* Global boot information */
static arm_boot_info_t boot_info = {0};

/*
 * Initialize boot parameters
 * Called early in boot sequence from platform_boot.S with DTB address
 */
void hal_boot_init(void *boot_ptr) {
    Serial_WriteString("[ARM] ════════════════════════════════════════════════════════════\n");
    Serial_WriteString("[ARM] System 7.1 Portable - ARM Boot Initialization\n");
    Serial_WriteString("[ARM] ════════════════════════════════════════════════════════════\n");

    /* Initialize basic boot state */
    boot_info.size = sizeof(arm_boot_info_t);
    boot_info.reserved = 0;

    /* Initialize Device Tree from bootloader
     * DTB address passed in r0 by platform_boot.S
     */
    if (boot_ptr) {
        Serial_WriteString("[ARM] Parsing Device Tree Blob...\n");
        device_tree_init(boot_ptr);
        device_tree_dump();
    } else {
        Serial_WriteString("[ARM] Warning: No Device Tree provided by bootloader\n");
    }

    /* Detect hardware model early
     * Used by USB, framebuffer, and other subsystems
     */
    Serial_WriteString("[ARM] Detecting hardware model...\n");
    hardware_detect_model(boot_info.board_model, sizeof(boot_info.board_model));
    hardware_report_info();

    /* Get actual memory size from DTB or estimate */
    boot_info.memory_size = device_tree_get_memory_size();
    Serial_Printf("[ARM] Detected memory: %lu MB\n",
                  (unsigned long)(boot_info.memory_size / (1024 * 1024)));

    Serial_WriteString("[ARM] Boot initialization complete\n");
}

/*
 * Get memory size detected at boot
 * Returns actual detected memory from DTB or sensible default
 */
uint32_t hal_get_memory_size(void) {
    /* Return memory size detected during boot initialization */
    if (boot_info.memory_size > 0) {
        return boot_info.memory_size;
    }

    /* Fallback if not yet initialized */
    return 512 * 1024 * 1024;  /* 512MB default */
}

/*
 * Get system framebuffer information
 * Required by QuickDraw for display output
 */
int hal_get_framebuffer_info(hal_framebuffer_info_t *info) {
    if (!info) {
        return -1;
    }

    return arm_framebuffer_get_info(info);
}

/*
 * Platform-specific initialization
 * Called after basic kernel setup, before system initialization
 */
int hal_platform_init(void) {
    Serial_WriteString("[ARM] Platform-specific initialization\n");

    /* Initialize timer (critical for TimeManager) */
    if (arm_platform_timer_init() != 0) {
        Serial_WriteString("[ARM] Warning: Timer initialization failed\n");
        /* Don't fail - some timing features may degrade but system continues */
    }

    /* Initialize framebuffer for graphics output */
    if (arm_framebuffer_init() != 0) {
        Serial_WriteString("[ARM] Warning: Framebuffer initialization failed\n");
        Serial_WriteString("[ARM] System will continue with serial output only\n");
    }

    /* Initialize USB host controller (for keyboard/mouse input)
     * Automatically selects XHCI for Pi 4/5 or DWCOTG for Pi 3
     */
    if (usb_controller_init() != 0) {
        Serial_WriteString("[ARM] Warning: USB controller initialization failed\n");
        Serial_WriteString("[ARM] Keyboard/mouse input will not be available\n");
    } else {
        /* Enumerate connected USB devices */
        if (usb_controller_enumerate() != 0) {
            Serial_WriteString("[ARM] Warning: USB device enumeration failed\n");
        }
    }

    /* TODO: Initialize platform-specific hardware:
     * - GPIO controller (for input fallback)
     * - SD card controller improvements
     * - Interrupt controller
     * - Power management
     */

    Serial_WriteString("[ARM] Platform initialization complete\n");
    return 0;
}

/*
 * Platform shutdown
 * Called on kernel halt/reboot
 */
void hal_platform_shutdown(void) {
    Serial_WriteString("[ARM] Platform shutdown\n");

    /* TODO: Graceful shutdown sequence:
     * - Stop peripherals
     * - Flush caches
     * - Reset to bootloader
     */
}

int hal_framebuffer_present(void) {
    return arm_framebuffer_present();
}
