/*
 * FontResources.h - Font Resource (FOND/NFNT) Structures
 *
 * System 7.1-compatible font resource parsing
 * Based on Inside Macintosh: Text (1993)
 */

#ifndef FONT_RESOURCES_H
#define FONT_RESOURCES_H

#include "SystemTypes.h"
#include "FontTypes.h"

/* NFNT (Bitmap Font) Resource Structure */
typedef struct NFNTResource {
    /* Header */
    SInt16  fontType;       /* Font type (bitmap = 0x9000) */
    SInt16  firstChar;      /* First character in font */
    SInt16  lastChar;       /* Last character in font */
    SInt16  widMax;         /* Maximum width */
    SInt16  kernMax;        /* Maximum kern */
    SInt16  nDescent;       /* Negative of descent */
    SInt16  fRectWidth;     /* Font rectangle width */
    SInt16  fRectHeight;    /* Font rectangle height */
    UInt16  owTLoc;         /* Offset to offset/width table */
    SInt16  ascent;         /* Ascent */
    SInt16  descent;        /* Descent */
    SInt16  leading;        /* Leading */
    SInt16  rowWords;       /* Words per row of bitmap */

    /* Variable length data follows:
     * - Bitmap data (rowWords * fRectHeight * 2 bytes)
     * - Offset/Width table
     * - Optional: Width table (fractional widths)
     * - Optional: Kerning table
     */
} NFNTResource;

/* Offset/Width Table Entry (OWT) */
typedef struct OWTEntry {
    UInt8   offset;         /* Offset to character (high byte) */
    UInt8   width;          /* Width and offset low bits */
} OWTEntry;

/* FOND (Font Family) Resource Structure */
#pragma pack(push, 2)
typedef struct FONDResource {
    /* Header */
    SInt16  ffFlags;        /* Family flags */
    SInt16  ffFamID;        /* Family ID */
    SInt16  ffFirstChar;    /* First character */
    SInt16  ffLastChar;     /* Last character */
    SInt16  ffAscent;       /* Ascent */
    SInt16  ffDescent;      /* Descent */
    SInt16  ffLeading;      /* Leading */
    SInt16  ffWidMax;       /* Maximum width */
    UInt32  ffWTabOff;      /* Offset to width table */
    UInt32  ffKernOff;      /* Offset to kerning table */
    UInt32  ffStylOff;      /* Offset to style mapping table */

    /* Style properties (9 words) */
    SInt16  ffProperty[9];  /* Extra width for styles */

    /* International info */
    SInt16  ffIntl[2];      /* Reserved for international */

    /* Font association table */
    SInt16  ffVersion;      /* Version number */
    SInt16  ffNumEntries;   /* Decoded count; stored on disk as count minus one */

    /* Variable length data follows:
     * - Font association table entries
     * - Optional: Width table
     * - Optional: Kerning table
     * - Optional: Style mapping table
     */
} FONDResource;
#pragma pack(pop)

/* Font Association Table Entry */
typedef struct FontAssocEntry {
    SInt16  fontSize;       /* Point size */
    SInt16  fontStyle;      /* Style bits */
    SInt16  fontID;         /* Resource ID of NFNT */
} FontAssocEntry;

/* Width Table (for fractional widths) */
typedef struct FractionalWidthTable {
    SInt16  numWidths;      /* Number of width entries */
    /* Variable: Fixed widths[numWidths] */
} FractionalWidthTable;

/* Kerning Table */
typedef struct KernTable {
    SInt16  numKerns;       /* Number of kern pairs */
    /* Variable: KernPair entries[numKerns] */
} KernTable;

/* Style Mapping Table Entry */
typedef struct StyleMapEntry {
    SInt16  fontSize;       /* Point size */
    SInt16  fontStyle;      /* Style to map from */
    SInt16  fontID;         /* NFNT resource ID to use */
} StyleMapEntry;

/* Resource Loading Functions */
OSErr FM_LoadFONDResource(Handle fondHandle, FONDResource **fondOut);
OSErr FM_LoadNFNTResource(Handle nfntHandle, NFNTResource **nfntOut);
OSErr FM_ParseOWTTable(const NFNTResource *nfnt, OWTEntry **owtOut);
OSErr FM_BuildWidthTable(const NFNTResource *nfnt, const OWTEntry *owt, UInt8 **widthsOut);
OSErr FM_ExtractBitmap(const NFNTResource *nfnt, UInt8 **bitmapOut, Size *sizeOut);

/* Font Association Functions */
SInt16 FM_FindBestMatch(const FONDResource *fond, SInt16 size, Style face);
OSErr FM_GetFontAssociation(const FONDResource *fond, SInt16 index, const FontAssocEntry **entryOut);

/* Resource Utilities */
Boolean FM_IsValidFOND(Handle fondHandle);
Boolean FM_IsValidNFNT(Handle nfntHandle);
void FM_DisposeFOND(FONDResource *fond);
void FM_DisposeNFNT(NFNTResource *nfnt);

/* Debug Functions */
void FM_DumpFOND(const FONDResource *fond);
void FM_DumpNFNT(const NFNTResource *nfnt);
void FM_DumpOWT(const OWTEntry *owt, SInt16 firstChar, SInt16 lastChar);

#endif /* FONT_RESOURCES_H */
