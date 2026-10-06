/*
 * FontResourceLoader.c - FOND/NFNT Resource Loading Implementation
 *
 * Parses System 7.1 font resources and builds font strikes
 */

#include "FontManager/FontManager.h"
#include "FontManager/FontResources.h"
#include "FontManager/FontTypes.h"
#include "MemoryMgr/MemoryManager.h"
#include "SystemTypes.h"
#include <string.h>
#include "FontManager/FontLogging.h"

/* Ensure boolean constants */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Helper function */
static int fm_abs(int x) {
    return x < 0 ? -x : x;
}

_Static_assert(sizeof(FONDResource) == 54, "FOND header includes its association count");
_Static_assert(sizeof(FontAssocEntry) == 6, "FOND association entries contain three words");

static UInt16 FontReadBE16(const UInt8* bytes) {
    return ((UInt16)bytes[0] << 8) | bytes[1];
}

static UInt32 FontReadBE32(const UInt8* bytes) {
    return ((UInt32)FontReadBE16(bytes) << 16) | FontReadBE16(bytes + 2);
}

static Boolean ValidateFONDData(const UInt8* bytes, size_t length, short* countOut) {
    if (!bytes || length < sizeof(FONDResource)) return FALSE;
    UInt16 storedCount = FontReadBE16(bytes + 52);
    int count = storedCount == 0xFFFF ? 0 : (int)storedCount + 1;
    if (count > 32767 || (size_t)count > (length - sizeof(FONDResource)) / sizeof(FontAssocEntry)) {
        return FALSE;
    }
    size_t tableEnd = sizeof(FONDResource) + (size_t)count * sizeof(FontAssocEntry);
    for (size_t at = 16; at <= 24; at += 4) {
        UInt32 offset = FontReadBE32(bytes + at);
        if (offset && ((offset & 1) || offset < tableEnd || offset > length - 2)) return FALSE;
    }
    if (countOut) *countOut = (short)count;
    return TRUE;
}

/* Debug logging */
#define FRL_DEBUG 1

#if FRL_DEBUG
#define FRL_LOG(...) FONT_LOG_DEBUG("FRL: " __VA_ARGS__)
#else
#define FRL_LOG(...)
#endif

/* ============================================================================
 * NFNT Resource Loading
 * ============================================================================ */

OSErr FM_LoadNFNTResource(Handle nfntHandle, NFNTResource **nfntOut) {
    if (!nfntHandle || !nfntOut) {
        return paramErr;
    }

    Size handleSize = GetHandleSize(nfntHandle);
    if (handleSize < 0 || (size_t)handleSize < sizeof(NFNTResource)) {
        /* handleSize is Size (long on x86-32): %d would pass 4 bytes to printf. */
        FRL_LOG("NFNT handle too small: %ld bytes\n", (long)handleSize);
        return resNotFound;
    }

    HLock(nfntHandle);
    NFNTResource *nfnt = (NFNTResource*)HandleDataAligned(nfntHandle);
    if (!nfnt) {
        HUnlock(nfntHandle);
        return memPurgedErr;
    }

    /* Validate NFNT header */
    if ((nfnt->fontType & 0xF000) != 0x9000) {
        FRL_LOG("Invalid NFNT type: 0x%04X\n", nfnt->fontType);
        HUnlock(nfntHandle);
        return resNotFound;
    }

    /* Allocate our own copy */
    NFNTResource *copy = (NFNTResource*)__builtin_assume_aligned(
        NewPtr(sizeof(NFNTResource)), _Alignof(NFNTResource));
    if (!copy) {
        HUnlock(nfntHandle);
        return memFullErr;
    }

    /* Copy header */
    *copy = *nfnt;

    FRL_LOG("Loaded NFNT: chars %d-%d, size %dx%d, ascent=%d descent=%d\n",
            copy->firstChar, copy->lastChar,
            copy->fRectWidth, copy->fRectHeight,
            copy->ascent, copy->descent);

    HUnlock(nfntHandle);
    *nfntOut = copy;
    return noErr;
}

/* ============================================================================
 * Offset/Width Table Parsing
 * ============================================================================ */

OSErr FM_ParseOWTTable(const NFNTResource *nfnt, OWTEntry **owtOut) {
    if (!nfnt || !owtOut) {
        return paramErr;
    }

    SInt16 numChars = nfnt->lastChar - nfnt->firstChar + 1;
    if (numChars <= 0) {
        return paramErr;
    }

    /* Calculate where OWT starts in the resource */
    /* After bitmap: rowWords * fRectHeight * 2 bytes */
    Size bitmapSize = (Size)nfnt->rowWords * nfnt->fRectHeight * 2;
    const UInt8 *resourceBase = (const UInt8*)nfnt;
    const UInt8 *owtPtr = resourceBase + sizeof(NFNTResource) + bitmapSize;

    /* Sanity check: resource values should be within reasonable bounds */
    if (numChars > 256 || nfnt->rowWords > 1024 || nfnt->fRectHeight > 256) {
        return paramErr;  /* Unreasonable values from malformed resource */
    }

    /* Allocate OWT array (+1 for the extra entry that defines last char's width) */
    if (numChars < 0 ||
        (size_t)numChars > SIZE_MAX / sizeof(OWTEntry) - 1) {
        return memFullErr;
    }
    Size owtSize = (numChars + 1) * sizeof(OWTEntry);
    OWTEntry *owt = (OWTEntry*)NewPtr(owtSize);
    if (!owt) {
        return memFullErr;
    }

    /* Parse OWT entries */
    for (SInt16 i = 0; i <= numChars; i++) {
        owt[i].offset = owtPtr[i * 2];
        owt[i].width = owtPtr[i * 2 + 1];
    }

    FRL_LOG("Parsed OWT: %ld entries\n", (long)(numChars + 1));

    *owtOut = owt;
    return noErr;
}

/* ============================================================================
 * Width Table Building
 * ============================================================================ */

OSErr FM_BuildWidthTable(const NFNTResource *nfnt, const OWTEntry *owt, UInt8 **widthsOut) {
    if (!nfnt || !owt || !widthsOut) {
        return paramErr;
    }

    SInt16 numChars = nfnt->lastChar - nfnt->firstChar + 1;

    /* Allocate width table */
    UInt8 *widths = (UInt8*)NewPtr(256); /* Full ASCII table */
    if (!widths) {
        return memFullErr;
    }

    /* Clear table */
    for (int i = 0; i < 256; i++) {
        widths[i] = 0;
    }

    /* Fill in widths from OWT */
    for (SInt16 i = 0; i < numChars; i++) {
        UInt8 ch = nfnt->firstChar + i;

        /* Calculate pixel width from OWT entries */
        /* Width is the difference in bit offsets between consecutive chars */
        UInt16 startBit = (owt[i].offset << 8) | (owt[i].width & 0x0F);
        UInt16 endBit = (owt[i + 1].offset << 8) | (owt[i + 1].width & 0x0F);
        UInt8 pixelWidth = (endBit - startBit);

        /* Store in table */
        widths[ch] = pixelWidth;

        FRL_LOG("Char %d: width = %d pixels\n", ch, pixelWidth);
    }

    *widthsOut = widths;
    return noErr;
}

/* ============================================================================
 * Bitmap Extraction
 * ============================================================================ */

OSErr FM_ExtractBitmap(const NFNTResource *nfnt, UInt8 **bitmapOut, Size *sizeOut) {
    if (!nfnt || !bitmapOut || !sizeOut) {
        return paramErr;
    }

    /* Calculate bitmap size */
    Size bitmapSize = nfnt->rowWords * nfnt->fRectHeight * 2;

    /* Allocate buffer */
    UInt8 *bitmap = (UInt8*)NewPtr(bitmapSize);
    if (!bitmap) {
        return memFullErr;
    }

    /* Copy bitmap data (starts right after NFNTResource header) */
    const UInt8 *sourcePtr = (const UInt8*)nfnt + sizeof(NFNTResource);
    memcpy(bitmap, sourcePtr, bitmapSize);

    FRL_LOG("Extracted bitmap: %ld bytes (%d words x %d rows)\n",
            (long)bitmapSize, nfnt->rowWords, nfnt->fRectHeight);

    *bitmapOut = bitmap;
    *sizeOut = bitmapSize;
    return noErr;
}

/* ============================================================================
 * FOND Resource Loading
 * ============================================================================ */

OSErr FM_LoadFONDResource(Handle fondHandle, FONDResource **fondOut) {
    if (!fondHandle || !fondOut) {
        return paramErr;
    }
    *fondOut = NULL;

    Size handleSize = GetHandleSize(fondHandle);
    if (handleSize < 0 || (size_t)handleSize < sizeof(FONDResource)) {
        FRL_LOG("FOND handle too small: %ld bytes\n", (long)handleSize);
        return resNotFound;
    }

    UInt8 state = HGetState(fondHandle);
    HLock(fondHandle);
    const UInt8* bytes = (const UInt8*)*fondHandle;
    if (!bytes) {
        HSetState(fondHandle, state);
        return memPurgedErr;
    }
    short count;
    if (!ValidateFONDData(bytes, (size_t)handleSize, &count)) {
        HSetState(fondHandle, state);
        return resNotFound;
    }

    /* Retain associations and optional tables alongside the decoded header. */
    FONDResource *copy = (FONDResource*)__builtin_assume_aligned(
        NewPtr(handleSize), _Alignof(FONDResource));
    if (!copy) {
        HSetState(fondHandle, state);
        return memFullErr;
    }
    memcpy(copy, bytes, (size_t)handleSize);
    UInt8* decoded = (UInt8*)copy;
    for (size_t at = 0; at < sizeof(FONDResource); at += 2) {
        UInt16 word = FontReadBE16(bytes + at);
        memcpy(decoded + at, &word, sizeof(word));
    }
    copy->ffWTabOff = FontReadBE32(bytes + 16);
    copy->ffKernOff = FontReadBE32(bytes + 20);
    copy->ffStylOff = FontReadBE32(bytes + 24);
    copy->ffNumEntries = count;
    FontAssocEntry* entries = (FontAssocEntry*)__builtin_assume_aligned(
        decoded + sizeof(FONDResource), _Alignof(FontAssocEntry));
    for (short i = 0; i < count; i++) {
        const UInt8* entry = bytes + sizeof(FONDResource) + (size_t)i * sizeof(FontAssocEntry);
        entries[i].fontSize = (SInt16)FontReadBE16(entry);
        entries[i].fontStyle = (SInt16)FontReadBE16(entry + 2);
        entries[i].fontID = (SInt16)FontReadBE16(entry + 4);
    }

    FRL_LOG("Loaded FOND: family=%d, chars %d-%d, %d associations\n",
            copy->ffFamID, copy->ffFirstChar, copy->ffLastChar,
            copy->ffNumEntries);

    HSetState(fondHandle, state);
    *fondOut = copy;
    return noErr;
}

/* ============================================================================
 * Font Association
 * ============================================================================ */

SInt16 FM_FindBestMatch(const FONDResource *fond, SInt16 size, Style face) {
    if (!fond) {
        return -1;
    }

    /* Validate entry count from resource data */
    if (fond->ffNumEntries <= 0) return -1;

    /* Get pointer to font association table */
    const FontAssocEntry *entries = (const FontAssocEntry*)__builtin_assume_aligned(
        (const UInt8*)fond + sizeof(FONDResource), _Alignof(FontAssocEntry));

    SInt16 bestID = -1;
    int bestSizeDiff = 65536;

    /* Search for exact or closest match */
    for (SInt16 i = 0; i < fond->ffNumEntries; i++) {
        const FontAssocEntry *entry = &entries[i];

        /* Check style match */
        if ((entry->fontStyle & face) == face) {
            int sizeDiff = fm_abs((int)entry->fontSize - size);

            /* Exact match? */
            if (sizeDiff == 0) {
                FRL_LOG("Found exact match: size=%d style=0x%02X id=%d\n",
                        entry->fontSize, entry->fontStyle, entry->fontID);
                return entry->fontID;
            }

            /* Better than current best? */
            if (sizeDiff < bestSizeDiff) {
                bestID = entry->fontID;
                bestSizeDiff = sizeDiff;
            }
        }
    }

    if (bestID != -1) {
        FRL_LOG("Found best match: id=%d (size diff=%d)\n", bestID, bestSizeDiff);
    }

    return bestID;
}

OSErr FM_GetFontAssociation(const FONDResource *fond, SInt16 index, const FontAssocEntry **entryOut) {
    if (!fond || !entryOut || index < 0 || index >= fond->ffNumEntries) {
        return paramErr;
    }

    const FontAssocEntry *entries = (const FontAssocEntry*)__builtin_assume_aligned(
        (const UInt8*)fond + sizeof(FONDResource), _Alignof(FontAssocEntry));
    *entryOut = &entries[index];

    return noErr;
}

/* ============================================================================
 * Validation Functions
 * ============================================================================ */

Boolean FM_IsValidFOND(Handle fondHandle) {
    if (!fondHandle) return FALSE;

    Size size = GetHandleSize(fondHandle);
    if (size < 0 || (size_t)size < sizeof(FONDResource)) return FALSE;

    UInt8 state = HGetState(fondHandle);
    HLock(fondHandle);
    Boolean valid = ValidateFONDData((const UInt8*)*fondHandle, (size_t)size, NULL);
    HSetState(fondHandle, state);
    return valid;
}

Boolean FM_IsValidNFNT(Handle nfntHandle) {
    if (!nfntHandle) return FALSE;

    Size size = GetHandleSize(nfntHandle);
    if (size < 0 || (size_t)size < sizeof(NFNTResource)) return FALSE;

    HLock(nfntHandle);
    NFNTResource *nfnt = (NFNTResource*)HandleDataAligned(nfntHandle);
    if (!nfnt) {
        HUnlock(nfntHandle);
        return FALSE;
    }

    /* Check for bitmap font type */
    Boolean valid = ((nfnt->fontType & 0xF000) == 0x9000);

    HUnlock(nfntHandle);
    return valid;
}

/* ============================================================================
 * Cleanup Functions
 * ============================================================================ */

void FM_DisposeFOND(FONDResource *fond) {
    if (fond) {
        DisposePtr((Ptr)fond);
    }
}

void FM_DisposeNFNT(NFNTResource *nfnt) {
    if (nfnt) {
        DisposePtr((Ptr)nfnt);
    }
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

void FM_DumpFOND(const FONDResource *fond) {
    if (!fond) return;

    FRL_LOG("=== FOND Resource ===\n");
    FRL_LOG("  Family ID: %d\n", fond->ffFamID);
    FRL_LOG("  Flags: 0x%04X\n", fond->ffFlags);
    FRL_LOG("  Character range: %d-%d\n", fond->ffFirstChar, fond->ffLastChar);
    FRL_LOG("  Metrics: ascent=%d descent=%d leading=%d widMax=%d\n",
            fond->ffAscent, fond->ffDescent, fond->ffLeading, fond->ffWidMax);
    FRL_LOG("  Associations: %d entries\n", fond->ffNumEntries);

    /* Dump associations */
    const FontAssocEntry *entries = (const FontAssocEntry*)__builtin_assume_aligned(
        (const UInt8*)fond + sizeof(FONDResource), _Alignof(FontAssocEntry));
    for (SInt16 i = 0; i < fond->ffNumEntries; i++) {
        FRL_LOG("    [%d] size=%d style=0x%02X -> NFNT %d\n",
                i, entries[i].fontSize, entries[i].fontStyle, entries[i].fontID);
    }
}

void FM_DumpNFNT(const NFNTResource *nfnt) {
    if (!nfnt) return;

    FRL_LOG("=== NFNT Resource ===\n");
    FRL_LOG("  Type: 0x%04X\n", nfnt->fontType);
    FRL_LOG("  Character range: %d-%d\n", nfnt->firstChar, nfnt->lastChar);
    FRL_LOG("  Bitmap size: %dx%d pixels\n", nfnt->fRectWidth, nfnt->fRectHeight);
    FRL_LOG("  Row words: %d\n", nfnt->rowWords);
    FRL_LOG("  Metrics: ascent=%d descent=%d leading=%d widMax=%d\n",
            nfnt->ascent, nfnt->descent, nfnt->leading, nfnt->widMax);
    FRL_LOG("  OWT offset: %d\n", nfnt->owTLoc);
}

void FM_DumpOWT(const OWTEntry *owt, SInt16 firstChar, SInt16 lastChar) {
    if (!owt) return;

    FRL_LOG("=== Offset/Width Table ===\n");
    SInt16 numChars = lastChar - firstChar + 1;

    for (SInt16 i = 0; i < numChars; i++) {
        UInt16 startBit = (owt[i].offset << 8) | (owt[i].width & 0x0F);
        UInt16 endBit = (owt[i + 1].offset << 8) | (owt[i + 1].width & 0x0F);
        UInt8 width = endBit - startBit;

        FRL_LOG("  Char %3d: offset=0x%02X width=0x%02X -> %d pixels\n",
                firstChar + i, owt[i].offset, owt[i].width, width);
    }
}
