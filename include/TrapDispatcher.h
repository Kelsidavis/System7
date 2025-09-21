/*
 * TrapDispatcher.h
 *
 * Mac OS 7.1 Trap Dispatcher - Portable C Implementation
 * Converted from original 68k assembly in OS/TrapDispatcher/
 *
 * This module implements the core trap dispatch system that routes all Mac OS
 * system calls (A-line traps) and handles F-line traps for unimplemented opcodes.
 *
 * Copyright (c) 1982-1993 Apple Computer, Inc.
 * Portable conversion for System7.1-Portable project
 */

#ifndef TRAP_DISPATCHER_H
#define TRAP_DISPATCHER_H

#include "SystemTypes.h"

#include "SystemTypes.h"

/* Trap dispatcher constants */
#define NUM_TOOLBOX_TRAPS    1024    /* Number of toolbox trap entries */
#define NUM_OS_TRAPS         256     /* Number of OS trap entries */
#define NUM_TRAP_MASK        0x3FF   /* Mask for trap number bits (1023) */
#define OS_TRAP_MASK         0xFF    /* Mask for OS trap numbers (255) */

/* A-line trap format: 1010 tabc nnnn nnnn
 * t=1 => Toolbox, t=0 => OS
 * a=1 => autoPop (extra return address to remove)
 * b,c => modifier bits for OS traps
 * nnnnnnnn => trap number
 */
#define TRAP_BASE            0xA000  /* Base of A-line trap range */
#define TOOLBOX_BASE         0xA800  /* Base of toolbox traps */
#define AUTOPOP_BIT          0x0400  /* Bit 10: autopop flag */
#define DONT_SAVE_A0_BIT     0x0100  /* Bit 8: don't save A0 for OS traps */
#define PRESERVE_REGS_BIT    0x0200  /* Bit 9: preserve D1-D2/A1 instead of D1-D2/A0-A1 */

/* Special trap numbers for dispatcher routines */
#define GET_TRAP_ADDRESS_TRAP  0x46  /* _GetTrapAddress */
#define SET_TRAP_ADDRESS_TRAP  0x47  /* _SetTrapAddress */

/* Trap address bits for Get/SetTrapAddress */
#define TRAP_NEW_BIT         9       /* Bit 9: new trap numbering */
#define TRAP_TOOLBOX_BIT     10      /* Bit 10: toolbox vs OS */

/* Come-from patch header constants */
#define COME_FROM_HEADER     0x60064EF9  /* Branch over JMP.L opcode */

#include "SystemTypes.h"

/* System error codes */
#define DS_CORE_ERR          12      /* Unimplemented core routine error */
#define DS_BAD_PATCH_HEADER  83      /* SetTrapAddress saw come-from header */

#include "SystemTypes.h"

/* CPU register context for trap dispatch */

/* Trap handler function pointer type */

/* Trap dispatch tables */

/* F-line trap context for unimplemented opcodes */

/* F-line handler function pointer type */

/* Public API Functions */

/**
 * Initialize the trap dispatcher system
 * Sets up trap tables and installs exception handlers
 * @return 0 on success, error code on failure
 */
int TrapDispatcher_Initialize(void);

/**
 * Cleanup the trap dispatcher system
 * Removes exception handlers and frees resources
 */
void TrapDispatcher_Cleanup(void);

/**
 * Main A-line trap dispatcher entry point
 * Called from exception handler for all A-line traps
 * @param context CPU context at time of trap
 * @return Result code from trap handler
 */
SInt32 TrapDispatcher_DispatchATrap(TrapContext *context);

/**
 * Main F-line trap dispatcher entry point
 * Called from exception handler for all F-line traps
 * @param context F-line trap context
 * @return Result code from trap handler
 */
SInt32 TrapDispatcher_DispatchFTrap(FLineTrapContext *context);

/**
 * Get the address of a trap handler
 * Implements _GetTrapAddress functionality
 * @param trap_number Trap number to look up
 * @param trap_word Original trap word with modifier bits
 * @return Pointer to trap handler, or NULL if not found
 */
TrapHandler TrapDispatcher_GetTrapAddress(UInt16 trap_number, UInt16 trap_word);

/**
 * Set the address of a trap handler
 * Implements _SetTrapAddress functionality
 * @param trap_number Trap number to set
 * @param trap_word Original trap word with modifier bits
 * @param handler New trap handler function
 * @return 0 on success, error code on failure
 */
int TrapDispatcher_SetTrapAddress(UInt16 trap_number, UInt16 trap_word, TrapHandler handler);

/**
 * Install F-line trap handler
 * @param handler Function to handle F-line traps
 * @return Previous handler, or NULL if none
 */
FLineTrapHandler TrapDispatcher_SetFLineHandler(FLineTrapHandler handler);

/**
 * Default bad trap handler
 * Called for unimplemented or invalid trap numbers
 * @param context CPU context at time of trap
 * @return Always returns DS_CORE_ERR
 */
SInt32 TrapDispatcher_BadTrap(TrapContext *context);

/**
 * Flush instruction cache if needed
 * Required after modifying trap handlers for self-modifying code
 */
void TrapDispatcher_FlushCache(void);

/**
 * Get pointer to trap dispatch tables
 * Used by system components that need direct access
 * @return Pointer to dispatch tables structure
 */
TrapDispatchTables* TrapDispatcher_GetTables(void);

/**
 * Initialize extended toolbox table for Mac Plus/SE compatibility
 * @return 0 on success, -1 on failure
 */
int TrapDispatcher_InitializeExtendedTable(void);

/**
 * Set cache flush function (platform-specific)
 * @param flush_func Function to call for cache flushing, or NULL for default
 */
void TrapDispatcher_SetCacheFlushFunction(void (*flush_func)(void));

/**
 * Get trap statistics (debugging/diagnostic function)
 * @param num_toolbox Pointer to receive number of toolbox traps (can be NULL)
 * @param num_os Pointer to receive number of OS traps (can be NULL)
 * @param num_extended Pointer to receive number of extended traps (can be NULL)
 */
void TrapDispatcher_GetStatistics(int *num_toolbox, int *num_os, int *num_extended);

/**
 * Validate trap dispatcher state (debugging function)
 * @return true if state is valid, false otherwise
 */
Boolean TrapDispatcher_ValidateState(void);

/**
 * _GetTrapAddress trap handler implementation
 * @param context CPU context with trap parameters
 * @return Always returns 0 (noErr)
 */
SInt32 TrapDispatcher_GetTrapAddressTrap(TrapContext *context);

/**
 * _SetTrapAddress trap handler implementation
 * @param context CPU context with trap parameters
 * @return 0 on success, error code on failure
 */
SInt32 TrapDispatcher_SetTrapAddressTrap(TrapContext *context);

/**
 * Check if trap number is a toolbox trap
 * @param trap_word The A-line trap instruction word
 * @return true if toolbox trap, false if OS trap
 */
static inline Boolean TrapDispatcher_IsToolboxTrap(UInt16 trap_word) {
    return (trap_word & 0x0800) != 0;  /* Bit 11 set for toolbox */
}

/**
 * Check if trap has autopop flag set
 * @param trap_word The A-line trap instruction word
 * @return true if autopop flag set
 */
static inline Boolean TrapDispatcher_HasAutoPop(UInt16 trap_word) {
    return (trap_word & AUTOPOP_BIT) != 0;
}

/**
 * Extract trap number from trap word
 * @param trap_word The A-line trap instruction word
 * @return Trap number (0-1023 for toolbox, 0-255 for OS)
 */
static inline UInt16 TrapDispatcher_GetTrapNumber(UInt16 trap_word) {
    if (TrapDispatcher_IsToolboxTrap(trap_word)) {
        return ((trap_word - TOOLBOX_BASE) & NUM_TRAP_MASK);
    } else {
        return ((trap_word - TRAP_BASE) & OS_TRAP_MASK);
    }
}

/* Internal helper functions (for system use only) */
TrapHandler* TrapDispatcher_FindTableEntry(UInt16 trap_number, UInt16 trap_word);
int TrapDispatcher_InstallTrapTables(void);
void TrapDispatcher_InitializeTrapTables(void);

#endif /* TRAP_DISPATCHER_H */