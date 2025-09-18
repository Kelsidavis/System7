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

#include "DeskManager/Types.h"
#include "FileManager.h"
#include "DialogManager/Dialogs.h"
#include "EventManager/Events.h"

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
typedef OSType SFTypeList[4];
typedef const OSType *ConstSFTypeListPtr;

/* Classic reply structure */
typedef struct SFReply {
    Boolean     good;       /* True if user clicked OK */
    Boolean     copy;       /* Not used */
    OSType      fType;      /* File type */
    short       vRefNum;    /* Volume reference number */
    short       version;    /* Not used */
    Str63       fName;      /* Filename */
} SFReply;

/* System 7 reply structure */
typedef struct StandardFileReply {
    Boolean     sfGood;         /* True if user clicked OK */
    Boolean     sfReplacing;    /* True if replacing existing file */
    OSType      sfType;         /* File type */
    FSSpec      sfFile;         /* File specification */
    ScriptCode  sfScript;       /* Script system */
    short       sfFlags;        /* Finder flags */
    Boolean     sfIsFolder;     /* True if selection is folder */
    Boolean     sfIsVolume;     /* True if selection is volume */
    long        sfReserved1;    /* Reserved */
    short       sfReserved2;    /* Reserved */
} StandardFileReply;

/* Callback types */
typedef pascal short (*DlgHookProcPtr)(short item, DialogPtr theDialog);
typedef pascal Boolean (*FileFilterProcPtr)(CInfoPBPtr pb);
typedef pascal Boolean (*ModalFilterProcPtr)(DialogPtr theDialog, EventRecord *theEvent,
                                            short *itemHit);

/* System 7 callbacks with user data */
typedef pascal short (*DlgHookYDProcPtr)(short item, DialogPtr theDialog, void *yourDataPtr);
typedef pascal Boolean (*FileFilterYDProcPtr)(CInfoPBPtr pb, void *yourDataPtr);
typedef pascal Boolean (*ModalFilterYDProcPtr)(DialogPtr theDialog, EventRecord *theEvent,
                                              short *itemHit, void *yourDataPtr);
typedef pascal void (*ActivateYDProcPtr)(DialogPtr theDialog, short itemNo,
                                        Boolean activating, void *yourDataPtr);

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
               SFTypeList typeList,
               DlgHookProcPtr dlgHook,
               SFReply *reply);

void SFPGetFile(Point where,
                ConstStr255Param prompt,
                FileFilterProcPtr fileFilter,
                short numTypes,
                SFTypeList typeList,
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