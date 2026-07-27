/* #include "SystemTypes.h" */
#include <string.h>
/*
 * ScrapManagerCore.c - Core Scrap Manager Implementation
 * System 7.1 Portable - Scrap Manager Component
 *
 * Implements the core functionality of the Mac OS Scrap Manager including
 * data storage, basic operations, and low-level scrap management.
 */

// #include "CompatibilityFix.h" // Removed

#include "ScrapManager/ScrapManager.h"
#include "ScrapManager/ScrapTypes.h"
#include "ScrapManager/ScrapFormats.h"
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "MemoryMgr/memory_manager_types.h"
#include "ScrapManager/ScrapLogging.h"
/* #include "ErrorCodes.h"
 - error codes in MacTypes.h */

/* Global scrap data structure */
static ScrapStuff gScrapStuff = {0};
static Boolean gScrapInitialized = false;
static ScrapFormatTable *gFormatTable = NULL;
static UInt32 gScrapStats[4] = {0}; /* put, get, conversion, error counts */

/* Internal function prototypes */
static OSErr AllocateFormatTable(SInt16 maxFormats);
static void FreeFormatTable(void);
static OSErr AddFormatEntry(ResType type, SInt32 size, SInt32 offset);
static OSErr RemoveFormatEntry(ResType type);
static ScrapFormatEntry *FindFormatEntry(ResType type);
static OSErr ValidateScrapHandle(Handle scrapHandle);
static void UpdateScrapCount(void);
static OSErr SaveScrapToLowMemory(void);
static OSErr LoadScrapFromLowMemory(void);

/*
 * Core Scrap Manager Functions
 */

OSErr InitScrapManager(void)
{
    OSErr err = noErr;

    if (gScrapInitialized) {
        return noErr;
    }

    /* Initialize global scrap structure */
    memset(&gScrapStuff, 0, sizeof(gScrapStuff));
    gScrapStuff.scrapSize = 0;
    gScrapStuff.scrapHandle = NULL;
    gScrapStuff.scrapCount = 0;
    gScrapStuff.scrapState = 0;
    gScrapStuff.scrapName = NULL;
    gScrapStuff.lastModified = (UInt32)time(NULL);
    gScrapStuff.version = 1;
    gScrapStuff.flags = 0;

    /* Allocate format table */
    err = AllocateFormatTable(32); /* Start with 32 format slots */
    if (err != noErr) {
        return err;
    }

    /* Initialize statistics */
    memset(gScrapStats, 0, sizeof(gScrapStats));

    /* Try to load existing scrap from low memory */
    LoadScrapFromLowMemory();

    gScrapInitialized = true;
    return noErr;
}

void CleanupScrapManager(void)
{
    if (!gScrapInitialized) {
        return;
    }

    /* Save current scrap to low memory */
    SaveScrapToLowMemory();

    /* Free scrap data */
    if (gScrapStuff.scrapHandle) {
        DisposeHandle(gScrapStuff.scrapHandle);
        gScrapStuff.scrapHandle = NULL;
    }

    /* Free scrap name */
    if (gScrapStuff.scrapName) {
        DisposePtr((Ptr)gScrapStuff.scrapName);
        gScrapStuff.scrapName = NULL;
    }

    /* Free format table */
    FreeFormatTable();

    gScrapInitialized = false;
}

PScrapStuff InfoScrap(void)
{
    if (!gScrapInitialized) {
        InitScrapManager();
    }

    return &gScrapStuff;
}

OSErr ZeroScrap(void)
{
    if (!gScrapInitialized) {
        InitScrapManager();
    }

    /* Free existing scrap data */
    if (gScrapStuff.scrapHandle) {
        DisposeHandle(gScrapStuff.scrapHandle);
        gScrapStuff.scrapHandle = NULL;
    }

    /* Clear scrap information */
    gScrapStuff.scrapSize = 0;
    gScrapStuff.scrapState = 0;

    /* Clear format table */
    if (gFormatTable) {
        gFormatTable->count = 0;
    }

    /* Increment change counter */
    UpdateScrapCount();

    /* Update timestamp */
    gScrapStuff.lastModified = (UInt32)time(NULL);

    /* Save to low memory */
    SaveScrapToLowMemory();

    return noErr;
}

OSErr PutScrap(SInt32 length, ResType theType, const void *source)
{
    OSErr err = noErr;
    Handle newHandle = NULL;
    SInt32 newOffset = 0;
    ScrapFormatEntry *existingEntry;

    if (!gScrapInitialized) {
        InitScrapManager();
    }

    if (source == NULL || length < 0) {
        gScrapStats[3]++; /* error count */
        return paramErr;
    }

    if (length > MAX_SCRAP_SIZE) {
        gScrapStats[3]++;
        return scrapSizeError;
    }

    /* Check if this format already exists */
    existingEntry = FindFormatEntry(theType);
    if (existingEntry) {
        /* Remove existing format entry */
        err = RemoveFormatEntry(theType);
        if (err != noErr) {
            gScrapStats[3]++;
            return err;
        }
    }

    /* Calculate new offset */
    newOffset = gScrapStuff.scrapSize;

    /* Resize scrap handle */
    if (gScrapStuff.scrapHandle == NULL) {
        newHandle = NewHandle(length);
        if (newHandle == NULL) {
            gScrapStats[3]++;
            return memFullErr;
        }
    } else {
        if (!SetHandleSize(gScrapStuff.scrapHandle, gScrapStuff.scrapSize + length)) {
            gScrapStats[3]++;
            return memFullErr;
        }
        newHandle = gScrapStuff.scrapHandle;
    }

    /* Copy data to scrap handle */
    HLock(newHandle);
    memcpy(*newHandle + newOffset, source, length);
    HUnlock(newHandle);

    /* Update scrap information */
    gScrapStuff.scrapHandle = newHandle;
    gScrapStuff.scrapSize += length;
    gScrapStuff.scrapState |= SCRAP_STATE_LOADED;

    /* Add format entry */
    err = AddFormatEntry(theType, length, newOffset);
    if (err != noErr) {
        gScrapStats[3]++;
        return err;
    }

    /* Increment change counter */
    UpdateScrapCount();

    /* Update timestamp */
    gScrapStuff.lastModified = (UInt32)time(NULL);

    /* Save to low memory */
    SaveScrapToLowMemory();

    gScrapStats[0]++; /* put count */
    return noErr;
}

OSErr GetScrap(Handle destHandle, ResType theType, SInt32 *offset)
{
    OSErr err = noErr;
    ScrapFormatEntry *formatEntry;

    if (!gScrapInitialized) {
        InitScrapManager();
    }

    if (destHandle == NULL) {
        gScrapStats[3]++;
        return paramErr;
    }

    /* Find the requested format */
    formatEntry = FindFormatEntry(theType);
    if (formatEntry == NULL) {
        gScrapStats[3]++;
        return scrapNoTypeError;
    }

    /* Check if scrap data is available */
    if (gScrapStuff.scrapHandle == NULL) {
        gScrapStats[3]++;
        return scrapNoScrap;
    }

    /* Validate scrap handle */
    err = ValidateScrapHandle(gScrapStuff.scrapHandle);
    if (err != noErr) {
        gScrapStats[3]++;
        return err;
    }

    /* Resize destination handle */
    SetHandleSize(destHandle, formatEntry->size);
    if (MemError() != noErr) {
        gScrapStats[3]++;
        return memFullErr;
    }

    /* Copy data from scrap */
    HLock(destHandle);
    HLock(gScrapStuff.scrapHandle);
    memcpy(*destHandle, *gScrapStuff.scrapHandle + formatEntry->offset, formatEntry->size);
    HUnlock(gScrapStuff.scrapHandle);
    HUnlock(destHandle);

    /* Return offset if requested */
    if (offset) {
        *offset = formatEntry->offset;
    }

    gScrapStats[1]++; /* get count */
    return formatEntry->size; /* Return size as positive result */
}

OSErr LoadScrap(void)
{
    /* For now, scrap is always in memory or low memory globals */
    /* This would load from disk file in full implementation */

    if (!gScrapInitialized) {
        InitScrapManager();
    }

    return LoadScrapFromLowMemory();
}

OSErr UnloadScrap(void)
{
    /* For now, just save to low memory globals */
    /* This would save to disk file in full implementation */

    if (!gScrapInitialized) {
        return scrapNoScrap;
    }

    return SaveScrapToLowMemory();
}

/*
 * Extended Functions
 */

OSErr GetScrapFormats(ResType *types, SInt16 *count, SInt16 maxTypes)
{
    SInt16 i, actualCount = 0;

    if (!gScrapInitialized) {
        InitScrapManager();
    }

    if (types == NULL || count == NULL) {
        return paramErr;
    }

    if (gFormatTable == NULL) {
        *count = 0;
        return noErr;
    }

    for (i = 0; i < gFormatTable->count && actualCount < maxTypes; i++) {
        types[actualCount] = gFormatTable->formats[i].type;
        actualCount++;
    }

    *count = actualCount;
    return noErr;
}

Boolean HasScrapFormat(ResType theType)
{
    if (!gScrapInitialized) {
        InitScrapManager();
    }

    return (FindFormatEntry(theType) != NULL);
}

SInt32 GetScrapSize(ResType theType)
{
    ScrapFormatEntry *formatEntry;

    if (!gScrapInitialized) {
        InitScrapManager();
    }

    formatEntry = FindFormatEntry(theType);
    if (formatEntry == NULL) {
        return 0;
    }

    return formatEntry->size;
}

/*
 * Internal Helper Functions
 */

static OSErr AllocateFormatTable(SInt16 maxFormats)
{
    size_t tableSize = sizeof(ScrapFormatTable) +
                      (maxFormats - 1) * sizeof(ScrapFormatEntry);

    gFormatTable = (ScrapFormatTable *)NewPtrClear(tableSize);
    if (gFormatTable == NULL) {
        return memFullErr;
    }

    gFormatTable->count = 0;
    gFormatTable->maxCount = maxFormats;

    return noErr;
}

static void FreeFormatTable(void)
{
    if (gFormatTable) {
        DisposePtr((Ptr)gFormatTable);
        gFormatTable = NULL;
    }
}

static OSErr AddFormatEntry(ResType type, SInt32 size, SInt32 offset)
{
    ScrapFormatEntry *entry;

    if (gFormatTable == NULL) {
        return paramErr;
    }

    if (gFormatTable->count >= gFormatTable->maxCount) {
        return scrapTooManyFormats;
    }

    entry = &gFormatTable->formats[gFormatTable->count];
    entry->type = type;
    entry->size = size;
    entry->offset = offset;
    entry->flags = 0;
    entry->reserved = 0;

    gFormatTable->count++;

    return noErr;
}

static OSErr RemoveFormatEntry(ResType type)
{
    SInt16 i, j;

    if (gFormatTable == NULL) {
        return paramErr;
    }

    for (i = 0; i < gFormatTable->count; i++) {
        if (gFormatTable->formats[i].type == type) {
            /* Shift remaining entries down */
            for (j = i; j < gFormatTable->count - 1; j++) {
                gFormatTable->formats[j] = gFormatTable->formats[j + 1];
            }
            gFormatTable->count--;
            return noErr;
        }
    }

    return scrapNoTypeError;
}

static ScrapFormatEntry *FindFormatEntry(ResType type)
{
    SInt16 i;

    if (gFormatTable == NULL) {
        return NULL;
    }

    for (i = 0; i < gFormatTable->count; i++) {
        if (gFormatTable->formats[i].type == type) {
            return &gFormatTable->formats[i];
        }
    }

    return NULL;
}

static OSErr ValidateScrapHandle(Handle scrapHandle)
{
    if (scrapHandle == NULL) {
        return scrapNoScrap;
    }

    /* Check handle validity */
    if (GetHandleSize(scrapHandle) != gScrapStuff.scrapSize) {
        return scrapCorruptError;
    }

    return noErr;
}

static void UpdateScrapCount(void)
{
    gScrapStuff.scrapCount++;
    if (gScrapStuff.scrapCount < 0) {
        gScrapStuff.scrapCount = 1; /* Handle overflow */
    }
}

static OSErr SaveScrapToLowMemory(void)
{
    /* This would save to actual low memory globals in real implementation */
    /* For now, we simulate this with internal state */

    /* In real Mac OS, this would set:
     * - ScrapSize at 0x0960
     * - ScrapHandle at 0x0964
     * - ScrapCount at 0x0968
     * - ScrapState at 0x096A
     * - ScrapName at 0x096C
     */

    return noErr;
}

static OSErr LoadScrapFromLowMemory(void)
{
    /* This would load from actual low memory globals in real implementation */
    /* For now, we start with empty scrap */

    /* In real Mac OS, this would read:
     * - ScrapSize from 0x0960
     * - ScrapHandle from 0x0964
     * - ScrapCount from 0x0968
     * - ScrapState from 0x096A
     * - ScrapName from 0x096C
     */

    return noErr;
}

/*
 * Statistics and Debugging Functions
 */

OSErr GetScrapStats(UInt32 *putCount, UInt32 *getCount,
                   UInt32 *conversionCount, UInt32 *errorCount)
{
    if (putCount) *putCount = gScrapStats[0];
    if (getCount) *getCount = gScrapStats[1];
    if (conversionCount) *conversionCount = gScrapStats[2];
    if (errorCount) *errorCount = gScrapStats[3];

    return noErr;
}

void ResetScrapStats(void)
{
    memset(gScrapStats, 0, sizeof(gScrapStats));
}

OSErr ValidateScrapData(void)
{
    SInt16 i;
    ScrapFormatEntry *entry;
    SInt32 totalSize = 0;

    if (!gScrapInitialized) {
        return scrapNoScrap;
    }

    if (gFormatTable == NULL) {
        return noErr; /* Empty scrap is valid */
    }

    /* Validate format table consistency */
    for (i = 0; i < gFormatTable->count; i++) {
        entry = &gFormatTable->formats[i];

        /* Check for valid offset and size */
        if (entry->offset < 0 || entry->size < 0) {
            return scrapCorruptError;
        }

        if (entry->offset + entry->size > gScrapStuff.scrapSize) {
            return scrapCorruptError;
        }

        totalSize += entry->size;
    }

    /* Validate scrap handle */
    return ValidateScrapHandle(gScrapStuff.scrapHandle);
}

/*
 * Legacy Compatibility Functions
 */

SInt16 TEGetScrapLength(void)
{
    SInt32 size = GetScrapSize(SCRAP_TYPE_TEXT);
    return (size > 32767) ? 32767 : (SInt16)size;
}

OSErr TEFromScrap(void)
{
    /* This would integrate with TextEdit in real implementation */
    return noErr;
}

OSErr TEToScrap(void)
{
    /* This would integrate with TextEdit in real implementation */
    return noErr;
}

Handle GetScrapHandle(void)
{
    if (!gScrapInitialized) {
        InitScrapManager();
    }

    return gScrapStuff.scrapHandle;
}

OSErr SetScrapHandle(Handle scrapHandle)
{
    if (!gScrapInitialized) {
        InitScrapManager();
    }

    /* Free existing handle */
    if (gScrapStuff.scrapHandle && gScrapStuff.scrapHandle != scrapHandle) {
        DisposeHandle(gScrapStuff.scrapHandle);
    }

    gScrapStuff.scrapHandle = scrapHandle;
    gScrapStuff.scrapSize = scrapHandle ? GetHandleSize(scrapHandle) : 0;

    /* Clear format table since we don't know the new format structure */
    if (gFormatTable) {
        gFormatTable->count = 0;
    }

    UpdateScrapCount();
    SaveScrapToLowMemory();

    return noErr;
}
