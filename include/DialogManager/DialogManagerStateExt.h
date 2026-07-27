#ifndef DIALOGMANAGERSTATEEXT_H
#define DIALOGMANAGERSTATEEXT_H

#include "../SystemTypes.h"
#include "../WindowManager/WindowManager.h"
#include "DialogManager.h"

/* DialogRecord is already defined in SystemTypes.h */

/* DialogGlobals and DialogManagerState are now defined in DialogManagerInternal.h */

/* Only define extended DialogManagerState if not already defined */
#ifndef DIALOGMANAGERINTERNAL_H
/* Extended DialogManagerState with all expected fields */
typedef struct DialogManagerState {
    /* Basic fields from DialogManagerInternal.h */
    DialogPtr currentDialog;
    short modalDepth;
    Boolean inProgress;
    Handle itemList;
    short itemCount;

    /* Extended fields expected by DialogManagerCore.c */
    DialogGlobals globals;
    Boolean initialized;
    SInt16 modalLevel;
    Boolean systemModal;
    Boolean useNativeDialogs;
    Boolean useAccessibility;
    float scaleFactor;
    void* platformContext;
    DialogPtr modalStack[16];

    /* Edit-text focus tracking */
    SInt16 focusedEditTextItem;     /* Item number of focused edit text, or 0 if none */
    UInt32 caretBlinkTime;          /* Tick count of last caret blink */
    Boolean caretVisible;           /* Current caret visibility state */
} DialogManagerState;
#endif

/* DialogItemInternal for DialogItems.h */
typedef struct DialogItemInternal {
    Handle itemHandle;
    Rect itemRect;
    UInt8 itemType;
    UInt8 itemLength;
    SInt16 controlItem;
    void* itemData;
} DialogItemInternal;

/* Helper to access extended DialogManagerState fields
   Cast basic DialogManagerState* to extended version */
#define GET_EXTENDED_DLG_STATE(state) ((DialogManagerState_Extended*)(state))

/* Extended state type with focus tracking fields */
typedef struct DialogManagerState_Extended {
    /* Basic fields from DialogManagerInternal.h */
    DialogPtr currentDialog;
    short modalDepth;
    Boolean inProgress;
    Handle itemList;
    short itemCount;

    /* Extended fields */
    DialogGlobals globals;
    Boolean initialized;
    SInt16 modalLevel;
    Boolean systemModal;
    Boolean useNativeDialogs;
    Boolean useAccessibility;
    float scaleFactor;
    void* platformContext;
    DialogPtr modalStack[16];

    /* Edit-text focus tracking */
    SInt16 focusedEditTextItem;
    UInt32 caretBlinkTime;
    Boolean caretVisible;

    /*
     * TextEdit integration for dialog items.
     *
     * One array, indexed by item number alone, so it can only ever describe a
     * single dialog. Nothing said so, and nothing cleared it when a dialog
     * went away: the entries outlived their dialog, and the next window to
     * take an update event drew a disposed edit field into its own port -
     * which is why cancelling SimpleText's Find box left garbage across the
     * top of the document underneath it.
     *
     * teOwner names the dialog the entries belong to, so the assumption is
     * written down and can be enforced.
     */
    void* teHandles[256];       /* TEHandles for dialog items (max 256 items) */
    DialogPtr teOwner;          /* the dialog those handles belong to */
} DialogManagerState_Extended;

/* Free the edit fields belonging to one dialog, and clear their slots. */
void DialogEditText_ReleaseAll(DialogPtr owner);

#endif /* DIALOGMANAGERSTATEEXT_H */