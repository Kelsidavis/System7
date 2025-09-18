/*
 * PackageManager.h
 * System 7.1 Portable Package Manager Implementation
 *
 * Main header file for the Package Manager system that provides Mac OS PACK resources
 * for essential functionality like SANE floating point, Standard File dialogs,
 * List Manager, International Utilities, and String utilities.
 *
 * This is CRITICAL for application stability - without proper package support,
 * Mac applications may crash on floating point operations or file dialogs.
 */

#ifndef __PACKAGE_MANAGER_H__
#define __PACKAGE_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Package IDs from original Mac OS */
#define listMgr     0   /* List Manager Package */
#define dskInit     2   /* Disk Initialization Package */
#define stdFile     3   /* Standard File Package */
#define flPoint     4   /* Floating-Point Arithmetic Package (SANE) */
#define trFunc      5   /* Transcendental Functions Package */
#define intUtil     6   /* International Utilities Package */
#define bdConv      7   /* Binary/Decimal Conversion Package */
#define editionMgr  11  /* Edition Manager Package */

/* Package error codes */
#define noErr               0
#define paramErr           -50
#define memFullErr         -108
#define unimpErr           -4
#define packageNotFound    -192

/* Package manager status */
typedef struct {
    bool initialized;
    uint32_t loadedPackages;    /* Bit mask of loaded packages */
    void *packageHandles[16];   /* Package implementation handles */
    uint32_t packageVersions[16]; /* Package version numbers */
} PackageManagerState;

/* Core Package Manager Functions */

/**
 * Initialize the Package Manager system
 * Must be called before any package operations
 */
int32_t InitPackageManager(void);

/**
 * Initialize a specific package by ID
 * @param packID Package identifier (listMgr, stdFile, flPoint, etc.)
 */
void InitPack(int16_t packID);

/**
 * Initialize all supported packages
 * Equivalent to calling InitPack for each package
 */
void InitAllPacks(void);

/**
 * Check if a package is loaded and available
 * @param packID Package identifier
 * @return true if package is loaded, false otherwise
 */
bool IsPackageLoaded(int16_t packID);

/**
 * Get package version information
 * @param packID Package identifier
 * @return Package version number or 0 if not loaded
 */
uint32_t GetPackageVersion(int16_t packID);

/**
 * Unload a specific package
 * @param packID Package identifier
 */
void UnloadPackage(int16_t packID);

/**
 * Shutdown the Package Manager and unload all packages
 */
void ShutdownPackageManager(void);

/* Package dispatch mechanism */

/**
 * Generic package trap handler
 * Routes package calls to appropriate implementations
 * @param trapWord Trap instruction word containing package and selector
 * @param params Parameter block for the package call
 * @return Error code or function result
 */
int32_t PackageTrap(uint16_t trapWord, void *params);

/**
 * Call a specific package function
 * @param packID Package identifier
 * @param selector Function selector within package
 * @param params Parameter block
 * @return Function result
 */
int32_t CallPackage(int16_t packID, int16_t selector, void *params);

/* Package manager debugging and diagnostics */

/**
 * Get current package manager state
 * @param state Pointer to state structure to fill
 */
void GetPackageManagerState(PackageManagerState *state);

/**
 * Validate package integrity
 * @param packID Package to validate
 * @return true if package passes validation
 */
bool ValidatePackage(int16_t packID);

/**
 * Enable/disable package debugging
 * @param enabled true to enable debug output
 */
void SetPackageDebug(bool enabled);

/* Package resource loading */

/**
 * Load package from resource data
 * @param packID Package identifier
 * @param resourceData Package resource data
 * @param resourceSize Size of resource data
 * @return Error code
 */
int32_t LoadPackageFromResource(int16_t packID, const void *resourceData, size_t resourceSize);

/**
 * Load package from file
 * @param packID Package identifier
 * @param filename Path to package file
 * @return Error code
 */
int32_t LoadPackageFromFile(int16_t packID, const char *filename);

/* Math environment and precision control */

/**
 * Set floating point environment for SANE package
 * @param precision Desired precision (single, double, extended)
 * @param rounding Rounding mode
 */
void SetMathEnvironment(int precision, int rounding);

/**
 * Get current math environment settings
 * @param precision Pointer to receive precision setting
 * @param rounding Pointer to receive rounding mode
 */
void GetMathEnvironment(int *precision, int *rounding);

/* Thread safety */

/**
 * Enable thread-safe operation
 * Adds mutex protection to package calls
 * @param enabled true to enable thread safety
 */
void SetPackageThreadSafe(bool enabled);

/**
 * Lock package manager for exclusive access
 * Used internally for thread safety
 */
void LockPackageManager(void);

/**
 * Unlock package manager
 */
void UnlockPackageManager(void);

/* Platform integration */

/**
 * Set platform-specific math library
 * @param mathLib Pointer to platform math functions
 */
void SetPlatformMathLibrary(void *mathLib);

/**
 * Set platform-specific file dialog handler
 * @param dialogHandler Pointer to platform file dialog functions
 */
void SetPlatformFileDialogs(void *dialogHandler);

/**
 * Set platform-specific sound system
 * @param soundSystem Pointer to platform sound functions
 */
void SetPlatformSoundSystem(void *soundSystem);

#ifdef __cplusplus
}
#endif

#endif /* __PACKAGE_MANAGER_H__ */