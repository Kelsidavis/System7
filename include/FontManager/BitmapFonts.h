/*
 * BitmapFonts.h - Bitmap Font Support APIs
 *
 * Support for classic Mac OS bitmap fonts (FONT and NFNT resources).
 * Handles font loading, character rendering, and bitmap manipulation.
 */

#ifndef BITMAP_FONTS_H
#define BITMAP_FONTS_H

#include "SystemTypes.h"

/* Forward declarations */

#include "FontTypes.h"
#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bitmap Font Constants */

/* Bitmap Font Flags */

/* Bitmap Font Structures */

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
