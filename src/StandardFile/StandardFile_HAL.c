/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * StandardFile_HAL.c - Hardware Abstraction Layer for Standard File Package
 * Provides platform-specific file dialog implementations
 */

#include "StandardFile/StandardFile.h"
#include "StandardFile/StandardFile_HAL.h"
#include "DialogManager/DialogManager.h"
#include "WindowManager/WindowManager.h"
#include "ControlManager/ControlManager.h"
#include "ListManager/ListManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <AppKit/AppKit.h>
#else
#ifdef HAS_GTK
#include <gtk/gtk.h>
#endif
#endif

/* Dialog structure */
typedef struct {
    DialogPtr dialog;
    WindowPtr window;
    ListHandle fileList;
    ControlHandle scrollBar;
    TEHandle nameField;
    Str255 currentPath;
    Boolean isOpen;
    short selectedIndex;
} SFDialog;

/* Global state */
static struct {
    Boolean initialized;
    SFDialog *currentDialog;
    char homePath[1024];
    char desktopPath[1024];
} gSFHAL = {0};

/* Forward declarations */
static void StandardFile_HAL_InitPaths(void);
static OSErr StandardFile_HAL_CreateDialogWindow(SFDialog **dialog, Boolean isOpen);
static void StandardFile_HAL_DisposeDialogWindow(SFDialog *dialog);

/*
 * Initialize HAL
 */
void StandardFile_HAL_Init(void) {
    if (gSFHAL.initialized) return;

    StandardFile_HAL_InitPaths();
    gSFHAL.initialized = true;
}

/*
 * Initialize system paths
 */
static void StandardFile_HAL_InitPaths(void) {
#ifdef __APPLE__
    NSString *home = NSHomeDirectory();
    NSString *desktop = [home stringByAppendingPathComponent:@"Desktop"];

    strncpy(gSFHAL.homePath, [home UTF8String], sizeof(gSFHAL.homePath) - 1);
    strncpy(gSFHAL.desktopPath, [desktop UTF8String], sizeof(gSFHAL.desktopPath) - 1);
#else
    /* Get home directory */
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }

    if (home) {
        strncpy(gSFHAL.homePath, home, sizeof(gSFHAL.homePath) - 1);
        snprintf(gSFHAL.desktopPath, sizeof(gSFHAL.desktopPath),
                "%s/Desktop", home);
    } else {
        strcpy(gSFHAL.homePath, "/");
        strcpy(gSFHAL.desktopPath, "/");
    }
#endif
}

/*
 * Create save dialog
 */
OSErr StandardFile_HAL_CreateSaveDialog(DialogPtr *dialog,
                                       ConstStr255Param prompt,
                                       ConstStr255Param defaultName) {
#ifdef __APPLE__
    @autoreleasepool {
        /* Use native Cocoa save panel */
        NSSavePanel *savePanel = [NSSavePanel savePanel];

        if (prompt && prompt[0] > 0) {
            NSString *promptStr = [[NSString alloc] initWithBytes:&prompt[1]
                                                        length:prompt[0]
                                                      encoding:NSMacOSRomanStringEncoding];
            [savePanel setMessage:promptStr];
        }

        if (defaultName && defaultName[0] > 0) {
            NSString *nameStr = [[NSString alloc] initWithBytes:&defaultName[1]
                                                       length:defaultName[0]
                                                     encoding:NSMacOSRomanStringEncoding];
            [savePanel setNameFieldStringValue:nameStr];
        }

        /* Store reference for later use */
        *dialog = (DialogPtr)CFBridgingRetain(savePanel);
        return noErr;
    }
#else
    /* Create custom dialog window */
    SFDialog *sfDialog;
    OSErr err = StandardFile_HAL_CreateDialogWindow(&sfDialog, false);
    if (err != noErr) return err;

    /* Set prompt and default name */
    if (prompt) {
        /* Would set prompt text in dialog */
    }

    if (defaultName && sfDialog->nameField) {
        TESetText(defaultName + 1, defaultName[0], sfDialog->nameField);
    }

    *dialog = sfDialog->dialog;
    gSFHAL.currentDialog = sfDialog;
    return noErr;
#endif
}

/*
 * Create open dialog
 */
OSErr StandardFile_HAL_CreateOpenDialog(DialogPtr *dialog, ConstStr255Param prompt) {
#ifdef __APPLE__
    @autoreleasepool {
        /* Use native Cocoa open panel */
        NSOpenPanel *openPanel = [NSOpenPanel openPanel];

        if (prompt && prompt[0] > 0) {
            NSString *promptStr = [[NSString alloc] initWithBytes:&prompt[1]
                                                        length:prompt[0]
                                                      encoding:NSMacOSRomanStringEncoding];
            [openPanel setMessage:promptStr];
        }

        [openPanel setCanChooseFiles:YES];
        [openPanel setCanChooseDirectories:NO];
        [openPanel setAllowsMultipleSelection:NO];

        /* Store reference */
        *dialog = (DialogPtr)CFBridgingRetain(openPanel);
        return noErr;
    }
#else
    /* Create custom dialog window */
    SFDialog *sfDialog;
    OSErr err = StandardFile_HAL_CreateDialogWindow(&sfDialog, true);
    if (err != noErr) return err;

    *dialog = sfDialog->dialog;
    gSFHAL.currentDialog = sfDialog;
    return noErr;
#endif
}

/*
 * Create dialog window (non-Mac platforms)
 */
static OSErr StandardFile_HAL_CreateDialogWindow(SFDialog **dialog, Boolean isOpen) {
    SFDialog *sfDialog = (SFDialog*)calloc(1, sizeof(SFDialog));
    if (!sfDialog) return memFullErr;

    /* Create window */
    Rect bounds;
    SetRect(&bounds, 100, 100, 500, 400);

    Str255 title;
    if (isOpen) {
        strcpy((char*)title, "\pOpen File");
    } else {
        strcpy((char*)title, "\pSave File");
    }

    sfDialog->window = NewWindow(NULL, &bounds, title, true,
                                movableDBoxProc, (WindowPtr)-1L, true, 0);

    if (!sfDialog->window) {
        free(sfDialog);
        return memFullErr;
    }

    /* Create dialog record */
    sfDialog->dialog = (DialogPtr)sfDialog->window;
    sfDialog->isOpen = isOpen;

    /* Create file list */
    Rect listBounds;
    SetRect(&listBounds, 20, 50, 380, 250);

    Point cellSize = {0, 0};
    ListBounds dataBounds = {0, 0, 0, 1};

    sfDialog->fileList = LNew(&listBounds, &dataBounds, cellSize, 0,
                             sfDialog->window, false, false, false, true);

    /* Create scroll bar */
    Rect scrollBounds;
    SetRect(&scrollBounds, 380, 50, 396, 250);
    sfDialog->scrollBar = NewControl(sfDialog->window, &scrollBounds,
                                    "\p", true, 0, 0, 0,
                                    scrollBarProc, 0);

    /* Create text field for save dialog */
    if (!isOpen) {
        Rect textBounds;
        SetRect(&textBounds, 20, 270, 380, 290);
        sfDialog->nameField = TENew(&textBounds, &textBounds);
    }

    /* Store current directory */
    strcpy((char*)sfDialog->currentPath, gSFHAL.homePath);

    *dialog = sfDialog;
    return noErr;
}

/*
 * Dispose save dialog
 */
void StandardFile_HAL_DisposeSaveDialog(DialogPtr dialog) {
#ifdef __APPLE__
    if (dialog) {
        CFRelease((CFTypeRef)dialog);
    }
#else
    StandardFile_HAL_DisposeDialogWindow(gSFHAL.currentDialog);
    gSFHAL.currentDialog = NULL;
#endif
}

/*
 * Dispose open dialog
 */
void StandardFile_HAL_DisposeOpenDialog(DialogPtr dialog) {
    StandardFile_HAL_DisposeSaveDialog(dialog);
}

/*
 * Dispose dialog window
 */
static void StandardFile_HAL_DisposeDialogWindow(SFDialog *dialog) {
    if (!dialog) return;

    if (dialog->fileList) {
        LDispose(dialog->fileList);
    }

    if (dialog->scrollBar) {
        DisposeControl(dialog->scrollBar);
    }

    if (dialog->nameField) {
        TEDispose(dialog->nameField);
    }

    if (dialog->window) {
        DisposeWindow(dialog->window);
    }

    free(dialog);
}

/*
 * Run dialog event loop
 */
void StandardFile_HAL_RunDialog(DialogPtr dialog, short *itemHit) {
#ifdef __APPLE__
    @autoreleasepool {
        id panel = (__bridge id)dialog;

        if ([panel isKindOfClass:[NSSavePanel class]]) {
            NSSavePanel *savePanel = (NSSavePanel*)panel;
            NSModalResponse result = [savePanel runModal];

            if (result == NSModalResponseOK) {
                *itemHit = sfItemOpenButton;  /* Save button */
            } else {
                *itemHit = sfItemCancelButton;
            }
        } else if ([panel isKindOfClass:[NSOpenPanel class]]) {
            NSOpenPanel *openPanel = (NSOpenPanel*)panel;
            NSModalResponse result = [openPanel runModal];

            if (result == NSModalResponseOK) {
                *itemHit = sfItemOpenButton;
            } else {
                *itemHit = sfItemCancelButton;
            }
        }
    }
#else
    /* Simple modal dialog loop */
    EventRecord event;
    Boolean done = false;

    while (!done) {
        if (WaitNextEvent(everyEvent, &event, 60, NULL)) {
            switch (event.what) {
                case mouseDown:
                    {
                        Point pt = event.where;
                        GlobalToLocal(&pt);

                        /* Check for button clicks */
                        if (pt.v >= 350) {
                            if (pt.h < 200) {
                                *itemHit = sfItemCancelButton;
                                done = true;
                            } else {
                                *itemHit = sfItemOpenButton;
                                done = true;
                            }
                        }
                    }
                    break;

                case keyDown:
                    {
                        char key = event.message & charCodeMask;
                        if (key == '\r') {
                            *itemHit = sfItemOpenButton;
                            done = true;
                        } else if (key == 27) {  /* ESC */
                            *itemHit = sfItemCancelButton;
                            done = true;
                        }
                    }
                    break;
            }
        }
    }
#endif
}

/*
 * Get filename from save dialog
 */
void StandardFile_HAL_GetSaveFileName(DialogPtr dialog, Str255 fileName) {
#ifdef __APPLE__
    @autoreleasepool {
        NSSavePanel *savePanel = (__bridge NSSavePanel*)dialog;
        NSURL *url = [savePanel URL];

        if (url) {
            NSString *name = [url lastPathComponent];
            const char *cStr = [name UTF8String];
            size_t len = strlen(cStr);
            if (len > 255) len = 255;
            fileName[0] = len;
            memcpy(&fileName[1], cStr, len);
        } else {
            fileName[0] = 0;
        }
    }
#else
    if (gSFHAL.currentDialog && gSFHAL.currentDialog->nameField) {
        CharsHandle text = TEGetText(gSFHAL.currentDialog->nameField);
        short length = (*gSFHAL.currentDialog->nameField)->teLength;

        if (length > 255) length = 255;
        fileName[0] = length;
        BlockMove(*text, &fileName[1], length);
    } else {
        fileName[0] = 0;
    }
#endif
}

/*
 * Set filename in save dialog
 */
void StandardFile_HAL_SetSaveFileName(DialogPtr dialog, ConstStr255Param fileName) {
#ifdef __APPLE__
    @autoreleasepool {
        NSSavePanel *savePanel = (__bridge NSSavePanel*)dialog;
        NSString *nameStr = [[NSString alloc] initWithBytes:&fileName[1]
                                                   length:fileName[0]
                                                 encoding:NSMacOSRomanStringEncoding];
        [savePanel setNameFieldStringValue:nameStr];
    }
#else
    if (gSFHAL.currentDialog && gSFHAL.currentDialog->nameField) {
        TESetText(&fileName[1], fileName[0], gSFHAL.currentDialog->nameField);
    }
#endif
}

/*
 * Clear file list
 */
void StandardFile_HAL_ClearFileList(DialogPtr dialog) {
    if (gSFHAL.currentDialog && gSFHAL.currentDialog->fileList) {
        /* Clear all cells */
        ListHandle list = gSFHAL.currentDialog->fileList;
        while ((*list)->dataBounds.bottom > 0) {
            LDelRow(1, 0, list);
        }
    }
}

/*
 * Add file to list
 */
void StandardFile_HAL_AddFileToList(DialogPtr dialog, ConstStr255Param fileName,
                                   Boolean isFolder) {
    if (gSFHAL.currentDialog && gSFHAL.currentDialog->fileList) {
        ListHandle list = gSFHAL.currentDialog->fileList;
        short row = LAddRow(1, 32767, list);

        Point cell = {0, row};

        /* Add folder indicator */
        if (isFolder) {
            char text[256];
            text[0] = '▶';
            text[1] = ' ';
            memcpy(&text[2], &fileName[1], fileName[0]);
            LSetCell(text, fileName[0] + 2, cell, list);
        } else {
            LSetCell(&fileName[1], fileName[0], cell, list);
        }
    }
}

/*
 * Update file list display
 */
void StandardFile_HAL_UpdateFileList(DialogPtr dialog) {
    if (gSFHAL.currentDialog && gSFHAL.currentDialog->fileList) {
        LUpdate((**gSFHAL.currentDialog->fileList).visible,
               gSFHAL.currentDialog->fileList);
    }
}

/*
 * Get selected file index
 */
short StandardFile_HAL_GetSelectedFile(DialogPtr dialog) {
    if (gSFHAL.currentDialog && gSFHAL.currentDialog->fileList) {
        Point cell = {0, 0};
        if (LGetSelect(true, &cell, gSFHAL.currentDialog->fileList)) {
            return cell.v;
        }
    }
    return -1;
}

/*
 * Select file in list
 */
void StandardFile_HAL_SelectFile(DialogPtr dialog, short index) {
    if (gSFHAL.currentDialog && gSFHAL.currentDialog->fileList) {
        Point cell = {0, index};
        LSetSelect(true, cell, gSFHAL.currentDialog->fileList);
        gSFHAL.currentDialog->selectedIndex = index;
    }
}

/*
 * Confirm replace dialog
 */
Boolean StandardFile_HAL_ConfirmReplace(ConstStr255Param fileName) {
#ifdef __APPLE__
    @autoreleasepool {
        NSString *nameStr = [[NSString alloc] initWithBytes:&fileName[1]
                                                   length:fileName[0]
                                                 encoding:NSMacOSRomanStringEncoding];

        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithFormat:@"Replace "%@"?", nameStr]];
        [alert setInformativeText:@"A file with this name already exists."];
        [alert addButtonWithTitle:@"Replace"];
        [alert addButtonWithTitle:@"Cancel"];

        NSModalResponse response = [alert runModal];
        return (response == NSAlertFirstButtonReturn);
    }
#else
    /* Simple confirmation - for now always return true */
    return true;
#endif
}

/*
 * Get default location
 */
OSErr StandardFile_HAL_GetDefaultLocation(short *vRefNum, long *dirID) {
    /* Use home directory as default */
    *vRefNum = -1;  /* Default volume */
    *dirID = 2;     /* Root directory ID */
    return noErr;
}

/*
 * Navigate to desktop
 */
void StandardFile_HAL_NavigateToDesktop(short *vRefNum, long *dirID) {
    *vRefNum = -1;
    *dirID = 2;

    if (gSFHAL.currentDialog) {
        strcpy((char*)gSFHAL.currentDialog->currentPath, gSFHAL.desktopPath);
    }
}

/*
 * Handle directory popup menu
 */
Boolean StandardFile_HAL_HandleDirPopup(DialogPtr dialog, long *selectedDir) {
    /* Simplified - just return false for now */
    return false;
}

/*
 * Get new folder name
 */
Boolean StandardFile_HAL_GetNewFolderName(Str255 folderName) {
#ifdef __APPLE__
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"New Folder"];
        [alert setInformativeText:@"Enter a name for the new folder:"];
        [alert addButtonWithTitle:@"Create"];
        [alert addButtonWithTitle:@"Cancel"];

        NSTextField *input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
        [input setStringValue:@"untitled folder"];
        [alert setAccessoryView:input];

        NSModalResponse response = [alert runModal];

        if (response == NSAlertFirstButtonReturn) {
            NSString *name = [input stringValue];
            const char *cStr = [name UTF8String];
            size_t len = strlen(cStr);
            if (len > 255) len = 255;
            folderName[0] = len;
            memcpy(&folderName[1], cStr, len);
            return true;
        }
    }
#else
    /* Default name */
    strcpy((char*)folderName, "\pNew Folder");
    return true;
#endif
    return false;
}

/*
 * Eject volume
 */
void StandardFile_HAL_EjectVolume(short vRefNum) {
    /* Platform-specific volume ejection */
    /* On modern systems, this is largely a no-op */
}

/*
 * Get file info from path
 */
OSErr StandardFile_HAL_GetFileInfo(const char *path, Boolean *isFolder,
                                  OSType *fileType, OSType *creator) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return fnfErr;
    }

    *isFolder = S_ISDIR(st.st_mode);

    /* Default types */
    if (*isFolder) {
        *fileType = 'fold';
        *creator = 'MACS';
    } else {
        /* Determine file type by extension */
        const char *ext = strrchr(path, '.');
        if (ext) {
            if (strcmp(ext, ".txt") == 0) {
                *fileType = 'TEXT';
                *creator = 'ttxt';
            } else if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) {
                *fileType = 'TEXT';
                *creator = 'CWIE';
            } else if (strcmp(ext, ".pdf") == 0) {
                *fileType = 'PDF ';
                *creator = 'CARO';
            } else {
                *fileType = '????';
                *creator = '????';
            }
        } else {
            *fileType = '????';
            *creator = '????';
        }
    }

    return noErr;
}