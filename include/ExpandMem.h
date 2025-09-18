/*
 * System 7.1 Portable - ExpandMem (Extended Memory Globals)
 *
 * ExpandMem provides extended global variable space beyond the original
 * Mac OS low memory globals. It's essential for System 7.1's extended
 * functionality including international support, extended file system
 * features, and process management.
 *
 * Based on System 7.1 ExpandMemPriv.a definitions
 * Copyright (c) 2024 - Portable Mac OS Project
 */

#ifndef EXPANDMEM_H
#define EXPANDMEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ExpandMem version and sizing */
#define EM_CURRENT_VERSION      0x0200     /* Version 2.0 for System 7.1 */
#define EM_MIN_SIZE            0x1000      /* Minimum ExpandMem size (4KB) */
#define EM_STANDARD_SIZE       0x2000      /* Standard size (8KB) */
#define EM_EXTENDED_SIZE       0x4000      /* Extended size for future (16KB) */

/* Key cache constants for keyboard support */
#define KEY_CACHE_MIN          0x400       /* Minimum key cache size */
#define KEY_CACHE_SLOP         0x100       /* Extra space for safety */
#define KEY_DEAD_STATE_SIZE    16          /* Dead key state buffer */

/* ExpandMem Record Structure */
typedef struct ExpandMemRec {
    /* Header */
    uint16_t    emVersion;                 /* ExpandMem version */
    uint32_t    emSize;                    /* Size of this record */

    /* Keyboard and International Support */
    void*       emKeyCache;                 /* Pointer to keyboard cache (KCHR) */
    uint8_t     emKeyDeadState[KEY_DEAD_STATE_SIZE]; /* Dead key state */
    uint32_t    emKeyboardType;            /* Current keyboard type */
    uint32_t    emScriptCode;              /* Current script system */

    /* File System Extensions */
    void*       emFSQueueHook;              /* File system queue hook */
    void*       emFSSpecCache;              /* FSSpec cache for performance */
    uint32_t    emDefaultVolume;            /* Default volume reference */
    uint32_t    emBootVolume;               /* Boot volume reference */

    /* Process Manager Support */
    void*       emProcessMgrGlobals;        /* Process Manager globals */
    void*       emCurrentProcess;            /* Current process control block */
    void*       emProcessList;               /* Head of process list */
    uint32_t    emProcessCount;             /* Number of active processes */

    /* Memory Manager Extensions */
    void*       emHeapCheck;                /* Heap check routine */
    void*       emHeapEnd;                  /* Extended heap end */
    uint32_t    emPhysicalRAMSize;          /* Total physical RAM */
    uint32_t    emLogicalRAMSize;           /* Logical RAM size (with VM) */

    /* AppleTalk and Networking */
    bool        emAppleTalkInactiveOnBoot;  /* AppleTalk was inactive at boot */
    void*       emATalkGlobals;             /* AppleTalk globals */
    uint32_t    emNetworkConfig;            /* Network configuration flags */

    /* Resource Manager Extensions */
    void*       emResourceCache;            /* Resource cache for performance */
    void*       emDecompressor;              /* Decompression defProc */
    uint32_t    emResourceLoadFlags;        /* Resource loading flags */

    /* QuickDraw Extensions */
    void*       emQDExtensions;             /* QuickDraw GX/3D extensions */
    void*       emColorTable;               /* System color table */
    uint32_t    emScreenCount;              /* Number of screens */

    /* Sound Manager */
    void*       emSoundGlobals;             /* Sound Manager globals */
    uint32_t    emSoundChannels;            /* Number of sound channels */

    /* Power Management */
    void*       emPowerMgrGlobals;          /* Power Manager globals */
    uint32_t    emSleepTime;                /* System sleep timer */
    uint32_t    emBatteryLevel;             /* Battery level (portable Macs) */

    /* Alias Manager */
    void*       emAliasGlobals;             /* Alias Manager globals */

    /* Edition Manager */
    void*       emEditionGlobals;           /* Edition Manager globals */

    /* Component Manager */
    void*       emComponentGlobals;         /* Component Manager globals */

    /* Thread Manager (future) */
    void*       emThreadGlobals;            /* Thread Manager globals */

    /* System Error Handling */
    void*       emSystemErrorProc;          /* System error handler */
    uint32_t    emLastSystemError;          /* Last system error code */

    /* Debugging Support */
    void*       emDebuggerGlobals;          /* MacsBug globals */
    uint32_t    emDebugFlags;               /* System debug flags */

    /* Gestalt Extensions */
    void*       emGestaltTable;             /* Gestalt selector table */

    /* Time Manager Extensions */
    void*       emTimeMgrExtensions;        /* Extended Time Manager */

    /* Notification Manager */
    void*       emNotificationGlobals;      /* Notification Manager globals */

    /* Help Manager */
    void*       emHelpGlobals;              /* Help Manager globals */

    /* PPC Toolbox */
    void*       emPPCGlobals;               /* Program-to-Program Communication */

    /* Virtual Memory */
    void*       emVMGlobals;                /* Virtual Memory globals */
    bool        emVMEnabled;                /* VM is enabled */
    uint32_t    emVMPageSize;               /* VM page size */

    /* Display Manager */
    void*       emDisplayMgrGlobals;        /* Display Manager globals */

    /* Reserved space for future expansion */
    uint8_t     emReserved[1024];           /* Reserved for future use */

} ExpandMemRec;

/* ExpandMem API Functions */

/**
 * Initialize ExpandMem structure
 * Called early in system initialization
 *
 * @param size Size of ExpandMem to allocate (0 for default)
 * @return Pointer to initialized ExpandMem, NULL on failure
 */
ExpandMemRec* ExpandMemInit(size_t size);

/**
 * Get current ExpandMem pointer
 *
 * @return Current ExpandMem pointer (NULL if not initialized)
 */
ExpandMemRec* ExpandMemGet(void);

/**
 * Extend ExpandMem size
 * Used when system components need more global space
 *
 * @param new_size New size required
 * @return true on success, false on failure
 */
bool ExpandMemExtend(size_t new_size);

/**
 * Initialize keyboard cache in ExpandMem
 * Sets up KCHR resource and dead key state
 *
 * @param em ExpandMem pointer
 * @param kchr_id KCHR resource ID to load (0 for default)
 * @return true on success
 */
bool ExpandMemInitKeyboard(ExpandMemRec* em, int16_t kchr_id);

/**
 * Set AppleTalk inactive flag
 * Called during boot if AppleTalk is not configured
 *
 * @param em ExpandMem pointer
 * @param inactive true if AppleTalk is inactive
 */
void ExpandMemSetAppleTalkInactive(ExpandMemRec* em, bool inactive);

/**
 * Install decompressor in ExpandMem
 * Sets up resource decompression hook
 *
 * @param em ExpandMem pointer
 * @param decompressor Decompressor procedure pointer
 */
void ExpandMemInstallDecompressor(ExpandMemRec* em, void* decompressor);

/**
 * Validate ExpandMem integrity
 * Checks version, size, and critical pointers
 *
 * @param em ExpandMem pointer to validate
 * @return true if valid, false if corrupted
 */
bool ExpandMemValidate(const ExpandMemRec* em);

/**
 * Dump ExpandMem contents for debugging
 *
 * @param em ExpandMem pointer
 * @param output_func Function to output debug text
 */
void ExpandMemDump(const ExpandMemRec* em,
                   void (*output_func)(const char* text));

/**
 * Clean up ExpandMem on shutdown
 * Frees resources and cleans up allocations
 *
 * @param em ExpandMem pointer
 */
void ExpandMemCleanup(ExpandMemRec* em);

#ifdef __cplusplus
}
#endif

#endif /* EXPANDMEM_H */