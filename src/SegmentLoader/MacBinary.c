/*
 * MacBinary.c - MacBinary I/II/III archive unpacker
 *
 * See MacBinary.h for the layout. Sources: the MacBinary II specification
 * (Yoshida et al., 1987) and MacBinary III (Leonard Rosenthol, 1996);
 * the resource fork header is Inside Macintosh: More Macintosh Toolbox 1-122.
 */

#include "SegmentLoader/MacBinary.h"

static UInt32 mb_be32(const UInt8* p)
{
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) |
           ((UInt32)p[2] << 8)  | (UInt32)p[3];
}

static UInt16 mb_be16(const UInt8* p)
{
    return (UInt16)(((UInt16)p[0] << 8) | p[1]);
}

/* Sections after the header start on 128-byte boundaries. 64-bit so a
 * hostile length cannot wrap. */
static UInt64 mb_pad128(UInt64 n)
{
    return (n + 127) & ~(UInt64)127;
}

UInt16 MacBinary_CRC16(const UInt8* bytes, UInt32 len)
{
    UInt16 crc = 0;
    for (UInt32 i = 0; i < len; i++) {
        crc ^= (UInt16)bytes[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (UInt16)((crc << 1) ^ 0x1021) : (UInt16)(crc << 1);
        }
    }
    return crc;
}

/* Works out where the forks sit. Returns false if the header is not
 * MacBinary or the forks do not fit in 'size'. */
static Boolean mb_layout(const UInt8* a, UInt32 size, MacBinaryHeader* hdr,
                         UInt32* dataOff, UInt32* rsrcOff)
{
    if (a == NULL || size < kMacBinHeaderSize) {
        return false;
    }
    /* There is no magic number; these are what every version must have. */
    if (a[0] != 0 || a[74] != 0 || a[1] < 1 || a[1] > kMacBinMaxNameLen) {
        return false;
    }

    /* II and later sign the header with a CRC. Without a match, the spec has
     * a reader fall back to MacBinary I, which also needs byte 82 zero and
     * ignores everything from byte 99 on. */
    Boolean v2 = mb_be16(a + 124) == MacBinary_CRC16(a, 124);
    if (!v2 && a[82] != 0) {
        return false;
    }

    hdr->fileName[0] = a[1];
    for (int i = 0; i < a[1]; i++) {
        hdr->fileName[1 + i] = a[2 + i];
    }
    for (int i = 0; i < 4; i++) {
        hdr->fileTypeId[i]  = a[65 + i];
        hdr->fileCreator[i] = a[69 + i];
    }
    hdr->finderFlags        = (UInt16)((a[73] << 8) | (v2 ? a[101] : 0));
    hdr->dataForkLength     = mb_be32(a + 83);
    hdr->resourceForkLength = mb_be32(a + 87);
    hdr->version            = v2 ? a[122] : 0;

    UInt64 secondary = v2 ? mb_pad128(mb_be16(a + 120)) : 0;
    UInt64 data = kMacBinHeaderSize + secondary;
    UInt64 rsrc = data + mb_pad128(hdr->dataForkLength);
    /* The last fork need not be padded out; some writers stop at its end. */
    if (data + hdr->dataForkLength > size ||
        rsrc + hdr->resourceForkLength > size) {
        return false;
    }
    *dataOff = (UInt32)data;
    *rsrcOff = (UInt32)rsrc;
    return true;
}

Boolean MacBinary_IsMacBinary(const UInt8* archive, UInt32 size)
{
    MacBinaryHeader hdr;
    UInt32 dataOff, rsrcOff;
    return mb_layout(archive, size, &hdr, &dataOff, &rsrcOff);
}

OSErr MacBinary_Parse(const UInt8* archive, UInt32 size, MacBinaryArchive* out)
{
    MacBinaryHeader hdr;
    UInt32 dataOff, rsrcOff;

    if (out == NULL) {
        return paramErr;
    }
    if (!mb_layout(archive, size, &hdr, &dataOff, &rsrcOff)) {
        return paramErr;   /* not MacBinary, or truncated */
    }

    /* A non-empty resource fork must have a header the Resource Manager can
     * use: data and map each inside the fork, and a map at least as long as
     * its own fixed header (28 bytes, IM:MMT 1-123). */
    UInt32 rsrcLen = hdr.resourceForkLength;
    if (rsrcLen > 0) {
        if (rsrcLen < 16) {
            return mapReadErr;
        }
        const UInt8* r = archive + rsrcOff;
        UInt64 rDataOff = mb_be32(r + 0);
        UInt64 rMapOff  = mb_be32(r + 4);
        UInt64 rDataLen = mb_be32(r + 8);
        UInt64 rMapLen  = mb_be32(r + 12);
        if (rDataOff + rDataLen > rsrcLen || rMapOff + rMapLen > rsrcLen ||
            rMapLen < 28) {
            return mapReadErr;
        }
    }

    out->header           = hdr;
    out->dataFork         = hdr.dataForkLength ? archive + dataOff : NULL;
    out->dataForkSize     = hdr.dataForkLength;
    out->resourceFork     = rsrcLen ? archive + rsrcOff : NULL;
    out->resourceForkSize = rsrcLen;
    return noErr;
}
