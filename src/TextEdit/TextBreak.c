/*
 * TextBreak.c - TextEdit Line Breaking and Word Wrap
 *
 * Handles word wrapping, line breaking, and tab stops
 */

#include "TextEdit/TextEdit.h"
#include "MemoryMgr/MemoryManager.h"
#include "FontManager/FontManager.h"
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
#define TEB_DEBUG 1

#if TEB_DEBUG
#define TEB_LOG(...) TE_LOG_DEBUG("TEB: " __VA_ARGS__)
#else
#define TEB_LOG(...)
#endif

/* Constants */
#define TAB_WIDTH       8       /* Default tab width in characters */
#define MAX_LINES       2048    /* Maximum lines */

/* Forward declarations */
static SInt32 TE_FindBreakPoint(TEHandle hTE, SInt32 start, SInt32 end, SInt16 maxWidth);
static Boolean TE_IsBreakChar(char ch);
static SInt16 TE_GetTabStop(TEHandle hTE, SInt16 currentX);

/* ============================================================================
 * Main Line Breaking Function
 * ============================================================================ */

/*
 * TE_RecalcLines - Recalculate line breaks for entire text
 */
void TE_RecalcLines(TEHandle hTE) {
    TEExtPtr pTE;
    char *pText;
    SInt32 *pLines;
    SInt32 textPos, lineStart;
    SInt32 lineNum;
    SInt16 maxWidth;
    SInt32 breakPos;
    Handle newLines;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    TEB_LOG("TE_RecalcLines: recalculating for %d bytes\n", pTE->base.teLength);

    /* Calculate maximum line width */
    if (pTE->wordWrap) {
        maxWidth = pTE->base.destRect.right - pTE->base.destRect.left;
    } else {
        maxWidth = 32767;  /* No wrap - very wide */
    }

    /* Allocate new line starts array */
    newLines = NewHandle(sizeof(SInt32) * MAX_LINES);
    if (!newLines) {
        HUnlock((Handle)hTE);
        return;
    }

    HLock(newLines);
    pLines = (SInt32*)*newLines;

    /* Set font for measurement */
    TextFont(pTE->base.txFont);
    TextSize(pTE->base.txSize);
    TextFace(pTE->base.txFace);

    /* Process text to find line breaks */
    HLock(pTE->base.hText);
    pText = *pTE->base.hText;

    textPos = 0;
    lineNum = 0;
    pLines[lineNum++] = 0;  /* First line starts at 0 */

    while (textPos < pTE->base.teLength && lineNum < MAX_LINES) {
        lineStart = textPos;

        /* Find next CR or line break point */
        breakPos = textPos;
        while (breakPos < pTE->base.teLength && pText[breakPos] != '\r') {
            breakPos++;
        }

        /* Check if we need to wrap before CR */
        if (pTE->wordWrap) {
            SInt32 wrapPos = TE_FindBreakPoint(hTE, lineStart,
                                               breakPos, maxWidth);
            if (wrapPos < breakPos && wrapPos > lineStart) {
                breakPos = wrapPos;
            }
        }

        /* Move to next position */
        textPos = breakPos;

        /* Skip CR if present */
        if (textPos < pTE->base.teLength && pText[textPos] == '\r') {
            textPos++;
        }

        /* Add line start if not at end */
        if (textPos < pTE->base.teLength) {
            pLines[lineNum++] = textPos;
        }
    }

    HUnlock(pTE->base.hText);
    HUnlock(newLines);

    /* Update TE record */
    if (pTE->hLines) {
        DisposeHandle(pTE->hLines);
    }
    pTE->hLines = newLines;
    pTE->nLines = lineNum;

    TEB_LOG("TE_RecalcLines: found %d lines\n", lineNum);

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Break Point Finding
 * ============================================================================ */

/*
 * TE_FindBreakPoint - Find best break point for line
 */
static SInt32 TE_FindBreakPoint(TEHandle hTE, SInt32 start, SInt32 end, SInt16 maxWidth) {
    TEExtPtr pTE;
    char *pText;
    SInt32 pos, lastBreak;
    SInt16 width;
    SInt16 charWidth;

    if (start >= end) return end;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    HLock(pTE->base.hText);
    pText = *pTE->base.hText;

    /* Measure characters until we exceed width */
    width = 0;
    pos = start;
    lastBreak = start;

    while (pos < end) {
        /* Check for explicit break */
        if (pText[pos] == '\r' || pText[pos] == '\n') {
            HUnlock(pTE->base.hText);
            HUnlock((Handle)hTE);
            return pos;
        }

        /* Handle tabs */
        if (pText[pos] == '\t') {
            charWidth = TE_GetTabStop(hTE, width) - width;
        } else {
            charWidth = CharWidth(pText[pos]);
            if (charWidth <= 0) charWidth = 1;
        }

        /* Check if we exceed width */
        if (width + charWidth > maxWidth) {
            /* Use last break point if available */
            if (lastBreak > start) {
                HUnlock(pTE->base.hText);
                HUnlock((Handle)hTE);
                return lastBreak;
            }
            /* Otherwise break here (hard break) */
            HUnlock(pTE->base.hText);
            HUnlock((Handle)hTE);
            return pos;
        }

        width += charWidth;

        /* Remember break points */
        if (TE_IsBreakChar(pText[pos])) {
            lastBreak = pos + 1;  /* Break after space */
        }

        pos++;
    }

    HUnlock(pTE->base.hText);
    HUnlock((Handle)hTE);

    return end;
}

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

/*
 * TE_GetTabStop - Get next tab stop position
 */
static SInt16 TE_GetTabStop(TEHandle hTE, SInt16 currentX) {
    TEExtPtr pTE;
    SInt16 tabWidth;
    SInt16 nextStop;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Calculate tab width based on current font metrics */
    TextFont(pTE->base.txFont);
    TextSize(pTE->base.txSize);
    TextFace(pTE->base.txFace);

    tabWidth = CharWidth(' ') * TAB_WIDTH;
    if (tabWidth <= 0) {
        tabWidth = TAB_WIDTH * 7;  /* Fallback spacing */
    }

    /* Find next tab stop */
    nextStop = ((currentX / tabWidth) + 1) * tabWidth;

    HUnlock((Handle)hTE);

    return nextStop;
}

/* ============================================================================
 * Character Classification
 * ============================================================================ */

/*
 * TE_IsBreakChar - Check if character is a break point
 */
static Boolean TE_IsBreakChar(char ch) {
    /* Break after spaces, hyphens, and certain punctuation */
    return (ch == ' ' || ch == '\t' || ch == '-' ||
            ch == '/' || ch == '\\' || ch == ',' ||
            ch == ';' || ch == ':' || ch == '.');
}
