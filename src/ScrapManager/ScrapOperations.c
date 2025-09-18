/*
 * ScrapOperations.c - High-Level Scrap Operation Implementation
 * System 7.1 Portable - Scrap Manager Component
 *
 * Implements high-level scrap operations including copy/paste operations,
 * batch operations, streaming, and content analysis for the Mac OS Scrap Manager.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ScrapManager/ScrapOperations.h"
#include "ScrapManager/ScrapManager.h"
#include "ScrapManager/ScrapFormats.h"
#include "ScrapManager/ScrapTypes.h"
#include "MacTypes.h"
#include "MemoryManager.h"
#include "ErrorCodes.h"

/* Async operation tracking */
typedef struct {
    int32_t         operationID;
    Boolean         isActive;
    Boolean         isComplete;
    OSErr           result;
    ScrapIOCallback completion;
    void            *userData;
    time_t          startTime;
} AsyncOperation;

/* Stream operation tracking */
typedef struct {
    int32_t         streamID;
    Boolean         isActive;
    Boolean         isReading;
    ResType         dataType;
    Handle          dataHandle;
    int32_t         currentOffset;
    int32_t         totalSize;
} StreamOperation;

/* Global operation state */
static AsyncOperation gAsyncOps[16];
static StreamOperation gStreamOps[8];
static int32_t gNextOperationID = 1;
static int32_t gNextStreamID = 1;
static Boolean gOperationsInitialized = false;

/* Internal function prototypes */
static void InitializeOperations(void);
static AsyncOperation *AllocateAsyncOperation(void);
static void FreeAsyncOperation(int32_t operationID);
static StreamOperation *AllocateStreamOperation(void);
static void FreeStreamOperation(int32_t streamID);
static OSErr ExecuteAsyncCopy(const void *data, int32_t size, ResType dataType,
                              ScrapIOCallback completion, void *userData);
static OSErr ExecuteAsyncPaste(ResType dataType, ScrapIOCallback completion,
                               void *userData);

/*
 * High-Level Scrap Operations
 */

OSErr CopyToScrap(const void *data, int32_t size, ResType primaryType,
                 const ResType *additionalTypes, int16_t typeCount)
{
    OSErr err = noErr;
    int16_t i;

    if (data == NULL || size < 0) {
        return paramErr;
    }

    if (!gOperationsInitialized) {
        InitializeOperations();
    }

    /* Clear existing scrap */
    err = ZeroScrap();
    if (err != noErr) {
        return err;
    }

    /* Put primary data */
    err = PutScrap(size, primaryType, data);
    if (err != noErr) {
        return err;
    }

    /* Add additional format conversions */
    if (additionalTypes && typeCount > 0) {
        for (i = 0; i < typeCount; i++) {
            Handle convertedData = NULL;

            /* Convert primary data to additional format */
            err = ConvertScrapFormat(primaryType, additionalTypes[i],
                                   NewHandleFromData(data, size), &convertedData);

            if (err == noErr && convertedData) {
                HLock(convertedData);
                PutScrap(GetHandleSize(convertedData), additionalTypes[i], *convertedData);
                HUnlock(convertedData);
                DisposeHandle(convertedData);
            }
        }
    }

    return noErr;
}

OSErr PasteFromScrap(Handle *data, ResType *actualType,
                    const ResType *preferredTypes, int16_t typeCount)
{
    OSErr err = noErr;
    int16_t i;
    int32_t offset;

    if (data == NULL || actualType == NULL) {
        return paramErr;
    }

    if (!gOperationsInitialized) {
        InitializeOperations();
    }

    *data = NULL;
    *actualType = 0;

    /* Try preferred types in order */
    if (preferredTypes && typeCount > 0) {
        for (i = 0; i < typeCount; i++) {
            if (HasScrapFormat(preferredTypes[i])) {
                *data = NewHandle(0);
                if (*data == NULL) {
                    return memFullErr;
                }

                err = GetScrap(*data, preferredTypes[i], &offset);
                if (err >= 0) { /* GetScrap returns size on success */
                    *actualType = preferredTypes[i];
                    return noErr;
                } else {
                    DisposeHandle(*data);
                    *data = NULL;
                }
            }
        }
    }

    /* Try to get any available format */
    ResType availableTypes[16];
    int16_t availableCount;

    err = GetScrapFormats(availableTypes, &availableCount, 16);
    if (err != noErr || availableCount == 0) {
        return scrapNoScrap;
    }

    *data = NewHandle(0);
    if (*data == NULL) {
        return memFullErr;
    }

    err = GetScrap(*data, availableTypes[0], &offset);
    if (err >= 0) {
        *actualType = availableTypes[0];
        return noErr;
    }

    DisposeHandle(*data);
    *data = NULL;
    return err;
}

OSErr CopyTextToScrap(const void *textData, int32_t textSize, Boolean includeFormatting)
{
    OSErr err = noErr;
    ResType additionalTypes[4];
    int16_t typeCount = 0;

    if (textData == NULL || textSize < 0) {
        return paramErr;
    }

    /* Always include UTF-8 conversion for modern compatibility */
    additionalTypes[typeCount++] = SCRAP_TYPE_UTF8;

    if (includeFormatting) {
        /* Include style information if requested */
        additionalTypes[typeCount++] = SCRAP_TYPE_STYLE;
        additionalTypes[typeCount++] = SCRAP_TYPE_RTF;
    }

    return CopyToScrap(textData, textSize, SCRAP_TYPE_TEXT, additionalTypes, typeCount);
}

OSErr PasteTextFromScrap(Handle *textData, ResType preferredFormat)
{
    ResType preferredTypes[4];
    ResType actualType;
    int16_t typeCount = 0;

    if (textData == NULL) {
        return paramErr;
    }

    /* Build preference list */
    if (preferredFormat != 0) {
        preferredTypes[typeCount++] = preferredFormat;
    }

    /* Add common text formats in preference order */
    preferredTypes[typeCount++] = SCRAP_TYPE_UTF8;
    preferredTypes[typeCount++] = SCRAP_TYPE_TEXT;
    preferredTypes[typeCount++] = SCRAP_TYPE_RTF;

    return PasteFromScrap(textData, &actualType, preferredTypes, typeCount);
}

OSErr CopyImageToScrap(Handle imageData, ResType sourceFormat)
{
    OSErr err = noErr;
    ResType additionalTypes[3];
    int16_t typeCount = 0;

    if (imageData == NULL) {
        return paramErr;
    }

    /* Add modern image format conversions */
    if (sourceFormat == SCRAP_TYPE_PICT) {
        additionalTypes[typeCount++] = SCRAP_TYPE_PNG;
        additionalTypes[typeCount++] = SCRAP_TYPE_JPEG;
    } else {
        additionalTypes[typeCount++] = SCRAP_TYPE_PICT;
    }

    HLock(imageData);
    err = CopyToScrap(*imageData, GetHandleSize(imageData), sourceFormat,
                     additionalTypes, typeCount);
    HUnlock(imageData);

    return err;
}

OSErr PasteImageFromScrap(Handle *imageData, ResType preferredFormat,
                         int16_t maxWidth, int16_t maxHeight)
{
    OSErr err = noErr;
    ResType preferredTypes[4];
    ResType actualType;
    int16_t typeCount = 0;

    if (imageData == NULL) {
        return paramErr;
    }

    /* Build image format preference list */
    if (preferredFormat != 0) {
        preferredTypes[typeCount++] = preferredFormat;
    }

    preferredTypes[typeCount++] = SCRAP_TYPE_PNG;
    preferredTypes[typeCount++] = SCRAP_TYPE_PICT;
    preferredTypes[typeCount++] = SCRAP_TYPE_JPEG;

    err = PasteFromScrap(imageData, &actualType, preferredTypes, typeCount);

    /* TODO: Implement image scaling if maxWidth/maxHeight specified */
    /* This would require image processing capabilities */

    return err;
}

/*
 * Batch Operations
 */

OSErr CopyBatchToScrap(const void **dataItems, const int32_t *dataSizes,
                      const ResType *dataTypes, int16_t itemCount)
{
    OSErr err = noErr;
    int16_t i;

    if (dataItems == NULL || dataSizes == NULL || dataTypes == NULL || itemCount <= 0) {
        return paramErr;
    }

    /* Clear existing scrap */
    err = ZeroScrap();
    if (err != noErr) {
        return err;
    }

    /* Add each data item */
    for (i = 0; i < itemCount; i++) {
        if (dataItems[i] && dataSizes[i] > 0) {
            err = PutScrap(dataSizes[i], dataTypes[i], dataItems[i]);
            if (err != noErr) {
                return err; /* Stop on first error */
            }
        }
    }

    return noErr;
}

OSErr PasteBatchFromScrap(void ***dataItems, int32_t **dataSizes,
                         ResType **dataTypes, int16_t *itemCount,
                         const ResType *requestedTypes, int16_t requestCount)
{
    OSErr err = noErr;
    ResType availableTypes[32];
    int16_t availableCount, actualCount = 0;
    int16_t i, j;

    if (dataItems == NULL || dataSizes == NULL || dataTypes == NULL || itemCount == NULL) {
        return paramErr;
    }

    /* Get available formats */
    err = GetScrapFormats(availableTypes, &availableCount, 32);
    if (err != noErr) {
        return err;
    }

    /* Count matching formats */
    if (requestedTypes && requestCount > 0) {
        for (i = 0; i < requestCount; i++) {
            for (j = 0; j < availableCount; j++) {
                if (requestedTypes[i] == availableTypes[j]) {
                    actualCount++;
                    break;
                }
            }
        }
    } else {
        actualCount = availableCount;
    }

    if (actualCount == 0) {
        *itemCount = 0;
        return scrapNoScrap;
    }

    /* Allocate arrays */
    *dataItems = (void **)NewPtrClear(actualCount * sizeof(void *));
    *dataSizes = (int32_t *)NewPtrClear(actualCount * sizeof(int32_t));
    *dataTypes = (ResType *)NewPtrClear(actualCount * sizeof(ResType));

    if (!*dataItems || !*dataSizes || !*dataTypes) {
        return memFullErr;
    }

    /* Extract data items */
    actualCount = 0;
    if (requestedTypes && requestCount > 0) {
        for (i = 0; i < requestCount; i++) {
            if (HasScrapFormat(requestedTypes[i])) {
                Handle dataHandle = NewHandle(0);
                int32_t offset;

                err = GetScrap(dataHandle, requestedTypes[i], &offset);
                if (err >= 0) {
                    (*dataSizes)[actualCount] = GetHandleSize(dataHandle);
                    (*dataItems)[actualCount] = NewPtr((*dataSizes)[actualCount]);
                    (*dataTypes)[actualCount] = requestedTypes[i];

                    if ((*dataItems)[actualCount]) {
                        HLock(dataHandle);
                        memcpy((*dataItems)[actualCount], *dataHandle, (*dataSizes)[actualCount]);
                        HUnlock(dataHandle);
                        actualCount++;
                    }
                }
                DisposeHandle(dataHandle);
            }
        }
    } else {
        for (i = 0; i < availableCount; i++) {
            Handle dataHandle = NewHandle(0);
            int32_t offset;

            err = GetScrap(dataHandle, availableTypes[i], &offset);
            if (err >= 0) {
                (*dataSizes)[actualCount] = GetHandleSize(dataHandle);
                (*dataItems)[actualCount] = NewPtr((*dataSizes)[actualCount]);
                (*dataTypes)[actualCount] = availableTypes[i];

                if ((*dataItems)[actualCount]) {
                    HLock(dataHandle);
                    memcpy((*dataItems)[actualCount], *dataHandle, (*dataSizes)[actualCount]);
                    HUnlock(dataHandle);
                    actualCount++;
                }
            }
            DisposeHandle(dataHandle);
        }
    }

    *itemCount = actualCount;
    return noErr;
}

/*
 * Streaming Operations
 */

OSErr BeginScrapStream(ResType dataType, int32_t estimatedSize, int32_t *streamID)
{
    StreamOperation *stream;

    if (streamID == NULL) {
        return paramErr;
    }

    if (!gOperationsInitialized) {
        InitializeOperations();
    }

    stream = AllocateStreamOperation();
    if (stream == NULL) {
        return scrapMemoryError;
    }

    stream->dataType = dataType;
    stream->isReading = false;
    stream->currentOffset = 0;
    stream->totalSize = estimatedSize;
    stream->dataHandle = NewHandle(estimatedSize > 0 ? estimatedSize : 1024);

    if (stream->dataHandle == NULL) {
        FreeStreamOperation(stream->streamID);
        return memFullErr;
    }

    *streamID = stream->streamID;
    return noErr;
}

OSErr WriteScrapStream(int32_t streamID, const void *data, int32_t size)
{
    StreamOperation *stream = NULL;
    int16_t i;

    if (data == NULL || size <= 0) {
        return paramErr;
    }

    /* Find stream operation */
    for (i = 0; i < 8; i++) {
        if (gStreamOps[i].isActive && gStreamOps[i].streamID == streamID) {
            stream = &gStreamOps[i];
            break;
        }
    }

    if (stream == NULL || stream->isReading) {
        return paramErr;
    }

    /* Resize handle if necessary */
    if (stream->currentOffset + size > GetHandleSize(stream->dataHandle)) {
        SetHandleSize(stream->dataHandle, stream->currentOffset + size + 1024);
        if (MemError() != noErr) {
            return memFullErr;
        }
    }

    /* Write data */
    HLock(stream->dataHandle);
    memcpy(*stream->dataHandle + stream->currentOffset, data, size);
    HUnlock(stream->dataHandle);

    stream->currentOffset += size;

    return noErr;
}

OSErr EndScrapStream(int32_t streamID)
{
    StreamOperation *stream = NULL;
    OSErr err = noErr;
    int16_t i;

    /* Find stream operation */
    for (i = 0; i < 8; i++) {
        if (gStreamOps[i].isActive && gStreamOps[i].streamID == streamID) {
            stream = &gStreamOps[i];
            break;
        }
    }

    if (stream == NULL) {
        return paramErr;
    }

    if (!stream->isReading) {
        /* Finalize write stream */
        SetHandleSize(stream->dataHandle, stream->currentOffset);

        HLock(stream->dataHandle);
        err = PutScrap(stream->currentOffset, stream->dataType, *stream->dataHandle);
        HUnlock(stream->dataHandle);
    }

    /* Clean up */
    if (stream->dataHandle) {
        DisposeHandle(stream->dataHandle);
    }
    FreeStreamOperation(streamID);

    return err;
}

/*
 * Asynchronous Operations
 */

OSErr AsyncCopyToScrap(const void *data, int32_t size, ResType dataType,
                      ScrapIOCallback completion, void *userData)
{
    if (!gOperationsInitialized) {
        InitializeOperations();
    }

    /* For now, execute synchronously and call completion */
    return ExecuteAsyncCopy(data, size, dataType, completion, userData);
}

OSErr AsyncPasteFromScrap(ResType dataType, ScrapIOCallback completion, void *userData)
{
    if (!gOperationsInitialized) {
        InitializeOperations();
    }

    /* For now, execute synchronously and call completion */
    return ExecuteAsyncPaste(dataType, completion, userData);
}

OSErr CheckAsyncOperation(int32_t operationID, Boolean *isComplete, OSErr *result)
{
    int16_t i;

    if (isComplete == NULL) {
        return paramErr;
    }

    for (i = 0; i < 16; i++) {
        if (gAsyncOps[i].isActive && gAsyncOps[i].operationID == operationID) {
            *isComplete = gAsyncOps[i].isComplete;
            if (result) {
                *result = gAsyncOps[i].result;
            }
            return noErr;
        }
    }

    return paramErr;
}

/*
 * Content Analysis Functions
 */

Boolean ScrapContainsText(void)
{
    return HasScrapFormat(SCRAP_TYPE_TEXT) || HasScrapFormat(SCRAP_TYPE_UTF8) ||
           HasScrapFormat(SCRAP_TYPE_STRING) || HasScrapFormat(SCRAP_TYPE_RTF);
}

Boolean ScrapContainsImages(void)
{
    return HasScrapFormat(SCRAP_TYPE_PICT) || HasScrapFormat(SCRAP_TYPE_PNG) ||
           HasScrapFormat(SCRAP_TYPE_JPEG) || HasScrapFormat(SCRAP_TYPE_TIFF);
}

Boolean ScrapContainsFiles(void)
{
    return HasScrapFormat(SCRAP_TYPE_FILE) || HasScrapFormat(SCRAP_TYPE_FOLDER);
}

Boolean ScrapContainsSound(void)
{
    return HasScrapFormat(SCRAP_TYPE_SOUND);
}

ResType GetBestTextFormat(void)
{
    if (HasScrapFormat(SCRAP_TYPE_UTF8)) return SCRAP_TYPE_UTF8;
    if (HasScrapFormat(SCRAP_TYPE_TEXT)) return SCRAP_TYPE_TEXT;
    if (HasScrapFormat(SCRAP_TYPE_RTF)) return SCRAP_TYPE_RTF;
    if (HasScrapFormat(SCRAP_TYPE_STRING)) return SCRAP_TYPE_STRING;
    return 0;
}

ResType GetBestImageFormat(void)
{
    if (HasScrapFormat(SCRAP_TYPE_PNG)) return SCRAP_TYPE_PNG;
    if (HasScrapFormat(SCRAP_TYPE_PICT)) return SCRAP_TYPE_PICT;
    if (HasScrapFormat(SCRAP_TYPE_JPEG)) return SCRAP_TYPE_JPEG;
    if (HasScrapFormat(SCRAP_TYPE_TIFF)) return SCRAP_TYPE_TIFF;
    return 0;
}

/*
 * Internal Helper Functions
 */

static void InitializeOperations(void)
{
    if (gOperationsInitialized) {
        return;
    }

    memset(gAsyncOps, 0, sizeof(gAsyncOps));
    memset(gStreamOps, 0, sizeof(gStreamOps));

    gOperationsInitialized = true;
}

static AsyncOperation *AllocateAsyncOperation(void)
{
    int16_t i;

    for (i = 0; i < 16; i++) {
        if (!gAsyncOps[i].isActive) {
            memset(&gAsyncOps[i], 0, sizeof(AsyncOperation));
            gAsyncOps[i].operationID = gNextOperationID++;
            gAsyncOps[i].isActive = true;
            gAsyncOps[i].startTime = time(NULL);
            return &gAsyncOps[i];
        }
    }

    return NULL;
}

static StreamOperation *AllocateStreamOperation(void)
{
    int16_t i;

    for (i = 0; i < 8; i++) {
        if (!gStreamOps[i].isActive) {
            memset(&gStreamOps[i], 0, sizeof(StreamOperation));
            gStreamOps[i].streamID = gNextStreamID++;
            gStreamOps[i].isActive = true;
            return &gStreamOps[i];
        }
    }

    return NULL;
}

static void FreeStreamOperation(int32_t streamID)
{
    int16_t i;

    for (i = 0; i < 8; i++) {
        if (gStreamOps[i].isActive && gStreamOps[i].streamID == streamID) {
            gStreamOps[i].isActive = false;
            break;
        }
    }
}

static OSErr ExecuteAsyncCopy(const void *data, int32_t size, ResType dataType,
                              ScrapIOCallback completion, void *userData)
{
    OSErr err = PutScrap(size, dataType, data);

    if (completion) {
        completion(err, userData);
    }

    return err;
}

static OSErr ExecuteAsyncPaste(ResType dataType, ScrapIOCallback completion, void *userData)
{
    Handle dataHandle = NewHandle(0);
    int32_t offset;
    OSErr err;

    if (dataHandle == NULL) {
        err = memFullErr;
    } else {
        err = GetScrap(dataHandle, dataType, &offset);
        DisposeHandle(dataHandle);
    }

    if (completion) {
        completion(err, userData);
    }

    return err;
}

/* Helper function to create handle from data */
static Handle NewHandleFromData(const void *data, int32_t size)
{
    Handle h = NewHandle(size);
    if (h && data && size > 0) {
        HLock(h);
        memcpy(*h, data, size);
        HUnlock(h);
    }
    return h;
}