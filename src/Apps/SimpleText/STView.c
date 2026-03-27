/*
 * STView.c - SimpleText View/TextEdit Handling
 *
 * Manages TextEdit records, scrolling, selection, and styling
 */

#include <string.h>
#include <stdio.h>
#include "Apps/SimpleText.h"
#include "MemoryMgr/MemoryManager.h"
#include "FontManager/FontManager.h"
#include "QuickDraw/QuickDrawPlatform.h"

extern void serial_puts(const char* str);

/* Internal TextEdit record extension (mirrors TextEdit modules) */
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

typedef TEExtRec *TEExtPtr;

/* Layout helpers */
static void STView_ComputeLayout(STDocument* doc, Rect* textRect, Rect* scrollRect);
static void STView_RepositionScrollBar(STDocument* doc);
static void STView_UpdateScrollMetrics(STDocument* doc);
static void STView_ClearScrollArea(STDocument* doc, const Rect* textRect, const Rect* scrollRect);
static void STView_RedrawScrollBar(STDocument* doc, const Rect* scrollRect);

/* Helper functions */
static void ApplyStyleToSelection(STDocument* doc, SInt16 font, SInt16 size, Style style);
static void GetSelectionStyle(STDocument* doc, SInt16* font, SInt16* size, Style* style);

/*
 * STView_Create - Create TextEdit view for document
 */
void STView_Create(STDocument* doc) {
    Rect destRect, viewRect, scrollRect;

    if (!doc || !doc->window) return;

    ST_Log("Creating TextEdit view\n");

    /* Set up port */
    SetPort((GrafPtr)doc->window);

    /* Calculate text rectangles */
    STView_ComputeLayout(doc, &destRect, &scrollRect);
    viewRect = destRect;

    /* Create TextEdit record */
    doc->hTE = TENew(&destRect, &viewRect);
    if (!doc->hTE) {
        ST_Log("Failed to create TextEdit record\n");
        return;
    }

    /* Set default attributes */
    (*doc->hTE)->txFont = g_ST.currentFont;
    (*doc->hTE)->txSize = g_ST.currentSize;
    (*doc->hTE)->txFace = g_ST.currentStyle;

    /* Enable word wrap */
    (*doc->hTE)->crOnly = -1;  /* Word wrap mode */

    /* Create vertical scrollbar (optional for v1) */
    doc->vScroll = NewVScrollBar(doc->window, &scrollRect, 0, 0, 0);
    if (doc->vScroll) {
        STView_RepositionScrollBar(doc);
        STView_UpdateScrollMetrics(doc);
        DrawControls(doc->window);
    }

    /* Initialize style runs */
    doc->styles.numRuns = 0;
    doc->styles.hRuns = NULL;

    ST_Log("TextEdit view created\n");
}

/*
 * STView_Dispose - Dispose TextEdit view
 */
void STView_Dispose(STDocument* doc) {
    if (!doc) return;

    ST_Log("Disposing TextEdit view\n");

    if (doc->vScroll) {
        DisposeControl(doc->vScroll);
        doc->vScroll = NULL;
    }

    /* Dispose TextEdit record */
    if (doc->hTE) {
        TEDispose(doc->hTE);
        doc->hTE = NULL;
    }

}

/*
 * STView_Draw - Draw TextEdit content
 */
void STView_Draw(STDocument* doc) {
    Rect updateRect;
    Rect textRect;
    Rect scrollRect;

    if (!doc || !doc->hTE || !doc->window) return;

    SetPort((GrafPtr)doc->window);

    STView_ComputeLayout(doc, &textRect, &scrollRect);

    /* Clear text region */
    EraseRect(&textRect);

    /* Draw TextEdit content */
    updateRect = textRect;
    TEUpdate(&updateRect, doc->hTE);

    /* Prepare scrollbar gutter and redraw control */
    STView_ClearScrollArea(doc, &textRect, &scrollRect);
    STView_UpdateScrollMetrics(doc);
    STView_RedrawScrollBar(doc, &scrollRect);

    /* Ensure platform framebuffer reflects latest content */
    QDPlatform_FlushScreen();
}

void STView_ForceDraw(STDocument* doc) {
    if (!doc || !doc->window || !doc->hTE) return;

    GrafPtr oldPort;
    GetPort(&oldPort);
    SetPort((GrafPtr)doc->window);

    Rect dirty = (*doc->hTE)->viewRect;
    InvalRect(&dirty);
    if (doc->vScroll) {
        InvalRect(&(*doc->vScroll)->contrlRect);
    }

    {
        char logBuf[128];
        snprintf(logBuf, sizeof(logBuf), "[STView] ForceDraw window=%p\n", doc->window);
        serial_puts(logBuf);
    }

    BeginUpdate(doc->window);
    STView_Draw(doc);
    EndUpdate(doc->window);

    SetPort(oldPort);
}

/* Layout helpers ========================================================= */

static void STView_ComputeLayout(STDocument* doc, Rect* textRect, Rect* scrollRect) {
    if (!doc || !doc->window) return;

    Rect bounds = ((GrafPtr)doc->window)->portRect;
    Rect content = bounds;

    content.top += 4;
    content.left += 4;
    content.bottom -= 4;
    content.right -= kScrollBarWidth + 4;

    if (content.right < content.left + 16) {
        content.right = content.left + 16;
    }
    if (content.bottom < content.top + 16) {
        content.bottom = content.top + 16;
    }

    if (textRect) {
        *textRect = content;
    }

    if (scrollRect) {
        Rect sb;
        sb.top = content.top;
        sb.bottom = content.bottom;
        sb.right = bounds.right - 2;
        sb.left = sb.right - kScrollBarWidth;

        if (sb.left < content.right + 2) {
            sb.left = content.right + 2;
            sb.right = sb.left + kScrollBarWidth;
        }

        if (sb.left < bounds.left + 4) {
            sb.left = bounds.left + 4;
            sb.right = sb.left + kScrollBarWidth;
        }

        if (sb.bottom <= sb.top) {
            sb.bottom = sb.top + 1;
        }

        *scrollRect = sb;
    }
}

static void STView_RepositionScrollBar(STDocument* doc) {
    if (!doc || !doc->window || !doc->vScroll) return;

    Rect textRect;
    Rect scrollRect;

    STView_ComputeLayout(doc, &textRect, &scrollRect);

    SInt16 width = (SInt16)(scrollRect.right - scrollRect.left);
    SInt16 height = (SInt16)(scrollRect.bottom - scrollRect.top);
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    MoveControl(doc->vScroll, scrollRect.left, scrollRect.top);
   SizeControl(doc->vScroll, width, height);
}

static void STView_UpdateScrollMetrics(STDocument* doc) {
    if (!doc || !doc->hTE || !doc->vScroll) return;

    SInt16 viewHeight;
    SInt32 totalHeight;
    SInt32 maxScroll32;
    SInt16 scrollValue;

    HLock((Handle)doc->hTE);
    TEExtPtr pTE = (TEExtPtr)*doc->hTE;
    if (!pTE) {
        HUnlock((Handle)doc->hTE);
        return;
    }

    viewHeight = pTE->base.viewRect.bottom - pTE->base.viewRect.top;
    if (viewHeight < 1) viewHeight = 1;

    totalHeight = (SInt32)pTE->nLines * pTE->base.lineHeight;
    if (totalHeight < viewHeight) {
        totalHeight = viewHeight;
    }
    if (totalHeight > 32767) {
        totalHeight = 32767;
    }

    scrollValue = pTE->viewDV;
    HUnlock((Handle)doc->hTE);

    if (scrollValue < 0) scrollValue = 0;
    maxScroll32 = totalHeight - viewHeight;
    if (maxScroll32 < 0) maxScroll32 = 0;
    if (maxScroll32 > 32767) maxScroll32 = 32767;

    SInt16 maxScroll = (SInt16)maxScroll32;
    if (scrollValue > maxScroll) {
        scrollValue = maxScroll;
    }

    SetControlMinimum(doc->vScroll, 0);
    SetControlMaximum(doc->vScroll, maxScroll);
    SetScrollBarPageSize(doc->vScroll, viewHeight);

    if (GetControlValue(doc->vScroll) != scrollValue) {
        SetControlValue(doc->vScroll, scrollValue);
    }

}

static void STView_ClearScrollArea(STDocument* doc, const Rect* textRect, const Rect* scrollRect) {
    if (!doc || !doc->window || !scrollRect) return;

    GrafPtr oldPort;
    GetPort(&oldPort);
    SetPort((GrafPtr)doc->window);

    Pattern whitePat = {{0}};
    Pattern savedFill = ((GrafPtr)doc->window)->fillPat;
    Pattern savedPen = ((GrafPtr)doc->window)->pnPat;
    Pattern savedBack = ((GrafPtr)doc->window)->bkPat;
    BackPat(&whitePat);
    PenPat(&whitePat);

    if (textRect) {
        Rect gapRect = *textRect;
        gapRect.left = textRect->right;
        gapRect.right = scrollRect->left;
        if (gapRect.right > gapRect.left) {
            FillRect(&gapRect, &whitePat);
        }
    }

    Rect scrollFill = *scrollRect;
    FillRect(&scrollFill, &whitePat);

    PenPat(&savedPen);
    ((GrafPtr)doc->window)->fillPat = savedFill;
    BackPat(&savedBack);

    SetPort(oldPort);
}

static void STView_RedrawScrollBar(STDocument* doc, const Rect* scrollRect) {
    if (!doc || !doc->vScroll) return;

    Rect controlRect = (*doc->vScroll)->contrlRect;
    Rect targetRect;

    if (scrollRect) {
        targetRect = *scrollRect;
    } else {
        targetRect = controlRect;
    }

    RgnHandle clipRgn = NULL;
    if (EqualRect(&targetRect, &controlRect)) {
        Draw1Control(doc->vScroll);
    } else {
        clipRgn = NewRgn();
        if (clipRgn) {
            RectRgn(clipRgn, &targetRect);
            UpdateControls(doc->window, clipRgn);
            DisposeRgn(clipRgn);
        } else {
            Draw1Control(doc->vScroll);
        }
    }
}

/*
 * STView_Click - Handle mouse click in view
 */
void STView_Click(STDocument* doc, EventRecord* event) {
    Point localPt;
    Boolean shiftDown;
    static UInt32 lastClickTime = 0;
    static Point lastClickPt = {0, 0};
    static int clickCount = 1;

    if (!doc || !doc->hTE) return;

    SetPort((GrafPtr)doc->window);

    /* Convert to local coordinates */
    localPt = event->where;
    GlobalToLocal(&localPt);

    /* Check for shift key (extend selection) */
    shiftDown = (event->modifiers & shiftKey) != 0;

    /* Check for double/triple click */
    if ((event->when - lastClickTime) < GetDblTime()) {
        /* Quick enough for multi-click */
        if (abs(localPt.h - lastClickPt.h) < 3 && abs(localPt.v - lastClickPt.v) < 3) {
            clickCount++;
            if (clickCount == 2) {
                /* Double-click - select word */
                ST_Log("Double-click at (%d,%d) - selecting word\n", localPt.h, localPt.v);

                /* Get click position */
                SInt16 offset = TEGetOffset(localPt, doc->hTE);

                /* Find word boundaries */
                extern SInt32 TE_FindWordBoundary(TEHandle hTE, SInt32 offset, Boolean forward);
                SInt32 wordStart = TE_FindWordBoundary(doc->hTE, offset, false);
                SInt32 wordEnd = TE_FindWordBoundary(doc->hTE, offset, true);

                /* Select the word */
                TESetSelect(wordStart, wordEnd, doc->hTE);
            } else if (clickCount >= 3) {
                /* Triple-click - select line */
                ST_Log("Triple-click at (%d,%d) - selecting line\n", localPt.h, localPt.v);

                /* Get click position */
                SInt16 offset = TEGetOffset(localPt, doc->hTE);

                /* Find line boundaries */
                extern SInt32 TE_FindLineStart(TEHandle hTE, SInt32 offset);
                extern SInt32 TE_FindLineEnd(TEHandle hTE, SInt32 offset);
                SInt32 lineStart = TE_FindLineStart(doc->hTE, offset);
                SInt32 lineEnd = TE_FindLineEnd(doc->hTE, offset);

                /* Select the line */
                TESetSelect(lineStart, lineEnd, doc->hTE);
                clickCount = 3;  /* Cap at triple */
            }
        } else {
            clickCount = 1;
        }
    } else {
        clickCount = 1;
    }

    lastClickTime = event->when;
    lastClickPt = localPt;

    /* Handle regular click or shift-click */
    TEClick(localPt, shiftDown, doc->hTE);

    /* Save undo state after selection change */
    STClip_SaveUndo(doc);
}

/*
 * STView_Key - Handle keyboard input
 */
void STView_Key(STDocument* doc, EventRecord* event) {
    char key;
    SInt16 selStart, selEnd;

    if (!doc || !doc->hTE) return;

    key = event->message & charCodeMask;

    /* Get current selection */
    selStart = (*doc->hTE)->selStart;
    selEnd = (*doc->hTE)->selEnd;

    /* Check for special keys */
    switch (key) {
        case 0x08:  /* Backspace */
            if (selStart == selEnd && selStart > 0) {
                /* Delete character before cursor */
                TESetSelect(selStart - 1, selStart, doc->hTE);
            }
            TEDelete(doc->hTE);
            break;

        case 0x7F:  /* Delete */
            if (selStart == selEnd && selStart < (*doc->hTE)->teLength) {
                /* Delete character after cursor */
                TESetSelect(selStart, selStart + 1, doc->hTE);
            }
            TEDelete(doc->hTE);
            break;

        case 0x0D:  /* Return */
            /* Convert to CR for classic Mac */
            TEKey('\r', doc->hTE);
            break;

        case 0x1C:  /* Left arrow */
            if (selStart > 0) {
                if (event->modifiers & cmdKey) {
                    /* Cmd-Left: beginning of line */
                    extern SInt32 TE_FindLineStart(TEHandle hTE, SInt32 offset);
                    SInt32 lineStart = TE_FindLineStart(doc->hTE, selStart);
                    TESetSelect(lineStart, lineStart, doc->hTE);
                } else {
                    TESetSelect(selStart - 1, selStart - 1, doc->hTE);
                }
            }
            break;

        case 0x1D:  /* Right arrow */
            if (selEnd < (*doc->hTE)->teLength) {
                if (event->modifiers & cmdKey) {
                    /* Cmd-Right: end of line */
                    extern SInt32 TE_FindLineEnd(TEHandle hTE, SInt32 offset);
                    SInt32 lineEnd = TE_FindLineEnd(doc->hTE, selEnd);
                    TESetSelect(lineEnd, lineEnd, doc->hTE);
                } else {
                    TESetSelect(selEnd + 1, selEnd + 1, doc->hTE);
                }
            }
            break;

        case 0x1E:  /* Up arrow */ {
            /* Move cursor up one line */
            Point cursorPos = TEGetPoint(selEnd, doc->hTE);
            SInt16 lineHeight = (*doc->hTE)->lineHeight;

            if (lineHeight <= 0) lineHeight = 12;  /* Default if not set */

            /* Calculate point one line above */
            Point targetPos = cursorPos;
            targetPos.v -= lineHeight;

            /* Don't go above the text bounds */
            if (targetPos.v < (*doc->hTE)->viewRect.top) {
                targetPos.v = (*doc->hTE)->viewRect.top;
            }

            /* Convert point to offset */
            SInt16 newOffset = TEGetOffset(targetPos, doc->hTE);
            TESetSelect(newOffset, newOffset, doc->hTE);
            break;
        }

        case 0x1F:  /* Down arrow */ {
            /* Move cursor down one line */
            Point cursorPos = TEGetPoint(selEnd, doc->hTE);
            SInt16 lineHeight = (*doc->hTE)->lineHeight;

            if (lineHeight <= 0) lineHeight = 12;  /* Default if not set */

            /* Calculate point one line below */
            Point targetPos = cursorPos;
            targetPos.v += lineHeight;

            /* Don't go below the text bounds */
            if (targetPos.v > (*doc->hTE)->viewRect.bottom) {
                targetPos.v = (*doc->hTE)->viewRect.bottom;
            }

            /* Convert point to offset */
            SInt16 newOffset = TEGetOffset(targetPos, doc->hTE);
            TESetSelect(newOffset, newOffset, doc->hTE);
            break;
        }

        default:
            /* Regular character */
            if (key >= 0x20 || key == 0x09) {  /* Printable or tab */
                /* Check for overflow */
                if ((*doc->hTE)->teLength >= kMaxFileSize) {
                    ST_Beep();
                    ST_Log("Text buffer overflow\n");
                    return;
                }
                TEKey(key, doc->hTE);
            }
            break;
    }

    /* Mark as dirty */
    STView_Draw(doc);
    STDoc_SetDirty(doc, true);
}

/*
 * STView_Resize - Handle window resize
 */
void STView_Resize(STDocument* doc) {
    Rect destRect, viewRect;

    if (!doc || !doc->hTE || !doc->window) return;

    ST_Log("Resizing TextEdit view\n");

    SetPort((GrafPtr)doc->window);

    STView_ComputeLayout(doc, &destRect, NULL);

    viewRect = destRect;

    /* Update TextEdit rectangles */
    (*doc->hTE)->destRect = destRect;
    (*doc->hTE)->viewRect = viewRect;

    /* Recalculate line breaks */
    TECalText(doc->hTE);

    /* Invalidate window */
    InvalRect(&((GrafPtr)doc->window)->portRect);

    STView_RepositionScrollBar(doc);
    STView_UpdateScrollMetrics(doc);
}

/*
 * STView_Scroll - Scroll view
 */
void STView_Scroll(STDocument* doc, SInt16 dv, SInt16 dh) {
    if (!doc || !doc->hTE) return;

    /* Use TEScroll to scroll content */
    TEScroll(dh, dv, doc->hTE);
    STView_UpdateScrollMetrics(doc);
}

/*
 * STView_SetStyle - Apply style to selection
 */
void STView_SetStyle(STDocument* doc, SInt16 font, SInt16 size, Style style) {
    if (!doc || !doc->hTE) return;

    ST_Log("Setting style: font=%d size=%d style=%d\n", font, size, style);

    /* Apply to selection or insertion point */
    ApplyStyleToSelection(doc, font, size, style);

    /* Mark as dirty */
    STDoc_SetDirty(doc, true);
}

/*
 * STView_GetStyle - Get style at selection
 */
void STView_GetStyle(STDocument* doc, SInt16* font, SInt16* size, Style* style) {
    if (!doc || !doc->hTE) return;

    GetSelectionStyle(doc, font, size, style);
}

/*
 * STView_UpdateCaret - Update caret blink
 */
void STView_UpdateCaret(STDocument* doc) {
    if (!doc || !doc->hTE) return;

    /* Let TextEdit handle caret */
    TEIdle(doc->hTE);
}

/*
 * ApplyStyleToSelection - Apply text style to current selection
 */
static void ApplyStyleToSelection(STDocument* doc, SInt16 font, SInt16 size, Style style) {
    SInt16 selStart, selEnd;

    if (!doc || !doc->hTE) return;

    selStart = (*doc->hTE)->selStart;
    selEnd = (*doc->hTE)->selEnd;

    /* Set the default style for the TE record.
     * System 7 TextEdit applies font/face/size uniformly to the entire record.
     * Per-range styling requires TESetStyle from TextFormatting (not yet in build). */
    (*doc->hTE)->txFont = font;
    (*doc->hTE)->txSize = size;
    (*doc->hTE)->txFace = style;

    if (selStart != selEnd) {
        /* Recalculate line breaks and redraw with new style */
        TECalText(doc->hTE);
        InvalRect(&(*doc->hTE)->viewRect);
    }

    /* Update global current style */
    g_ST.currentFont = font;
    g_ST.currentSize = size;
    g_ST.currentStyle = style;
}

/*
 * GetSelectionStyle - Get style at current selection
 */
static void GetSelectionStyle(STDocument* doc, SInt16* font, SInt16* size, Style* style) {
    if (!doc || !doc->hTE) return;

    /* Return the current TE record default style */
    if (font) *font = (*doc->hTE)->txFont;
    if (size) *size = (*doc->hTE)->txSize;
    if (style) *style = (*doc->hTE)->txFace;
}
