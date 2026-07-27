/* #include "SystemTypes.h" */
#include "SystemTypes.h"
/************************************************************
 *
 * TextDisplay.c
 * System 7.1 Portable - TextEdit Display and Rendering
 *
 * Text rendering, scrolling, and viewport management for
 * TextEdit. Handles all visual aspects of text display
 * including selection highlighting and caret drawing.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Derived from ROM analysis: TextEdit Manager 1985-1992
 *
 ************************************************************/

// #include "CompatibilityFix.h" // Removed
#include "TextEdit/TextEdit.h"
#include "TextEdit/TextTypes.h"
#include "TextEdit/TextSelection.h"
#include "MemoryMgr/memory_manager_types.h"
#include "QuickDraw/QuickDraw.h"
#include "Fonts.h"


/************************************************************
 * Display Constants
 ************************************************************/

#define kTECaretWidth        1
#define kTECaretBlinkRate    30    /* 30 ticks = 0.5 seconds */
#define kTEDefaultTabWidth   32    /* Default tab width in pixels */
#define kTEMaxLineWidth      32767 /* Maximum line width */

/************************************************************
 * Internal Display Utilities
 ************************************************************/

static void TESetTextAttributes(TEHandle hTE)
{
    TERec **teRec;
    GrafPtr currentPort;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;
    GetPort(&currentPort);

    if (currentPort) {
        TextFont((**teRec).txFont);
        TextFace((**teRec).txFace);
        TextSize((**teRec).txSize);
        TextMode((**teRec).txMode);
    }

    (**teRec).inPort = currentPort;
}

static short TEGetCharWidth(char ch, TEHandle hTE)
{
    TERec **teRec;
    FontInfo fontInfo;

    if (!hTE || !*hTE) return 0;

    teRec = (TERec **)hTE;

    TESetTextAttributes(hTE);

    if (ch == '\t') {
        /* Tab character - return tab width */
        return kTEDefaultTabWidth;
    }

    if (ch < 0x20) {
        /* Control character - no width */
        return 0;
    }

    /* Get character width using QuickDraw */
    return CharWidth(ch);
}

static short TEGetTextWidth(Ptr textPtr, short length, TEHandle hTE)
{
    short totalWidth = 0;
    short i;

    if (!textPtr || length <= 0 || !hTE) return 0;

    TESetTextAttributes(hTE);

    for (i = 0; i < length; i++) {
        totalWidth += TEGetCharWidth(textPtr[i], hTE);
    }

    return totalWidth;
}

static void TEGetFontMetrics(TEHandle hTE, FontInfo *fontInfo)
{
    if (!hTE || !fontInfo) return;

    TESetTextAttributes(hTE);
    GetFontInfo(fontInfo);
}

static Point TECalculateTextPosition(TEHandle hTE, long offset)
{
    TERec **teRec;
    Point position;
    Handle textHandle;
    char *textPtr;
    long currentOffset = 0;
    short currentLine = 0;
    short lineStart, lineEnd;
    FontInfo fontInfo;

    if (!hTE || !*hTE) {
        position.h = 0;
        position.v = 0;
        return position;
    }

    teRec = (TERec **)hTE;
    textHandle = (**teRec).hText;

    position.h = (**teRec).destRect.left;
    position.v = (**teRec).destRect.top;

    if (!textHandle || offset <= 0) return position;

    /* Validate nLines to prevent underflow in loop bounds */
    if ((**teRec).nLines <= 0) return position;

    TEGetFontMetrics(hTE, &fontInfo);

    HLock(textHandle);
    textPtr = *textHandle;

    /* Find which line contains the offset */
    for (currentLine = 0; currentLine < (**teRec).nLines - 1; currentLine++) {
        if (offset < (**teRec).lineStarts[currentLine + 1]) {
            break;
        }
    }

    /* Calculate vertical position */
    position.v += (currentLine * (**teRec).lineHeight) + fontInfo.ascent;

    /* Calculate horizontal position within the line */
    lineStart = (**teRec).lineStarts[currentLine];
    if (currentLine < (**teRec).nLines - 1) {
        lineEnd = (**teRec).lineStarts[currentLine + 1] - 1;
    } else {
        lineEnd = (**teRec).teLength;
    }

    if (offset > lineStart) {
        long charsInLine = offset - lineStart;
        if (charsInLine > lineEnd - lineStart) {
            charsInLine = lineEnd - lineStart;
        }
        position.h += TEGetTextWidth(textPtr + lineStart, charsInLine, hTE);
    }

    /* Apply justification */
    switch ((**teRec).just) {
        case teJustCenter:
            {
                short lineWidth = TEGetTextWidth(textPtr + lineStart, lineEnd - lineStart, hTE);
                short rectWidth = (**teRec).destRect.right - (**teRec).destRect.left;
                position.h = (**teRec).destRect.left + ((rectWidth - lineWidth) / 2);
                if (offset > lineStart) {
                    position.h += TEGetTextWidth(textPtr + lineStart, offset - lineStart, hTE);
                }
            }
            break;

        case teJustRight:
            {
                short lineWidth = TEGetTextWidth(textPtr + lineStart, lineEnd - lineStart, hTE);
                short rectWidth = (**teRec).destRect.right - (**teRec).destRect.left;
                position.h = (**teRec).destRect.right - lineWidth;
                if (offset > lineStart) {
                    position.h += TEGetTextWidth(textPtr + lineStart, offset - lineStart, hTE);
                }
            }
            break;

        default: /* teJustLeft */
            /* Already calculated above */
            break;
    }

    HUnlock(textHandle);
    return position;
}

/************************************************************
 * Core Display Functions
 ************************************************************/

pascal void TEUpdate(const Rect *rUpdate, TEHandle hTE)
{
    TERec **teRec;
    GrafPtr currentPort;
    Rect drawRect;
    RgnHandle updateRgn = NULL;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;

    GetPort(&currentPort);
    if (!currentPort) return;

    /* Set up drawing context */
    TESetTextAttributes(hTE);

    /* Determine update area */
    if (rUpdate) {
        drawRect = *rUpdate;
        SectRect(&drawRect, &(**teRec).destRect, &drawRect);
    } else {
        drawRect = (**teRec).destRect;
    }

    /* Create update region */
    updateRgn = NewRgn();
    if (updateRgn) {
        RectRgn(updateRgn, &drawRect);
    }

    /* Clear the update area */
    EraseRect(&drawRect);

    /* Draw the text */
    TEDrawText(hTE, &drawRect);

    /* Draw selection if active */
    if ((**teRec).active && (**teRec).selStart != (**teRec).selEnd) {
        TEDrawSelection(hTE);
    }

    /* Draw caret if active and visible */
    if ((**teRec).active && (**teRec).selStart == (**teRec).selEnd && (**teRec).caretState) {
        TEDrawCaret(hTE);
    }

    /* Clean up */
    if (updateRgn) {
        DisposeRgn(updateRgn);
    }
}

OSErr TEDrawText(TEHandle hTE, const Rect *drawRect)
{
    TERec **teRec;
    Handle textHandle;
    char *textPtr;
    FontInfo fontInfo;
    short lineNumber;
    long lineStart, lineEnd;
    short drawY;
    Rect lineRect;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;
    textHandle = (**teRec).hText;

    if (!textHandle || (**teRec).teLength == 0) return noErr;

    TESetTextAttributes(hTE);
    TEGetFontMetrics(hTE, &fontInfo);

    HLock(textHandle);
    textPtr = *textHandle;

    /* Draw each line */
    for (lineNumber = 0; lineNumber < (**teRec).nLines; lineNumber++) {
        lineStart = (**teRec).lineStarts[lineNumber];

        if (lineNumber < (**teRec).nLines - 1) {
            lineEnd = (**teRec).lineStarts[lineNumber + 1];
            /* Skip line break character */
            if (lineEnd > lineStart && (textPtr[lineEnd - 1] == '\r' || textPtr[lineEnd - 1] == '\n')) {
                lineEnd--;
            }
        } else {
            lineEnd = (**teRec).teLength;
        }

        /* Calculate line rectangle */
        lineRect = (**teRec).destRect;
        lineRect.top = (**teRec).destRect.top + (lineNumber * (**teRec).lineHeight);
        lineRect.bottom = lineRect.top + (**teRec).lineHeight;

        /* Check if line intersects with draw rectangle */
        if (drawRect && !SectRect(&lineRect, drawRect, &lineRect)) {
            continue;
        }

        drawY = lineRect.top + fontInfo.ascent;

        if (lineEnd > lineStart) {
            /* Draw the line of text */
            MoveTo(lineRect.left, drawY);

            /* Apply justification */
            switch ((**teRec).just) {
                case teJustCenter:
                    {
                        short lineWidth = TEGetTextWidth(textPtr + lineStart, lineEnd - lineStart, hTE);
                        short rectWidth = lineRect.right - lineRect.left;
                        MoveTo(lineRect.left + ((rectWidth - lineWidth) / 2), drawY);
                    }
                    break;

                case teJustRight:
                    {
                        short lineWidth = TEGetTextWidth(textPtr + lineStart, lineEnd - lineStart, hTE);
                        MoveTo(lineRect.right - lineWidth, drawY);
                    }
                    break;
            }

            /* Draw characters in the line */
            DrawText(textPtr + lineStart, 0, lineEnd - lineStart);
        }
    }

    HUnlock(textHandle);
    return noErr;
}

OSErr TEDrawSelection(TEHandle hTE)
{
    TERec **teRec;
    long selStart, selEnd;
    Point startPt, endPt;
    Rect highlightRect;
    Pattern grayPattern;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    selStart = (**teRec).selStart;
    selEnd = (**teRec).selEnd;

    if (selStart == selEnd) return noErr; /* No selection to draw */

    /* Calculate selection rectangle(s) */
    startPt = TECalculateTextPosition(hTE, selStart);
    endPt = TECalculateTextPosition(hTE, selEnd);

    /* Simple case: selection on single line */
    if (startPt.v == endPt.v) {
        SetRect(&highlightRect, startPt.h, startPt.v - (**teRec).fontAscent,
                endPt.h, startPt.v + ((**teRec).lineHeight - (**teRec).fontAscent));

        /* Highlight the selection */
        GetQDGlobalsGray(&grayPattern);
        PenMode(patXor);
        PenPat(&grayPattern);
        PaintRect(&highlightRect);
        PenNormal();
    } else {
        /* Multi-line selection - draw each line separately */
        FontInfo fontInfo;
        short lineHeight;
        short currentLine;
        long lineStart, lineEnd;
        Handle textHandle = (**teRec).hText;

        TEGetFontMetrics(hTE, &fontInfo);
        lineHeight = (**teRec).lineHeight;

        GetQDGlobalsGray(&grayPattern);
        PenMode(patXor);
        PenPat(&grayPattern);

        /* Find starting line */
        for (currentLine = 0; currentLine < (**teRec).nLines; currentLine++) {
            lineStart = (**teRec).lineStarts[currentLine];
            if (currentLine < (**teRec).nLines - 1) {
                lineEnd = (**teRec).lineStarts[currentLine + 1];
            } else {
                lineEnd = (**teRec).teLength;
            }

            if (selEnd <= lineStart) break;
            if (selStart >= lineEnd) continue;

            /* Calculate highlight rectangle for this line */
            long lineSelStart = (selStart > lineStart) ? selStart : lineStart;
            long lineSelEnd = (selEnd < lineEnd) ? selEnd : lineEnd;

            Point lineStartPt = TECalculateTextPosition(hTE, lineSelStart);
            Point lineEndPt = TECalculateTextPosition(hTE, lineSelEnd);

            SetRect(&highlightRect, lineStartPt.h, lineStartPt.v - (**teRec).fontAscent,
                    lineEndPt.h, lineStartPt.v + (lineHeight - (**teRec).fontAscent));

            PaintRect(&highlightRect);
        }

        PenNormal();
    }

    return noErr;
}

OSErr TEDrawCaret(TEHandle hTE)
{
    TERec **teRec;
    Point caretPt;
    Rect caretRect;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    if (!(**teRec).caretState) return noErr; /* Caret not visible */

    /* Calculate caret position */
    caretPt = TECalculateTextPosition(hTE, (**teRec).selStart);

    /* Draw caret as a vertical line */
    SetRect(&caretRect, caretPt.h, caretPt.v - (**teRec).fontAscent,
            caretPt.h + kTECaretWidth, caretPt.v + ((**teRec).lineHeight - (**teRec).fontAscent));

    PenMode(patXor);
    PaintRect(&caretRect);
    PenNormal();

    return noErr;
}

/************************************************************
 * Scrolling Functions
 ************************************************************/

pascal void TEScroll(short dh, short dv, TEHandle hTE)
{
    TERec **teRec;
    GrafPtr currentPort;
    Rect scrollRect;
    RgnHandle updateRgn;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;

    GetPort(&currentPort);
    if (!currentPort) return;

    /* Scroll the content */
    scrollRect = (**teRec).viewRect;
    updateRgn = NewRgn();

    if (updateRgn) {
        ScrollRect(&scrollRect, dh, dv, updateRgn);

        /* Update the view rectangle offset */
        OffsetRect(&(**teRec).viewRect, -dh, -dv);

        /* Update any exposed areas */
        if (!EmptyRgn(updateRgn)) {
            TEUpdate(NULL, hTE);
        }

        DisposeRgn(updateRgn);
    }
}

pascal void TEPinScroll(short dh, short dv, TEHandle hTE)
{
    TERec **teRec;
    FontInfo fontInfo;
    short maxVScroll, maxHScroll;
    short totalHeight, totalWidth;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;

    TEGetFontMetrics(hTE, &fontInfo);

    /* Calculate maximum scroll values */
    totalHeight = (**teRec).nLines * (**teRec).lineHeight;
    totalWidth = (**teRec).destRect.right - (**teRec).destRect.left;

    maxVScroll = totalHeight - ((**teRec).viewRect.bottom - (**teRec).viewRect.top);
    if (maxVScroll < 0) maxVScroll = 0;

    maxHScroll = totalWidth - ((**teRec).viewRect.right - (**teRec).viewRect.left);
    if (maxHScroll < 0) maxHScroll = 0;

    /* Pin scroll deltas to valid range */
    if (dv < -maxVScroll) dv = -maxVScroll;
    if (dv > maxVScroll) dv = maxVScroll;
    if (dh < -maxHScroll) dh = -maxHScroll;
    if (dh > maxHScroll) dh = maxHScroll;

    /* Perform the scroll */
    if (dh != 0 || dv != 0) {
        TEScroll(dh, dv, hTE);
    }
}

pascal void TESelView(TEHandle hTE)
{
    TERec **teRec;
    Point selPt;
    Rect viewRect;
    short dh = 0, dv = 0;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;
    viewRect = (**teRec).viewRect;

    /* Get the position of the selection start */
    selPt = TECalculateTextPosition(hTE, (**teRec).selStart);

    /* Calculate scroll needed to bring selection into view */
    if (selPt.h < viewRect.left) {
        dh = selPt.h - viewRect.left;
    } else if (selPt.h >= viewRect.right) {
        dh = selPt.h - viewRect.right + 1;
    }

    if (selPt.v < viewRect.top) {
        dv = selPt.v - viewRect.top;
    } else if (selPt.v >= viewRect.bottom) {
        dv = selPt.v - viewRect.bottom + (**teRec).lineHeight;
    }

    /* Scroll if necessary */
    if (dh != 0 || dv != 0) {
        TEScroll(dh, dv, hTE);
    }
}

pascal void TEAutoView(Boolean fAuto, TEHandle hTE)
{
    TERec **teRec;
    TEDispatchHandle hDispatch;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;
    hDispatch = (TEDispatchHandle)(**teRec).hDispatchRec;

    if (hDispatch) {
        TEDispatchRec **dispatch = (TEDispatchRec **)hDispatch;
        if (fAuto) {
            (**dispatch).newTEFlags |= (1 << teFAutoScroll);
        } else {
            (**dispatch).newTEFlags &= ~(1 << teFAutoScroll);
        }
    }
}

/************************************************************
 * Text Layout Functions
 ************************************************************/

pascal void TECalText(TEHandle hTE)
{
    TERec **teRec;
    Handle textHandle;
    char *textPtr;
    long textLen;
    short lineCount = 0;
    long i;
    FontInfo fontInfo;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;
    textHandle = (**teRec).hText;

    if (!textHandle) return;

    HLock(textHandle);
    textPtr = *textHandle;
    textLen = (**teRec).teLength;

    /* Clear existing line starts */
    (**teRec).lineStarts[lineCount++] = 0;

    /* Calculate line breaks based on text content and word wrapping */
    for (i = 0; i < textLen && lineCount < 16000; i++) {
        if (textPtr[i] == '\r' || textPtr[i] == '\n') {
            /* Hard line break */
            if (i + 1 < textLen && lineCount < 16000) {
                (**teRec).lineStarts[lineCount++] = i + 1;
            }
        }
        /* Word wrapping logic would go here for soft line breaks */
    }

    (**teRec).nLines = lineCount;

    /* Update font metrics */
    TEGetFontMetrics(hTE, &fontInfo);
    (**teRec).lineHeight = fontInfo.ascent + fontInfo.descent + fontInfo.leading;
    (**teRec).fontAscent = fontInfo.ascent;

    HUnlock(textHandle);
}

/************************************************************
 * TextBox Function
 ************************************************************/

pascal void TETextBox(const void *text, long length, const Rect *box, short just)
{
    GrafPtr currentPort;
    FontInfo fontInfo;
    short lineHeight;
    const char *textPtr = (const char *)text;
    long currentPos = 0;
    short currentY;
    Rect textRect;

    if (!text || length <= 0 || !box) return;

    GetPort(&currentPort);
    if (!currentPort) return;

    GetFontInfo(&fontInfo);
    lineHeight = fontInfo.ascent + fontInfo.descent + fontInfo.leading;

    textRect = *box;
    currentY = textRect.top + fontInfo.ascent;

    /* Simple text drawing - breaks on \r or \n */
    while (currentPos < length && currentY < textRect.bottom) {
        long lineStart = currentPos;
        long lineEnd = lineStart;

        /* Find end of current line */
        while (lineEnd < length && textPtr[lineEnd] != '\r' && textPtr[lineEnd] != '\n') {
            lineEnd++;
        }

        if (lineEnd > lineStart) {
            short lineWidth;
            short drawX = textRect.left;

            /* Calculate line width */
            lineWidth = TextWidth(textPtr + lineStart, 0, lineEnd - lineStart);

            /* Apply justification */
            switch (just) {
                case teJustCenter:
                    drawX = textRect.left + ((textRect.right - textRect.left - lineWidth) / 2);
                    break;
                case teJustRight:
                    drawX = textRect.right - lineWidth;
                    break;
            }

            /* Draw the line */
            MoveTo(drawX, currentY);
            DrawText(textPtr + lineStart, 0, lineEnd - lineStart);
        }

        /* Move to next line */
        currentPos = lineEnd;
        if (currentPos < length && (textPtr[currentPos] == '\r' || textPtr[currentPos] == '\n')) {
            currentPos++;
        }
        currentY += lineHeight;
    }
}

pascal void TextBox(const void *text, long length, const Rect *box, short just)
{
    TETextBox(text, length, box, just);
}

/************************************************************
 * Advanced Display Features
 ************************************************************/

OSErr TESetDisplayMode(TEHandle hTE, short mode)
{
    /* Enhanced display modes would be implemented here */
    return unimpErr;
}

OSErr TEGetDisplayBounds(TEHandle hTE, Rect *bounds)
{
    TERec **teRec;

    if (!hTE || !*hTE || !bounds) return paramErr;

    teRec = (TERec **)hTE;
    *bounds = (**teRec).destRect;

    return noErr;
}

OSErr TEInvalidateRange(TEHandle hTE, long start, long end)
{
    Point startPt, endPt;
    Rect invalidRect;
    GrafPtr currentPort;

    if (!hTE || !*hTE) return paramErr;

    GetPort(&currentPort);
    if (!currentPort) return paramErr;

    /* Calculate invalidation rectangle */
    startPt = TECalculateTextPosition(hTE, start);
    endPt = TECalculateTextPosition(hTE, end);

    TERec **teRec = (TERec **)hTE;

    SetRect(&invalidRect,
            (startPt.h < endPt.h) ? startPt.h : endPt.h,
            (startPt.v < endPt.v) ? startPt.v - (**teRec).fontAscent : endPt.v - (**teRec).fontAscent,
            (startPt.h > endPt.h) ? startPt.h : endPt.h,
            (startPt.v > endPt.v) ? startPt.v + (**teRec).lineHeight - (**teRec).fontAscent :
                                   endPt.v + (**teRec).lineHeight - (**teRec).fontAscent);

    /* Invalidate the rectangle */
    InvalRect(&invalidRect);

    return noErr;
}
