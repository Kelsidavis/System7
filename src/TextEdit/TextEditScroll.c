/*
 * TextEditScroll.c - TextEdit Scrolling Support
 *
 * Handles text scrolling and visibility management
 */

#include "TextEdit/TextEdit.h"
#include "MemoryMgr/MemoryManager.h"
#include "QuickDraw/QuickDraw.h"

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
} TEExtRec;

typedef TEExtRec *TEExtPtr, **TEExtHandle;
#include <string.h>
#include "TextEdit/TELogging.h"

/* Debug logging */
#define TES_DEBUG 1

#if TES_DEBUG
#define TES_LOG(...) TE_LOG_DEBUG("TES: " __VA_ARGS__)
#else
#define TES_LOG(...)
#endif

/* Constants */
#define SCROLL_MARGIN   2       /* Lines to keep visible above/below */

/* Forward declarations */
static void TE_ScrollToLine(TEHandle hTE, SInt32 lineNum);
static void TE_ScrollToOffset(TEHandle hTE, SInt32 offset);
static SInt16 TE_GetVisibleLines(TEHandle hTE);
static Boolean TE_IsLineVisible(TEHandle hTE, SInt32 lineNum);

/* ============================================================================
 * Main Scrolling Functions
 * ============================================================================ */

/*
 * TEScroll - Scroll text by specified amount
 */
void TEScroll(SInt16 dh, SInt16 dv, TEHandle hTE) {
    TEExtPtr pTE;
    Rect updateRect;
    SInt16 maxScroll;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    TES_LOG("TEScroll: dh=%d, dv=%d, current=(%d,%d)\n",
            dh, dv, pTE->viewDH, pTE->viewDV);

    /* Calculate max scroll values */
    maxScroll = pTE->nLines * pTE->base.lineHeight -
                (pTE->base.viewRect.bottom - pTE->base.viewRect.top);
    if (maxScroll < 0) maxScroll = 0;

    /* Update scroll position */
    pTE->viewDH += dh;
    pTE->viewDV += dv;

    /* Clamp horizontal scroll */
    if (pTE->viewDH < 0) {
        pTE->viewDH = 0;
    }
    /* TODO: Calculate max horizontal scroll based on longest line */

    /* Clamp vertical scroll */
    if (pTE->viewDV < 0) {
        pTE->viewDV = 0;
    }
    if (pTE->viewDV > maxScroll) {
        pTE->viewDV = maxScroll;
    }

    /* Invalidate view rect for redraw */
    updateRect = pTE->base.viewRect;
    InvalRect(&updateRect);

    HUnlock((Handle)hTE);
}

/*
 * TESelView - Ensure selection is visible
 */
void TESelView(TEHandle hTE) {
    TEExtPtr pTE;
    SInt32 selLine;
    SInt16 visibleLines;
    SInt16 scrollDV;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Get line containing selection end */
    selLine = TE_OffsetToLine(hTE, pTE->base.selEnd);

    TES_LOG("TESelView: selEnd=%d, line=%d\n", pTE->base.selEnd, selLine);

    /* Check if line is visible */
    if (!TE_IsLineVisible(hTE, selLine)) {
        /* Calculate scroll needed */
        visibleLines = TE_GetVisibleLines(hTE);

        if (selLine < pTE->viewDV / pTE->base.lineHeight) {
            /* Selection above view - scroll up */
            scrollDV = selLine * pTE->base.lineHeight;
        } else {
            /* Selection below view - scroll down */
            scrollDV = (selLine - visibleLines + 1 + SCROLL_MARGIN) * pTE->base.lineHeight;
        }

        /* Apply scroll */
        TEScroll(0, scrollDV - pTE->viewDV, hTE);
    }

    /* Also ensure horizontal visibility if no word wrap */
    if (!pTE->wordWrap) {
        Point selPt = TEGetPoint(pTE->base.selEnd, hTE);

        if (selPt.h < pTE->base.viewRect.left) {
            /* Scroll left */
            TEScroll(pTE->base.viewRect.left - selPt.h - 10, 0, hTE);
        } else if (selPt.h > pTE->base.viewRect.right) {
            /* Scroll right */
            TEScroll(selPt.h - pTE->base.viewRect.right + 10, 0, hTE);
        }
    }

    HUnlock((Handle)hTE);
}

/*
 * TEPinScroll - Scroll with pinning (limit to content)
 */
void TEPinScroll(SInt16 dh, SInt16 dv, TEHandle hTE) {
    TEExtPtr pTE;
    SInt16 maxVScroll, maxHScroll;
    SInt16 newDH, newDV;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    TES_LOG("TEPinScroll: dh=%d, dv=%d\n", dh, dv);

    /* Calculate maximum scroll values */
    maxVScroll = pTE->nLines * pTE->base.lineHeight -
                 (pTE->base.viewRect.bottom - pTE->base.viewRect.top);
    if (maxVScroll < 0) maxVScroll = 0;

    /* TODO: Calculate max horizontal scroll */
    maxHScroll = 0;

    /* Calculate new positions */
    newDH = pTE->viewDH + dh;
    newDV = pTE->viewDV + dv;

    /* Pin to valid range */
    if (newDH < 0) newDH = 0;
    if (newDH > maxHScroll) newDH = maxHScroll;
    if (newDV < 0) newDV = 0;
    if (newDV > maxVScroll) newDV = maxVScroll;

    /* Apply pinned scroll */
    TEScroll(newDH - pTE->viewDH, newDV - pTE->viewDV, hTE);

    HUnlock((Handle)hTE);
}

/*
 * TEAutoView - Enable/disable auto-scrolling
 */
void TEAutoView(Boolean autoView, TEHandle hTE) {
    TEExtPtr pTE;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    TES_LOG("TEAutoView: autoView=%d\n", autoView);

    /* Set auto-scroll flag */
    /* Note: This is handled in TEIdle and TEClick */
    /* For now, just log it */

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Layout Recalculation
 * ============================================================================ */

/*
 * TECalText - Recalculate text layout
 */
void TECalText(TEHandle hTE) {
    TEExtPtr pTE;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    TES_LOG("TECalText: recalculating layout\n");

    /* Mark for recalc */
    pTE->dirty = TRUE;

    /* Recalculate line breaks */
    TE_RecalcLines(hTE);

    /* Reset dirty flag */
    pTE->dirty = FALSE;

    /* Ensure selection is visible */
    TESelView(hTE);

    /* Invalidate for redraw */
    InvalRect(&pTE->base.viewRect);

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/*
 * TE_ScrollToLine - Scroll to make line visible
 */
static void TE_ScrollToLine(TEHandle hTE, SInt32 lineNum) {
    TEExtPtr pTE;
    SInt16 lineTop;
    SInt16 viewHeight;
    SInt16 scrollDV;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Calculate line position */
    lineTop = lineNum * pTE->base.lineHeight;
    viewHeight = pTE->base.viewRect.bottom - pTE->base.viewRect.top;

    /* Check if scrolling needed */
    if (lineTop < pTE->viewDV) {
        /* Line above view */
        scrollDV = lineTop;
    } else if (lineTop + pTE->base.lineHeight > pTE->viewDV + viewHeight) {
        /* Line below view */
        scrollDV = lineTop + pTE->base.lineHeight - viewHeight;
    } else {
        /* Already visible */
        HUnlock((Handle)hTE);
        return;
    }

    /* Apply scroll */
    TEScroll(0, scrollDV - pTE->viewDV, hTE);

    HUnlock((Handle)hTE);
}

/*
 * TE_ScrollToOffset - Scroll to make text offset visible
 */
static void TE_ScrollToOffset(TEHandle hTE, SInt32 offset) {
    SInt32 lineNum;

    /* Get line containing offset */
    lineNum = TE_OffsetToLine(hTE, offset);

    /* Scroll to line */
    TE_ScrollToLine(hTE, lineNum);
}

/*
 * TE_GetVisibleLines - Get number of visible lines
 */
static SInt16 TE_GetVisibleLines(TEHandle hTE) {
    TEExtPtr pTE;
    SInt16 viewHeight;
    SInt16 visibleLines;

    if (!hTE) return 0;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    viewHeight = pTE->base.viewRect.bottom - pTE->base.viewRect.top;
    visibleLines = viewHeight / pTE->base.lineHeight;
    if (viewHeight % pTE->base.lineHeight) {
        visibleLines++;  /* Partial line counts */
    }

    HUnlock((Handle)hTE);

    return visibleLines;
}

/*
 * TE_IsLineVisible - Check if line is visible
 */
static Boolean TE_IsLineVisible(TEHandle hTE, SInt32 lineNum) {
    TEExtPtr pTE;
    SInt16 lineTop, lineBottom;
    SInt16 viewTop, viewBottom;

    if (!hTE) return FALSE;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Calculate line bounds */
    lineTop = lineNum * pTE->base.lineHeight;
    lineBottom = lineTop + pTE->base.lineHeight;

    /* Calculate view bounds */
    viewTop = pTE->viewDV;
    viewBottom = pTE->viewDV + (pTE->base.viewRect.bottom - pTE->base.viewRect.top);

    HUnlock((Handle)hTE);

    /* Check overlap */
    return (lineBottom > viewTop && lineTop < viewBottom);
}

/* ============================================================================
 * Line Offset Calculations
 * ============================================================================ */

/*
 * TE_OffsetToLine - Convert text offset to line number
 */
SInt32 TE_OffsetToLine(TEHandle hTE, SInt32 offset) {
    TEExtPtr pTE;
    SInt32 lineNum;
    SInt32 *pLines;

    if (!hTE) return 0;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Clamp offset */
    if (offset < 0) offset = 0;
    if (offset > pTE->base.teLength) offset = pTE->base.teLength;

    /* Binary search for line containing offset */
    HLock(pTE->hLines);
    pLines = (SInt32*)*pTE->hLines;

    lineNum = 0;
    for (SInt32 i = 0; i < pTE->nLines; i++) {
        if (i + 1 < pTE->nLines) {
            if (offset >= pLines[i] && offset < pLines[i + 1]) {
                lineNum = i;
                break;
            }
        } else {
            /* Last line */
            if (offset >= pLines[i]) {
                lineNum = i;
            }
        }
    }

    HUnlock(pTE->hLines);
    HUnlock((Handle)hTE);

    TES_LOG("TE_OffsetToLine: offset %d -> line %d\n", offset, lineNum);

    return lineNum;
}

/*
 * TE_LineToOffset - Convert line number to text offset
 */
SInt32 TE_LineToOffset(TEHandle hTE, SInt32 line) {
    TEExtPtr pTE;
    SInt32 offset;
    SInt32 *pLines;

    if (!hTE || line < 0) return 0;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Clamp line number */
    if (line >= pTE->nLines) {
        offset = pTE->base.teLength;
    } else {
        HLock(pTE->hLines);
        pLines = (SInt32*)*pTE->hLines;
        offset = pLines[line];
        HUnlock(pTE->hLines);
    }

    HUnlock((Handle)hTE);

    TES_LOG("TE_LineToOffset: line %d -> offset %d\n", line, offset);

    return offset;
}