/*
 * StandardFileHAL.h - Hardware Abstraction Layer for Standard File Package
 *
 * Internal interface between StandardFile.c and platform-specific implementations
 */

#ifndef STANDARDFILE_HAL_H
#define STANDARDFILE_HAL_H

#include "SystemTypes.h"
#include "StandardFile/StandardFile.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HAL Initialization */
OSErr StandardFile_HAL_Init(void);

/* Dialog Creation/Disposal */
OSErr StandardFile_HAL_CreateOpenDialog(DialogPtr *outDialog, ConstStr255Param prompt);
OSErr StandardFile_HAL_CreateSaveDialog(DialogPtr *outDialog, ConstStr255Param prompt,
                                        ConstStr255Param defaultName);
void StandardFile_HAL_DisposeOpenDialog(DialogPtr dialog);
void StandardFile_HAL_DisposeSaveDialog(DialogPtr dialog);

/* Dialog Interaction */
void StandardFile_HAL_RunDialog(DialogPtr dialog, short *itemHit);

/* File List Management */
void StandardFile_HAL_ClearFileList(DialogPtr dialog);
void StandardFile_HAL_AddFileToList(DialogPtr dialog, const FSSpec *spec, OSType fileType);
void StandardFile_HAL_UpdateFileList(DialogPtr dialog);
void StandardFile_HAL_SelectFile(DialogPtr dialog, short index);
short StandardFile_HAL_GetSelectedFile(DialogPtr dialog);

/* Save Dialog Name Field */
void StandardFile_HAL_SetSaveFileName(DialogPtr dialog, ConstStr255Param name);
void StandardFile_HAL_GetSaveFileName(DialogPtr dialog, Str255 name);
const FSSpec* StandardFile_HAL_GetSelectedFileSpec(void);

/* User Confirmations */
Boolean StandardFile_HAL_ConfirmReplace(ConstStr255Param fileName);
Boolean StandardFile_HAL_GetNewFolderName(Str255 folderName);

/* Navigation */
OSErr StandardFile_HAL_GetDefaultLocation(short *vRefNum, long *dirID);
OSErr StandardFile_HAL_EjectVolume(short vRefNum);
OSErr StandardFile_HAL_NavigateToDesktop(short *vRefNum, long *dirID);
Boolean StandardFile_HAL_HandleDirPopup(DialogPtr dialog, short vRefNum,
                                        long *selectedDir);

#ifdef __cplusplus
}
#endif

#endif /* STANDARDFILE_HAL_H */
