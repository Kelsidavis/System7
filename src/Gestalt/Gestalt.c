/*
 * Gestalt.c - Gestalt Manager core implementation
 * Based on Inside Macintosh: Operating System Utilities
 * Clean-room implementation for freestanding System 7.1
 */

#include "SystemTypes.h"
#include "Gestalt/Gestalt.h"
#include "Gestalt/GestaltPriv.h"

/* External serial logging */
extern void serial_puts(const char *s);
extern void serial_putchar(char c);

/* Local hex print helper - used for debug output */
__attribute__((unused))
static void gestalt_print_hex(uint32_t value) {
    const char* hex = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 7; i >= 0; i--) {
        serial_putchar(hex[(value >> (i * 4)) & 0xF]);
    }
}

/* Static table for Gestalt entries.
 * ---------------------------------
 * Mirrors the ROM’s global selector table: once a selector is installed it
 * stays resident for the life of the system.  We deliberately keep this
 * fixed-size and heap-free, because the ROM never freed entries either. */
static GestaltEntry gTable[GESTALT_MAX_ENTRIES];
static Boolean gInitialized = false;

/* Helper: Find index of selector in table.
 * ---------------------------------------
 * Linear scan is fine – the real System 7 typically registered fewer than 40
 * selectors. */
static SInt32 find_index(OSType selector) {
    SInt32 i;

    for (i = 0; i < GESTALT_MAX_ENTRIES; i++) {
        if (gTable[i].inUse && gTable[i].selector == selector) {
            return i;
        }
    }

    return -1;  /* Not found */
}

/* Helper: First-fit free slot search.  The ROM’s GestaltGlue code behaved the
 * same way, so we inherit the behaviour (keeping the table densely packed). */
static SInt32 find_free_slot(void) {
    SInt32 i;

    for (i = 0; i < GESTALT_MAX_ENTRIES; i++) {
        if (!gTable[i].inUse) {
            return i;
        }
    }

    return -1;  /* Table full */
}

/* Initialize Gestalt Manager */
OSErr Gestalt_Init(void) {
    SInt32 i;

    /* Clear table */
    for (i = 0; i < GESTALT_MAX_ENTRIES; i++) {
        gTable[i].selector = 0;
        gTable[i].proc = NULL;
        gTable[i].inUse = false;
    }

    /* Must be initialized before registering selectors */
    gInitialized = true;

    /* Register built-in selectors */
    Gestalt_Register_Builtins();

    serial_puts("[Gestalt] Manager initialized\n");

    return noErr;
}

/* Shutdown Gestalt Manager */
void Gestalt_Shutdown(void) {
    SInt32 i;

    /* Clear table */
    for (i = 0; i < GESTALT_MAX_ENTRIES; i++) {
        gTable[i].selector = 0;
        gTable[i].proc = NULL;
        gTable[i].inUse = false;
    }

    gInitialized = false;
    serial_puts("[Gestalt] Manager shutdown\n");
}

/* Query a Gestalt selector */
OSErr Gestalt(OSType selector, long *response) {
    SInt32 index;

    /* Validate parameters */
    if (!response) {
        return paramErr;
    }

    /* Check initialized */
    if (!gInitialized) {
        return unimpErr;
    }

    /* Find selector */
    index = find_index(selector);
    if (index < 0) {
        return gestaltUnknownErr;
    }

    /* Call the proc */
    if (gTable[index].proc) {
        return gTable[index].proc(response);
    }

    return gestaltUnknownErr;
}

/* Register a new Gestalt selector */
OSErr NewGestalt(OSType selector, GestaltProc proc) {
    SInt32 index;

    /* Validate parameters */
    if (!proc) {
        return paramErr;
    }

    /* Check initialized */
    if (!gInitialized) {
        return unimpErr;
    }

    /* Check if already exists */
    index = find_index(selector);
    if (index >= 0) {
        return gestaltDupSelectorErr;
    }

    /* Find free slot */
    index = find_free_slot();
    if (index < 0) {
        return gestaltTableFullErr;  /* Table full */
    }

    /* Register it */
    gTable[index].selector = selector;
    gTable[index].proc = proc;
    gTable[index].inUse = true;

    return noErr;
}

/* Replace an existing Gestalt selector */
OSErr ReplaceGestalt(OSType selector, GestaltProc proc) {
    SInt32 index;

    /* Validate parameters */
    if (!proc) {
        return paramErr;
    }

    /* Check initialized */
    if (!gInitialized) {
        return unimpErr;
    }

    /* Find selector */
    index = find_index(selector);
    if (index < 0) {
        return gestaltUnknownErr;
    }

    /* Replace the proc */
    gTable[index].proc = proc;

    return noErr;
}

/* Check if a selector exists */
Boolean Gestalt_Has(OSType selector) {
    SInt32 index;

    /* Check initialized */
    if (!gInitialized) {
        return false;
    }

    /* Find selector */
    index = find_index(selector);

    return (index >= 0);
}

/* Get System Environment (simplified version) */
OSErr GetSysEnv(short versionRequested, SysEnvRec *answer) {
    long value;

    /* Validate parameters */
    if (!answer) {
        return paramErr;
    }

    /* We only support version 1 */
    if (versionRequested != 1) {
        return envBadVers;
    }

    /* Clear structure */
    answer->machineType = 0;
    answer->systemVersion = 0;
    answer->hasFPU = 0;
    answer->hasMMU = 0;

    /* Get machine type from Gestalt */
    if (Gestalt(gestaltMachineType, &value) == noErr) {
        answer->machineType = (UInt16)value;
    }

    /* Get system version from Gestalt */
    if (Gestalt(gestaltSystemVersion, &value) == noErr) {
        answer->systemVersion = (UInt32)value;
    }

    /* Get FPU status from Gestalt */
    if (Gestalt(gestaltFPUType, &value) == noErr) {
        answer->hasFPU = (value != 0) ? 1 : 0;
    }

    /* Always report MMU present (protected mode) */
    answer->hasMMU = 1;

    return noErr;
}
