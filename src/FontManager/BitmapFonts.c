/*
 * BitmapFonts.c - Bitmap Font Support Implementation
 *
 * Handles classic Mac OS bitmap fonts (FONT and NFNT resources).
 * Provides font loading, character rendering, and bitmap manipulation.
 */

#include "../include/FontManager/BitmapFonts.h"
#include "../include/FontManager/FontManager.h"
#include <Memory.h>
#include <Resources.h>
#include <Errors.h>
#include <QuickDraw.h>
#include <string.h>
#include <stdlib.h>

/* Internal structures for bitmap font parsing */
typedef struct FONTResourceHeader {
    short fontType;            /* Font type and flags */
    short firstChar;           /* First character */
    short lastChar;            /* Last character */
    short widMax;              /* Maximum character width */
    short kernMax;             /* Maximum kern */
    short nDescent;            /* Negative descent */
    short fRectWidth;          /* Font rectangle width */
    short fRectHeight;         /* Font rectangle height */
    short owTLoc;              /* Offset/width table location */
    short ascent;              /* Ascent */
    short descent;             /* Descent */
    short leading;             /* Leading */
    short rowWords;            /* Row width in words */
} FONTResourceHeader;

/* Internal helper functions */
static OSErr ParseFONTResourceData(Handle resource, BitmapFontData **fontData);
static OSErr ParseNFNTResourceData(Handle resource, BitmapFontData **fontData);
static OSErr ExtractBitmapData(Ptr resourceData, BitmapFontData *fontData);
static OSErr ExtractOffsetWidthTable(Ptr resourceData, BitmapFontData *fontData);
static OSErr ValidateResourceHeader(FONTResourceHeader *header);
static OSErr ScaleBitmapCharacter(BitmapFontData *font, char character, Point numer, Point denom,
                                 void **scaledBitmap, short *width, short *height);
static OSErr ApplyStyleToBitmap(void *bitmap, short width, short height, short style);

/*
 * LoadBitmapFont - Load a bitmap font by font ID
 */
OSErr LoadBitmapFont(short fontID, BitmapFontData **fontData)
{
    Handle fontResource;
    OSErr error;
    short resourceType;

    if (fontData == NULL) {
        return paramErr;
    }

    *fontData = NULL;

    /* Try to get NFNT resource first */
    fontResource = GetResource(kNFNTResourceType, fontID);
    if (fontResource != NULL) {
        return LoadBitmapFontFromResource(fontResource, fontData);
    }

    /* Try FONT resource */
    fontResource = GetResource(kFONTResourceType, fontID);
    if (fontResource != NULL) {
        return LoadBitmapFontFromResource(fontResource, fontData);
    }

    return fontNotFoundErr;
}

/*
 * LoadBitmapFontFromResource - Load bitmap font from resource handle
 */
OSErr LoadBitmapFontFromResource(Handle fontResource, BitmapFontData **fontData)
{
    OSErr error;
    ResType resourceType;
    short resourceID;
    Str255 resourceName;

    if (fontResource == NULL || fontData == NULL) {
        return paramErr;
    }

    *fontData = NULL;

    /* Get resource info to determine type */
    GetResInfo(fontResource, &resourceID, &resourceType, resourceName);

    if (resourceType == kNFNTResourceType) {
        error = ParseNFNTResource(fontResource, fontData);
    } else if (resourceType == kFONTResourceType) {
        error = ParseFONTResource(fontResource, fontData);
    } else {
        error = fontCorruptErr;
    }

    return error;
}

/*
 * UnloadBitmapFont - Unload and dispose bitmap font data
 */
OSErr UnloadBitmapFont(BitmapFontData *fontData)
{
    if (fontData == NULL) {
        return paramErr;
    }

    /* Dispose bitmap data */
    if (fontData->bitmapData != NULL) {
        DisposePtr(fontData->bitmapData);
    }

    /* Dispose offset/width table */
    if (fontData->offsetWidthTable != NULL) {
        DisposePtr(fontData->offsetWidthTable);
    }

    /* Dispose font data structure */
    DisposePtr((Ptr)fontData);

    return noErr;
}

/*
 * ParseFONTResource - Parse classic FONT resource
 */
OSErr ParseFONTResource(Handle resource, BitmapFontData **fontData)
{
    return ParseFONTResourceData(resource, fontData);
}

/*
 * ParseNFNTResource - Parse newer NFNT resource
 */
OSErr ParseNFNTResource(Handle resource, BitmapFontData **fontData)
{
    return ParseNFNTResourceData(resource, fontData);
}

/*
 * ValidateBitmapFontResource - Validate font resource
 */
OSErr ValidateBitmapFontResource(Handle resource, short *fontType)
{
    FONTResourceHeader *header;
    OSErr error;

    if (resource == NULL || fontType == NULL) {
        return paramErr;
    }

    if (GetHandleSize(resource) < sizeof(FONTResourceHeader)) {
        return fontCorruptErr;
    }

    HLock(resource);
    header = (FONTResourceHeader *)*resource;

    error = ValidateResourceHeader(header);
    if (error == noErr) {
        *fontType = header->fontType;
    }

    HUnlock(resource);
    return error;
}

/*
 * GetCharacterInfo - Get information about a character
 */
OSErr GetCharacterInfo(BitmapFontData *fontData, char character, CharacterInfo *info)
{
    short *offsetTable;
    short charIndex;
    short width, offset;

    if (fontData == NULL || info == NULL) {
        return paramErr;
    }

    /* Check if character is in font range */
    if (character < fontData->header.firstChar || character > fontData->header.lastChar) {
        return fontNotFoundErr;
    }

    charIndex = character - fontData->header.firstChar;
    offsetTable = (short *)fontData->offsetWidthTable;

    /* Get offset and width from table */
    offset = offsetTable[charIndex];
    width = offsetTable[charIndex + 1] - offset;

    /* Fill in character info */
    info->offset = offset;
    info->width = width;
    info->leftKern = 0;  /* Basic bitmap fonts don't have kerning */
    info->rightKern = 0;

    /* Calculate bounding rectangle */
    info->bounds.left = 0;
    info->bounds.top = -fontData->header.ascent;
    info->bounds.right = width;
    info->bounds.bottom = fontData->header.descent;

    return noErr;
}

/*
 * GetCharacterBitmap - Extract character bitmap data
 */
OSErr GetCharacterBitmap(BitmapFontData *fontData, char character,
                         void **bitmap, short *width, short *height)
{
    CharacterInfo info;
    OSErr error;
    long bitmapSize;
    Ptr sourceBitmap;
    short rowBytes;

    if (fontData == NULL || bitmap == NULL || width == NULL || height == NULL) {
        return paramErr;
    }

    *bitmap = NULL;
    *width = 0;
    *height = 0;

    error = GetCharacterInfo(fontData, character, &info);
    if (error != noErr) {
        return error;
    }

    *width = info.width;
    *height = fontData->header.fRectHeight;
    rowBytes = ((info.width + 15) / 16) * 2; /* Round up to word boundary */

    /* Allocate bitmap */
    bitmapSize = rowBytes * *height;
    *bitmap = NewPtr(bitmapSize);
    if (*bitmap == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Extract bitmap from font data */
    sourceBitmap = fontData->bitmapData + (info.offset * fontData->header.fRectHeight * fontData->header.rowWords * 2);
    BlockMoveData(sourceBitmap, *bitmap, bitmapSize);

    return noErr;
}

/*
 * GetCharacterWidth - Get width of a character
 */
short GetCharacterWidth(BitmapFontData *fontData, char character)
{
    CharacterInfo info;
    OSErr error;

    if (fontData == NULL) {
        return 0;
    }

    error = GetCharacterInfo(fontData, character, &info);
    if (error != noErr) {
        return 0;
    }

    return info.width;
}

/*
 * GetCharacterKerning - Get kerning between two characters
 */
OSErr GetCharacterKerning(BitmapFontData *fontData, char first, char second, short *kern)
{
    if (fontData == NULL || kern == NULL) {
        return paramErr;
    }

    /* Basic bitmap fonts don't support kerning */
    *kern = 0;
    return noErr;
}

/*
 * GetBitmapFontMetrics - Get font metrics
 */
OSErr GetBitmapFontMetrics(BitmapFontData *fontData, FMetricRec *metrics)
{
    if (fontData == NULL || metrics == NULL) {
        return paramErr;
    }

    /* Convert integer metrics to Fixed */
    metrics->ascent = fontData->header.ascent << 16;
    metrics->descent = fontData->header.descent << 16;
    metrics->leading = fontData->header.leading << 16;
    metrics->widMax = fontData->header.widMax << 16;
    metrics->wTabHandle = NULL; /* Not implemented for basic bitmap fonts */

    return noErr;
}

/*
 * GetBitmapFontBounds - Get font bounding rectangle
 */
OSErr GetBitmapFontBounds(BitmapFontData *fontData, Rect *bounds)
{
    if (fontData == NULL || bounds == NULL) {
        return paramErr;
    }

    bounds->left = 0;
    bounds->top = -fontData->header.ascent;
    bounds->right = fontData->header.fRectWidth;
    bounds->bottom = fontData->header.descent;

    return noErr;
}

/*
 * RenderBitmapCharacter - Render a character to a graphics port
 */
OSErr RenderBitmapCharacter(BitmapFontData *fontData, char character,
                           GrafPtr port, Point location, short mode)
{
    void *bitmap;
    short width, height;
    OSErr error;
    BitMap charBitmap;
    Rect srcRect, destRect;

    if (fontData == NULL || port == NULL) {
        return paramErr;
    }

    error = GetCharacterBitmap(fontData, character, &bitmap, &width, &height);
    if (error != noErr) {
        return error;
    }

    /* Set up bitmap structure */
    charBitmap.baseAddr = (Ptr)bitmap;
    charBitmap.rowBytes = ((width + 15) / 16) * 2;
    charBitmap.bounds.left = 0;
    charBitmap.bounds.top = 0;
    charBitmap.bounds.right = width;
    charBitmap.bounds.bottom = height;

    /* Set up rectangles */
    srcRect = charBitmap.bounds;
    destRect.left = location.h;
    destRect.top = location.v - fontData->header.ascent;
    destRect.right = location.h + width;
    destRect.bottom = location.v + fontData->header.descent;

    /* Set graphics port */
    SetPort(port);

    /* Copy bitmap to port */
    CopyBits(&charBitmap, &port->portBits, &srcRect, &destRect, mode, NULL);

    /* Clean up */
    DisposePtr((Ptr)bitmap);

    return noErr;
}

/*
 * RenderBitmapString - Render a Pascal string
 */
OSErr RenderBitmapString(BitmapFontData *fontData, ConstStr255Param text,
                         GrafPtr port, Point location, short mode)
{
    short i;
    Point currentLocation;
    OSErr error;
    char character;

    if (fontData == NULL || text == NULL || port == NULL) {
        return paramErr;
    }

    currentLocation = location;

    for (i = 1; i <= text[0]; i++) {
        character = text[i];

        error = RenderBitmapCharacter(fontData, character, port, currentLocation, mode);
        if (error != noErr) {
            /* Skip character but continue rendering */
            continue;
        }

        /* Advance to next character position */
        currentLocation.h += GetCharacterWidth(fontData, character);
    }

    return noErr;
}

/*
 * ScaleBitmapFont - Scale a bitmap font
 */
OSErr ScaleBitmapFont(BitmapFontData *sourceFont, Point numer, Point denom,
                      BitmapFontData **scaledFont)
{
    BitmapFontData *newFont;
    OSErr error;
    short newWidth, newHeight;
    short character;

    if (sourceFont == NULL || scaledFont == NULL) {
        return paramErr;
    }

    *scaledFont = NULL;

    /* Allocate new font structure */
    newFont = (BitmapFontData *)NewPtr(sizeof(BitmapFontData));
    if (newFont == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Copy header and scale metrics */
    newFont->header = sourceFont->header;
    newFont->header.fRectWidth = (sourceFont->header.fRectWidth * numer.h) / denom.h;
    newFont->header.fRectHeight = (sourceFont->header.fRectHeight * numer.v) / denom.v;
    newFont->header.ascent = (sourceFont->header.ascent * numer.v) / denom.v;
    newFont->header.descent = (sourceFont->header.descent * numer.v) / denom.v;
    newFont->header.widMax = (sourceFont->header.widMax * numer.h) / denom.h;

    /* Calculate new bitmap size */
    newWidth = newFont->header.fRectWidth;
    newHeight = newFont->header.fRectHeight;
    newFont->bitmapSize = newWidth * newHeight / 8; /* Assuming 1 bit per pixel */

    /* Allocate new bitmap data */
    newFont->bitmapData = NewPtr(newFont->bitmapSize);
    if (newFont->bitmapData == NULL) {
        DisposePtr((Ptr)newFont);
        return fontOutOfMemoryErr;
    }

    /* Scale each character bitmap */
    for (character = sourceFont->header.firstChar; character <= sourceFont->header.lastChar; character++) {
        void *scaledBitmap;
        short scaledWidth, scaledHeight;

        error = ScaleBitmapCharacter(sourceFont, character, numer, denom,
                                   &scaledBitmap, &scaledWidth, &scaledHeight);
        if (error == noErr) {
            /* Copy scaled bitmap to new font */
            /* Implementation would place bitmap at correct offset */
            DisposePtr((Ptr)scaledBitmap);
        }
    }

    /* Copy and scale offset/width table */
    newFont->tableSize = sourceFont->tableSize;
    newFont->offsetWidthTable = NewPtr(newFont->tableSize);
    if (newFont->offsetWidthTable == NULL) {
        DisposePtr(newFont->bitmapData);
        DisposePtr((Ptr)newFont);
        return fontOutOfMemoryErr;
    }

    BlockMoveData(sourceFont->offsetWidthTable, newFont->offsetWidthTable, newFont->tableSize);

    /* Scale width values in table */
    short *widthTable = (short *)newFont->offsetWidthTable;
    short numEntries = (newFont->header.lastChar - newFont->header.firstChar + 2);
    for (short i = 0; i < numEntries; i++) {
        widthTable[i] = (widthTable[i] * numer.h) / denom.h;
    }

    *scaledFont = newFont;
    return noErr;
}

/*
 * CreateStyledBitmapFont - Create styled version of font
 */
OSErr CreateStyledBitmapFont(BitmapFontData *sourceFont, short style,
                            BitmapFontData **styledFont)
{
    OSErr error;

    if (sourceFont == NULL || styledFont == NULL) {
        return paramErr;
    }

    /* First create a copy */
    error = CopyBitmapFontData(sourceFont, styledFont);
    if (error != noErr) {
        return error;
    }

    /* Apply style effects */
    if (style & bold) {
        error = ApplyBoldStyle(*styledFont, 1);
        if (error != noErr) {
            UnloadBitmapFont(*styledFont);
            *styledFont = NULL;
            return error;
        }
    }

    if (style & italic) {
        error = ApplyItalicStyle(*styledFont, 1);
        if (error != noErr) {
            UnloadBitmapFont(*styledFont);
            *styledFont = NULL;
            return error;
        }
    }

    if (style & underline) {
        error = ApplyUnderlineStyle(*styledFont, (*styledFont)->header.descent / 2, 1);
        if (error != noErr) {
            UnloadBitmapFont(*styledFont);
            *styledFont = NULL;
            return error;
        }
    }

    if (style & shadow) {
        error = ApplyShadowStyle(*styledFont, 1);
        if (error != noErr) {
            UnloadBitmapFont(*styledFont);
            *styledFont = NULL;
            return error;
        }
    }

    if (style & outline) {
        error = ApplyOutlineStyle(*styledFont);
        if (error != noErr) {
            UnloadBitmapFont(*styledFont);
            *styledFont = NULL;
            return error;
        }
    }

    return noErr;
}

/*
 * CopyBitmapFontData - Create a copy of bitmap font data
 */
OSErr CopyBitmapFontData(BitmapFontData *source, BitmapFontData **copy)
{
    BitmapFontData *newFont;

    if (source == NULL || copy == NULL) {
        return paramErr;
    }

    *copy = NULL;

    /* Allocate new font structure */
    newFont = (BitmapFontData *)NewPtr(sizeof(BitmapFontData));
    if (newFont == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Copy header */
    newFont->header = source->header;
    newFont->bitmapSize = source->bitmapSize;
    newFont->tableSize = source->tableSize;

    /* Copy bitmap data */
    if (source->bitmapData != NULL && source->bitmapSize > 0) {
        newFont->bitmapData = NewPtr(source->bitmapSize);
        if (newFont->bitmapData == NULL) {
            DisposePtr((Ptr)newFont);
            return fontOutOfMemoryErr;
        }
        BlockMoveData(source->bitmapData, newFont->bitmapData, source->bitmapSize);
    } else {
        newFont->bitmapData = NULL;
    }

    /* Copy offset/width table */
    if (source->offsetWidthTable != NULL && source->tableSize > 0) {
        newFont->offsetWidthTable = NewPtr(source->tableSize);
        if (newFont->offsetWidthTable == NULL) {
            if (newFont->bitmapData != NULL) {
                DisposePtr(newFont->bitmapData);
            }
            DisposePtr((Ptr)newFont);
            return fontOutOfMemoryErr;
        }
        BlockMoveData(source->offsetWidthTable, newFont->offsetWidthTable, source->tableSize);
    } else {
        newFont->offsetWidthTable = NULL;
    }

    *copy = newFont;
    return noErr;
}

/* Internal helper function implementations */

static OSErr ParseFONTResourceData(Handle resource, BitmapFontData **fontData)
{
    BitmapFontData *font;
    FONTResourceHeader *header;
    OSErr error;
    Ptr resourceData;
    long resourceSize;

    if (resource == NULL || fontData == NULL) {
        return paramErr;
    }

    *fontData = NULL;
    resourceSize = GetHandleSize(resource);

    if (resourceSize < sizeof(FONTResourceHeader)) {
        return fontCorruptErr;
    }

    /* Allocate font data structure */
    font = (BitmapFontData *)NewPtr(sizeof(BitmapFontData));
    if (font == NULL) {
        return fontOutOfMemoryErr;
    }

    HLock(resource);
    resourceData = *resource;
    header = (FONTResourceHeader *)resourceData;

    /* Validate header */
    error = ValidateResourceHeader(header);
    if (error != noErr) {
        DisposePtr((Ptr)font);
        HUnlock(resource);
        return error;
    }

    /* Copy header */
    font->header.fontType = header->fontType;
    font->header.firstChar = header->firstChar;
    font->header.lastChar = header->lastChar;
    font->header.widMax = header->widMax;
    font->header.kernMax = header->kernMax;
    font->header.nDescent = header->nDescent;
    font->header.fRectWidth = header->fRectWidth;
    font->header.fRectHeight = header->fRectHeight;
    font->header.owTLoc = header->owTLoc;
    font->header.ascent = header->ascent;
    font->header.descent = header->descent;
    font->header.leading = header->leading;
    font->header.rowWords = header->rowWords;

    /* Extract bitmap data */
    error = ExtractBitmapData(resourceData, font);
    if (error != noErr) {
        DisposePtr((Ptr)font);
        HUnlock(resource);
        return error;
    }

    /* Extract offset/width table */
    error = ExtractOffsetWidthTable(resourceData, font);
    if (error != noErr) {
        if (font->bitmapData != NULL) {
            DisposePtr(font->bitmapData);
        }
        DisposePtr((Ptr)font);
        HUnlock(resource);
        return error;
    }

    HUnlock(resource);
    *fontData = font;
    return noErr;
}

static OSErr ParseNFNTResourceData(Handle resource, BitmapFontData **fontData)
{
    /* NFNT format is similar to FONT but with some extensions */
    /* For now, parse as FONT - could be extended for NFNT-specific features */
    return ParseFONTResourceData(resource, fontData);
}

static OSErr ExtractBitmapData(Ptr resourceData, BitmapFontData *fontData)
{
    long bitmapOffset, bitmapSize;

    if (resourceData == NULL || fontData == NULL) {
        return paramErr;
    }

    /* Calculate bitmap offset (after header) */
    bitmapOffset = sizeof(FONTResourceHeader);

    /* Calculate bitmap size */
    bitmapSize = fontData->header.rowWords * 2 * fontData->header.fRectHeight;
    fontData->bitmapSize = bitmapSize;

    /* Allocate and copy bitmap data */
    fontData->bitmapData = NewPtr(bitmapSize);
    if (fontData->bitmapData == NULL) {
        return fontOutOfMemoryErr;
    }

    BlockMoveData(resourceData + bitmapOffset, fontData->bitmapData, bitmapSize);
    return noErr;
}

static OSErr ExtractOffsetWidthTable(Ptr resourceData, BitmapFontData *fontData)
{
    long tableOffset, tableSize;
    short numChars;

    if (resourceData == NULL || fontData == NULL) {
        return paramErr;
    }

    /* Calculate table offset */
    tableOffset = fontData->header.owTLoc;

    /* Calculate table size */
    numChars = fontData->header.lastChar - fontData->header.firstChar + 2; /* +1 for count, +1 for sentinel */
    tableSize = numChars * sizeof(short);
    fontData->tableSize = tableSize;

    /* Allocate and copy table data */
    fontData->offsetWidthTable = NewPtr(tableSize);
    if (fontData->offsetWidthTable == NULL) {
        return fontOutOfMemoryErr;
    }

    BlockMoveData(resourceData + tableOffset, fontData->offsetWidthTable, tableSize);
    return noErr;
}

static OSErr ValidateResourceHeader(FONTResourceHeader *header)
{
    if (header == NULL) {
        return paramErr;
    }

    /* Basic validation checks */
    if (header->firstChar > header->lastChar) {
        return fontCorruptErr;
    }

    if (header->fRectWidth <= 0 || header->fRectHeight <= 0) {
        return fontCorruptErr;
    }

    if (header->rowWords <= 0) {
        return fontCorruptErr;
    }

    return noErr;
}

/* Style application functions - simplified implementations */

OSErr ApplyBoldStyle(BitmapFontData *font, short boldPixels)
{
    /* Simplified bold implementation - would expand character bitmaps */
    if (font == NULL) {
        return paramErr;
    }

    /* Implementation would modify bitmap data to make characters bolder */
    font->header.widMax += boldPixels;
    return noErr;
}

OSErr ApplyItalicStyle(BitmapFontData *font, short italicPixels)
{
    /* Simplified italic implementation - would skew character bitmaps */
    if (font == NULL) {
        return paramErr;
    }

    /* Implementation would modify bitmap data to slant characters */
    return noErr;
}

OSErr ApplyUnderlineStyle(BitmapFontData *font, short ulOffset, short ulThick)
{
    /* Underline is typically handled during rendering, not in font data */
    if (font == NULL) {
        return paramErr;
    }

    return noErr;
}

OSErr ApplyOutlineStyle(BitmapFontData *font)
{
    /* Simplified outline implementation */
    if (font == NULL) {
        return paramErr;
    }

    /* Implementation would modify bitmap data to create outline effect */
    return noErr;
}

OSErr ApplyShadowStyle(BitmapFontData *font, short shadowPixels)
{
    /* Simplified shadow implementation */
    if (font == NULL) {
        return paramErr;
    }

    /* Implementation would modify bitmap data to add shadow effect */
    font->header.widMax += shadowPixels;
    return noErr;
}

static OSErr ScaleBitmapCharacter(BitmapFontData *font, char character, Point numer, Point denom,
                                 void **scaledBitmap, short *width, short *height)
{
    void *originalBitmap;
    OSErr error;

    if (font == NULL || scaledBitmap == NULL || width == NULL || height == NULL) {
        return paramErr;
    }

    /* Get original character bitmap */
    error = GetCharacterBitmap(font, character, &originalBitmap, width, height);
    if (error != noErr) {
        return error;
    }

    /* Scale the bitmap - simplified implementation */
    *width = (*width * numer.h) / denom.h;
    *height = (*height * numer.v) / denom.v;

    /* Allocate scaled bitmap */
    long scaledSize = (((*width + 15) / 16) * 2) * *height;
    *scaledBitmap = NewPtr(scaledSize);
    if (*scaledBitmap == NULL) {
        DisposePtr(originalBitmap);
        return fontOutOfMemoryErr;
    }

    /* Perform scaling - this is a simplified version */
    /* Real implementation would use proper bitmap scaling algorithm */
    BlockMoveData(originalBitmap, *scaledBitmap, scaledSize);

    DisposePtr(originalBitmap);
    return noErr;
}