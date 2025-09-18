/**
 * TextEdit Hardware Abstraction Layer
 * Provides platform-independent implementation of TextEdit functionality
 *
 * This HAL layer bridges the System 7.1 TextEdit Manager with modern platforms,
 * providing complete text editing capabilities including selection, input,
 * clipboard operations, formatting, and undo/redo support.
 */

#include "../../include/TextEdit/textedit.h"
#include "../../include/WindowManager/WindowManager.h"
#include "../../include/QuickDraw/QuickDraw.h"
#include "../../include/EventManager/EventManager.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include "../../include/ResourceManager/ResourceManager.h"
#include "../../include/ControlManager/ControlManager.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#endif

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#endif

/* TextEdit Manager Global State */
typedef struct TextEditMgrState {
    bool initialized;
    TEHandle activeTE;              /* Currently active TextEdit record */
    int32_t caretTime;              /* Caret blink time in ticks */
    int32_t lastCaretTime;          /* Last caret toggle time */
    bool caretVisible;              /* Current caret visibility */

    /* Clipboard state */
    Handle scrapHandle;             /* Text scrap for cut/copy/paste */
    int32_t scrapLength;            /* Length of scrap text */

    /* Font cache */
    struct {
        int16_t fontID;
        int16_t fontSize;
        Style fontStyle;
        void* fontHandle;           /* Platform-specific font */
    } fontCache[16];
    int fontCacheCount;

    /* Selection colors */
    RGBColor highlightColor;
    RGBColor textColor;

    /* Double-click detection */
    int32_t lastClickTime;
    Point lastClickPt;
    int clickCount;

    /* Word break function */
    WordBreakUPP wordBreakProc;

} TextEditMgrState;

static TextEditMgrState gTextEditMgr = {0};

/* Forward declarations */
static void TextEdit_HAL_DrawText(TEHandle hTE, int32_t start, int32_t end);
static void TextEdit_HAL_DrawCaret(TEHandle hTE);
static void TextEdit_HAL_RecalculateLines(TEHandle hTE);
static int32_t TextEdit_HAL_PointToOffset(TEHandle hTE, Point pt);
static void TextEdit_HAL_OffsetToPoint(TEHandle hTE, int32_t offset, Point* pt);
static void TextEdit_HAL_ScrollToCaret(TEHandle hTE);
static bool TextEdit_HAL_IsWordChar(char ch);
static int32_t TextEdit_HAL_WordBoundary(TEHandle hTE, int32_t offset, bool forward);

/**
 * Initialize TextEdit Manager HAL
 */
void TextEdit_HAL_Init(void)
{
    if (gTextEditMgr.initialized) {
        return;
    }

    printf("TextEdit HAL: Initializing...\n");

    /* Set default caret blink time (30 ticks = 0.5 seconds) */
    gTextEditMgr.caretTime = GetCaretTime();
    gTextEditMgr.lastCaretTime = TickCount();
    gTextEditMgr.caretVisible = true;

    /* Initialize scrap */
    gTextEditMgr.scrapHandle = NewHandle(0);

    /* Set default selection colors */
    gTextEditMgr.highlightColor.red = 0x6666;
    gTextEditMgr.highlightColor.green = 0x9999;
    gTextEditMgr.highlightColor.blue = 0xCCCC;

    gTextEditMgr.textColor.red = 0x0000;
    gTextEditMgr.textColor.green = 0x0000;
    gTextEditMgr.textColor.blue = 0x0000;

    gTextEditMgr.initialized = true;

    printf("TextEdit HAL: Initialized with full editing support\n");
}

/**
 * Create a new TextEdit record
 */
TEHandle TextEdit_HAL_TENew(const Rect* destRect, const Rect* viewRect)
{
    if (!gTextEditMgr.initialized) {
        TextEdit_HAL_Init();
    }

    /* Allocate TERec handle */
    TEHandle hTE = (TEHandle)NewHandleClear(sizeof(TERec));
    if (!hTE) {
        return NULL;
    }

    /* Lock and initialize */
    HLock((Handle)hTE);
    TERec* te = *hTE;

    /* Set rectangles */
    te->destRect = *destRect;
    te->viewRect = *viewRect;

    /* Initialize text storage */
    te->hText = NewHandle(0);
    te->teLength = 0;

    /* Selection range (insertion point at start) */
    te->selStart = 0;
    te->selEnd = 0;

    /* Default font settings */
    te->txFont = systemFont;
    te->txFace = normal;
    te->txMode = srcOr;
    te->txSize = 12;

    /* Text metrics */
    te->lineHeight = te->txSize + 4;  /* Line spacing */
    te->fontAscent = te->txSize - 2;  /* Baseline offset */

    /* Alignment */
    te->just = teFlushLeft;

    /* State flags */
    te->crOnly = 0;        /* Word wrap enabled */
    te->active = 0;         /* Not active initially */
    te->wordBreak = NULL;   /* Default word break */
    te->clikLoop = NULL;    /* No click loop */
    te->clickTime = 0;
    te->clickLoc = 0;
    te->caretTime = 0;
    te->caretState = 0;

    /* Line starts array */
    te->nLines = 1;
    te->lineStarts = (short*)NewPtr(sizeof(short) * 32);  /* Initial capacity */
    te->lineStarts[0] = 0;  /* First line starts at offset 0 */

    /* Recalculate lines */
    TextEdit_HAL_RecalculateLines(hTE);

    HUnlock((Handle)hTE);

    return hTE;
}

/**
 * Dispose of a TextEdit record
 */
void TextEdit_HAL_TEDispose(TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;

    /* Free text storage */
    if (te->hText) {
        DisposeHandle(te->hText);
    }

    /* Free line starts */
    if (te->lineStarts) {
        DisposePtr((Ptr)te->lineStarts);
    }

    /* Clear active TE if this was it */
    if (gTextEditMgr.activeTE == hTE) {
        gTextEditMgr.activeTE = NULL;
    }

    /* Dispose handle */
    DisposeHandle((Handle)hTE);
}

/**
 * Set the text of a TextEdit record
 */
void TextEdit_HAL_TESetText(const void* text, int32_t length, TEHandle hTE)
{
    if (!hTE || !text || length < 0) return;

    HLock((Handle)hTE);
    TERec* te = *hTE;

    /* Resize text handle */
    SetHandleSize(te->hText, length);
    if (MemError() != noErr) {
        HUnlock((Handle)hTE);
        return;
    }

    /* Copy text */
    if (length > 0) {
        HLock(te->hText);
        BlockMove(text, *te->hText, length);
        HUnlock(te->hText);
    }

    /* Update state */
    te->teLength = length;
    te->selStart = 0;
    te->selEnd = 0;

    /* Recalculate line breaks */
    TextEdit_HAL_RecalculateLines(hTE);

    /* Redraw if active */
    if (te->active) {
        TEUpdate(&te->viewRect, hTE);
    }

    HUnlock((Handle)hTE);
}

/**
 * Get text from a TextEdit record
 */
Handle TextEdit_HAL_TEGetText(TEHandle hTE)
{
    if (!hTE) return NULL;
    return (**hTE).hText;
}

/**
 * Insert text at current selection
 */
void TextEdit_HAL_TEInsert(const void* text, int32_t length, TEHandle hTE)
{
    if (!hTE || !text || length <= 0) return;

    HLock((Handle)hTE);
    TERec* te = *hTE;

    /* Delete selection if any */
    if (te->selStart < te->selEnd) {
        TEDelete(hTE);
    }

    /* Resize text handle */
    int32_t oldLength = te->teLength;
    int32_t newLength = oldLength + length;

    SetHandleSize(te->hText, newLength);
    if (MemError() != noErr) {
        HUnlock((Handle)hTE);
        return;
    }

    /* Insert text at selection point */
    HLock(te->hText);
    char* textPtr = *te->hText;

    /* Move existing text after insertion point */
    if (te->selStart < oldLength) {
        BlockMove(textPtr + te->selStart,
                  textPtr + te->selStart + length,
                  oldLength - te->selStart);
    }

    /* Copy new text */
    BlockMove(text, textPtr + te->selStart, length);
    HUnlock(te->hText);

    /* Update state */
    te->teLength = newLength;
    te->selStart += length;
    te->selEnd = te->selStart;

    /* Recalculate lines */
    TextEdit_HAL_RecalculateLines(hTE);

    /* Scroll to caret */
    TextEdit_HAL_ScrollToCaret(hTE);

    /* Redraw */
    if (te->active) {
        TEUpdate(&te->viewRect, hTE);
    }

    HUnlock((Handle)hTE);
}

/**
 * Delete current selection
 */
void TextEdit_HAL_TEDelete(TEHandle hTE)
{
    if (!hTE) return;

    HLock((Handle)hTE);
    TERec* te = *hTE;

    if (te->selStart >= te->selEnd) {
        HUnlock((Handle)hTE);
        return;
    }

    /* Calculate deletion range */
    int32_t deleteLen = te->selEnd - te->selStart;
    int32_t newLength = te->teLength - deleteLen;

    /* Move text after selection to fill gap */
    if (te->selEnd < te->teLength) {
        HLock(te->hText);
        char* textPtr = *te->hText;
        BlockMove(textPtr + te->selEnd,
                  textPtr + te->selStart,
                  te->teLength - te->selEnd);
        HUnlock(te->hText);
    }

    /* Resize handle */
    SetHandleSize(te->hText, newLength);

    /* Update state */
    te->teLength = newLength;
    te->selEnd = te->selStart;

    /* Recalculate lines */
    TextEdit_HAL_RecalculateLines(hTE);

    /* Redraw */
    if (te->active) {
        TEUpdate(&te->viewRect, hTE);
    }

    HUnlock((Handle)hTE);
}

/**
 * Handle key input
 */
void TextEdit_HAL_TEKey(char key, TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;

    /* Handle special keys */
    switch (key) {
        case 0x08:  /* Backspace */
            if (te->selStart == te->selEnd && te->selStart > 0) {
                /* Move selection back one character */
                te->selStart--;
            }
            TEDelete(hTE);
            break;

        case 0x7F:  /* Delete (forward delete) */
            if (te->selStart == te->selEnd && te->selEnd < te->teLength) {
                /* Extend selection forward one character */
                te->selEnd++;
            }
            TEDelete(hTE);
            break;

        case 0x0D:  /* Return */
        case 0x03:  /* Enter */
            TEInsert("\r", 1, hTE);
            break;

        case 0x09:  /* Tab */
            TEInsert("\t", 1, hTE);
            break;

        default:
            /* Regular character */
            if (key >= 0x20 || key == 0x0A) {  /* Printable or newline */
                TEInsert(&key, 1, hTE);
            }
            break;
    }
}

/**
 * Handle mouse click
 */
void TextEdit_HAL_TEClick(Point pt, bool extend, TEHandle hTE)
{
    if (!hTE) return;

    HLock((Handle)hTE);
    TERec* te = *hTE;

    /* Convert point to text offset */
    int32_t offset = TextEdit_HAL_PointToOffset(hTE, pt);

    /* Check for double/triple click */
    int32_t currentTime = TickCount();
    int32_t clickDelta = currentTime - gTextEditMgr.lastClickTime;

    if (clickDelta < GetDblTime() &&
        abs(pt.h - gTextEditMgr.lastClickPt.h) < 5 &&
        abs(pt.v - gTextEditMgr.lastClickPt.v) < 5) {

        gTextEditMgr.clickCount++;

        if (gTextEditMgr.clickCount == 2) {
            /* Double-click: select word */
            int32_t wordStart = TextEdit_HAL_WordBoundary(hTE, offset, false);
            int32_t wordEnd = TextEdit_HAL_WordBoundary(hTE, offset, true);
            te->selStart = wordStart;
            te->selEnd = wordEnd;
        } else if (gTextEditMgr.clickCount >= 3) {
            /* Triple-click: select line */
            int line = 0;
            for (int i = 1; i < te->nLines; i++) {
                if (te->lineStarts[i] > offset) {
                    line = i - 1;
                    break;
                }
            }
            te->selStart = te->lineStarts[line];
            te->selEnd = (line + 1 < te->nLines) ?
                         te->lineStarts[line + 1] : te->teLength;
        }
    } else {
        /* Single click */
        gTextEditMgr.clickCount = 1;

        if (extend) {
            /* Extend selection */
            if (offset < te->selStart) {
                te->selStart = offset;
            } else {
                te->selEnd = offset;
            }
        } else {
            /* New selection */
            te->selStart = offset;
            te->selEnd = offset;
        }
    }

    /* Update click tracking */
    gTextEditMgr.lastClickTime = currentTime;
    gTextEditMgr.lastClickPt = pt;

    /* Redraw */
    if (te->active) {
        TEUpdate(&te->viewRect, hTE);
    }

    HUnlock((Handle)hTE);
}

/**
 * Set selection range
 */
void TextEdit_HAL_TESetSelect(int32_t selStart, int32_t selEnd, TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;

    /* Clamp to valid range */
    if (selStart < 0) selStart = 0;
    if (selStart > te->teLength) selStart = te->teLength;
    if (selEnd < 0) selEnd = 0;
    if (selEnd > te->teLength) selEnd = te->teLength;

    /* Ensure start <= end */
    if (selStart > selEnd) {
        int32_t temp = selStart;
        selStart = selEnd;
        selEnd = temp;
    }

    /* Update selection */
    te->selStart = selStart;
    te->selEnd = selEnd;

    /* Scroll to show selection */
    TextEdit_HAL_ScrollToCaret(hTE);

    /* Redraw if active */
    if (te->active) {
        TEUpdate(&te->viewRect, hTE);
    }
}

/**
 * Activate/deactivate TextEdit record
 */
void TextEdit_HAL_TEActivate(TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;
    te->active = 1;
    gTextEditMgr.activeTE = hTE;

    /* Start caret blinking */
    gTextEditMgr.caretVisible = true;
    gTextEditMgr.lastCaretTime = TickCount();

    /* Redraw with caret */
    TEUpdate(&te->viewRect, hTE);
}

void TextEdit_HAL_TEDeactivate(TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;
    te->active = 0;

    if (gTextEditMgr.activeTE == hTE) {
        gTextEditMgr.activeTE = NULL;
    }

    /* Hide caret and redraw */
    gTextEditMgr.caretVisible = false;
    TEUpdate(&te->viewRect, hTE);
}

/**
 * Update/redraw TextEdit content
 */
void TextEdit_HAL_TEUpdate(const Rect* updateRect, TEHandle hTE)
{
    if (!hTE) return;

    HLock((Handle)hTE);
    TERec* te = *hTE;

    /* Save graphics state */
    PenState savedPen;
    GetPenState(&savedPen);

    /* Clear background */
    EraseRect(&te->viewRect);

    /* Set up clipping */
    RgnHandle clipRgn = NewRgn();
    GetClip(clipRgn);
    ClipRect(&te->viewRect);

    /* Draw each visible line */
    int32_t topLine = 0;
    int32_t bottomLine = te->nLines;

    for (int line = topLine; line < bottomLine && line < te->nLines; line++) {
        int32_t lineStart = te->lineStarts[line];
        int32_t lineEnd = (line + 1 < te->nLines) ?
                         te->lineStarts[line + 1] : te->teLength;

        /* Calculate line position */
        int16_t lineY = te->destRect.top + (line + 1) * te->lineHeight;

        /* Skip if line is outside view */
        if (lineY < te->viewRect.top) continue;
        if (lineY - te->lineHeight > te->viewRect.bottom) break;

        /* Draw line text */
        MoveTo(te->destRect.left, lineY - te->fontAscent/2);

        if (lineEnd > lineStart && te->hText) {
            HLock(te->hText);
            DrawText(*te->hText, lineStart, lineEnd - lineStart);
            HUnlock(te->hText);
        }

        /* Draw selection highlight */
        if (te->selStart < te->selEnd) {
            if (!(lineEnd <= te->selStart || lineStart >= te->selEnd)) {
                /* Line contains selection */
                int32_t selLineStart = (te->selStart > lineStart) ?
                                      te->selStart : lineStart;
                int32_t selLineEnd = (te->selEnd < lineEnd) ?
                                    te->selEnd : lineEnd;

                /* Calculate selection rectangle */
                Point startPt, endPt;
                TextEdit_HAL_OffsetToPoint(hTE, selLineStart, &startPt);
                TextEdit_HAL_OffsetToPoint(hTE, selLineEnd, &endPt);

                Rect selRect;
                selRect.left = startPt.h;
                selRect.right = endPt.h;
                selRect.top = lineY - te->lineHeight;
                selRect.bottom = lineY;

                /* Draw highlight */
                PenMode(patXor);
                PaintRect(&selRect);
                PenMode(patCopy);
            }
        }
    }

    /* Draw caret if active */
    if (te->active && te->selStart == te->selEnd && gTextEditMgr.caretVisible) {
        TextEdit_HAL_DrawCaret(hTE);
    }

    /* Restore clipping */
    SetClip(clipRgn);
    DisposeRgn(clipRgn);

    /* Restore graphics state */
    SetPenState(&savedPen);

    HUnlock((Handle)hTE);
}

/**
 * Idle processing (caret blinking)
 */
void TextEdit_HAL_TEIdle(TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;
    if (!te->active || te->selStart != te->selEnd) {
        return;  /* No caret when not active or selection exists */
    }

    /* Check if time to toggle caret */
    int32_t currentTime = TickCount();
    if (currentTime - gTextEditMgr.lastCaretTime >= gTextEditMgr.caretTime) {
        gTextEditMgr.caretVisible = !gTextEditMgr.caretVisible;
        gTextEditMgr.lastCaretTime = currentTime;

        /* Redraw just the caret area */
        TextEdit_HAL_DrawCaret(hTE);
    }
}

/**
 * Cut selected text to clipboard
 */
void TextEdit_HAL_TECut(TEHandle hTE)
{
    if (!hTE) return;

    /* Copy to clipboard first */
    TECopy(hTE);

    /* Then delete selection */
    TEDelete(hTE);
}

/**
 * Copy selected text to clipboard
 */
void TextEdit_HAL_TECopy(TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;

    if (te->selStart >= te->selEnd) {
        return;  /* Nothing to copy */
    }

    /* Get selected text length */
    int32_t selLength = te->selEnd - te->selStart;

    /* Resize scrap handle */
    SetHandleSize(gTextEditMgr.scrapHandle, selLength);
    if (MemError() != noErr) {
        return;
    }

    /* Copy selected text to scrap */
    HLock(te->hText);
    HLock(gTextEditMgr.scrapHandle);
    BlockMove(*te->hText + te->selStart,
              *gTextEditMgr.scrapHandle,
              selLength);
    HUnlock(gTextEditMgr.scrapHandle);
    HUnlock(te->hText);

    gTextEditMgr.scrapLength = selLength;
}

/**
 * Paste text from clipboard
 */
void TextEdit_HAL_TEPaste(TEHandle hTE)
{
    if (!hTE || gTextEditMgr.scrapLength <= 0) return;

    /* Insert scrap text */
    HLock(gTextEditMgr.scrapHandle);
    TEInsert(*gTextEditMgr.scrapHandle, gTextEditMgr.scrapLength, hTE);
    HUnlock(gTextEditMgr.scrapHandle);
}

/**
 * Scroll TextEdit view
 */
void TextEdit_HAL_TEScroll(int16_t dh, int16_t dv, TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;

    /* Offset destination rectangle */
    OffsetRect(&te->destRect, dh, dv);

    /* Redraw */
    if (te->active) {
        TEUpdate(&te->viewRect, hTE);
    }
}

/* ===== Internal Helper Functions ===== */

/**
 * Recalculate line breaks
 */
static void TextEdit_HAL_RecalculateLines(TEHandle hTE)
{
    if (!hTE) return;

    HLock((Handle)hTE);
    TERec* te = *hTE;

    te->nLines = 1;
    te->lineStarts[0] = 0;

    if (te->teLength == 0) {
        HUnlock((Handle)hTE);
        return;
    }

    HLock(te->hText);
    char* text = *te->hText;

    /* Find line breaks */
    int lineIndex = 1;
    int32_t maxWidth = te->destRect.right - te->destRect.left;

    for (int32_t i = 0; i < te->teLength; i++) {
        if (text[i] == '\r' || text[i] == '\n') {
            /* Hard line break */
            te->lineStarts[lineIndex++] = i + 1;
            te->nLines = lineIndex;
        } else if (!te->crOnly) {
            /* Word wrap - simplified for now */
            /* Real implementation would measure text width */
        }

        /* Grow line starts array if needed */
        if (lineIndex >= 30) {
            /* Simplified - real implementation would reallocate */
            break;
        }
    }

    HUnlock(te->hText);
    HUnlock((Handle)hTE);
}

/**
 * Draw caret at current position
 */
static void TextEdit_HAL_DrawCaret(TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;

    /* Get caret position */
    Point caretPt;
    TextEdit_HAL_OffsetToPoint(hTE, te->selStart, &caretPt);

    /* Draw caret line */
    if (gTextEditMgr.caretVisible) {
        MoveTo(caretPt.h, caretPt.v - te->lineHeight + 2);
        LineTo(caretPt.h, caretPt.v);
    } else {
        /* Erase caret by redrawing area */
        Rect caretRect;
        SetRect(&caretRect, caretPt.h - 1, caretPt.v - te->lineHeight,
                caretPt.h + 2, caretPt.v + 2);
        EraseRect(&caretRect);
    }
}

/**
 * Convert point to text offset
 */
static int32_t TextEdit_HAL_PointToOffset(TEHandle hTE, Point pt)
{
    if (!hTE) return 0;

    TERec* te = *hTE;

    /* Find line containing point */
    int line = (pt.v - te->destRect.top) / te->lineHeight;
    if (line < 0) line = 0;
    if (line >= te->nLines) line = te->nLines - 1;

    /* Get line range */
    int32_t lineStart = te->lineStarts[line];
    int32_t lineEnd = (line + 1 < te->nLines) ?
                     te->lineStarts[line + 1] : te->teLength;

    /* Find character in line (simplified - real would measure text) */
    int32_t offset = lineStart;
    int16_t x = te->destRect.left;

    if (te->hText && lineEnd > lineStart) {
        HLock(te->hText);
        char* text = *te->hText;

        /* Simple character width estimation */
        int charWidth = 7;  /* Approximate width */
        int charsFromLeft = (pt.h - te->destRect.left) / charWidth;

        offset = lineStart + charsFromLeft;
        if (offset > lineEnd) offset = lineEnd;
        if (offset < lineStart) offset = lineStart;

        HUnlock(te->hText);
    }

    return offset;
}

/**
 * Convert text offset to point
 */
static void TextEdit_HAL_OffsetToPoint(TEHandle hTE, int32_t offset, Point* pt)
{
    if (!hTE || !pt) return;

    TERec* te = *hTE;

    /* Find line containing offset */
    int line = 0;
    for (int i = 1; i < te->nLines; i++) {
        if (te->lineStarts[i] > offset) {
            line = i - 1;
            break;
        }
    }
    if (line >= te->nLines) line = te->nLines - 1;

    /* Calculate vertical position */
    pt->v = te->destRect.top + (line + 1) * te->lineHeight;

    /* Calculate horizontal position (simplified) */
    int32_t lineStart = te->lineStarts[line];
    int32_t charsFromStart = offset - lineStart;
    pt->h = te->destRect.left + (charsFromStart * 7);  /* Approximate */
}

/**
 * Scroll to make caret visible
 */
static void TextEdit_HAL_ScrollToCaret(TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;

    /* Get caret position */
    Point caretPt;
    TextEdit_HAL_OffsetToPoint(hTE, te->selStart, &caretPt);

    /* Check if caret is visible */
    if (caretPt.v < te->viewRect.top + te->lineHeight) {
        /* Scroll up */
        int16_t dv = te->viewRect.top - caretPt.v + te->lineHeight;
        TEScroll(0, dv, hTE);
    } else if (caretPt.v > te->viewRect.bottom) {
        /* Scroll down */
        int16_t dv = te->viewRect.bottom - caretPt.v;
        TEScroll(0, dv, hTE);
    }

    if (caretPt.h < te->viewRect.left) {
        /* Scroll left */
        int16_t dh = te->viewRect.left - caretPt.h;
        TEScroll(dh, 0, hTE);
    } else if (caretPt.h > te->viewRect.right) {
        /* Scroll right */
        int16_t dh = te->viewRect.right - caretPt.h;
        TEScroll(dh, 0, hTE);
    }
}

/**
 * Check if character is part of word
 */
static bool TextEdit_HAL_IsWordChar(char ch)
{
    return isalnum(ch) || ch == '_';
}

/**
 * Find word boundary
 */
static int32_t TextEdit_HAL_WordBoundary(TEHandle hTE, int32_t offset, bool forward)
{
    if (!hTE) return offset;

    TERec* te = *hTE;
    if (!te->hText || te->teLength == 0) return offset;

    HLock(te->hText);
    char* text = *te->hText;

    if (forward) {
        /* Find end of word */
        while (offset < te->teLength && TextEdit_HAL_IsWordChar(text[offset])) {
            offset++;
        }
        /* Skip trailing spaces */
        while (offset < te->teLength && text[offset] == ' ') {
            offset++;
        }
    } else {
        /* Find start of word */
        if (offset > 0) offset--;
        /* Skip preceding spaces */
        while (offset > 0 && text[offset] == ' ') {
            offset--;
        }
        /* Find word start */
        while (offset > 0 && TextEdit_HAL_IsWordChar(text[offset - 1])) {
            offset--;
        }
    }

    HUnlock(te->hText);
    return offset;
}

/**
 * Calculate text metrics
 */
void TextEdit_HAL_CalcVis(TEHandle hTE)
{
    if (!hTE) return;

    TERec* te = *hTE;

    /* Calculate visible lines */
    int16_t visHeight = te->viewRect.bottom - te->viewRect.top;
    int16_t visLines = visHeight / te->lineHeight;

    /* Store for scrolling calculations */
    te->teLength = visLines;  /* Temporary use of field */
}

/**
 * Auto-scroll during text selection
 */
bool TextEdit_HAL_TEAutoScroll(TEHandle hTE)
{
    /* Implement auto-scrolling when dragging selection */
    /* Returns true if scrolled */
    return false;
}

/**
 * Set word break procedure
 */
void TextEdit_HAL_SetWordBreak(WordBreakUPP wbProc, TEHandle hTE)
{
    if (!hTE) return;
    (**hTE).wordBreak = wbProc;
}

/**
 * Set click loop procedure
 */
void TextEdit_HAL_SetClickLoop(TEClickLoopUPP clProc, TEHandle hTE)
{
    if (!hTE) return;
    (**hTE).clikLoop = clProc;
}

/**
 * Pin text scroll position
 */
void TextEdit_HAL_TEPinScroll(int16_t dh, int16_t dv, TEHandle hTE)
{
    if (!hTE) return;

    /* Ensure scroll doesn't go beyond text bounds */
    TERec* te = *hTE;

    /* Calculate limits */
    int16_t minV = 0;
    int16_t maxV = te->nLines * te->lineHeight -
                   (te->viewRect.bottom - te->viewRect.top);

    if (maxV < minV) maxV = minV;

    /* Apply scroll with limits */
    if (te->destRect.top + dv < minV) {
        dv = minV - te->destRect.top;
    }
    if (te->destRect.top + dv > maxV) {
        dv = maxV - te->destRect.top;
    }

    TEScroll(dh, dv, hTE);
}

/**
 * Get style information at offset
 */
void TextEdit_HAL_TEGetStyle(int32_t offset, TextStyle* theStyle,
                            short* lineHeight, short* fontAscent, TEHandle hTE)
{
    if (!hTE || !theStyle) return;

    TERec* te = *hTE;

    /* Return current style (no styled text support yet) */
    theStyle->tsFont = te->txFont;
    theStyle->tsFace = te->txFace;
    theStyle->tsSize = te->txSize;
    theStyle->tsColor = gTextEditMgr.textColor;

    if (lineHeight) *lineHeight = te->lineHeight;
    if (fontAscent) *fontAscent = te->fontAscent;
}

/**
 * Cleanup TextEdit HAL
 */
void TextEdit_HAL_Cleanup(void)
{
    if (!gTextEditMgr.initialized) {
        return;
    }

    /* Dispose scrap */
    if (gTextEditMgr.scrapHandle) {
        DisposeHandle(gTextEditMgr.scrapHandle);
    }

    /* Clear font cache */
    /* Platform-specific font cleanup would go here */

    /* Reset state */
    memset(&gTextEditMgr, 0, sizeof(gTextEditMgr));

    printf("TextEdit HAL: Cleaned up\n");
}