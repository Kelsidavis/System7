/* #include "SystemTypes.h" */
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
/************************************************************
 *
 * TextSelection.c
 * System 7.1 Portable - TextEdit Selection Management
 *
 * Text selection, highlighting, cursor management, and
 * navigation implementation for TextEdit. Handles all
 * aspects of text selection and cursor positioning.
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


/************************************************************
 * Selection State Management
 ************************************************************/

static TESelectionState* TEGetSelectionStateInternal(TEHandle hTE)
{
    TERec **teRec = (TERec **)hTE;
    TEDispatchHandle hDispatch;

    if (!hTE || !*hTE) return NULL;

    hDispatch = (TEDispatchHandle)(**teRec).hDispatchRec;
    if (!hDispatch) return NULL;

    /* In a full implementation, selection state would be stored in dispatch record */
    /* For now, we'll create a temporary state from the TERec */
    static TESelectionState tempState;

    tempState.selection.start = (**teRec).selStart;
    tempState.selection.end = (**teRec).selEnd;
    tempState.selection.type = ((**teRec).selStart == (**teRec).selEnd) ?
                               kTESelectionCaret : kTESelectionRange;
    tempState.selection.mode = kTESelectionModeChar;
    tempState.selection.sticky = false;
    tempState.selection.visible = (**teRec).active != 0;

    tempState.caretPos = (**teRec).selPoint;
    tempState.caretLine = 0; /* Would be calculated properly */
    tempState.caretOffset = (**teRec).selStart;
    tempState.caretVisible = (**teRec).caretState != 0;
    tempState.caretBlinkTime = 30; /* 30 ticks = 0.5 seconds */
    tempState.caretStyle = kTECaretThin;
    tempState.caretColor.red = 0x0000;
    tempState.caretColor.green = 0x0000;
    tempState.caretColor.blue = 0x0000;

    return &tempState;
}

/************************************************************
 * Core Selection Functions
 ************************************************************/

OSErr TESetSelection(TEHandle hTE, long start, long end)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    /* Clamp selection to text bounds */
    if (start < 0) start = 0;
    if (end < 0) end = 0;
    if (start > (**teRec).teLength) start = (**teRec).teLength;
    if (end > (**teRec).teLength) end = (**teRec).teLength;

    /* Ensure proper ordering */
    if (start > end) {
        long temp = start;
        start = end;
        end = temp;
    }

    (**teRec).selStart = start;
    (**teRec).selEnd = end;

    return noErr;
}

OSErr TEGetSelection(TEHandle hTE, long* start, long* end)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;
    if (!start || !end) return paramErr;

    teRec = (TERec **)hTE;

    *start = (**teRec).selStart;
    *end = (**teRec).selEnd;

    return noErr;
}

OSErr TEGetSelectionRange(TEHandle hTE, TESelectionRange* range)
{
    TERec **teRec;

    if (!hTE || !*hTE || !range) return paramErr;

    teRec = (TERec **)hTE;

    range->start = (**teRec).selStart;
    range->end = (**teRec).selEnd;
    range->type = ((**teRec).selStart == (**teRec).selEnd) ?
                  kTESelectionCaret : kTESelectionRange;
    range->mode = kTESelectionModeChar;
    range->sticky = false;
    range->visible = (**teRec).active != 0;

    return noErr;
}

OSErr TESetSelectionRange(TEHandle hTE, const TESelectionRange* range)
{
    if (!range) return paramErr;

    return TESetSelection(hTE, range->start, range->end);
}

OSErr TEExtendSelection(TEHandle hTE, long newEnd)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    /* Clamp to text bounds */
    if (newEnd < 0) newEnd = 0;
    if (newEnd > (**teRec).teLength) newEnd = (**teRec).teLength;

    (**teRec).selEnd = newEnd;

    return noErr;
}

/************************************************************
 * Word Selection Functions
 ************************************************************/

static Boolean TEIsWordChar(char ch)
{
    return ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_');
}

static long TEFindWordStart(TEHandle hTE, long position)
{
    TERec **teRec;
    char *textPtr;
    long start = position;

    if (!hTE || !*hTE) return position;

    teRec = (TERec **)hTE;
    if (!(**teRec).hText) return position;

    if (start > (**teRec).teLength) start = (**teRec).teLength;
    if (start <= 0) return 0;

    HLock((**teRec).hText);
    textPtr = *(**teRec).hText;

    /* Move back to find word start */
    while (start > 0 && TEIsWordChar(textPtr[start - 1])) {
        start--;
    }

    HUnlock((**teRec).hText);
    return start;
}

static long TEFindWordEnd(TEHandle hTE, long position)
{
    TERec **teRec;
    char *textPtr;
    long end = position;

    if (!hTE || !*hTE) return position;

    teRec = (TERec **)hTE;
    if (!(**teRec).hText) return position;

    if (end < 0) end = 0;
    if (end >= (**teRec).teLength) return (**teRec).teLength;

    HLock((**teRec).hText);
    textPtr = *(**teRec).hText;

    /* Move forward to find word end */
    while (end < (**teRec).teLength && TEIsWordChar(textPtr[end])) {
        end++;
    }

    HUnlock((**teRec).hText);
    return end;
}

OSErr TESelectWord(TEHandle hTE, long position)
{
    long start, end;

    start = TEFindWordStart(hTE, position);
    end = TEFindWordEnd(hTE, position);

    return TESetSelection(hTE, start, end);
}

OSErr TEGetWordBoundaries(TEHandle hTE, long position, long* start, long* end)
{
    if (!start || !end) return paramErr;

    *start = TEFindWordStart(hTE, position);
    *end = TEFindWordEnd(hTE, position);

    return noErr;
}

/************************************************************
 * Line Selection Functions
 ************************************************************/

static long TEFindLineStart(TEHandle hTE, long position)
{
    TERec **teRec;
    char *textPtr;
    long start = position;

    if (!hTE || !*hTE) return position;

    teRec = (TERec **)hTE;
    if (!(**teRec).hText) return position;

    if (start > (**teRec).teLength) start = (**teRec).teLength;
    if (start <= 0) return 0;

    HLock((**teRec).hText);
    textPtr = *(**teRec).hText;

    /* Move back to find line start */
    while (start > 0 && textPtr[start - 1] != '\r' && textPtr[start - 1] != '\n') {
        start--;
    }

    HUnlock((**teRec).hText);
    return start;
}

static long TEFindLineEnd(TEHandle hTE, long position)
{
    TERec **teRec;
    char *textPtr;
    long end = position;

    if (!hTE || !*hTE) return position;

    teRec = (TERec **)hTE;
    if (!(**teRec).hText) return position;

    if (end < 0) end = 0;
    if (end >= (**teRec).teLength) return (**teRec).teLength;

    HLock((**teRec).hText);
    textPtr = *(**teRec).hText;

    /* Move forward to find line end */
    while (end < (**teRec).teLength && textPtr[end] != '\r' && textPtr[end] != '\n') {
        end++;
    }

    HUnlock((**teRec).hText);
    return end;
}

OSErr TESelectLine(TEHandle hTE, long position)
{
    long start, end;

    start = TEFindLineStart(hTE, position);
    end = TEFindLineEnd(hTE, position);

    return TESetSelection(hTE, start, end);
}

OSErr TEGetLineBoundaries(TEHandle hTE, long position, long* start, long* end)
{
    if (!start || !end) return paramErr;

    *start = TEFindLineStart(hTE, position);
    *end = TEFindLineEnd(hTE, position);

    return noErr;
}

/************************************************************
 * Paragraph Selection Functions
 ************************************************************/

static long TEFindParagraphStart(TEHandle hTE, long position)
{
    TERec **teRec;
    char *textPtr;
    long start = position;

    if (!hTE || !*hTE) return position;

    teRec = (TERec **)hTE;
    if (!(**teRec).hText) return position;

    if (start > (**teRec).teLength) start = (**teRec).teLength;
    if (start <= 0) return 0;

    HLock((**teRec).hText);
    textPtr = *(**teRec).hText;

    /* Move back to find paragraph start (double line breaks or start of text) */
    while (start > 0) {
        if (textPtr[start - 1] == '\r' || textPtr[start - 1] == '\n') {
            if (start > 1 && (textPtr[start - 2] == '\r' || textPtr[start - 2] == '\n')) {
                break; /* Found paragraph boundary */
            }
        }
        start--;
    }

    HUnlock((**teRec).hText);
    return start;
}

static long TEFindParagraphEnd(TEHandle hTE, long position)
{
    TERec **teRec;
    char *textPtr;
    long end = position;

    if (!hTE || !*hTE) return position;

    teRec = (TERec **)hTE;
    if (!(**teRec).hText) return position;

    if (end < 0) end = 0;
    if (end >= (**teRec).teLength) return (**teRec).teLength;

    HLock((**teRec).hText);
    textPtr = *(**teRec).hText;

    /* Move forward to find paragraph end */
    while (end < (**teRec).teLength) {
        if (textPtr[end] == '\r' || textPtr[end] == '\n') {
            if (end + 1 < (**teRec).teLength &&
                (textPtr[end + 1] == '\r' || textPtr[end + 1] == '\n')) {
                break; /* Found paragraph boundary */
            }
        }
        end++;
    }

    HUnlock((**teRec).hText);
    return end;
}

OSErr TESelectParagraph(TEHandle hTE, long position)
{
    long start, end;

    start = TEFindParagraphStart(hTE, position);
    end = TEFindParagraphEnd(hTE, position);

    return TESetSelection(hTE, start, end);
}

OSErr TEGetParagraphBoundaries(TEHandle hTE, long position, long* start, long* end)
{
    if (!start || !end) return paramErr;

    *start = TEFindParagraphStart(hTE, position);
    *end = TEFindParagraphEnd(hTE, position);

    return noErr;
}

/************************************************************
 * Document Selection Functions
 ************************************************************/

OSErr TESelectAll(TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    return TESetSelection(hTE, 0, (**teRec).teLength);
}

OSErr TESelectNone(TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    return TESetSelection(hTE, (**teRec).selStart, (**teRec).selStart);
}

Boolean TEHasSelection(TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return false;

    teRec = (TERec **)hTE;

    return (**teRec).selStart != (**teRec).selEnd;
}

Boolean TEIsSelectionEmpty(TEHandle hTE)
{
    return !TEHasSelection(hTE);
}

/************************************************************
 * Caret Management Functions
 ************************************************************/

OSErr TESetCaretPosition(TEHandle hTE, long position)
{
    return TESetSelection(hTE, position, position);
}

long TEGetCaretPosition(TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return 0;

    teRec = (TERec **)hTE;

    return (**teRec).selStart;
}

OSErr TEShowCaret(TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    (**teRec).caretState = 1;

    /* In full implementation, would trigger caret drawing */
    return noErr;
}

OSErr TEHideCaret(TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    (**teRec).caretState = 0;

    /* In full implementation, would erase caret */
    return noErr;
}

Boolean TEIsCaretVisible(TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return false;

    teRec = (TERec **)hTE;

    return (**teRec).caretState != 0;
}

/************************************************************
 * Navigation Functions
 ************************************************************/

OSErr TEMoveLeft(TEHandle hTE, Boolean extend)
{
    TERec **teRec;
    long newPos;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    if (extend) {
        newPos = (**teRec).selEnd;
        if (newPos > 0) newPos--;
        return TEExtendSelection(hTE, newPos);
    } else {
        newPos = (**teRec).selStart;
        if (newPos > 0) newPos--;
        return TESetSelection(hTE, newPos, newPos);
    }
}

OSErr TEMoveRight(TEHandle hTE, Boolean extend)
{
    TERec **teRec;
    long newPos;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    if (extend) {
        newPos = (**teRec).selEnd;
        if (newPos < (**teRec).teLength) newPos++;
        return TEExtendSelection(hTE, newPos);
    } else {
        newPos = (**teRec).selEnd;
        if (newPos < (**teRec).teLength) newPos++;
        return TESetSelection(hTE, newPos, newPos);
    }
}

OSErr TEMoveWordLeft(TEHandle hTE, Boolean extend)
{
    TERec **teRec;
    long newPos;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    newPos = TEFindWordStart(hTE, (**teRec).selStart - 1);

    if (extend) {
        return TEExtendSelection(hTE, newPos);
    } else {
        return TESetSelection(hTE, newPos, newPos);
    }
}

OSErr TEMoveWordRight(TEHandle hTE, Boolean extend)
{
    TERec **teRec;
    long newPos;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    newPos = TEFindWordEnd(hTE, (**teRec).selEnd);

    if (extend) {
        return TEExtendSelection(hTE, newPos);
    } else {
        return TESetSelection(hTE, newPos, newPos);
    }
}

OSErr TEMoveToLineStart(TEHandle hTE, Boolean extend)
{
    TERec **teRec;
    long newPos;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    newPos = TEFindLineStart(hTE, (**teRec).selStart);

    if (extend) {
        return TEExtendSelection(hTE, newPos);
    } else {
        return TESetSelection(hTE, newPos, newPos);
    }
}

OSErr TEMoveToLineEnd(TEHandle hTE, Boolean extend)
{
    TERec **teRec;
    long newPos;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    newPos = TEFindLineEnd(hTE, (**teRec).selStart);

    if (extend) {
        return TEExtendSelection(hTE, newPos);
    } else {
        return TESetSelection(hTE, newPos, newPos);
    }
}

OSErr TEMoveToDocumentStart(TEHandle hTE, Boolean extend)
{
    if (extend) {
        return TEExtendSelection(hTE, 0);
    } else {
        return TESetSelection(hTE, 0, 0);
    }
}

OSErr TEMoveToDocumentEnd(TEHandle hTE, Boolean extend)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    if (extend) {
        return TEExtendSelection(hTE, (**teRec).teLength);
    } else {
        return TESetSelection(hTE, (**teRec).teLength, (**teRec).teLength);
    }
}

/************************************************************
 * Hit Testing Functions
 ************************************************************/

long TEPointToOffset(TEHandle hTE, Point pt)
{
    TERec **teRec;

    if (!hTE || !*hTE) return 0;

    teRec = (TERec **)hTE;

    /* Simplified implementation - just check bounds */
    if (pt.h < (**teRec).destRect.left || pt.v < (**teRec).destRect.top) {
        return 0;
    }
    if (pt.h > (**teRec).destRect.right || pt.v > (**teRec).destRect.bottom) {
        return (**teRec).teLength;
    }

    /* In full implementation, would calculate based on font metrics and layout */
    return (**teRec).teLength / 2; /* Rough approximation */
}

Point TEOffsetToPoint(TEHandle hTE, long offset)
{
    TERec **teRec;
    Point pt = {0, 0};

    if (!hTE || !*hTE) return pt;

    teRec = (TERec **)hTE;

    /* Simplified implementation */
    pt.h = (**teRec).destRect.left;
    pt.v = (**teRec).destRect.top;

    return pt;
}

Boolean TEPointInSelection(TEHandle hTE, Point pt)
{
    long offset;
    TERec **teRec;

    if (!hTE || !*hTE) return false;

    teRec = (TERec **)hTE;

    if ((**teRec).selStart == (**teRec).selEnd) return false;

    offset = TEPointToOffset(hTE, pt);
    return (offset >= (**teRec).selStart && offset < (**teRec).selEnd);
}

/************************************************************
 * Selection Utilities
 ************************************************************/

long TEGetSelectionLength(TEHandle hTE)
{
    TERec **teRec;

    if (!hTE || !*hTE) return 0;

    teRec = (TERec **)hTE;

    return (**teRec).selEnd - (**teRec).selStart;
}

OSErr TEGetSelectedText(TEHandle hTE, Handle* textHandle)
{
    TERec **teRec;
    long selLength;
    long selStart;
    char *srcPtr;

    if (!hTE || !*hTE || !textHandle) return paramErr;

    /* CRITICAL: Lock hTE before dereferencing to prevent heap compaction issues */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;
    selLength = (**teRec).selEnd - (**teRec).selStart;

    if (selLength <= 0) {
        *textHandle = NULL;
        HUnlock((Handle)hTE);
        return noErr;
    }

    selStart = (**teRec).selStart;
    HUnlock((Handle)hTE);

    *textHandle = NewHandle(selLength);
    if (!*textHandle) return MemError();

    HLock((Handle)hTE);
    HLock((**teRec).hText);
    HLock(*textHandle);

    srcPtr = *(**teRec).hText + selStart;
    memcpy(**textHandle, srcPtr, selLength);

    HUnlock(*textHandle);
    HUnlock((**teRec).hText);
    HUnlock((Handle)hTE);

    return noErr;
}

OSErr TEValidateSelection(TEHandle hTE, long* start, long* end)
{
    TERec **teRec;

    if (!hTE || !*hTE || !start || !end) return paramErr;

    teRec = (TERec **)hTE;

    /* Clamp to valid range */
    if (*start < 0) *start = 0;
    if (*end < 0) *end = 0;
    if (*start > (**teRec).teLength) *start = (**teRec).teLength;
    if (*end > (**teRec).teLength) *end = (**teRec).teLength;

    /* Ensure proper ordering */
    if (*start > *end) {
        long temp = *start;
        *start = *end;
        *end = temp;
    }

    return noErr;
}

Boolean TEIsValidSelectionRange(TEHandle hTE, long start, long end)
{
    TERec **teRec;

    if (!hTE || !*hTE) return false;

    teRec = (TERec **)hTE;

    return (start >= 0 && end >= 0 &&
            start <= (**teRec).teLength && end <= (**teRec).teLength &&
            start <= end);
}
