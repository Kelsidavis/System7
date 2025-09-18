/*
 * PackageManagerCore.c
 * System 7.1 Portable Package Manager Core Implementation
 *
 * This is the heart of the Package Manager system, providing PACK resource loading,
 * dispatch, and management for all Mac OS packages. Critical for application stability.
 *
 * Implements the core Mac OS Package Manager traps:
 * - InitPack (0xA9E5)
 * - InitAllPacks (0xA9E6)
 * - Package dispatch and routing
 */

#include "PackageManager/PackageManager.h"
#include "PackageManager/PackageTypes.h"
#include "PackageManager/SANEPackage.h"
#include "PackageManager/StringPackage.h"
#include "PackageManager/StandardFilePackage.h"
#include "PackageManager/ListManagerPackage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Global package manager state */
static PackageManagerState g_packageState = {0};
static PackageDescriptor g_packages[MAX_PACKAGES] = {0};
static PackageManagerConfig g_config = {0};
static ThreadSafetyInfo g_threadSafety = {0};
static pthread_mutex_t g_packageMutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations for package implementations */
extern int32_t InitListManagerPackage(void);
extern int32_t InitSANEPackage(void);
extern int32_t InitStandardFilePackage(void);
extern int32_t InitStringPackage(void);
extern int32_t InitInternationalPackage(void);
extern int32_t InitSoundPackage(void);

extern int32_t ListManagerDispatch(int16_t selector, void *params);
extern int32_t SANEDispatch(int16_t selector, void *params);
extern int32_t StandardFileDispatch(int16_t selector, void *params);
extern int32_t StringDispatch(int16_t selector, void *params);
extern int32_t InternationalDispatch(int16_t selector, void *params);
extern int32_t SoundDispatch(int16_t selector, void *params);

/* Utility functions */
static void LogDebug(const char *format, ...);
static void LogError(const char *format, ...);
static void InitializePackageDescriptor(int16_t packID, const char *name,
                                       PackageInit initFunc, PackageFunction dispatchFunc);

/**
 * Initialize the Package Manager system
 */
int32_t InitPackageManager(void)
{
    if (g_packageState.initialized) {
        return PACKAGE_NO_ERROR;
    }

    LogDebug("Initializing Package Manager...");

    /* Clear state */
    memset(&g_packageState, 0, sizeof(g_packageState));
    memset(g_packages, 0, sizeof(g_packages));

    /* Set default configuration */
    g_config.debugEnabled = true;
    g_config.threadSafe = true;
    g_config.strictCompatibility = true;
    g_config.maxMemoryMB = 64;

    /* Initialize thread safety */
    if (g_config.threadSafe) {
        g_threadSafety.enabled = true;
        g_threadSafety.mutex = &g_packageMutex;
        pthread_mutex_init(&g_packageMutex, NULL);
    }

    /* Initialize package descriptors */
    InitializePackageDescriptor(listMgr, "List Manager",
                               (PackageInit)InitListManagerPackage, ListManagerDispatch);
    InitializePackageDescriptor(stdFile, "Standard File",
                               (PackageInit)InitStandardFilePackage, StandardFileDispatch);
    InitializePackageDescriptor(flPoint, "SANE Floating Point",
                               (PackageInit)InitSANEPackage, SANEDispatch);
    InitializePackageDescriptor(trFunc, "Transcendental Functions",
                               (PackageInit)InitSANEPackage, SANEDispatch);
    InitializePackageDescriptor(intUtil, "International Utilities",
                               (PackageInit)InitStringPackage, StringDispatch);
    InitializePackageDescriptor(bdConv, "Binary/Decimal Conversion",
                               (PackageInit)InitSANEPackage, SANEDispatch);

    g_packageState.initialized = true;
    LogDebug("Package Manager initialized successfully");

    return PACKAGE_NO_ERROR;
}

/**
 * Initialize package descriptor
 */
static void InitializePackageDescriptor(int16_t packID, const char *name,
                                       PackageInit initFunc, PackageFunction dispatchFunc)
{
    if (packID >= 0 && packID < MAX_PACKAGES) {
        PackageDescriptor *pkg = &g_packages[packID];
        pkg->packID = packID;
        pkg->version = 0x0701; /* System 7.1 version */
        strncpy((char*)&pkg->name, name, MAX_PACKAGE_NAME - 1);
        pkg->initFunc = initFunc;
        pkg->dispatchFunc = dispatchFunc;
        pkg->cleanupFunc = NULL;
        pkg->privateData = NULL;
        pkg->loaded = false;
        pkg->initialized = false;
    }
}

/**
 * Initialize a specific package by ID
 */
void InitPack(int16_t packID)
{
    if (!g_packageState.initialized) {
        InitPackageManager();
    }

    if (g_threadSafety.enabled) {
        LockPackageManager();
    }

    LogDebug("InitPack: Initializing package %d", packID);

    if (packID < 0 || packID >= MAX_PACKAGES) {
        LogError("InitPack: Invalid package ID %d", packID);
        goto cleanup;
    }

    PackageDescriptor *pkg = &g_packages[packID];
    if (!pkg->initFunc) {
        LogError("InitPack: No initialization function for package %d", packID);
        goto cleanup;
    }

    if (pkg->initialized) {
        LogDebug("InitPack: Package %d already initialized", packID);
        goto cleanup;
    }

    /* Call package-specific initialization */
    if (pkg->initFunc) {
        int32_t result = ((int32_t(*)(void))pkg->initFunc)();
        if (result != PACKAGE_NO_ERROR) {
            LogError("InitPack: Failed to initialize package %d: %d", packID, result);
            goto cleanup;
        }
    }

    pkg->initialized = true;
    pkg->loaded = true;
    g_packageState.loadedPackages |= (1 << packID);
    g_packageState.packageVersions[packID] = pkg->version;

    LogDebug("InitPack: Package %d (%s) initialized successfully", packID, pkg->name);

cleanup:
    if (g_threadSafety.enabled) {
        UnlockPackageManager();
    }
}

/**
 * Initialize all supported packages
 */
void InitAllPacks(void)
{
    LogDebug("InitAllPacks: Initializing all packages");

    /* Initialize in dependency order */
    InitPack(flPoint);      /* SANE first - needed by others */
    InitPack(trFunc);       /* Transcendental functions */
    InitPack(bdConv);       /* Binary/decimal conversion */
    InitPack(intUtil);      /* International utilities */
    InitPack(listMgr);      /* List manager */
    InitPack(stdFile);      /* Standard file dialogs */
    InitPack(dskInit);      /* Disk initialization */
    InitPack(editionMgr);   /* Edition manager */

    LogDebug("InitAllPacks: All packages initialized");
}

/**
 * Check if a package is loaded and available
 */
bool IsPackageLoaded(int16_t packID)
{
    if (packID < 0 || packID >= MAX_PACKAGES) {
        return false;
    }

    return (g_packageState.loadedPackages & (1 << packID)) != 0;
}

/**
 * Get package version information
 */
uint32_t GetPackageVersion(int16_t packID)
{
    if (packID < 0 || packID >= MAX_PACKAGES) {
        return 0;
    }

    return g_packageState.packageVersions[packID];
}

/**
 * Generic package trap handler
 */
int32_t PackageTrap(uint16_t trapWord, void *params)
{
    /* Extract package ID from trap word */
    int16_t packID = -1;
    int16_t selector = 0;

    /* Decode trap word based on Mac OS package trap format */
    switch (trapWord & 0xF000) {
        case 0xA000: /* Package trap format */
            switch (trapWord) {
                case 0xA9E5: /* InitPack */
                    if (params) {
                        packID = *(int16_t*)params;
                        InitPack(packID);
                    }
                    return PACKAGE_NO_ERROR;

                case 0xA9E6: /* InitAllPacks */
                    InitAllPacks();
                    return PACKAGE_NO_ERROR;

                case 0xA9E7: /* List Manager Package */
                    packID = listMgr;
                    selector = *(int16_t*)params;
                    break;

                case 0xA9EA: /* Standard File Package */
                    packID = stdFile;
                    selector = *(int16_t*)params;
                    break;

                case 0xA9EB: /* Floating Point Package */
                case 0xA9EC: /* Transcendental Functions Package */
                    packID = (trapWord == 0xA9EB) ? flPoint : trFunc;
                    selector = *(int16_t*)params;
                    break;

                case 0xA9ED: /* International Utilities Package */
                    packID = intUtil;
                    selector = *(int16_t*)params;
                    break;

                case 0xA9EE: /* Binary/Decimal Conversion Package */
                    packID = bdConv;
                    selector = *(int16_t*)params;
                    break;

                default:
                    LogError("PackageTrap: Unknown trap word 0x%04X", trapWord);
                    return PACKAGE_INVALID_SELECTOR;
            }
            break;

        default:
            LogError("PackageTrap: Invalid trap word format 0x%04X", trapWord);
            return PACKAGE_INVALID_SELECTOR;
    }

    return CallPackage(packID, selector, params);
}

/**
 * Call a specific package function
 */
int32_t CallPackage(int16_t packID, int16_t selector, void *params)
{
    if (g_threadSafety.enabled) {
        LockPackageManager();
    }

    int32_t result = PACKAGE_INVALID_ID;

    if (packID < 0 || packID >= MAX_PACKAGES) {
        LogError("CallPackage: Invalid package ID %d", packID);
        goto cleanup;
    }

    PackageDescriptor *pkg = &g_packages[packID];
    if (!pkg->loaded || !pkg->initialized) {
        LogError("CallPackage: Package %d not loaded or initialized", packID);
        result = PACKAGE_NOT_LOADED;
        goto cleanup;
    }

    if (!pkg->dispatchFunc) {
        LogError("CallPackage: No dispatch function for package %d", packID);
        result = PACKAGE_INVALID_SELECTOR;
        goto cleanup;
    }

    /* Call package dispatch function */
    result = pkg->dispatchFunc(selector, params);

cleanup:
    if (g_threadSafety.enabled) {
        UnlockPackageManager();
    }

    return result;
}

/**
 * Unload a specific package
 */
void UnloadPackage(int16_t packID)
{
    if (packID < 0 || packID >= MAX_PACKAGES) {
        return;
    }

    if (g_threadSafety.enabled) {
        LockPackageManager();
    }

    PackageDescriptor *pkg = &g_packages[packID];
    if (pkg->loaded) {
        if (pkg->cleanupFunc) {
            pkg->cleanupFunc();
        }

        pkg->loaded = false;
        pkg->initialized = false;
        g_packageState.loadedPackages &= ~(1 << packID);
        g_packageState.packageVersions[packID] = 0;

        LogDebug("UnloadPackage: Package %d unloaded", packID);
    }

    if (g_threadSafety.enabled) {
        UnlockPackageManager();
    }
}

/**
 * Shutdown the Package Manager and unload all packages
 */
void ShutdownPackageManager(void)
{
    LogDebug("Shutting down Package Manager...");

    /* Unload all packages */
    for (int i = 0; i < MAX_PACKAGES; i++) {
        if (g_packages[i].loaded) {
            UnloadPackage(i);
        }
    }

    /* Cleanup thread safety */
    if (g_threadSafety.enabled) {
        pthread_mutex_destroy(&g_packageMutex);
        g_threadSafety.enabled = false;
    }

    g_packageState.initialized = false;
    LogDebug("Package Manager shutdown complete");
}

/**
 * Get current package manager state
 */
void GetPackageManagerState(PackageManagerState *state)
{
    if (state) {
        *state = g_packageState;
    }
}

/**
 * Validate package integrity
 */
bool ValidatePackage(int16_t packID)
{
    if (packID < 0 || packID >= MAX_PACKAGES) {
        return false;
    }

    PackageDescriptor *pkg = &g_packages[packID];
    return (pkg->loaded && pkg->initialized && pkg->dispatchFunc != NULL);
}

/**
 * Thread safety functions
 */
void LockPackageManager(void)
{
    if (g_threadSafety.enabled && g_threadSafety.mutex) {
        pthread_mutex_lock((pthread_mutex_t*)g_threadSafety.mutex);
        g_threadSafety.lockCount++;
    }
}

void UnlockPackageManager(void)
{
    if (g_threadSafety.enabled && g_threadSafety.mutex) {
        if (g_threadSafety.lockCount > 0) {
            g_threadSafety.lockCount--;
        }
        pthread_mutex_unlock((pthread_mutex_t*)g_threadSafety.mutex);
    }
}

/**
 * Configuration functions
 */
void SetPackageDebug(bool enabled)
{
    g_config.debugEnabled = enabled;
}

void SetPackageThreadSafe(bool enabled)
{
    g_config.threadSafe = enabled;
    g_threadSafety.enabled = enabled;
}

/**
 * Logging functions
 */
static void LogDebug(const char *format, ...)
{
    if (!g_config.debugEnabled) {
        return;
    }

    va_list args;
    va_start(args, format);
    printf("[PackageManager DEBUG] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

static void LogError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[PackageManager ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/* Platform integration stubs */
void SetPlatformMathLibrary(void *mathLib)
{
    /* Platform-specific math library integration */
    LogDebug("SetPlatformMathLibrary called");
}

void SetPlatformFileDialogs(void *dialogHandler)
{
    /* Platform-specific file dialog integration */
    LogDebug("SetPlatformFileDialogs called");
}

void SetPlatformSoundSystem(void *soundSystem)
{
    /* Platform-specific sound system integration */
    LogDebug("SetPlatformSoundSystem called");
}