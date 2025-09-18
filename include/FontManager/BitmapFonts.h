/*
 * BitmapFonts.h - Bitmap Font Support APIs
 *
 * Support for classic Mac OS bitmap fonts (FONT and NFNT resources).
 * Handles font loading, character rendering, and bitmap manipulation.
 */

#ifndef BITMAP_FONTS_H
#define BITMAP_FONTS_H

#include "FontTypes.h"
#include <Types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bitmap Font Constants */
enum {
    kFONTResourceType = 'FONT',
    kNFNTResourceType = 'NFNT',
    kFONDResourceType = 'FOND'
};

/* Bitmap Font Flags */
enum {
    kBitmapFontPropFlag = 0x9000,     /* Proportional font */
    kBitmapFontFixedFlag = 0xB000,    /* Fixed-width font */
    kBitmapFontColorFlag = 0x8000     /* Color font */
};

/* Bitmap Font Structures */
typedef struct BitmapFontHeader {
    short fontType;        /* Font type and flags */
    short firstChar;       /* First character code */
    short lastChar;        /* Last character code */
    short widMax;          /* Maximum character width */
    short kernMax;         /* Maximum kern value */
    short nDescent;        /* Descent (negative) */
    short fRectWidth;      /* Font rectangle width */
    short fRectHeight;     /* Font rectangle height */
    short owTLoc;          /* Offset/width table location */
    short ascent;          /* Font ascent */
    short descent;         /* Font descent */
    short leading;         /* Leading */
    short rowWords;        /* Row width in words */
} BitmapFontHeader;

typedef struct BitmapFontData {
    BitmapFontHeader header;    /* Font header */
    Ptr bitmapData;            /* Bitmap image data */
    Ptr offsetWidthTable;      /* Offset/width table */
    long bitmapSize;           /* Size of bitmap data */
    long tableSize;            /* Size of offset/width table */
} BitmapFontData;

typedef struct CharacterInfo {
    short offset;          /* Character bitmap offset */
    short width;           /* Character width */
    short leftKern;        /* Left kern value */
    short rightKern;       /* Right kern value */
    Rect bounds;           /* Character bounding rectangle */
} CharacterInfo;

/* Bitmap Font Loading */
OSErr LoadBitmapFont(short fontID, BitmapFontData **fontData);
OSErr LoadBitmapFontFromResource(Handle fontResource, BitmapFontData **fontData);
OSErr UnloadBitmapFont(BitmapFontData *fontData);

/* Font Resource Parsing */
OSErr ParseFONTResource(Handle resource, BitmapFontData **fontData);
OSErr ParseNFNTResource(Handle resource, BitmapFontData **fontData);
OSErr ValidateBitmapFontResource(Handle resource, short *fontType);

/* Character Information */
OSErr GetCharacterInfo(BitmapFontData *fontData, char character, CharacterInfo *info);
OSErr GetCharacterBitmap(BitmapFontData *fontData, char character,
                         void **bitmap, short *width, short *height);
short GetCharacterWidth(BitmapFontData *fontData, char character);
OSErr GetCharacterKerning(BitmapFontData *fontData, char first, char second, short *kern);

/* Font Metrics */
OSErr GetBitmapFontMetrics(BitmapFontData *fontData, FMetricRec *metrics);
OSErr GetBitmapFontBounds(BitmapFontData *fontData, Rect *bounds);
OSErr GetStringMetrics(BitmapFontData *fontData, ConstStr255Param text,
                       short *width, short *height, Rect *bounds);

/* Bitmap Font Rendering */
OSErr RenderBitmapCharacter(BitmapFontData *fontData, char character,
                           GrafPtr port, Point location, short mode);
OSErr RenderBitmapString(BitmapFontData *fontData, ConstStr255Param text,
                         GrafPtr port, Point location, short mode);
OSErr DrawBitmapText(BitmapFontData *fontData, const void *text, short length,
                     Point location, short mode);

/* Bitmap Manipulation */
OSErr ScaleBitmapFont(BitmapFontData *sourceFont, Point numer, Point denom,
                      BitmapFontData **scaledFont);
OSErr CreateStyledBitmapFont(BitmapFontData *sourceFont, short style,
                            BitmapFontData **styledFont);
OSErr CopyBitmapFontData(BitmapFontData *source, BitmapFontData **copy);

/* Style Generation */
OSErr ApplyBoldStyle(BitmapFontData *font, short boldPixels);
OSErr ApplyItalicStyle(BitmapFontData *font, short italicPixels);
OSErr ApplyUnderlineStyle(BitmapFontData *font, short ulOffset, short ulThick);
OSErr ApplyOutlineStyle(BitmapFontData *font);
OSErr ApplyShadowStyle(BitmapFontData *font, short shadowPixels);

/* Font Conversion */
OSErr ConvertBitmapToOutline(BitmapFontData *bitmapFont, void **outlineData, long *size);
OSErr ConvertBitmapToDifferentSize(BitmapFontData *sourceFont, short newSize,
                                   BitmapFontData **newFont);

/* Color Font Support */
OSErr LoadColorBitmapFont(short fontID, BitmapFontData **fontData);
OSErr RenderColorCharacter(BitmapFontData *fontData, char character,
                          GrafPtr port, Point location, RGBColor *foreColor, RGBColor *backColor);
Boolean IsColorBitmapFont(BitmapFontData *fontData);

/* Font Analysis */
OSErr AnalyzeBitmapFont(BitmapFontData *fontData, short *charCount,
                        short *minWidth, short *maxWidth, short *avgWidth);
OSErr GetBitmapFontStatistics(BitmapFontData *fontData, long *totalPixels,
                             short *whitePixels, short *blackPixels);
Boolean IsPropspaced(BitmapFontData *fontData);
Boolean IsFixedWidth(BitmapFontData *fontData);

/* Font Validation and Repair */
OSErr ValidateBitmapFontData(BitmapFontData *fontData);
OSErr RepairBitmapFont(BitmapFontData *fontData);
OSErr CheckBitmapFontIntegrity(BitmapFontData *fontData, Boolean *isValid,
                              char **errorMessage);

/* Low-level Bitmap Operations */
OSErr ExtractCharacterBitmap(BitmapFontData *fontData, char character,
                            void **bitmap, short *rowBytes, Rect *bounds);
OSErr SetCharacterBitmap(BitmapFontData *fontData, char character,
                         const void *bitmap, short rowBytes, Rect bounds);
OSErr CopyCharacterBitmap(BitmapFontData *sourceFont, char sourceChar,
                          BitmapFontData *destFont, char destChar);

/* Font Resource Creation */
OSErr CreateFONTResource(BitmapFontData *fontData, Handle *resource);
OSErr CreateNFNTResource(BitmapFontData *fontData, Handle *resource);
OSErr CreateFONDResource(const BitmapFontData *fonts[], short fontCount,
                         ConstStr255Param familyName, Handle *resource);

/* Memory Management */
OSErr AllocateBitmapFontData(BitmapFontData **fontData);
OSErr DisposeBitmapFontData(BitmapFontData *fontData);
OSErr CompactBitmapFont(BitmapFontData *fontData);

/* Utility Functions */
OSErr GetOffsetWidthEntry(BitmapFontData *fontData, char character,
                         short *offset, short *width);
OSErr SetOffsetWidthEntry(BitmapFontData *fontData, char character,
                         short offset, short width);
short CalculateBitmapRowBytes(short width);
OSErr CalculateFontRectangle(BitmapFontData *fontData, Rect *fontRect);

/* Font Comparison */
Boolean CompareBitmapFonts(BitmapFontData *font1, BitmapFontData *font2);
OSErr GetBitmapFontDifferences(BitmapFontData *font1, BitmapFontData *font2,
                              char **differentChars, short *count);

#ifdef __cplusplus
}
#endif

#endif /* BITMAP_FONTS_H */