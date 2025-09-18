/************************************************************
 *
 * TextFormatting.h
 * System 7.1 Portable - TextEdit Formatting APIs
 *
 * Text styling, formatting, and layout APIs for TextEdit.
 * Provides comprehensive font, style, color, and paragraph
 * formatting capabilities.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Based on Apple Computer, Inc. TextEdit Manager 1985-1992
 *
 ************************************************************/

#ifndef __TEXTFORMATTING_H__
#define __TEXTFORMATTING_H__

#ifndef __TYPES_H__
#include <Types.h>
#endif

#ifndef __QUICKDRAW_H__
#include <Quickdraw.h>
#endif

#ifndef __FONTS_H__
#include <Fonts.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************
 * Formatting Constants
 ************************************************************/

/* Advanced justification modes */
enum {
    kTEJustifyLeft = 0,         /* Left justify */
    kTEJustifyCenter = 1,       /* Center justify */
    kTEJustifyRight = 2,        /* Right justify */
    kTEJustifyFull = 3,         /* Full justify */
    kTEJustifyNatural = 4       /* Natural justify based on script */
};

/* Line spacing modes */
enum {
    kTELineSpacingSingle = 0,   /* Single spacing */
    kTELineSpacingOneHalf = 1,  /* 1.5x spacing */
    kTELineSpacingDouble = 2,   /* Double spacing */
    kTELineSpacingCustom = 3    /* Custom spacing */
};

/* Tab alignment types */
enum {
    kTETabLeft = 0,             /* Left aligned tab */
    kTETabCenter = 1,           /* Center aligned tab */
    kTETabRight = 2,            /* Right aligned tab */
    kTETabDecimal = 3           /* Decimal aligned tab */
};

/* Paragraph direction */
enum {
    kTEParaDirectionLTR = 0,    /* Left to right */
    kTEParaDirectionRTL = 1     /* Right to left */
};

/************************************************************
 * Formatting Data Structures
 ************************************************************/

/* Extended text style */
typedef struct TETextStyle {
    short tsFont;               /* Font family ID */
    Style tsFace;               /* Style flags */
    char tsFiller;              /* Padding byte */
    short tsSize;               /* Point size */
    RGBColor tsColor;           /* Text color */
    RGBColor tsBackColor;       /* Background color */
    short tsUnderlineStyle;     /* Underline style */
    RGBColor tsUnderlineColor;  /* Underline color */
    short tsStrikeStyle;        /* Strikethrough style */
    RGBColor tsStrikeColor;     /* Strikethrough color */
    Fixed tsScaleH;             /* Horizontal scaling */
    Fixed tsScaleV;             /* Vertical scaling */
    short tsTracking;           /* Character tracking */
    short tsKerning;            /* Kerning adjustment */
    short tsLanguage;           /* Language code */
    short tsScript;             /* Script code */
} TETextStyle;

/* Tab stop definition */
typedef struct TETabStop {
    short tabPosition;          /* Tab position in pixels */
    short tabType;              /* Tab alignment type */
    char tabLeader;             /* Tab leader character */
    char tabPadding;            /* Padding */
} TETabStop;

/* Paragraph formatting */
typedef struct TEParaFormat {
    short paraJust;             /* Paragraph justification */
    short paraDirection;        /* Text direction */
    short paraLeftIndent;       /* Left indent in pixels */
    short paraRightIndent;      /* Right indent in pixels */
    short paraFirstLineIndent;  /* First line indent */
    short paraLineSpacing;      /* Line spacing mode */
    Fixed paraLineSpacingValue; /* Custom line spacing value */
    short paraSpaceBefore;      /* Space before paragraph */
    short paraSpaceAfter;       /* Space after paragraph */
    short paraTabCount;         /* Number of tab stops */
    TETabStop paraTabs[32];     /* Tab stop array */
    RGBColor paraBackColor;     /* Paragraph background color */
    short paraBorderStyle;      /* Border style */
    RGBColor paraBorderColor;   /* Border color */
    short paraBorderWidth;      /* Border width */
} TEParaFormat;

/* Style run with extended formatting */
typedef struct TEStyleRunEx {
    long startChar;             /* Starting character position */
    TETextStyle textStyle;      /* Text style */
    TEParaFormat paraFormat;    /* Paragraph format */
} TEStyleRunEx;

/* Extended style table */
typedef struct TEStyleTableEx {
    short stCount;              /* Number of styles */
    TEStyleRunEx styles[1];     /* Variable length array */
} TEStyleTableEx, *TEStyleTableExPtr, **TEStyleTableExHandle;

/* Format run for efficient storage */
typedef struct TEFormatRun {
    long runStart;              /* Start of run */
    long runLength;             /* Length of run */
    short styleIndex;           /* Index in style table */
    short paraIndex;            /* Index in paragraph table */
} TEFormatRun;

/* Ruler information */
typedef struct TERuler {
    short rulerVisible;         /* Ruler visibility */
    short rulerHeight;          /* Ruler height */
    short rulerUnits;           /* Measurement units */
    short rulerOrigin;          /* Origin position */
    TEParaFormat rulerFormat;   /* Default paragraph format */
} TERuler;

/************************************************************
 * Style Management Functions
 ************************************************************/

/* Extended style operations */
OSErr TESetTextStyleEx(TEHandle hTE, long start, long end, const TETextStyle* style);
OSErr TEGetTextStyleEx(TEHandle hTE, long offset, TETextStyle* style);
OSErr TESetDefaultTextStyle(TEHandle hTE, const TETextStyle* style);
OSErr TEGetDefaultTextStyle(TEHandle hTE, TETextStyle* style);

/* Style inheritance and cascading */
OSErr TECreateStyleFromParent(const TETextStyle* parent, const TETextStyle* changes,
                             TETextStyle* result);
OSErr TEMergeStyles(const TETextStyle* base, const TETextStyle* overlay,
                   TETextStyle* result);
OSErr TECopyStyle(const TETextStyle* source, TETextStyle* dest);

/* Style comparison and analysis */
Boolean TECompareStyles(const TETextStyle* style1, const TETextStyle* style2);
OSErr TEGetStyleDifferences(const TETextStyle* style1, const TETextStyle* style2,
                           short* differences);
OSErr TEOptimizeStyleRuns(TEHandle hTE);

/************************************************************
 * Paragraph Formatting
 ************************************************************/

/* Paragraph format operations */
OSErr TESetParaFormat(TEHandle hTE, long start, long end, const TEParaFormat* format);
OSErr TEGetParaFormat(TEHandle hTE, long offset, TEParaFormat* format);
OSErr TESetDefaultParaFormat(TEHandle hTE, const TEParaFormat* format);
OSErr TEGetDefaultParaFormat(TEHandle hTE, TEParaFormat* format);

/* Justification and alignment */
OSErr TESetParagraphJustification(TEHandle hTE, long start, long end, short just);
OSErr TESetParagraphDirection(TEHandle hTE, long start, long end, short direction);
OSErr TESetParagraphIndents(TEHandle hTE, long start, long end,
                           short leftIndent, short rightIndent, short firstLineIndent);

/* Line spacing */
OSErr TESetLineSpacing(TEHandle hTE, long start, long end, short spacingMode, Fixed customValue);
OSErr TEGetLineSpacing(TEHandle hTE, long offset, short* spacingMode, Fixed* customValue);

/* Paragraph spacing */
OSErr TESetParagraphSpacing(TEHandle hTE, long start, long end,
                           short spaceBefore, short spaceAfter);
OSErr TEGetParagraphSpacing(TEHandle hTE, long offset,
                           short* spaceBefore, short* spaceAfter);

/************************************************************
 * Tab Management
 ************************************************************/

/* Tab stop operations */
OSErr TESetTabStops(TEHandle hTE, long start, long end, const TETabStop* tabs, short count);
OSErr TEGetTabStops(TEHandle hTE, long offset, TETabStop* tabs, short* count);
OSErr TEAddTabStop(TEHandle hTE, long start, long end, const TETabStop* tab);
OSErr TERemoveTabStop(TEHandle hTE, long start, long end, short position);
OSErr TEClearTabStops(TEHandle hTE, long start, long end);

/* Tab navigation and positioning */
OSErr TEGetNextTabStop(TEHandle hTE, long offset, short currentPos,
                      short* nextPos, short* tabType);
OSErr TEGetTabLeaderChar(TEHandle hTE, long offset, short tabPos, char* leader);

/************************************************************
 * Font and Character Formatting
 ************************************************************/

/* Font operations */
OSErr TESetFontByName(TEHandle hTE, long start, long end, const char* fontName);
OSErr TEGetFontName(TEHandle hTE, long offset, char* fontName, short maxLength);
OSErr TESetFontByID(TEHandle hTE, long start, long end, short fontID);
OSErr TEGetFontID(TEHandle hTE, long offset, short* fontID);

/* Font size operations */
OSErr TESetFontSize(TEHandle hTE, long start, long end, short size);
OSErr TEGetFontSize(TEHandle hTE, long offset, short* size);
OSErr TEChangeFontSize(TEHandle hTE, long start, long end, short delta);
OSErr TEScaleFontSize(TEHandle hTE, long start, long end, Fixed scale);

/* Character style operations */
OSErr TESetCharacterStyle(TEHandle hTE, long start, long end, Style style, Boolean add);
OSErr TEGetCharacterStyle(TEHandle hTE, long offset, Style* style);
OSErr TEToggleCharacterStyle(TEHandle hTE, long start, long end, Style style);

/* Color operations */
OSErr TESetTextColor(TEHandle hTE, long start, long end, const RGBColor* color);
OSErr TEGetTextColor(TEHandle hTE, long offset, RGBColor* color);
OSErr TESetBackgroundColor(TEHandle hTE, long start, long end, const RGBColor* color);
OSErr TEGetBackgroundColor(TEHandle hTE, long offset, RGBColor* color);

/************************************************************
 * Advanced Character Formatting
 ************************************************************/

/* Underline and strikethrough */
OSErr TESetUnderline(TEHandle hTE, long start, long end, short style, const RGBColor* color);
OSErr TEGetUnderline(TEHandle hTE, long offset, short* style, RGBColor* color);
OSErr TESetStrikethrough(TEHandle hTE, long start, long end, short style, const RGBColor* color);
OSErr TEGetStrikethrough(TEHandle hTE, long offset, short* style, RGBColor* color);

/* Character scaling and spacing */
OSErr TESetCharacterScaling(TEHandle hTE, long start, long end, Fixed hScale, Fixed vScale);
OSErr TEGetCharacterScaling(TEHandle hTE, long offset, Fixed* hScale, Fixed* vScale);
OSErr TESetCharacterTracking(TEHandle hTE, long start, long end, short tracking);
OSErr TEGetCharacterTracking(TEHandle hTE, long offset, short* tracking);
OSErr TESetKerning(TEHandle hTE, long start, long end, short kerning);
OSErr TEGetKerning(TEHandle hTE, long offset, short* kerning);

/* Language and script formatting */
OSErr TESetTextLanguage(TEHandle hTE, long start, long end, short language);
OSErr TEGetTextLanguage(TEHandle hTE, long offset, short* language);
OSErr TESetTextScript(TEHandle hTE, long start, long end, short script);
OSErr TEGetTextScript(TEHandle hTE, long offset, short* script);

/************************************************************
 * Ruler and Measurement
 ************************************************************/

/* Ruler management */
OSErr TECreateRuler(TEHandle hTE, TERuler** ruler);
OSErr TEDisposeRuler(TERuler* ruler);
OSErr TESetRulerVisible(TEHandle hTE, Boolean visible);
Boolean TEIsRulerVisible(TEHandle hTE);
OSErr TEGetRulerRect(TEHandle hTE, Rect* rulerRect);

/* Measurement and positioning */
OSErr TEGetCharacterBounds(TEHandle hTE, long offset, Rect* bounds);
OSErr TEGetLineBounds(TEHandle hTE, long lineIndex, Rect* bounds);
OSErr TEGetParagraphBounds(TEHandle hTE, long offset, Rect* bounds);

/* Text metrics */
OSErr TEGetLineMetrics(TEHandle hTE, long lineIndex, short* ascent, short* descent,
                      short* leading, short* width);
OSErr TEGetParagraphMetrics(TEHandle hTE, long offset, short* lineCount,
                           short* totalHeight, short* maxWidth);

/************************************************************
 * Format Import/Export
 ************************************************************/

/* Style scrap operations */
OSErr TECreateStyleScrapEx(const TEStyleTableEx* styleTable, StScrpHandle* scrapHandle);
OSErr TEStyleScrapToTableEx(StScrpHandle scrapHandle, TEStyleTableExHandle* styleTable);
OSErr TEImportStyleScrap(TEHandle hTE, long start, long end, StScrpHandle scrapHandle);
OSErr TEExportStyleScrap(TEHandle hTE, long start, long end, StScrpHandle* scrapHandle);

/* RTF-like format support */
OSErr TEExportRTF(TEHandle hTE, long start, long end, Handle* rtfData);
OSErr TEImportRTF(TEHandle hTE, long insertPos, Handle rtfData);

/* Plain text with formatting preservation */
OSErr TEExportPlainTextWithStyles(TEHandle hTE, long start, long end,
                                 Handle* textData, Handle* styleData);
OSErr TEImportPlainTextWithStyles(TEHandle hTE, long insertPos,
                                 Handle textData, Handle styleData);

/************************************************************
 * Format Validation and Utilities
 ************************************************************/

/* Format validation */
OSErr TEValidateTextStyle(const TETextStyle* style);
OSErr TEValidateParaFormat(const TEParaFormat* format);
OSErr TEValidateStyleTable(TEStyleTableExHandle styleTable);

/* Format utilities */
OSErr TEInitializeTextStyle(TETextStyle* style);
OSErr TEInitializeParaFormat(TEParaFormat* format);
OSErr TEGetSystemDefaultStyles(TETextStyle* textStyle, TEParaFormat* paraFormat);

/* Format analysis */
OSErr TEAnalyzeFormatting(TEHandle hTE, long start, long end, short* styleCount,
                         short* paraCount, Boolean* hasComplexFormatting);
OSErr TEGetFormattingStatistics(TEHandle hTE, short* totalStyles, short* totalParaFormats,
                               long* formattedChars, long* plainChars);

#ifdef __cplusplus
}
#endif

#endif /* __TEXTFORMATTING_H__ */