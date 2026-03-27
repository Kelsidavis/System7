/*
 * STClipboard.c - SimpleText Clipboard Operations
 *
 * Handles cut/copy/paste and undo operations
 */

#include <string.h>
#include "Apps/SimpleText.h"
#include "MemoryMgr/MemoryManager.h"
#include "ScrapManager/ScrapManager.h"


/* Save current selection for undo */
void STClip_SaveUndo(STDocument* doc)
{
    CharsHandle textHandle;
    SInt16 selStart, selEnd;
    SInt32 selLen;

    if (!doc || !doc->hTE) {
        return;
    }

    /* Get current selection */
    selStart = (*doc->hTE)->selStart;
    selEnd = (*doc->hTE)->selEnd;
    selLen = selEnd - selStart;

    ST_Log("STClip_SaveUndo: Selection %d-%d (%d chars)",
           selStart, selEnd, selLen);

    /* Dispose old undo buffer */
    if (doc->undoText) {
        DisposeHandle(doc->undoText);
        doc->undoText = NULL;
    }

    /* Save selection range */
    doc->undoStart = selStart;
    doc->undoEnd = selEnd;

    /* If there's selected text, save it */
    if (selLen > 0) {
        textHandle = TEGetText(doc->hTE);
        if (textHandle) {
            /* Allocate undo buffer */
            doc->undoText = NewHandle(selLen);
            if (doc->undoText) {
                HLock(textHandle);
                HLock(doc->undoText);
                memcpy(*doc->undoText, *textHandle + selStart, selLen);
                HUnlock(doc->undoText);
                HUnlock(textHandle);
            }
        }
    }
}

/* Cut selected text to clipboard */
void STClip_Cut(STDocument* doc)
{
    ST_Log("STClip_Cut");

    if (!doc || !doc->hTE) {
        return;
    }

    /* Save for undo */
    STClip_SaveUndo(doc);

    /* Cut using TextEdit */
    TECut(doc->hTE);

    /* Mark document as modified */
    STDoc_SetDirty(doc, true);

    /* Update view */
    STView_Draw(doc);
}

/* Copy selected text to clipboard */
void STClip_Copy(STDocument* doc)
{
    ST_Log("STClip_Copy");

    if (!doc || !doc->hTE) {
        return;
    }

    /* Copy using TextEdit */
    TECopy(doc->hTE);
}

/* Paste text from clipboard */
void STClip_Paste(STDocument* doc)
{
    SInt32 scrapLen;
    OSErr err;

    ST_Log("STClip_Paste");

    if (!doc || !doc->hTE) {
        return;
    }

    /* Check if there's text in the scrap */
    err = GetScrap(NULL, 'TEXT', (long *)&scrapLen);
    if (err != noErr || scrapLen <= 0) {
        ST_Log("No text in clipboard");
        return;
    }

    /* Save for undo */
    STClip_SaveUndo(doc);

    /* Paste using TextEdit */
    TEPaste(doc->hTE);

    /* Mark document as modified */
    STDoc_SetDirty(doc, true);

    /* Update view */
    STView_Draw(doc);
}

/* Clear selected text */
void STClip_Clear(STDocument* doc)
{
    ST_Log("STClip_Clear");

    if (!doc || !doc->hTE) {
        return;
    }

    /* Save for undo */
    STClip_SaveUndo(doc);

    /* Delete using TextEdit */
    TEDelete(doc->hTE);

    /* Mark document as modified */
    STDoc_SetDirty(doc, true);

    /* Update view */
    STView_Draw(doc);
}

/* Select all text */
void STClip_SelectAll(STDocument* doc)
{
    ST_Log("STClip_SelectAll");

    if (!doc || !doc->hTE) {
        return;
    }

    /* Select all text */
    TESetSelect(0, (*doc->hTE)->teLength, doc->hTE);

    /* Update view */
    STView_Draw(doc);
}

/* Check if clipboard has text */
Boolean STClip_HasText(void)
{
    SInt32 scrapLen;
    OSErr err;

    /* Check for TEXT in scrap */
    err = GetScrap(NULL, 'TEXT', (long *)&scrapLen);

    return (err == noErr && scrapLen > 0);
}

/* Undo last operation (single-level) */
void STClip_Undo(STDocument* doc)
{
    /* Placeholders for future redo support removed to avoid warnings */
    SInt32 undoLen;

    ST_Log("STClip_Undo");

    if (!doc || !doc->hTE) {
        return;
    }

    /* TODO: capture current selection for future redo support */

    /* If we have undo text, it was a deletion - restore it */
    if (doc->undoText) {
        undoLen = GetHandleSize(doc->undoText);

        /* Set selection to undo position */
        TESetSelect(doc->undoStart, doc->undoStart, doc->hTE);

        /* Insert the undo text */
        HLock(doc->undoText);
        TEInsert(*doc->undoText, undoLen, doc->hTE);
        HUnlock(doc->undoText);

        /* Select the restored text */
        TESetSelect(doc->undoStart, doc->undoStart + undoLen, doc->hTE);

        /* Clear undo buffer */
        DisposeHandle(doc->undoText);
        doc->undoText = NULL;
    }
    /* Otherwise, it was an insertion (paste/typing) - delete the inserted range */
    else if (doc->undoEnd > doc->undoStart) {
        TESetSelect(doc->undoStart, doc->undoEnd, doc->hTE);
        TEDelete(doc->hTE);
        doc->undoEnd = doc->undoStart;
    }

    /* Mark document as modified */
    STDoc_SetDirty(doc, true);

    /* Update view */
    STView_Draw(doc);
}
