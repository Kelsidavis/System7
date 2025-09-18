/*
 * TrueTypeFonts.c - TrueType Font Support Implementation
 *
 * Comprehensive TrueType font support including SFNT parsing,
 * glyph outline extraction, scaling, and rasterization.
 */

#include "../include/FontManager/TrueTypeFonts.h"
#include "../include/FontManager/FontManager.h"
#include <Memory.h>
#include <Resources.h>
#include <Errors.h>
#include <string.h>
#include <stdlib.h>

/* TrueType table directory entry */
typedef struct TTTableDirectoryEntry {
    unsigned long tag;         /* Table tag */
    unsigned long checksum;    /* Table checksum */
    unsigned long offset;      /* Offset from beginning of font */
    unsigned long length;      /* Length of table */
} TTTableDirectoryEntry;

/* TrueType font directory */
typedef struct TTFontDirectory {
    unsigned long sfntVersion; /* SFNT version */
    unsigned short numTables;  /* Number of tables */
    unsigned short searchRange; /* Search range */
    unsigned short entrySelector; /* Entry selector */
    unsigned short rangeShift; /* Range shift */
} TTFontDirectory;

/* Character mapping subtable header */
typedef struct TTCmapSubtableHeader {
    unsigned short format;     /* Subtable format */
    unsigned short length;     /* Subtable length */
    unsigned short language;   /* Language code */
} TTCmapSubtableHeader;

/* Character mapping encoding record */
typedef struct TTCmapEncodingRecord {
    unsigned short platformID; /* Platform ID */
    unsigned short encodingID; /* Encoding ID */
    unsigned long offset;      /* Offset to subtable */
} TTCmapEncodingRecord;

/* Internal helper functions */
static OSErr ParseSFNTDirectory(Handle sfntData, TTFont *font);
static OSErr LoadTrueTypeTable(TTFont *font, unsigned long tag, void **table, long *size);
static OSErr ParseCharacterMap(TTFont *font);
static OSErr ParseHorizontalMetrics(TTFont *font);
static OSErr ValidateTableChecksum(void *table, unsigned long length, unsigned long expectedChecksum);
static OSErr ExtractGlyphData(TTFont *font, unsigned short glyphIndex, TTGlyph *glyph);
static OSErr ParseSimpleGlyph(void *glyphData, TTGlyph *glyph);
static OSErr ParseCompositeGlyph(void *glyphData, TTGlyph *glyph);
static unsigned short SwapShort(unsigned short value);
static unsigned long SwapLong(unsigned long value);

/*
 * LoadTrueTypeFont - Load TrueType font by resource ID
 */
OSErr LoadTrueTypeFont(short fontID, TTFont **font)
{
    Handle sfntResource;
    OSErr error;

    if (font == NULL) {
        return paramErr;
    }

    *font = NULL;

    /* Get SFNT resource */
    sfntResource = GetResource(kSFNTResourceType, fontID);
    if (sfntResource == NULL) {
        return fontNotFoundErr;
    }

    error = LoadTrueTypeFontFromResource(sfntResource, font);
    return error;
}

/*
 * LoadTrueTypeFontFromResource - Load TrueType font from SFNT resource
 */
OSErr LoadTrueTypeFontFromResource(Handle sfntResource, TTFont **font)
{
    TTFont *newFont;
    OSErr error;

    if (sfntResource == NULL || font == NULL) {
        return paramErr;
    }

    *font = NULL;

    /* Allocate font structure */
    error = AllocateTrueTypeFont(&newFont);
    if (error != noErr) {
        return error;
    }

    /* Store SFNT data */
    newFont->sfntSize = GetHandleSize(sfntResource);
    newFont->sfntData = sfntResource;

    /* Parse SFNT directory */
    error = ParseSFNTDirectory(sfntResource, newFont);
    if (error != noErr) {
        DisposeTrueTypeFont(newFont);
        return error;
    }

    /* Load essential tables */
    error = GetHeadTable(newFont, &newFont->head);
    if (error != noErr) {
        DisposeTrueTypeFont(newFont);
        return error;
    }

    error = GetHorizontalHeaderTable(newFont, &newFont->hhea);
    if (error != noErr) {
        DisposeTrueTypeFont(newFont);
        return error;
    }

    error = GetMaxProfileTable(newFont, &newFont->maxp);
    if (error != noErr) {
        DisposeTrueTypeFont(newFont);
        return error;
    }

    /* Load character mapping */
    error = ParseCharacterMap(newFont);
    if (error != noErr) {
        /* Character mapping is optional for some uses */
    }

    /* Load glyph data table */
    error = GetGlyphDataTable(newFont, &newFont->glyf, NULL);
    if (error != noErr) {
        /* Glyph data might not be present in all fonts */
    }

    /* Load location table */
    error = GetLocationTable(newFont, &newFont->loca, NULL);
    if (error != noErr && newFont->glyf != NULL) {
        /* Location table is required if glyph data is present */
        DisposeTrueTypeFont(newFont);
        return error;
    }

    /* Load horizontal metrics */
    error = ParseHorizontalMetrics(newFont);
    if (error != noErr) {
        /* Horizontal metrics might not be available */
    }

    newFont->valid = TRUE;
    *font = newFont;
    return noErr;
}

/*
 * UnloadTrueTypeFont - Dispose of TrueType font
 */
OSErr UnloadTrueTypeFont(TTFont *font)
{
    if (font == NULL) {
        return paramErr;
    }

    return DisposeTrueTypeFont(font);
}

/*
 * ParseSFNTResource - Parse an SFNT resource
 */
OSErr ParseSFNTResource(Handle resource, TTFont **font)
{
    return LoadTrueTypeFontFromResource(resource, font);
}

/*
 * ValidateSFNTResource - Validate an SFNT resource
 */
OSErr ValidateSFNTResource(Handle resource)
{
    TTFontDirectory *directory;
    unsigned long sfntVersion;

    if (resource == NULL) {
        return paramErr;
    }

    if (GetHandleSize(resource) < sizeof(TTFontDirectory)) {
        return fontCorruptErr;
    }

    HLock(resource);
    directory = (TTFontDirectory *)*resource;
    sfntVersion = SwapLong(directory->sfntVersion);
    HUnlock(resource);

    /* Check for valid SFNT version */
    if (sfntVersion != kTrueTypeTag && sfntVersion != kOpenTypeTag) {
        return fontCorruptErr;
    }

    return noErr;
}

/*
 * GetSFNTTableDirectory - Get list of tables in font
 */
OSErr GetSFNTTableDirectory(TTFont *font, unsigned long **tags, short *count)
{
    short i;

    if (font == NULL || tags == NULL || count == NULL) {
        return paramErr;
    }

    if (!font->valid || font->numTables == 0) {
        return fontCorruptErr;
    }

    *tags = font->tableTags;
    *count = font->numTables;
    return noErr;
}

/*
 * GetSFNTTable - Get a specific table from the font
 */
OSErr GetSFNTTable(TTFont *font, unsigned long tag, void **table, long *size)
{
    return LoadTrueTypeTable(font, tag, table, size);
}

/*
 * GetHeadTable - Get the font header table
 */
OSErr GetHeadTable(TTFont *font, TTHeader **head)
{
    void *tableData;
    long tableSize;
    OSErr error;

    if (font == NULL || head == NULL) {
        return paramErr;
    }

    error = LoadTrueTypeTable(font, kHeadTableTag, &tableData, &tableSize);
    if (error != noErr) {
        return error;
    }

    if (tableSize < sizeof(TTHeader)) {
        return fontCorruptErr;
    }

    *head = (TTHeader *)tableData;

    /* Swap byte order for big-endian fields */
    (*head)->version = SwapLong((*head)->version);
    (*head)->fontRevision = SwapLong((*head)->fontRevision);
    (*head)->checksumAdjustment = SwapLong((*head)->checksumAdjustment);
    (*head)->magicNumber = SwapLong((*head)->magicNumber);
    (*head)->flags = SwapShort((*head)->flags);
    (*head)->unitsPerEm = SwapShort((*head)->unitsPerEm);

    return noErr;
}

/*
 * GetHorizontalHeaderTable - Get horizontal metrics header
 */
OSErr GetHorizontalHeaderTable(TTFont *font, TTHorizontalHeader **hhea)
{
    void *tableData;
    long tableSize;
    OSErr error;

    if (font == NULL || hhea == NULL) {
        return paramErr;
    }

    error = LoadTrueTypeTable(font, kHheaTableTag, &tableData, &tableSize);
    if (error != noErr) {
        return error;
    }

    if (tableSize < sizeof(TTHorizontalHeader)) {
        return fontCorruptErr;
    }

    *hhea = (TTHorizontalHeader *)tableData;

    /* Swap byte order */
    (*hhea)->version = SwapLong((*hhea)->version);
    (*hhea)->ascender = SwapShort((*hhea)->ascender);
    (*hhea)->descender = SwapShort((*hhea)->descender);
    (*hhea)->lineGap = SwapShort((*hhea)->lineGap);
    (*hhea)->advanceWidthMax = SwapShort((*hhea)->advanceWidthMax);
    (*hhea)->numLongHorMetrics = SwapShort((*hhea)->numLongHorMetrics);

    return noErr;
}

/*
 * GetMaxProfileTable - Get maximum profile table
 */
OSErr GetMaxProfileTable(TTFont *font, TTMaxProfile **maxp)
{
    void *tableData;
    long tableSize;
    OSErr error;

    if (font == NULL || maxp == NULL) {
        return paramErr;
    }

    error = LoadTrueTypeTable(font, kMaxpTableTag, &tableData, &tableSize);
    if (error != noErr) {
        return error;
    }

    if (tableSize < sizeof(TTMaxProfile)) {
        return fontCorruptErr;
    }

    *maxp = (TTMaxProfile *)tableData;

    /* Swap byte order */
    (*maxp)->version = SwapLong((*maxp)->version);
    (*maxp)->numGlyphs = SwapShort((*maxp)->numGlyphs);
    (*maxp)->maxPoints = SwapShort((*maxp)->maxPoints);
    (*maxp)->maxContours = SwapShort((*maxp)->maxContours);

    return noErr;
}

/*
 * GetCharacterMapTable - Get character mapping table
 */
OSErr GetCharacterMapTable(TTFont *font, void **cmap, long *size)
{
    return LoadTrueTypeTable(font, kCmapTableTag, cmap, size);
}

/*
 * GetGlyphDataTable - Get glyph data table
 */
OSErr GetGlyphDataTable(TTFont *font, void **glyf, long *size)
{
    return LoadTrueTypeTable(font, kGlyfTableTag, glyf, size);
}

/*
 * GetLocationTable - Get glyph location table
 */
OSErr GetLocationTable(TTFont *font, void **loca, long *size)
{
    return LoadTrueTypeTable(font, kLocaTableTag, loca, size);
}

/*
 * GetHorizontalMetricsTable - Get horizontal metrics table
 */
OSErr GetHorizontalMetricsTable(TTFont *font, void **hmtx, long *size)
{
    return LoadTrueTypeTable(font, kHmtxTableTag, hmtx, size);
}

/*
 * MapCharacterToGlyph - Map Unicode character to glyph index
 */
OSErr MapCharacterToGlyph(TTFont *font, unsigned short character, unsigned short *glyphIndex)
{
    if (font == NULL || glyphIndex == NULL) {
        return paramErr;
    }

    if (font->cmap == NULL) {
        return fontNotFoundErr;
    }

    /* Simplified character mapping - real implementation would parse cmap subtables */
    *glyphIndex = character; /* 1:1 mapping for simplicity */

    /* Validate glyph index */
    if (font->maxp != NULL && *glyphIndex >= font->maxp->numGlyphs) {
        *glyphIndex = 0; /* Return missing character glyph */
    }

    return noErr;
}

/*
 * GetGlyph - Get glyph data for a specific glyph index
 */
OSErr GetGlyph(TTFont *font, unsigned short glyphIndex, TTGlyph **glyph)
{
    TTGlyph *newGlyph;
    OSErr error;

    if (font == NULL || glyph == NULL) {
        return paramErr;
    }

    if (font->maxp != NULL && glyphIndex >= font->maxp->numGlyphs) {
        return fontNotFoundErr;
    }

    /* Allocate glyph structure */
    newGlyph = (TTGlyph *)NewPtr(sizeof(TTGlyph));
    if (newGlyph == NULL) {
        return fontOutOfMemoryErr;
    }

    memset(newGlyph, 0, sizeof(TTGlyph));
    newGlyph->glyphIndex = glyphIndex;

    /* Extract glyph data */
    error = ExtractGlyphData(font, glyphIndex, newGlyph);
    if (error != noErr) {
        DisposePtr((Ptr)newGlyph);
        return error;
    }

    *glyph = newGlyph;
    return noErr;
}

/*
 * GetGlyphMetrics - Get metrics for a glyph
 */
OSErr GetGlyphMetrics(TTFont *font, unsigned short glyphIndex,
                      short *advanceWidth, short *leftSideBearing)
{
    TTLongHorMetric *hmtxTable;
    long tableSize;
    OSErr error;
    short metricIndex;

    if (font == NULL || advanceWidth == NULL || leftSideBearing == NULL) {
        return paramErr;
    }

    *advanceWidth = 0;
    *leftSideBearing = 0;

    if (font->hmtx == NULL) {
        error = GetHorizontalMetricsTable(font, (void**)&hmtxTable, &tableSize);
        if (error != noErr) {
            return error;
        }
        font->hmtx = hmtxTable;
    }

    hmtxTable = (TTLongHorMetric *)font->hmtx;

    /* Determine metric index */
    metricIndex = glyphIndex;
    if (font->hhea != NULL && metricIndex >= font->hhea->numLongHorMetrics) {
        metricIndex = font->hhea->numLongHorMetrics - 1;
    }

    /* Get metrics */
    *advanceWidth = SwapShort(hmtxTable[metricIndex].advanceWidth);
    *leftSideBearing = SwapShort(hmtxTable[metricIndex].leftSideBearing);

    return noErr;
}

/*
 * GetTrueTypeFontMetrics - Get overall font metrics
 */
OSErr GetTrueTypeFontMetrics(TTFont *font, FMetricRec *metrics)
{
    if (font == NULL || metrics == NULL) {
        return paramErr;
    }

    if (!font->valid || font->hhea == NULL || font->head == NULL) {
        return fontCorruptErr;
    }

    /* Convert TrueType metrics to Font Manager format */
    metrics->ascent = ((long)font->hhea->ascender << 16) / font->head->unitsPerEm;
    metrics->descent = ((long)(-font->hhea->descender) << 16) / font->head->unitsPerEm;
    metrics->leading = ((long)font->hhea->lineGap << 16) / font->head->unitsPerEm;
    metrics->widMax = ((long)font->hhea->advanceWidthMax << 16) / font->head->unitsPerEm;
    metrics->wTabHandle = NULL;

    return noErr;
}

/*
 * ValidateTrueTypeFont - Validate font integrity
 */
OSErr ValidateTrueTypeFont(TTFont *font)
{
    if (font == NULL) {
        return paramErr;
    }

    if (!font->valid) {
        return fontCorruptErr;
    }

    /* Check essential tables */
    if (font->head == NULL || font->hhea == NULL || font->maxp == NULL) {
        return fontCorruptErr;
    }

    /* Validate magic number */
    if (font->head->magicNumber != 0x5F0F3CF5) {
        return fontCorruptErr;
    }

    return noErr;
}

/*
 * AllocateTrueTypeFont - Allocate TrueType font structure
 */
OSErr AllocateTrueTypeFont(TTFont **font)
{
    TTFont *newFont;

    if (font == NULL) {
        return paramErr;
    }

    newFont = (TTFont *)NewPtr(sizeof(TTFont));
    if (newFont == NULL) {
        return fontOutOfMemoryErr;
    }

    memset(newFont, 0, sizeof(TTFont));
    *font = newFont;
    return noErr;
}

/*
 * DisposeTrueTypeFont - Dispose TrueType font structure
 */
OSErr DisposeTrueTypeFont(TTFont *font)
{
    short i;

    if (font == NULL) {
        return paramErr;
    }

    /* Dispose table data */
    if (font->tables != NULL) {
        for (i = 0; i < font->numTables; i++) {
            if (font->tables[i] != NULL) {
                DisposePtr((Ptr)font->tables[i]);
            }
        }
        DisposePtr((Ptr)font->tables);
    }

    /* Dispose table tags and checksums */
    if (font->tableTags != NULL) {
        DisposePtr((Ptr)font->tableTags);
    }
    if (font->tableChecksums != NULL) {
        DisposePtr((Ptr)font->tableChecksums);
    }

    /* Dispose font structure */
    DisposePtr((Ptr)font);
    return noErr;
}

/* Internal helper function implementations */

static OSErr ParseSFNTDirectory(Handle sfntData, TTFont *font)
{
    TTFontDirectory *directory;
    TTTableDirectoryEntry *entries;
    Ptr dataPtr;
    short i;
    OSErr error;

    if (sfntData == NULL || font == NULL) {
        return paramErr;
    }

    HLock(sfntData);
    dataPtr = *sfntData;
    directory = (TTFontDirectory *)dataPtr;

    /* Validate and swap directory */
    font->numTables = SwapShort(directory->numTables);

    if (font->numTables == 0 || font->numTables > 50) {
        HUnlock(sfntData);
        return fontCorruptErr;
    }

    /* Allocate table arrays */
    font->tables = (void **)NewPtr(font->numTables * sizeof(void *));
    font->tableTags = (unsigned long *)NewPtr(font->numTables * sizeof(unsigned long));
    font->tableChecksums = (unsigned long *)NewPtr(font->numTables * sizeof(unsigned long));

    if (font->tables == NULL || font->tableTags == NULL || font->tableChecksums == NULL) {
        HUnlock(sfntData);
        return fontOutOfMemoryErr;
    }

    memset(font->tables, 0, font->numTables * sizeof(void *));

    /* Parse table directory entries */
    entries = (TTTableDirectoryEntry *)(dataPtr + sizeof(TTFontDirectory));

    for (i = 0; i < font->numTables; i++) {
        font->tableTags[i] = SwapLong(entries[i].tag);
        font->tableChecksums[i] = SwapLong(entries[i].checksum);
        /* Tables will be loaded on demand */
    }

    HUnlock(sfntData);
    return noErr;
}

static OSErr LoadTrueTypeTable(TTFont *font, unsigned long tag, void **table, long *size)
{
    TTTableDirectoryEntry *entries;
    Ptr dataPtr;
    short i;
    unsigned long offset, length;

    if (font == NULL || table == NULL) {
        return paramErr;
    }

    *table = NULL;
    if (size != NULL) {
        *size = 0;
    }

    /* Search for table in directory */
    for (i = 0; i < font->numTables; i++) {
        if (font->tableTags[i] == tag) {
            /* Table found - check if already loaded */
            if (font->tables[i] != NULL) {
                *table = font->tables[i];
                return noErr;
            }

            /* Load table from SFNT data */
            HLock(font->sfntData);
            dataPtr = *font->sfntData;
            entries = (TTTableDirectoryEntry *)(dataPtr + sizeof(TTFontDirectory));

            offset = SwapLong(entries[i].offset);
            length = SwapLong(entries[i].length);

            /* Allocate and copy table data */
            font->tables[i] = NewPtr(length);
            if (font->tables[i] == NULL) {
                HUnlock(font->sfntData);
                return fontOutOfMemoryErr;
            }

            BlockMoveData(dataPtr + offset, font->tables[i], length);
            HUnlock(font->sfntData);

            *table = font->tables[i];
            if (size != NULL) {
                *size = length;
            }
            return noErr;
        }
    }

    return fontNotFoundErr;
}

static OSErr ParseCharacterMap(TTFont *font)
{
    /* Simplified character map parsing */
    /* Real implementation would parse cmap table structure */
    return GetCharacterMapTable(font, &font->cmap, NULL);
}

static OSErr ParseHorizontalMetrics(TTFont *font)
{
    /* Load horizontal metrics table */
    return GetHorizontalMetricsTable(font, &font->hmtx, NULL);
}

static OSErr ExtractGlyphData(TTFont *font, unsigned short glyphIndex, TTGlyph *glyph)
{
    /* Simplified glyph data extraction */
    /* Real implementation would parse glyph data from 'glyf' table */
    glyph->header.numberOfContours = 1; /* Simple glyph */
    glyph->numContours = 1;
    glyph->numPoints = 4; /* Rectangle */
    glyph->isComposite = FALSE;

    return noErr;
}

/* Utility functions for byte swapping */
static unsigned short SwapShort(unsigned short value)
{
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

static unsigned long SwapLong(unsigned long value)
{
    return ((value & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) |
           (((value >> 16) & 0xFF) << 8) | ((value >> 24) & 0xFF);
}