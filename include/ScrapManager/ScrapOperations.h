/*
 * ScrapOperations.h - Scrap Operation APIs
 * System 7.1 Portable - Scrap Manager Component
 *
 * Defines high-level scrap operation functions for data transfer,
 * clipboard management, and inter-application communication.
 */

#ifndef SCRAP_OPERATIONS_H
#define SCRAP_OPERATIONS_H

#include "ScrapTypes.h"
#include "MacTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-Level Scrap Operations
 */

/* Copy data to scrap with multiple formats */
OSErr CopyToScrap(const void *data, int32_t size, ResType primaryType,
                 const ResType *additionalTypes, int16_t typeCount);

/* Paste data from scrap with format preference */
OSErr PasteFromScrap(Handle *data, ResType *actualType,
                    const ResType *preferredTypes, int16_t typeCount);

/* Copy text with automatic format detection */
OSErr CopyTextToScrap(const void *textData, int32_t textSize,
                     Boolean includeFormatting);

/* Paste text with format conversion */
OSErr PasteTextFromScrap(Handle *textData, ResType preferredFormat);

/* Copy image with multiple format variants */
OSErr CopyImageToScrap(Handle imageData, ResType sourceFormat);

/* Paste image with format conversion */
OSErr PasteImageFromScrap(Handle *imageData, ResType preferredFormat,
                         int16_t maxWidth, int16_t maxHeight);

/* Copy file references to scrap */
OSErr CopyFilesToScrap(const FSSpec *files, int16_t fileCount);

/* Paste file references from scrap */
OSErr PasteFilesFromScrap(FSSpec **files, int16_t *fileCount);

/*
 * Batch Operations
 */

/* Copy multiple data items as a batch */
OSErr CopyBatchToScrap(const void **dataItems, const int32_t *dataSizes,
                      const ResType *dataTypes, int16_t itemCount);

/* Paste multiple data items as a batch */
OSErr PasteBatchFromScrap(void ***dataItems, int32_t **dataSizes,
                         ResType **dataTypes, int16_t *itemCount,
                         const ResType *requestedTypes, int16_t requestCount);

/* Clear and replace entire scrap contents */
OSErr ReplaceScrapContents(const void **dataItems, const int32_t *dataSizes,
                          const ResType *dataTypes, int16_t itemCount);

/*
 * Streaming Operations
 */

/* Begin streaming data to scrap */
OSErr BeginScrapStream(ResType dataType, int32_t estimatedSize,
                      int32_t *streamID);

/* Write data to scrap stream */
OSErr WriteScrapStream(int32_t streamID, const void *data, int32_t size);

/* End streaming data to scrap */
OSErr EndScrapStream(int32_t streamID);

/* Begin streaming data from scrap */
OSErr BeginScrapRead(ResType dataType, int32_t *streamID, int32_t *totalSize);

/* Read data from scrap stream */
OSErr ReadScrapStream(int32_t streamID, void *buffer, int32_t *bufferSize);

/* End streaming data from scrap */
OSErr EndScrapRead(int32_t streamID);

/* Cancel streaming operation */
OSErr CancelScrapStream(int32_t streamID);

/*
 * Asynchronous Operations
 */

/* Asynchronously copy data to scrap */
OSErr AsyncCopyToScrap(const void *data, int32_t size, ResType dataType,
                      ScrapIOCallback completion, void *userData);

/* Asynchronously paste data from scrap */
OSErr AsyncPasteFromScrap(ResType dataType, ScrapIOCallback completion,
                         void *userData);

/* Check status of asynchronous operation */
OSErr CheckAsyncOperation(int32_t operationID, Boolean *isComplete,
                         OSErr *result);

/* Cancel asynchronous operation */
OSErr CancelAsyncOperation(int32_t operationID);

/*
 * Scrap Content Analysis
 */

/* Get summary of scrap contents */
OSErr GetScrapSummary(int16_t *formatCount, int32_t *totalSize,
                     ResType *primaryFormat);

/* Check if scrap contains specific content type */
Boolean ScrapContainsText(void);
Boolean ScrapContainsImages(void);
Boolean ScrapContainsFiles(void);
Boolean ScrapContainsSound(void);

/* Get best available format for purpose */
ResType GetBestTextFormat(void);
ResType GetBestImageFormat(void);
ResType GetBestSoundFormat(void);

/* Estimate data transfer time */
OSErr EstimateTransferTime(ResType dataType, int32_t *milliseconds);

/*
 * Scrap History and Undo
 */

/* Save current scrap state for undo */
OSErr SaveScrapState(int32_t *stateID);

/* Restore scrap from saved state */
OSErr RestoreScrapState(int32_t stateID);

/* Clear saved scrap state */
OSErr ClearScrapState(int32_t stateID);

/* Get scrap history count */
int16_t GetScrapHistoryCount(void);

/* Clear all scrap history */
void ClearScrapHistory(void);

/*
 * Content Filtering and Validation
 */

/* Set content filter for incoming data */
OSErr SetScrapContentFilter(ResType dataType, Boolean (*filterFunc)(const void*, int32_t));

/* Remove content filter */
OSErr RemoveScrapContentFilter(ResType dataType);

/* Validate scrap content before paste */
OSErr ValidateScrapContent(ResType dataType, Boolean *isValid, Str255 reason);

/* Sanitize scrap content */
OSErr SanitizeScrapContent(ResType dataType, Handle *sanitizedData);

/*
 * Scrap Locking and Sharing
 */

/* Lock scrap for exclusive access */
OSErr LockScrap(int32_t timeout, int32_t *lockID);

/* Unlock scrap */
OSErr UnlockScrap(int32_t lockID);

/* Check if scrap is locked */
Boolean IsScrapLocked(void);

/* Share scrap with specific applications */
OSErr ShareScrapWith(const ProcessSerialNumber *targetApps, int16_t appCount);

/* Restrict scrap access */
OSErr RestrictScrapAccess(const ProcessSerialNumber *allowedApps, int16_t appCount);

/* Clear access restrictions */
OSErr ClearScrapRestrictions(void);

/*
 * Performance and Monitoring
 */

/* Monitor scrap performance */
OSErr EnableScrapMonitoring(Boolean enable);

/* Get performance statistics */
OSErr GetScrapPerformanceStats(uint32_t *copyCount, uint32_t *pasteCount,
                              uint32_t *avgCopyTime, uint32_t *avgPasteTime);

/* Reset performance counters */
void ResetScrapPerformanceStats(void);

/* Set performance options */
OSErr SetScrapPerformanceOptions(Boolean enableCaching, Boolean enableCompression,
                                Boolean enablePrefetch);

/*
 * Error Handling and Recovery
 */

/* Set error handler for operations */
OSErr SetScrapErrorHandler(OSErr (*errorHandler)(OSErr, const char*, void*),
                          void *userData);

/* Get detailed error information */
OSErr GetScrapErrorDetails(OSErr errorCode, Str255 description,
                          Str255 suggestion);

/* Attempt to recover from error */
OSErr RecoverFromScrapError(OSErr errorCode);

/* Check scrap integrity */
OSErr CheckScrapIntegrity(Boolean *isValid, int32_t *errorCount);

/* Repair scrap data if possible */
OSErr RepairScrapData(void);

/*
 * Debugging and Diagnostics
 */

/* Dump scrap contents to debug log */
void DumpScrapContents(void);

/* Trace scrap operations */
OSErr EnableScrapTracing(Boolean enable, const char *logFile);

/* Get memory usage details */
OSErr GetScrapMemoryDetails(int32_t *heapUsed, int32_t *tempUsed,
                           int32_t *diskUsed, int32_t *peakUsage);

/* Force garbage collection */
OSErr ForceScrapGC(void);

/*
 * Legacy Support Functions
 */

/* Get scrap in legacy format */
OSErr GetLegacyScrap(Handle *data, ResType dataType);

/* Put scrap in legacy format */
OSErr PutLegacyScrap(Handle data, ResType dataType);

/* Convert legacy scrap to modern format */
OSErr ConvertLegacyScrap(void);

/* Check if legacy format conversion is needed */
Boolean NeedsLegacyConversion(void);

/*
 * Operation Flags
 */
#define SCRAP_OP_SYNC           0x0001    /* Synchronous operation */
#define SCRAP_OP_ASYNC          0x0002    /* Asynchronous operation */
#define SCRAP_OP_NO_CONVERT     0x0004    /* Disable format conversion */
#define SCRAP_OP_FORCE_CONVERT  0x0008    /* Force format conversion */
#define SCRAP_OP_COMPRESS       0x0010    /* Compress data */
#define SCRAP_OP_ENCRYPT        0x0020    /* Encrypt sensitive data */
#define SCRAP_OP_VALIDATE       0x0040    /* Validate data integrity */
#define SCRAP_OP_NO_CACHE       0x0080    /* Disable caching */

/* Copy/Paste operation priorities */
#define SCRAP_PRIORITY_LOW      0
#define SCRAP_PRIORITY_NORMAL   1
#define SCRAP_PRIORITY_HIGH     2
#define SCRAP_PRIORITY_CRITICAL 3

/* Stream buffer sizes */
#define SCRAP_STREAM_BUFFER_SMALL   1024
#define SCRAP_STREAM_BUFFER_MEDIUM  8192
#define SCRAP_STREAM_BUFFER_LARGE   32768

#ifdef __cplusplus
}
#endif

#endif /* SCRAP_OPERATIONS_H */