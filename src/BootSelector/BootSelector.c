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
    printf("\n");
    printf("┌──────────────────────────────────────┐\n");
    printf("│   System 7.1 - Boot Mode Selector    │\n");
    printf("├──────────────────────────────────────┤\n");
    printf("│  L - Legacy Mode (Classic Desktop)   │\n");
    printf("│  X - X11 Mode (Modern, Default)      │\n");
    printf("│  (Default boots X11 in 3 seconds)    │\n");
    printf("└──────────────────────────────────────┘\n");
    printf("\nSelect mode: ");

    /* TODO: Implement actual keyboard input from serial/keyboard */
    /* For now, default to X11 */
    printf("X (defaulting to X11)\n");
    return BOOT_MODE_X11;
}

/**
 * Initialize boot mode
 */
BootMode BootSelector_Init(void) {
    BootMode mode;

    printf("[BOOT] Initializing boot selector...\n");

    /* Show boot menu */
    mode = BootSelector_ShowMenu();

    printf("[BOOT] Selected mode: %s\n",
           mode == BOOT_MODE_LEGACY ? "Legacy" : "X11");

    return mode;
}

/**
 * Load and execute boot mode
 */
void BootSelector_LoadMode(BootMode mode) {
    printf("[BOOT] Loading %s environment...\n",
           mode == BOOT_MODE_LEGACY ? "Legacy" : "X11");

    if (mode == BOOT_MODE_LEGACY) {
        printf("[BOOT] Starting Legacy System 7 desktop...\n");
        /* Call legacy initialization */
        extern void InitializeDesktop(void);
        InitializeDesktop();
    } else {
        printf("[BOOT] Starting X11 environment...\n");
        /* Call X11 initialization */
        extern void X11_Initialize(void);
        X11_Initialize();
    }
}
