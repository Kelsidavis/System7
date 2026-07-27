#include "MemoryMgr/MemoryManager.h"
/* #include "SystemTypes.h" */
#include "DialogManager/DialogInternal.h"
#include <stdlib.h>
#include <string.h>
/*
 * dialog_manager_core.c - Core Dialog Manager Implementation
 *
 * RE-AGENT-BANNER: This file implements the core Dialog Manager functions
 * implemented based on Mac OS System 7.1 sources. All function
 * implementations are based on implementation and behavioral analysis.
 *
 * Evidence sources:
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "EventManager/EventTypes.h"
#include "WindowManager/WindowManager.h"
#include "DialogManager/dialog_manager_private.h"  /* For full DialogMgrGlobals definition */
#include "DialogManager/dialog_manager_core.h"
#include "DialogManager/dialog_manager_dispatch.h"
#include "DialogManager/DialogLogging.h"


static DialogMgrGlobals gDialogMgrGlobals = {
    .AnalyzedWindowState = 0,
    .IsDialogState = 0,
    .AnalyzedWindow = NULL,
    .SavedMenuState = NULL
};

/* Parameter text storage - evidence from implementation usage patterns */
static Str255 gParamText[4] = {{0}, {0}, {0}, {0}};

/* Dialog Manager initialization state */
static Boolean gDialogMgrInitialized = false;
static ResumeProcPtr gResumeProc = NULL;
static SoundProcPtr gSoundProc = NULL;

/*
 * String conversion utilities - evidence from implementation c2pstr/p2cstr patterns
 */
void C2PStr(char* str) {
    if (!str) return;

    /* Convert C string to Pascal string in place */
    unsigned char len = strlen(str);
    if (len > 255) len = 255;

    /* Move string data forward by 1 byte */
    memmove(str + 1, str, len);
    str[0] = len;  /* Set length byte */
}

void P2CStr(unsigned char* str) {
    if (!str) return;

    /* Convert Pascal string to C string in place */
    unsigned char len = str[0];
    if (len > 255) len = 255;

    /* Move string data back by 1 byte */
    memmove(str, str + 1, len);
    str[len] = '\0';  /* Null terminate */
}

/*
 * InitDialogs - Initialize Dialog Manager

 * implementation suggests this sets up global state
 */
void InitDialogs(ResumeProcPtr resumeProc) {
    gResumeProc = resumeProc;
    gSoundProc = NULL;

    /* Clear global state - evidence from DialogMgrGlobals structure */
    gDialogMgrGlobals.AnalyzedWindowState = 0;
    gDialogMgrGlobals.IsDialogState = 0;
    gDialogMgrGlobals.AnalyzedWindow = NULL;
    gDialogMgrGlobals.SavedMenuState = NULL;

    /* Clear parameter text */
    for (int i = 0; i < 4; i++) {
        gParamText[i][0] = 0;
    }

    gDialogMgrInitialized = true;
}

/*
 * ErrorSound - Set error sound procedure

 */
void ErrorSound(SoundProcPtr soundProc) {
    gSoundProc = soundProc;
}

/*
 * NewDialog - Create new dialog
 * Assembly shows c2pstr/p2cstr conversion for title parameter
 */
DialogPtr NewDialog(void* wStorage, const Rect* boundsRect,
                   const unsigned char* title, Boolean visible, SInt16 procID,
                   WindowPtr behind, Boolean goAwayFlag, SInt32 refCon,
                   Handle itmLstHndl) {
    DialogPtr dialog;

    if (!gDialogMgrInitialized) {
        return NULL;
    }

    /* Allocate dialog structure */
    if (wStorage) {
        dialog = (DialogPtr)wStorage;
    } else {
        dialog = (DialogPtr)NewPtr(sizeof(DialogRecord));
        if (!dialog) return NULL;
    }

    /* Initialize dialog record - evidence from DialogRecord structure */
    memset(dialog, 0, sizeof(DialogRecord));

    /* Set up dialog-specific fields */
    DialogPeek dialogPeek = (DialogPeek)dialog;
    dialogPeek->items = itmLstHndl;
    dialogPeek->textH = NULL;
    dialogPeek->editField = -1;  /* No active edit field */
    dialogPeek->editOpen = 0;
    dialogPeek->aDefItem = ok;   /* Default button is OK */

    /* Create underlying window - evidence from implementation creating window first */
    WindowPtr window = NewWindow(wStorage, boundsRect, title, visible,
                                procID, behind, goAwayFlag, refCon);
    if (!window) {
        if (!wStorage) DisposePtr((Ptr)dialog);
        return NULL;
    }

    /* Copy window structure into dialog - evidence from DialogRecord layout */
    memcpy(&dialogPeek->window, window, sizeof(WindowRecord));

    return dialog;
}

/*
 * NewColorDialog - Create new color dialog
 * Identical to NewDialog but uses color QuickDraw
 */
DialogPtr NewColorDialog(void* wStorage, const Rect* boundsRect,
                        const unsigned char* title, Boolean visible, SInt16 procID,
                        WindowPtr behind, Boolean goAwayFlag, SInt32 refCon,
                        Handle itmLstHndl) {
    /* Implementation identical to NewDialog for this reverse engineering */
    /* implementation shows same parameter handling with different trap vector */
    return NewDialog(wStorage, boundsRect, title, visible, procID,
                    behind, goAwayFlag, refCon, itmLstHndl);
}

/*
 * ParamText - Set parameter text for dialog substitution
 * Assembly shows c2pstr/p2cstr conversion for all 4 parameters
 */
void ParamText(const unsigned char* param0, const unsigned char* param1,
               const unsigned char* param2, const unsigned char* param3) {
    const unsigned char* params[4] = {param0, param1, param2, param3};

    /* Store parameter text - evidence from implementation storing all 4 parameters */
    for (int i = 0; i < 4; i++) {
        if (params[i]) {
            /* Copy parameter text - handle both C and Pascal strings */
            unsigned char len = strlen((const char*)params[i]);
            if (len > 255) len = 255;

            gParamText[i][0] = len;
            memcpy(&gParamText[i][1], params[i], len);
        } else {
            gParamText[i][0] = 0;  /* Empty string */
        }
    }
}

/*
 * GetDialogItemText - Get text from dialog item
 * Assembly shows p2cstr conversion for result text
 */
void GetDialogItemText(Handle item, Str255 text) {
    if (!item || !text) return;

    /* For this reverse engineering, implement basic text retrieval */
    /* Real implementation would extract text from item based on item type */
    text[0] = 0;  /* Empty string by default */

    /* Extract text from dialog item handle */

    /* Check if item contains text data */
    /* Dialog items store text after the item type/rect data */
    if (GetHandleSize(item) > sizeof(DialogItem)) {
        /* Text stored as Pascal string after item data */
        unsigned char* itemData = (unsigned char*)*item;
        unsigned char* textData = itemData + sizeof(DialogItem);

        /* Copy Pascal string */
        unsigned char len = textData[0];
        if (len > 255) len = 255;

        text[0] = len;
        for (int i = 0; i < len; i++) {
            text[i+1] = textData[i+1];
        }

        // DIALOG_LOG_DEBUG("GetDialogItemText: Retrieved %d chars\n", len);
    } else {
        /* No text data */
        text[0] = 0;
    }
}

/*
 * SetDialogItemText - Set text in dialog item
 * Assembly shows c2pstr/p2cstr conversion for text parameter
 */
void SetDialogItemText(Handle item, const Str255 text) {
    if (!item || !text) return;

    /* For this reverse engineering, implement basic text setting */
    /* Real implementation would set text in item based on item type */

    /* Set text in dialog item handle */

    unsigned char len = text[0];
    if (len > 255) len = 255;

    /* CRITICAL: Lock handle before SetHandleSize which can trigger heap compaction */
    HLock(item);

    /* Resize handle to accommodate new text */
    Size newSize = sizeof(DialogItem) + len + 1;
    SetHandleSize(item, newSize);

    if (MemError() == noErr) {
        /* Store text as Pascal string after item data */
        unsigned char* itemData = (unsigned char*)*item;
        unsigned char* textData = itemData + sizeof(DialogItem);

        /* Copy Pascal string */
        textData[0] = len;
        for (int i = 0; i < len; i++) {
            textData[i+1] = text[i+1];
        }

        // DIALOG_LOG_DEBUG("SetDialogItemText: Set %d chars\n", len);
    } else {
        // DIALOG_LOG_DEBUG("SetDialogItemText: Failed to resize handle\n");
    }

    HUnlock(item);
}

/*
 * FindDialogItem - Find dialog item at point
 * Assembly shows Point parameter handling: move.l (a0),-(sp)
 */
SInt16 FindDialogItem(DialogPtr theDialog, Point thePt) {
    if (!theDialog) return 0;

    DialogPeek dialogPeek = (DialogPeek)theDialog;
    Handle itemList = dialogPeek->items;

    if (!itemList) return 0;

    /* For this reverse engineering, implement basic hit testing */
    /* Real implementation would iterate through DITL and test each item rect */

    /* Implement hit testing against dialog item rectangles */

    /* DITL format: count word followed by items */
    short* ditlData = (short*)*itemList;
    short itemCount = ditlData[0] + 1;  /* Count is 0-based */

    /* Start after count word */
    unsigned char* itemPtr = (unsigned char*)&ditlData[1];

    for (short i = 1; i <= itemCount; i++) {
        /* Each item has: placeholder(4), rect(8), type(1), data... */
        itemPtr += 4;  /* Skip placeholder */

        /* Get item rectangle */
        Rect* itemRect = (Rect*)itemPtr;
        itemPtr += sizeof(Rect);

        /* Check if point is in this item's rect */
        if (thePt.h >= itemRect->left && thePt.h < itemRect->right &&
            thePt.v >= itemRect->top && thePt.v < itemRect->bottom) {
            // DIALOG_LOG_DEBUG("FindDialogItem: Hit item %d at (%d,%d)\n", i, thePt.h, thePt.v);
            return i;
        }

        /* Skip type byte and variable-length data */
        unsigned char itemType = *itemPtr++;

        /* Skip data based on type */
        if (itemType & 0x04) {  /* Has text */
            unsigned char textLen = *itemPtr;
            itemPtr += textLen + 1;
        }
    }

    return 0;  /* No item found */
}

/*
 * StdFilterProc - Standard filter procedure
 * implementation is a trampoline that gets actual filter address
 */
Boolean StdFilterProc(DialogPtr dlg, EventRecord* evt, SInt16* itemHit) {
    if (!dlg || !evt || !itemHit) return false;

    /* For this reverse engineering, implement basic standard filtering */
    /* Real implementation calls _GetStdFilterProc to get actual filter */

    /* Handle standard dialog events - evidence from Mac documentation */
    switch (evt->what) {
        case keyDown:
        case autoKey:
            /* Handle Return/Enter as default button - evidence from behavior */
            if ((evt->message & charCodeMask) == '\r' ||
                (evt->message & charCodeMask) == 3) {  /* Enter */
                DialogPeek dialogPeek = (DialogPeek)dlg;
                *itemHit = dialogPeek->aDefItem;
                return true;
            }
            /* Handle Escape/Cmd-. as cancel - evidence from Mac standards */
            if ((evt->message & charCodeMask) == 27 ||  /* Escape */
                ((evt->message & charCodeMask) == '.' &&
                 (evt->modifiers & cmdKey))) {  /* Cmd-. */
                *itemHit = 2;  /* Standard cancel button item */
                return true;
            }
            break;

        case mouseDown:
            /* Let dialog handle mouse clicks normally */
            return false;

        default:
            return false;
    }

    return false;
}

/*
 * GetDialogManagerGlobals - Access global state
 */
DialogMgrGlobals* GetDialogManagerGlobals(void) {
    return &gDialogMgrGlobals;
}
