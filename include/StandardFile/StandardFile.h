/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * StandardFile.h - Standard File Package Interface
 * System 7.1 file open/save dialogs
 */

#ifndef STANDARDFILE_H
#define STANDARDFILE_H

#include "SystemTypes.h"
#include "DialogManager/DialogManager.h"
#include "EventManager/EventManager.h"
#include "StandardFile/Compat.h"

/* Dialog IDs */
#define putDlgID        (-3999)    /* Classic save dialog */
#define getDlgID        (-4000)    /* Classic open dialog */
#define sfPutDialogID   (-6043)    /* System 7 save dialog */
#define sfGetDialogID   (-6042)    /* System 7 open dialog */

/* Dialog item numbers */
#define sfItemOpenButton        1
#define sfItemCancelButton      2
#define sfItemBalloonHelp       3
#define sfItemVolumeUser        4
#define sfItemEjectButton       5
#define sfItemDesktopButton     6
#define sfItemFileListUser      7
#define sfItemPopUpMenuUser     8
#define sfItemDividerLinePict   9
#define sfItemFileNameTextEdit  10
#define sfItemPromptStaticText  11
#define sfItemNewFolderUser     12

/* Hook messages */
#define sfHookFirstCall         (-1)
#define sfHookCharOffset        0x1000
#define sfHookNullEvent         100
#define sfHookRebuildList       101
#define sfHookFolderPopUp       102
#define sfHookOpenFolder        103
#define sfHookLastCall          (-2)

/* File types */

/* Ptr is defined in MacTypes.h */

/* Classic reply structure */

/* System 7 reply structure */

/* Callback types */

/* System 7 callbacks with user data */

/* Classic Standard File routines */
void SFPutFile(Point where,
               ConstStr255Param prompt,
               ConstStr255Param origName,
               DlgHookProcPtr dlgHook,
               SFReply *reply);

void SFPPutFile(Point where,
                ConstStr255Param prompt,
                ConstStr255Param origName,
                DlgHookProcPtr dlgHook,
                SFReply *reply,
                short dlgID,
                ModalFilterProcPtr filterProc);

void SFGetFile(Point where,
               ConstStr255Param prompt,
               FileFilterProcPtr fileFilter,
               short numTypes,
               ConstSFTypeListPtr typeList,
               DlgHookProcPtr dlgHook,
               SFReply *reply);

void SFPGetFile(Point where,
                ConstStr255Param prompt,
                FileFilterProcPtr fileFilter,
                short numTypes,
                ConstSFTypeListPtr typeList,
                DlgHookProcPtr dlgHook,
                SFReply *reply,
                short dlgID,
                ModalFilterProcPtr filterProc);

/* System 7 Standard File routines */
void StandardPutFile(ConstStr255Param prompt,
                    ConstStr255Param defaultName,
                    StandardFileReply *reply);

void StandardGetFile(FileFilterProcPtr fileFilter,
                    short numTypes,
                    ConstSFTypeListPtr typeList,
                    StandardFileReply *reply);

/* Where the next dialog opens; each dialog leaves it where it was closed */
void StandardFile_SetStartLocation(short vRefNum, long dirID);
void StandardFile_GetStartLocation(short *vRefNum, long *dirID);

void CustomPutFile(ConstStr255Param prompt,
                  ConstStr255Param defaultName,
                  StandardFileReply *reply,
                  short dlgID,
                  Point where,
                  DlgHookYDProcPtr dlgHook,
                  ModalFilterYDProcPtr modalFilter,
                  ActivateYDProcPtr activeList,
                  void *yourDataPtr);

void CustomGetFile(FileFilterYDProcPtr fileFilter,
                  short numTypes,
                  ConstSFTypeListPtr typeList,
                  StandardFileReply *reply,
                  short dlgID,
                  Point where,
                  DlgHookYDProcPtr dlgHook,
                  ModalFilterYDProcPtr modalFilter,
                  ActivateYDProcPtr activeList,
                  void *yourDataPtr,
                  ConstStr255Param prompt);

#endif /* STANDARDFILE_H */
