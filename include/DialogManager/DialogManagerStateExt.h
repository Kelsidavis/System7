#ifndef DIALOGMANAGERSTATEEXT_H
#define DIALOGMANAGERSTATEEXT_H

#include "DialogManagerInternal.h"
#include "DialogManager.h"

/* DialogRecord is already defined in SystemTypes.h */

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

/* TextEdit state follows the canonical manager state in the same allocation. */
typedef struct DialogManagerState_Extended {
    DialogManagerState base;

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
