/*
 * ScrapManager.h - Main Scrap Manager API
 * System 7.1 Portable - Scrap Manager Component
 *
 * Main header file for the Mac OS Scrap Manager, providing clipboard functionality
 * for inter-application data exchange with modern platform integration.
 */

#ifndef SCRAP_MANAGER_H
#define SCRAP_MANAGER_H

#include "SystemTypes.h"

/* Forward declarations */

#include "ScrapTypes.h"
#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Core Scrap Manager Functions
 * The classic Mac OS Scrap Manager API functions are declared in ScrapTypes.h
 * with their authentic System 7 signatures
 */

/*
 * Extended Scrap Manager Functions
 * These provide additional functionality for modern systems
 */


/* Check if a specific format is available */
/* Note: ScrapTypes.h declares ScrapHasFlavor for this purpose */

/* Get the size of data for a specific format */
/* Note: ScrapTypes.h declares ScrapGetFlavorSize for this purpose */

/* Put data with format conversion */
OSErr PutScrapWithConversion(SInt32 length, ResType theType,
                            const void *source, Boolean allowConversion);

/* Get data with format conversion */
OSErr GetScrapWithConversion(Handle destHandle, ResType theType,
                            SInt32 *offset, Boolean allowConversion);

/* Register a format converter */
OSErr RegisterScrapConverter(ResType sourceType, ResType destType,
                            ScrapConverterProc converter, void *refCon);


/*
 * Scrap File Management Functions
 */


/*
 * Memory Management Functions
 */


/* Get current memory usage */
OSErr GetScrapMemoryInfo(SInt32 *memoryUsed, SInt32 *diskUsed,
                        SInt32 *totalSize);


/*
 * Inter-Application Functions
 */


/*
 * Modern Clipboard Integration Functions
 */


/* Register platform format mapping */
OSErr RegisterPlatformFormat(ResType macType, UInt32 platformFormat,
                            const char *formatName);


/*
 * Utility Functions
 */


/* Get scrap statistics */
OSErr GetScrapStats(UInt32 *putCount, UInt32 *getCount,
                   UInt32 *conversionCount, UInt32 *errorCount);


/* Run self-test suite */
void Scrap_RunSelfTest(void);

/*
 * Legacy Compatibility Functions
 * These maintain compatibility with older System versions
 */


/* Copy from scrap to TextEdit (legacy) */
OSErr TEFromScrap(void);

/* Copy from TextEdit to scrap (legacy) */
OSErr TEToScrap(void);


/*
 * Constants for backward compatibility
 */
#define InfoScrapTrap    0xA9F9
#define UnloadScrapTrap  0xA9FA
#define LoadScrapTrap    0xA9FB
#define ZeroScrapTrap    0xA9FC
#define GetScrapTrap     0xA9FD
#define PutScrapTrap     0xA9FE

/* Legacy error codes */
#define noScrapErr       scrapNoScrap
#define noTypeErr        scrapNoTypeError

#ifdef __cplusplus
}
#endif

#endif /* SCRAP_MANAGER_H */
