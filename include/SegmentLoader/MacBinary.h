/*
 * MacBinary.h - MacBinary I/II/III archive unpacker
 *
 * A MacBinary file packs a classic Mac file - both forks and its Finder
 * info - into one flat byte stream, so it survives a trip through a
 * fork-less file system or a download:
 *
 *   0       128-byte header (name, type, creator, fork lengths, ...)
 *   128     secondary header, if any, padded to a multiple of 128
 *   ...     data fork, padded to a multiple of 128
 *   ...     resource fork, padded to a multiple of 128
 *   ...     Get Info comment (MacBinary II), padded likewise
 *
 * Header offsets (decimal, as the MacBinary II spec gives them):
 *     0  old version number, must be 0
 *     1  file name length (1..63), name at 2..64
 *    65  file type          69  file creator
 *    73  Finder flags (high byte)
 *    74  must be 0
 *    82  must be 0
 *    83  data fork length   87  resource fork length
 *   102  'mBIN' (MacBinary III only)
 *   120  secondary header length
 *   122  version that wrote it (129 = II, 130 = III)
 *   123  minimum version needed to read it (129)
 *   124  CRC-16/XMODEM of bytes 0..123 (II and later)
 *
 * The file has no magic number. MacBinary II and III are recognised by the
 * header CRC; MacBinary I (no CRC) by the bytes that must be zero and
 * fork lengths that fit the file.
 *
 * Pure parsing: no allocation, no file I/O. The caller hands in the whole
 * archive and gets back pointers into that same buffer. Every read is
 * bounds-checked, and multi-byte fields are decoded byte by byte, because
 * they are big-endian whatever the host is.
 */

#ifndef MACBINARY_H
#define MACBINARY_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kMacBinHeaderSize   = 128,
    kMacBinMaxNameLen   = 63,
    kMacBinVersionII    = 129,
    kMacBinVersionIII   = 130,
    /* Finder flags that describe the sender's desktop, not the file:
     * kIsOnDesk and kHasBeenInited (Finder/finder.h, which cannot be
     * included beside FileManager.h). Cleared when a file is unpacked. */
    kMacBinSenderFinderFlags = 0x0101
};

/* The header fields a loader needs; the rest (dates, window position,
 * comment) are Finder transport metadata. */
typedef struct MacBinaryHeader {
    UInt8  fileName[64];        /* 1..64: Pascal string */
    UInt8  fileTypeId[4];       /* 65: file type ('APPL') */
    UInt8  fileCreator[4];      /* 69: creator */
    UInt16 finderFlags;         /* 73 (high byte), 101 (low byte, II+) */
    UInt32 dataForkLength;      /* 83 */
    UInt32 resourceForkLength;  /* 87 */
    UInt8  version;             /* 122: 0 for MacBinary I */
} MacBinaryHeader;

/* The two forks carved out of the archive. The pointers alias the caller's
 * buffer; a fork of length 0 has a NULL pointer. */
typedef struct MacBinaryArchive {
    MacBinaryHeader header;
    const UInt8*    dataFork;
    UInt32          dataForkSize;
    const UInt8*    resourceFork;
    UInt32          resourceForkSize;
} MacBinaryArchive;

/*
 * MacBinary_Parse - validate and split a MacBinary archive.
 *
 * Returns noErr and fills 'out' on success. A buffer that is not MacBinary,
 * whose forks run past its end, or whose resource fork header points
 * outside the fork, returns an error and leaves 'out' untouched.
 */
OSErr MacBinary_Parse(const UInt8* archive, UInt32 size, MacBinaryArchive* out);

/*
 * MacBinary_IsMacBinary - is this a MacBinary header whose forks fit in an
 * archive 'size' bytes long? Reads only the first 128 bytes, so 'archive'
 * may be just the header with 'size' the length of the whole file.
 */
Boolean MacBinary_IsMacBinary(const UInt8* archive, UInt32 size);

/* CRC-16/XMODEM (poly 0x1021, init 0), as MacBinary II header CRCs use. */
UInt16 MacBinary_CRC16(const UInt8* bytes, UInt32 len);

/* --- Files on disk (MacBinaryFile.c) ------------------------------------ */

/* Does this file's data fork hold a MacBinary archive? Reads its header. */
Boolean MacBinary_IsMacBinaryFile(short vRefNum, long dirID, ConstStr255Param name);

/*
 * MacBinary_UnpackFile - write the file a MacBinary archive carries.
 *
 * Creates it beside the archive, under the name in the header (or that name
 * with ".1" .. ".9" if taken), with both forks, type, creator and Finder
 * flags. The archive is left as it was. On success 'result' names the new
 * file; on failure nothing is left behind.
 */
OSErr MacBinary_UnpackFile(short vRefNum, long dirID, ConstStr255Param archiveName,
                           FSSpec* result);

#ifdef __cplusplus
}
#endif

#endif /* MACBINARY_H */
