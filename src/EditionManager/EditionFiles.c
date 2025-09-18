/*
 * EditionFiles.c
 *
 * Edition file management for Edition Manager
 * Handles creation, storage, and access to edition container files
 *
 * Edition files store shared data in multiple formats and maintain
 * metadata about publishers and subscribers
 */

#include "EditionManager/EditionManager.h"
#include "EditionManager/EditionManagerPrivate.h"
#include "EditionManager/EditionFiles.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* Edition file format structures */
#define EDITION_FILE_SIGNATURE 0x45444954  /* 'EDIT' */
#define EDITION_FILE_VERSION 0x0001

typedef struct {
    uint32_t signature;         /* File signature 'EDIT' */
    uint16_t version;           /* File format version */
    uint16_t flags;             /* File flags */
    TimeStamp creationDate;     /* File creation date */
    TimeStamp modificationDate; /* Last modification date */
    OSType creatorType;         /* Creator application type */
    OSType fileType;            /* File type */
    uint32_t formatCount;       /* Number of data formats */
    uint32_t metadataOffset;    /* Offset to metadata section */
    uint32_t dataOffset;        /* Offset to data section */
    uint32_t reserved[4];       /* Reserved for future use */
} EditionFileHeader;

typedef struct {
    FormatType formatType;      /* Format type (e.g., 'TEXT', 'PICT') */
    uint32_t dataOffset;        /* Offset to format data */
    uint32_t dataSize;          /* Size of format data */
    uint32_t flags;             /* Format-specific flags */
    TimeStamp creationDate;     /* When this format was added */
    TimeStamp modificationDate; /* When this format was last updated */
    uint32_t reserved[2];       /* Reserved for future use */
} FormatEntry;

typedef struct {
    SectionType publisherType;  /* Publisher section type */
    int32_t publisherID;        /* Publisher section ID */
    OSType publisherApp;        /* Publisher application type */
    Handle publisherAlias;      /* Alias to publisher document */
    TimeStamp lastPublishTime;  /* Last time data was published */
    uint32_t subscriberCount;   /* Number of active subscribers */
    uint32_t reserved[4];       /* Reserved for future use */
} EditionMetadata;

/* Global edition file management state */
static EditionFileBlock** gOpenEditions = NULL;
static int32_t gMaxOpenEditions = 32;
static int32_t gOpenEditionCount = 0;

/* Internal helper functions */
static OSErr AllocateEditionFileBlock(EditionFileBlock** fileBlock);
static OSErr ReadEditionFileHeader(int fd, EditionFileHeader* header);
static OSErr WriteEditionFileHeader(int fd, const EditionFileHeader* header);
static OSErr ReadFormatEntries(int fd, uint32_t count, FormatEntry** entries);
static OSErr WriteFormatEntries(int fd, uint32_t count, const FormatEntry* entries);
static OSErr ReadEditionMetadata(int fd, uint32_t offset, EditionMetadata* metadata);
static OSErr WriteEditionMetadata(int fd, uint32_t offset, const EditionMetadata* metadata);
static OSErr FindFormatEntry(EditionFileBlock* fileBlock, FormatType format, FormatEntry** entry);
static void FreeEditionFileBlock(EditionFileBlock* fileBlock);

/*
 * CreateEditionContainerFile
 *
 * Create a new edition container file.
 */
OSErr CreateEditionContainerFile(const FSSpec* editionFile,
                                OSType fdCreator,
                                ScriptCode editionFileNameScript)
{
    if (!editionFile) {
        return badEditionFileErr;
    }

    /* Create the file */
    int fd = open(editionFile->path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        if (errno == EEXIST) {
            return dupFNErr;  /* File already exists */
        }
        return ioErr;
    }

    /* Initialize file header */
    EditionFileHeader header;
    memset(&header, 0, sizeof(header));
    header.signature = EDITION_FILE_SIGNATURE;
    header.version = EDITION_FILE_VERSION;
    header.flags = 0;
    header.creationDate = GetCurrentTimeStamp();
    header.modificationDate = header.creationDate;
    header.creatorType = fdCreator;
    header.fileType = kUnknownEditionFileType;
    header.formatCount = 0;
    header.metadataOffset = sizeof(EditionFileHeader);
    header.dataOffset = sizeof(EditionFileHeader) + sizeof(EditionMetadata);

    /* Write file header */
    OSErr err = WriteEditionFileHeader(fd, &header);
    if (err != noErr) {
        close(fd);
        unlink(editionFile->path);
        return err;
    }

    /* Initialize metadata */
    EditionMetadata metadata;
    memset(&metadata, 0, sizeof(metadata));
    metadata.publisherType = stPublisher;
    metadata.publisherID = 0;
    metadata.publisherApp = fdCreator;
    metadata.publisherAlias = NULL;
    metadata.lastPublishTime = 0;
    metadata.subscriberCount = 0;

    err = WriteEditionMetadata(fd, header.metadataOffset, &metadata);
    if (err != noErr) {
        close(fd);
        unlink(editionFile->path);
        return err;
    }

    close(fd);
    return noErr;
}

/*
 * DeleteEditionContainerFile
 *
 * Delete an edition container file.
 */
OSErr DeleteEditionContainerFile(const FSSpec* editionFile)
{
    if (!editionFile) {
        return badEditionFileErr;
    }

    if (unlink(editionFile->path) == -1) {
        switch (errno) {
            case ENOENT:
                return fnfErr;  /* File not found */
            case EACCES:
                return permErr; /* Permission denied */
            default:
                return ioErr;
        }
    }

    return noErr;
}

/*
 * OpenEditionFileInternal
 *
 * Open an edition file for reading or writing.
 */
OSErr OpenEditionFileInternal(const FSSpec* fileSpec, bool forWriting, EditionFileBlock** fileBlock)
{
    if (!fileSpec || !fileBlock) {
        return badEditionFileErr;
    }

    /* Allocate edition file block */
    OSErr err = AllocateEditionFileBlock(fileBlock);
    if (err != noErr) {
        return err;
    }

    EditionFileBlock* block = *fileBlock;

    /* Copy file spec */
    block->fileSpec = *fileSpec;
    block->isWritable = forWriting;

    /* Open the file */
    int flags = forWriting ? O_RDWR : O_RDONLY;
    int fd = open(fileSpec->path, flags);
    if (fd == -1) {
        FreeEditionFileBlock(block);
        switch (errno) {
            case ENOENT:
                return fnfErr;
            case EACCES:
                return permErr;
            default:
                return ioErr;
        }
    }

    block->platformHandle = (void*)(intptr_t)fd;

    /* Read and validate file header */
    EditionFileHeader header;
    err = ReadEditionFileHeader(fd, &header);
    if (err != noErr) {
        close(fd);
        FreeEditionFileBlock(block);
        return err;
    }

    if (header.signature != EDITION_FILE_SIGNATURE) {
        close(fd);
        FreeEditionFileBlock(block);
        return notAnEditionContainerErr;
    }

    /* Read format entries */
    if (header.formatCount > 0) {
        err = ReadFormatEntries(fd, header.formatCount,
                               (FormatEntry**)&block->supportedFormats);
        if (err != noErr) {
            close(fd);
            FreeEditionFileBlock(block);
            return err;
        }
        block->formatCount = header.formatCount;
    }

    /* Initialize I/O buffer */
    block->bufferSize = kDefaultIOBufferSize;
    block->ioBuffer = malloc(block->bufferSize);
    if (!block->ioBuffer) {
        close(fd);
        FreeEditionFileBlock(block);
        return editionMgrInitErr;
    }

    block->isOpen = true;
    block->currentMark = 0;
    block->openCount = 1;

    /* Add to global list of open editions */
    if (gOpenEditionCount < gMaxOpenEditions) {
        if (!gOpenEditions) {
            gOpenEditions = (EditionFileBlock**)malloc(sizeof(EditionFileBlock*) * gMaxOpenEditions);
        }
        if (gOpenEditions) {
            gOpenEditions[gOpenEditionCount++] = block;
        }
    }

    return noErr;
}

/*
 * CloseEditionFileInternal
 *
 * Close an edition file.
 */
OSErr CloseEditionFileInternal(EditionFileBlock* fileBlock)
{
    if (!fileBlock || !fileBlock->isOpen) {
        return noErr;
    }

    /* Decrement reference count */
    fileBlock->openCount--;
    if (fileBlock->openCount > 0) {
        return noErr;  /* Still in use */
    }

    /* Close the file */
    if (fileBlock->platformHandle) {
        int fd = (int)(intptr_t)fileBlock->platformHandle;
        close(fd);
        fileBlock->platformHandle = NULL;
    }

    /* Remove from global list */
    for (int32_t i = 0; i < gOpenEditionCount; i++) {
        if (gOpenEditions[i] == fileBlock) {
            /* Move last element to this position */
            gOpenEditions[i] = gOpenEditions[gOpenEditionCount - 1];
            gOpenEditionCount--;
            break;
        }
    }

    /* Free resources */
    FreeEditionFileBlock(fileBlock);

    return noErr;
}

/*
 * WriteDataToEditionFile
 *
 * Write data in a specific format to an edition file.
 */
OSErr WriteDataToEditionFile(EditionFileBlock* fileBlock, FormatType format,
                           const void* data, Size dataSize)
{
    if (!fileBlock || !fileBlock->isOpen || !fileBlock->isWritable || !data) {
        return badEditionFileErr;
    }

    int fd = (int)(intptr_t)fileBlock->platformHandle;

    /* Find or create format entry */
    FormatEntry* formatEntry;
    OSErr err = FindFormatEntry(fileBlock, format, &formatEntry);

    if (err != noErr) {
        /* Create new format entry */
        FormatEntry* newEntries = (FormatEntry*)realloc(fileBlock->supportedFormats,
                                                       sizeof(FormatEntry) * (fileBlock->formatCount + 1));
        if (!newEntries) {
            return editionMgrInitErr;
        }

        fileBlock->supportedFormats = (FormatType*)newEntries;
        formatEntry = &newEntries[fileBlock->formatCount];
        fileBlock->formatCount++;

        /* Initialize new format entry */
        memset(formatEntry, 0, sizeof(FormatEntry));
        formatEntry->formatType = format;
        formatEntry->creationDate = GetCurrentTimeStamp();
    }

    /* Update format entry */
    formatEntry->dataSize = dataSize;
    formatEntry->modificationDate = GetCurrentTimeStamp();

    /* Determine data offset (append to end of file) */
    off_t fileSize = lseek(fd, 0, SEEK_END);
    formatEntry->dataOffset = (uint32_t)fileSize;

    /* Write the data */
    ssize_t bytesWritten = write(fd, data, dataSize);
    if (bytesWritten != dataSize) {
        return ioErr;
    }

    /* Update file header with new format count */
    lseek(fd, 0, SEEK_SET);
    EditionFileHeader header;
    err = ReadEditionFileHeader(fd, &header);
    if (err != noErr) {
        return err;
    }

    header.formatCount = fileBlock->formatCount;
    header.modificationDate = GetCurrentTimeStamp();

    lseek(fd, 0, SEEK_SET);
    err = WriteEditionFileHeader(fd, &header);
    if (err != noErr) {
        return err;
    }

    /* Write updated format entries */
    lseek(fd, header.dataOffset, SEEK_SET);
    err = WriteFormatEntries(fd, fileBlock->formatCount, (FormatEntry*)fileBlock->supportedFormats);

    return err;
}

/*
 * ReadDataFromEditionFile
 *
 * Read data in a specific format from an edition file.
 */
OSErr ReadDataFromEditionFile(EditionFileBlock* fileBlock, FormatType format,
                            void* buffer, Size* bufferSize)
{
    if (!fileBlock || !fileBlock->isOpen || !buffer || !bufferSize) {
        return badEditionFileErr;
    }

    /* Find format entry */
    FormatEntry* formatEntry;
    OSErr err = FindFormatEntry(fileBlock, format, &formatEntry);
    if (err != noErr) {
        return err;
    }

    /* Check buffer size */
    if (*bufferSize < formatEntry->dataSize) {
        *bufferSize = formatEntry->dataSize;
        return buffersTooSmall;
    }

    /* Read the data */
    int fd = (int)(intptr_t)fileBlock->platformHandle;
    lseek(fd, formatEntry->dataOffset, SEEK_SET);

    ssize_t bytesRead = read(fd, buffer, formatEntry->dataSize);
    if (bytesRead != formatEntry->dataSize) {
        return ioErr;
    }

    *bufferSize = formatEntry->dataSize;
    return noErr;
}

/*
 * CheckFormatInEditionFile
 *
 * Check if a format exists in an edition file.
 */
OSErr CheckFormatInEditionFile(EditionFileBlock* fileBlock, FormatType format, Size* size)
{
    if (!fileBlock || !fileBlock->isOpen) {
        return badEditionFileErr;
    }

    FormatEntry* formatEntry;
    OSErr err = FindFormatEntry(fileBlock, format, &formatEntry);
    if (err != noErr) {
        return err;
    }

    if (size) {
        *size = formatEntry->dataSize;
    }

    return noErr;
}

/*
 * GetStandardFormats
 *
 * Get standard formats from an edition container.
 */
OSErr GetStandardFormats(const EditionContainerSpec* container,
                        FormatType* previewFormat,
                        Handle preview,
                        Handle publisherAlias,
                        Handle formats)
{
    if (!container) {
        return badEditionFileErr;
    }

    /* Open the edition file for reading */
    EditionFileBlock* fileBlock;
    OSErr err = OpenEditionFileInternal(&container->theFile, false, &fileBlock);
    if (err != noErr) {
        return err;
    }

    /* Read preview format if requested */
    if (previewFormat && preview) {
        *previewFormat = kPreviewFormat;
        Size previewSize = 0;

        err = CheckFormatInEditionFile(fileBlock, kPreviewFormat, &previewSize);
        if (err == noErr && previewSize > 0) {
            *preview = NewHandleFromData(NULL, previewSize);
            if (*preview) {
                Size actualSize = previewSize;
                ReadDataFromEditionFile(fileBlock, kPreviewFormat, **preview, &actualSize);
            }
        }
    }

    /* Read publisher alias if requested */
    if (publisherAlias) {
        Size aliasSize = 0;
        err = CheckFormatInEditionFile(fileBlock, kPublisherDocAliasFormat, &aliasSize);
        if (err == noErr && aliasSize > 0) {
            *publisherAlias = NewHandleFromData(NULL, aliasSize);
            if (*publisherAlias) {
                Size actualSize = aliasSize;
                ReadDataFromEditionFile(fileBlock, kPublisherDocAliasFormat,
                                      **publisherAlias, &actualSize);
            }
        }
    }

    /* Create format list if requested */
    if (formats) {
        Size formatListSize = fileBlock->formatCount * sizeof(FormatType);
        *formats = NewHandleFromData(fileBlock->supportedFormats, formatListSize);
    }

    CloseEditionFileInternal(fileBlock);
    return noErr;
}

/* Internal helper functions */

static OSErr AllocateEditionFileBlock(EditionFileBlock** fileBlock)
{
    *fileBlock = (EditionFileBlock*)malloc(sizeof(EditionFileBlock));
    if (!*fileBlock) {
        return editionMgrInitErr;
    }

    memset(*fileBlock, 0, sizeof(EditionFileBlock));
    return noErr;
}

static OSErr ReadEditionFileHeader(int fd, EditionFileHeader* header)
{
    if (read(fd, header, sizeof(EditionFileHeader)) != sizeof(EditionFileHeader)) {
        return ioErr;
    }
    return noErr;
}

static OSErr WriteEditionFileHeader(int fd, const EditionFileHeader* header)
{
    if (write(fd, header, sizeof(EditionFileHeader)) != sizeof(EditionFileHeader)) {
        return ioErr;
    }
    return noErr;
}

static OSErr ReadFormatEntries(int fd, uint32_t count, FormatEntry** entries)
{
    if (count == 0) {
        *entries = NULL;
        return noErr;
    }

    size_t entriesSize = sizeof(FormatEntry) * count;
    *entries = (FormatEntry*)malloc(entriesSize);
    if (!*entries) {
        return editionMgrInitErr;
    }

    if (read(fd, *entries, entriesSize) != entriesSize) {
        free(*entries);
        *entries = NULL;
        return ioErr;
    }

    return noErr;
}

static OSErr WriteFormatEntries(int fd, uint32_t count, const FormatEntry* entries)
{
    if (count == 0) {
        return noErr;
    }

    size_t entriesSize = sizeof(FormatEntry) * count;
    if (write(fd, entries, entriesSize) != entriesSize) {
        return ioErr;
    }

    return noErr;
}

static OSErr ReadEditionMetadata(int fd, uint32_t offset, EditionMetadata* metadata)
{
    lseek(fd, offset, SEEK_SET);
    if (read(fd, metadata, sizeof(EditionMetadata)) != sizeof(EditionMetadata)) {
        return ioErr;
    }
    return noErr;
}

static OSErr WriteEditionMetadata(int fd, uint32_t offset, const EditionMetadata* metadata)
{
    lseek(fd, offset, SEEK_SET);
    if (write(fd, metadata, sizeof(EditionMetadata)) != sizeof(EditionMetadata)) {
        return ioErr;
    }
    return noErr;
}

static OSErr FindFormatEntry(EditionFileBlock* fileBlock, FormatType format, FormatEntry** entry)
{
    FormatEntry* entries = (FormatEntry*)fileBlock->supportedFormats;

    for (int32_t i = 0; i < fileBlock->formatCount; i++) {
        if (entries[i].formatType == format) {
            *entry = &entries[i];
            return noErr;
        }
    }

    return badSubPartErr;  /* Format not found */
}

static void FreeEditionFileBlock(EditionFileBlock* fileBlock)
{
    if (fileBlock->supportedFormats) {
        free(fileBlock->supportedFormats);
    }
    if (fileBlock->ioBuffer) {
        free(fileBlock->ioBuffer);
    }
    free(fileBlock);
}