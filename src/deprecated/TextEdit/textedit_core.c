#include "MemoryMgr/MemoryManager.h"
/* #include "SystemTypes.h" */
#include <stdlib.h>
#include <string.h>
/* RE-AGENT-BANNER
 * TextEdit Implementation - Apple System 7.1 TextEdit Manager
 *
 * implemented based on: System.rsrc
 * Architecture: m68k (68000)
 * ABI: classic_macos
 *

 * Mappings: mappings.textedit.json
 * Layouts: layouts.curated.textedit.json
 *
 * This file implements the Mac OS TextEdit Manager, providing text editing
 * functionality for applications. Implementation based on reverse engineering
 * of System 7.1 TextEdit from System resource fork.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "TextEdit/TextEdit.h"
#include "TextEdit/TELogging.h"


/* Static TextEdit state - Evidence: System-wide TextEdit initialization */
static int g_TEInitialized = 0;

/* Helper functions - Internal implementation details */
static void TECalcLines(TEHandle hTE);
static void TESetupFont(TEHandle hTE);
static void TEDrawText(TEHandle hTE);
static void TEUpdateCaret(TEHandle hTE);

/* TEInit - Initialize TextEdit subsystem
 * Trap: 0xA9CC, Evidence: offset 0x00003780, size 47
 * Provenance: evidence.curated.textedit.json:functions[0]
 */
void TEInit(void)
{
    /*
    if (!g_TEInitialized) {
        g_TEInitialized = 1;
        /* System-level TextEdit manager initialization */
        /* Original implementation would set up global TextEdit state */
    }
}

/* TENew - Create new TextEdit record
 * Trap: 0xA9D2, Evidence: offset 0x00003f00, size 125
 * Provenance: evidence.curated.textedit.json:functions[1]
 * Layout: layouts.curated.textedit.json:TERec
 */
TEHandle TENew(Rect *destRect, Rect *viewRect)
{
    TEHandle hTE;
    TEPtr pTE;

    /*
    hTE = (TEHandle)NewPtr(sizeof(TEPtr));
    if (!hTE) return NULL;

    pTE = (TEPtr)NewPtr(sizeof(TERec));
    if (!pTE) {
        DisposePtr((Ptr)hTE);
        return NULL;
    }
    *hTE = pTE;

    /* Initialize TERec fields - Layout: layouts.curated.textedit.json */
    if (destRect) pTE->destRect = *destRect;
    if (viewRect) pTE->viewRect = *viewRect;

    /* Initialize selection and text state */
    pTE->selStart = 0;
    pTE->selEnd = 0;
    pTE->active = 0;
    pTE->wordWrap = 1;
    pTE->teLength = 0;
    pTE->just = teJustLeft;  /*

    /* Initialize text handles */
    pTE->hText = NULL;
    pTE->hDispText = NULL;

    /* Initialize font attributes */
    pTE->txFont = 0;    /* Default font */
    pTE->txFace = 0;    /* Plain style */
    pTE->txSize = 12;   /* 12 point default */
    pTE->lineHeight = 16;
    pTE->fontAscent = 12;

    /* Initialize callback pointers */
    pTE->clickLoop = NULL;
    pTE->clikLoop = NULL;
    pTE->highHook = NULL;
    pTE->caretHook = NULL;

    return hTE;
}

/* TEDispose - Dispose TextEdit record
 * Trap: 0xA9CD, Evidence: offset 0x00004180, size 38
 * Provenance: evidence.curated.textedit.json:functions[2]
 */
void TEDispose(TEHandle hTE)
{
    TEPtr pTE;

    if (!hTE) return;

    pTE = *hTE;
    if (pTE) {
        /*
        if (pTE->hText) {
            DisposePtr((Ptr)pTE->hText);
        }
        if (pTE->hDispText) {
            DisposePtr((Ptr)pTE->hDispText);
        }
        DisposePtr((Ptr)pTE);
    }
    DisposePtr((Ptr)hTE);
}

/* TESetText - Set text content
 * Trap: 0xA9CF, Evidence: offset 0x00004280, size 89
 * Provenance: evidence.curated.textedit.json:functions[3]
 */
void TESetText(void *text, SInt32 length, TEHandle hTE)
{
    TEPtr pTE;
    char *textPtr;

    if (!hTE || !*hTE) return;
    pTE = *hTE;

    /*
    if (pTE->hText) {
        DisposePtr((Ptr)pTE->hText);
    }

    if (length > 0 && text) {
        pTE->hText = NewPtr(length + 1);
        if (pTE->hText) {
            textPtr = (char*)pTE->hText;
            memcpy(textPtr, text, length);
            textPtr[length] = '\0';
            pTE->teLength = length;
        }
    } else {
        pTE->hText = NULL;
        pTE->teLength = 0;
    }

    /* Reset selection */
    pTE->selStart = 0;
    pTE->selEnd = 0;

    /* Recalculate line breaks */
    TECalcLines(hTE);
}

/* TEGetText - Get text handle
 * Trap: 0xA9CB, Evidence: offset 0x00004350, size 42
 * Provenance: evidence.curated.textedit.json:functions[4]
 */
CharsHandle TEGetText(TEHandle hTE)
{
    if (!hTE || !*hTE) return NULL;

    /*
    return (CharsHandle)((*hTE)->hText);
}

/* TEClick - Handle mouse clicks
 * Trap: 0xA9D4, Evidence: offset 0x00004450, size 167
 * Provenance: evidence.curated.textedit.json:functions[5]
 */
void TEClick(Point pt, SInt16 extend, TEHandle hTE)
{
    TEPtr pTE;
    SInt32 charPos = 0;

    if (!hTE || !*hTE) return;
    pTE = *hTE;

    /* Convert point to character position - simplified single-line mapping */
    {
        /* Compute x within the view rect */
        SInt16 avgCharWidth;
        SInt32 lineStart = 0;
        SInt32 lineEnd = pTE->teLength;
        SInt16 x = pt.h - pTE->viewRect.left;
        SInt16 y = pt.v - pTE->viewRect.top;

        /* If we have multiple lines recorded, use them to narrow the range */
        if (pTE->nLines > 0 && pTE->lineStarts[0] == 0) {
            SInt16 line = y / (pTE->lineHeight > 0 ? pTE->lineHeight : 1);
            if (line < 0) line = 0;
            if (line >= pTE->nLines) line = pTE->nLines - 1;
            lineStart = pTE->lineStarts[line];
            lineEnd = (line + 1 < pTE->nLines) ? pTE->lineStarts[line + 1] : pTE->teLength;
        }

        /* Estimate average character width from font size (approx 2/3 em) */
        avgCharWidth = (SInt16)((pTE->txSize * 2) / 3);
        if (avgCharWidth <= 0) avgCharWidth = 8; /* reasonable default */

        if (x <= 0) {
            charPos = lineStart;
        } else {
            SInt32 charsFromLeft = x / avgCharWidth;
            if (charsFromLeft < 0) charsFromLeft = 0;
            if (lineStart + charsFromLeft > lineEnd) {
                charPos = lineEnd;
            } else {
                charPos = lineStart + charsFromLeft;
            }
        }
        if (charPos < 0) charPos = 0;
        if (charPos > pTE->teLength) charPos = pTE->teLength;
    }

    if (extend) {
        /* Extend selection */
        pTE->selEnd = charPos;
    } else {
        /* Set new selection */
        pTE->selStart = charPos;
        pTE->selEnd = charPos;
    }

    /* Update click time/location for double-click detection */
    {
        extern UInt32 TickCount(void);
        pTE->clickTime = (SInt32)TickCount();
        /* Store horizontal position within view as clickLoc */
        pTE->clickLoc = pt.h;
    }
}

/* TEKey - Handle keyboard input
 * Trap: 0xA9DC, Evidence: offset 0x00004650, size 203
 * Provenance: evidence.curated.textedit.json:functions[6]
 */
void TEKey(SInt16 key, TEHandle hTE)
{
    TEPtr pTE;
    char keyChar;

    if (!hTE || !*hTE) return;
    pTE = *hTE;

    /*
    keyChar = (char)(key & 0xFF);

    /* Handle special keys */
    switch (keyChar) {
        case 0x08: /* Backspace */
            if (pTE->selStart > 0) {
                pTE->selStart--;
                pTE->selEnd = pTE->selStart;
                TEDelete(hTE);
            }
            break;

        case 0x7F: /* Delete */
            TEDelete(hTE);
            break;

        default:
            /* Insert regular character */
            if (keyChar >= 0x20 && keyChar < 0x7F) {
                TEInsert(&keyChar, 1, hTE);
            }
            break;
    }
}

/* TECut - Cut selected text to clipboard
 * Trap: 0xA9D6, Evidence: offset 0x00004800, size 78
 * Provenance: evidence.curated.textedit.json:functions[7]
 */
void TECut(TEHandle hTE)
{
    /*
    TECopy(hTE);
    TEDelete(hTE);
}

/* TECopy - Copy selected text to clipboard
 * Trap: 0xA9D5, Evidence: offset 0x00004880, size 65
 * Provenance: evidence.curated.textedit.json:functions[8]
 */
void TECopy(TEHandle hTE)
{
    TEPtr pTE;
    char *textPtr;
    SInt32 selLength;

    if (!hTE || !*hTE) return;
    pTE = *hTE;

    /*
    if (pTE->selStart == pTE->selEnd) return; /* No selection */

    selLength = pTE->selEnd - pTE->selStart;
    if (pTE->hText && selLength > 0) {
        textPtr = (char*)pTE->hText;
        /* Copy to system clipboard using Scrap Manager */
        extern OSErr PutScrap(SInt32 length, ResType type, const void* data);

        TE_LOG_DEBUG("TECopy: Copying %ld chars to clipboard\n", (long)selLength);
        PutScrap(selLength, 'TEXT', textPtr + pTE->selStart);
    }
}

/* TEPaste - Paste text from clipboard
 * Trap: 0xA9D7, Evidence: offset 0x00004920, size 112
 * Provenance: evidence.curated.textedit.json:functions[9]
 */
void TEPaste(TEHandle hTE)
{
    /*
    /* Get text from system clipboard */
    extern OSErr GetScrap(Handle destHandle, ResType type, SInt32* offset);

    Handle scrapHandle = NULL;
    SInt32 offset = 0;

    /* Try to get TEXT from clipboard */
    OSErr err = GetScrap(scrapHandle, 'TEXT', &offset);
    if (err == noErr && scrapHandle) {
        SInt32 length = GetHandleSize(scrapHandle);
        if (length > 0) {
            char* scrapText = (char*)*scrapHandle;
            TE_LOG_DEBUG("TEPaste: Pasting %ld chars from clipboard\n", (long)length);
            TEInsert(scrapText, length, hTE);
        }
        DisposeHandle(scrapHandle);
    } else {
        TE_LOG_DEBUG("TEPaste: No text available in clipboard\n");
    }
}

/* TEDelete - Delete selected text
 * Trap: 0xA9D3, Evidence: offset 0x00004a00, size 56
 * Provenance: evidence.curated.textedit.json:functions[10]
 */
void TEDelete(TEHandle hTE)
{
    TEPtr pTE;
    char *textPtr;
    SInt32 selLength, remaining;

    if (!hTE || !*hTE) return;
    pTE = *hTE;

    /*
    if (pTE->selStart == pTE->selEnd) return; /* No selection */

    selLength = pTE->selEnd - pTE->selStart;
    if (pTE->hText && pTE->teLength > 0) {
        textPtr = (char*)pTE->hText;
        remaining = pTE->teLength - pTE->selEnd;

        /* Move remaining text over deleted section */
        if (remaining > 0) {
            memmove(textPtr + pTE->selStart, textPtr + pTE->selEnd, remaining);
        }

        pTE->teLength -= selLength;
        pTE->selEnd = pTE->selStart;

        /* Null terminate */
        textPtr[pTE->teLength] = '\0';
    }
}

/* TEInsert - Insert text at cursor
 * Trap: 0xA9DE, Evidence: offset 0x00004b00, size 134
 * Provenance: evidence.curated.textedit.json:functions[11]
 */
void TEInsert(void *text, SInt32 length, TEHandle hTE)
{
    TEPtr pTE;
    char *oldText, *newText;
    SInt32 newLength;

    if (!hTE || !*hTE || !text || length <= 0) return;
    pTE = *hTE;

    /*
    /* First delete any selection */
    if (pTE->selStart != pTE->selEnd) {
        TEDelete(hTE);
    }

    newLength = pTE->teLength + length;
    newText = NewPtr(newLength + 1);
    if (!newText) return;

    oldText = (char*)pTE->hText;

    /* Copy text before insertion point */
    if (pTE->selStart > 0 && oldText) {
        memcpy(newText, oldText, pTE->selStart);
    }

    /* Copy inserted text */
    memcpy(newText + pTE->selStart, text, length);

    /* Copy text after insertion point */
    if (pTE->selStart < pTE->teLength && oldText) {
        memcpy(newText + pTE->selStart + length,
               oldText + pTE->selStart,
               pTE->teLength - pTE->selStart);
    }

    newText[newLength] = '\0';

    /* Replace old text */
    if (pTE->hText) DisposePtr((Ptr)pTE->hText);
    pTE->hText = newText;
    pTE->teLength = newLength;

    /* Update selection */
    pTE->selStart += length;
    pTE->selEnd = pTE->selStart;
}

/* TESetSelect - Set selection range
 * Trap: 0xA9D1, Evidence: offset 0x00004c50, size 67
 * Provenance: evidence.curated.textedit.json:functions[12]
 */
void TESetSelect(SInt32 selStart, SInt32 selEnd, TEHandle hTE)
{
    TEPtr pTE;

    if (!hTE || !*hTE) return;
    pTE = *hTE;

    /*
    /* Clamp to valid range */
    if (selStart < 0) selStart = 0;
    if (selStart > pTE->teLength) selStart = pTE->teLength;
    if (selEnd < selStart) selEnd = selStart;
    if (selEnd > pTE->teLength) selEnd = pTE->teLength;

    pTE->selStart = selStart;
    pTE->selEnd = selEnd;
}

/* TEActivate - Activate TextEdit field
 * Trap: 0xA9D8, Evidence: offset 0x00004d20, size 45
 * Provenance: evidence.curated.textedit.json:functions[13]
 */
void TEActivate(TEHandle hTE)
{
    if (!hTE || !*hTE) return;

    /*
    (*hTE)->active = 1;
    (*hTE)->caretState = 1; /* Show caret */
}

/* TEDeactivate - Deactivate TextEdit field
 * Trap: 0xA9D9, Evidence: offset 0x00004d80, size 42
 * Provenance: evidence.curated.textedit.json:functions[14]
 */
void TEDeactivate(TEHandle hTE)
{
    if (!hTE || !*hTE) return;

    /*
    (*hTE)->active = 0;
    (*hTE)->caretState = 0; /* Hide caret */
}

/* TEIdle - Handle idle time processing
 * Trap: 0xA9DA, Evidence: offset 0x00004e00, size 89
 * Provenance: evidence.curated.textedit.json:functions[15]
 */
void TEIdle(TEHandle hTE)
{
    TEPtr pTE;

    if (!hTE || !*hTE) return;
    pTE = *hTE;

    /*
    if (pTE->active) {
        /* Toggle caret state for blinking */
        pTE->caretState = !pTE->caretState;
        /* Redraw caret at current position */
        TE_LOG_DEBUG("TEIdle: Caret state = %s\n", pTE->caretState ? "visible" : "hidden");

        /* Calculate caret position and draw/erase */
        if (pTE->caretState) {
            /* Draw caret - would use QuickDraw line drawing */
            Rect caretRect;
            caretRect.left = pTE->destRect.left + pTE->selStart * 6; /* Approx char width */
            caretRect.top = pTE->destRect.top;
            caretRect.right = caretRect.left + 1;
            caretRect.bottom = caretRect.top + pTE->lineHeight;

            extern void InvertRect(const Rect* r);
            InvertRect(&caretRect);
        }
    }
}

/* TEUpdate - Update display of TextEdit content
 * Trap: 0xA9D3, Evidence: offset 0x00004f00, size 156
 * Provenance: evidence.curated.textedit.json:functions[16]
 */
void TEUpdate(void *updateRgn, TEHandle hTE)
{
    if (!hTE || !*hTE) return;

    /*
    /* Implement text drawing using QuickDraw */
    extern void DrawText(const void* textBuf, short firstByte, short byteCount);
    extern void MoveTo(short h, short v);

    TEPtr pTE = *hTE;
    if (pTE->teLength > 0 && pTE->hText) {
        char* text = (char*)pTE->hText;

        /* Draw text at destination position */
        MoveTo(pTE->destRect.left, pTE->destRect.top + pTE->lineHeight);

        TE_LOG_DEBUG("TEUpdate: Drawing %d chars\n", pTE->teLength);
        DrawText(text, 0, pTE->teLength);
    }

    TEDrawText(hTE);
}

/* TEScroll - Scroll TextEdit content
 * Trap: 0xA9DD, Evidence: offset 0x00005100, size 98
 * Provenance: evidence.curated.textedit.json:functions[17]
 */
void TEScroll(SInt16 dh, SInt16 dv, TEHandle hTE)
{
    TEPtr pTE;

    if (!hTE || !*hTE) return;
    pTE = *hTE;

    /*
    /* Implement scrolling by adjusting view rectangles */

    TE_LOG_DEBUG("TEScroll: Scrolling by (dh=%d, dv=%d)\n", dh, dv);

    /* Adjust view and destination rectangles */
    pTE->viewRect.left += dh;
    pTE->viewRect.right += dh;
    pTE->viewRect.top += dv;
    pTE->viewRect.bottom += dv;

    pTE->destRect.left += dh;
    pTE->destRect.right += dh;
    pTE->destRect.top += dv;
    pTE->destRect.bottom += dv;

    /* Force redraw */
    extern void InvalRect(const Rect* rect);
    InvalRect(&pTE->viewRect);
}

/* TETextBox - Draw text in rectangle with justification
 * Trap: 0xA9CE, Evidence: offset 0x00005200, size 134
 * Provenance: evidence.curated.textedit.json:functions[18]
 */
void TETextBox(void *text, SInt32 length, Rect *box, SInt16 just)
{
    /*
    if (!text || length <= 0 || !box) return;

    /* Implement text drawing using QuickDraw */
    extern void DrawText(const void* textBuf, short firstByte, short byteCount);
    extern void MoveTo(short h, short v);

    /* Calculate text position based on justification */
    short x = box->left;
    short y = box->top + 12; /* Baseline offset */

    switch (just) {
        case teCenter:
            x = (box->left + box->right) / 2 - (length * 3); /* Approx centering */
            break;
        case teFlushRight:
            x = box->right - (length * 6); /* Approx right align */
            break;
        case teFlushDefault:
        case teFlushLeft:
        default:
            x = box->left + 2;
            break;
    }

    TE_LOG_DEBUG("TETextBox: Drawing %ld chars with just=%d at (%d,%d)\n",
                  (long)length, just, x, y);

    MoveTo(x, y);
    DrawText(text, 0, length);
}

/* Helper Functions - Internal implementation */

static void TECalcLines(TEHandle hTE)
{
    if (!hTE || !*hTE) return;

    TEPtr pTE = *hTE;
    if (!pTE->hText || !*(pTE->hText)) return;

    char* text = *(pTE->hText);
    short textLen = pTE->teLength;
    short rectWidth = pTE->destRect.right - pTE->destRect.left;

    /* Reset line count and starts */
    pTE->nLines = 0;
    pTE->lineStarts[0] = 0;

    short currentLine = 0;
    short lineStart = 0;
    short currentPos = 0;

    while (currentPos < textLen && currentLine < 16000) {
        short lineEnd = currentPos;
        short lastSpace = -1;
        short lineWidth = 0;

        /* Find end of line or word wrap point */
        while (lineEnd < textLen) {
            char ch = text[lineEnd];

            /* Check for explicit line break */
            if (ch == '\r' || ch == '\n') {
                lineEnd++;
                break;
            }

            /* Track spaces for word wrapping */
            if (ch == ' ') {
                lastSpace = lineEnd;
            }

            /* Simple width estimation: 6 pixels per char (Chicago font approximation) */
            lineWidth += 6;

            /* Check if line exceeds width */
            if (lineWidth > rectWidth) {
                /* Word wrap at last space if available */
                if (lastSpace > lineStart) {
                    lineEnd = lastSpace + 1;  /* Break after space */
                } else {
                    /* Force break mid-word if no spaces */
                    if (lineEnd > lineStart) {
                        /* At least one character per line */
                    }
                }
                break;
            }

            lineEnd++;
        }

        /* Start next line */
        currentLine++;
        if (currentLine < 16000) {
            pTE->lineStarts[currentLine] = lineEnd;
        }
        currentPos = lineEnd;
        lineStart = lineEnd;
    }

    pTE->nLines = currentLine;
}

static void TESetupFont(TEHandle hTE)
{
    if (!hTE || !*hTE) return;

    TEPtr pTE = *hTE;

    /* Set QuickDraw port */
    if (pTE->inPort) {
        extern void SetPort(GrafPtr port);
        SetPort(pTE->inPort);
    }

    /* Set font */
    extern void TextFont(short font);
    TextFont(pTE->txFont);

    /* Set size */
    extern void TextSize(short size);
    TextSize(pTE->txSize);

    /* Set face/style */
    extern void TextFace(short face);
    TextFace(pTE->txFace);

    /* Set text mode */
    extern void TextMode(short mode);
    TextMode(pTE->txMode);
}

static void TEDrawText(TEHandle hTE)
{
    if (!hTE || !*hTE) return;

    TEPtr pTE = *hTE;
    if (!pTE->hText || !*(pTE->hText)) return;

    char* text = *(pTE->hText);

    /* Setup font */
    TESetupFont(hTE);

    /* Draw each line */
    short y = pTE->destRect.top + pTE->fontAscent;

    for (short line = 0; line < pTE->nLines; line++) {
        short lineStart = pTE->lineStarts[line];
        short lineEnd = (line + 1 < pTE->nLines) ? pTE->lineStarts[line + 1] : pTE->teLength;
        short lineLen = lineEnd - lineStart;

        /* Skip trailing line breaks */
        while (lineLen > 0 && (text[lineStart + lineLen - 1] == '\r' ||
                               text[lineStart + lineLen - 1] == '\n')) {
            lineLen--;
        }

        if (lineLen > 0) {
            /* Draw line of text */
            extern void MoveTo(short h, short v);
            extern void DrawText(const char* textBuf, short firstByte, short byteCount);

            MoveTo(pTE->destRect.left, y);
            DrawText(text, lineStart, lineLen);
        }

        /* Move to next line */
        y += pTE->lineHeight;

        /* Stop if we've drawn past viewRect */
        if (y > pTE->viewRect.bottom) {
            break;
        }
    }
}

static void TEUpdateCaret(TEHandle hTE)
{
    if (!hTE || !*hTE) return;

    TEPtr pTE = *hTE;
    if (!pTE->active) return;

    /* Simple caret blinking based on tick count */
    extern UInt32 TickCount(void);
    UInt32 currentTime = TickCount();

    /* Toggle caret every 30 ticks (0.5 seconds) */
    if (currentTime - pTE->caretTime >= 30) {
        pTE->caretState = !pTE->caretState;
        pTE->caretTime = currentTime;

        /* Calculate caret position */
        short caretLine = 0;
        short caretOffset = pTE->selStart;

        /* Find which line contains the caret */
        for (short i = 0; i < pTE->nLines; i++) {
            if (pTE->lineStarts[i] <= caretOffset &&
                (i + 1 >= pTE->nLines || pTE->lineStarts[i + 1] > caretOffset)) {
                caretLine = i;
                break;
            }
        }

        /* Calculate caret x position (simple: 6 pixels per char) */
        short lineStart = pTE->lineStarts[caretLine];
        short charsBeforeCaret = caretOffset - lineStart;
        short caretX = pTE->destRect.left + (charsBeforeCaret * 6);
        short caretY = pTE->destRect.top + (caretLine * pTE->lineHeight);

        /* Draw/erase caret using InvertRect */
        if (pTE->inPort) {
            extern void SetPort(GrafPtr port);
            SetPort(pTE->inPort);
        }

        Rect caretRect;
        caretRect.left = caretX;
        caretRect.right = caretX + 1;
        caretRect.top = caretY;
        caretRect.bottom = caretY + pTE->lineHeight;

        extern void InvertRect(const Rect* r);
        InvertRect(&caretRect);
    }
}
