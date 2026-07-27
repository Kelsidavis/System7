/* #include "SystemTypes.h" */
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
/************************************************************
 *
 * TextClipboard.c
 * System 7.1 Portable - TextEdit Clipboard Operations
 *
 * Cut, copy, paste operations with Scrap Manager integration.
 * Handles text and styled text clipboard operations with
 * full System 7.1 compatibility.
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
#include "Scrap.h"


/************************************************************
 * Clipboard Constants
 ************************************************************/

#define kTEScrapType     'TEXT'
#define kTEStyleScrapType 'styl'
#define kTEUnicodeScrapType 'utxt'

/************************************************************
 * Global Scrap State (System 7.1 compatibility)
 ************************************************************/

static Handle gTEScrapHandle = NULL;
static long gTEScrapLength = 0;
static Boolean gTEScrapDirty = false;

/************************************************************
 * Internal Scrap Utilities
 ************************************************************/

static OSErr TEInitializeScrap(void)
{
    if (!gTEScrapHandle) {
        gTEScrapHandle = NewHandle(0);
        if (!gTEScrapHandle) return MemError();
    }

    gTEScrapLength = 0;
    gTEScrapDirty = false;

    return noErr;
}

static OSErr TEValidateScrapHandle(void)
{
    if (!gTEScrapHandle) {
        return TEInitializeScrap();
    }

    /* Verify handle is still valid */
    if (GetHandleSize(gTEScrapHandle) < 0) {
        gTEScrapHandle = NewHandle(0);
        if (!gTEScrapHandle) return MemError();
        gTEScrapLength = 0;
    }

    return noErr;
}

static OSErr TEResizeScrap(long newSize)
{
    OSErr err;

    err = TEValidateScrapHandle();
    if (err != noErr) return err;

    if (newSize < 0) newSize = 0;

    SetHandleSize(gTEScrapHandle, newSize);
    err = MemError();
    if (err != noErr) return err;

    if (newSize < gTEScrapLength) {
        gTEScrapLength = newSize;
    }

    return noErr;
}

/************************************************************
 * System Scrap Manager Integration
 ************************************************************/

pascal OSErr TEToScrap(void)
{
    OSErr err;
    long offset;

    err = TEValidateScrapHandle();
    if (err != noErr) return err;

    if (gTEScrapLength == 0) {
        /* Clear the system scrap */
        err = ZeroScrap();
        if (err != noErr) return err;
        return noErr;
    }

    /* Clear existing scrap */
    err = ZeroScrap();
    if (err != noErr) return err;

    /* Put text into system scrap */
    HLock(gTEScrapHandle);
    err = PutScrap(gTEScrapLength, kTEScrapType, *gTEScrapHandle);
    HUnlock(gTEScrapHandle);

    if (err != noErr) return err;

    gTEScrapDirty = false;
    return noErr;
}

pascal OSErr TEFromScrap(void)
{
    Handle scrapHandle;
    long scrapLength;
    long scrapOffset;
    OSErr err;

    /* Get text from system scrap */
    scrapLength = GetScrap(NULL, kTEScrapType, &scrapOffset);
    if (scrapLength < 0) {
        /* No text in scrap or error */
        gTEScrapLength = 0;
        return (OSErr)scrapLength;
    }

    /* Resize our scrap handle */
    err = TEResizeScrap(scrapLength);
    if (err != noErr) return err;

    if (scrapLength > 0) {
        /* Get the actual scrap data */
        scrapHandle = NULL;
        scrapLength = GetScrap(&scrapHandle, kTEScrapType, &scrapOffset);
        if (scrapLength < 0 || !scrapHandle) {
            gTEScrapLength = 0;
            return (OSErr)scrapLength;
        }

        /* Copy scrap data to our handle */
        HLock(gTEScrapHandle);
        HLock(scrapHandle);
        memcpy(*gTEScrapHandle, *scrapHandle, scrapLength);
        HUnlock(scrapHandle);
        HUnlock(gTEScrapHandle);
    }

    gTEScrapLength = scrapLength;
    gTEScrapDirty = false;

    return noErr;
}

pascal void TESetScrapLength(long length)
{
    OSErr err;

    if (length < 0) length = 0;

    err = TEResizeScrap(length);
    if (err == noErr) {
        gTEScrapLength = length;
        gTEScrapDirty = true;
    }
}

pascal void TESetScrapLen(long length)
{
    TESetScrapLength(length);
}

/************************************************************
 * Core Clipboard Operations
 ************************************************************/

pascal void TECut(TEHandle hTE)
{
    /* Copy selection to clipboard, then delete it */
    TECopy(hTE);
    TEDelete(hTE);
}

pascal void TECopy(TEHandle hTE)
{
    TERec **teRec;
    Handle textHandle;
    long selStart, selEnd, selLength;
    char *srcPtr;
    OSErr err;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;
    textHandle = (**teRec).hText;

    if (!textHandle) return;

    selStart = (**teRec).selStart;
    selEnd = (**teRec).selEnd;
    selLength = selEnd - selStart;

    if (selLength <= 0) {
        /* No selection to copy */
        TESetScrapLength(0);
        return;
    }

    /* Initialize and resize scrap */
    err = TEResizeScrap(selLength);
    if (err != noErr) return;

    /* Copy selected text to scrap */
    HLock(textHandle);
    HLock(gTEScrapHandle);

    /* Validate handles after locking */
    if (!*textHandle || !gTEScrapHandle || !*gTEScrapHandle) {
        HUnlock(gTEScrapHandle);
        HUnlock(textHandle);
        return;
    }

    srcPtr = *textHandle + selStart;
    memcpy(*gTEScrapHandle, srcPtr, selLength);

    HUnlock(gTEScrapHandle);
    HUnlock(textHandle);

    gTEScrapLength = selLength;
    gTEScrapDirty = true;

    /* Update system scrap */
    TEToScrap();
}

pascal void TEPaste(TEHandle hTE)
{
    TERec **teRec;
    OSErr err;

    if (!hTE || !*hTE) return;

    teRec = (TERec **)hTE;

    /* Update from system scrap first */
    TEFromScrap();

    if (gTEScrapLength <= 0) return; /* Nothing to paste */

    /* Delete current selection first */
    if ((**teRec).selStart != (**teRec).selEnd) {
        TEDelete(hTE);
    }

    /* Insert scrap contents */
    HLock(gTEScrapHandle);
    TEInsert(*gTEScrapHandle, gTEScrapLength, hTE);
    HUnlock(gTEScrapHandle);
}

/************************************************************
 * Styled Text Clipboard Operations
 ************************************************************/

pascal void TEStylePaste(TEHandle hTE)
{
    /* For now, just do regular paste */
    /* Full implementation would handle styled text scrap */
    TEPaste(hTE);
}

pascal void TEStylPaste(TEHandle hTE)
{
    TEStylePaste(hTE);
}

pascal StScrpHandle TEGetStyleScrapHandle(TEHandle hTE)
{
    StScrpHandle styleScrap;
    StScrpRec **scrapRec;

    /* Create empty style scrap */
    styleScrap = (StScrpHandle)NewHandleClear(sizeof(StScrpRec));
    if (!styleScrap) return NULL;

    scrapRec = (StScrpRec **)styleScrap;
    (**scrapRec).scrpNStyles = 0;

    /* In full implementation, would extract style information from selection */

    return styleScrap;
}

pascal StScrpHandle GetStyleScrap(TEHandle hTE)
{
    return TEGetStyleScrapHandle(hTE);
}

pascal StScrpHandle GetStylScrap(TEHandle hTE)
{
    return TEGetStyleScrapHandle(hTE);
}

pascal void TEStyleInsert(const void *text, long length, StScrpHandle hST, TEHandle hTE)
{
    if (!hTE || !text || length <= 0) return;

    /* For now, just insert the text without style information */
    TEInsert(text, length, hTE);

    /* In full implementation, would apply style information from hST */
}

pascal void TEStylInsert(const void *text, long length, StScrpHandle hST, TEHandle hTE)
{
    TEStyleInsert(text, length, hST, hTE);
}

pascal void TEUseStyleScrap(long rangeStart, long rangeEnd, StScrpHandle newStyles,
                           Boolean fRedraw, TEHandle hTE)
{
    if (!hTE || !newStyles) return;

    /* In full implementation, would apply styles to the specified range */
    /* For now, this is a no-op */
}

pascal void SetStyleScrap(long rangeStart, long rangeEnd, StScrpHandle newStyles,
                         Boolean redraw, TEHandle hTE)
{
    TEUseStyleScrap(rangeStart, rangeEnd, newStyles, redraw, hTE);
}

pascal void SetStylScrap(long rangeStart, long rangeEnd, StScrpHandle newStyles,
                        Boolean redraw, TEHandle hTE)
{
    TEUseStyleScrap(rangeStart, rangeEnd, newStyles, redraw, hTE);
}

/************************************************************
 * Advanced Clipboard Operations
 ************************************************************/

OSErr TECopyRange(TEHandle hTE, long start, long end)
{
    TERec **teRec;
    long oldSelStart, oldSelEnd;
    OSErr err;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    /* Validate range */
    if (start < 0 || end < 0 || start > (**teRec).teLength || end > (**teRec).teLength) {
        return paramErr;
    }

    if (start > end) {
        long temp = start;
        start = end;
        end = temp;
    }

    /* Save current selection */
    oldSelStart = (**teRec).selStart;
    oldSelEnd = (**teRec).selEnd;

    /* Set selection to specified range */
    (**teRec).selStart = start;
    (**teRec).selEnd = end;

    /* Copy the range */
    TECopy(hTE);

    /* Restore original selection */
    (**teRec).selStart = oldSelStart;
    (**teRec).selEnd = oldSelEnd;

    return noErr;
}

OSErr TECutRange(TEHandle hTE, long start, long end)
{
    OSErr err;

    err = TECopyRange(hTE, start, end);
    if (err != noErr) return err;

    /* Set selection to the range and delete it */
    TESetSelection(hTE, start, end);
    TEDelete(hTE);

    return noErr;
}

OSErr TEPasteAt(TEHandle hTE, long position)
{
    TERec **teRec;
    long oldSelStart, oldSelEnd;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    if (position < 0 || position > (**teRec).teLength) return paramErr;

    /* Save current selection */
    oldSelStart = (**teRec).selStart;
    oldSelEnd = (**teRec).selEnd;

    /* Set insertion point */
    (**teRec).selStart = position;
    (**teRec).selEnd = position;

    /* Paste at the position */
    TEPaste(hTE);

    return noErr;
}

/************************************************************
 * Clipboard Format Support
 ************************************************************/

OSErr TECopyAsRTF(TEHandle hTE, Handle *rtfHandle)
{
    TERec **teRec;
    Handle textHandle;
    long selStart, selEnd, selLength;
    char *textPtr;
    char *rtfPtr;
    long rtfLength;
    long actualSize;

    if (!hTE || !*hTE || !rtfHandle) return paramErr;

    *rtfHandle = NULL;

    /* CRITICAL: Lock hTE before dereferencing to prevent heap compaction issues */
    HLock((Handle)hTE);
    teRec = (TERec **)hTE;
    textHandle = (**teRec).hText;

    if (!textHandle) {
        HUnlock((Handle)hTE);
        return paramErr;
    }

    selStart = (**teRec).selStart;
    selEnd = (**teRec).selEnd;
    selLength = selEnd - selStart;

    if (selLength <= 0) {
        HUnlock((Handle)hTE);
        return paramErr;
    }

    /* Guard against integer overflow in buffer size calculation */
    /* Max reasonable selection: ~100MB text = ~600MB RTF with 6x expansion */
    #define MAX_SAFE_SELECTION (100 * 1024 * 1024)
    if (selLength > MAX_SAFE_SELECTION) {
        HUnlock((Handle)hTE);
        return memFullErr;  /* Selection too large */
    }

    /* Create minimal RTF wrapper */
    /* Calculate safe buffer size accounting for escaping:
     * - Each char could need escaping (2 bytes: \ + char)
     * - Carriage returns become "\\par " (6 bytes)
     * - Header + footer (~60 bytes)
     */
    const char* rtfHeader = "{\\rtf1\\mac\\deff0 {\\fonttbl\\f0\\fswiss Monaco;} \\f0 ";
    const char* rtfFooter = "}";
    rtfLength = strlen(rtfHeader) + (selLength * 6) + strlen(rtfFooter) + 1;

    HUnlock((Handle)hTE);

    *rtfHandle = NewHandle(rtfLength);
    if (!*rtfHandle) return MemError();

    HLock((Handle)hTE);
    textHandle = (**teRec).hText;
    selStart = (**teRec).selStart;

    HLock(*rtfHandle);
    HLock(textHandle);

    rtfPtr = **rtfHandle;
    textPtr = *textHandle + selStart;
    char* rtfEnd = rtfPtr + rtfLength;  /* Track end for safety */

    /* Create basic RTF document with bounds checking */
    size_t headerLen = strlen(rtfHeader);
    if (rtfPtr + headerLen >= rtfEnd) {
        HUnlock(*rtfHandle);
        HUnlock(textHandle);
        HUnlock((Handle)hTE);
        DisposeHandle(*rtfHandle);
        *rtfHandle = NULL;
        return memFullErr;
    }
    memcpy(rtfPtr, rtfHeader, headerLen);
    rtfPtr += headerLen;

    /* Copy text, escaping special characters */
    for (long i = 0; i < selLength; i++) {
        /* Safety check: ensure at least 6 bytes available (max expansion) */
        if (rtfPtr + 6 >= rtfEnd) break;

        char ch = textPtr[i];
        if (ch == '\\' || ch == '{' || ch == '}') {
            *rtfPtr++ = '\\';
        }
        if (ch == '\r') {
            memcpy(rtfPtr, "\\par ", 5);
            rtfPtr += 5;
        } else {
            *rtfPtr++ = ch;
        }
    }

    /* Add footer with safety check */
    if (rtfPtr + strlen(rtfFooter) < rtfEnd) {
        memcpy(rtfPtr, rtfFooter, strlen(rtfFooter));
        rtfPtr += strlen(rtfFooter);
    }

    /* Calculate actual size before unlocking */
    actualSize = rtfPtr - **rtfHandle;

    HUnlock(textHandle);
    HUnlock(*rtfHandle);
    HUnlock((Handle)hTE);

    /* Resize handle to actual size */
    SetHandleSize(*rtfHandle, actualSize);

    return noErr;
}

OSErr TEPasteFromRTF(TEHandle hTE, Handle rtfHandle)
{
    /* Simplified RTF parsing - just extract plain text */
    /* Full implementation would parse RTF formatting */

    if (!hTE || !rtfHandle) return paramErr;

    /* For now, just treat as plain text after removing RTF codes */
    /* This is a very simplified approach */

    long rtfSize = GetHandleSize(rtfHandle);
    Handle plainTextHandle = NewHandle(rtfSize);
    if (!plainTextHandle) return MemError();

    HLock(rtfHandle);
    HLock(plainTextHandle);

    char *rtfPtr = *rtfHandle;
    char *textPtr = *plainTextHandle;
    long textLength = 0;
    Boolean inGroup = false;
    Boolean inControl = false;

    /* Simple RTF to plain text conversion */
    for (long i = 0; i < rtfSize; i++) {
        char ch = rtfPtr[i];

        if (ch == '{') {
            inGroup = true;
            continue;
        }
        if (ch == '}') {
            inGroup = false;
            continue;
        }
        if (ch == '\\') {
            inControl = true;
            continue;
        }
        if (inControl && (ch == ' ' || ch == '\r' || ch == '\n')) {
            inControl = false;
            continue;
        }
        if (inControl) {
            continue;
        }

        if (!inGroup || (inGroup && ch >= 0x20)) {
            textPtr[textLength++] = ch;
        }
    }

    HUnlock(plainTextHandle);
    HUnlock(rtfHandle);

    /* Insert the extracted text */
    SetHandleSize(plainTextHandle, textLength);
    HLock(plainTextHandle);
    TEInsert(*plainTextHandle, textLength, hTE);
    HUnlock(plainTextHandle);

    DisposeHandle(plainTextHandle);

    return noErr;
}

/************************************************************
 * Unicode Clipboard Support
 ************************************************************/

OSErr TECopyAsUnicode(TEHandle hTE, Handle *unicodeHandle)
{
    /* Unicode support would require text encoding conversion */
    /* For now, return unimplemented */
    if (unicodeHandle) *unicodeHandle = NULL;
    return unimpErr;
}

OSErr TEPasteFromUnicode(TEHandle hTE, Handle unicodeHandle)
{
    /* Unicode support would require text encoding conversion */
    return unimpErr;
}

/************************************************************
 * Clipboard State Queries
 ************************************************************/

Boolean TECanPaste(TEHandle hTE)
{
    long scrapLength;
    long scrapOffset;

    if (!hTE) return false;

    /* Check if there's text in the system scrap */
    scrapLength = GetScrap(NULL, kTEScrapType, &scrapOffset);
    return (scrapLength > 0);
}

Boolean TEHasClipboardText(void)
{
    long scrapLength;
    long scrapOffset;

    scrapLength = GetScrap(NULL, kTEScrapType, &scrapOffset);
    return (scrapLength > 0);
}

long TEGetClipboardTextLength(void)
{
    long scrapLength;
    long scrapOffset;

    scrapLength = GetScrap(NULL, kTEScrapType, &scrapOffset);
    return (scrapLength > 0) ? scrapLength : 0;
}
