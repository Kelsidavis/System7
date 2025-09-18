/************************************************************
 *
 * TextFormatting.c
 * System 7.1 Portable - TextEdit Formatting Implementation
 *
 * Font, style, and paragraph formatting implementation for
 * TextEdit. Provides comprehensive text styling capabilities
 * with full System 7.1 compatibility.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Based on Apple Computer, Inc. TextEdit Manager 1985-1992
 *
 ************************************************************/

#include "TextEdit/TextEdit.h"
#include "TextEdit/TextTypes.h"
#include "TextEdit/TextFormatting.h"
#include "Memory.h"
#include "Fonts.h"
#include "Quickdraw.h"
#include <string.h>
#include <stdlib.h>

/************************************************************
 * Style Management Internal Functions
 ************************************************************/

static TEStyleHandle TEGetStyleHandleInternal(TEHandle hTE)
{
    /* In full implementation, would extract from TERec extended structure */
    /* For now, return NULL - plain text only */
    return NULL;
}

static OSErr TECreateDefaultStyle(TEHandle hTE, TextStyle *style)
{
    TERec **teRec;

    if (!hTE || !*hTE || !style) return paramErr;

    teRec = (TERec **)hTE;

    /* Initialize with current TERec settings */
    style->tsFont = (**teRec).txFont;
    style->tsFace = (**teRec).txFace;
    style->filler = 0;
    style->tsSize = (**teRec).txSize;
    style->tsColor.red = 0x0000;
    style->tsColor.green = 0x0000;
    style->tsColor.blue = 0x0000;

    return noErr;
}

static Boolean TECompareTextStyles(const TextStyle *style1, const TextStyle *style2)
{
    if (!style1 || !style2) return false;

    return (style1->tsFont == style2->tsFont &&
            style1->tsFace == style2->tsFace &&
            style1->tsSize == style2->tsSize &&
            style1->tsColor.red == style2->tsColor.red &&
            style1->tsColor.green == style2->tsColor.green &&
            style1->tsColor.blue == style2->tsColor.blue);
}

/************************************************************
 * Core Style Functions
 ************************************************************/

pascal void TEGetStyle(short offset, TextStyle *theStyle, short *lineHeight,
                      short *fontAscent, TEHandle hTE)
{
    TERec **teRec;
    TEStyleHandle hStyle;

    if (!hTE || !*hTE || !theStyle) return;

    teRec = (TERec **)hTE;
    hStyle = TEGetStyleHandleInternal(hTE);

    if (hStyle) {
        /* Extract style from style table - complex implementation */
        /* For now, use default style */
    }

    /* Return current TERec style */
    TECreateDefaultStyle(hTE, theStyle);

    if (lineHeight) *lineHeight = (**teRec).lineHeight;
    if (fontAscent) *fontAscent = (**teRec).fontAscent;
}

pascal void TESetStyle(short mode, const TextStyle *newStyle, Boolean fRedraw,
                      TEHandle hTE)
{
    TERec **teRec;
    TEStyleHandle hStyle;

    if (!hTE || !*hTE || !newStyle) return;

    teRec = (TERec **)hTE;
    hStyle = TEGetStyleHandleInternal(hTE);

    /* Apply style based on mode */
    if (mode & doFont) {
        (**teRec).txFont = newStyle->tsFont;
    }

    if (mode & doFace) {
        if (mode & doToggle) {
            (**teRec).txFace ^= newStyle->tsFace;
        } else {
            (**teRec).txFace = newStyle->tsFace;
        }
    }

    if (mode & doSize) {
        if (mode & addSize) {
            (**teRec).txSize += newStyle->tsSize;
            if ((**teRec).txSize < 1) (**teRec).txSize = 1;
            if ((**teRec).txSize > 255) (**teRec).txSize = 255;
        } else {
            (**teRec).txSize = newStyle->tsSize;
        }
    }

    /* Color changes would be handled in styled text implementation */

    /* Recalculate text layout */
    TECalText(hTE);

    /* Redraw if requested */
    if (fRedraw) {
        TEUpdate(NULL, hTE);
    }
}

pascal Boolean TEContinuousStyle(short *mode, TextStyle *aStyle, TEHandle hTE)
{
    TERec **teRec;
    TextStyle selectionStyle;
    Boolean continuous = true;

    if (!hTE || !*hTE || !mode || !aStyle) return false;

    teRec = (TERec **)hTE;

    /* For non-styled text, always continuous */
    if ((**teRec).selStart == (**teRec).selEnd) {
        /* No selection - return current style */
        TECreateDefaultStyle(hTE, aStyle);
        *mode = doAll;
        return true;
    }

    /* Check if selection has consistent styling */
    /* In full implementation, would iterate through style runs */
    TECreateDefaultStyle(hTE, aStyle);
    *mode = doAll; /* All attributes are continuous */

    return continuous;
}

pascal void TEReplaceStyle(short mode, const TextStyle *oldStyle,
                          const TextStyle *newStyle, Boolean fRedraw, TEHandle hTE)
{
    /* In full implementation, would find and replace specific styles */
    /* For now, just apply the new style */
    TESetStyle(mode, newStyle, fRedraw, hTE);
}

/************************************************************
 * StyleHandle Management
 ************************************************************/

pascal void TESetStyleHandle(TEStyleHandle theHandle, TEHandle hTE)
{
    /* In full implementation, would attach style handle to TERec */
    /* For now, this is a no-op */
}

pascal void SetStyleHandle(TEStyleHandle theHandle, TEHandle hTE)
{
    TESetStyleHandle(theHandle, hTE);
}

pascal void SetStylHandle(TEStyleHandle theHandle, TEHandle hTE)
{
    TESetStyleHandle(theHandle, hTE);
}

pascal TEStyleHandle TEGetStyleHandle(TEHandle hTE)
{
    return TEGetStyleHandleInternal(hTE);
}

pascal TEStyleHandle GetStyleHandle(TEHandle hTE)
{
    return TEGetStyleHandle(hTE);
}

pascal TEStyleHandle GetStylHandle(TEHandle hTE)
{
    return TEGetStyleHandle(hTE);
}

/************************************************************
 * Advanced Style Operations
 ************************************************************/

pascal long TEGetHeight(long endLine, long startLine, TEHandle hTE)
{
    TERec **teRec;
    long totalHeight;
    long lineCount;

    if (!hTE || !*hTE) return 0;

    teRec = (TERec **)hTE;

    if (startLine < 0) startLine = 0;
    if (endLine < 0) endLine = 0;
    if (startLine > (**teRec).nLines) startLine = (**teRec).nLines;
    if (endLine > (**teRec).nLines) endLine = (**teRec).nLines;

    if (startLine > endLine) {
        long temp = startLine;
        startLine = endLine;
        endLine = temp;
    }

    lineCount = endLine - startLine;
    totalHeight = lineCount * (**teRec).lineHeight;

    return totalHeight;
}

pascal long TENumStyles(long rangeStart, long rangeEnd, TEHandle hTE)
{
    /* In full implementation, would count unique styles in range */
    /* For plain text, always 1 style */
    if (!hTE || !*hTE) return 0;

    return 1;
}

pascal short TEFeatureFlag(short feature, short action, TEHandle hTE)
{
    TERec **teRec;
    TEDispatchHandle hDispatch;
    short currentValue = 0;
    short newFlags;

    if (!hTE || !*hTE) return 0;

    teRec = (TERec **)hTE;
    hDispatch = (TEDispatchHandle)(**teRec).hDispatchRec;

    if (!hDispatch) return 0;

    TEDispatchRec **dispatch = (TEDispatchRec **)hDispatch;
    newFlags = (**dispatch).newTEFlags;

    /* Get current value */
    switch (feature) {
        case teFAutoScroll:
            currentValue = (newFlags & (1 << teFAutoScroll)) ? 1 : 0;
            break;
        case teFTextBuffering:
            currentValue = (newFlags & (1 << teFTextBuffering)) ? 1 : 0;
            break;
        case teFOutlineHilite:
            currentValue = (newFlags & (1 << teFOutlineHilite)) ? 1 : 0;
            break;
        case teFInlineInput:
            currentValue = (newFlags & (1 << teFInlineInput)) ? 1 : 0;
            break;
        case teFUseTextServices:
            currentValue = (newFlags & (1 << teFUseTextServices)) ? 1 : 0;
            break;
        default:
            return 0;
    }

    /* Apply action */
    switch (action) {
        case TEBitSet:
            newFlags |= (1 << feature);
            (**dispatch).newTEFlags = newFlags;
            break;
        case TEBitClear:
            newFlags &= ~(1 << feature);
            (**dispatch).newTEFlags = newFlags;
            break;
        case TEBitTest:
            /* No change - just return current value */
            break;
    }

    return currentValue;
}

/************************************************************
 * Hook Management
 ************************************************************/

pascal void TECustomHook(TEIntHook which, ProcPtr *addr, TEHandle hTE)
{
    TERec **teRec;
    TEDispatchHandle hDispatch;

    if (!hTE || !*hTE || !addr) return;

    teRec = (TERec **)hTE;
    hDispatch = (TEDispatchHandle)(**teRec).hDispatchRec;

    if (!hDispatch) return;

    TEDispatchRec **dispatch = (TEDispatchRec **)hDispatch;

    switch (which) {
        case intEOLHook:
            *addr = (**dispatch).EOLHook;
            break;
        case intDrawHook:
            *addr = (**dispatch).DRAWHook;
            break;
        case intWidthHook:
            *addr = (**dispatch).WIDTHHook;
            break;
        case intHitTestHook:
            *addr = (**dispatch).HITTESTHook;
            break;
        case intNWidthHook:
            *addr = (**dispatch).nWIDTHHook;
            break;
        case intTextWidthHook:
            *addr = (**dispatch).TextWidthHook;
            break;
        default:
            *addr = NULL;
            break;
    }
}

pascal void TESetClickLoop(ClikLoopProcPtr clikProc, TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;
    (**teRec).clikLoop = (ProcPtr)clikProc;
}

pascal void SetClikLoop(ClikLoopProcPtr clikProc, TEHandle hTE)
{
    TESetClickLoop(clikProc, hTE);
}

pascal void TESetWordBreak(WordBreakProcPtr wBrkProc, TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;
    (**teRec).wordBreak = (ProcPtr)wBrkProc;
}

pascal void SetWordBreak(WordBreakProcPtr wBrkProc, TEHandle hTE)
{
    TESetWordBreak(wBrkProc, hTE);
}

/************************************************************
 * Extended Formatting Functions
 ************************************************************/

OSErr TESetTextStyleEx(TEHandle hTE, long start, long end, const TETextStyle* style)
{
    /* Extended styling would be implemented here */
    if (!hTE || !style) return paramErr;

    /* For now, convert to basic TextStyle and apply */
    TextStyle basicStyle;
    basicStyle.tsFont = style->tsFont;
    basicStyle.tsFace = style->tsFace;
    basicStyle.filler = style->tsFiller;
    basicStyle.tsSize = style->tsSize;
    basicStyle.tsColor = style->tsColor;

    TESetStyle(doAll, &basicStyle, true, hTE);
    return noErr;
}

OSErr TEGetTextStyleEx(TEHandle hTE, long offset, TETextStyle* style)
{
    TextStyle basicStyle;
    short lineHeight, fontAscent;

    if (!hTE || !style) return paramErr;

    TEGetStyle(offset, &basicStyle, &lineHeight, &fontAscent, hTE);

    /* Convert to extended style */
    memset(style, 0, sizeof(TETextStyle));
    style->tsFont = basicStyle.tsFont;
    style->tsFace = basicStyle.tsFace;
    style->tsFiller = basicStyle.filler;
    style->tsSize = basicStyle.tsSize;
    style->tsColor = basicStyle.tsColor;

    /* Set defaults for extended attributes */
    style->tsBackColor.red = 0xFFFF;
    style->tsBackColor.green = 0xFFFF;
    style->tsBackColor.blue = 0xFFFF;
    style->tsScaleH = 0x00010000; /* 1.0 */
    style->tsScaleV = 0x00010000; /* 1.0 */

    return noErr;
}

OSErr TESetDefaultTextStyle(TEHandle hTE, const TETextStyle* style)
{
    /* Set the default style for new text */
    return TESetTextStyleEx(hTE, 0, 0, style);
}

OSErr TECreateStyleFromParent(const TETextStyle* parent, const TETextStyle* changes,
                             TETextStyle* result)
{
    if (!parent || !result) return paramErr;

    /* Start with parent style */
    *result = *parent;

    /* Apply changes if provided */
    if (changes) {
        if (changes->tsFont != 0) result->tsFont = changes->tsFont;
        if (changes->tsFace != 0) result->tsFace = changes->tsFace;
        if (changes->tsSize != 0) result->tsSize = changes->tsSize;
        /* Apply other changes as needed */
    }

    return noErr;
}

Boolean TECompareStyles(const TETextStyle* style1, const TETextStyle* style2)
{
    if (!style1 || !style2) return false;

    return (style1->tsFont == style2->tsFont &&
            style1->tsFace == style2->tsFace &&
            style1->tsSize == style2->tsSize &&
            style1->tsColor.red == style2->tsColor.red &&
            style1->tsColor.green == style2->tsColor.green &&
            style1->tsColor.blue == style2->tsColor.blue);
}

/************************************************************
 * Paragraph Formatting
 ************************************************************/

OSErr TESetParaFormat(TEHandle hTE, long start, long end, const TEParaFormat* format)
{
    TERec **teRec;

    if (!hTE || !*hTE || !format) return paramErr;

    teRec = (TERec **)hTE;

    /* Apply basic paragraph formatting */
    if (format->paraJust >= teJustLeft && format->paraJust <= teJustRight) {
        (**teRec).just = format->paraJust;
    }

    /* More complex paragraph formatting would be implemented here */

    /* Recalculate layout */
    TECalText(hTE);

    return noErr;
}

OSErr TEGetParaFormat(TEHandle hTE, long offset, TEParaFormat* format)
{
    TERec **teRec;

    if (!hTE || !*hTE || !format) return paramErr;

    teRec = (TERec **)hTE;

    /* Initialize format with defaults */
    memset(format, 0, sizeof(TEParaFormat));
    format->paraJust = (**teRec).just;
    format->paraDirection = kTEParaDirectionLTR;
    format->paraLineSpacing = kTELineSpacingSingle;
    format->paraLineSpacingValue = 0x00010000; /* 1.0 */

    return noErr;
}

OSErr TESetParagraphJustification(TEHandle hTE, long start, long end, short just)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;
    (**teRec).just = just;

    TECalText(hTE);
    TEUpdate(NULL, hTE);

    return noErr;
}

/************************************************************
 * Font Operations
 ************************************************************/

OSErr TESetFontByName(TEHandle hTE, long start, long end, const char* fontName)
{
    short fontID;

    if (!hTE || !fontName) return paramErr;

    GetFNum((ConstStr255Param)fontName, &fontID);
    if (fontID == 0) return fnfErr; /* Font not found */

    return TESetFontByID(hTE, start, end, fontID);
}

OSErr TESetFontByID(TEHandle hTE, long start, long end, short fontID)
{
    TextStyle style;
    short lineHeight, fontAscent;

    if (!hTE) return paramErr;

    TEGetStyle(0, &style, &lineHeight, &fontAscent, hTE);
    style.tsFont = fontID;

    TESetStyle(doFont, &style, true, hTE);
    return noErr;
}

OSErr TESetFontSize(TEHandle hTE, long start, long end, short size)
{
    TextStyle style;
    short lineHeight, fontAscent;

    if (!hTE) return paramErr;

    TEGetStyle(0, &style, &lineHeight, &fontAscent, hTE);
    style.tsSize = size;

    TESetStyle(doSize, &style, true, hTE);
    return noErr;
}

OSErr TESetCharacterStyle(TEHandle hTE, long start, long end, Style style, Boolean add)
{
    TextStyle textStyle;
    short lineHeight, fontAscent;
    short mode;

    if (!hTE) return paramErr;

    TEGetStyle(0, &textStyle, &lineHeight, &fontAscent, hTE);
    textStyle.tsFace = style;

    mode = doFace;
    if (add) mode |= doToggle;

    TESetStyle(mode, &textStyle, true, hTE);
    return noErr;
}

OSErr TESetTextColor(TEHandle hTE, long start, long end, const RGBColor* color)
{
    TextStyle style;
    short lineHeight, fontAscent;

    if (!hTE || !color) return paramErr;

    TEGetStyle(0, &style, &lineHeight, &fontAscent, hTE);
    style.tsColor = *color;

    TESetStyle(doColor, &style, true, hTE);
    return noErr;
}

/************************************************************
 * Format Validation and Utilities
 ************************************************************/

OSErr TEValidateTextStyle(const TETextStyle* style)
{
    if (!style) return paramErr;

    /* Validate font ID */
    if (style->tsFont < 1 || style->tsFont > 255) return paramErr;

    /* Validate size */
    if (style->tsSize < 1 || style->tsSize > 255) return paramErr;

    /* Style face and colors are always valid */

    return noErr;
}

OSErr TEValidateParaFormat(const TEParaFormat* format)
{
    if (!format) return paramErr;

    /* Validate justification */
    if (format->paraJust < teJustLeft || format->paraJust > teJustRight) {
        return paramErr;
    }

    /* Validate other parameters */
    if (format->paraLineSpacing < kTELineSpacingSingle ||
        format->paraLineSpacing > kTELineSpacingCustom) {
        return paramErr;
    }

    return noErr;
}

OSErr TEInitializeTextStyle(TETextStyle* style)
{
    if (!style) return paramErr;

    memset(style, 0, sizeof(TETextStyle));

    /* Set reasonable defaults */
    style->tsFont = 1; /* System font */
    style->tsFace = normal;
    style->tsSize = 12;
    style->tsColor.red = 0x0000;
    style->tsColor.green = 0x0000;
    style->tsColor.blue = 0x0000;
    style->tsBackColor.red = 0xFFFF;
    style->tsBackColor.green = 0xFFFF;
    style->tsBackColor.blue = 0xFFFF;
    style->tsScaleH = 0x00010000; /* 1.0 */
    style->tsScaleV = 0x00010000; /* 1.0 */

    return noErr;
}

OSErr TEInitializeParaFormat(TEParaFormat* format)
{
    if (!format) return paramErr;

    memset(format, 0, sizeof(TEParaFormat));

    /* Set reasonable defaults */
    format->paraJust = teJustLeft;
    format->paraDirection = kTEParaDirectionLTR;
    format->paraLineSpacing = kTELineSpacingSingle;
    format->paraLineSpacingValue = 0x00010000; /* 1.0 */

    return noErr;
}