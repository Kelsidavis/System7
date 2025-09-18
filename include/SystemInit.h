/*
 * System 7.1 Portable - System Initialization
 *
 * This module handles critical system initialization including:
 * - ExpandMem (extended memory globals) setup
 * - Input system initialization (ADB abstraction)
 * - Resource decompression hook installation
 * - System boot sequence coordination
 *
 * Based on System 7.1 BeforePatches.a assembly code
 * Copyright (c) 2024 - Portable Mac OS Project
 */

#ifndef SYSTEMINIT_H
#define SYSTEMINIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct ExpandMemRec ExpandMemRec;
typedef struct SystemGlobals SystemGlobals;

/* System version information */
#define SYSTEM_VERSION      0x0710  /* System 7.1 */
#define SYSTEM_VERSION_BCD  0x0701  /* BCD format */

/* Error codes */
typedef enum {
    SYS_OK = 0,
    SYS_ERR_NO_MEMORY = -1,
    SYS_ERR_RESOURCE_NOT_FOUND = -2,
    SYS_ERR_INITIALIZATION_FAILED = -3,
    SYS_ERR_INVALID_CONFIGURATION = -4,
    SYS_ERR_HARDWARE_NOT_SUPPORTED = -5,
    SYS_ERR_FATAL = -100
} SystemError;

/* System initialization stages */
typedef enum {
    INIT_STAGE_EARLY = 0,      /* Very early boot - minimal setup */
    INIT_STAGE_MEMORY,         /* Memory manager initialization */
    INIT_STAGE_EXPANDMEM,      /* ExpandMem setup */
    INIT_STAGE_RESOURCES,      /* Resource manager and decompression */
    INIT_STAGE_INPUT,          /* Input system (ADB/keyboard/mouse) */
    INIT_STAGE_FILESYSTEM,     /* File system initialization */
    INIT_STAGE_PROCESS,        /* Process manager preparation */
    INIT_STAGE_COMPLETE        /* System fully initialized */
} SystemInitStage;

/* Boot configuration flags */
typedef struct {
    bool verbose_boot;         /* Show boot progress */
    bool safe_mode;           /* Safe boot mode */
    bool disable_extensions;  /* Don't load extensions */
    bool force_32bit;         /* Force 32-bit addressing */
    bool disable_vm;          /* Disable virtual memory */
    uint32_t reserved_flags;  /* Reserved for future use */
} BootConfiguration;

/* System capabilities (detected hardware features) */
typedef struct {
    bool has_color_quickdraw;
    bool has_fpu;
    bool has_mmu;
    bool has_adb;            /* Apple Desktop Bus */
    bool has_scsi;
    bool has_ethernet;
    bool has_sound_manager;
    bool has_power_manager;
    uint32_t cpu_type;       /* 68000, 68020, 68030, 68040, PPC, etc. */
    uint32_t ram_size;       /* Total RAM in bytes */
    uint32_t rom_version;    /* ROM version */
} SystemCapabilities;

/* System initialization callbacks */
typedef struct {
    void (*stage_complete)(SystemInitStage stage);
    void (*error_handler)(SystemError error, const char* message);
    void (*progress_update)(const char* message, int percent);
    void* user_data;
} SystemInitCallbacks;

/* Main system initialization API */

/**
 * Initialize the Mac OS 7.1 portable system
 * This is the main entry point for system initialization
 *
 * @param config Boot configuration options (can be NULL for defaults)
 * @param callbacks Optional callbacks for initialization progress
 * @return SYS_OK on success, error code on failure
 */
SystemError SystemInit(const BootConfiguration* config,
                       const SystemInitCallbacks* callbacks);

/**
 * Initialize system to a specific stage
 * Allows partial initialization for testing or special boot modes
 *
 * @param target_stage Stage to initialize up to
 * @return SYS_OK on success, error code on failure
 */
SystemError SystemInitToStage(SystemInitStage target_stage);

/**
 * Get current system initialization stage
 *
 * @return Current initialization stage
 */
SystemInitStage SystemGetInitStage(void);

/**
 * Get system capabilities (detected hardware features)
 *
 * @param caps Pointer to capabilities structure to fill
 * @return SYS_OK on success
 */
SystemError SystemGetCapabilities(SystemCapabilities* caps);

/**
 * Get system globals structure
 * Returns the main system globals used throughout Mac OS
 *
 * @return Pointer to system globals (never NULL after init)
 */
SystemGlobals* SystemGetGlobals(void);

/**
 * Get ExpandMem structure
 * Returns the extended memory globals structure
 *
 * @return Pointer to ExpandMem (NULL if not initialized)
 */
ExpandMemRec* SystemGetExpandMem(void);

/* System shutdown and cleanup */

/**
 * Shutdown the system cleanly
 * Performs orderly shutdown of all subsystems
 *
 * @param restart If true, prepare for restart instead of poweroff
 * @return SYS_OK on success
 */
SystemError SystemShutdown(bool restart);

/**
 * Emergency system halt
 * Used for fatal errors - minimal cleanup
 *
 * @param error_code System error code that caused the halt
 * @param message Optional error message
 */
void SystemPanic(SystemError error_code, const char* message);

/* Debugging and diagnostics */

/**
 * Dump system state for debugging
 *
 * @param output_func Function to output debug text
 */
void SystemDumpState(void (*output_func)(const char* text));

/**
 * Validate system integrity
 * Performs self-tests on critical system structures
 *
 * @return SYS_OK if system is healthy
 */
SystemError SystemValidate(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEMINIT_H */