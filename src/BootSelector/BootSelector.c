/**
 * Boot Selector - Choose between Legacy System 7 and X11 Mode
 *
 * Provides boot-time menu to select environment:
 * - Legacy Mode: Original Mac-like desktop environment
 * - X11 Mode: Modern X11 with Mac Classic theming (default)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef enum {
    BOOT_MODE_LEGACY = 0,
    BOOT_MODE_X11 = 1,
    BOOT_MODE_UNKNOWN = 2
} BootMode;

/* Boot mode stored in non-volatile memory */
#define BOOT_MODE_ADDR 0x500000

/**
 * Display boot menu and get user selection
 */
BootMode BootSelector_ShowMenu(void) {
    extern void serial_puts(const char* str);

    serial_puts("\n");
    serial_puts("========== System 7.1 - Boot Mode Selector ==========\n");
    serial_puts("  L - Legacy Mode (Classic Desktop)\n");
    serial_puts("  X - X11 Mode (Modern, Default)\n");
    serial_puts("====================================================\n");
    serial_puts("Defaulting to X11 mode...\n");

    /* TODO: Implement actual keyboard input from serial/keyboard */
    return BOOT_MODE_X11;
}

/**
 * Initialize boot mode
 */
BootMode BootSelector_Init(void) {
    BootMode mode;
    extern void serial_puts(const char* str);

    serial_puts("[BOOT] Initializing boot selector...\n");

    /* Show boot menu */
    mode = BootSelector_ShowMenu();

    serial_puts("[BOOT] Boot mode selected\n");

    return mode;
}

/**
 * Load and execute boot mode
 */
void BootSelector_LoadMode(BootMode mode) {
    if (mode == BOOT_MODE_LEGACY) {
        /* Legacy System 7 desktop initialization would go here */
        extern void serial_puts(const char* str);
        serial_puts("[BOOT] Starting Legacy System 7 desktop...\n");
        /* TODO: Call legacy initialization */
    } else {
        extern void serial_puts(const char* str);
        serial_puts("[BOOT] Starting X11 environment...\n");
        /* Call X11 initialization */
        extern void X11_Initialize(void);
        X11_Initialize();
    }
}
