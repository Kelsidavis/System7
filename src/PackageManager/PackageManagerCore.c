/*
 * PackageManagerCore.c - Package Manager Dispatcher
 *
 * Implements the Package Manager, which provides centralized access to
 * System 7 Toolbox packages (e.g., List Manager, Standard File, SANE).
 * Packages are ROM/RAM code modules that extend the Toolbox.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations */
OSErr InitPacks(void);
OSErr InitAllPacks(void);
OSErr CallPackage(short packID, short selector, void* params);

/* Package dispatcher declarations */
extern OSErr Pack0_Dispatch(short selector, void* params);  /* List Manager */
extern OSErr Pack3_Dispatch(short selector, void* params);  /* Standard File */
extern OSErr Pack4_Dispatch(short selector, void* params);  /* SANE (Floating Point Math) */
extern OSErr Pack6_Dispatch(short selector, void* params);  /* International Utilities */
extern OSErr Pack7_Dispatch(short selector, void* params);  /* Binary/Decimal Conversion */
extern OSErr Pack8_Dispatch(short selector, void* params);  /* Apple Events (stub) */
extern OSErr Pack10_Dispatch(short selector, void* params); /* Edition Manager (stub) */
extern OSErr Pack12_Dispatch(short selector, void* params); /* Dictionary Manager (stub) */
extern OSErr Pack13_Dispatch(short selector, void* params); /* PPC Toolbox (stub) */
extern OSErr Pack14_Dispatch(short selector, void* params); /* Help Manager (stub) */
extern OSErr Pack15_Dispatch(short selector, void* params); /* Picture Utilities (stub) */

/* Debug logging */
#define PKG_MGR_DEBUG 0

#if PKG_MGR_DEBUG
#define PKG_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[PkgMgr] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define PKG_LOG(...)
#endif

/* Error code for unimplemented features */
#ifndef unimpErr
#define unimpErr -4
#endif

/* Package initialization flags */
static Boolean g_packagesInitialized = false;

/*
 * InitPacks - Initialize the Package Manager
 *
 * Initializes the Package Manager so that packages can be called.
 * In classic Mac OS, this would load package resources from ROM/disk.
 * In our implementation, packages are linked directly, so this mainly
 * sets up internal state.
 *
 * Returns:
 *   noErr if successful
 *
 * Note: This must be called before using any package functions.
 * Applications typically call this during initialization.
 *
 * Example:
 *   OSErr err = InitPacks();
 *   if (err != noErr) {
 *       // Handle initialization failure
 *   }
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */
OSErr InitPacks(void) {
    if (g_packagesInitialized) {
        PKG_LOG("InitPacks: Already initialized\n");
        return noErr;
    }

    PKG_LOG("InitPacks: Initializing Package Manager\n");

    /* In classic Mac OS, this would:
     * 1. Allocate package loading tables
     * 2. Initialize package cache
     * 3. Load default packages from ROM
     *
     * In our implementation, packages are statically linked,
     * so we just mark as initialized.
     */

    g_packagesInitialized = true;

    PKG_LOG("InitPacks: Package Manager initialized\n");
    return noErr;
}

/*
 * InitAllPacks - Initialize all standard packages
 *
 * Initializes all standard System 7 packages. This is a convenience
 * function that initializes:
 * - Pack0: List Manager
 * - Pack3: Standard File
 * - Pack4: SANE (Floating Point)
 * - Pack6: International Utilities
 * - Pack7: Binary/Decimal Conversion
 * - Pack8: Apple Events
 * - Pack10: Edition Manager
 * - Pack12: Dictionary Manager
 * - Pack13: PPC Toolbox
 * - Pack14: Help Manager
 * - Pack15: Picture Utilities
 *
 * Returns:
 *   noErr if successful, error code otherwise
 *
 * Note: Applications can call InitAllPacks() for convenience instead
 * of calling InitPacks() and initializing individual packages.
 *
 * Example:
 *   OSErr err = InitAllPacks();
 *   if (err != noErr) {
 *       ExitToShell();
 *   }
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */
OSErr InitAllPacks(void) {
    OSErr err;

    PKG_LOG("InitAllPacks: Initializing all packages\n");

    /* First initialize the Package Manager itself */
    err = InitPacks();
    if (err != noErr) {
        PKG_LOG("InitAllPacks: InitPacks failed with error %d\n", err);
        return err;
    }

    /* In classic Mac OS, this would call initialization routines for
     * each standard package. In our implementation, most packages are
     * initialized on-demand when first called.
     *
     * Individual package initialization could go here if needed.
     */

    PKG_LOG("InitAllPacks: All packages initialized\n");
    return noErr;
}

/*
 * CallPackage - Call a package trap with parameters
 *
 * Dispatches a call to a specific package. This is the central dispatcher
 * that routes calls to the appropriate package code.
 *
 * Parameters:
 *   packID - Package ID (0-15)
 *   selector - Function selector within package
 *   params - Pointer to parameter block (package-specific format)
 *
 * Returns:
 *   noErr if successful, or package-specific error code
 *
 * Package IDs:
 *   0  = List Manager
 *   3  = Standard File
 *   4  = SANE (Floating Point Math)
 *   6  = International Utilities
 *   7  = Binary/Decimal Conversion
 *   8  = Apple Events
 *   10 = Edition Manager
 *   12 = Dictionary Manager
 *   13 = PPC Toolbox
 *   14 = Help Manager
 *   15 = Picture Utilities
 *
 * Example:
 *   // Call Standard File package (Pack3), selector 1 (SFGetFile)
 *   StandardFileReply reply;
 *   OSErr err = CallPackage(3, 1, &reply);
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */
OSErr CallPackage(short packID, short selector, void* params) {
    if (!g_packagesInitialized) {
        PKG_LOG("CallPackage: Package Manager not initialized\n");
        return paramErr;
    }

    PKG_LOG("CallPackage: packID=%d, selector=%d, params=%p\n",
            packID, selector, params);

    /* Dispatch to appropriate package based on packID */
    switch (packID) {
        case 0:
            /* Pack0: List Manager */
            PKG_LOG("CallPackage: Pack0 (List Manager) dispatching\n");
            return Pack0_Dispatch(selector, params);

        case 3:
            /* Pack3: Standard File */
            PKG_LOG("CallPackage: Pack3 (Standard File) dispatching\n");
            return Pack3_Dispatch(selector, params);

        case 4:
            /* Pack4: SANE (Floating Point Math) */
            PKG_LOG("CallPackage: Pack4 (SANE) dispatching\n");
            return Pack4_Dispatch(selector, params);

        case 6:
            /* Pack6: International Utilities */
            PKG_LOG("CallPackage: Pack6 (International Utilities) dispatching\n");
            return Pack6_Dispatch(selector, params);

        case 7:
            /* Pack7: Binary/Decimal Conversion */
            PKG_LOG("CallPackage: Pack7 (Binary/Decimal) dispatching\n");
            return Pack7_Dispatch(selector, params);

        case 8:
            /* Pack8: Apple Events */
            PKG_LOG("CallPackage: Pack8 (Apple Events) dispatching to stub\n");
            return Pack8_Dispatch(selector, params);

        case 10:
            /* Pack10: Edition Manager */
            PKG_LOG("CallPackage: Pack10 (Edition Manager) dispatching to stub\n");
            return Pack10_Dispatch(selector, params);

        case 12:
            /* Pack12: Dictionary Manager */
            PKG_LOG("CallPackage: Pack12 (Dictionary Manager) dispatching to stub\n");
            return Pack12_Dispatch(selector, params);

        case 13:
            /* Pack13: PPC Toolbox */
            PKG_LOG("CallPackage: Pack13 (PPC Toolbox) dispatching to stub\n");
            return Pack13_Dispatch(selector, params);

        case 14:
            /* Pack14: Help Manager */
            PKG_LOG("CallPackage: Pack14 (Help Manager) dispatching to stub\n");
            return Pack14_Dispatch(selector, params);

        case 15:
            /* Pack15: Picture Utilities */
            PKG_LOG("CallPackage: Pack15 (Picture Utilities) dispatching to stub\n");
            return Pack15_Dispatch(selector, params);

        default:
            PKG_LOG("CallPackage: Invalid package ID %d\n", packID);
            return paramErr;
    }
}
