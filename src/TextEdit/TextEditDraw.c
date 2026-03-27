/*
 * TextEditDraw.c - TextEdit Drawing and Display
 *
 * Handles text rendering, selection highlighting, and caret blinking
 */

#include "TextEdit/TextEdit.h"
#include "MemoryMgr/MemoryManager.h"
#include "FontManager/FontManager.h"
#include "QuickDraw/QuickDraw.h"
#include "EventManager/EventManager.h"
#include <string.h>
#include "TextEdit/TELogging.h"

/* Boolean constants */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Extended TERec with additional fields - must match TextEdit.c */
typedef struct TEExtRec {
    TERec       base;           /* Standard TERec */
    Handle      hLines;         /* Line starts array */
    SInt16      nLines;         /* Number of lines */
    Handle      hStyles;        /* Style record handle */
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

/* Debug logging */
#define TED_DEBUG 1

#if TED_DEBUG
#define TED_LOG(...) TE_LOG_DEBUG("TED: " __VA_ARGS__)
#else
#define TED_LOG(...)
#endif

/* Constants */
#define CARET_WIDTH     1
#define CARET_BLINK     30      /* Ticks between blinks */

/* External functions */
extern UInt32 TickCount(void);
extern GrafPtr g_currentPort;
extern void InvertRect(const Rect *r);
extern void EraseRect(const Rect *r);
extern void InvalRect(const Rect *r);
extern void DrawText(const void *text, SInt16 firstByte, SInt16 byteCount);

/* Style structures - must match TextFormatting.c */
typedef struct StyleTable {
    SInt16      nStyles;
    TextStyle   styles[1];
} StyleTable;

typedef struct RunArray {
    SInt16      nRuns;
    StyleRun    runs[1];
} RunArray;

typedef struct STRec_Internal {
    SInt16      nRuns;
    SInt16      nStyles;
    Handle      styleTab;
    Handle      runArray;
    Handle      lineHeights;
} STRec_Internal;

/* Forward declarations */
static void TE_DrawLineSegment(TEHandle hTE, SInt32 start, SInt32 end,
                               SInt16 x, SInt16 y, Boolean selected);
static void TE_DrawStyledSegment(TEHandle hTE, SInt32 start, SInt32 end,
                                 SInt16 x, SInt16 y, Boolean selected);
static SInt16 TE_MeasureText(TEHandle hTE, SInt32 start, SInt32 length);

/* ============================================================================
 * Main Drawing Functions
 * ============================================================================ */

/*
 * TEUpdate - Update text display
 */
void TEUpdate(const Rect *updateRect, TEHandle hTE) {
    TEExtPtr pTE;
    Rect clipRect;
    SInt32 lineNum;
    SInt16 y;
    GrafPtr savedPort;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    TED_LOG("TEUpdate: updating rect (%d,%d,%d,%d)\n",
            updateRect->top, updateRect->left,
            updateRect->bottom, updateRect->right);

    /* Save and set port */
    GetPort(&savedPort);
    SetPort(pTE->base.inPort);

    /* Set font */
    TextFont(pTE->base.txFont);
    TextSize(pTE->base.txSize);
    TextFace(pTE->base.txFace);

    /* Calculate clip rect */
    if (updateRect) {
        SectRect(&pTE->base.viewRect, updateRect, &clipRect);
    } else {
        clipRect = pTE->base.viewRect;
    }

    /* Erase background */
    EraseRect(&clipRect);

    /* Draw each visible line */
    y = pTE->base.viewRect.top - pTE->viewDV + pTE->base.fontAscent;

    for (lineNum = 0; lineNum < pTE->nLines; lineNum++) {
        /* Check if line is visible */
        if (y + pTE->base.lineHeight < clipRect.top) {
            y += pTE->base.lineHeight;
            continue;
        }
        if (y - pTE->base.fontAscent > clipRect.bottom) {
            break;
        }

        /* Draw the line */
        TE_DrawLine(hTE, lineNum, y);

        y += pTE->base.lineHeight;
    }

    /* Draw caret if active and no selection */
    if (pTE->base.active && pTE->base.selStart == pTE->base.selEnd) {
        TE_UpdateCaret(hTE, FALSE);
    }

    /* Restore port */
    SetPort(savedPort);

    HUnlock((Handle)hTE);
}

/*
 * TETextBox - Draw text in a box
 */
void TETextBox(const void *text, SInt32 length, const Rect *box, SInt16 just) {
    TEHandle hTE;
    Rect destRect, viewRect;

    TED_LOG("TETextBox: %d bytes, just=%d\n", length, just);

    /* Create temporary TE record */
    destRect = *box;
    viewRect = *box;

    hTE = TENew(&destRect, &viewRect);
    if (!hTE) return;

    /* Set text and justification */
    TESetText(text, length, hTE);
    TESetJust(just, hTE);

    /* Draw it */
    TEUpdate(&viewRect, hTE);

    /* Clean up */
    TEDispose(hTE);
}

/*
 * TE_DrawLine - Draw a single line of text
 */
void TE_DrawLine(TEHandle hTE, SInt32 lineNum, SInt16 y) {
    TEExtPtr pTE;
    char *pText;
    SInt32 lineStart, lineEnd;
    SInt32 selStart, selEnd;
    SInt16 x;
    SInt32 *pLines;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Get line boundaries */
    HLock(pTE->hLines);
    pLines = (SInt32*)*pTE->hLines;
    lineStart = pLines[lineNum];
    lineEnd = (lineNum + 1 < pTE->nLines) ? pLines[lineNum + 1] : pTE->base.teLength;
    HUnlock(pTE->hLines);

    /* Remove trailing CR if present */
    if (lineEnd > lineStart) {
        HLock(pTE->base.hText);
        pText = *pTE->base.hText;
        if (pText[lineEnd - 1] == '\r') {
            lineEnd--;
        }
        HUnlock(pTE->base.hText);
    }

    TED_LOG("TE_DrawLine: line %d [%d,%d) at y=%d\n",
            lineNum, lineStart, lineEnd, y);

    /* Calculate starting X based on justification */
    x = pTE->base.viewRect.left - pTE->viewDH;

    if (pTE->base.just != teJustLeft && lineEnd > lineStart) {
        SInt16 lineWidth = TE_MeasureText(hTE, lineStart, lineEnd - lineStart);
        SInt16 viewWidth = pTE->base.viewRect.right - pTE->base.viewRect.left;

        if (pTE->base.just == teJustCenter) {
            x += (viewWidth - lineWidth) / 2;
        } else if (pTE->base.just == teJustRight) {
            x += viewWidth - lineWidth;
        }
    }

    /* Get selection range for this line */
    selStart = pTE->base.selStart;
    selEnd = pTE->base.selEnd;

    /* Draw the line with selection handling */
    if (selEnd > selStart && selEnd > lineStart && selStart < lineEnd) {
        /* Line has selection */
        if (selStart <= lineStart && selEnd >= lineEnd) {
            /* Entire line selected */
            TE_DrawLineSegment(hTE, lineStart, lineEnd, x, y, TRUE);
        } else if (selStart > lineStart && selEnd < lineEnd) {
            /* Selection in middle */
            SInt16 segX = x;

            /* Draw before selection */
            TE_DrawLineSegment(hTE, lineStart, selStart, segX, y, FALSE);
            segX += TE_MeasureText(hTE, lineStart, selStart - lineStart);

            /* Draw selection */
            TE_DrawLineSegment(hTE, selStart, selEnd, segX, y, TRUE);
            segX += TE_MeasureText(hTE, selStart, selEnd - selStart);

            /* Draw after selection */
            TE_DrawLineSegment(hTE, selEnd, lineEnd, segX, y, FALSE);
        } else if (selStart <= lineStart) {
            /* Selection starts before line */
            SInt16 segX = x;

            /* Draw selection */
            TE_DrawLineSegment(hTE, lineStart, selEnd, segX, y, TRUE);
            segX += TE_MeasureText(hTE, lineStart, selEnd - lineStart);

            /* Draw after selection */
            TE_DrawLineSegment(hTE, selEnd, lineEnd, segX, y, FALSE);
        } else {
            /* Selection starts in line */
            SInt16 segX = x;

            /* Draw before selection */
            TE_DrawLineSegment(hTE, lineStart, selStart, segX, y, FALSE);
            segX += TE_MeasureText(hTE, lineStart, selStart - lineStart);

            /* Draw selection */
            TE_DrawLineSegment(hTE, selStart, lineEnd, segX, y, TRUE);
        }
    } else {
        /* No selection in line */
        TE_DrawLineSegment(hTE, lineStart, lineEnd, x, y, FALSE);
    }

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Selection Drawing
 * ============================================================================ */

/*
 * TE_InvalidateSelection - Invalidate selection area for redraw
 */
void TE_InvalidateSelection(TEHandle hTE) {
    TEExtPtr pTE;
    Rect selRect;
    SInt32 startLine, endLine;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    if (pTE->base.selStart == pTE->base.selEnd) {
        /* Just caret - invalidate caret position */
        Point caretPt = TEGetPoint(pTE->base.selStart, hTE);
        SetRect(&selRect, caretPt.h - 1, caretPt.v - pTE->base.fontAscent,
                caretPt.h + 2, caretPt.v + pTE->base.lineHeight - pTE->base.fontAscent);
    } else {
        /* Get line range */
        startLine = TE_OffsetToLine(hTE, pTE->base.selStart);
        endLine = TE_OffsetToLine(hTE, pTE->base.selEnd);

        /* Build rect covering all lines */
        selRect.left = pTE->base.viewRect.left;
        selRect.right = pTE->base.viewRect.right;
        selRect.top = pTE->base.viewRect.top + startLine * pTE->base.lineHeight - pTE->viewDV;
        selRect.bottom = selRect.top + (endLine - startLine + 1) * pTE->base.lineHeight;
    }

    /* Clip to view rect */
    SectRect(&selRect, &pTE->base.viewRect, &selRect);

    /* Invalidate */
    if (!EmptyRect(&selRect)) {
        InvalRect(&selRect);
    }

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Caret Management
 * ============================================================================ */

/*
 * TEIdle - Handle idle time (caret blinking, autoscroll)
 */
void TEIdle(TEHandle hTE) {
    TEExtPtr pTE;
    UInt32 currentTick;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Only process if active */
    if (!pTE->base.active) {
        HUnlock((Handle)hTE);
        return;
    }

    currentTick = TickCount();

    /* Handle caret blinking */
    if (pTE->base.selStart == pTE->base.selEnd) {
        /* Check if time to blink */
        if (currentTick - pTE->base.caretTime >= CARET_BLINK) {
            pTE->base.caretTime = currentTick;
            pTE->base.caretState = pTE->base.caretState ? 0 : 0xFF;
            TE_UpdateCaret(hTE, FALSE);
        }
    }

    /* Handle autoscroll during drag selection */
    if (pTE->inDragSel) {
        Point mousePt;
        Rect viewRect = pTE->base.viewRect;
        SInt16 autoscrollDistance = 4;  /* Pixels per autoscroll */

        GetMouse(&mousePt);

        /* Check if mouse is near view edges - autoscroll if so */
        if (mousePt.h < viewRect.left + 16) {
            /* Near left edge - scroll left */
            TEScroll(-autoscrollDistance, 0, hTE);
            TED_LOG("TEIdle: autoscroll left\n");
        } else if (mousePt.h > viewRect.right - 16) {
            /* Near right edge - scroll right */
            TEScroll(autoscrollDistance, 0, hTE);
            TED_LOG("TEIdle: autoscroll right\n");
        }

        if (mousePt.v < viewRect.top + 16) {
            /* Near top edge - scroll up */
            TEScroll(0, -autoscrollDistance, hTE);
            TED_LOG("TEIdle: autoscroll up\n");
        } else if (mousePt.v > viewRect.bottom - 16) {
            /* Near bottom edge - scroll down */
            TEScroll(0, autoscrollDistance, hTE);
            TED_LOG("TEIdle: autoscroll down\n");
        }
    }

    HUnlock((Handle)hTE);
}

/*
 * TE_UpdateCaret - Draw or erase caret
 */
void TE_UpdateCaret(TEHandle hTE, Boolean forceOn) {
    TEExtPtr pTE;
    Point caretPt;
    Rect caretRect;
    GrafPtr savedPort;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Only draw if active and no selection */
    if (!pTE->base.active || pTE->base.selStart != pTE->base.selEnd) {
        HUnlock((Handle)hTE);
        return;
    }

    TED_LOG("TE_UpdateCaret: state=%d, force=%d\n", pTE->base.caretState, forceOn);

    /* Get caret position */
    caretPt = TEGetPoint(pTE->base.selStart, hTE);

    /* Build caret rect */
    SetRect(&caretRect,
            caretPt.h,
            caretPt.v - pTE->base.fontAscent,
            caretPt.h + CARET_WIDTH,
            caretPt.v + (pTE->base.lineHeight - pTE->base.fontAscent));

    /* Clip to view rect */
    SectRect(&caretRect, &pTE->base.viewRect, &caretRect);

    if (!EmptyRect(&caretRect)) {
        /* Save and set port */
        GetPort(&savedPort);
        SetPort(pTE->base.inPort);

        /* Draw/erase caret */
        if (forceOn) {
            pTE->base.caretState = 0xFF;
            InvertRect(&caretRect);
        } else if (pTE->base.caretState) {
            InvertRect(&caretRect);
        }

        /* Restore port */
        SetPort(savedPort);
    }

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Position Calculations
 * ============================================================================ */

/*
 * TEGetPoint - Get screen position for text offset
 */
Point TEGetPoint(SInt16 offset, TEHandle hTE) {
    TEExtPtr pTE;
    Point pt;
    SInt32 lineNum;
    SInt32 lineStart;
    SInt32 *pLines;

    if (!hTE) {
        pt.h = pt.v = 0;
        return pt;
    }

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Clamp offset */
    if (offset < 0) offset = 0;
    if (offset > pTE->base.teLength) offset = pTE->base.teLength;

    /* Find line containing offset */
    lineNum = TE_OffsetToLine(hTE, offset);

    /* Get line start */
    HLock(pTE->hLines);
    pLines = (SInt32*)*pTE->hLines;
    lineStart = pLines[lineNum];
    HUnlock(pTE->hLines);

    /* Calculate vertical position */
    pt.v = pTE->base.viewRect.top + lineNum * pTE->base.lineHeight - pTE->viewDV + pTE->base.fontAscent;

    /* Calculate horizontal position */
    pt.h = pTE->base.viewRect.left - pTE->viewDH;

    /* Add width of text before offset */
    if (offset > lineStart) {
        pt.h += TE_MeasureText(hTE, lineStart, offset - lineStart);
    }

    TED_LOG("TEGetPoint: offset %d -> (%d,%d)\n", offset, pt.h, pt.v);

    HUnlock((Handle)hTE);

    return pt;
}

/*
 * TEGetOffset - Get text offset for screen position
 */
SInt16 TEGetOffset(Point pt, TEHandle hTE) {
    TEExtPtr pTE;
    SInt32 lineNum;
    SInt32 lineStart, lineEnd;
    SInt32 *pLines;
    SInt16 x;
    SInt32 offset;
    char *pText;

    if (!hTE) return 0;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Guard against divide-by-zero: lineHeight must be positive */
    if (pTE->base.lineHeight <= 0) {
        HUnlock((Handle)hTE);
        return 0;
    }

    /* Find line at y position */
    lineNum = (pt.v - pTE->base.viewRect.top + pTE->viewDV) / pTE->base.lineHeight;
    if (lineNum < 0) lineNum = 0;
    if (lineNum >= pTE->nLines) lineNum = pTE->nLines - 1;

    /* Get line boundaries */
    HLock(pTE->hLines);
    pLines = (SInt32*)*pTE->hLines;
    lineStart = pLines[lineNum];
    lineEnd = (lineNum + 1 < pTE->nLines) ? pLines[lineNum + 1] : pTE->base.teLength;
    HUnlock(pTE->hLines);

    /* Find character at x position */
    x = pTE->base.viewRect.left - pTE->viewDH;
    offset = lineStart;

    HLock(pTE->base.hText);
    pText = *pTE->base.hText;

    while (offset < lineEnd) {
        /* Skip CR at end */
        if (pText[offset] == '\r') {
            break;
        }

        /* Measure next character */
        SInt16 charWidth = CharWidth(pText[offset]);
        if (charWidth <= 0) charWidth = 1;  /* Prevent zero/negative width */

        /* Check if we've passed the point */
        if (x + charWidth/2 > pt.h) {
            break;
        }

        x += charWidth;
        offset++;
    }

    HUnlock(pTE->base.hText);

    TED_LOG("TEGetOffset: (%d,%d) -> %d\n", pt.h, pt.v, offset);

    HUnlock((Handle)hTE);

    return offset;
}

/* ============================================================================
 * Internal Drawing Functions
 * ============================================================================ */

/*
 * TE_DrawLineSegment - Draw a segment of text
 */
/*
 * TE_DrawStyledSegment - Draw text segment with multiple style runs
 */
static void TE_DrawStyledSegment(TEHandle hTE, SInt32 start, SInt32 end,
                                 SInt16 x, SInt16 y, Boolean selected) {
    TEExtPtr pTE;
    STRec_Internal* stRec;
    RunArray* runArr;
    StyleTable* styleTab;
    char *pText;
    SInt32 pos, nextPos;
    SInt16 currentX;
    SInt16 i;
    Rect selRect;

    if (start >= end) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    if (!pTE->hStyles || !*pTE->hStyles) {
        /* No styles - draw plain */
        HUnlock((Handle)hTE);
        TE_DrawLineSegment(hTE, start, end, x, y, selected);
        return;
    }

    stRec = (STRec_Internal*)*pTE->hStyles;
    if (!stRec->runArray || !*stRec->runArray ||
        !stRec->styleTab || !*stRec->styleTab) {
        /* Invalid style record */
        HUnlock((Handle)hTE);
        TE_DrawLineSegment(hTE, start, end, x, y, selected);
        return;
    }

    runArr = (RunArray*)*stRec->runArray;
    styleTab = (StyleTable*)*stRec->styleTab;

    HLock(pTE->base.hText);
    pText = *pTE->base.hText;

    /* Draw selection background for entire segment if needed */
    if (selected) {
        SInt16 width = TE_MeasureText(hTE, start, end - start);
        SetRect(&selRect, x, y - pTE->base.fontAscent,
                x + width, y + pTE->base.lineHeight - pTE->base.fontAscent);
        SectRect(&selRect, &pTE->base.viewRect, &selRect);
        if (!EmptyRect(&selRect)) {
            InvertRect(&selRect);
        }
    }

    /* Draw text in style runs */
    pos = start;
    currentX = x;

    while (pos < end) {
        /* Find run containing pos */
        SInt16 styleIndex = 0;
        nextPos = end;

        for (i = runArr->nRuns - 1; i >= 0; i--) {
            if (pos >= runArr->runs[i].startChar) {
                styleIndex = runArr->runs[i].styleIndex;

                /* Find next run boundary */
                if (i + 1 < runArr->nRuns) {
                    if (runArr->runs[i + 1].startChar < end) {
                        nextPos = runArr->runs[i + 1].startChar;
                    }
                }
                break;
            }
        }

        /* Ensure we don't go past end */
        if (nextPos > end) {
            nextPos = end;
        }

        /* Apply style for this run */
        if (styleIndex >= 0 && styleIndex < styleTab->nStyles) {
            TextStyle* style = &styleTab->styles[styleIndex];
            TextFont(style->tsFont);
            TextSize(style->tsSize);
            TextFace(style->tsFace);
            /* Color would be applied here if supported */
        }

        /* Draw this run */
        MoveTo(currentX, y);
        DrawText(pText, pos, nextPos - pos);

        /* Measure width for next segment */
        SInt16 segWidth = TextWidth(pText, pos, nextPos - pos);
        currentX += segWidth;

        pos = nextPos;
    }

    HUnlock(pTE->base.hText);
    HUnlock((Handle)hTE);
}

static void TE_DrawLineSegment(TEHandle hTE, SInt32 start, SInt32 end,
                               SInt16 x, SInt16 y, Boolean selected) {
    TEExtPtr pTE;
    char *pText;
    Rect selRect;

    if (start >= end) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Check if we have styles - if so, use styled drawing */
    if (pTE->hStyles && *pTE->hStyles) {
        HUnlock((Handle)hTE);
        TE_DrawStyledSegment(hTE, start, end, x, y, selected);
        return;
    }

    HLock(pTE->base.hText);
    pText = *pTE->base.hText;

    /* Draw selection background if needed */
    if (selected) {
        SInt16 width = TE_MeasureText(hTE, start, end - start);
        SetRect(&selRect, x, y - pTE->base.fontAscent,
                x + width, y + pTE->base.lineHeight - pTE->base.fontAscent);

        /* Clip to view rect */
        SectRect(&selRect, &pTE->base.viewRect, &selRect);

        if (!EmptyRect(&selRect)) {
            /* Use inverse for selection (classic Mac style) */
            InvertRect(&selRect);
        }
    }

    /* Draw text */
    MoveTo(x, y);
    DrawText(pText, start, end - start);

    HUnlock(pTE->base.hText);
    HUnlock((Handle)hTE);
}

/*
 * TE_MeasureText - Measure text width (handles styled text)
 */
static SInt16 TE_MeasureText(TEHandle hTE, SInt32 start, SInt32 length) {
    TEExtPtr pTE;
    STRec_Internal* stRec;
    RunArray* runArr;
    StyleTable* styleTab;
    char *pText;
    SInt16 width;
    SInt32 pos, end, nextPos;
    SInt16 i;

    if (length <= 0) return 0;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    end = start + length;

    /* Check if we have styles */
    if (!pTE->hStyles || !*pTE->hStyles) {
        /* Plain text - measure with current font */
        TextFont(pTE->base.txFont);
        TextSize(pTE->base.txSize);
        TextFace(pTE->base.txFace);

        HLock(pTE->base.hText);
        pText = *pTE->base.hText;
        width = TextWidth(pText, start, length);
        HUnlock(pTE->base.hText);
        HUnlock((Handle)hTE);
        return width;
    }

    stRec = (STRec_Internal*)*pTE->hStyles;
    if (!stRec->runArray || !*stRec->runArray ||
        !stRec->styleTab || !*stRec->styleTab) {
        /* Invalid style record - use plain measurement */
        TextFont(pTE->base.txFont);
        TextSize(pTE->base.txSize);
        TextFace(pTE->base.txFace);

        HLock(pTE->base.hText);
        pText = *pTE->base.hText;
        width = TextWidth(pText, start, length);
        HUnlock(pTE->base.hText);
        HUnlock((Handle)hTE);
        return width;
    }

    runArr = (RunArray*)*stRec->runArray;
    styleTab = (StyleTable*)*stRec->styleTab;

    HLock(pTE->base.hText);
    pText = *pTE->base.hText;

    /* Measure text across style runs */
    width = 0;
    pos = start;

    while (pos < end) {
        /* Find run containing pos */
        SInt16 styleIndex = 0;
        nextPos = end;

        for (i = runArr->nRuns - 1; i >= 0; i--) {
            if (pos >= runArr->runs[i].startChar) {
                styleIndex = runArr->runs[i].styleIndex;

                /* Find next run boundary */
                if (i + 1 < runArr->nRuns) {
                    if (runArr->runs[i + 1].startChar < end) {
                        nextPos = runArr->runs[i + 1].startChar;
                    }
                }
                break;
            }
        }

        /* Ensure we don't go past end */
        if (nextPos > end) {
            nextPos = end;
        }

        /* Apply style and measure */
        if (styleIndex >= 0 && styleIndex < styleTab->nStyles) {
            TextStyle* style = &styleTab->styles[styleIndex];
            TextFont(style->tsFont);
            TextSize(style->tsSize);
            TextFace(style->tsFace);
        }

        /* Measure this segment */
        width += TextWidth(pText, pos, nextPos - pos);

        pos = nextPos;
    }

    HUnlock(pTE->base.hText);
    HUnlock((Handle)hTE);
    return width;
}
