/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * StandardFile_HAL.h - Hardware Abstraction Layer for Standard File Package
 * Platform-specific file dialog implementation
 */

#ifndef STANDARDFILE_HAL_H
#define STANDARDFILE_HAL_H

#include "DeskManager/Types.h"
#include "FileManager.h"
#include "StandardFile/StandardFile.h"

/* Platform detection */
#if defined(__APPLE__) && defined(__MACH__)
    #define STANDARDFILE_USE_COCOA 1
#elif defined(__linux__)
    #define STANDARDFILE_USE_GTK 0  /* GTK support not yet implemented */
    #define STANDARDFILE_USE_CUSTOM 1
#elif defined(_WIN32)
    #define STANDARDFILE_USE_WIN32 0  /* Win32 support not yet implemented */
    #define STANDARDFILE_USE_CUSTOM 1
#else
    #define STANDARDFILE_USE_CUSTOM 1
#endif

/* Dialog types */
typedef enum {
    kStandardFileOpen,
    kStandardFileSave,
    kStandardFileCustomOpen,
    kStandardFileCustomSave
} StandardFileDialogType;

/* HAL Dialog structure */
typedef struct StandardFileHALDialog {
    StandardFileDialogType type;
    ConstStr255Param prompt;
    ConstStr255Param defaultName;
    FileFilterProcPtr fileFilter;
    FileFilterYDProcPtr fileFilterYD;
    short numTypes;
    ConstSFTypeListPtr typeList;
    DialogPtr dialog;
    ListHandle fileList;
    TEHandle nameEdit;
    ControlHandle scrollBar;
    ControlHandle volumePopup;
    short currentVRefNum;
    long currentDirID;
    Boolean isNavigating;
    void *platformData;  /* Platform-specific data */
    void *userData;      /* User callback data */
} StandardFileHALDialog;

/* Platform-specific initialization */
OSErr StandardFile_HAL_Init(void);
void StandardFile_HAL_Cleanup(void);

/* Dialog creation and management */
OSErr StandardFile_HAL_CreateDialog(StandardFileHALDialog *halDialog);
void StandardFile_HAL_DestroyDialog(StandardFileHALDialog *halDialog);

/* Dialog execution */
OSErr StandardFile_HAL_RunModal(StandardFileHALDialog *halDialog,
                                StandardFileReply *reply);

/* File list management */
OSErr StandardFile_HAL_BuildFileList(StandardFileHALDialog *halDialog);
OSErr StandardFile_HAL_NavigateToDirectory(StandardFileHALDialog *halDialog,
                                          short vRefNum, long dirID);
OSErr StandardFile_HAL_SelectFile(StandardFileHALDialog *halDialog,
                                 short index);

/* Volume management */
OSErr StandardFile_HAL_GetVolumeList(StandardFileHALDialog *halDialog,
                                    Handle *volumeList);
OSErr StandardFile_HAL_MountVolume(short vRefNum);
OSErr StandardFile_HAL_EjectVolume(short vRefNum);

/* Platform dialog integration */
#ifdef STANDARDFILE_USE_COCOA
OSErr StandardFile_HAL_ShowCocoaDialog(StandardFileHALDialog *halDialog,
                                      StandardFileReply *reply);
#endif

#ifdef STANDARDFILE_USE_GTK
OSErr StandardFile_HAL_ShowGTKDialog(StandardFileHALDialog *halDialog,
                                    StandardFileReply *reply);
#endif

#ifdef STANDARDFILE_USE_WIN32
OSErr StandardFile_HAL_ShowWin32Dialog(StandardFileHALDialog *halDialog,
                                      StandardFileReply *reply);
#endif

#ifdef STANDARDFILE_USE_CUSTOM
OSErr StandardFile_HAL_ShowCustomDialog(StandardFileHALDialog *halDialog,
                                       StandardFileReply *reply);
#endif

/* File filtering */
Boolean StandardFile_HAL_FilterFile(StandardFileHALDialog *halDialog,
                                   CInfoPBPtr pb);

/* Dialog customization */
OSErr StandardFile_HAL_CustomizeDialog(StandardFileHALDialog *halDialog,
                                      short dlgID,
                                      DlgHookYDProcPtr hookProc);

/* Event handling */
Boolean StandardFile_HAL_HandleEvent(StandardFileHALDialog *halDialog,
                                    EventRecord *event,
                                    short *itemHit);

/* Utilities */
OSErr StandardFile_HAL_GetFileInfo(FSSpec *file, CInfoPBRec *info);
OSErr StandardFile_HAL_CheckFileExists(FSSpec *file, Boolean *exists);
OSErr StandardFile_HAL_CreateNewFolder(StandardFileHALDialog *halDialog,
                                      ConstStr255Param folderName);

/* Desktop navigation */
OSErr StandardFile_HAL_GoToDesktop(StandardFileHALDialog *halDialog);
OSErr StandardFile_HAL_GoToDocuments(StandardFileHALDialog *halDialog);
OSErr StandardFile_HAL_GoToApplications(StandardFileHALDialog *halDialog);

/* Recent files support */
OSErr StandardFile_HAL_GetRecentFiles(Handle *recentList);
OSErr StandardFile_HAL_AddToRecentFiles(FSSpec *file);

/* Platform path conversion */
OSErr StandardFile_HAL_FSSpecToPath(FSSpec *spec, char *path, size_t maxPath);
OSErr StandardFile_HAL_PathToFSSpec(const char *path, FSSpec *spec);

#endif /* STANDARDFILE_HAL_H */