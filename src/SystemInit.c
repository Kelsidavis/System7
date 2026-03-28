#include "MemoryMgr/MemoryManager.h"
#include "SystemTypes.h"
#include "multiboot.h"
#include "System71StdLib.h"
#include "System/SystemLogging.h"
#include "Platform/Halt.h"
/*
 * System 7.1 Portable - System Initialization Implementation
 *
 * Core system initialization routines that set up the Mac OS 7.1
 * environment on modern platforms. This includes memory management,
 * resource loading, input systems, and subsystem initialization.
 *
 * Copyright (c) 2024 - Portable Mac OS Project
 */

#include "SystemInit.h"
#include "ExpandMem.h"
#include "ResourceManager.h"
#include "ResourceDecompression.h"
#include "FileManager.h"
#include <assert.h>

/* Define USE_THREADING if we want thread safety */
#ifdef PLATFORM_REMOVED_LINUX
#define USE_THREADING
#endif

/* Platform-specific includes */
#ifdef PLATFORM_REMOVED_WIN32
    #include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
    #include <signal.h>
    #include <unistd.h>
#endif

/* Extended System Globals Structure */
typedef struct SystemGlobalsExt {
    /* Core system state */
    SystemInitStage init_stage;
    SystemCapabilities capabilities;
    BootConfiguration boot_config;

    /* Memory management */
    void* system_heap;
    size_t system_heap_size;
    void* application_heap;
    size_t application_heap_size;

    /* Global pointers */
    ExpandMemRec* expand_mem;
    void* resource_mgr;  /* ResourceManager is opaque */
    void* file_mgr;      /* FileManager is opaque */

    /* System version and identification */
    UInt16 system_version;
    UInt32 system_build;
    char system_name[32];

    /* Error handling */
    SystemError last_error;
    char error_message[256];

    /* Callbacks */
    SystemInitCallbacks callbacks;

    /* Thread safety */
#ifdef USE_THREADING
    pthread_mutex_t globals_mutex;
#endif
} SystemGlobalsExt;

/* Define missing constants */
#define INIT_STAGE_EARLY          0
#define INIT_STAGE_MEMORY         1
#define INIT_STAGE_EXPANDMEM      2
#define INIT_STAGE_RESOURCES      3
#define INIT_STAGE_INPUT          4
#define INIT_STAGE_FILESYSTEM     5
#define INIT_STAGE_PROCESS        6
#define INIT_STAGE_COMPLETE       7

#define SYS_OK                    0
#define SYS_ERR_INVALID_CONFIGURATION -1
#define SYS_ERR_NO_MEMORY         -2
#define SYS_ERR_RESOURCE_NOT_FOUND -3
#define SYS_ERR_INITIALIZATION_FAILED -4
#define SYSTEM_VERSION            0x0710

#ifndef EM_STANDARD_SIZE
#define EM_STANDARD_SIZE 32
#endif
#ifndef EM_CURRENT_VERSION  
#define EM_CURRENT_VERSION 1
#endif

/* Static system globals instance */
static SystemGlobalsExt g_system = {
    .init_stage = INIT_STAGE_EARLY,
    .system_version = SYSTEM_VERSION,
    .system_name = "System 7.1 Portable"
};

/* Forward declarations for internal functions */
static SystemError InitializeMemory(void);
static SystemError InitializeExpandMem(void);
static SystemError InitializeResources(void);
static SystemError InitializeInputSystem(void);
static SystemError InitializeFileSystem(void);
static SystemError InitializeProcessManager(void);
static SystemError DetectHardwareCapabilities(void);
static SystemError LoadSystemResources(void);
static SystemError InstallDecompressor(void);
static void ReportProgress(const char* message, int percent);
static void ReportError(SystemError error, const char* message);

/* ============================================================================
 * Main System Initialization
 * ============================================================================ */

SystemError SystemInit(const BootConfiguration* config,
                       const SystemInitCallbacks* callbacks) {

    /* Store boot configuration */
    if (config) {
        memcpy(&g_system.boot_config, config, sizeof(BootConfiguration));
    } else {
        /* Use default configuration */
        memset(&g_system.boot_config, 0, sizeof(BootConfiguration));
    }

    /* Store callbacks */
    if (callbacks) {
        memcpy(&g_system.callbacks, callbacks, sizeof(SystemInitCallbacks));
    }

    /* Initialize to complete stage */
    return SystemInitToStage(INIT_STAGE_COMPLETE);
}

SystemError SystemInitToStage(SystemInitStage target_stage) {
    SystemError err = SYS_OK;

    /* Initialize each stage in sequence */
    while (g_system.init_stage < target_stage && err == SYS_OK) {
        switch (g_system.init_stage) {
            case INIT_STAGE_EARLY:
                ReportProgress("Detecting hardware capabilities...", 10);
                err = DetectHardwareCapabilities();
                if (err == SYS_OK) {
                    g_system.init_stage = INIT_STAGE_MEMORY;
                }
                break;

            case INIT_STAGE_MEMORY:
                ReportProgress("Initializing memory management...", 20);
                err = InitializeMemory();
                if (err == SYS_OK) {
                    g_system.init_stage = INIT_STAGE_EXPANDMEM;
                    /* If target is EXPANDMEM, we need to init it too */
                    if (target_stage == INIT_STAGE_EXPANDMEM) {
                        ReportProgress("Setting up ExpandMem globals...", 30);
                        err = InitializeExpandMem();
                    }
                }
                break;

            case INIT_STAGE_EXPANDMEM:
                ReportProgress("Setting up ExpandMem globals...", 30);
                err = InitializeExpandMem();
                if (err == SYS_OK) {
                    g_system.init_stage = INIT_STAGE_RESOURCES;
                }
                break;

            case INIT_STAGE_RESOURCES:
                ReportProgress("Initializing Resource Manager...", 40);
                err = InitializeResources();
                if (err == SYS_OK) {
                    g_system.init_stage = INIT_STAGE_INPUT;
                }
                break;

            case INIT_STAGE_INPUT:
                ReportProgress("Setting up input systems...", 50);
                err = InitializeInputSystem();
                if (err == SYS_OK) {
                    g_system.init_stage = INIT_STAGE_FILESYSTEM;
                }
                break;

            case INIT_STAGE_FILESYSTEM:
                ReportProgress("Initializing file system...", 60);
                err = InitializeFileSystem();
                if (err == SYS_OK) {
                    g_system.init_stage = INIT_STAGE_PROCESS;
                }
                break;

            case INIT_STAGE_PROCESS:
                ReportProgress("Preparing Process Manager...", 80);
                err = InitializeProcessManager();
                if (err == SYS_OK) {
                    g_system.init_stage = INIT_STAGE_COMPLETE;
                    ReportProgress("System initialization complete", 100);
                }
                break;

            case INIT_STAGE_COMPLETE:
                /* Already complete */
                break;
        }

        /* Report stage completion (callback support removed) */
    }

    if (err != SYS_OK) {
        ReportError(err, "System initialization failed");
        g_system.last_error = err;
    }

    return err;
}

/* ============================================================================
 * Hardware Detection
 * ============================================================================ */

static SystemError DetectHardwareCapabilities(void) {
    SystemCapabilities* caps = &g_system.capabilities;

    /* Reset capabilities */
    memset(caps, 0, sizeof(SystemCapabilities));

    /* Detect CPU architecture */
#ifdef __aarch64__
    caps->cpu_type = 0xAA64;  /* ARM64 */
#elif defined(__x86_64__)
    caps->cpu_type = 0x8664;   /* x86-64 */
#elif defined(__i386__)
    caps->cpu_type = 0x0386;   /* x86 */
#elif defined(__powerpc64__)
    caps->cpu_type = 0x5064;   /* PowerPC 64-bit */
#elif defined(__powerpc__)
    caps->cpu_type = 0x5032;   /* PowerPC 32-bit */
#else
    caps->cpu_type = 0x0000;   /* Unknown */
#endif

    /* Detect system RAM */
#ifdef PLATFORM_REMOVED_WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        caps->ram_size = (UInt32)(statex.ullTotalPhys / (1024 * 1024)) * (1024 * 1024);
    }
#elif defined(__linux__)
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        caps->ram_size = (UInt32)(pages * page_size);
    }
#endif

    /* Default to reasonable minimums if detection fails */
    if (caps->ram_size == 0) {
        caps->ram_size = 64 * 1024 * 1024;  /* 64MB default */
    }

    /* Set capabilities based on System 7.1 features */
    caps->has_color_quickdraw = true;
    caps->has_fpu = true;              /* Assume FPU available */
    caps->has_mmu = true;              /* Modern systems have MMU */
    caps->has_adb = false;             /* No real ADB, will emulate */
    caps->has_scsi = false;            /* No SCSI, use modern storage */
    caps->has_ethernet = true;         /* Network available */
    caps->has_sound_manager = true;    /* Can emulate sound */
    caps->has_power_manager = true;    /* Power management available */
    caps->rom_version = 0x077C;        /* Quadra 700 ROM version */

    return SYS_OK;
}

/* ============================================================================
 * Memory Management Initialization
 * ============================================================================ */

static SystemError InitializeMemory(void) {
    /* Allocate system heap */
    g_system.system_heap_size = 2 * 1024 * 1024;  /* 2MB system heap */
    g_system.system_heap = NewPtrClear(g_system.system_heap_size);
    if (!g_system.system_heap) {
        return SYS_ERR_NO_MEMORY;
    }

    /* Allocate application heap */
    g_system.application_heap_size = 8 * 1024 * 1024;  /* 8MB app heap */
    g_system.application_heap = NewPtrClear(g_system.application_heap_size);
    if (!g_system.application_heap) {
        DisposePtr((Ptr)g_system.system_heap);
        g_system.system_heap = NULL;
        return SYS_ERR_NO_MEMORY;
    }

    /* Initialize thread safety if needed */
#ifdef USE_THREADING
    pthread_mutex_init(&g_system.globals_mutex, NULL);
#endif

    return SYS_OK;
}

/* ============================================================================
 * ExpandMem Initialization
 * ============================================================================ */

static SystemError InitializeExpandMem(void) {
    /* Initialize ExpandMem with standard size */
    g_system.expand_mem = ExpandMemInit(EM_STANDARD_SIZE);
    if (!g_system.expand_mem) {
        return SYS_ERR_NO_MEMORY;
    }

    /* ExpandMem version is set during ExpandMemInit */

    /* Initialize keyboard support */
    if (!ExpandMemInitKeyboard(g_system.expand_mem, 0)) {
        ReportError(SYS_ERR_RESOURCE_NOT_FOUND,
                   "Failed to load keyboard resources (KCHR)");
        /* Continue anyway - keyboard may work with defaults */
    }

    /* Check AppleTalk configuration */
    /* For portable version, AppleTalk is always inactive */
    ExpandMemSetAppleTalkInactive(g_system.expand_mem, true);

    return SYS_OK;
}

/* ============================================================================
 * Resource Manager Initialization
 * ============================================================================ */

static SystemError InitializeResources(void) {
    /* For now, just mark resource manager as initialized */
    /* In a full implementation, this would:
     * - Initialize resource map structures
     * - Open system resource file
     * - Set up resource chain
     */
    g_system.resource_mgr = (void*)1;  /* Non-NULL to indicate initialized */

    /* Install decompressor hook */
    SystemError err = InstallDecompressor();
    if (err != SYS_OK) {
        ReportError(err, "Failed to install resource decompressor");
        /* Continue anyway - non-compressed resources will still work */
    }

    /* Load system resources */
    err = LoadSystemResources();
    if (err != SYS_OK) {
        ReportError(err, "Failed to load system resources");
        /* Continue anyway - system may work with minimal resources */
    }

    return SYS_OK;
}

/* ============================================================================
 * Decompressor Installation
 * ============================================================================ */

static SystemError InstallDecompressor(void) {

    /* Enable automatic decompression in Resource Manager */
    SetAutoDecompression(true);

    /* Set decompression cache size */
    ResourceManager_SetDecompressionCacheSize(32);  /* 32 cached decompressed resources */

    /* Enable decompression debugging if in verbose mode */
    /* if (g_system.boot_config.verbose_boot) {
        SetDecompressDebug(true);
    } */

    /* Register built-in decompressors */
    /* DonnBits (dcmp 0) and GreggyBits (dcmp 2) are built into ResourceDecompression.c */
    /* Custom decompressors can be registered later via ResourceManager_RegisterDecompressor */

    /* Create decompression hook function */
    DecompressHookProc hookProc = NULL;  /* Use default built-in decompression */

    /* Install the decompression hook */
    InstallDecompressHook(hookProc);

    /* Store decompressor info in ExpandMem for compatibility */
    if (g_system.expand_mem) {
        /* Store a marker indicating decompression is available */
        void* decomp_marker = (void*)0xDC6D7000;  /* 'dcmp' marker */
        ExpandMemInstallDecompressor(g_system.expand_mem, decomp_marker);

        /* Set resource loading flags to enable decompression */
        /* This is already done by ExpandMemInstallDecompressor */
    }

    ReportProgress("Resource decompression system initialized", 35);

    return SYS_OK;
}

/* ============================================================================
 * System Resource Loading
 * ============================================================================ */

static SystemError LoadSystemResources(void) {
    /* In a real implementation, this would load:
     * - System file resources
     * - ROM resources
     * - Essential system resources (cursors, icons, patterns, etc.)
     * - Keyboard layouts (KCHR)
     * - International resources (itlc, itl0, itl1, etc.)
     */

    /* For now, just return success */
    return SYS_OK;
}

/* ============================================================================
 * Input System Initialization (ADB Emulation)
 * ============================================================================ */

static SystemError InitializeInputSystem(void) {
    extern OSErr SoundManagerInit(void);

    /* Initialize keyboard emulation */
    /* In the portable version, we map modern keyboard/mouse events
     * to classic Mac OS ADB events */

    /* Keyboard type is detected by PS/2 driver at runtime */

    /* Initialize mouse tracking */
    /* Will be handled by platform-specific event loop */

    /* Initialize Sound Manager */
    OSErr err = SoundManagerInit();
    if (err != noErr) {
        serial_puts("SystemInit: WARNING - Sound Manager initialization failed\n");
        /* Non-fatal - continue without sound */
    }

    return SYS_OK;
}

/* ============================================================================
 * File System Initialization
 * ============================================================================ */

static SystemError InitializeFileSystem(void) {
    extern OSErr hal_storage_init(void);

    /* Initialize storage subsystem */
    serial_puts("SystemInit: Initializing storage subsystem\n");
    OSErr ata_err = hal_storage_init();
    if (ata_err != noErr) {
        serial_puts("SystemInit: WARNING - Storage initialization failed\n");
        /* Non-fatal - continue without disk access */
    }

    /* For now, just mark file system as initialized */
    /* In a full implementation, this would:
     * - Initialize VCB, FCB, and WDCB structures
     * - Mount boot volume
     * - Set up file system globals
     */
    g_system.file_mgr = (void*)1;  /* Non-NULL to indicate initialized */

    /* Mount boot volume */
    /* In portable version, map host filesystem to HFS */

    /* Set default and boot volumes in ExpandMem */
    if (g_system.expand_mem) {
        /* g_system.expand_mem->emBootVolume = 1; */     /* Boot volume refnum */
        /* g_system.expand_mem->emDefaultVolume = 1; */  /* Default volume */
    }

    return SYS_OK;
}

/* ============================================================================
 * Process Manager Initialization
 * ============================================================================ */

static SystemError InitializeProcessManager(void) {
    /* Process Manager provides cooperative multitasking */
    /* For the portable version, we'll emulate this with a simple
     * process/context switching system */

    /* Set up Process Manager globals in ExpandMem */
    if (g_system.expand_mem) {
        /* g_system.expand_mem->emProcessCount = 1; */  /* System process only */
        /* Process list and current process would be initialized here */
    }

    return SYS_OK;
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

SystemInitStage SystemGetInitStage(void) {
    return g_system.init_stage;
}

SystemError SystemGetCapabilities(SystemCapabilities* caps) {
    if (!caps) {
        return SYS_ERR_INVALID_CONFIGURATION;
    }

    memcpy(caps, &g_system.capabilities, sizeof(SystemCapabilities));
    return SYS_OK;
}

SystemGlobals* SystemGetGlobals(void) {
    /* Return the base part of the extended structure */
    static SystemGlobals globals;
    globals.capabilities = g_system.capabilities;
    globals.bootConfig = g_system.boot_config;
    globals.expandMem = g_system.expand_mem;
    globals.systemHeap = g_system.system_heap;
    globals.applZone = g_system.application_heap;
    return &globals;
}

ExpandMemRec* SystemGetExpandMem(void) {
    return g_system.expand_mem;
}

/* ============================================================================
 * System Shutdown
 * ============================================================================ */

SystemError SystemShutdown(Boolean restart) {
    ReportProgress("Shutting down system...", 0);

    /* Shutdown in reverse order of initialization */

    /* Shutdown Process Manager */
    /* (cleanup process structures) */

    /* Shutdown File System */
    if (g_system.file_mgr) {
        /* Would flush all volumes and cleanup */
        g_system.file_mgr = NULL;
    }

    /* Shutdown Input System */
    /* (cleanup input handlers) */

    /* Shutdown Resource Manager */
    if (g_system.resource_mgr) {
        /* Would close resource files and cleanup */
        g_system.resource_mgr = NULL;
    }

    /* Cleanup ExpandMem */
    if (g_system.expand_mem) {
        ExpandMemCleanup(g_system.expand_mem);
        DisposePtr((Ptr)g_system.expand_mem);
        g_system.expand_mem = NULL;
    }

    /* Free memory heaps */
    if (g_system.application_heap) {
        DisposePtr((Ptr)g_system.application_heap);
        g_system.application_heap = NULL;
    }

    if (g_system.system_heap) {
        DisposePtr((Ptr)g_system.system_heap);
        g_system.system_heap = NULL;
    }

    /* Thread cleanup */
#ifdef USE_THREADING
    pthread_mutex_destroy(&g_system.globals_mutex);
#endif

    g_system.init_stage = INIT_STAGE_EARLY;

    if (restart) {
        ReportProgress("Ready for restart", 100);
    } else {
        ReportProgress("System shutdown complete", 100);
    }

    return SYS_OK;
}

void SystemPanic(SystemError error_code, const char* message) {
    SYSTEM_LOG_ERROR( "\n*** SYSTEM PANIC ***\n");
    SYSTEM_LOG_ERROR( "Error Code: %d\n", error_code);
    if (message) {
        SYSTEM_LOG_ERROR( "Message: %s\n", message);
    }
    SYSTEM_LOG_ERROR( "System Stage: %d\n", g_system.init_stage);

    /* Minimal cleanup */
    if (g_system.callbacks.errorCallback) {
        g_system.callbacks.errorCallback(error_code, message);
    }

    /* Force halt - no exit in kernel mode */
    for (;;) {
        platform_halt();
    }
}

/* ============================================================================
 * Debugging and Diagnostics
 * ============================================================================ */

void SystemDumpState(void (*output_func)(const char* text)) {
    char buffer[256];

    if (!output_func) {
        return;
    }

    output_func("=== System 7.1 Portable State Dump ===\n");

    snprintf(buffer, sizeof(buffer), "System Version: 0x%04X\n",
             g_system.system_version);
    output_func(buffer);

    snprintf(buffer, sizeof(buffer), "Init Stage: %d\n",
             g_system.init_stage);
    output_func(buffer);

    snprintf(buffer, sizeof(buffer), "CPU Type: 0x%08X\n",
             g_system.capabilities.cpu_type);
    output_func(buffer);

    snprintf(buffer, sizeof(buffer), "RAM Size: %u MB\n",
             g_system.capabilities.ram_size / (1024 * 1024));
    output_func(buffer);

    snprintf(buffer, sizeof(buffer), "System Heap: %p (%zu bytes)\n",
             g_system.system_heap, g_system.system_heap_size);
    output_func(buffer);

    snprintf(buffer, sizeof(buffer), "ExpandMem: %p\n",
             (void*)g_system.expand_mem);
    output_func(buffer);

    if (g_system.expand_mem) {
        ExpandMemDump(g_system.expand_mem, output_func);
    }

    output_func("=== End State Dump ===\n");
}

SystemError SystemValidate(void) {
    /* Validate system heap */
    if (!g_system.system_heap) {
        return SYS_ERR_INITIALIZATION_FAILED;
    }

    /* Validate ExpandMem if initialized */
    if (g_system.init_stage >= INIT_STAGE_EXPANDMEM) {
        if (!g_system.expand_mem ||
            !ExpandMemValidate(g_system.expand_mem)) {
            return SYS_ERR_INITIALIZATION_FAILED;
        }
    }

    /* Validate Resource Manager if initialized */
    if (g_system.init_stage >= INIT_STAGE_RESOURCES) {
        if (!g_system.resource_mgr) {
            return SYS_ERR_INITIALIZATION_FAILED;
        }
    }

    /* Validate File Manager if initialized */
    if (g_system.init_stage >= INIT_STAGE_FILESYSTEM) {
        if (!g_system.file_mgr) {
            return SYS_ERR_INITIALIZATION_FAILED;
        }
    }

    return SYS_OK;
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static void ReportProgress(const char* message, int percent) {
    if (g_system.callbacks.progressCallback) {
        g_system.callbacks.progressCallback(message, percent);
    }

    /* if (g_system.boot_config.verbose_boot) {
        printf("[BOOT] %s (%d%%)\n", message, percent);
    } */
}

static void ReportError(SystemError error, const char* message) {
    g_system.last_error = error;

    if (message) {
        strncpy(g_system.error_message, message,
                sizeof(g_system.error_message) - 1);
    }

    if (g_system.callbacks.errorCallback) {
        g_system.callbacks.errorCallback(error, message);
    }

    SYSTEM_LOG_ERROR( "[ERROR] Code %d: %s\n", error, message ? message : "");
}
