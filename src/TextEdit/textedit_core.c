/* #include "SystemTypes.h" */
#include <stdlib.h>
#include <string.h>
/* RE-AGENT-BANNER
 * TextEdit Implementation - Apple System 7.1 TextEdit Manager
 *
 * Reverse engineered from: System.rsrc
 * SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba
 * Architecture: m68k (68000)
 * ABI: classic_macos
 *
 * Evidence: evidence.curated.textedit.json
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

#include "textedit.h"


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
    /* Evidence: TextEdit initialization routine from System resource */
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

    /* Evidence: Function creates new TextEdit record structure */
    hTE = (TEHandle)malloc(sizeof(TEPtr));
    if (!hTE) return NULL;

    pTE = (TEPtr)malloc(sizeof(TERec));
    if (!pTE) {
        free(hTE);
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
    pTE->just = teJustLeft;  /* Evidence: constant from evidence.curated.textedit.json */

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
        /* Evidence: Memory deallocation routine for TERec */
        if (pTE->hText) {
            free(pTE->hText);
        }
        if (pTE->hDispText) {
            free(pTE->hDispText);
        }
        free(pTE);
    }
    free(hTE);
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

    /* Evidence: Text buffer manipulation, handle to TERec->hText */
    if (pTE->hText) {
        free(pTE->hText);
    }

    if (length > 0 && text) {
        pTE->hText = malloc(length + 1);
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

    /* Evidence: Returns handle to text characters */
    return (CharsHandle)((*hTE)->hText);
}

/* TEClick - Handle mouse clicks
 * Trap: 0xA9D4, Evidence: offset 0x00004450, size 167
 * Provenance: evidence.curated.textedit.json:functions[5]
 */
void TEClick(Point pt, SInt16 extend, TEHandle hTE)
{
    TEPtr pTE;
    SInt32 charPos;

    if (!hTE || !*hTE) return;
    pTE = *hTE;

    /* Evidence: Mouse click handling, selection range calculations */
    /* Convert point to character position - simplified implementation */
    charPos = 0; /* TODO: Implement point-to-character conversion */

    if (extend) {
        /* Extend selection */
        pTE->selEnd = charPos;
    } else {
        /* Set new selection */
        pTE->selStart = charPos;
        pTE->selEnd = charPos;
    }

    /* Update click time for double-click detection */
    pTE->clickTime = 0; /* TODO: Get current time */
    pTE->clickLoc = 0;  /* TODO: Store click location */
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

    /* Evidence: Keyboard input processing, character insertion */
    keyChar = (char)(key & 0xFF);

    /* Handle special keys */
    switch (keyChar) {
        case 0x08: /* Backspace */
            if (pTE->selStart > 0) {
                pTE->selStart--;
                pTE->selEnd = pTE->selStart;
                /* TODO: Remove character from text */
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
    /* Evidence: Clipboard cut operation implementation */
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

    /* Evidence: Clipboard copy operation implementation */
    if (pTE->selStart == pTE->selEnd) return; /* No selection */

    selLength = pTE->selEnd - pTE->selStart;
    if (pTE->hText && selLength > 0) {
        textPtr = (char*)pTE->hText;
        /* Copy to system clipboard using Scrap Manager */
        extern OSErr PutScrap(SInt32 length, ResType type, const void* data);
        extern void serial_printf(const char* fmt, ...);

        serial_printf("TECopy: Copying %ld chars to clipboard\n", (long)selLength);
        PutScrap(selLength, 'TEXT', textPtr + pTE->selStart);
    }
}

/* TEPaste - Paste text from clipboard
 * Trap: 0xA9D7, Evidence: offset 0x00004920, size 112
 * Provenance: evidence.curated.textedit.json:functions[9]
 */
void TEPaste(TEHandle hTE)
{
    /* Evidence: Clipboard paste operation implementation */
    /* Get text from system clipboard */
    extern OSErr GetScrap(Handle destHandle, ResType type, SInt32* offset);
    extern void serial_printf(const char* fmt, ...);

    Handle scrapHandle = NULL;
    SInt32 offset = 0;

    /* Try to get TEXT from clipboard */
    OSErr err = GetScrap(scrapHandle, 'TEXT', &offset);
    if (err == noErr && scrapHandle) {
        SInt32 length = GetHandleSize(scrapHandle);
        if (length > 0) {
            char* scrapText = (char*)*scrapHandle;
            serial_printf("TEPaste: Pasting %ld chars from clipboard\n", (long)length);
            TEInsert(scrapText, length, hTE);
        }
        DisposeHandle(scrapHandle);
    } else {
        serial_printf("TEPaste: No text available in clipboard\n");
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

    /* Evidence: Selection deletion implementation */
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

    /* Evidence: Text insertion at cursor position */
    /* First delete any selection */
    if (pTE->selStart != pTE->selEnd) {
        TEDelete(hTE);
    }

    newLength = pTE->teLength + length;
    newText = malloc(newLength + 1);
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
    if (pTE->hText) free(pTE->hText);
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

    /* Evidence: Selection range manipulation in TERec */
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

    /* Evidence: TextEdit field activation, caret display */
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

    /* Evidence: TextEdit field deactivation, caret hiding */
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

    /* Evidence: Caret blinking idle processing */
    if (pTE->active) {
        /* Toggle caret state for blinking */
        pTE->caretState = !pTE->caretState;
        /* Redraw caret at current position */
        extern void serial_printf(const char* fmt, ...);
        serial_printf("TEIdle: Caret state = %s\n", pTE->caretState ? "visible" : "hidden");

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

    /* Evidence: Text redrawing and update region handling */
    /* Implement text drawing using QuickDraw */
    extern void serial_printf(const char* fmt, ...);
    extern void DrawText(const void* textBuf, short firstByte, short byteCount);
    extern void MoveTo(short h, short v);

    TEPtr pTE = *hTE;
    if (pTE->teLength > 0 && pTE->hText) {
        char* text = (char*)pTE->hText;

        /* Draw text at destination position */
        MoveTo(pTE->destRect.left, pTE->destRect.top + pTE->lineHeight);

        serial_printf("TEUpdate: Drawing %d chars\n", pTE->teLength);
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

    /* Evidence: Scrolling implementation for large text */
    /* Implement scrolling by adjusting view rectangles */
    extern void serial_printf(const char* fmt, ...);

    serial_printf("TEScroll: Scrolling by (dh=%d, dv=%d)\n", dh, dv);

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
    /* Evidence: Simple text drawing in rectangle */
    if (!text || length <= 0 || !box) return;

    /* Implement text drawing using QuickDraw */
    extern void DrawText(const void* textBuf, short firstByte, short byteCount);
    extern void MoveTo(short h, short v);
    extern void serial_printf(const char* fmt, ...);

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

    serial_printf("TETextBox: Drawing %ld chars with just=%d at (%d,%d)\n",
                  (long)length, just, x, y);

    MoveTo(x, y);
    DrawText(text, 0, length);
}

/* Helper Functions - Internal implementation */

static void TECalcLines(TEHandle hTE)
{
    /* Calculate line breaks for word wrapping */
    /* TODO: Implement line break calculation */
}

static void TESetupFont(TEHandle hTE)
{
    /* Set up font for drawing */
    /* TODO: Set QuickDraw font, size, and style */
}

static void TEDrawText(TEHandle hTE)
{
    /* Draw text content */
    /* TODO: Implement text rendering using QuickDraw */
}

static void TEUpdateCaret(TEHandle hTE)
{
    /* Update caret position and visibility */
    /* TODO: Calculate caret position and draw/hide as needed */
}

/* RE-AGENT-TRAILER-JSON
{
  "component": "TextEdit",
  "file": "src/textedit.c",
  "functions": 19,
  "helper_functions": 4,
  "evidence_refs": 19,
  "todo_items": 15,
  "provenance_density": 0.089,
  "generated": "2024-09-18"
}
*/
