#include "MemoryMgr/MemoryManager.h"
/*
 * StandardFileHAL_Shims.c - Hardware Abstraction Layer for Standard File Package
 *
 * Adapts StandardFile.c to the System 7.1 reimplementation's existing managers
 * (DialogManager, WindowManager, ControlManager, FileManager).
 */

#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "StandardFile/StandardFile.h"
#include "StandardFile/StandardFileHAL.h"
#include "DialogManager/DialogManager.h"
#include "WindowManager/WindowManager.h"
#include "ControlManager/ControlManager.h"
#include "EventManager/EventManager.h"
#include "QuickDraw/QuickDraw.h"
#include "FileMgr/file_manager.h"
#include "DeskManager/DeskManager.h"
#include "ListManager/ListManager.h"
#include "ToolboxCompat.h"

/* Debug logging */
#ifdef SF_HAL_DEBUG
#define SF_HAL_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleStandardFile, kLogLevelTrace, "[SF HAL] " fmt, ##__VA_ARGS__)
#else
#define SF_HAL_LOG_DEBUG(fmt, ...)
#endif

#define SF_HAL_LOG_INFO(fmt, ...)  serial_logf(kLogModuleStandardFile, kLogLevelInfo, "[SF HAL] " fmt, ##__VA_ARGS__)
#define SF_HAL_LOG_WARN(fmt, ...)  serial_logf(kLogModuleStandardFile, kLogLevelWarn, "[SF HAL] " fmt, ##__VA_ARGS__)

/* File list entry (for data storage) */
typedef struct {
    FSSpec spec;
    OSType fileType;
    Boolean isFolder;
} FileListEntry;

/* HAL state */
static Boolean gHALInitialized = false;

/* File list data (stored separately from visual list) */
static FileListEntry *gFileListArray = NULL;
static short gFileListCount = 0;
static short gFileListCapacity = 0;

/* Visual list control */
static ListHandle gFileListHandle = NULL;

/* Selection and navigation */
static short gSelectedIndex = -1;
static short gCurrentVRefNum = 0;
static long gCurrentDirID = 0;
static Boolean gNavigationRequested = false;

/* List dimensions */
#define LIST_LEFT 10
#define LIST_TOP 30
#define LIST_RIGHT 440
#define LIST_BOTTOM 280
#define INITIAL_FILE_LIST_CAPACITY 100

/* Forward declarations */
extern void SF_PopulateFileList(void);
static void StandardFile_HAL_NavigateToFolder(const FSSpec *folderSpec);
static OSErr StandardFile_HAL_CreateListControl(DialogPtr dialog, ListHandle *outList);

/*
 * StandardFile_HAL_CreateListControl - Create list control in dialog
 */
static OSErr StandardFile_HAL_CreateListControl(DialogPtr dialog, ListHandle *outList) {
    if (!dialog || !outList) {
        return paramErr;
    }

    Rect listBounds = {LIST_TOP, LIST_LEFT, LIST_BOTTOM, LIST_RIGHT};
    Rect cellSize = {0, 0, 16, LIST_RIGHT - LIST_LEFT};  /* Row height 16, full width */

    ListParams params = {
        .viewRect = listBounds,
        .cellSizeRect = cellSize,
        .window = (WindowPtr)dialog,
        .hasVScroll = true,
        .hasHScroll = false,
        .selMode = lsSingleSel,
        .refCon = 0
    };

    *outList = LNew(&params);
    if (*outList == NULL) {
        SF_HAL_LOG_WARN("StandardFile HAL: Failed to create list control\n");
        return memFullErr;
    }

    SF_HAL_LOG_DEBUG("StandardFile HAL: Created list control\n");
    return noErr;
}

/*
 * StandardFile_HAL_Init - Initialize HAL subsystem
 */
OSErr StandardFile_HAL_Init(void) {
    if (!gHALInitialized) {
        SF_HAL_LOG_DEBUG("StandardFile HAL: Initializing\n");

        /* Allocate file list array with overflow protection */
        gFileListCapacity = INITIAL_FILE_LIST_CAPACITY;

        /* Check for integer overflow before allocation */
        if (gFileListCapacity > 0 && sizeof(FileListEntry) > SIZE_MAX / gFileListCapacity) {
            return memFullErr;  /* Would overflow */
        }

        gFileListArray = (FileListEntry*)NewPtr(gFileListCapacity * sizeof(FileListEntry));
        if (!gFileListArray) {
            gFileListCapacity = 0;
            return memFullErr;  /* Allocation failed */
        }

        gFileListCount = 0;
        gSelectedIndex = -1;
        gFileListHandle = NULL;

        gHALInitialized = true;
    }
    return noErr;
}

/*
 * StandardFile_HAL_CreateOpenDialog - Create an open file dialog
 */
OSErr StandardFile_HAL_CreateOpenDialog(DialogPtr *outDialog, ConstStr255Param prompt) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: CreateOpenDialog prompt='%s'\n", prompt ? prompt : "(null)");

    /* Create a modal dialog with list area */
    Rect bounds = {100, 100, 400, 500};
    static unsigned char title[] = {9, 'O','p','e','n',' ','F','i','l','e'};
    *outDialog = NewDialog(NULL, &bounds, title, true, dBoxProc,
                           (WindowPtr)-1, false, 0, NULL);

    if (*outDialog == NULL) {
        return memFullErr;
    }

    /* Create list control in the dialog */
    OSErr err = StandardFile_HAL_CreateListControl(*outDialog, &gFileListHandle);
    if (err != noErr) {
        DisposeDialog(*outDialog);
        *outDialog = NULL;
        gFileListHandle = NULL;
        return err;
    }

    /* Clear file list data */
    gFileListCount = 0;
    gSelectedIndex = -1;

    return noErr;
}

/*
 * StandardFile_HAL_CreateSaveDialog - Create a save file dialog
 */
OSErr StandardFile_HAL_CreateSaveDialog(DialogPtr *outDialog, ConstStr255Param prompt,
                                        ConstStr255Param defaultName) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: CreateSaveDialog prompt='%s' default='%s'\n",
           prompt ? prompt : "(null)", defaultName ? defaultName : "(null)");

    /* Create a modal dialog with list area */
    Rect bounds = {100, 100, 400, 500};
    static unsigned char title[] = {9, 'S','a','v','e',' ','F','i','l','e'};
    *outDialog = NewDialog(NULL, &bounds, title, true, dBoxProc,
                           (WindowPtr)-1, false, 0, NULL);

    if (*outDialog == NULL) {
        return memFullErr;
    }

    /* Create list control in the dialog */
    OSErr err = StandardFile_HAL_CreateListControl(*outDialog, &gFileListHandle);
    if (err != noErr) {
        DisposeDialog(*outDialog);
        *outDialog = NULL;
        gFileListHandle = NULL;
        return err;
    }

    /* Clear file list data */
    gFileListCount = 0;
    gSelectedIndex = -1;

    return noErr;
}

/*
 * StandardFile_HAL_DisposeOpenDialog - Dispose of open dialog
 */
void StandardFile_HAL_DisposeOpenDialog(DialogPtr dialog) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: DisposeOpenDialog\n");

    /* Dispose list control */
    if (gFileListHandle) {
        LDispose(gFileListHandle);
        gFileListHandle = NULL;
    }

    if (dialog) {
        DisposeDialog(dialog);
    }
}

/*
 * StandardFile_HAL_DisposeSaveDialog - Dispose of save dialog
 */
void StandardFile_HAL_DisposeSaveDialog(DialogPtr dialog) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: DisposeSaveDialog\n");

    /* Dispose list control */
    if (gFileListHandle) {
        LDispose(gFileListHandle);
        gFileListHandle = NULL;
    }

    if (dialog) {
        DisposeDialog(dialog);
    }
}

/*
 * StandardFile_HAL_RunDialog - Run modal dialog and return item hit
 */
void StandardFile_HAL_RunDialog(DialogPtr dialog, short *itemHit) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: RunDialog - starting modal loop\n");

    if (!dialog || !itemHit || !gFileListHandle) {
        if (itemHit) *itemHit = sfItemCancelButton;
        return;
    }

    EventRecord event;
    Boolean done = false;
    DialogPtr whichDialog;
    short item;
    short listItem = 0;

    /* Show the dialog window */
    ShowWindow(dialog);

    /* Modal event loop */
    while (!done) {
        if (GetNextEvent(everyEvent, &event)) {
            if (IsDialogEvent(&event)) {
                if (DialogSelect(&event, &whichDialog, &item)) {
                    if (whichDialog == dialog) {
                        SF_HAL_LOG_DEBUG("StandardFile HAL: Dialog item hit: %d\n", item);

                        /* Handle button clicks */
                        if (item == sfItemOpenButton) {
                            /* Open/Save button */
                            if (gSelectedIndex >= 0 && gSelectedIndex < gFileListCount) {
                                /* Check if it's a folder */
                                if (gFileListArray[gSelectedIndex].isFolder) {
                                    /* Navigate into folder - don't exit, repopulate */
                                    SF_HAL_LOG_DEBUG("StandardFile HAL: Navigating into folder\n");
                                    StandardFile_HAL_NavigateToFolder(&gFileListArray[gSelectedIndex].spec);
                                    /* Signal StandardFile.c to repopulate */
                                    *itemHit = sfItemOpenButton;  /* Will be handled by StandardFile.c */
                                    done = true;  /* Exit loop so StandardFile can handle navigation */
                                } else {
                                    /* File selected - exit with Open */
                                    *itemHit = sfItemOpenButton;
                                    done = true;
                                }
                            } else {
                                /* No valid selection - just log for now */
                                SF_HAL_LOG_DEBUG("StandardFile HAL: Open clicked but no valid selection\n");
                            }
                        } else if (item == sfItemCancelButton) {
                            /* Cancel button */
                            *itemHit = sfItemCancelButton;
                            done = true;
                        }
                        /* Handle other items like Eject, Desktop, etc. */
                    }
                }
            } else {
                /* Handle list control interactions */
                if (gFileListHandle && event.what == mouseDown) {
                    Point localPt = event.where;
                    WindowPtr eventWindow = NULL;
                    if (FindWindow(event.where, &eventWindow) == inContent) {
                        if (eventWindow == (WindowPtr)dialog) {
                            GlobalToLocal(&localPt);

                            /* Try to handle list click */
                            if (LClick(gFileListHandle, localPt, event.modifiers, &listItem)) {
                                gSelectedIndex = listItem - 1;  /* Convert to 0-indexed */
                                SF_HAL_LOG_DEBUG("StandardFile HAL: List item clicked: %d (selected %d)\n",
                                       listItem, gSelectedIndex);

                                /* Check for double-click */
                                if (LClick(gFileListHandle, localPt, event.modifiers, &listItem)) {
                                    /* Double-click detected */
                                    if (gSelectedIndex >= 0 && gSelectedIndex < gFileListCount) {
                                        if (gFileListArray[gSelectedIndex].isFolder) {
                                            /* Navigate into folder */
                                            SF_HAL_LOG_DEBUG("StandardFile HAL: Double-click navigating into folder\n");
                                            StandardFile_HAL_NavigateToFolder(&gFileListArray[gSelectedIndex].spec);
                                            *itemHit = sfItemOpenButton;
                                            done = true;
                                        } else {
                                            /* Double-click on file = Open */
                                            *itemHit = sfItemOpenButton;
                                            done = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                /* Handle other event types (update, activate, etc.) */
                switch (event.what) {
                    case updateEvt: {
                        /* Redraw list when window is updated */
                        if ((WindowPtr)dialog == (WindowPtr)(uintptr_t)event.message) {
                            if (gFileListHandle) {
                                LDraw(gFileListHandle);
                            }
                        }
                        break;
                    }

                    case keyDown:
                    case autoKey: {
                        char key = event.message & charCodeMask;
                        if (event.modifiers & cmdKey) {
                            /* Command-key shortcuts */
                            if (key == '.') {
                                /* Cmd-Period = Cancel */
                                *itemHit = sfItemCancelButton;
                                done = true;
                            }
                        } else {
                            /* Regular keys - forward to list */
                            switch (key) {
                                case 0x1E:  /* Up arrow key */
                                    /* Move selection up */
                                    if (gSelectedIndex > 0) {
                                        gSelectedIndex--;
                                        Cell cell = {0, gSelectedIndex};
                                        LSetSelect(gFileListHandle, true, cell);
                                        LDraw(gFileListHandle);
                                    }
                                    break;

                                case 0x1F:  /* Down arrow key */
                                    /* Move selection down */
                                    if (gFileListCount > 0 && gSelectedIndex < gFileListCount - 1) {
                                        gSelectedIndex++;
                                        Cell cell = {0, gSelectedIndex};
                                        LSetSelect(gFileListHandle, true, cell);
                                        LDraw(gFileListHandle);
                                    }
                                    break;

                                case '\r':
                                case 0x03:
                                    /* Return/Enter = Open */
                                    if (gSelectedIndex >= 0 && gSelectedIndex < gFileListCount) {
                                        if (gFileListArray[gSelectedIndex].isFolder) {
                                            StandardFile_HAL_NavigateToFolder(&gFileListArray[gSelectedIndex].spec);
                                            *itemHit = sfItemOpenButton;
                                        } else {
                                            *itemHit = sfItemOpenButton;
                                        }
                                        done = true;
                                    }
                                    break;

                                case 0x1B:
                                    /* Escape = Cancel */
                                    *itemHit = sfItemCancelButton;
                                    done = true;
                                    break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        /* Yield to system */
        SystemTask();
    }

    SF_HAL_LOG_DEBUG("StandardFile HAL: RunDialog exiting with item %d\n", *itemHit);
}

/*
 * StandardFile_HAL_ClearFileList - Clear the file list
 */
void StandardFile_HAL_ClearFileList(DialogPtr dialog) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: ClearFileList\n");

    /* Clear list rows from ListHandle */
    if (gFileListHandle) {
        short rows = (*gFileListHandle)->dataBounds.bottom - (*gFileListHandle)->dataBounds.top;
        if (rows > 0) {
            LDelRow(gFileListHandle, rows, 0);
        }
    }

    /* Clear data */
    gFileListCount = 0;
    gSelectedIndex = -1;
}

/*
 * StandardFile_HAL_AddFileToList - Add a file to the list
 */
void StandardFile_HAL_AddFileToList(DialogPtr dialog, const FSSpec *spec, OSType fileType) {
    if (!spec) return;

    /* Check if we need to expand the array */
    if (gFileListCount >= gFileListCapacity) {
        /* Check for integer overflow in doubling capacity */
        if (gFileListCapacity > SIZE_MAX / (2 * sizeof(FileListEntry))) {
            gFileListCapacity = 0;
            gFileListCount = 0;
            return;
        }

        Size oldSize = gFileListCapacity * sizeof(FileListEntry);
        gFileListCapacity *= 2;
        FileListEntry* newArray = (FileListEntry*)NewPtr(gFileListCapacity * sizeof(FileListEntry));
        if (!newArray) {
            gFileListCapacity = 0;
            gFileListCount = 0;
            return;
        }
        if (gFileListArray) {
            BlockMove(gFileListArray, newArray, oldSize);
            DisposePtr((Ptr)gFileListArray);
        }
        gFileListArray = newArray;
    }

    /* Determine if this is a folder by checking with File Manager */
    CInfoPBRec pb;
    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = (StringPtr)(uintptr_t)spec->name;
    pb.ioVRefNum = spec->vRefNum;
    pb.u.dirInfo.ioDrDirID = spec->parID;
    pb.u.hFileInfo.ioFDirIndex = 0;

    Boolean isFolder = false;
    if (PBGetCatInfoSync(&pb) == noErr) {
        isFolder = (pb.u.hFileInfo.ioFlAttrib & 0x10) != 0;  /* 0x10 = directory bit */
    }

    /* Add to data array */
    gFileListArray[gFileListCount].spec = *spec;
    gFileListArray[gFileListCount].fileType = fileType;
    gFileListArray[gFileListCount].isFolder = isFolder;

    SF_HAL_LOG_DEBUG("StandardFile HAL: AddFileToList [%d] name='%.*s' type='%.4s' isFolder=%d\n",
           gFileListCount, spec->name[0], spec->name + 1,
           (char*)&fileType, isFolder);

    /* Add row to list display */
    if (gFileListHandle) {
        /* Add a new row at the end of the list */
        LAddRow(gFileListHandle, 1, gFileListCount);

        /* Build display string: "[folder] Name" or "Name (TYPE)" */
        Str255 displayStr;
        if (isFolder) {
            /* Check for overflow: folder marker + name must fit in Str255 */
            UInt8 nameLen = spec->name[0];
            if (nameLen > 254) {
                nameLen = 254;  /* Truncate to fit with folder marker */
            }
            displayStr[0] = nameLen + 1;  /* +1 for folder indicator */
            displayStr[1] = '*';  /* Folder marker */
            BlockMove(&spec->name[1], &displayStr[2], nameLen);
        } else {
            BlockMove(spec->name, displayStr, spec->name[0] + 1);
        }

        /* Set cell content */
        Cell cell = {0, gFileListCount};
        LSetCell(gFileListHandle, &displayStr[1], displayStr[0], cell);
    }

    gFileListCount++;
}

/*
 * StandardFile_HAL_UpdateFileList - Refresh the file list display
 */
void StandardFile_HAL_UpdateFileList(DialogPtr dialog) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: UpdateFileList count=%d\n", gFileListCount);

    /* Redraw the list control */
    if (gFileListHandle) {
        LDraw(gFileListHandle);
    }
}

/*
 * StandardFile_HAL_SelectFile - Select a file in the list
 */
void StandardFile_HAL_SelectFile(DialogPtr dialog, short index) {
    if (index >= 0 && index < gFileListCount) {
        gSelectedIndex = index;

        /* Update list selection */
        if (gFileListHandle) {
            Cell cell = {0, index};
            LSetSelect(gFileListHandle, true, cell);
        }

        SF_HAL_LOG_DEBUG("StandardFile HAL: SelectFile index=%d name='%.*s'\n",
               index, gFileListArray[index].spec.name[0], gFileListArray[index].spec.name + 1);
    } else {
        gSelectedIndex = -1;

        /* Clear selection */
        if (gFileListHandle) {
            Cell cell = {0, 0};
            LSetSelect(gFileListHandle, false, cell);
        }

        SF_HAL_LOG_DEBUG("StandardFile HAL: SelectFile index=%d (invalid, cleared)\n", index);
    }
}

/*
 * StandardFile_HAL_GetSelectedFile - Get the selected file index
 */
short StandardFile_HAL_GetSelectedFile(DialogPtr dialog) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: GetSelectedFile returning %d\n", gSelectedIndex);
    return gSelectedIndex;
}

/*
 * Get selected file spec
 */
const FSSpec* StandardFile_HAL_GetSelectedFileSpec(void) {
    if (gSelectedIndex >= 0 && gSelectedIndex < gFileListCount) {
        return &gFileListArray[gSelectedIndex].spec;
    }
    return NULL;
}

/*
 * StandardFile_HAL_SetSaveFileName - Set the save file name field
 */
void StandardFile_HAL_SetSaveFileName(DialogPtr dialog, ConstStr255Param name) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: SetSaveFileName name='%s'\n", name ? name : "(null)");
    /* Stub: would set TEHandle text in dialog */
}

/*
 * StandardFile_HAL_GetSaveFileName - Get the save file name from field
 */
void StandardFile_HAL_GetSaveFileName(DialogPtr dialog, Str255 name) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: GetSaveFileName\n");
    /* Return selected file name or default */
    if (name) {
        if (gSelectedIndex >= 0 && gSelectedIndex < gFileListCount) {
            BlockMove(gFileListArray[gSelectedIndex].spec.name, name,
                     gFileListArray[gSelectedIndex].spec.name[0] + 1);
        } else {
            const char *defaultName = "Untitled";
            int len = strlen(defaultName);
            name[0] = len;
            memcpy(name + 1, defaultName, len);
        }
    }
}

/*
 * StandardFile_HAL_ConfirmReplace - Ask user to confirm file replacement
 */
Boolean StandardFile_HAL_ConfirmReplace(ConstStr255Param fileName) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: ConfirmReplace file='%s'\n", fileName ? fileName : "(null)");
    /* Stub: auto-confirm for now */
    return true;
}

/*
 * StandardFile_HAL_GetDefaultLocation - Get default save/open location
 */
OSErr StandardFile_HAL_GetDefaultLocation(short *vRefNum, long *dirID) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: GetDefaultLocation\n");
    /* Stub: return root directory */
    if (vRefNum) *vRefNum = 0;
    if (dirID) *dirID = 2;  /* Root directory */
    return noErr;
}

/*
 * StandardFile_HAL_EjectVolume - Eject the current volume
 */
OSErr StandardFile_HAL_EjectVolume(short vRefNum) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: EjectVolume vRefNum=%d\n", vRefNum);
    /* Stub: not implemented */
    return noErr;
}

/*
 * StandardFile_HAL_NavigateToDesktop - Navigate to Desktop folder
 */
OSErr StandardFile_HAL_NavigateToDesktop(short *vRefNum, long *dirID) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: NavigateToDesktop\n");
    /* Stub: return desktop location */
    if (vRefNum) *vRefNum = 0;
    if (dirID) *dirID = 2;
    return noErr;
}

/* StandardFile_HAL_HandleDirPopup is now defined below after NavigateToFolder */

/*
 * StandardFile_HAL_GetNewFolderName - Prompt for new folder name
 */
Boolean StandardFile_HAL_GetNewFolderName(Str255 folderName) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: GetNewFolderName\n");
    /* Stub: return default folder name */
    if (folderName) {
        const char *defaultName = "New Folder";
        int len = strlen(defaultName);
        folderName[0] = len;
        memcpy(folderName + 1, defaultName, len);
    }
    return true;
}

/* NOTE: File Manager functions (PBGetCatInfoSync, HGetFInfo, DirCreate)
 * are provided by FileManager.c, not the HAL */

/*
 * StandardFile_HAL_NavigateToFolder - Navigate to a folder
 * Called internally when user double-clicks a folder
 */
static void StandardFile_HAL_NavigateToFolder(const FSSpec *folderSpec) {
    if (!folderSpec) return;

    /* Get the dirID of this folder */
    CInfoPBRec pb;
    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = (StringPtr)(uintptr_t)folderSpec->name;
    pb.ioVRefNum = folderSpec->vRefNum;
    pb.u.dirInfo.ioDrDirID = folderSpec->parID;
    pb.u.hFileInfo.ioFDirIndex = 0;

    if (PBGetCatInfoSync(&pb) == noErr) {
        if (pb.u.hFileInfo.ioFlAttrib & 0x10) {  /* It's a folder */
            gCurrentVRefNum = folderSpec->vRefNum;
            gCurrentDirID = pb.u.dirInfo.ioDrDirID;
            gNavigationRequested = true;
            SF_HAL_LOG_DEBUG("StandardFile HAL: Navigated to folder dirID=%ld\n", gCurrentDirID);
        }
    }
}

/*
 * StandardFile_HAL_HandleDirPopup - Handle directory popup menu
 * For now, implements simple Desktop and Parent navigation
 */
Boolean StandardFile_HAL_HandleDirPopup(DialogPtr dialog, long *selectedDir) {
    SF_HAL_LOG_DEBUG("StandardFile HAL: HandleDirPopup\n");

    /* Simple implementation: navigate to parent directory */
    if (gCurrentDirID != 2) {  /* 2 = root directory */
        CInfoPBRec pb;
        Str255 dirName;

        memset(&pb, 0, sizeof(pb));
        dirName[0] = 0;  /* Empty name to get parent info */
        pb.ioNamePtr = dirName;
        pb.ioVRefNum = gCurrentVRefNum;
        pb.u.dirInfo.ioDrDirID = gCurrentDirID;
        pb.u.hFileInfo.ioFDirIndex = -1;  /* -1 = get info about directory itself */

        if (PBGetCatInfoSync(&pb) == noErr) {
            /* Navigate to parent */
            gCurrentDirID = pb.u.dirInfo.ioDrParID;
            *selectedDir = gCurrentDirID;
            gNavigationRequested = true;
            SF_HAL_LOG_DEBUG("StandardFile HAL: Navigated to parent dirID=%ld\n", gCurrentDirID);
            return true;
        }
    }

    return false;
}
