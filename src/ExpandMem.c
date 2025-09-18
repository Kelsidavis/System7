/*
 * System 7.1 Portable - ExpandMem Implementation
 *
 * ExpandMem provides extended global variable space for System 7.1.
 * This implementation creates a portable version of the Mac OS
 * extended memory globals structure.
 *
 * Copyright (c) 2024 - Portable Mac OS Project
 */

#include "ExpandMem.h"
#include "ResourceManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Global ExpandMem pointer (simulates Mac OS low memory global) */
static ExpandMemRec* g_ExpandMem = NULL;

/* KCHR resource structure for keyboard layouts */
typedef struct {
    uint16_t version;
    uint8_t tableIndex[256];    /* Modifier combination to table mapping */
    uint16_t tableCount;
    uint8_t tables[1];          /* Variable size - actual keyboard tables */
} KCHRResource;

/* Dead key state for keyboard processing */
typedef struct {
    uint32_t deadKeyState;      /* Current dead key state */
    uint16_t lastKeyCode;       /* Last key code processed */
    uint16_t lastModifiers;     /* Last modifier state */
} KeyboardDeadState;

/* Internal helper functions */
static bool LoadDefaultKCHR(ExpandMemRec* em);
static void InitializeKeyboardState(ExpandMemRec* em);
static size_t CalculateExpandMemSize(size_t requested_size);

/* ============================================================================
 * ExpandMem Initialization
 * ============================================================================ */

ExpandMemRec* ExpandMemInit(size_t size) {
    ExpandMemRec* em;
    size_t actual_size;

    /* Calculate actual size needed */
    actual_size = CalculateExpandMemSize(size);
    if (actual_size < sizeof(ExpandMemRec)) {
        actual_size = sizeof(ExpandMemRec);
    }

    /* Allocate and clear ExpandMem */
    em = (ExpandMemRec*)calloc(1, actual_size);
    if (!em) {
        return NULL;
    }

    /* Initialize header */
    em->emVersion = EM_CURRENT_VERSION;
    em->emSize = actual_size;

    /* Initialize subsystem pointers to NULL */
    em->emKeyCache = NULL;
    em->emFSQueueHook = NULL;
    em->emFSSpecCache = NULL;
    em->emProcessMgrGlobals = NULL;
    em->emCurrentProcess = NULL;
    em->emProcessList = NULL;
    em->emResourceCache = NULL;
    em->emDecompressor = NULL;
    em->emQDExtensions = NULL;
    em->emColorTable = NULL;
    em->emSoundGlobals = NULL;
    em->emPowerMgrGlobals = NULL;
    em->emAliasGlobals = NULL;
    em->emEditionGlobals = NULL;
    em->emComponentGlobals = NULL;
    em->emThreadGlobals = NULL;
    em->emSystemErrorProc = NULL;
    em->emDebuggerGlobals = NULL;
    em->emGestaltTable = NULL;
    em->emTimeMgrExtensions = NULL;
    em->emNotificationGlobals = NULL;
    em->emHelpGlobals = NULL;
    em->emPPCGlobals = NULL;
    em->emVMGlobals = NULL;
    em->emDisplayMgrGlobals = NULL;

    /* Initialize counters and flags */
    em->emProcessCount = 0;
    em->emScreenCount = 1;  /* Assume at least one screen */
    em->emSoundChannels = 4;  /* Default sound channels */
    em->emAppleTalkInactiveOnBoot = false;
    em->emVMEnabled = false;
    em->emVMPageSize = 4096;  /* Default page size */

    /* Initialize keyboard dead state */
    memset(em->emKeyDeadState, 0, KEY_DEAD_STATE_SIZE);

    /* Store global pointer */
    g_ExpandMem = em;

    return em;
}

/* ============================================================================
 * ExpandMem Access
 * ============================================================================ */

ExpandMemRec* ExpandMemGet(void) {
    return g_ExpandMem;
}

/* ============================================================================
 * ExpandMem Extension
 * ============================================================================ */

bool ExpandMemExtend(size_t new_size) {
    ExpandMemRec* new_em;
    ExpandMemRec* old_em = g_ExpandMem;
    size_t copy_size;

    if (!old_em) {
        /* No existing ExpandMem - just initialize */
        new_em = ExpandMemInit(new_size);
        return (new_em != NULL);
    }

    /* Check if already large enough */
    if (old_em->emSize >= new_size) {
        return true;
    }

    /* Allocate new larger ExpandMem */
    new_em = (ExpandMemRec*)calloc(1, new_size);
    if (!new_em) {
        return false;
    }

    /* Copy existing data */
    copy_size = (old_em->emSize < new_size) ? old_em->emSize : new_size;
    memcpy(new_em, old_em, copy_size);

    /* Update size */
    new_em->emSize = new_size;

    /* Update global pointer */
    g_ExpandMem = new_em;

    /* Free old ExpandMem */
    free(old_em);

    return true;
}

/* ============================================================================
 * Keyboard Support
 * ============================================================================ */

bool ExpandMemInitKeyboard(ExpandMemRec* em, int16_t kchr_id) {
    if (!em) {
        return false;
    }

    /* If KCHR ID is 0, load default US keyboard */
    if (kchr_id == 0) {
        return LoadDefaultKCHR(em);
    }

    /* In a real implementation, would load KCHR from resources */
    /* For now, use default */
    return LoadDefaultKCHR(em);
}

static bool LoadDefaultKCHR(ExpandMemRec* em) {
    KCHRResource* kchr;
    size_t kchr_size;

    /* Calculate size for minimal KCHR */
    kchr_size = KEY_CACHE_MIN + KEY_CACHE_SLOP;

    /* Allocate key cache */
    em->emKeyCache = calloc(1, kchr_size);
    if (!em->emKeyCache) {
        return false;
    }

    /* Set up minimal KCHR structure */
    kchr = (KCHRResource*)em->emKeyCache;
    kchr->version = 0x0200;  /* KCHR version 2 */
    kchr->tableCount = 1;     /* One table for simplicity */

    /* Initialize table index (all modifiers use table 0) */
    memset(kchr->tableIndex, 0, 256);

    /* Initialize keyboard state */
    InitializeKeyboardState(em);

    /* Set keyboard type */
    em->emKeyboardType = 2;  /* Extended keyboard */
    em->emScriptCode = 0;    /* Roman script */

    return true;
}

static void InitializeKeyboardState(ExpandMemRec* em) {
    /* Clear dead key state */
    memset(em->emKeyDeadState, 0, KEY_DEAD_STATE_SIZE);

    /* Could initialize with specific dead key patterns here */
    /* For now, just clear it */
}

/* ============================================================================
 * AppleTalk Configuration
 * ============================================================================ */

void ExpandMemSetAppleTalkInactive(ExpandMemRec* em, bool inactive) {
    if (em) {
        em->emAppleTalkInactiveOnBoot = inactive;

        /* If AppleTalk is inactive, clear network globals */
        if (inactive) {
            em->emATalkGlobals = NULL;
            em->emNetworkConfig = 0;
        }
    }
}

/* ============================================================================
 * Decompressor Installation
 * ============================================================================ */

void ExpandMemInstallDecompressor(ExpandMemRec* em, void* decompressor) {
    if (em) {
        em->emDecompressor = decompressor;

        /* Set resource loading flags to enable decompression */
        em->emResourceLoadFlags |= 0x0001;  /* Enable decompression bit */
    }
}

/* ============================================================================
 * ExpandMem Validation
 * ============================================================================ */

bool ExpandMemValidate(const ExpandMemRec* em) {
    if (!em) {
        return false;
    }

    /* Check version */
    if (em->emVersion != EM_CURRENT_VERSION) {
        return false;
    }

    /* Check size is reasonable */
    if (em->emSize < sizeof(ExpandMemRec) ||
        em->emSize > EM_EXTENDED_SIZE * 2) {
        return false;
    }

    /* Validate critical pointers if they should be set */
    /* Based on what stage of init we're at, different pointers
     * should be valid */

    return true;
}

/* ============================================================================
 * ExpandMem Debugging
 * ============================================================================ */

void ExpandMemDump(const ExpandMemRec* em,
                   void (*output_func)(const char* text)) {
    char buffer[256];

    if (!em || !output_func) {
        return;
    }

    output_func("\n--- ExpandMem Contents ---\n");

    snprintf(buffer, sizeof(buffer),
             "Version: 0x%04X, Size: %u bytes\n",
             em->emVersion, (unsigned)em->emSize);
    output_func(buffer);

    /* Keyboard info */
    snprintf(buffer, sizeof(buffer),
             "Keyboard: Type=%u, Script=%u, KeyCache=%p\n",
             (unsigned)em->emKeyboardType,
             (unsigned)em->emScriptCode,
             em->emKeyCache);
    output_func(buffer);

    /* Process info */
    snprintf(buffer, sizeof(buffer),
             "Processes: Count=%u, Current=%p, List=%p\n",
             (unsigned)em->emProcessCount,
             em->emCurrentProcess,
             em->emProcessList);
    output_func(buffer);

    /* Memory info */
    snprintf(buffer, sizeof(buffer),
             "Memory: Physical=%u MB, Logical=%u MB\n",
             (unsigned)(em->emPhysicalRAMSize / (1024 * 1024)),
             (unsigned)(em->emLogicalRAMSize / (1024 * 1024)));
    output_func(buffer);

    /* Network info */
    snprintf(buffer, sizeof(buffer),
             "Network: AppleTalk %s, Config=0x%08X\n",
             em->emAppleTalkInactiveOnBoot ? "Inactive" : "Active",
             (unsigned)em->emNetworkConfig);
    output_func(buffer);

    /* VM info */
    snprintf(buffer, sizeof(buffer),
             "VM: %s, PageSize=%u\n",
             em->emVMEnabled ? "Enabled" : "Disabled",
             (unsigned)em->emVMPageSize);
    output_func(buffer);

    /* Resource Manager info */
    snprintf(buffer, sizeof(buffer),
             "Resources: Cache=%p, Decompressor=%p, Flags=0x%08X\n",
             em->emResourceCache,
             em->emDecompressor,
             (unsigned)em->emResourceLoadFlags);
    output_func(buffer);

    /* Display info */
    snprintf(buffer, sizeof(buffer),
             "Display: Screens=%u, ColorTable=%p\n",
             (unsigned)em->emScreenCount,
             em->emColorTable);
    output_func(buffer);

    /* Sound info */
    snprintf(buffer, sizeof(buffer),
             "Sound: Channels=%u, Globals=%p\n",
             (unsigned)em->emSoundChannels,
             em->emSoundGlobals);
    output_func(buffer);

    /* Error info */
    snprintf(buffer, sizeof(buffer),
             "Errors: LastError=0x%08X, ErrorProc=%p\n",
             (unsigned)em->emLastSystemError,
             em->emSystemErrorProc);
    output_func(buffer);

    /* File System info */
    snprintf(buffer, sizeof(buffer),
             "FileSystem: DefaultVol=%u, BootVol=%u\n",
             (unsigned)em->emDefaultVolume,
             (unsigned)em->emBootVolume);
    output_func(buffer);

    output_func("--- End ExpandMem ---\n");
}

/* ============================================================================
 * ExpandMem Cleanup
 * ============================================================================ */

void ExpandMemCleanup(ExpandMemRec* em) {
    if (!em) {
        return;
    }

    /* Free allocated subsystem memory */
    if (em->emKeyCache) {
        free(em->emKeyCache);
        em->emKeyCache = NULL;
    }

    if (em->emFSSpecCache) {
        free(em->emFSSpecCache);
        em->emFSSpecCache = NULL;
    }

    if (em->emResourceCache) {
        free(em->emResourceCache);
        em->emResourceCache = NULL;
    }

    /* Clean up subsystem globals */
    /* In a real implementation, would call cleanup functions
     * for each subsystem that has globals in ExpandMem */

    /* Clear sensitive data */
    memset(em->emKeyDeadState, 0, KEY_DEAD_STATE_SIZE);

    /* Clear global pointer if this is the current ExpandMem */
    if (g_ExpandMem == em) {
        g_ExpandMem = NULL;
    }

    /* Note: We don't free ExpandMem itself here - that's done by caller */
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static size_t CalculateExpandMemSize(size_t requested_size) {
    size_t min_size = sizeof(ExpandMemRec);

    /* Use default if no size specified */
    if (requested_size == 0) {
        return EM_STANDARD_SIZE;
    }

    /* Ensure minimum size */
    if (requested_size < min_size) {
        return min_size;
    }

    /* Round up to next 1KB boundary */
    return ((requested_size + 1023) / 1024) * 1024;
}