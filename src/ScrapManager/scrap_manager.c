/* #include "SystemTypes.h" */
#include <stdlib.h>
#include <string.h>
/*
 * Macintosh Scrap Manager Implementation
 * Based on System 6.0.7 behavior
 *
 * This implementation follows the behavior documented in Inside Macintosh
 * and observed through analysis of System 6.0.7
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "scrap_manager.h"
#include "ScrapManager/ScrapLogging.h"


/* Global scrap state (normally in low memory) */
static ScrapStuff gScrapStuff = {
    .scrapSize = 0,
    .scrapHandle = NULL,
    .scrapCount = 0,
    .scrapState = scrapNotLoaded,
    .scrapName = "Desk Scrap"
};

/*
 * ZeroScrap (A9FC)
 * Clears the scrap and increments the change count
 *

 * System 6.0.7: Trap table entry at A9FC
 */
OSErr ZeroScrap(void)
{
    /* Dispose existing scrap handle if present */
    if (gScrapStuff.scrapHandle != NULL) {
        /* DisposeHandle(gScrapStuff.scrapHandle); */
        free(gScrapStuff.scrapHandle);
        gScrapStuff.scrapHandle = NULL;
    }

    /* Reset scrap state */
    gScrapStuff.scrapSize = 0;
    gScrapStuff.scrapCount++;  /* Increment change count */
    gScrapStuff.scrapState = scrapLoaded;

    /* Allocate minimal scrap */
    gScrapStuff.scrapHandle = (Handle)malloc(sizeof(void*));
    if (gScrapStuff.scrapHandle) {
        *gScrapStuff.scrapHandle = malloc(4);  /* Minimal allocation */
    }

    return noErr;
}

/*
 * InfoScrap (A9F9)
 * Returns pointer to scrap information record
 *

 * System 6.0.7: Trap table entry at A9F9
 */
ScrapStuff* InfoScrap(void)
{
    return &gScrapStuff;
}

/*
 * PutScrap (A9FE)
 * Puts data of specified type into the scrap
 *

 * System 6.0.7: Trap table entry at A9FE
 *
 * Parameters:
 *   length: Number of bytes to copy
 *   theType: OSType identifying data type
 *   source: Pointer to source data
 */
OSErr PutScrap(SInt32 length, OSType theType, Ptr source)
{
    /* Ensure scrap is loaded */
    if (gScrapStuff.scrapState != scrapLoaded) {
        OSErr err = LoadScrap();
        if (err != noErr) return err;
    }

    /* Calculate new size needed */
    SInt32 newSize = gScrapStuff.scrapSize + 8 + length;  /* Type + Length + Data */

    /* Reallocate scrap handle */
    if (gScrapStuff.scrapHandle == NULL) {
        gScrapStuff.scrapHandle = (Handle)malloc(sizeof(void*));
        *gScrapStuff.scrapHandle = malloc(newSize);
    } else {
        *gScrapStuff.scrapHandle = realloc(*gScrapStuff.scrapHandle, newSize);
    }

    if (*gScrapStuff.scrapHandle == NULL) {
        return -1;  /* Memory error */
    }

    /* Add new data to scrap */
    char* scrapPtr = (char*)*gScrapStuff.scrapHandle + gScrapStuff.scrapSize;

    /* Write type (4 bytes) */
    *(OSType*)scrapPtr = theType;
    scrapPtr += 4;

    /* Write length (4 bytes) */
    *(SInt32*)scrapPtr = length;
    scrapPtr += 4;

    /* Write data */
    memcpy(scrapPtr, source, length);

    /* Update scrap size */
    gScrapStuff.scrapSize = newSize;
    gScrapStuff.scrapState = scrapDirty;

    return noErr;
}

/*
 * GetScrap (A9FD)
 * Gets data of specified type from the scrap
 *

 * System 6.0.7: Trap table entry at A9FD
 *
 * Parameters:
 *   hDest: Handle to receive data (can be NULL to just get size)
 *   theType: OSType of data to retrieve
 *   offset: Returns offset of data in scrap
 *
 * Returns: Size of data, or negative error code
 */
SInt32 GetScrap(Handle hDest, OSType theType, SInt32 *offset)
{
    /* Ensure scrap is loaded */
    if (gScrapStuff.scrapState != scrapLoaded && gScrapStuff.scrapState != scrapDirty) {
        OSErr err = LoadScrap();
        if (err != noErr) return err;
    }

    if (gScrapStuff.scrapHandle == NULL || gScrapStuff.scrapSize == 0) {
        return noTypeErr;  /* No data in scrap */
    }

    /* Search for requested type in scrap */
    char* scrapPtr = (char*)*gScrapStuff.scrapHandle;
    SInt32 currentOffset = 0;

    while (currentOffset < gScrapStuff.scrapSize) {
        OSType dataType = *(OSType*)(scrapPtr + currentOffset);
        SInt32 dataSize = *(SInt32*)(scrapPtr + currentOffset + 4);

        if (dataType == theType) {
            /* Found requested type */
            if (offset) *offset = currentOffset + 8;

            if (hDest != NULL) {
                /* Resize destination handle */
                if (*hDest) {
                    *hDest = realloc(*hDest, dataSize);
                } else {
                    *hDest = malloc(dataSize);
                }

                /* Copy data */
                memcpy(*hDest, scrapPtr + currentOffset + 8, dataSize);
            }

            return dataSize;
        }

        /* Move to next entry */
        currentOffset += 8 + dataSize;
    }

    return noTypeErr;  /* Type not found */
}

/*
 * LoadScrap (A9FB)
 * Loads scrap from desk scrap file
 *

 * System 6.0.7: Trap table entry at A9FB
 */
OSErr LoadScrap(void)
{
    /* TODO: Read from actual desk scrap file */
    /* For now, just mark as loaded */

    if (gScrapStuff.scrapState == scrapNotLoaded) {
        /* Allocate empty scrap */
        if (gScrapStuff.scrapHandle == NULL) {
            gScrapStuff.scrapHandle = (Handle)malloc(sizeof(void*));
            *gScrapStuff.scrapHandle = malloc(4);
        }
        gScrapStuff.scrapState = scrapLoaded;
    }

    return noErr;
}

/*
 * UnloadScrap (A9FA)
 * Writes scrap to desk scrap file and frees memory
 *

 * System 6.0.7: Trap table entry at A9FA
 */
OSErr UnloadScrap(void)
{
    if (gScrapStuff.scrapState == scrapDirty) {
        /* TODO: Write to desk scrap file */
    }

    /* Free scrap memory */
    if (gScrapStuff.scrapHandle != NULL) {
        if (*gScrapStuff.scrapHandle) {
            free(*gScrapStuff.scrapHandle);
        }
        free(gScrapStuff.scrapHandle);
        gScrapStuff.scrapHandle = NULL;
    }

    gScrapStuff.scrapState = scrapNotLoaded;

    return noErr;
}
