/**
 * Boot Selector - Boot Mode Selection Header
 */

#ifndef BOOT_SELECTOR_H
#define BOOT_SELECTOR_H

typedef enum {
    BOOT_MODE_LEGACY = 0,
    BOOT_MODE_X11 = 1,
    BOOT_MODE_UNKNOWN = 2
} BootMode;

/* Boot selector functions */
BootMode BootSelector_ShowMenu(void);
BootMode BootSelector_Init(void);
void BootSelector_LoadMode(BootMode mode);

#endif
