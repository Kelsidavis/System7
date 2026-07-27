/*
 * FontManager.h - Main Font Manager API
 *
 * Complete Font Manager API compatible with Mac OS 7.1
 * Supports bitmap fonts, TrueType fonts, and modern font formats.
 */

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include "SystemTypes.h"

/* Forward declarations */

#include "FontTypes.h"
#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Font Manager Initialization */
void InitFonts(void);
OSErr FlushFonts(void);

/* Font Family Management */
void GetFontName(short familyID, Str255 name);
void GetFNum(ConstStr255Param name, short *familyID);
Boolean RealFont(short fontNum, short size);

/* Font Swapping and Metrics */
FMOutPtr FMSwapFont(const FMInput *inRec);
void GetFontMetrics(FMetricRec *theMetrics);  /* Get current font metrics */

/* Font Scaling and Outline Support */
void SetFScaleDisable(Boolean fscaleDisable);
void SetFractEnable(Boolean fractEnable);
Boolean IsOutline(Point numer, Point denom);
void SetOutlinePreferred(Boolean outlinePreferred);
Boolean GetOutlinePreferred(void);
void SetPreserveGlyph(Boolean preserveGlyph);
Boolean GetPreserveGlyph(void);

/* Outline Font Metrics */
OSErr OutlineMetrics(short byteCount, const void *textPtr, Point numer,
                     Point denom, short *yMax, short *yMin, Fixed* awArray,
                     Fixed* lsbArray, Rect* boundsArray);

/* Font Locking */
void SetFontLock(Boolean lockFlag);

/* System Font Access */
short GetDefFontSize(void);
short GetSysFont(void);
short GetAppFont(void);

/* C-style Font Name Functions */
void getfnum(char *theName, short *familyID);
void getfontname(short familyID, char *theName);

/* Extended Font Manager Functions */

/* Font Cache Management */
OSErr InitFontCache(short maxEntries, unsigned long maxSize);
OSErr FlushFontCache(void);
OSErr PurgeFontCache(short familyID);
OSErr GetFontCacheStats(short *entries, unsigned long *size);

/* Font Format Detection */
short GetFontFormat(short familyID, short size);
Boolean IsBitmapFont(short familyID, short size);
Boolean IsTrueTypeFont(short familyID, short size);
Boolean IsOpenTypeFont(short familyID, short size);
Boolean IsWOFFFont(short familyID, short size);
Boolean IsWOFF2Font(short familyID, short size);
Boolean IsPostScriptFont(short familyID, short size);
Boolean IsSystemFont(short familyID, short size);

/* Font Substitution */
OSErr SetFontSubstitution(short originalID, short substituteID);
OSErr GetFontSubstitution(short originalID, short *substituteID);
OSErr RemoveFontSubstitution(short originalID);
OSErr ClearFontSubstitutions(void);

/* Font Loading and Validation */
OSErr LoadFontFamily(short familyID);
OSErr UnloadFontFamily(short familyID);
OSErr ValidateFontResource(Handle fontResource);
OSErr GetFontResourceInfo(short familyID, short size, short *resourceID, short *resourceType);

/* Advanced Font Metrics */
OSErr GetFontBoundingBox(short familyID, short size, short style, Rect *bbox);
OSErr GetCharacterBounds(short familyID, short size, short style, char character, Rect *bounds);
Fixed GetCharacterWidth(short familyID, short size, short style, char character);
OSErr GetKerningPairs(short familyID, short size, short style, KernPair **pairs, short *count);

/* Font Rendering Support */
OSErr PrepareFontForRendering(short familyID, short size, short style, Point dpi);
OSErr RenderGlyph(short familyID, short size, short style, char character,
                  GrafPtr port, Point location, short mode);
OSErr GetGlyphOutline(short familyID, short size, short style, char character,
                      void **outline, long *outlineSize);

/* Unicode and International Support */
OSErr GetFontCharacterSet(short familyID, short *charset);
OSErr MapCharacterToGlyph(short familyID, short unicodeChar, short *glyph);
OSErr GetFontLanguageSupport(short familyID, short **languages, short *count);

/* Font Installation and Management */
OSErr InstallFontFile(ConstStr255Param filePath, short *familyID);
OSErr InstallOpenTypeFont(ConstStr255Param filePath, short *familyID);
OSErr InstallWOFFFont(ConstStr255Param filePath, short *familyID);
OSErr InstallWOFF2Font(ConstStr255Param filePath, short *familyID);
OSErr InstallSystemFont(ConstStr255Param fontName, short *familyID);
OSErr RemoveFontFile(short familyID);
OSErr GetInstalledFonts(short **familyIDs, short *count);
OSErr RefreshFontList(void);
OSErr ScanModernFontDirectories(void);

/* Modern Platform Font Support */
OSErr InitializePlatformFonts(void);
OSErr RegisterSystemFontDirectory(ConstStr255Param directoryPath);
OSErr ScanForSystemFonts(void);
OSErr LoadPlatformFont(ConstStr255Param fontName, short *familyID);

/* Modern Font Format Support */
OSErr InitializeOpenTypeFonts(void);
OSErr InitializeWOFFSupport(void);
OSErr InitializeSystemFontSupport(void);
OSErr LoadOpenTypeFont(ConstStr255Param filePath, OpenTypeFont **font);
OSErr LoadWOFFFont(ConstStr255Param filePath, WOFFFont **font);
OSErr LoadSystemFont(ConstStr255Param fontName, SystemFont **font);
OSErr UnloadOpenTypeFont(OpenTypeFont *font);
OSErr UnloadWOFFFont(WOFFFont *font);
OSErr UnloadSystemFont(SystemFont *font);

/* Font Collection Support */
OSErr LoadFontCollection(ConstStr255Param filePath, FontCollection **collection);
OSErr UnloadFontCollection(FontCollection *collection);
OSErr GetFontFromCollection(FontCollection *collection, short index, ModernFont **font);
OSErr GetCollectionFontCount(FontCollection *collection, short *count);

/* Web Font Support */
OSErr DownloadWebFont(ConstStr255Param url, ConstStr255Param cachePath, short *familyID);
OSErr ValidateWebFont(ConstStr255Param filePath);
OSErr GetWebFontMetadata(ConstStr255Param filePath, WebFontMetadata *metadata);

/* Font Hinting and Rasterization */
OSErr SetFontHinting(Boolean enableHinting);
Boolean GetFontHinting(void);
OSErr SetFontSmoothing(Boolean enableSmoothing);
Boolean GetFontSmoothing(void);
OSErr SetFontGamma(Fixed gammaValue);
Fixed GetFontGamma(void);

/* Internal Font Manager Functions (Implementation Details) */
OSErr _InitFontManagerTables(void);
OSErr _BuildFontCache(void);
OSErr _ParseFontResource(Handle resource, FontRec **fontRec);
OSErr _ParseFamilyResource(Handle resource, FamRec **famRec);
OSErr _BuildWidthTable(const FMInput *input, WidthTable **widthTable);
OSErr _ScaleFont(FontRec *font, Point numer, Point denom, FontRec **scaledFont);

/* Font Manager State */

/* QuickDraw Integration */
void TextFont(short font);
void TextFace(Style face);
void TextSize(short size);
void TextMode(short mode);

/* Width Measurement */
short CharWidth(short ch);

/* Width with no style applied - the base both CharWidth and
 * FM_GetStyledCharWidth measure from, so neither calls the other. */
short FM_GetPlainCharWidth(short ch);
short StringWidth(ConstStr255Param s);
short TextWidth(const void* textBuf, short firstByte, short byteCount);

/* Get current Font Manager state */
FontManagerState *GetFontManagerState(void);
FontStrike *FM_GetCurrentStrike(void);

/* Font Drawing and Measurement */
short FM_MeasureRun(const unsigned char* bytes, short len);
void FM_DrawRun(const unsigned char* bytes, short len, Point baseline);

/* Error handling */
OSErr GetLastFontError(void);
void SetFontErrorCallback(void (*callback)(OSErr error, const char *message));

/* Font Stack (Push/Pop for nested font operations) */
void FMPushFont(void);          /* Save current font state */
void FMPopFont(void);           /* Restore previous font state */
SInt16 FMGetFontStackDepth(void); /* Get current stack depth */
void FMSetFontSize(SInt16 size); /* Set font size with standard sizes (9, 12, 14, 18) */

#ifdef __cplusplus
}
#endif

#endif /* FONT_MANAGER_H */
