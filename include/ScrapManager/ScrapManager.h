/*
 * ScrapManager.h - Main Scrap Manager API
 * System 7.1 Portable - Scrap Manager Component
 *
 * Main header file for the Mac OS Scrap Manager, providing clipboard functionality
 * for inter-application data exchange with modern platform integration.
 */

#ifndef SCRAP_MANAGER_H
#define SCRAP_MANAGER_H

#include "ScrapTypes.h"
#include "MacTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Core Scrap Manager Functions
 * These functions provide the standard Mac OS Scrap Manager API
 */

/* Get information about the current scrap */
PScrapStuff InfoScrap(void);

/* Clear the scrap and increment change counter */
OSErr ZeroScrap(void);

/* Put data into the scrap */
OSErr PutScrap(int32_t length, ResType theType, const void *source);

/* Get data from the scrap */
OSErr GetScrap(Handle destHandle, ResType theType, int32_t *offset);

/* Load scrap from disk file */
OSErr LoadScrap(void);

/* Unload scrap to disk file */
OSErr UnloadScrap(void);

/*
 * Extended Scrap Manager Functions
 * These provide additional functionality for modern systems
 */

/* Initialize the Scrap Manager */
OSErr InitScrapManager(void);

/* Cleanup and shutdown the Scrap Manager */
void CleanupScrapManager(void);

/* Get available scrap formats */
OSErr GetScrapFormats(ResType *types, int16_t *count, int16_t maxTypes);

/* Check if a specific format is available */
Boolean HasScrapFormat(ResType theType);

/* Get the size of data for a specific format */
int32_t GetScrapSize(ResType theType);

/* Put data with format conversion */
OSErr PutScrapWithConversion(int32_t length, ResType theType,
                            const void *source, Boolean allowConversion);

/* Get data with format conversion */
OSErr GetScrapWithConversion(Handle destHandle, ResType theType,
                            int32_t *offset, Boolean allowConversion);

/* Register a format converter */
OSErr RegisterScrapConverter(ResType sourceType, ResType destType,
                            ScrapConverterProc converter, void *refCon);

/* Unregister a format converter */
OSErr UnregisterScrapConverter(ResType sourceType, ResType destType);

/*
 * Scrap File Management Functions
 */

/* Set the scrap file location */
OSErr SetScrapFile(ConstStr255Param fileName, int16_t vRefNum, int32_t dirID);

/* Get the current scrap file location */
OSErr GetScrapFile(Str255 fileName, int16_t *vRefNum, int32_t *dirID);

/* Force save scrap to file */
OSErr SaveScrapToFile(void);

/* Load scrap from a specific file */
OSErr LoadScrapFromFile(ConstStr255Param fileName, int16_t vRefNum, int32_t dirID);

/*
 * Memory Management Functions
 */

/* Set memory allocation preferences */
OSErr SetScrapMemoryPrefs(int32_t memoryThreshold, int32_t diskThreshold);

/* Get current memory usage */
OSErr GetScrapMemoryInfo(int32_t *memoryUsed, int32_t *diskUsed,
                        int32_t *totalSize);

/* Compact scrap memory */
OSErr CompactScrapMemory(void);

/* Purge least recently used scrap data */
OSErr PurgeScrapData(int32_t bytesToPurge);

/*
 * Inter-Application Functions
 */

/* Register for scrap change notifications */
OSErr RegisterScrapChangeCallback(ScrapChangeCallback callback, void *userData);

/* Unregister scrap change notifications */
OSErr UnregisterScrapChangeCallback(ScrapChangeCallback callback);

/* Get scrap ownership information */
OSErr GetScrapOwner(ProcessSerialNumber *psn, Str255 processName);

/* Set scrap ownership */
OSErr SetScrapOwner(const ProcessSerialNumber *psn);

/* Send scrap change notification */
void NotifyScrapChange(void);

/*
 * Modern Clipboard Integration Functions
 */

/* Initialize modern clipboard integration */
OSErr InitModernClipboard(void);

/* Shutdown modern clipboard integration */
void CleanupModernClipboard(void);

/* Sync with native platform clipboard */
OSErr SyncWithNativeClipboard(Boolean toNative);

/* Register platform format mapping */
OSErr RegisterPlatformFormat(ResType macType, uint32_t platformFormat,
                            const char *formatName);

/* Map Mac format to platform format */
uint32_t MacToPlatformFormat(ResType macType);

/* Map platform format to Mac format */
ResType PlatformToMacFormat(uint32_t platformFormat);

/* Check if native clipboard has changed */
Boolean HasNativeClipboardChanged(void);

/* Get data from native clipboard */
OSErr GetNativeClipboardData(uint32_t platformFormat, Handle *data);

/* Put data to native clipboard */
OSErr PutNativeClipboardData(uint32_t platformFormat, Handle data);

/*
 * Utility Functions
 */

/* Get scrap data type name */
void GetScrapTypeName(ResType theType, Str255 typeName);

/* Validate scrap data integrity */
OSErr ValidateScrapData(void);

/* Get scrap statistics */
OSErr GetScrapStats(uint32_t *putCount, uint32_t *getCount,
                   uint32_t *conversionCount, uint32_t *errorCount);

/* Reset scrap statistics */
void ResetScrapStats(void);

/* Enable/disable debug logging */
void SetScrapDebugMode(Boolean enable);

/* Get last error information */
OSErr GetLastScrapError(Str255 errorString);

/*
 * Legacy Compatibility Functions
 * These maintain compatibility with older System versions
 */

/* Get TextEdit scrap length (legacy) */
int16_t TEGetScrapLength(void);

/* Copy from scrap to TextEdit (legacy) */
OSErr TEFromScrap(void);

/* Copy from TextEdit to scrap (legacy) */
OSErr TEToScrap(void);

/* Get scrap handle directly (legacy) */
Handle GetScrapHandle(void);

/* Set scrap handle directly (legacy) */
OSErr SetScrapHandle(Handle scrapHandle);

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