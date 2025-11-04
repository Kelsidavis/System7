/*
 * TextEditInput.c - TextEdit Input Handling
 *
 * Handles keyboard input, mouse clicks, and selection management
 */

#include "TextEdit/TextEdit.h"
#include "MemoryMgr/MemoryManager.h"
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
#define TEI_DEBUG 1

#if TEI_DEBUG
#define TEI_LOG(...) TE_LOG_DEBUG("TEI: " __VA_ARGS__)
#else
#define TEI_LOG(...)
#endif

/* Forward declarations */
static void TE_TrackMouse(TEHandle hTE, Point startPt);

/* Key codes */
#define kBackspace      0x08
#define kTab            0x09
#define kReturn         0x0D
#define kEscape         0x1B
#define kLeftArrow      0x1C
#define kRightArrow     0x1D
#define kUpArrow        0x1E
#define kDownArrow      0x1F
#define kDelete         0x7F
#define kHome           0x01
#define kEnd            0x04
#define kPageUp         0x0B
#define kPageDown       0x0C

/* Click timing */
#define DOUBLE_CLICK_TIME   30      /* Ticks for double-click */
#define TRIPLE_CLICK_TIME   45      /* Ticks for triple-click */

/* External functions */
extern UInt32 TickCount(void);
/* Forward declarations */
static void TE_HandleArrowKey(TEHandle hTE, CharParameter key, Boolean shift,
                              Boolean option, Boolean command);
static void TE_ExtendSelection(TEHandle hTE, SInt32 newPos);
static Boolean TE_IsWordChar(char ch);
static Boolean TE_IsShiftDown(void);
static Boolean TE_IsOptionDown(void);
static Boolean TE_IsCommandDown(void);

/* ============================================================================
 * Keyboard Input
 * ============================================================================ */

/*
 * TEKey - Handle keyboard input
 */
void TEKey(CharParameter key, TEHandle hTE) {
    TEExtPtr pTE;
    Boolean shift, option, command;
    char ch;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Check read-only */
    if (pTE->readOnly && key != kLeftArrow && key != kRightArrow &&
        key != kUpArrow && key != kDownArrow) {
        HUnlock((Handle)hTE);
        return;
    }

    /* Get modifier keys */
    shift = TE_IsShiftDown();
    option = TE_IsOptionDown();
    command = TE_IsCommandDown();

    TEI_LOG("TEKey: key=0x%02X shift=%d option=%d cmd=%d\n",
            key, shift, option, command);

    /* Handle special keys */
    switch (key) {
        case kBackspace:
        case kDelete:
            if (pTE->base.selStart == pTE->base.selEnd) {
                /* Delete character before cursor */
                if (pTE->base.selStart > 0) {
                    TESetSelect(pTE->base.selStart - 1, pTE->base.selStart, hTE);
                    TEDelete(hTE);
                }
            } else {
                /* Delete selection */
                TEDelete(hTE);
            }
            break;

        case kReturn:
            /* Insert CR */
            ch = '\r';
            TEInsert(&ch, 1, hTE);
            break;

        case kTab:
            /* Insert tab */
            ch = '\t';
            TEInsert(&ch, 1, hTE);
            break;

        case kLeftArrow:
        case kRightArrow:
        case kUpArrow:
        case kDownArrow:
            TE_HandleArrowKey(hTE, key, shift, option, command);
            break;

        case kHome:
            if (shift) {
                TE_ExtendSelection(hTE, 0);
            } else {
                TESetSelect(0, 0, hTE);
            }
            TESelView(hTE);
            break;

        case kEnd:
            if (shift) {
                TE_ExtendSelection(hTE, pTE->base.teLength);
            } else {
                TESetSelect(pTE->base.teLength, pTE->base.teLength, hTE);
            }
            TESelView(hTE);
            break;

        case kPageUp:
            /* Scroll up by approximately view height */
            {
                SInt16 viewHeight = pTE->base.viewRect.bottom - pTE->base.viewRect.top;
                SInt16 lineHeight = pTE->base.lineHeight ? pTE->base.lineHeight : 16;
                SInt16 linesPerPage = (viewHeight / lineHeight) - 1;  /* Leave one line overlap */
                if (linesPerPage < 1) linesPerPage = 1;

                /* Scroll up */
                TEScroll(0, -(linesPerPage * lineHeight), hTE);

                /* Move selection to top of visible area */
                if (shift) {
                    /* Extend selection upward */
                    SInt32 newPos = pTE->base.selEnd - (linesPerPage * lineHeight);
                    if (newPos < 0) newPos = 0;
                    TE_ExtendSelection(hTE, newPos);
                } else {
                    /* Move cursor to top of visible area */
                    SInt32 topLine = TE_OffsetToLine(hTE, pTE->base.selEnd);
                    SInt32 targetLine = topLine - linesPerPage;
                    if (targetLine < 0) targetLine = 0;
                    SInt32 newPos = TE_LineToOffset(hTE, targetLine);
                    TESetSelect(newPos, newPos, hTE);
                }
            }
            break;

        case kPageDown:
            /* Scroll down by approximately view height */
            {
                SInt16 viewHeight = pTE->base.viewRect.bottom - pTE->base.viewRect.top;
                SInt16 lineHeight = pTE->base.lineHeight ? pTE->base.lineHeight : 16;
                SInt16 linesPerPage = (viewHeight / lineHeight) - 1;  /* Leave one line overlap */
                if (linesPerPage < 1) linesPerPage = 1;

                /* Scroll down */
                TEScroll(0, linesPerPage * lineHeight, hTE);

                /* Move selection to bottom of visible area */
                if (shift) {
                    /* Extend selection downward */
                    SInt32 newPos = pTE->base.selEnd + (linesPerPage * lineHeight);
                    if (newPos > pTE->base.teLength) newPos = pTE->base.teLength;
                    TE_ExtendSelection(hTE, newPos);
                } else {
                    /* Move cursor to bottom of visible area */
                    SInt32 bottomLine = TE_OffsetToLine(hTE, pTE->base.selEnd);
                    SInt32 targetLine = bottomLine + linesPerPage;
                    SInt32 newPos = TE_LineToOffset(hTE, targetLine);
                    if (newPos > pTE->base.teLength) newPos = pTE->base.teLength;
                    TESetSelect(newPos, newPos, hTE);
                }
            }
            break;

        default:
            /* Insert regular character */
            if (key >= 0x20 && key < 0x7F) {
                ch = key;
                TEInsert(&ch, 1, hTE);
            }
            break;
    }

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Mouse Input
 * ============================================================================ */

/*
 * TEClick - Handle mouse click
 */
void TEClick(Point pt, Boolean extendSelection, TEHandle hTE) {
    TEExtPtr pTE;
    SInt32 offset;
    UInt32 currentTime;
    SInt32 clickDelta;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    currentTime = TickCount();

    /* Get click offset */
    offset = TEGetOffset(pt, hTE);

    TEI_LOG("TEClick: pt=(%d,%d) offset=%d extend=%d\n",
            pt.h, pt.v, offset, extendSelection);

    /* Check for multi-click */
    if (!extendSelection && currentTime - pTE->lastClickTime < DOUBLE_CLICK_TIME) {
        clickDelta = offset - pTE->base.clickLoc;
        if (clickDelta < 0) clickDelta = -clickDelta;

        if (clickDelta <= 2) {
            /* Same location - multi-click */
            pTE->clickCount++;

            if (pTE->clickCount == 2) {
                /* Double-click - select word */
                TEI_LOG("Double-click: selecting word\n");
                SInt32 wordStart = TE_FindWordBoundary(hTE, offset, FALSE);
                SInt32 wordEnd = TE_FindWordBoundary(hTE, offset, TRUE);
                TESetSelect(wordStart, wordEnd, hTE);
                pTE->base.clickLoc = offset;
                HUnlock((Handle)hTE);
                return;
            } else if (pTE->clickCount >= 3) {
                /* Triple-click - select line */
                TEI_LOG("Triple-click: selecting line\n");
                SInt32 lineStart = TE_FindLineStart(hTE, offset);
                SInt32 lineEnd = TE_FindLineEnd(hTE, offset);
                TESetSelect(lineStart, lineEnd, hTE);
                pTE->clickCount = 0;  /* Reset */
                HUnlock((Handle)hTE);
                return;
            }
        } else {
            /* Different location - reset */
            pTE->clickCount = 1;
        }
    } else if (!extendSelection) {
        /* New click */
        pTE->clickCount = 1;
    }

    /* Update click tracking */
    pTE->lastClickTime = currentTime;
    pTE->base.clickLoc = offset;

    /* Handle selection */
    if (extendSelection) {
        /* Extend selection */
        TE_ExtendSelection(hTE, offset);
    } else {
        /* New selection */
        TESetSelect(offset, offset, hTE);
        pTE->dragAnchor = offset;
        pTE->inDragSel = TRUE;

        /* Track mouse for drag selection */
        TE_TrackMouse(hTE, pt);
    }

    HUnlock((Handle)hTE);
}

/*
 * TE_TrackMouse - Track mouse during drag selection
 */
static void TE_TrackMouse(TEHandle hTE, Point startPt) {
    TEExtPtr pTE;
    Point pt;
    SInt32 offset;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    TEI_LOG("TE_TrackMouse: starting drag from (%d,%d)\n", startPt.h, startPt.v);

    /* Track until mouse up */
    const UInt32 MAX_DRAG_ITERATIONS = 100000;  /* Safety timeout: ~1666 seconds at 60Hz */
    UInt32 loopCount = 0;

    while (StillDown() && loopCount < MAX_DRAG_ITERATIONS) {
        loopCount++;
        ProcessModernInput();  /* Update gCurrentButtons/g_mousePos */
        GetMouse(&pt);

        /* Get offset at current position */
        offset = TEGetOffset(pt, hTE);

        /* Update selection */
        if (offset != pTE->base.selEnd) {
            if (offset < pTE->dragAnchor) {
                TESetSelect(offset, pTE->dragAnchor, hTE);
            } else {
                TESetSelect(pTE->dragAnchor, offset, hTE);
            }
        }

        /* Check for autoscroll */
        if (pt.v < pTE->base.viewRect.top || pt.v > pTE->base.viewRect.bottom ||
            pt.h < pTE->base.viewRect.left || pt.h > pTE->base.viewRect.right) {
            /* Outside view - trigger autoscroll via TEIdle */
            TEIdle(hTE);
        }
    }

    if (loopCount >= MAX_DRAG_ITERATIONS) {
        /* Safety timeout reached - log warning */
        TEI_LOG("TE_TrackMouse: drag loop timeout after %u iterations\n", loopCount);
    }

    pTE->inDragSel = FALSE;

    TEI_LOG("TE_TrackMouse: ended with selection [%d,%d]\n",
            pTE->base.selStart, pTE->base.selEnd);

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Arrow Key Handling
 * ============================================================================ */

/*
 * TE_HandleArrowKey - Process arrow key input
 */
static void TE_HandleArrowKey(TEHandle hTE, CharParameter key, Boolean shift,
                              Boolean option, Boolean command) {
    TEExtPtr pTE;
    SInt32 newPos;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Get current position */
    newPos = pTE->base.selEnd;
    if (!shift && pTE->base.selStart != pTE->base.selEnd) {
        /* Collapse selection in direction of arrow */
        if (key == kLeftArrow || key == kUpArrow) {
            newPos = pTE->base.selStart;
        } else {
            newPos = pTE->base.selEnd;
        }
        TESetSelect(newPos, newPos, hTE);
        HUnlock((Handle)hTE);
        return;
    }

    switch (key) {
        case kLeftArrow:
            if (command) {
                /* Command-Left: beginning of line */
                newPos = TE_FindLineStart(hTE, newPos);
            } else if (option) {
                /* Option-Left: previous word */
                newPos = TE_FindWordBoundary(hTE, newPos - 1, FALSE);
            } else {
                /* Left: previous character */
                if (newPos > 0) newPos--;
            }
            break;

        case kRightArrow:
            if (command) {
                /* Command-Right: end of line */
                newPos = TE_FindLineEnd(hTE, newPos);
            } else if (option) {
                /* Option-Right: next word */
                newPos = TE_FindWordBoundary(hTE, newPos + 1, TRUE);
            } else {
                /* Right: next character */
                if (newPos < pTE->base.teLength) newPos++;
            }
            break;

        case kUpArrow:
            if (command) {
                /* Command-Up: beginning of text */
                newPos = 0;
            } else {
                /* Up: previous line */
                Point pt = TEGetPoint(newPos, hTE);
                pt.v -= pTE->base.lineHeight;
                newPos = TEGetOffset(pt, hTE);
            }
            break;

        case kDownArrow:
            if (command) {
                /* Command-Down: end of text */
                newPos = pTE->base.teLength;
            } else {
                /* Down: next line */
                Point pt = TEGetPoint(newPos, hTE);
                pt.v += pTE->base.lineHeight;
                newPos = TEGetOffset(pt, hTE);
            }
            break;
    }

    /* Update selection */
    if (shift) {
        TE_ExtendSelection(hTE, newPos);
    } else {
        TESetSelect(newPos, newPos, hTE);
    }

    /* Ensure visible */
    TESelView(hTE);

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Word and Line Boundaries
 * ============================================================================ */

/*
 * TE_FindWordBoundary - Find word boundary
 */
SInt32 TE_FindWordBoundary(TEHandle hTE, SInt32 offset, Boolean forward) {
    TEExtPtr pTE;
    char *pText;
    SInt32 pos;

    if (!hTE) return 0;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    HLock(pTE->base.hText);
    pText = *pTE->base.hText;

    /* Clamp offset */
    if (offset < 0) offset = 0;
    if (offset > pTE->base.teLength) offset = pTE->base.teLength;

    pos = offset;

    if (forward) {
        /* Find end of current word */
        while (pos < pTE->base.teLength && TE_IsWordChar(pText[pos])) {
            pos++;
        }
        /* Skip non-word chars */
        while (pos < pTE->base.teLength && !TE_IsWordChar(pText[pos])) {
            pos++;
        }
    } else {
        /* Find start of current word */
        while (pos > 0 && !TE_IsWordChar(pText[pos - 1])) {
            pos--;
        }
        while (pos > 0 && TE_IsWordChar(pText[pos - 1])) {
            pos--;
        }
    }

    HUnlock(pTE->base.hText);
    HUnlock((Handle)hTE);

    TEI_LOG("TE_FindWordBoundary: %d %s -> %d\n",
            offset, forward ? "forward" : "backward", pos);

    return pos;
}

/*
 * TE_FindLineStart - Find start of line
 */
SInt32 TE_FindLineStart(TEHandle hTE, SInt32 offset) {
    TEExtPtr pTE;
    SInt32 lineNum;
    SInt32 *pLines;
    SInt32 lineStart;

    if (!hTE) return 0;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Find line containing offset */
    lineNum = TE_OffsetToLine(hTE, offset);

    /* Get line start */
    HLock(pTE->hLines);
    pLines = (SInt32*)*pTE->hLines;
    lineStart = pLines[lineNum];
    HUnlock(pTE->hLines);

    HUnlock((Handle)hTE);

    return lineStart;
}

/*
 * TE_FindLineEnd - Find end of line
 */
SInt32 TE_FindLineEnd(TEHandle hTE, SInt32 offset) {
    TEExtPtr pTE;
    SInt32 lineNum;
    SInt32 *pLines;
    SInt32 lineEnd;
    char *pText;

    if (!hTE) return 0;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Find line containing offset */
    lineNum = TE_OffsetToLine(hTE, offset);

    /* Get line end */
    HLock(pTE->hLines);
    pLines = (SInt32*)*pTE->hLines;
    lineEnd = (lineNum + 1 < pTE->nLines) ? pLines[lineNum + 1] : pTE->base.teLength;
    HUnlock(pTE->hLines);

    /* Skip trailing CR */
    if (lineEnd > 0) {
        HLock(pTE->base.hText);
        pText = *pTE->base.hText;
        if (lineEnd > 0 && pText[lineEnd - 1] == '\r') {
            lineEnd--;
        }
        HUnlock(pTE->base.hText);
    }

    HUnlock((Handle)hTE);

    return lineEnd;
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/*
 * TE_ExtendSelection - Extend selection to new position
 */
static void TE_ExtendSelection(TEHandle hTE, SInt32 newPos) {
    TEExtPtr pTE = (TEExtPtr)*hTE;
    SInt32 anchor;

    /* Determine anchor point */
    if (pTE->base.selStart == pTE->base.selEnd) {
        /* No selection - use current position as anchor */
        anchor = pTE->base.selStart;
    } else if (pTE->dragAnchor >= 0) {
        /* Use drag anchor */
        anchor = pTE->dragAnchor;
    } else {
        /* Use opposite end */
        anchor = (newPos < pTE->base.selStart) ? pTE->base.selEnd : pTE->base.selStart;
    }

    /* Set new selection */
    if (newPos < anchor) {
        TESetSelect(newPos, anchor, hTE);
    } else {
        TESetSelect(anchor, newPos, hTE);
    }
}

/*
 * TE_IsWordChar - Check if character is word character
 */
static Boolean TE_IsWordChar(char ch) {
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           (ch == '_');
}

/*
 * TE_GetKeys - Get keyboard state
 */
static void TE_GetKeys(KeyMap keys) {
    if (!keys) {
        return;
    }
    GetKeys(keys);
}

/*
 * TE_IsShiftDown - Check if shift key is down
 */
static Boolean TE_IsShiftDown(void) {
    KeyMap keys;
    TE_GetKeys(keys);
    return (keys[1] & 0x01) != 0;  /* Shift key */
}

/*
 * TE_IsOptionDown - Check if option key is down
 */
static Boolean TE_IsOptionDown(void) {
    KeyMap keys;
    TE_GetKeys(keys);
    return (keys[1] & 0x04) != 0;  /* Option key */
}

/*
 * TE_IsCommandDown - Check if command key is down
 */
static Boolean TE_IsCommandDown(void) {
    KeyMap keys;
    TE_GetKeys(keys);
    return (keys[1] & 0x80) != 0;  /* Command key */
}
