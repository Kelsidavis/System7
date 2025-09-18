/*
 * TrapDispatcher.c
 *
 * Mac OS 7.1 Trap Dispatcher - Portable C Implementation
 * Converted from original 68k assembly in OS/TrapDispatcher/Dispatch.a
 *
 * This module implements the core trap dispatch system that routes all Mac OS
 * system calls (A-line traps) and handles F-line traps for unimplemented opcodes.
 *
 * Original assembly code written by Andy Hertzfeld (July 15, 1982)
 * Re-written and optimized by Gary G. Davidian (November 7, 1988)
 * Portable C conversion for System7.1-Portable project
 *
 * Key features implemented:
 * - A-line trap dispatch for both OS and Toolbox calls
 * - F-line trap handling for unimplemented opcodes
 * - Register context preservation per Mac OS conventions
 * - Trap patching mechanism with come-from header support
 * - Extended trap table support for Mac Plus/SE compatibility
 * - Cache flushing for self-modifying code compatibility
 *
 * Copyright (c) 1982-1993 Apple Computer, Inc.
 * Portable conversion for System7.1-Portable project
 */

#include "TrapDispatcher.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Global trap dispatch tables */
static TrapDispatchTables g_trap_tables = {0};
static FLineTrapHandler g_fline_handler = NULL;
static bool g_dispatcher_active = false;

/* Forward declarations */
static int32_t dispatch_toolbox_trap(TrapContext *context, uint16_t trap_word);
static int32_t dispatch_os_trap(TrapContext *context, uint16_t trap_word);
static TrapHandler* find_table_entry(uint16_t trap_number, uint16_t trap_word);
static bool is_come_from_patch(TrapHandler handler);
static TrapHandler resolve_come_from_patches(TrapHandler handler);
static void default_cache_flush(void);
static int32_t unimplemented_trap_handler(TrapContext *context);

/* Trap handler implementations */
int32_t TrapDispatcher_GetTrapAddressTrap(TrapContext *context);
int32_t TrapDispatcher_SetTrapAddressTrap(TrapContext *context);

/* System error reporting function - should be provided by system */
extern void SysError(int error_code);

/**
 * Initialize the trap dispatcher system
 */
int TrapDispatcher_Initialize(void) {
    if (g_trap_tables.initialized) {
        return 0;  /* Already initialized */
    }

    /* Clear all trap tables */
    memset(&g_trap_tables, 0, sizeof(TrapDispatchTables));

    /* Initialize all trap handlers to BadTrap */
    g_trap_tables.bad_trap_handler = TrapDispatcher_BadTrap;

    for (int i = 0; i < NUM_TOOLBOX_TRAPS; i++) {
        g_trap_tables.toolbox_table[i] = g_trap_tables.bad_trap_handler;
    }

    for (int i = 0; i < NUM_OS_TRAPS; i++) {
        g_trap_tables.os_table[i] = g_trap_tables.bad_trap_handler;
    }

    /* Set up built-in trap handlers */
    g_trap_tables.os_table[GET_TRAP_ADDRESS_TRAP] = (TrapHandler)TrapDispatcher_GetTrapAddressTrap;
    g_trap_tables.os_table[SET_TRAP_ADDRESS_TRAP] = (TrapHandler)TrapDispatcher_SetTrapAddressTrap;

    /* Initialize cache flush function */
    g_trap_tables.cache_flush = default_cache_flush;

    /* No extended table initially */
    g_trap_tables.extended_toolbox_table = NULL;
    g_trap_tables.has_extended_table = false;

    g_trap_tables.initialized = true;
    g_dispatcher_active = true;

    return 0;
}

/**
 * Cleanup the trap dispatcher system
 */
void TrapDispatcher_Cleanup(void) {
    if (!g_trap_tables.initialized) {
        return;
    }

    /* Free extended table if allocated */
    if (g_trap_tables.extended_toolbox_table) {
        free(g_trap_tables.extended_toolbox_table);
        g_trap_tables.extended_toolbox_table = NULL;
    }

    g_trap_tables.initialized = false;
    g_dispatcher_active = false;
}

/**
 * Main A-line trap dispatcher entry point
 * This is the heart of the Mac OS trap system
 */
int32_t TrapDispatcher_DispatchATrap(TrapContext *context) {
    if (!g_dispatcher_active) {
        return DS_CORE_ERR;
    }

    /* Extract the trap instruction from the PC */
    uint16_t trap_word = *(uint16_t*)(uintptr_t)(context->pc);

    /* Advance PC past the trap instruction */
    context->pc += 2;

    /* Check if this is a toolbox or OS trap */
    if (TrapDispatcher_IsToolboxTrap(trap_word)) {
        return dispatch_toolbox_trap(context, trap_word);
    } else {
        return dispatch_os_trap(context, trap_word);
    }
}

/**
 * Dispatch a toolbox trap (A800-AFFF range)
 */
static int32_t dispatch_toolbox_trap(TrapContext *context, uint16_t trap_word) {
    uint16_t trap_number = TrapDispatcher_GetTrapNumber(trap_word);
    bool has_autopop = TrapDispatcher_HasAutoPop(trap_word);
    TrapHandler handler;

    /* Handle extended trap table if needed */
    if (g_trap_tables.has_extended_table && trap_number >= 512) {
        /* Extended toolbox trap */
        uint16_t extended_index = trap_number - 512;
        if (extended_index < 512 && g_trap_tables.extended_toolbox_table) {
            handler = g_trap_tables.extended_toolbox_table[extended_index];
        } else {
            handler = g_trap_tables.bad_trap_handler;
        }
    } else {
        /* Standard toolbox trap */
        if (trap_number < NUM_TOOLBOX_TRAPS) {
            handler = g_trap_tables.toolbox_table[trap_number];
        } else {
            handler = g_trap_tables.bad_trap_handler;
        }
    }

    /* Resolve any come-from patches */
    handler = resolve_come_from_patches(handler);

    /* Toolbox traps follow Pascal register conventions:
     * - All registers preserved except D0-D2/A0-A1
     * - Parameters passed on stack
     * - Return value in D0
     */

    /* Save the trap word for handlers that need it */
    uint16_t saved_d1 = (uint16_t)context->d1;
    context->d1 = trap_word;

    /* Call the handler */
    int32_t result = handler(context);

    /* Handle autopop if required */
    if (has_autopop) {
        /* Remove extra return address from stack */
        context->a7 += 4;  /* Pop one long word */
    }

    /* Restore D1 if it wasn't modified by the handler */
    if ((context->d1 & 0xFFFF) == trap_word) {
        context->d1 = (context->d1 & 0xFFFF0000) | saved_d1;
    }

    /* Set condition codes based on D0.W */
    if (result & 0xFFFF) {
        context->sr &= ~0x04;  /* Clear Z flag */
    } else {
        context->sr |= 0x04;   /* Set Z flag */
    }

    return result;
}

/**
 * Dispatch an OS trap (A000-A7FF range)
 */
static int32_t dispatch_os_trap(TrapContext *context, uint16_t trap_word) {
    uint16_t trap_number = TrapDispatcher_GetTrapNumber(trap_word);
    TrapHandler handler;

    /* Note: Register preservation flags are handled by the calling emulator */

    /* Get the trap handler */
    if (trap_number < NUM_OS_TRAPS) {
        handler = g_trap_tables.os_table[trap_number];
    } else {
        handler = g_trap_tables.bad_trap_handler;
    }

    /* Resolve any come-from patches */
    handler = resolve_come_from_patches(handler);

    /* OS traps follow different register conventions:
     * - Parameters passed in registers
     * - D1.W contains the trap word
     * - Register saving depends on modifier bits
     */

    /* Always pass trap word in D1 */
    context->d1 = trap_word;

    /* OS trap register preservation rules:
     * - If DONT_SAVE_A0_BIT is set: preserve D1-D2/A1 (A0 not saved)
     * - If PRESERVE_REGS_BIT is set: preserve D1-D2/A1 instead of D1-D2/A0-A1
     * - Otherwise: preserve D1-D2/A0-A1 (standard case)
     */

    /* Call the handler */
    int32_t result = handler(context);

    /* Store result in D0 */
    context->d0 = result;

    /* Set condition codes based on D0.W */
    uint16_t result_word = result & 0xFFFF;
    context->sr &= ~0x0F;  /* Clear N, Z, V, C flags */

    if (result_word == 0) {
        context->sr |= 0x04;  /* Set Z flag */
    } else if (result_word & 0x8000) {
        context->sr |= 0x08;  /* Set N flag */
    }

    return result;
}

/**
 * Main F-line trap dispatcher entry point
 */
int32_t TrapDispatcher_DispatchFTrap(FLineTrapContext *context) {
    if (g_fline_handler) {
        return g_fline_handler(context);
    }

    /* Default F-line trap handling - treat as unimplemented instruction */
    return TrapDispatcher_BadTrap(context->cpu_ctx);
}

/**
 * Get the address of a trap handler (implements _GetTrapAddress)
 */
TrapHandler TrapDispatcher_GetTrapAddress(uint16_t trap_number, uint16_t trap_word) {
    TrapHandler* entry = find_table_entry(trap_number, trap_word);
    if (!entry) {
        return g_trap_tables.bad_trap_handler;
    }

    TrapHandler handler = *entry;
    return resolve_come_from_patches(handler);
}

/**
 * _GetTrapAddress trap handler
 */
int32_t TrapDispatcher_GetTrapAddressTrap(TrapContext *context) {
    uint16_t trap_number = context->d0 & 0xFFFF;
    uint16_t trap_word = context->d1 & 0xFFFF;

    TrapHandler handler = TrapDispatcher_GetTrapAddress(trap_number, trap_word);
    context->a0 = (uint32_t)(uintptr_t)handler;
    return 0;  /* noErr */
}

/**
 * Set the address of a trap handler (implements _SetTrapAddress)
 */
int TrapDispatcher_SetTrapAddress(uint16_t trap_number, uint16_t trap_word, TrapHandler handler) {
    /* Check for illegal come-from header in new handler */
    if (is_come_from_patch(handler)) {
        SysError(DS_BAD_PATCH_HEADER);
        return DS_BAD_PATCH_HEADER;
    }

    TrapHandler* entry = find_table_entry(trap_number, trap_word);
    if (!entry) {
        return DS_CORE_ERR;
    }

    /* Handle come-from patches in existing handler chain */
    TrapHandler current = *entry;
    TrapHandler* patch_target = entry;

    /* Walk the come-from chain to find the final patch point */
    while (is_come_from_patch(current)) {
        patch_target = (TrapHandler*)((uint8_t*)current + 4);  /* Skip header, point to address */
        current = *patch_target;
    }

    /* Install the new handler */
    *patch_target = handler;

    /* Flush instruction cache to ensure coherency */
    if (g_trap_tables.cache_flush) {
        g_trap_tables.cache_flush();
    }

    return 0;  /* noErr */
}

/**
 * _SetTrapAddress trap handler
 */
int32_t TrapDispatcher_SetTrapAddressTrap(TrapContext *context) {
    uint16_t trap_number = context->d0 & 0xFFFF;
    uint16_t trap_word = context->d1 & 0xFFFF;
    TrapHandler handler = (TrapHandler)(uintptr_t)context->a0;

    int result = TrapDispatcher_SetTrapAddress(trap_number, trap_word, handler);
    return result;
}

/**
 * Install F-line trap handler
 */
FLineTrapHandler TrapDispatcher_SetFLineHandler(FLineTrapHandler handler) {
    FLineTrapHandler old_handler = g_fline_handler;
    g_fline_handler = handler;
    return old_handler;
}

/**
 * Default bad trap handler for unimplemented traps
 */
int32_t TrapDispatcher_BadTrap(TrapContext *context) {
    /* Save all registers in debugger space - this would be system-specific */
    printf("BadTrap: Unimplemented trap at PC=0x%08X, trap_word=0x%04X\n",
           context->pc - 2, *(uint16_t*)(uintptr_t)(context->pc - 2));

    /* In original Mac OS, this would call SysError with DS_CORE_ERR */
    SysError(DS_CORE_ERR);
    return DS_CORE_ERR;
}

/**
 * Flush instruction cache (placeholder for platform-specific implementation)
 */
void TrapDispatcher_FlushCache(void) {
    if (g_trap_tables.cache_flush) {
        g_trap_tables.cache_flush();
    }
}

/**
 * Get pointer to trap dispatch tables
 */
TrapDispatchTables* TrapDispatcher_GetTables(void) {
    return &g_trap_tables;
}

/**
 * Find the table entry for a given trap number and word
 */
static TrapHandler* find_table_entry(uint16_t trap_number, uint16_t trap_word) {
    /* Decode trap addressing mode */
    bool is_new_format = (trap_word & (1 << TRAP_NEW_BIT)) != 0;
    bool is_toolbox = (trap_word & (1 << TRAP_TOOLBOX_BIT)) != 0;

    if (is_new_format) {
        /* New format: bit 10 determines toolbox vs OS */
        if (is_toolbox) {
            /* New toolbox trap */
            trap_number &= NUM_TRAP_MASK;
            if (trap_number >= NUM_TOOLBOX_TRAPS) return NULL;

            /* Check for extended table */
            if (g_trap_tables.has_extended_table && trap_number >= 512) {
                uint16_t ext_index = trap_number - 512;
                if (ext_index < 512 && g_trap_tables.extended_toolbox_table) {
                    return &g_trap_tables.extended_toolbox_table[ext_index];
                }
            }

            return &g_trap_tables.toolbox_table[trap_number];
        } else {
            /* New OS trap */
            trap_number &= OS_TRAP_MASK;
            if (trap_number >= NUM_OS_TRAPS) return NULL;
            return &g_trap_tables.os_table[trap_number];
        }
    } else {
        /* Old format: use original trap numbering scheme */
        trap_number &= 0x01FF;  /* 9 bits */

        if (trap_number <= 0x004F || trap_number == 0x0054 || trap_number == 0x0057) {
            /* OS traps: 0x00-0x4F, 0x54, 0x57 */
            trap_number &= OS_TRAP_MASK;
            if (trap_number >= NUM_OS_TRAPS) return NULL;
            return &g_trap_tables.os_table[trap_number];
        } else {
            /* Toolbox traps: everything else */
            if (trap_number >= NUM_TOOLBOX_TRAPS) return NULL;
            return &g_trap_tables.toolbox_table[trap_number];
        }
    }
}

/**
 * Check if a trap handler has a come-from patch header
 */
static bool is_come_from_patch(TrapHandler handler) {
    if (!handler) return false;

    /* Check for come-from header: BRA.S +6 followed by JMP.L */
    uint32_t header = *(uint32_t*)handler;
    return header == COME_FROM_HEADER;
}

/**
 * Resolve come-from patches to get the final handler
 */
static TrapHandler resolve_come_from_patches(TrapHandler handler) {
    if (!handler) return handler;

    /* Walk the come-from chain */
    while (is_come_from_patch(handler)) {
        /* Skip the header (BRA.S +6, JMP.L) and get the target address */
        handler = *(TrapHandler*)((uint8_t*)handler + 4);
    }

    return handler;
}

/**
 * Default cache flush implementation (no-op on most modern platforms)
 */
static void default_cache_flush(void) {
    /* On modern platforms, instruction cache coherency is usually automatic.
     * Platform-specific implementations can override this via the cache_flush pointer.
     */
#ifdef __GNUC__
    __builtin___clear_cache(0, (void*)-1);  /* GCC builtin for cache flush */
#endif
}

/**
 * Unimplemented trap handler (used as default for unused trap slots)
 */
static int32_t unimplemented_trap_handler(TrapContext *context) {
    return TrapDispatcher_BadTrap(context);
}

/* Weak symbol definitions for system functions that should be provided externally */
__attribute__((weak)) void SysError(int error_code) {
    fprintf(stderr, "SYSTEM ERROR %d: Unimplemented trap or critical failure\n", error_code);
    abort();
}

/**
 * Initialize extended toolbox table for Mac Plus/SE compatibility
 * This adds 512 additional toolbox trap slots beyond the standard 512
 */
int TrapDispatcher_InitializeExtendedTable(void) {
    if (g_trap_tables.has_extended_table) {
        return 0;  /* Already initialized */
    }

    /* Allocate memory for extended table */
    g_trap_tables.extended_toolbox_table = malloc(512 * sizeof(TrapHandler));
    if (!g_trap_tables.extended_toolbox_table) {
        return -1;  /* Memory allocation failed */
    }

    /* Initialize all entries to bad trap handler */
    for (int i = 0; i < 512; i++) {
        g_trap_tables.extended_toolbox_table[i] = g_trap_tables.bad_trap_handler;
    }

    g_trap_tables.has_extended_table = true;
    return 0;
}

/**
 * Set cache flush function (platform-specific)
 */
void TrapDispatcher_SetCacheFlushFunction(void (*flush_func)(void)) {
    g_trap_tables.cache_flush = flush_func ? flush_func : default_cache_flush;
}

/**
 * Get trap statistics (debugging/diagnostic function)
 */
void TrapDispatcher_GetStatistics(int *num_toolbox, int *num_os, int *num_extended) {
    if (num_toolbox) *num_toolbox = NUM_TOOLBOX_TRAPS;
    if (num_os) *num_os = NUM_OS_TRAPS;
    if (num_extended) *num_extended = g_trap_tables.has_extended_table ? 512 : 0;
}

/**
 * Validate trap dispatcher state (debugging function)
 */
bool TrapDispatcher_ValidateState(void) {
    if (!g_trap_tables.initialized) return false;
    if (!g_trap_tables.bad_trap_handler) return false;
    if (g_trap_tables.has_extended_table && !g_trap_tables.extended_toolbox_table) return false;
    return true;
}