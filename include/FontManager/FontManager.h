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


/* Font Rendering Support */
OSErr RenderGlyph(short familyID, short size, short style, char character,
                  GrafPtr port, Point location, short mode);
OSErr GetGlyphOutline(short familyID, short size, short style, char character,
                      void **outline, long *outlineSize);


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
