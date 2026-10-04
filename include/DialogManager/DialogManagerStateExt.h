#ifndef DIALOGMANAGERSTATEEXT_H
#define DIALOGMANAGERSTATEEXT_H

#include "DialogManagerInternal.h"

/* DialogRecord is already defined in SystemTypes.h */

/* Helper to access extended DialogManagerState fields
   Cast basic DialogManagerState* to extended version */
#define GET_EXTENDED_DLG_STATE(state) ((DialogManagerState_Extended*)(state))

/* TextEdit state follows the canonical manager state in the same allocation. */
typedef struct DialogEditTextState {
    struct DialogEditTextState* next;
    DialogPtr owner;
    void* teHandles[256];
    SInt16 focusedItem;
    UInt32 caretBlinkTime;
    Boolean caretVisible;
} DialogEditTextState;

typedef struct DialogManagerState_Extended {
    DialogManagerState base;
    DialogEditTextState* dialogStates;
} DialogManagerState_Extended;

/* Free the edit fields belonging to one dialog, and clear their slots. */
void DialogEditText_ReleaseAll(DialogPtr owner);

#endif /* DIALOGMANAGERSTATEEXT_H */
