/* DialogEvents.h - Dialog event API declarations */

#ifndef DIALOG_EVENTS_H
#define DIALOG_EVENTS_H

#include "SystemTypes.h"
#include "DialogTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

Boolean IsDialogEvent(const EventRecord* evt);
Boolean DialogSelect(const EventRecord* evt, DialogPtr* which, SInt16* itemHit);
SInt16 AdvanceDialogFocus(DialogPtr theDialog, Boolean backward);

/* Used by the Dialog Manager event dispatch and idle paths. */
void HandleDialogActivate(DialogPtr theDialog, const EventRecord* theEvent,
                          Boolean activating);
void ProcessDialogIdle(DialogPtr theDialog);
void InitDialogEvents(void);
void CleanupDialogEvents(void);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_EVENTS_H */
