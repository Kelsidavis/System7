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

#include <Types.h>
#include <Quickdraw.h>
#include <Windows.h>
#include <TextEdit.h>
#include <Events.h>
#include <Files.h>
#include <Memory.h>

/* Constants */
#define NOTEPAD_MAX_PAGES       8
#define NOTEPAD_FILE_TYPE       'TEXT'
#define NOTEPAD_CREATOR         'npad'
#define NOTEPAD_SIGNATURE       'NPAD'
#define NOTEPAD_FILE_NAME       "\pNote Pad File"

/* Menu Item IDs */
#define MENU_UNDO               1
#define MENU_CUT                3
#define MENU_COPY               4
#define MENU_PASTE              5
#define MENU_CLEAR              6

/* Notepad Global Data Structure */
typedef struct {
    WindowPtr       window;                         /* Notepad window */
    short           currentPage;                    /* Current page (0-7) */
    short           totalPages;                     /* Total pages (always 8) */
    Handle          pageData[NOTEPAD_MAX_PAGES];    /* Text data for each page */
    TEHandle        teRecord;                       /* TextEdit record */
    short           fileRefNum;                     /* File reference number */
    Boolean         isDirty;                        /* Needs saving flag */
    short           systemFolderVRefNum;            /* System folder volume */
    long            systemFolderDirID;              /* System folder directory */
} NotePadGlobals;

/* File Header Structure */
typedef struct {
    OSType          signature;      /* File signature 'NPAD' */
    short           version;        /* File format version */
    short           pageCount;      /* Number of pages */
    short           currentPage;    /* Last displayed page */
    char            reserved[6];    /* Reserved for future use */
} NotePadFileHeader;

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