/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * Notepad.h - Note Pad Desk Accessory Header
 * Multi-page text editor with 8 pages
 */

#ifndef NOTEPAD_H
#define NOTEPAD_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowTypes.h"
#include "TextEdit/TextEdit.h"
#include "EventManager/EventTypes.h"
#include "FileMgr/file_manager.h"

/* Constants */
#define NOTEPAD_MAX_PAGES       8
#define NOTEPAD_FILE_TYPE       'TEXT'
#define NOTEPAD_CREATOR         'npad'
#define NOTEPAD_SIGNATURE       'NPAD'
#define NOTEPAD_FILE_NAME       "Note Pad File"

/* Menu Item IDs */
#define MENU_UNDO               1
#define MENU_CUT                3
#define MENU_COPY               4
#define MENU_PASTE              5
#define MENU_CLEAR              6

/* Notepad File Header Structure */
typedef struct NotePadFileHeader {
    UInt32 signature;
    UInt16 version;
    UInt16 pageCount;
    UInt16 currentPage;
} NotePadFileHeader;

/* Notepad Global Data Structure */
typedef struct NotePadGlobals {
    WindowPtr window;
    TEHandle teRecord;
    Handle pageData[NOTEPAD_MAX_PAGES];
    short currentPage;
    short totalPages;
    Boolean isDirty;
    short fileRefNum;
    SInt16 systemFolderVRefNum;
    SInt32 systemFolderDirID;
} NotePadGlobals;

/* File Header Structure */

/* Function Prototypes */

/* Initialization and Shutdown */
OSErr Notepad_Initialize(void);
void Notepad_Shutdown(void);

/* Window Management */
OSErr Notepad_Open(WindowPtr *window);
void Notepad_Close(void);
void Notepad_Draw(void);

/* Event Handling */
void Notepad_HandleEvent(EventRecord *event);

/* Page Navigation */
void Notepad_NextPage(void);
void Notepad_PreviousPage(void);
void Notepad_GotoPage(short pageNum);

/* Status Functions */
WindowPtr Notepad_GetWindow(void);
Boolean Notepad_IsDirty(void);
short Notepad_GetCurrentPage(void);

#endif /* NOTEPAD_H */