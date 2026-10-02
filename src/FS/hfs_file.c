/* HFS File Operations Implementation */
#include "../../include/FS/hfs_file.h"
#include "../../include/FS/hfs_endian.h"
#include "../../include/FS/hfs_btree.h"
#include "../../include/MemoryMgr/MemoryManager.h"
#include <string.h>
#include "FS/FSLogging.h"

/* Serial debug output */

/* Context for find_file_record callback */
typedef struct {
    FileID target;
    HFS_CatFileRec* result;
    bool found;
} FindContext;

/* Callback for B-tree iteration during file lookup */
static bool find_file_callback(void* keyPtr, uint16_t keyLen,
                               void* dataPtr, uint16_t dataLen,
                               void* context) {
    (void)keyPtr;
    (void)keyLen;
    (void)dataLen;
    FindContext* findCtx = (FindContext*)context;
    uint16_t recordType = be16_read(dataPtr);

    if (recordType == kHFS_FileRecord) {
        HFS_CatFileRec* rec = (HFS_CatFileRec*)dataPtr;
        uint32_t fileID = be32_read(&rec->fileID);

        if (fileID == findCtx->target) {
            memcpy(findCtx->result, rec, sizeof(HFS_CatFileRec));
            findCtx->found = true;
            return false;  /* Stop iteration */
        }
    }

    return true;  /* Continue */
}

/* Find file record in catalog by ID */
static bool find_file_record(HFS_Catalog* cat, FileID id, HFS_CatFileRec* fileRec) {
    /* This is a simplified version - in reality we'd search the B-tree directly */

    /* Build a thread record key to find the file */
    /* For now, do a linear search through catalog */

    FindContext ctx = { .target = id, .result = fileRec, .found = false };

    HFS_BT_IterateLeaves(&cat->bt, find_file_callback, &ctx);
    return ctx.found;
}

/* Read data from file extents */
static bool read_from_extents(HFS_Volume* vol, const HFS_Extent* extents,
                             uint32_t fileSize, uint32_t offset,
                             void* buffer, uint32_t length,
                             uint32_t* bytesRead) {
    if (!vol || !extents || !buffer || !bytesRead) return false;

    *bytesRead = 0;

    /* Limit read to file size */
    if (offset >= fileSize) return true;  /* EOF */
    if (offset + length > fileSize) {
        length = fileSize - offset;
    }

    uint32_t currentOffset = 0;
    uint32_t remaining = length;
    uint8_t* dst = (uint8_t*)buffer;

    /* Read from first 3 extents */
    for (int i = 0; i < 3 && remaining > 0; i++) {
        if (extents[i].blockCount == 0) break;

        uint32_t extentBytes = extents[i].blockCount * vol->alBlkSize;

        /* Skip this extent if we're past it */
        if (offset >= currentOffset + extentBytes) {
            currentOffset += extentBytes;
            continue;
        }

        /* Calculate read position within this extent */
        uint32_t extentOffset = offset - currentOffset;
        uint32_t toRead = extentBytes - extentOffset;
        if (toRead > remaining) toRead = remaining;

        /* Prevent division by zero */
        if (vol->alBlkSize == 0) {
            return false;
        }

        /* Calculate allocation block and offset within it */
        uint32_t startAllocBlock = extents[i].startBlock + (extentOffset / vol->alBlkSize);
        uint32_t allocOffset = extentOffset % vol->alBlkSize;
        uint32_t blocksToRead = (toRead + allocOffset + vol->alBlkSize - 1) / vol->alBlkSize;

        /* Allocate temporary buffer for block-aligned read */
        /* Check for integer overflow in multiplication */
        if (blocksToRead > UINT32_MAX / vol->alBlkSize) {
            return false;  /* Overflow would occur */
        }

        uint8_t* tempBuf = NewPtr(blocksToRead * vol->alBlkSize);
        if (!tempBuf) return false;

        /* Read the allocation blocks */
        if (!HFS_ReadAllocBlocks(vol, startAllocBlock, blocksToRead, tempBuf)) {
            DisposePtr((Ptr)tempBuf);
            return false;
        }

        /* Copy requested portion to destination */
        memcpy(dst, tempBuf + allocOffset, toRead);
        DisposePtr((Ptr)tempBuf);

        dst += toRead;
        offset += toRead;
        remaining -= toRead;
        *bytesRead += toRead;
        currentOffset += extentBytes;
    }

    return true;
}

/*
 * Read data from the extents overflow file.
 *
 * Past a file's first three extents, the rest are in records of three,
 * each keyed by the file allocation block it starts at (Inside Macintosh:
 * Files, 2-83 and 2-89). The first record starts where the first three
 * extents end; each one after starts where the one before ends. Records are
 * followed until one covers the offset wanted.
 *
 * This built a nine-byte key in a layout no HFS volume uses, looked up only
 * the first record, and read from it with the file offset unchanged - as if
 * the overflow extents began at byte 0 of the file.
 */
static bool read_from_overflow_extents(HFS_Volume* vol, FileID fileID,
                                       bool isResource, uint32_t fileSize,
                                       uint32_t offset, void* buffer,
                                       uint32_t length, uint32_t* bytesRead,
                                       uint32_t firstThreeExtentsSize) {
    if (!vol || !buffer || !bytesRead || vol->alBlkSize == 0) return false;

    *bytesRead = 0;

    /* Skip if read is within first 3 extents */
    if (offset < firstThreeExtentsSize) return true;

    HFS_BTree extBTree;
    if (!HFS_BT_Init(&extBTree, vol, kBTreeExtents)) {
        return false;
    }

    bool ok = false;
    uint32_t baseBytes = firstThreeExtentsSize;
    for (int guard = 0; guard < 4096; guard++) {
        uint8_t extKey[8];
        extKey[0] = 7;
        extKey[1] = isResource ? 0xFF : 0x00;
        be32_write(extKey + 2, fileID);
        be16_write(extKey + 6, (uint16_t)(baseBytes / vol->alBlkSize));

        uint8_t record[12];
        uint16_t recordLen = sizeof(record);
        if (!HFS_BT_FindRecord(&extBTree, extKey, sizeof(extKey), record, &recordLen) ||
            recordLen < 12) {
            break;   /* no record for this stretch: the file has no more */
        }

        HFS_Extent extents[3];
        uint32_t spanBytes = 0;
        for (int i = 0; i < 3; i++) {
            extents[i].startBlock = be16_read(record + i * 4);
            extents[i].blockCount = be16_read(record + i * 4 + 2);
            spanBytes += (uint32_t)extents[i].blockCount * vol->alBlkSize;
        }
        if (spanBytes == 0) break;

        if (offset < baseBytes + spanBytes) {
            /* Measured from where these extents begin, not from the file's start. */
            ok = read_from_extents(vol, extents, fileSize - baseBytes,
                                   offset - baseBytes, buffer, length, bytesRead);
            break;
        }
        baseBytes += spanBytes;
    }

    HFS_BT_Close(&extBTree);
    return ok;
}

HFSFile* HFS_FileOpen(HFS_Catalog* cat, FileID id, bool resourceFork) {
    if (!cat || id < HFS_FIRST_CNID) return NULL;

    /* Find the file record */
    HFS_CatFileRec fileRec;
    if (!find_file_record(cat, id, &fileRec)) {
        /* FS_LOG_DEBUG("HFS File: File ID %u not found\n", id); */
        return NULL;
    }

    /* Allocate file handle */
    HFSFile* file = (HFSFile*)NewPtr(sizeof(HFSFile));
    if (!file) return NULL;

    memset(file, 0, sizeof(HFSFile));
    file->vol = cat->vol;
    file->id = id;
    file->isResource = resourceFork;

    /* Copy file info */
    file->dataSize = be32_read(&fileRec.dataLogicalSize);
    file->rsrcSize = be32_read(&fileRec.rsrcLogicalSize);

    /* Copy extents (converting from big-endian) */
    for (int i = 0; i < 3; i++) {
        file->dataExtents[i].startBlock = be16_read(&fileRec.dataExtents[i].startBlock);
        file->dataExtents[i].blockCount = be16_read(&fileRec.dataExtents[i].blockCount);
        file->rsrcExtents[i].startBlock = be16_read(&fileRec.rsrcExtents[i].startBlock);
        file->rsrcExtents[i].blockCount = be16_read(&fileRec.rsrcExtents[i].blockCount);
    }

    file->position = 0;

    /* id and the fork size are 32-bit: %u would pass a 4-byte
     * int where the printf expects a long. */
    FS_LOG_DEBUG("HFS File: Opened file ID %lu (%s fork, %lu bytes)\n",
                  (unsigned long)id, resourceFork ? "rsrc" : "data",
                  (unsigned long)(resourceFork ? file->rsrcSize : file->dataSize));

    return file;
}

HFSFile* HFS_FileOpenByPath(HFS_Catalog* cat, const char* path, bool resourceFork) {
    if (!cat || !path) return NULL;

    /* Get volume info to find root directory */
    extern bool VFS_Lookup(VRefNum vref, DirID dir, const char* name, CatEntry* entry);
    VRefNum vref = (VRefNum)(size_t)cat->vol;  /* Simplified - assumes vol pointer can be cast */
    DirID currentDir = 2;  /* HFS root directory is typically CNID 2 */

    const char* cursor = path;

    /* Skip leading slashes */
    while (*cursor == '/') cursor++;

    if (*cursor == '\0') {
        /* Empty path */
        return NULL;
    }

    /* Parse path components */
    FileID targetID = 0;
    while (*cursor != '\0') {
        /* Skip multiple slashes */
        while (*cursor == '/') cursor++;
        if (*cursor == '\0') break;

        /* Find end of component */
        const char* nextSlash = cursor;
        while (*nextSlash != '\0' && *nextSlash != '/') {
            nextSlash++;
        }

        size_t len = (size_t)(nextSlash - cursor);
        if (len == 0 || len > 31) {
            return NULL;  /* Invalid component */
        }

        /* Extract component name */
        char component[32];
        memcpy(component, cursor, len);
        component[len] = '\0';

        /* Look up component in current directory */
        CatEntry entry;
        if (!VFS_Lookup(vref, currentDir, component, &entry)) {
            FS_LOG_DEBUG("HFS File: Component '%s' not found in path '%s'\n", component, path);
            return NULL;
        }

        /* Check if this is the last component */
        Boolean isLast = (*nextSlash == '\0');

        if (isLast) {
            /* Last component should be a file */
            if (entry.kind != kNodeFile) {
                FS_LOG_DEBUG("HFS File: Path '%s' resolves to directory, not file\n", path);
                return NULL;
            }
            targetID = entry.id;
        } else {
            /* Intermediate component should be a directory */
            if (entry.kind != kNodeDir) {
                FS_LOG_DEBUG("HFS File: Intermediate component '%s' is not a directory\n", component);
                return NULL;
            }
            currentDir = entry.id;
        }

        cursor = nextSlash;
    }

    if (targetID == 0) {
        return NULL;
    }

    /* Open file by ID */
    return HFS_FileOpen(cat, targetID, resourceFork);
}

void HFS_FileClose(HFSFile* file) {
    if (file) {
        DisposePtr((Ptr)file);
    }
}

bool HFS_FileRead(HFSFile* file, void* buffer, uint32_t length, uint32_t* bytesRead) {
    if (!file || !buffer || !bytesRead) return false;

    const HFS_Extent* extents = file->isResource ? file->rsrcExtents : file->dataExtents;
    uint32_t fileSize = file->isResource ? file->rsrcSize : file->dataSize;
    uint32_t initialBytesRead = 0;

    /* First, try to read from the first 3 extents */
    bool result = read_from_extents(file->vol, extents, fileSize,
                                   file->position, buffer, length, &initialBytesRead);

    *bytesRead = initialBytesRead;

    /* If we didn't get all requested bytes and haven't read anything yet,
     * try overflow extents for large files */
    if (result && initialBytesRead < length && initialBytesRead == 0) {
        /* Calculate size covered by first 3 extents */
        uint32_t firstThreeSize = 0;
        for (int i = 0; i < 3; i++) {
            if (extents[i].blockCount == 0) break;
            firstThreeSize += extents[i].blockCount * file->vol->alBlkSize;
        }

        /* Try reading from overflow extents */
        uint32_t overflowBytesRead = 0;
        if (read_from_overflow_extents(file->vol, file->id, file->isResource,
                                      fileSize, file->position, buffer,
                                      length, &overflowBytesRead, firstThreeSize)) {
            *bytesRead += overflowBytesRead;
        }
    }

    if (result && *bytesRead > 0) {
        file->position += *bytesRead;
    }

    return result;
}

bool HFS_FileSeek(HFSFile* file, uint32_t position) {
    if (!file) return false;

    uint32_t fileSize = file->isResource ? file->rsrcSize : file->dataSize;
    if (position > fileSize) position = fileSize;

    file->position = position;
    return true;
}

uint32_t HFS_FileGetSize(HFSFile* file) {
    if (!file) return 0;
    return file->isResource ? file->rsrcSize : file->dataSize;
}

uint32_t HFS_FileTell(HFSFile* file) {
    if (!file) return 0;
    return file->position;
}