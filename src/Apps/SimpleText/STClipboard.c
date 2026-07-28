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

    /* Cut using TextEdit, then hand the result to the system scrap.
     * See STClip_Copy for why that second step is not optional. */
    TECut(doc->hTE);
    TEToScrap();

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

    /*
     * Copy, then publish to the system scrap.
     *
     * TECopy fills TextEdit's own scrap and nothing else - that is what the
     * Toolbox does, and TEToScrap is the call that moves it to the scrap the
     * rest of the system can see. Without it Copy and Paste were using two
     * different clipboards: STClip_Paste asks GetScrap how much TEXT is
     * available, found nothing there however much had just been copied, and
     * returned before it ever reached TEPaste. Copy appeared to work and
     * Paste did nothing at all.
     */
    TECopy(doc->hTE);
    TEToScrap();
}

/* Paste text from clipboard */
void STClip_Paste(STDocument* doc)
{
    SInt32 scrapLen;
    long scrapOffset = 0;

    ST_Log("STClip_Paste");

    if (!doc || !doc->hTE) {
        return;
    }

    /*
     * GetScrap returns the number of bytes, or a negative OSErr. The third
     * argument is where the data starts, which it writes only when given a
     * destination handle - and this passes NULL.
     *
     * Both of this file's calls read it the other way round: they took the
     * return value for an error code and the untouched offset for a length.
     * So the test "err == noErr && len > 0" was false exactly when the scrap
     * did hold something, because a successful call returns the byte count
     * rather than zero. Paste stayed disabled after every Copy, and being
     * disabled it never reached this function at all - MenuKey does not
     * report a dimmed item.
     */
    scrapLen = GetScrap(NULL, 'TEXT', &scrapOffset);
    if (scrapLen <= 0) {
        ST_Log("No text in clipboard");
        return;
    }

    /* Check if paste would exceed TextEdit 32K limit */
    SInt32 currentLen = (*doc->hTE)->teLength;
    SInt32 selLen = (*doc->hTE)->selEnd - (*doc->hTE)->selStart;
    SInt32 newLen = currentLen - selLen + scrapLen;
    if (newLen > kMaxFileSize) {
        ST_Log("STClip_Paste: Would exceed 32K limit (%d bytes)", (int)newLen);
        ST_Beep();
        return;
    }

    /* Save for undo */
    STClip_SaveUndo(doc);

    /* Load the system scrap into TextEdit's before pasting, the counterpart
     * of the TEToScrap above. */
    TEFromScrap();
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
    long scrapOffset = 0;

    /* The byte count comes back as the return value; see STClip_Paste. */
    return (GetScrap(NULL, 'TEXT', &scrapOffset) > 0);
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
