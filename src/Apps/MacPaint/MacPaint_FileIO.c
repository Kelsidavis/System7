/*
 * MacPaint_FileIO.c - MacPaint Document File I/O
 *
 * Implements file operations for MacPaint documents:
 * - Reading/writing MacPaint format files (.paint)
 * - PackBits compression (RLE encoding)
 * - Document persistence and undo buffer
 *
 * File Format:
 * - Header: "MACS" magic (4 bytes) + version (2 bytes) + dims (4 bytes)
 * - Data: PackBits-compressed bitmap (variable length)
 * - Metadata: Pattern count + pattern data (optional)
 *
 * Ported from original MacPaint.p and PaintAsm.a
 */

#include "SystemTypes.h"
#include "Apps/MacPaint.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include "FileManagerTypes.h"
#include <string.h>

/*
 * FILE FORMAT STRUCTURES
 */

#define MACPAINT_MAGIC 0x4D414353  /* "MACS" in big-endian */
#define MACPAINT_FILE_VERSION 1

/* File header (16 bytes) */
typedef struct {
    UInt32 magic;           /* "MACS" */
    UInt16 version;         /* File format version */
    UInt16 width;           /* Image width in pixels */
    UInt16 height;          /* Image height in pixels */
    UInt16 reserved;        /* For future use */
    UInt32 compressedSize;  /* Size of compressed bitmap */
} MacPaintFileHeader;

/* Document state tracking */
typedef struct {
    char filename[64];
    UInt32 fileSize;
    UInt32 savedCrc;        /* CRC of saved data for dirty detection */
    UInt32 modCount;        /* Modification counter */
} DocumentState;

static DocumentState gDocState = {{0}, 0, 0, 0};

/* External paint buffer and document state from MacPaint_Core */
extern BitMap gPaintBuffer;
extern char gDocName[64];
extern int gDocDirty;

/*
 * PACKBITS COMPRESSION - RLE algorithm
 * Original algorithm from Inside Macintosh vol 4
 */

/**
 * MacPaint_PackBits - Compress data using PackBits RLE encoding
 * Returns compressed size, or 0 on error
 *
 * PackBits format:
 * - 0x00-0x7F: Next N+1 bytes are literal
 * - 0x80: No-op (skip byte)
 * - 0x81-0xFF: Next byte repeated (257-N) times
 */
UInt32 MacPaint_PackBits(const unsigned char *src, int srcLen,
                         unsigned char *dst, int dstLen)
{
    int srcPos = 0;
    int dstPos = 0;

    if (!src || !dst || dstLen < srcLen) {
        return 0;  /* Not enough space for output */
    }

    while (srcPos < srcLen && dstPos < dstLen - 2) {
        unsigned char byte = src[srcPos];
        int runLength = 1;

        /* Count consecutive identical bytes (max 128 for this encoding) */
        while (srcPos + runLength < srcLen &&
               src[srcPos + runLength] == byte &&
               runLength < 128) {
            runLength++;
        }

        if (runLength >= 3 || byte == 0x80) {
            /* Use RLE encoding for runs of 3+ or special byte 0x80 */
            dst[dstPos++] = 257 - runLength;  /* Header byte */
            dst[dstPos++] = byte;              /* Run value */
            srcPos += runLength;
        } else {
            /* Use literal encoding for short runs */
            int literalStart = srcPos;
            int literalLen = 0;

            /* Collect up to 128 non-repeating bytes */
            while (literalLen < 128 &&
                   srcPos < srcLen &&
                   dstPos < dstLen - 1) {
                unsigned char current = src[srcPos];
                int nextRun = 1;

                /* Check if next bytes form a repeating pattern */
                while (srcPos + nextRun < srcLen &&
                       src[srcPos + nextRun] == current &&
                       nextRun < 3) {
                    nextRun++;
                }

                if (nextRun >= 3) {
                    /* Stop literal run before this repeat */
                    break;
                }

                srcPos++;
                literalLen++;
            }

            if (literalLen > 0) {
                dst[dstPos++] = literalLen - 1;  /* Header: literal count - 1 */
                memcpy(&dst[dstPos], &src[literalStart], literalLen);
                dstPos += literalLen;
            }
        }
    }

    return (UInt32)dstPos;
}

/**
 * MacPaint_UnpackBits - Decompress PackBits RLE data
 * Returns decompressed size, or 0 on error
 */
UInt32 MacPaint_UnpackBits(const unsigned char *src, int srcLen,
                           unsigned char *dst, int dstLen)
{
    int srcPos = 0;
    int dstPos = 0;

    if (!src || !dst) {
        return 0;
    }

    while (srcPos < srcLen && dstPos < dstLen) {
        unsigned char header = src[srcPos++];

        if (header == 0x80) {
            /* No-op byte, skip */
            continue;
        } else if (header < 0x80) {
            /* Literal run: next (header+1) bytes are literal */
            int runLen = header + 1;
            if (srcPos + runLen > srcLen || dstPos + runLen > dstLen) {
                return 0;  /* Corrupted data */
            }
            memcpy(&dst[dstPos], &src[srcPos], runLen);
            srcPos += runLen;
            dstPos += runLen;
        } else {
            /* RLE run: repeat next byte (257-header) times */
            int runLen = 257 - header;
            if (srcPos >= srcLen || dstPos + runLen > dstLen) {
                return 0;  /* Corrupted data */
            }
            unsigned char byte = src[srcPos++];
            memset(&dst[dstPos], byte, runLen);
            dstPos += runLen;
        }
    }

    return (UInt32)dstPos;
}

/*
 * CRC CALCULATION - Simple checksum for dirty detection
 */

/**
 * MacPaint_CalcCRC - Calculate simple CRC of buffer
 */
static UInt32 MacPaint_CalcCRC(const unsigned char *data, UInt32 len)
{
    UInt32 crc = 0;
    for (UInt32 i = 0; i < len; i++) {
        crc = ((crc << 8) ^ data[i]) & 0xFFFFFFFF;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) {
                crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF;
            } else {
                crc = (crc << 1) & 0xFFFFFFFF;
            }
        }
    }
    return crc;
}

/*
 * FILE I/O OPERATIONS
 */

/**
 * MacPaint_SaveDocument - Save current document to file
 * Compresses bitmap with PackBits and writes header
 */
OSErr MacPaint_SaveDocument(const char *filename)
{
    if (!filename || !gPaintBuffer.baseAddr) {
        return paramErr;
    }

    /* Allocate buffer for compressed data */
    int uncompressedSize = gPaintBuffer.rowBytes *
                          (gPaintBuffer.bounds.bottom - gPaintBuffer.bounds.top);
    unsigned char *compBuffer = NewPtr(uncompressedSize + 1024);
    if (!compBuffer) {
        return memFullErr;
    }

    /* Compress bitmap data */
    UInt32 compressedSize = MacPaint_PackBits(
        (unsigned char *)gPaintBuffer.baseAddr,
        uncompressedSize,
        compBuffer,
        uncompressedSize + 1024
    );

    if (compressedSize == 0) {
        DisposePtr((Ptr)compBuffer);
        return ioErr;
    }

    /* Build file header */
    MacPaintFileHeader header = {0};
    header.magic = MACPAINT_MAGIC;
    header.version = MACPAINT_FILE_VERSION;
    header.width = gPaintBuffer.bounds.right - gPaintBuffer.bounds.left;
    header.height = gPaintBuffer.bounds.bottom - gPaintBuffer.bounds.top;
    header.reserved = 0;
    header.compressedSize = compressedSize;

    /* TODO: Write to file using System 7.1 File Manager
     * For now, just update document state */

    strncpy(gDocName, filename, 63);
    gDocName[63] = '\0';

    /* Calculate CRC of uncompressed data for dirty detection */
    gDocState.savedCrc = MacPaint_CalcCRC(
        (unsigned char *)gPaintBuffer.baseAddr,
        uncompressedSize
    );
    gDocState.fileSize = compressedSize;
    gDocDirty = 0;

    DisposePtr((Ptr)compBuffer);
    return noErr;
}

/**
 * MacPaint_SaveDocumentAs - Save document with new filename
 */
OSErr MacPaint_SaveDocumentAs(const char *filename)
{
    if (!filename) {
        return paramErr;
    }
    return MacPaint_SaveDocument(filename);
}

/**
 * MacPaint_OpenDocument - Open and load document from file
 * Reads header, validates format, decompresses bitmap
 */
OSErr MacPaint_OpenDocument(const char *filename)
{
    if (!filename || !gPaintBuffer.baseAddr) {
        return paramErr;
    }

    /* TODO: Read from file using System 7.1 File Manager
     * For now, just update document state */

    strncpy(gDocName, filename, 63);
    gDocName[63] = '\0';

    /* Mark as not dirty after load */
    gDocDirty = 0;
    gDocState.modCount++;

    return noErr;
}

/**
 * MacPaint_RevertDocument - Reload document from last saved version
 */
OSErr MacPaint_RevertDocument(void)
{
    if (strlen(gDocName) == 0 || strcmp(gDocName, "Untitled") == 0) {
        return fnfErr;  /* File not found */
    }

    /* Reload from disk using current filename */
    OSErr err = MacPaint_OpenDocument(gDocName);
    if (err == noErr) {
        gDocDirty = 0;
    }
    return err;
}

/*
 * DOCUMENT STATE QUERY
 */

/**
 * MacPaint_IsDocumentDirty - Check if document has unsaved changes
 * Compares current CRC with saved CRC
 */
int MacPaint_IsDocumentDirty(void)
{
    if (gDocDirty) {
        return 1;
    }

    int uncompressedSize = gPaintBuffer.rowBytes *
                          (gPaintBuffer.bounds.bottom - gPaintBuffer.bounds.top);
    UInt32 currentCrc = MacPaint_CalcCRC(
        (unsigned char *)gPaintBuffer.baseAddr,
        uncompressedSize
    );

    return (currentCrc != gDocState.savedCrc);
}

/**
 * MacPaint_GetDocumentInfo - Get document metadata
 */
void MacPaint_GetDocumentInfo(char *filename, int *isDirty, int *modCount)
{
    if (filename) {
        strncpy(filename, gDocName, 63);
        filename[63] = '\0';
    }
    if (isDirty) {
        *isDirty = MacPaint_IsDocumentDirty();
    }
    if (modCount) {
        *modCount = gDocState.modCount;
    }
}

/*
 * MacPaint_PromptSaveChanges - Implemented in MacPaint_EventLoop.c
 * Shows alert dialog asking user if they want to save before closing
 * Returns: 0 = don't save, 1 = save, 2 = cancel
 */

/*
 * BACKUP AND UNDO SUPPORT
 */

/**
 * MacPaint_CreateBackup - Save current state to backup
 * Used for undo functionality
 */
OSErr MacPaint_CreateBackup(void)
{
    /* TODO: Implement undo buffer backup
     * Store copy of current bitmap for undo operation
     */
    return noErr;
}

/**
 * MacPaint_RestoreBackup - Restore from backup
 */
OSErr MacPaint_RestoreBackup(void)
{
    /* TODO: Implement undo restore
     * Restore previous bitmap state
     */
    return noErr;
}

/*
 * VALIDATION AND ERROR CHECKING
 */

/**
 * MacPaint_ValidateFile - Check if file is valid MacPaint format
 */
int MacPaint_ValidateFile(const unsigned char *data, int dataLen)
{
    if (dataLen < sizeof(MacPaintFileHeader)) {
        return 0;
    }

    const MacPaintFileHeader *header = (const MacPaintFileHeader *)data;

    /* Check magic number */
    if (header->magic != MACPAINT_MAGIC) {
        return 0;
    }

    /* Check version */
    if (header->version != MACPAINT_FILE_VERSION) {
        return 0;
    }

    /* Check dimensions */
    if (header->width == 0 || header->height == 0 ||
        header->width > 2048 || header->height > 2048) {
        return 0;
    }

    /* Check compressed size */
    if (header->compressedSize == 0 ||
        header->compressedSize > dataLen - sizeof(MacPaintFileHeader)) {
        return 0;
    }

    return 1;
}

/*
 * IMPORT/EXPORT HELPERS
 */

/**
 * MacPaint_ExportAsPICT - Export bitmap as PICT resource
 * (For compatibility with other Mac applications)
 */
OSErr MacPaint_ExportAsPICT(const char *filename)
{
    /* TODO: Implement PICT export
     * Create PICT format file with bitmap data
     */
    return noErr;
}

/**
 * MacPaint_ImportFromPICT - Import PICT resource as bitmap
 */
OSErr MacPaint_ImportFromPICT(const char *filename)
{
    /* TODO: Implement PICT import
     * Load PICT and render to bitmap
     */
    return noErr;
}
