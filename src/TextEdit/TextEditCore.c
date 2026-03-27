/* #include "SystemTypes.h" */
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
/************************************************************
 *
 * TextEditCore.c
 * System 7.1 Portable - TextEdit Core Implementation
 *
 * Core TextEdit functionality including TERec management,
 * basic editing operations, initialization, and lifecycle
 * management. Provides the foundation for all TextEdit operations.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Derived from ROM analysis: TextEdit Manager 1985-1992
 *
 ************************************************************/

// #include "CompatibilityFix.h" // Removed
#include "TextEdit/TextEdit.h"
#include "TextEdit/TextTypes.h"
#include "MemoryMgr/MemoryManager.h"
#include "ScrapManager/ScrapManager.h"
#include "ErrorCodes.h"
#include "EventManager/EventManager.h"
/* #include "Fonts.h" - not available yet */


/************************************************************
 * Type Definitions
 ************************************************************/

/* TEGlobals - System-wide TextEdit state */
typedef struct {
    Handle TEScrapHandle;
    long TEScrapLength;
    short TELastScript;
    long TEDefaultEncoding;
    Boolean TEAccessibilityMode;
    Boolean TEInited;
    Boolean TEPlatformInited;
    Boolean TEUnicodeSupport;
} TEGlobals;

/* Error codes */
#define unimpErr -4  /* Unimplemented feature */

/* TextEdit justification constants */
#define teJustLeft 0
#define teJustCenter 1
#define teJustRight -1
#define teFlushDefault 0
#define teFlushLeft 0
#define teFlushRight -1
#define teFlushCenter 1

/* QuickDraw transfer modes */
#define srcOr 1
#define srcXor 2
#define srcBic 3
#define srcCopy 8

/************************************************************
 * Global TextEdit State
 ************************************************************/

static TEGlobals gTEGlobals = {0};
static Boolean gTEInitialized = false;

/************************************************************
 * Internal Utility Functions
 ************************************************************/

static OSErr TEValidateHandle(TEHandle hTE)
{
    if (!hTE || !*hTE) return paramErr;
    if (GetHandleSize((Handle)hTE) < sizeof(TERec)) return paramErr;
    return noErr;
}

static OSErr TEAllocateText(TEHandle hTE, long length)
{
    TERec **teRec;
    Handle newText;
    OSErr result;

    if (length < 0) return paramErr;
    if (length == 0) length = 1; /* Always allocate at least 1 byte */

    /* CRITICAL: Lock hTE before dereferencing to prevent heap compaction issues */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;

    newText = NewHandle(length);
    if (!newText) {
        HUnlock((Handle)hTE);
        return MemError();
    }

    if ((**teRec).hText) {
        DisposeHandle((**teRec).hText);
    }

    (**teRec).hText = newText;
    (**teRec).teLength = 0;

    result = noErr;
    HUnlock((Handle)hTE);
    return result;
}

static void TERecalculateLines(TEHandle hTE)
{
    TERec **teRec;
    char *textPtr;
    long textLen;
    short lineCount = 0;
    long i;

    /* Validate handle pointer before any dereferencing */
    if (!hTE) return;

    /* CRITICAL: Lock hTE before dereferencing to prevent heap compaction issues */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;

    /* Now safe to dereference after locking */
    if (!*teRec) {
        HUnlock((Handle)hTE);
        return;
    }

    if (!(**teRec).hText) {
        HUnlock((Handle)hTE);
        return;
    }

    HLock((**teRec).hText);
    textPtr = *(**teRec).hText;
    textLen = (**teRec).teLength;

    /* First line always starts at 0 */
    (**teRec).lineStarts[lineCount++] = 0;

    /* Find line breaks */
    for (i = 0; i < textLen && lineCount < 16000; i++) {
        if (textPtr[i] == '\r' || textPtr[i] == '\n') {
            (**teRec).lineStarts[lineCount++] = i + 1;
        }
    }

    (**teRec).nLines = lineCount;
    HUnlock((**teRec).hText);
    HUnlock((Handle)hTE);
}

TEDispatchHandle TECreateDispatchRec(void)
{
    TEDispatchHandle hDispatch;
    TEDispatchRec **dispatch;

    hDispatch = (TEDispatchHandle)NewHandleClear(sizeof(TEDispatchRec));
    if (!hDispatch) return NULL;

    /* Lock handle before dereferencing for safety */
    HLock((Handle)hDispatch);
    dispatch = (TEDispatchRec **)hDispatch;

    /* Initialize default hooks to NULL - they will use built-in behavior */
    (**dispatch).EOLHook = NULL;
    (**dispatch).DRAWHook = NULL;
    (**dispatch).WIDTHHook = NULL;
    (**dispatch).HITTESTHook = NULL;
    (**dispatch).nWIDTHHook = NULL;
    (**dispatch).TextWidthHook = NULL;

    /* Initialize flags */
    (**dispatch).newTEFlags = 0;
    (**dispatch).TwoByteCharBuffer = 0;
    (**dispatch).lastScript = 0;

    HUnlock((Handle)hDispatch);
    return hDispatch;
}

/************************************************************
 * TextEdit Initialization
 ************************************************************/

pascal void TEInit(void)
{
    if (gTEInitialized) return;

    /* Initialize global state */
    memset(&gTEGlobals, 0, sizeof(TEGlobals));
    gTEGlobals.TEScrapHandle = NULL;
    gTEGlobals.TEScrapLength = 0;
    gTEGlobals.TELastScript = 0;
    gTEGlobals.TEInited = true;
    gTEGlobals.TEPlatformInited = false;
    gTEGlobals.TEUnicodeSupport = false;
    gTEGlobals.TEAccessibilityMode = false;
    gTEGlobals.TEDefaultEncoding = 0; /* Mac Roman */

    /* Initialize platform-specific components */
    TEInitPlatform();

    gTEInitialized = true;
}

OSErr TEInitPlatform(void)
{
    if (gTEGlobals.TEPlatformInited) return noErr;

    /* Initialize platform-specific input handling */
    TEInitPlatformInput();

    gTEGlobals.TEPlatformInited = true;
    return noErr;
}

void TECleanupPlatform(void)
{
    if (!gTEGlobals.TEPlatformInited) return;

    /* Clean up platform-specific components */
    TECleanupPlatformInput();

    gTEGlobals.TEPlatformInited = false;
}

/************************************************************
 * TextEdit Record Management
 ************************************************************/

pascal TEHandle TENew(const Rect *destRect, const Rect *viewRect)
{
    TEHandle hTE;
    TERec **teRec;
    TEDispatchHandle hDispatch;
    OSErr err;

    if (!gTEInitialized) TEInit();

    /* Allocate the TextEdit record */
    hTE = (TEHandle)NewHandleClear(sizeof(TERec));
    if (!hTE) return NULL;

    /* CRITICAL: Lock hTE before dereferencing to prevent heap compaction issues
     * This is especially important because we call TEAllocateText and TECreateDispatchRec
     * below, which trigger NewHandle calls that can compact the heap */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;

    /* Set up rectangles */
    if (destRect) (**teRec).destRect = *destRect;
    else SetRect(&(**teRec).destRect, 0, 0, 100, 100);

    if (viewRect) (**teRec).viewRect = *viewRect;
    else (**teRec).viewRect = (**teRec).destRect;

    (**teRec).selRect = (**teRec).destRect;

    /* Initialize basic fields */
    (**teRec).lineHeight = 12;  /* Default line height */
    (**teRec).fontAscent = 9;   /* Default font ascent */
    (**teRec).selPoint.h = (**teRec).destRect.left;
    (**teRec).selPoint.v = (**teRec).destRect.top;
    (**teRec).selStart = 0;
    (**teRec).selEnd = 0;
    (**teRec).active = 0;
    (**teRec).wordBreak = NULL;
    (**teRec).clikLoop = NULL;
    (**teRec).clickTime = 0;
    (**teRec).clickLoc = 0;
    (**teRec).caretTime = 0;
    (**teRec).caretState = 0;
    (**teRec).just = teJustLeft;
    (**teRec).teLength = 0;

    /* Allocate text storage */
    err = TEAllocateText(hTE, 256); /* Initial size */
    if (err != noErr) {
        HUnlock((Handle)hTE);
        DisposeHandle((Handle)hTE);
        return NULL;
    }

    /* Create dispatch record */
    hDispatch = TECreateDispatchRec();
    if (!hDispatch) {
        DisposeHandle((**teRec).hText);
        HUnlock((Handle)hTE);
        DisposeHandle((Handle)hTE);
        return NULL;
    }

    (**teRec).hDispatchRec = (long)hDispatch;

    /* Initialize more fields */
    (**teRec).clikStuff = 0;
    (**teRec).crOnly = 0;
    (**teRec).txFont = 1;       /* System font */
    (**teRec).txFace = normal;
    (**teRec).filler = 0;
    (**teRec).txMode = srcOr;
    (**teRec).txSize = 12;
    (**teRec).inPort = NULL;    /* Will be set when drawing */
    (**teRec).highHook = NULL;
    (**teRec).caretHook = NULL;
    (**teRec).nLines = 1;
    (**teRec).lineStarts[0] = 0;

    HUnlock((Handle)hTE);
    return hTE;
}

pascal TEHandle TEStyleNew(const Rect *destRect, const Rect *viewRect)
{
    TEHandle hTE;
    TEStyleHandle hStyle;
    TERec **teRec;

    /* Create basic TextEdit record */
    hTE = TENew(destRect, viewRect);
    if (!hTE) return NULL;

    /* CRITICAL: Lock hTE before calling NewHandleClear which can trigger heap compaction */
    HLock((Handle)hTE);

    /* Create style record */
    hStyle = (TEStyleHandle)NewHandleClear(sizeof(TEStyleRec));
    if (!hStyle) {
        HUnlock((Handle)hTE);
        TEDispose(hTE);
        return NULL;
    }

    /* Initialize style record */
    teRec = (TERec **)hTE;
    /* Style handle would be stored in an extended TERec structure */
    /* For now, we'll store it in the dispatch record's extended area */

    HUnlock((Handle)hTE);
    return hTE;
}

pascal TEHandle TEStylNew(const Rect *destRect, const Rect *viewRect)
{
    return TEStyleNew(destRect, viewRect);
}

pascal void TEDispose(TEHandle hTE)
{
    TERec **teRec;
    TEDispatchHandle hDispatch;

    if (TEValidateHandle(hTE) != noErr) return;

    /* Lock hTE before dereferencing */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;

    /* Dispose text storage */
    if ((**teRec).hText) {
        DisposeHandle((**teRec).hText);
    }

    /* Dispose dispatch record */
    if ((**teRec).hDispatchRec) {
        hDispatch = (TEDispatchHandle)(**teRec).hDispatchRec;
        DisposeHandle((Handle)hDispatch);
    }

    /* No need to unlock - we're disposing the handle */
    /* Dispose the TextEdit record itself */
    DisposeHandle((Handle)hTE);
}

/************************************************************
 * Text Access Functions
 ************************************************************/

pascal void TESetText(const void *text, long length, TEHandle hTE)
{
    TERec **teRec;
    OSErr err;

    if (TEValidateHandle(hTE) != noErr) return;
    if (!text && length > 0) return;
    if (length < 0) length = 0;

    /* CRITICAL: Lock hTE before dereferencing - TEAllocateText can trigger heap compaction */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;

    /* Resize text handle if necessary */
    if (length > GetHandleSize((**teRec).hText)) {
        err = TEAllocateText(hTE, length + 256); /* Extra space for growth */
        if (err != noErr) {
            HUnlock((Handle)hTE);
            return;
        }
    }

    /* Copy text data */
    if (length > 0 && text) {
        HLock((**teRec).hText);
        memcpy(*(**teRec).hText, text, length);
        HUnlock((**teRec).hText);
    }

    (**teRec).teLength = length;

    /* Reset selection */
    (**teRec).selStart = 0;
    (**teRec).selEnd = 0;

    HUnlock((Handle)hTE);

    /* Recalculate line breaks */
    TERecalculateLines(hTE);
}

pascal CharsHandle TEGetText(TEHandle hTE)
{
    TERec **teRec;

    if (TEValidateHandle(hTE) != noErr) return NULL;

    teRec = (TERec **)hTE;
    return (CharsHandle)(**teRec).hText;
}

/************************************************************
 * Selection Management
 ************************************************************/

pascal void TESetSelect(long selStart, long selEnd, TEHandle hTE)
{
    TERec **teRec;

    if (TEValidateHandle(hTE) != noErr) return;

    teRec = (TERec **)hTE;

    /* Clamp selection to text bounds */
    if (selStart < 0) selStart = 0;
    if (selEnd < 0) selEnd = 0;
    if (selStart > (**teRec).teLength) selStart = (**teRec).teLength;
    if (selEnd > (**teRec).teLength) selEnd = (**teRec).teLength;

    /* Ensure proper ordering */
    if (selStart > selEnd) {
        long temp = selStart;
        selStart = selEnd;
        selEnd = temp;
    }

    (**teRec).selStart = selStart;
    (**teRec).selEnd = selEnd;

    /* Update selection point and rectangle */
    /* This is a simplified version - full implementation would calculate pixel positions */
    (**teRec).selPoint.h = (**teRec).destRect.left;
    (**teRec).selPoint.v = (**teRec).destRect.top;
    (**teRec).selRect = (**teRec).destRect;
}

pascal void TEActivate(TEHandle hTE)
{
    TERec **teRec;

    if (TEValidateHandle(hTE) != noErr) return;

    teRec = (TERec **)hTE;
    (**teRec).active = 1;
    (**teRec).caretState = 1;
    (**teRec).caretTime = TickCount();
}

pascal void TEDeactivate(TEHandle hTE)
{
    TERec **teRec;

    if (TEValidateHandle(hTE) != noErr) return;

    teRec = (TERec **)hTE;
    (**teRec).active = 0;
    (**teRec).caretState = 0;
}

pascal void TEIdle(TEHandle hTE)
{
    TERec **teRec;
    long currentTime;

    if (TEValidateHandle(hTE) != noErr) return;

    teRec = (TERec **)hTE;

    if (!(**teRec).active) return;

    currentTime = TickCount();

    /* Handle caret blinking */
    if (currentTime - (**teRec).caretTime > 30) { /* 30 ticks = 0.5 seconds */
        (**teRec).caretState = !(**teRec).caretState;
        (**teRec).caretTime = currentTime;

        /* In full implementation, this would redraw the caret */
    }
}

/************************************************************
 * Basic Editing Operations
 ************************************************************/

pascal void TEInsert(const void *text, long length, TEHandle hTE)
{
    TERec **teRec;
    Handle textHandle;
    long insertPos;
    long newLength;
    long moveLength;
    char *textPtr;
    OSErr err;

    if (TEValidateHandle(hTE) != noErr) return;
    if (!text || length <= 0) return;

    /* CRITICAL: Lock hTE before dereferencing - SetHandleSize can trigger heap compaction */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;
    textHandle = (**teRec).hText;
    insertPos = (**teRec).selStart;
    newLength = (**teRec).teLength + length;

    /* Check if we need to grow the text handle */
    if (newLength > GetHandleSize(textHandle)) {
        SetHandleSize(textHandle, newLength + 256);
        err = MemError();
        if (err != noErr) {
            HUnlock((Handle)hTE);
            return;
        }
    }

    HLock(textHandle);
    textPtr = *textHandle;

    /* Move existing text after insert point */
    moveLength = (**teRec).teLength - insertPos;
    if (moveLength > 0) {
        memmove(textPtr + insertPos + length, textPtr + insertPos, moveLength);
    }

    /* Insert new text */
    memcpy(textPtr + insertPos, text, length);

    HUnlock(textHandle);

    /* Update text length and selection */
    (**teRec).teLength = newLength;
    (**teRec).selStart = insertPos + length;
    (**teRec).selEnd = (**teRec).selStart;

    HUnlock((Handle)hTE);

    /* Recalculate line breaks */
    TERecalculateLines(hTE);
}

pascal void TEDelete(TEHandle hTE)
{
    TERec **teRec;
    Handle textHandle;
    long deleteStart, deleteEnd, deleteLength;
    long moveLength;
    char *textPtr;

    if (TEValidateHandle(hTE) != noErr) return;

    /* Lock hTE before dereferencing */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;
    textHandle = (**teRec).hText;
    deleteStart = (**teRec).selStart;
    deleteEnd = (**teRec).selEnd;

    if (deleteStart == deleteEnd) {
        HUnlock((Handle)hTE);
        return; /* Nothing to delete */
    }

    deleteLength = deleteEnd - deleteStart;

    HLock(textHandle);
    textPtr = *textHandle;

    /* Move text after deletion point */
    moveLength = (**teRec).teLength - deleteEnd;
    if (moveLength > 0) {
        memmove(textPtr + deleteStart, textPtr + deleteEnd, moveLength);
    }

    HUnlock(textHandle);

    /* Update text length and selection */
    (**teRec).teLength -= deleteLength;
    (**teRec).selEnd = deleteStart;

    HUnlock((Handle)hTE);

    /* Recalculate line breaks */
    TERecalculateLines(hTE);
}

pascal void TEKey(short key, TEHandle hTE)
{
    char ch = (char)(key & 0xFF);
    TERec **teRec;
    Boolean callDelete = false;
    Boolean callInsert = false;

    if (TEValidateHandle(hTE) != noErr) return;

    /* Lock hTE before dereferencing */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;

    /* Handle special keys */
    switch (ch) {
        case backspaceChar:
            if ((**teRec).selStart == (**teRec).selEnd && (**teRec).selStart > 0) {
                /* Backspace with no selection - delete character before cursor */
                (**teRec).selStart--;
            }
            callDelete = ((**teRec).selStart < (**teRec).selEnd);
            break;

        case leftArrowChar:
            if ((**teRec).selStart > 0) {
                (**teRec).selStart--;
                (**teRec).selEnd = (**teRec).selStart;
            }
            break;

        case rightArrowChar:
            if ((**teRec).selEnd < (**teRec).teLength) {
                (**teRec).selEnd++;
                (**teRec).selStart = (**teRec).selEnd;
            }
            break;

        case upArrowChar: {
            /* Move to same horizontal position on previous line */
            SInt16 curLine = 0;
            for (SInt16 i = 0; i < (**teRec).nLines; i++) {
                if ((**teRec).selStart >= (**teRec).lineStarts[i]) {
                    curLine = i;
                } else {
                    break;
                }
            }
            if (curLine > 0) {
                SInt16 colOffset = (**teRec).selStart - (**teRec).lineStarts[curLine];
                SInt16 prevLineStart = (**teRec).lineStarts[curLine - 1];
                SInt16 prevLineEnd = (**teRec).lineStarts[curLine] - 1;
                SInt16 newPos = prevLineStart + colOffset;
                if (newPos > prevLineEnd) newPos = prevLineEnd;
                if (newPos < prevLineStart) newPos = prevLineStart;
                (**teRec).selStart = newPos;
                (**teRec).selEnd = newPos;
            }
            break;
        }

        case downArrowChar: {
            /* Move to same horizontal position on next line */
            SInt16 curLine = 0;
            for (SInt16 i = 0; i < (**teRec).nLines; i++) {
                if ((**teRec).selEnd >= (**teRec).lineStarts[i]) {
                    curLine = i;
                } else {
                    break;
                }
            }
            if (curLine < (**teRec).nLines - 1) {
                SInt16 colOffset = (**teRec).selEnd - (**teRec).lineStarts[curLine];
                SInt16 nextLineStart = (**teRec).lineStarts[curLine + 1];
                SInt16 nextLineEnd = (curLine + 2 < (**teRec).nLines)
                                    ? (**teRec).lineStarts[curLine + 2] - 1
                                    : (**teRec).teLength;
                SInt16 newPos = nextLineStart + colOffset;
                if (newPos > nextLineEnd) newPos = nextLineEnd;
                (**teRec).selStart = newPos;
                (**teRec).selEnd = newPos;
            } else {
                /* On last line, move to end of text */
                (**teRec).selStart = (**teRec).teLength;
                (**teRec).selEnd = (**teRec).teLength;
            }
            break;
        }

        default:
            /* Regular character - insert it */
            if (ch >= 0x20 || ch == returnChar || ch == tabChar) {
                /* Check if we need to delete selection first */
                callDelete = ((**teRec).selStart != (**teRec).selEnd);
                callInsert = true;
            }
            break;
    }

    HUnlock((Handle)hTE);

    /* Call functions that lock hTE internally */
    if (callDelete) {
        TEDelete(hTE);
    }
    if (callInsert) {
        TEInsert(&ch, 1, hTE);
    }
}

/************************************************************
 * Display and Layout Functions
 ************************************************************/

pascal void TECalText(TEHandle hTE)
{
    if (TEValidateHandle(hTE) != noErr) return;

    /* Recalculate all line breaks and layout information */
    TERecalculateLines(hTE);
}

pascal void TEUpdate(const Rect *rUpdate, TEHandle hTE)
{
    /* This would contain the display update logic */
    /* For now, this is a placeholder */
    if (TEValidateHandle(hTE) != noErr) return;
    /* Full implementation would redraw text within update rectangle */
}

pascal void TEScroll(short dh, short dv, TEHandle hTE)
{
    TERec **teRec;

    if (TEValidateHandle(hTE) != noErr) return;

    teRec = (TERec **)hTE;

    /* Adjust view rectangle */
    OffsetRect(&(**teRec).viewRect, -dh, -dv);
    OffsetRect(&(**teRec).selRect, -dh, -dv);

    /* In full implementation, this would scroll the display */
}

pascal void TESetAlignment(short just, TEHandle hTE)
{
    TERec **teRec;

    if (TEValidateHandle(hTE) != noErr) return;

    teRec = (TERec **)hTE;
    (**teRec).just = just;
}

pascal void TESetJust(short just, TEHandle hTE)
{
    TESetAlignment(just, hTE);
}

/************************************************************
 * Point and Offset Conversion (Simplified)
 ************************************************************/

pascal short TEGetOffset(Point pt, TEHandle hTE)
{
    TERec **teRec;
    SInt16 lineHeight, fontAscent;
    SInt16 line, charOffset;
    char *textPtr;

    if (TEValidateHandle(hTE) != noErr) return 0;

    teRec = (TERec **)hTE;
    lineHeight = (**teRec).lineHeight;
    fontAscent = (**teRec).fontAscent;

    if (lineHeight <= 0) lineHeight = 12;  /* Safe default */

    /* Determine which line the point falls on */
    SInt16 relY = pt.v - (**teRec).destRect.top;
    if (relY < 0) return 0;

    line = relY / lineHeight;
    if (line >= (**teRec).nLines) {
        return (**teRec).teLength;
    }

    /* Get the character range for this line */
    SInt16 lineStart = (**teRec).lineStarts[line];
    SInt16 lineEnd = (line < (**teRec).nLines - 1)
                     ? (**teRec).lineStarts[line + 1]
                     : (**teRec).teLength;

    /* Simple horizontal offset: estimate character position from x coordinate */
    SInt16 relX = pt.h - (**teRec).destRect.left;
    if (relX <= 0) return lineStart;

    /* Use average character width (lineHeight * 0.6 is a reasonable approximation for Chicago 12) */
    SInt16 avgCharWidth = (lineHeight * 3) / 5;
    if (avgCharWidth <= 0) avgCharWidth = 7;

    charOffset = lineStart + (relX / avgCharWidth);
    if (charOffset > lineEnd) charOffset = lineEnd;
    if (charOffset < lineStart) charOffset = lineStart;

    return charOffset;
}

pascal Point TEGetPoint(short offset, TEHandle hTE)
{
    TERec **teRec;
    Point pt = {0, 0};

    if (TEValidateHandle(hTE) != noErr) return pt;

    teRec = (TERec **)hTE;

    SInt16 lineHeight = (**teRec).lineHeight;
    if (lineHeight <= 0) lineHeight = 12;

    /* Clamp offset */
    if (offset < 0) offset = 0;
    if (offset > (**teRec).teLength) offset = (**teRec).teLength;

    /* Find which line this offset falls on */
    SInt16 line = 0;
    for (SInt16 i = 0; i < (**teRec).nLines; i++) {
        SInt16 nextStart = (i < (**teRec).nLines - 1)
                          ? (**teRec).lineStarts[i + 1]
                          : (**teRec).teLength + 1;
        if (offset < nextStart) {
            line = i;
            break;
        }
        line = i;
    }

    /* Vertical position: top of destRect + line * lineHeight + fontAscent */
    pt.v = (**teRec).destRect.top + line * lineHeight + (**teRec).fontAscent;

    /* Horizontal: approximate from character offset within line */
    SInt16 lineStart = (**teRec).lineStarts[line];
    SInt16 charsIntoLine = offset - lineStart;
    SInt16 avgCharWidth = (lineHeight * 3) / 5;
    if (avgCharWidth <= 0) avgCharWidth = 7;
    pt.h = (**teRec).destRect.left + charsIntoLine * avgCharWidth;

    return pt;
}

pascal void TEClick(Point pt, Boolean fExtend, TEHandle h)
{
    short offset;
    TERec **teRec;

    if (TEValidateHandle(h) != noErr) return;

    teRec = (TERec **)h;
    offset = TEGetOffset(pt, h);

    if (fExtend) {
        /* Extend selection */
        (**teRec).selEnd = offset;
    } else {
        /* Set new selection */
        (**teRec).selStart = offset;
        (**teRec).selEnd = offset;
    }

    (**teRec).clickTime = TickCount();
}

void teclick(Point *pt, Boolean fExtend, TEHandle h)
{
    if (pt) {
        TEClick(*pt, fExtend, h);
    }
}

/************************************************************
 * Modern Platform Integration Stubs
 ************************************************************/

OSErr TESetTextEncoding(TEHandle hTE, TextEncoding encoding)
{
    /* Platform-specific implementation would handle encoding conversion */
    return unimpErr;
}

TextEncoding TEGetTextEncoding(TEHandle hTE)
{
    return gTEGlobals.TEDefaultEncoding;
}

OSErr TESetInputMethod(TEHandle hTE, Boolean useModernIM)
{
    /* Platform-specific implementation would configure input methods */
    return unimpErr;
}

Boolean TEGetInputMethod(TEHandle hTE)
{
    return false;
}

OSErr TESetAccessibilityEnabled(TEHandle hTE, Boolean enabled)
{
    gTEGlobals.TEAccessibilityMode = enabled;
    return noErr;
}

Boolean TEGetAccessibilityEnabled(TEHandle hTE)
{
    return gTEGlobals.TEAccessibilityMode;
}

/************************************************************
 * Platform Input Stubs
 ************************************************************/

OSErr TEInitPlatformInput(void)
{
    /* Platform-specific initialization - currently stubbed */
    return noErr;
}

void TECleanupPlatformInput(void)
{
    /* Platform-specific cleanup - currently stubbed */
}
