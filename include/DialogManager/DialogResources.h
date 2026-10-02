/*
 * DialogResources.h - loading dialog and alert templates from resources
 *
 * 'DLOG' (dialog template), 'DITL' (item list) and 'ALRT' (alert template),
 * in the layouts Inside Macintosh: Macintosh Toolbox Essentials gives them
 * (pp. 6-151 to 6-156). Resource data is big-endian.
 */

#ifndef DIALOG_RESOURCES_H
#define DIALOG_RESOURCES_H

#include "SystemTypes.h"
#include "DialogTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kDialogResourceType     FOURCC('D', 'L', 'O', 'G') /* Dialog template resource */
#define kDialogItemResourceType FOURCC('D', 'I', 'T', 'L') /* Dialog item list resource */
#define kAlertResourceType      FOURCC('A', 'L', 'R', 'T') /* Alert template resource */

/* Load a 'DLOG' into a new template the caller disposes. */
OSErr LoadDialogTemplate(SInt16 dialogID, DialogTemplate** template);

/* Copy a 'DITL' into a handle of the caller's own - a dialog keeps its item
 * list and edits it, which a resource handle must not be subjected to. */
OSErr LoadDialogItemList(SInt16 itemListID, Handle* itemList);

/* Load an 'ALRT' into a new template the caller disposes. */
OSErr LoadAlertTemplate(SInt16 alertID, AlertTemplate** template);

void DisposeDialogTemplate(DialogTemplate* template);
void DisposeDialogItemList(Handle itemList);
void DisposeAlertTemplate(AlertTemplate* template);

/* The parsers the loaders use, on resource data already in hand. */
OSErr ParseDLOGResource(Handle resourceData, DialogTemplate** template);
OSErr ParseALRTResource(Handle resourceData, AlertTemplate** template);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_RESOURCES_H */
