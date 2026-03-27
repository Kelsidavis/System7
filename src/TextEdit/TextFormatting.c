/* #include "SystemTypes.h" */
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
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
 * Derived from ROM analysis: TextEdit Manager 1985-1992
 *
 ************************************************************/

// #include "CompatibilityFix.h" // Removed
#include "TextEdit/TextEdit.h"
#include "TextEdit/TextTypes.h"
#include "TextEdit/TextFormatting.h"
#include "MemoryMgr/memory_manager_types.h"
#include "Fonts.h"
#include "QuickDraw/QuickDraw.h"


/************************************************************
 * Style Management Internal Functions
 ************************************************************/

/* Extended TERec with style support - matches TextEditClipboard.c */
typedef struct TEExtRec {
    TERec       base;           /* Standard TERec */
    Handle      hLines;         /* Line starts array */
    SInt16      nLines;         /* Number of lines */
    Handle      hStyles;        /* Style record handle (STHandle) */
    Boolean     dirty;          /* Needs recalc */
    Boolean     readOnly;       /* Read-only flag */
    Boolean     wordWrap;       /* Word wrap flag */
    SInt16      dragAnchor;     /* Drag selection anchor */
    Boolean     inDragSel;      /* In drag selection */
    UInt32      lastClickTime;  /* For double/triple click */
    SInt16      clickCount;     /* Click count */
    SInt16      viewDH;         /* Horizontal scroll */
    SInt16      viewDV;         /* Vertical scroll */
    Boolean     autoViewEnabled;/* Auto-scroll flag */
} TEExtRec;

typedef TEExtRec *TEExtPtr, **TEExtHandle;

/* Style table and run structures */
typedef struct StyleTable {
    SInt16      nStyles;        /* Number of unique styles */
    TextStyle   styles[1];      /* Variable array of styles */
} StyleTable;

typedef struct RunArray {
    SInt16      nRuns;          /* Number of style runs */
    StyleRun    runs[1];        /* Variable array of runs */
} RunArray;

typedef struct STRec_Internal {
    SInt16      nRuns;          /* Number of style runs */
    SInt16      nStyles;        /* Number of unique styles */
    Handle      styleTab;       /* Handle to StyleTable */
    Handle      runArray;       /* Handle to RunArray */
    Handle      lineHeights;    /* Handle to line height array (optional) */
} STRec_Internal;

static TEStyleHandle TEGetStyleHandleInternal(TEHandle hTE)
{
    TEStyleHandle result;

    if (!hTE || !*hTE) return NULL;

    HLock((Handle)hTE);
    result = (TEStyleHandle)((TEExtPtr)*hTE)->hStyles;
    HUnlock((Handle)hTE);
    return result;
}

static OSErr TECreateDefaultStyle(TEHandle hTE, TextStyle *style)
{
    TERec **teRec;

    if (!hTE || !*hTE || !style) return paramErr;

    teRec = (TERec **)hTE;

    /* Initialize with current TERec settings */
    style->tsFont = (**teRec).txFont;
    style->tsFace = (**teRec).txFace;
    style->tsSize = (**teRec).txSize;
    style->tsColor.red = 0x0000;
    style->tsColor.green = 0x0000;
    style->tsColor.blue = 0x0000;

    return noErr;
}

/* Create a new style record with default style */
static STHandle TECreateStyleRec(TEHandle hTE)
{
    extern Handle NewHandle(Size byteCount);
    extern void DisposeHandle(Handle h);

    STRec_Internal* stRec;
    Handle hStyle;
    StyleTable* styleTab;
    RunArray* runArr;
    TextStyle defaultStyle;

    /* Allocate STRec */
    hStyle = NewHandle(sizeof(STRec_Internal));
    if (!hStyle) return NULL;

    stRec = (STRec_Internal*)*hStyle;

    /* Create style table with one default style */
    stRec->styleTab = NewHandle(sizeof(StyleTable));
    if (!stRec->styleTab) {
        DisposeHandle(hStyle);
        return NULL;
    }

    styleTab = (StyleTable*)*stRec->styleTab;
    styleTab->nStyles = 1;
    TECreateDefaultStyle(hTE, &styleTab->styles[0]);

    /* Create run array with one run covering entire text */
    stRec->runArray = NewHandle(sizeof(RunArray));
    if (!stRec->runArray) {
        DisposeHandle(stRec->styleTab);
        DisposeHandle(hStyle);
        return NULL;
    }

    runArr = (RunArray*)*stRec->runArray;
    runArr->nRuns = 1;
    runArr->runs[0].startChar = 0;
    runArr->runs[0].styleIndex = 0;  /* Index into style table */

    stRec->nRuns = 1;
    stRec->nStyles = 1;
    stRec->lineHeights = NULL;  /* Optional */

    return (STHandle)hStyle;
}

/* Find style in table or add it, return index */
static SInt16 TEFindOrAddStyle(STHandle hStyle, const TextStyle *style)
{
    extern OSErr SetHandleSize(Handle h, Size newSize);

    STRec_Internal* stRec;
    StyleTable* styleTab;
    SInt16 i;

    if (!hStyle || !*hStyle || !style) return 0;

    stRec = (STRec_Internal*)*hStyle;
    if (!stRec->styleTab || !*stRec->styleTab) return 0;

    styleTab = (StyleTable*)*stRec->styleTab;

    /* Search for existing style */
    for (i = 0; i < styleTab->nStyles; i++) {
        if (TECompareTextStyles(&styleTab->styles[i], style)) {
            return i;  /* Found existing style */
        }
    }

    /* Not found - add new style */
    Size newSize = sizeof(StyleTable) + (styleTab->nStyles) * sizeof(TextStyle);
    if (SetHandleSize(stRec->styleTab, newSize) != noErr) {
        return 0;  /* Out of memory */
    }

    /* Re-get pointer after resize */
    styleTab = (StyleTable*)*stRec->styleTab;

    /* Add new style */
    styleTab->styles[styleTab->nStyles] = *style;
    styleTab->nStyles++;
    stRec->nStyles = styleTab->nStyles;

    return styleTab->nStyles - 1;
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
    STRec_Internal* stRec;
    RunArray* runArr;
    StyleTable* styleTab;
    SInt16 i;

    if (!hTE || !*hTE || !theStyle) return;

    teRec = (TERec **)hTE;
    hStyle = TEGetStyleHandleInternal(hTE);

    if (hStyle && *hStyle) {
        stRec = (STRec_Internal*)*hStyle;

        /* Find run containing offset */
        if (stRec->runArray && *stRec->runArray) {
            runArr = (RunArray*)*stRec->runArray;

            /* Search runs to find which one contains offset */
            for (i = runArr->nRuns - 1; i >= 0; i--) {
                if (offset >= runArr->runs[i].startChar) {
                    /* Found the run */
                    SInt16 styleIndex = runArr->runs[i].styleIndex;

                    /* Get style from table */
                    if (stRec->styleTab && *stRec->styleTab) {
                        styleTab = (StyleTable*)*stRec->styleTab;
                        if (styleIndex >= 0 && styleIndex < styleTab->nStyles) {
                            *theStyle = styleTab->styles[styleIndex];

                            if (lineHeight) *lineHeight = (**teRec).lineHeight;
                            if (fontAscent) *fontAscent = (**teRec).fontAscent;
                            return;
                        }
                    }
                    break;
                }
            }
        }
    }

    /* Fallback: return current TERec style */
    TECreateDefaultStyle(hTE, theStyle);

    if (lineHeight) *lineHeight = (**teRec).lineHeight;
    if (fontAscent) *fontAscent = (**teRec).fontAscent;
}

pascal void TESetStyle(short mode, const TextStyle *newStyle, Boolean fRedraw,
                      TEHandle hTE)
{
    extern OSErr SetHandleSize(Handle h, Size newSize);

    TEExtPtr pTE;
    TERec **teRec;
    TEStyleHandle hStyle;
    TextStyle appliedStyle;
    SInt16 selStart, selEnd;
    STRec_Internal* stRec;
    RunArray* runArr;
    SInt16 newStyleIdx;
    SInt16 i;
    SInt16 startRunIdx, endRunIdx;

    if (!hTE || !*hTE || !newStyle) return;

    pTE = (TEExtPtr)*hTE;
    teRec = (TERec **)hTE;
    hStyle = TEGetStyleHandleInternal(hTE);

    /* Ensure we have a style record */
    if (!hStyle) {
        hStyle = TECreateStyleRec(hTE);
        if (!hStyle) return;
        pTE->hStyles = hStyle;
    }

    /* Get selection boundaries */
    selStart = (**teRec).selStart;
    selEnd = (**teRec).selEnd;

    if (selStart == selEnd) {
        /* No selection - update TERec for insertion point */
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
        return;
    }

    stRec = (STRec_Internal*)*hStyle;
    if (!stRec->runArray || !*stRec->runArray) return;

    runArr = (RunArray*)*stRec->runArray;

    /* Get style at selection start and merge with new style */
    TEGetStyle(selStart, &appliedStyle, NULL, NULL, hTE);

    if (mode & doFont) {
        appliedStyle.tsFont = newStyle->tsFont;
    }
    if (mode & doFace) {
        if (mode & doToggle) {
            appliedStyle.tsFace ^= newStyle->tsFace;
        } else {
            appliedStyle.tsFace = newStyle->tsFace;
        }
    }
    if (mode & doSize) {
        if (mode & addSize) {
            appliedStyle.tsSize += newStyle->tsSize;
            if (appliedStyle.tsSize < 1) appliedStyle.tsSize = 1;
            if (appliedStyle.tsSize > 255) appliedStyle.tsSize = 255;
        } else {
            appliedStyle.tsSize = newStyle->tsSize;
        }
    }
    if (mode & doColor) {
        appliedStyle.tsColor = newStyle->tsColor;
    }

    /* Find or add the new style to the style table */
    newStyleIdx = TEFindOrAddStyle(hStyle, &appliedStyle);

    /* Re-get runArr pointer in case style table resized */
    stRec = (STRec_Internal*)*hStyle;
    if (!stRec->runArray || !*stRec->runArray) return;
    runArr = (RunArray*)*stRec->runArray;

    /* Find runs that need modification */
    startRunIdx = -1;
    endRunIdx = -1;

    for (i = 0; i < runArr->nRuns; i++) {
        if (startRunIdx < 0 && runArr->runs[i].startChar >= selStart) {
            startRunIdx = i;
        }
        if (runArr->runs[i].startChar < selEnd) {
            endRunIdx = i;
        } else if (runArr->runs[i].startChar >= selEnd) {
            break;
        }
    }

    if (startRunIdx < 0) startRunIdx = 0;
    if (endRunIdx < 0) endRunIdx = runArr->nRuns - 1;

    /* If selection doesn't start at run boundary, split the run */
    if (startRunIdx < runArr->nRuns && runArr->runs[startRunIdx].startChar < selStart) {
        Size newSize = sizeof(RunArray) + (runArr->nRuns + 1) * sizeof(StyleRun);
        if (SetHandleSize(stRec->runArray, newSize) != noErr) return;

        stRec = (STRec_Internal*)*hStyle;
        runArr = (RunArray*)*stRec->runArray;

        /* Shift runs to make room */
        for (i = runArr->nRuns; i > startRunIdx + 1; i--) {
            runArr->runs[i] = runArr->runs[i - 1];
        }

        /* Create new run at selection start */
        runArr->runs[startRunIdx + 1].startChar = selStart;
        runArr->runs[startRunIdx + 1].styleIndex = runArr->runs[startRunIdx].styleIndex;
        runArr->nRuns++;
        startRunIdx++;
        endRunIdx++;
    }

    /* If selection doesn't end at run boundary, split the run */
    if (endRunIdx < runArr->nRuns - 1 ||
        (endRunIdx == runArr->nRuns - 1 &&
         endRunIdx < runArr->nRuns &&
         runArr->runs[endRunIdx].startChar < selEnd)) {

        /* Find the run that contains selEnd */
        for (i = 0; i < runArr->nRuns; i++) {
            if (i == runArr->nRuns - 1 || runArr->runs[i + 1].startChar > selEnd) {
                if (runArr->runs[i].startChar < selEnd) {
                    Size newSize = sizeof(RunArray) + (runArr->nRuns + 1) * sizeof(StyleRun);
                    if (SetHandleSize(stRec->runArray, newSize) != noErr) return;

                    stRec = (STRec_Internal*)*hStyle;
                    runArr = (RunArray*)*stRec->runArray;

                    /* Shift runs to make room */
                    for (SInt16 j = runArr->nRuns; j > i + 1; j--) {
                        runArr->runs[j] = runArr->runs[j - 1];
                    }

                    /* Create new run at selection end */
                    runArr->runs[i + 1].startChar = selEnd;
                    runArr->runs[i + 1].styleIndex = runArr->runs[i].styleIndex;
                    runArr->nRuns++;
                    endRunIdx++;
                }
                break;
            }
        }
    }

    /* Apply new style to all runs in selection range */
    for (i = startRunIdx; i <= endRunIdx && i < runArr->nRuns; i++) {
        if (runArr->runs[i].startChar >= selStart &&
            (i == runArr->nRuns - 1 || runArr->runs[i + 1].startChar <= selEnd)) {
            runArr->runs[i].styleIndex = newStyleIdx;
        }
    }

    /* Merge adjacent runs with same style */
    i = 0;
    while (i < runArr->nRuns - 1) {
        if (runArr->runs[i].styleIndex == runArr->runs[i + 1].styleIndex) {
            /* Remove run i+1 */
            for (SInt16 j = i + 1; j < runArr->nRuns - 1; j++) {
                runArr->runs[j] = runArr->runs[j + 1];
            }
            runArr->nRuns--;
            Size newSize = sizeof(RunArray) + runArr->nRuns * sizeof(StyleRun);
            SetHandleSize(stRec->runArray, newSize);
            if (MemError() != noErr) {
                break;  /* Stop merging if resize fails - array is still consistent */
            }
            /* Re-dereference after handle resize since memory may have moved */
            runArr = (RunArray *)*stRec->runArray;
        } else {
            i++;
        }
    }

    stRec->nRuns = runArr->nRuns;

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
    (style)->red = 0xFFFF;
    (style)->green = 0xFFFF;
    (style)->blue = 0xFFFF;
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
            (style1)->red == (style2)->red &&
            (style1)->green == (style2)->green &&
            (style1)->blue == (style2)->blue);
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
    (style)->red = 0x0000;
    (style)->green = 0x0000;
    (style)->blue = 0x0000;
    (style)->red = 0xFFFF;
    (style)->green = 0xFFFF;
    (style)->blue = 0xFFFF;
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
