/* #include "SuperCompat.h" */
#include <stdlib.h>
#include <string.h>
/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/* #include "CompatibilityFix.h" */
#include "SystemTypes.h"
#include "System71StdLib.h"

#define SF_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleStandardFile, kLogLevelDebug, "[SF] " fmt, ##__VA_ARGS__)

/*
 * StandardFile.c - Standard File Package Implementation
 * Provides standard file open/save dialogs for System 7.1
 */

#include "StandardFile/StandardFile.h"
#include "StandardFile/StandardFileHAL.h"
#include "DialogManager/DialogManager.h"
#include "WindowManager/WindowManager.h"
#include "ControlManager/ControlManager.h"
#include "FileMgr/file_manager.h"
#include "MemoryMgr/MemoryManager.h"
#include "ToolboxCompat.h"
/* #include "ListManager/ListManager.h" */

/* External File Manager functions not in file_manager.h */
extern OSErr HGetFInfo(SInt16 vRefNum, SInt32 dirID, ConstStr255Param fileName, void* fInfo);
extern OSErr DirCreate(SInt16 vRefNum, SInt32 parentDirID, ConstStr255Param dirName, SInt32* createdDirID);


/* Dialog state */
typedef struct {
    DialogPtr dialog;
    ListHandle fileList;
    ControlHandle scrollBar;
    TEHandle nameField;

    /* Current directory */
    short vRefNum;
    long dirID;

    /* File list */
    short numFiles;
    FSSpec *files;

    /* User callbacks */
    FileFilterYDProcPtr fileFilter;
    DlgHookYDProcPtr dlgHook;
    ModalFilterYDProcPtr modalFilter;
    void *userData;

    /* Reply structure */
    StandardFileReply *reply;

    /* Dialog type */
    Boolean isOpen;  /* true for Open, false for Save */

    /* Filter */
    short numTypes;
    OSType *typeList;
} SFDialogState;

/* Global dialog state */
static SFDialogState gSFState = {0};

/* Forward declarations */
static void SF_DisposeDialog(void);
static void SF_PopulateFileList(void);
static void SF_SelectFile(short index);
static void SF_NavigateToFolder(FSSpec *folder);
static OSErr SF_GetCurrentLocation(short *vRefNum, long *dirID);

/* Keyboard focus helpers */
static void SF_PrimeInitialFocus(DialogPtr d)
{
    ControlHandle defBtn = NULL;
    ControlHandle c;

    if (!d) return;

    c = (ControlHandle)((DialogPeek)d)->items;
    /* prefer default button if present */
    while (c) {
        if (IsButtonControl(c) && IsDefaultButton(c)) {
            defBtn = c;
            break;
        }
        c = (*c)->nextControl;
    }
    if (defBtn) {
        DM_SetKeyboardFocus((WindowPtr)d, defBtn);
    } else {
        DM_FocusNextControl((WindowPtr)d, false);
    }
}

static SInt16 __attribute__((unused)) SF_ItemFromControl(ControlHandle c)
{
    /* refCon is item number */
    return (c && *c) ? (SInt16)(*c)->contrlRfCon : 0;
}

/*
 * Classic SFPutFile - Save file dialog
 */
void SFPutFile(Point where,
               ConstStr255Param prompt,
               ConstStr255Param origName,
               DlgHookProcPtr dlgHook,
               SFReply *reply) {

    StandardFileReply sfReply;

    /* Convert to modern call */
    StandardPutFile(prompt, origName, &sfReply);

    /* Convert reply */
    reply->good = sfReply.sfGood;
    reply->copy = 0;
    reply->fType = sfReply.sfType;
    reply->vRefNum = sfReply.sfFile.vRefNum;
    reply->version = 0;
    BlockMove(sfReply.sfFile.name, reply->fName, sfReply.sfFile.name[0] + 1);
}

/*
 * Classic SFPPutFile - Save file dialog with custom dialog
 */
void SFPPutFile(Point where,
                ConstStr255Param prompt,
                ConstStr255Param origName,
                DlgHookProcPtr dlgHook,
                SFReply *reply,
                short dlgID,
                ModalFilterProcPtr filterProc) {

    /* For now, just use standard dialog */
    SFPutFile(where, prompt, origName, dlgHook, reply);
}

/*
 * Classic SFGetFile - Open file dialog
 */
void SFGetFile(Point where,
               ConstStr255Param prompt,
               FileFilterProcPtr fileFilter,
               short numTypes,
               SFTypeList typeList,
               DlgHookProcPtr dlgHook,
               SFReply *reply) {

    StandardFileReply sfReply;

    /* Convert to modern call */
    StandardGetFile(fileFilter, numTypes, typeList, &sfReply);

    /* Convert reply */
    reply->good = sfReply.sfGood;
    reply->copy = 0;
    reply->fType = sfReply.sfType;
    reply->vRefNum = sfReply.sfFile.vRefNum;
    reply->version = 0;
    BlockMove(sfReply.sfFile.name, reply->fName, sfReply.sfFile.name[0] + 1);
}

/*
 * Classic SFPGetFile - Open file dialog with custom dialog
 */
void SFPGetFile(Point where,
                ConstStr255Param prompt,
                FileFilterProcPtr fileFilter,
                short numTypes,
                SFTypeList typeList,
                DlgHookProcPtr dlgHook,
                SFReply *reply,
                short dlgID,
                ModalFilterProcPtr filterProc) {

    /* For now, just use standard dialog */
    SFGetFile(where, prompt, fileFilter, numTypes, typeList, dlgHook, reply);
}

/*
 * Modern StandardPutFile - System 7 save dialog
 */
void StandardPutFile(ConstStr255Param prompt,
                    ConstStr255Param defaultName,
                    StandardFileReply *reply) {

    CustomPutFile(prompt, defaultName, reply, 0, (Point){-1, -1}, NULL, NULL, NULL, NULL);
}

/*
 * Modern StandardGetFile - System 7 open dialog
 */
void StandardGetFile(FileFilterProcPtr fileFilter,
                    short numTypes,
                    ConstSFTypeListPtr typeList,
                    StandardFileReply *reply) {

    /* Cast via void* to avoid -Wcast-function-type between different filter signatures */
    CustomGetFile((FileFilterYDProcPtr)(void*)fileFilter, numTypes, typeList, reply, 0, (Point){-1, -1},
                 NULL, NULL, NULL, NULL, NULL);
}

/*
 * CustomPutFile - Extended save dialog
 */
void CustomPutFile(ConstStr255Param prompt,
                  ConstStr255Param defaultName,
                  StandardFileReply *reply,
                  short dlgID,
                  Point where,
                  DlgHookYDProcPtr dlgHook,
                  ModalFilterYDProcPtr modalFilter,
                  ActivateYDProcPtr activeList,
                  void *yourDataPtr) {

    OSErr err;
    Boolean done = false;
    short itemHit;
    EventRecord event;

    /* Initialize reply */
    memset(reply, 0, sizeof(StandardFileReply));
    reply->sfGood = false;

    /* Initialize dialog state */
    memset(&gSFState, 0, sizeof(SFDialogState));
    gSFState.reply = reply;
    gSFState.isOpen = false;
    gSFState.dlgHook = dlgHook;
    gSFState.modalFilter = modalFilter;
    gSFState.userData = yourDataPtr;

    /* Initialize HAL */
    StandardFile_HAL_Init();

    /* Create dialog through HAL */
    err = StandardFile_HAL_CreateSaveDialog(&gSFState.dialog, prompt, defaultName);
    if (err != noErr) {
        return;
    }

    /* Get current directory */
    SF_GetCurrentLocation(&gSFState.vRefNum, &gSFState.dirID);

    /* Populate file list */
    SF_PopulateFileList();

    /* Call dialog hook for initialization */
    if (dlgHook) {
        dlgHook(sfHookFirstCall, gSFState.dialog, yourDataPtr);
    }

    /* Set initial keyboard focus */
    SF_PrimeInitialFocus(gSFState.dialog);

    /* Dialog loop */
    while (!done) {
        /* Check for keyboard events first */
        if (WaitNextEvent(everyEvent, &event, 60, NULL)) {
            if (event.what == keyDown || event.what == autoKey) {
                if (DM_HandleDialogKey((WindowPtr)gSFState.dialog, &event, &itemHit)) {
                    /* keyboard activated a control */
                    done = (itemHit == sfItemOpenButton || itemHit == sfItemCancelButton);
                    if (!done) {
                        /* Let switch handle non-terminal items */
                        goto handle_item;
                    }
                    continue;
                }
            }
        }
        /* Use HAL for event handling */
        StandardFile_HAL_RunDialog(gSFState.dialog, &itemHit);

handle_item:
        if (itemHit > 0) {
            SF_LOG_DEBUG("itemHit=%d\n", itemHit);
        }
        switch (itemHit) {
            case sfItemOpenButton:  /* Save button */
                {
                    /* Get filename from dialog */
                    Str255 fileName;
                    StandardFile_HAL_GetSaveFileName(gSFState.dialog, fileName);

                    if (fileName[0] > 0) {
                        /* Build FSSpec for file */
                        reply->sfFile.vRefNum = gSFState.vRefNum;
                        reply->sfFile.parID = gSFState.dirID;
                        BlockMove(fileName, reply->sfFile.name, fileName[0] + 1);

                        /* Check if file exists */
                        FInfo fndrInfo;
                        err = HGetFInfo(reply->sfFile.vRefNum, reply->sfFile.parID,
                                      reply->sfFile.name, &fndrInfo);

                        if (err == noErr) {
                            /* File exists - ask to replace */
                            reply->sfReplacing = StandardFile_HAL_ConfirmReplace(fileName);
                            if (reply->sfReplacing) {
                                reply->sfGood = true;
                                reply->sfType = fndrInfo.fdType;
                                done = true;
                            }
                        } else {
                            /* New file */
                            reply->sfGood = true;
                            reply->sfReplacing = false;
                            reply->sfType = 'TEXT';  /* Default type */
                            done = true;
                        }
                    }
                }
                break;

            case sfItemCancelButton:
                reply->sfGood = false;
                done = true;
                break;

            case sfItemEjectButton:
                /* Eject current volume */
                StandardFile_HAL_EjectVolume(gSFState.vRefNum);
                SF_NavigateToFolder(NULL);
                break;

            case sfItemDesktopButton:
                /* Go to desktop */
                StandardFile_HAL_NavigateToDesktop(&gSFState.vRefNum, &gSFState.dirID);
                SF_PopulateFileList();
                break;

            case sfItemPopUpMenuUser:
                /* Handle directory popup */
                {
                    long selectedDir;
                    if (StandardFile_HAL_HandleDirPopup(gSFState.dialog, &selectedDir)) {
                        gSFState.dirID = selectedDir;
                        SF_PopulateFileList();
                    }
                }
                break;

            case sfItemNewFolderUser:
                /* Create new folder */
                {
                    Str255 folderName;
                    if (StandardFile_HAL_GetNewFolderName(folderName)) {
                        long newDirID;
                        err = DirCreate(gSFState.vRefNum, gSFState.dirID,
                                      folderName, (SInt32 *)&newDirID);
                        if (err == noErr) {
                            /* Navigate into newly created folder */
                            gSFState.dirID = newDirID;
                            SF_PopulateFileList();
                        }
                    }
                }
                break;

            default:
                /* Handle file list selection */
                if (itemHit >= 100 && itemHit < 100 + gSFState.numFiles) {
                    SF_SelectFile(itemHit - 100);
                }
                break;
        }

        /* Call dialog hook */
        if (dlgHook && !done) {
            short hookResult = dlgHook(itemHit, gSFState.dialog, yourDataPtr);
            if (hookResult == sfHookLastCall) {
                done = true;
            }
        }
    }

    /* Call dialog hook for cleanup */
    if (dlgHook) {
        dlgHook(sfHookLastCall, gSFState.dialog, yourDataPtr);
    }

    /* Clean up */
    StandardFile_HAL_DisposeSaveDialog(gSFState.dialog);
    SF_DisposeDialog();
}

/*
 * CustomGetFile - Extended open dialog
 */
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
                  ConstStr255Param prompt) {

    OSErr err;
    Boolean done = false;
    short itemHit;
    EventRecord event;

    /* Initialize reply */
    memset(reply, 0, sizeof(StandardFileReply));
    reply->sfGood = false;

    /* Initialize dialog state */
    memset(&gSFState, 0, sizeof(SFDialogState));
    gSFState.reply = reply;
    gSFState.isOpen = true;
    gSFState.fileFilter = fileFilter;
    gSFState.dlgHook = dlgHook;
    gSFState.modalFilter = modalFilter;
    gSFState.userData = yourDataPtr;
    gSFState.numTypes = numTypes;

    /* Copy type list */
    if (numTypes > 0 && typeList != NULL) {
        /* Check for integer overflow in allocation size */
        if (numTypes > SIZE_MAX / sizeof(OSType)) {
            return;
        }
        gSFState.typeList = (OSType*)NewPtr(numTypes * sizeof(OSType));
        if (gSFState.typeList) {
            memcpy(gSFState.typeList, typeList, numTypes * sizeof(OSType));
        } else {
            /* Allocation failed - reset numTypes to prevent NULL dereference later */
            gSFState.numTypes = 0;
        }
    }

    /* Initialize HAL */
    StandardFile_HAL_Init();

    /* Create dialog through HAL */
    err = StandardFile_HAL_CreateOpenDialog(&gSFState.dialog, prompt);
    if (err != noErr) {
        if (gSFState.typeList) {
            DisposePtr((Ptr)gSFState.typeList);
            gSFState.typeList = NULL;
            gSFState.numTypes = 0;
        }
        return;
    }

    /* Get current directory */
    SF_GetCurrentLocation(&gSFState.vRefNum, &gSFState.dirID);

    /* Populate file list */
    SF_PopulateFileList();

    /* Call dialog hook for initialization */
    if (dlgHook) {
        dlgHook(sfHookFirstCall, gSFState.dialog, yourDataPtr);
    }

    /* Set initial keyboard focus */
    SF_PrimeInitialFocus(gSFState.dialog);

    /* Dialog loop */
    while (!done) {
        /* Check for keyboard events first */
        if (WaitNextEvent(everyEvent, &event, 60, NULL)) {
            if (event.what == keyDown || event.what == autoKey) {
                if (DM_HandleDialogKey((WindowPtr)gSFState.dialog, &event, &itemHit)) {
                    /* keyboard activated a control */
                    done = (itemHit == sfItemOpenButton || itemHit == sfItemCancelButton);
                    if (!done) {
                        /* Let switch handle non-terminal items */
                        goto handle_item2;
                    }
                    continue;
                }
            }
        }
        /* Use HAL for event handling */
        StandardFile_HAL_RunDialog(gSFState.dialog, &itemHit);

handle_item2:
        if (itemHit > 0) {
            SF_LOG_DEBUG("itemHit=%d\n", itemHit);
        }
        switch (itemHit) {
            case sfItemOpenButton:
                {
                    /* Get selected file */
                    short selectedIndex = StandardFile_HAL_GetSelectedFile(gSFState.dialog);

                    if (selectedIndex >= 0 && selectedIndex < gSFState.numFiles) {
                        FSSpec *selected = &gSFState.files[selectedIndex];

                        /* Check if it's a folder */
                        CInfoPBRec cpb;
                        memset(&cpb, 0, sizeof(cpb));
                        cpb.ioNamePtr = selected->name;
                        cpb.ioVRefNum = selected->vRefNum;
                        cpb.u.hFileInfo.ioDirID = selected->parID;

                        err = PBGetCatInfoSync(&cpb);
                        if (err == noErr) {
                            if (cpb.u.hFileInfo.ioFlAttrib & 0x10) {
                                /* It's a folder - navigate into it */
                                gSFState.dirID = cpb.u.dirInfo.ioDrDirID;
                                SF_PopulateFileList();
                            } else {
                                /* It's a file - return it */
                                reply->sfFile = *selected;
                                reply->sfGood = true;
                                reply->sfType = cpb.u.hFileInfo.ioFlFndrInfo.fdType;
                                reply->sfIsFolder = false;
                                reply->sfIsVolume = false;
                                done = true;
                            }
                        }
                    }
                }
                break;

            case sfItemCancelButton:
                reply->sfGood = false;
                done = true;
                break;

            case sfItemEjectButton:
                StandardFile_HAL_EjectVolume(gSFState.vRefNum);
                SF_NavigateToFolder(NULL);
                break;

            case sfItemDesktopButton:
                StandardFile_HAL_NavigateToDesktop(&gSFState.vRefNum, &gSFState.dirID);
                SF_PopulateFileList();
                break;

            case sfItemPopUpMenuUser:
                {
                    long selectedDir;
                    if (StandardFile_HAL_HandleDirPopup(gSFState.dialog, &selectedDir)) {
                        gSFState.dirID = selectedDir;
                        SF_PopulateFileList();
                    }
                }
                break;

            default:
                /* Handle file list double-click */
                if (itemHit >= 200 && itemHit < 200 + gSFState.numFiles) {
                    short index = itemHit - 200;
                    FSSpec *selected = &gSFState.files[index];

                    /* Check if folder */
                    CInfoPBRec cpb;
                    memset(&cpb, 0, sizeof(cpb));
                    cpb.ioNamePtr = selected->name;
                    cpb.ioVRefNum = selected->vRefNum;
                    cpb.u.hFileInfo.ioDirID = selected->parID;

                    err = PBGetCatInfoSync(&cpb);
                    if (err == noErr && (cpb.u.hFileInfo.ioFlAttrib & 0x10)) {
                        /* Navigate into folder */
                        gSFState.dirID = cpb.u.dirInfo.ioDrDirID;
                        SF_PopulateFileList();
                    }
                } else if (itemHit >= 100 && itemHit < 100 + gSFState.numFiles) {
                    /* Single click selection */
                    SF_SelectFile(itemHit - 100);
                }
                break;
        }

        /* Call dialog hook */
        if (dlgHook && !done) {
            short hookResult = dlgHook(itemHit, gSFState.dialog, yourDataPtr);
            if (hookResult == sfHookLastCall) {
                done = true;
            }
        }
    }

    /* Call dialog hook for cleanup */
    if (dlgHook) {
        dlgHook(sfHookLastCall, gSFState.dialog, yourDataPtr);
    }

    /* Clean up */
    StandardFile_HAL_DisposeOpenDialog(gSFState.dialog);
    SF_DisposeDialog();

    if (gSFState.typeList) {
        DisposePtr((Ptr)gSFState.typeList);
        gSFState.typeList = NULL;
    }
}

/*
 * Populate file list
 */
static void SF_PopulateFileList(void) {
    OSErr err;
    CInfoPBRec cpb;
    short index = 1;
    FSSpec tempSpec;

    /* Free existing file list */
    if (gSFState.files) {
        DisposePtr((Ptr)gSFState.files);
        gSFState.files = NULL;
    }
    gSFState.numFiles = 0;

    /* Allocate temporary storage */
    FSSpec *tempFiles = (FSSpec*)NewPtr(sizeof(FSSpec) * 256);
    if (!tempFiles) return;

    /* Clear file list in dialog */
    StandardFile_HAL_ClearFileList(gSFState.dialog);

    /* Enumerate files in current directory */
    while (true) {
        memset(&cpb, 0, sizeof(cpb));
        cpb.ioNamePtr = tempSpec.name;
        cpb.ioVRefNum = gSFState.vRefNum;
        cpb.u.hFileInfo.ioFDirIndex = index;
        cpb.u.hFileInfo.ioDirID = gSFState.dirID;

        err = PBGetCatInfoSync(&cpb);
        if (err != noErr) break;

        /* Build FSSpec */
        tempSpec.vRefNum = gSFState.vRefNum;
        tempSpec.parID = gSFState.dirID;

        /* Check if we should include this file */
        Boolean include = false;

        if (cpb.u.hFileInfo.ioFlAttrib & 0x10) {
            /* It's a folder - always include in open dialog */
            if (gSFState.isOpen) {
                include = true;
            }
        } else {
            /* It's a file */
            if (gSFState.fileFilter) {
                /* Use custom filter - returns non-zero to exclude */
                include = (gSFState.fileFilter((StringPtr)&cpb) == 0);
            } else if (gSFState.numTypes > 0) {
                /* Check against type list */
                OSType fileType = cpb.u.hFileInfo.ioFlFndrInfo.fdType;
                for (short i = 0; i < gSFState.numTypes; i++) {
                    if (fileType == gSFState.typeList[i]) {
                        include = true;
                        break;
                    }
                }
            } else {
                /* No filtering - include all */
                include = true;
            }
        }

        if (include && gSFState.numFiles < 256) {
            /* Add to list */
            tempFiles[gSFState.numFiles] = tempSpec;
            gSFState.numFiles++;

            /* Add to dialog list */
            OSType fileType = (cpb.u.hFileInfo.ioFlAttrib & 0x10) ? 'fold' : cpb.u.hFileInfo.ioFlFndrInfo.fdType;
            StandardFile_HAL_AddFileToList(gSFState.dialog, &tempSpec, fileType);
        }

        index++;
    }

    /* Copy final list */
    if (gSFState.numFiles > 0) {
        /* Check for integer overflow in allocation size */
        if (gSFState.numFiles > SIZE_MAX / sizeof(FSSpec)) {
            DisposePtr((Ptr)tempFiles);
            return;
        }
        gSFState.files = (FSSpec*)NewPtr(sizeof(FSSpec) * gSFState.numFiles);
        if (gSFState.files) {
            memcpy(gSFState.files, tempFiles, sizeof(FSSpec) * gSFState.numFiles);
        }
    }

    DisposePtr((Ptr)tempFiles);

    /* Update dialog display */
    StandardFile_HAL_UpdateFileList(gSFState.dialog);
}

/*
 * Select file in list
 */
static void SF_SelectFile(short index) {
    if (index >= 0 && index < gSFState.numFiles) {
        StandardFile_HAL_SelectFile(gSFState.dialog, index);

        if (!gSFState.isOpen) {
            /* For save dialog, put filename in text field */
            StandardFile_HAL_SetSaveFileName(gSFState.dialog,
                                            gSFState.files[index].name);
        }
    }
}

/*
 * Get current location
 */
static OSErr SF_GetCurrentLocation(short *vRefNum, long *dirID) {
    /* Use HAL to get default directory */
    return StandardFile_HAL_GetDefaultLocation(vRefNum, dirID);
}

/*
 * Navigate to a folder
 */
static void SF_NavigateToFolder(FSSpec *folder) {
    if (folder) {
        gSFState.vRefNum = folder->vRefNum;
        gSFState.dirID = folder->parID;
        SF_PopulateFileList();
    }
}

/*
 * Clean up dialog state
 */
static void SF_DisposeDialog(void) {
    if (gSFState.files) {
        DisposePtr((Ptr)gSFState.files);
        gSFState.files = NULL;
    }
    gSFState.numFiles = 0;
}
