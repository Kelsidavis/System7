/*
 * Pack_Stubs.c - Stub Implementations for Unimplemented Packages
 *
 * Provides minimal stub dispatchers for packages that don't yet have full
 * implementations. These stubs allow the Package Manager to handle calls
 * gracefully by returning unimpErr rather than crashing.
 *
 * Packages included:
 * - Pack8:  Apple Events
 * - Pack10: Edition Manager
 * - Pack12: Dictionary Manager
 * - Pack13: PPC Toolbox
 * - Pack14: Help Manager
 * - Pack15: Picture Utilities
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations */
OSErr Pack8_Dispatch(short selector, void* params);
OSErr Pack10_Dispatch(short selector, void* params);
OSErr Pack12_Dispatch(short selector, void* params);
OSErr Pack13_Dispatch(short selector, void* params);
OSErr Pack14_Dispatch(short selector, void* params);
OSErr Pack15_Dispatch(short selector, void* params);

/* Debug logging */
#define PACK_STUBS_DEBUG 0

#if PACK_STUBS_DEBUG
extern void serial_puts(const char* str);
#define STUB_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[PackStub] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define STUB_LOG(...)
#endif

/* Error code for unimplemented features */
#ifndef unimpErr
#define unimpErr -4
#endif

/*
 * Pack8_Dispatch - Apple Events Package (Pack8) stub
 *
 * Apple Events provide inter-application communication in System 7.
 * This stub returns unimpErr for all selectors.
 *
 * Full implementation would handle:
 * - AECreateDesc, AECreateList, AECreateAppleEvent
 * - AESend, AEProcessAppleEvent
 * - AEGetParamDesc, AEPutParamDesc
 * - Event handlers and callbacks
 *
 * Based on Inside Macintosh: Interapplication Communication
 */
OSErr Pack8_Dispatch(short selector, void* params) {
    (void)selector;
    (void)params;  /* Unused */
    STUB_LOG("Pack8 (Apple Events) selector %d not implemented\n", selector);
    return unimpErr;
}

/*
 * Pack10_Dispatch - Edition Manager Package (Pack10) stub
 *
 * Edition Manager provides publish-and-subscribe functionality for
 * sharing data between documents and applications.
 *
 * This stub returns unimpErr for all selectors.
 *
 * Full implementation would handle:
 * - NewSection, RegisterSection, UnRegisterSection
 * - CreateEditionContainerFile, OpenEdition, CloseEdition
 * - ReadEdition, WriteEdition
 * - Subscriber and publisher management
 *
 * Based on Inside Macintosh: Interapplication Communication
 */
OSErr Pack10_Dispatch(short selector, void* params) {
    (void)selector;
    (void)params;  /* Unused */
    STUB_LOG("Pack10 (Edition Manager) selector %d not implemented\n", selector);
    return unimpErr;
}

/*
 * Pack12_Dispatch - Dictionary Manager Package (Pack12) stub
 *
 * Dictionary Manager provides spelling checking and dictionary services.
 * This stub returns unimpErr for all selectors.
 *
 * Full implementation would handle:
 * - OpenDictionary, CloseDictionary
 * - LookUpString, CheckSpelling
 * - GetSuggestions
 * - Dictionary enumeration
 *
 * Based on Inside Macintosh: Text
 */
OSErr Pack12_Dispatch(short selector, void* params) {
    (void)selector;
    (void)params;  /* Unused */
    STUB_LOG("Pack12 (Dictionary Manager) selector %d not implemented\n", selector);
    return unimpErr;
}

/*
 * Pack13_Dispatch - PPC Toolbox Package (Pack13) stub
 *
 * PPC (Program-to-Program Communications) Toolbox provides low-level
 * inter-process and inter-machine communication.
 *
 * This stub returns unimpErr for all selectors.
 *
 * Full implementation would handle:
 * - PPCOpen, PPCClose
 * - PPCStart, PPCAccept, PPCReject
 * - PPCRead, PPCWrite
 * - Port and session management
 *
 * Based on Inside Macintosh: Interapplication Communication
 */
OSErr Pack13_Dispatch(short selector, void* params) {
    (void)selector;
    (void)params;  /* Unused */
    STUB_LOG("Pack13 (PPC Toolbox) selector %d not implemented\n", selector);
    return unimpErr;
}

/*
 * Pack14_Dispatch - Help Manager Package (Pack14) stub
 *
 * Help Manager provides balloon help and help content display.
 * This stub returns unimpErr for all selectors.
 *
 * Full implementation would handle:
 * - HMGetHelpMenuHandle
 * - HMShowBalloon, HMRemoveBalloon
 * - HMGetBalloons, HMSetBalloons
 * - Help content resources
 *
 * Based on Inside Macintosh: More Macintosh Toolbox
 */
OSErr Pack14_Dispatch(short selector, void* params) {
    (void)selector;
    (void)params;  /* Unused */
    STUB_LOG("Pack14 (Help Manager) selector %d not implemented\n", selector);
    return unimpErr;
}

/*
 * Pack15_Dispatch - Picture Utilities Package (Pack15) stub
 *
 * Picture Utilities provide operations on QuickDraw pictures, including
 * image processing, color analysis, and picture compression.
 *
 * This stub returns unimpErr for all selectors.
 *
 * Full implementation would handle:
 * - GetPictInfo - Analyze picture contents
 * - GetPixMapInfo - Analyze pixel map
 * - NewPictInfo, DisposPictInfo
 * - Picture comment parsing
 *
 * Based on Inside Macintosh: Imaging With QuickDraw
 */
OSErr Pack15_Dispatch(short selector, void* params) {
    (void)selector;
    (void)params;  /* Unused */
    STUB_LOG("Pack15 (Picture Utilities) selector %d not implemented\n", selector);
    return unimpErr;
}
