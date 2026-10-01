/* Dialog Manager Internal Functions */
/* This header exists but prototypes are in specific DialogManager headers */
/* See: DialogDrawing.h, DialogEditText.h, DialogHelpers.h, etc. */
#ifndef DIALOG_INTERNAL_H
#define DIALOG_INTERNAL_H

#include "SystemTypes.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogDrawing.h"
#include "DialogManager/DialogEditText.h"
#include "DialogManager/DialogHelpers.h"

/* Dialog Manager State */
struct DialogManagerState;
struct DialogManagerState* GetDialogManagerState(void);

#endif /* DIALOG_INTERNAL_H */
