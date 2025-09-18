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
 * Based on Apple Computer, Inc. TextEdit Manager 1985-1992
 *
 ************************************************************/

#include "TextEdit/TextEdit.h"
#include "TextEdit/TextTypes.h"
#include "Memory.h"
#include "Scrap.h"
#include "Fonts.h"
#include <string.h>
#include <stdlib.h>

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
    TERec **teRec = (TERec **)hTE;
    Handle newText;

    if (length < 0) return paramErr;
    if (length == 0) length = 1; /* Always allocate at least 1 byte */

    newText = NewHandle(length);
    if (!newText) return MemError();

    if ((**teRec).hText) {
        DisposeHandle((**teRec).hText);
    }

    (**teRec).hText = newText;
    (**teRec).teLength = 0;

    return noErr;
}

static void TERecalculateLines(TEHandle hTE)
{
    TERec **teRec = (TERec **)hTE;
    char *textPtr;
    long textLen;
    short lineCount = 0;
    long i;

    if (!hTE || !*hTE || !(**teRec).hText) return;

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
}

static TEDispatchHandle TECreateDispatchRec(void)
{
    TEDispatchHandle hDispatch;
    TEDispatchRec **dispatch;

    hDispatch = (TEDispatchHandle)NewHandleClear(sizeof(TEDispatchRec));
    if (!hDispatch) return NULL;

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
        DisposeHandle((Handle)hTE);
        return NULL;
    }

    /* Create dispatch record */
    hDispatch = TECreateDispatchRec();
    if (!hDispatch) {
        DisposeHandle((**teRec).hText);
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

    /* Create style record */
    hStyle = (TEStyleHandle)NewHandleClear(sizeof(TEStyleRec));
    if (!hStyle) {
        TEDispose(hTE);
        return NULL;
    }

    /* Initialize style record */
    teRec = (TERec **)hTE;
    /* Style handle would be stored in an extended TERec structure */
    /* For now, we'll store it in the dispatch record's extended area */

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

    teRec = (TERec **)hTE;

    /* Resize text handle if necessary */
    if (length > GetHandleSize((**teRec).hText)) {
        err = TEAllocateText(hTE, length + 256); /* Extra space for growth */
        if (err != noErr) return;
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

    teRec = (TERec **)hTE;
    textHandle = (**teRec).hText;
    insertPos = (**teRec).selStart;
    newLength = (**teRec).teLength + length;

    /* Check if we need to grow the text handle */
    if (newLength > GetHandleSize(textHandle)) {
        SetHandleSize(textHandle, newLength + 256);
        err = MemError();
        if (err != noErr) return;
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

    teRec = (TERec **)hTE;
    textHandle = (**teRec).hText;
    deleteStart = (**teRec).selStart;
    deleteEnd = (**teRec).selEnd;

    if (deleteStart == deleteEnd) return; /* Nothing to delete */

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

    /* Recalculate line breaks */
    TERecalculateLines(hTE);
}

pascal void TEKey(short key, TEHandle hTE)
{
    char ch = (char)(key & 0xFF);
    TERec **teRec;

    if (TEValidateHandle(hTE) != noErr) return;

    teRec = (TERec **)hTE;

    /* Handle special keys */
    switch (ch) {
        case backspaceChar:
            if ((**teRec).selStart == (**teRec).selEnd && (**teRec).selStart > 0) {
                /* Backspace with no selection - delete character before cursor */
                (**teRec).selStart--;
            }
            if ((**teRec).selStart < (**teRec).selEnd) {
                TEDelete(hTE);
            }
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

        case upArrowChar:
            /* Move to previous line - simplified implementation */
            if ((**teRec).selStart > 0) {
                (**teRec).selStart--;
                (**teRec).selEnd = (**teRec).selStart;
            }
            break;

        case downArrowChar:
            /* Move to next line - simplified implementation */
            if ((**teRec).selEnd < (**teRec).teLength) {
                (**teRec).selEnd++;
                (**teRec).selStart = (**teRec).selEnd;
            }
            break;

        default:
            /* Regular character - insert it */
            if (ch >= 0x20 || ch == returnChar || ch == tabChar) {
                /* Delete selection first if any */
                if ((**teRec).selStart != (**teRec).selEnd) {
                    TEDelete(hTE);
                }
                /* Insert the character */
                TEInsert(&ch, 1, hTE);
            }
            break;
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

    if (TEValidateHandle(hTE) != noErr) return 0;

    teRec = (TERec **)hTE;

    /* Simplified implementation - returns 0 or text length */
    if (pt.h < (**teRec).destRect.left || pt.v < (**teRec).destRect.top) {
        return 0;
    }
    return (**teRec).teLength;
}

pascal Point TEGetPoint(short offset, TEHandle hTE)
{
    TERec **teRec;
    Point pt = {0, 0};

    if (TEValidateHandle(hTE) != noErr) return pt;

    teRec = (TERec **)hTE;

    /* Simplified implementation */
    pt.h = (**teRec).destRect.left;
    pt.v = (**teRec).destRect.top;

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