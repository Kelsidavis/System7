/* #include "SystemTypes.h" */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * Notepad.c - Note Pad Desk Accessory Implementation
 * A simple multi-page text editor with 8 pages of notes
 * Based on the classic Mac OS System 7.1 Note Pad
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "DeskManager/Notepad.h"
#include "DeskManager/DeskManager.h"
#include "TextEdit/TextEdit.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "EventManager/EventManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "SoundManager/SoundManager.h"

/* External QuickDraw globals */
extern QDGlobals qd;

/* Global Notepad state */
static NotePadGlobals *gNotepad = NULL;

/* Forward declarations */
static OSErr Notepad_LoadFile(NotePadGlobals *notepad);
static OSErr Notepad_SaveFile(NotePadGlobals *notepad);
static void Notepad_SaveCurrentPage(NotePadGlobals *notepad);
static void Notepad_LoadPage(NotePadGlobals *notepad, short pageNum);
static void Notepad_UpdatePageDisplay(NotePadGlobals *notepad);
static void Notepad_DrawPageIndicator(NotePadGlobals *notepad);
static void Notepad_HandleEdit(NotePadGlobals *notepad, short item);

/*
 * Initialize Notepad
 */
OSErr Notepad_Initialize(void) {
    if (gNotepad != NULL) {
        return noErr; /* Already initialized */
    }

    /* Allocate globals */
    gNotepad = (NotePadGlobals*)NewPtrClear(sizeof(NotePadGlobals));
    if (gNotepad == NULL) {
        return memFullErr;
    }

    /* Initialize structure */
    gNotepad->currentPage = 0;
    gNotepad->totalPages = NOTEPAD_MAX_PAGES;
    gNotepad->isDirty = false;
    gNotepad->fileRefNum = 0;

    /* Allocate page data handles */
    for (short i = 0; i < NOTEPAD_MAX_PAGES; i++) {
        gNotepad->pageData[i] = NewHandle(0);
        if (gNotepad->pageData[i] == NULL) {
            /* Clean up on failure */
            for (short j = 0; j < i; j++) {
                DisposeHandle(gNotepad->pageData[j]);
            }
            DisposePtr((Ptr)gNotepad);
            gNotepad = NULL;
            return memFullErr;
        }
    }

    /* Try to load existing file */
    OSErr err = Notepad_LoadFile(gNotepad);
    if (err != noErr) {
        /* Initialize empty pages */
        for (short i = 0; i < NOTEPAD_MAX_PAGES; i++) {
            SetHandleSize(gNotepad->pageData[i], 1);
            if (MemError() == noErr) {
                **gNotepad->pageData[i] = '\0';
            }
        }
    }

    return noErr;
}

/*
 * Shutdown Notepad
 */
void Notepad_Shutdown(void) {
    if (gNotepad == NULL) {
        return;
    }

    /* Save current page */
    Notepad_SaveCurrentPage(gNotepad);

    /* Save file if dirty */
    if (gNotepad->isDirty) {
        Notepad_SaveFile(gNotepad);
    }

    /* Dispose TextEdit record */
    if (gNotepad->teRecord != NULL) {
        TEDispose(gNotepad->teRecord);
    }

    /* Dispose page data */
    for (short i = 0; i < NOTEPAD_MAX_PAGES; i++) {
        if (gNotepad->pageData[i] != NULL) {
            DisposeHandle(gNotepad->pageData[i]);
        }
    }

    /* Close window */
    if (gNotepad->window != NULL) {
        DisposeWindow(gNotepad->window);
    }

    /* Free globals */
    DisposePtr((Ptr)gNotepad);
    gNotepad = NULL;
}

/*
 * Open Notepad window
 */
OSErr Notepad_Open(WindowPtr *window) {
    OSErr err;
    Rect windowBounds, teRect;

    if (gNotepad == NULL) {
        err = Notepad_Initialize();
        if (err != noErr) {
            return err;
        }
    }

    /* If window already exists, just show it */
    if (gNotepad->window != NULL) {
        SelectWindow(gNotepad->window);
        ShowWindow(gNotepad->window);
        *window = gNotepad->window;
        return noErr;
    }

    /* Create window */
    SetRect(&windowBounds, 100, 50, 500, 350);
    static unsigned char notepadTitle[] = {8, 'N', 'o', 't', 'e', 'P', 'a', 'd', '\0'};
    gNotepad->window = NewWindow(NULL, &windowBounds, (ConstStr255Param)notepadTitle,
                                true, documentProc, (WindowPtr)-1L,
                                true, 0);

    if (gNotepad->window == NULL) {
        return memFullErr;
    }

    SetPort((GrafPtr)gNotepad->window);

    /* Create TextEdit record */
    SetRect(&teRect, 10, 40, 390, 270);
    gNotepad->teRecord = TENew(&teRect, &teRect);

    if (gNotepad->teRecord == NULL) {
        DisposeWindow(gNotepad->window);
        gNotepad->window = NULL;
        return memFullErr;
    }

    /* Set TextEdit attributes */
    TEAutoView(true, gNotepad->teRecord);
    TEActivate(gNotepad->teRecord);

    /* Load first page */
    Notepad_LoadPage(gNotepad, 0);

    /* Update display and show window */
    Notepad_UpdatePageDisplay(gNotepad);
    ShowWindow(gNotepad->window);

    /* Draw initial content */
    Notepad_Draw();

    *window = gNotepad->window;
    return noErr;
}

/*
 * Close Notepad window
 */
void Notepad_Close(void) {
    if (gNotepad == NULL || gNotepad->window == NULL) {
        return;
    }

    /* Save current page */
    Notepad_SaveCurrentPage(gNotepad);

    /* Hide window instead of destroying (desk accessory behavior) */
    HideWindow(gNotepad->window);

    /* Save file if dirty */
    if (gNotepad->isDirty) {
        Notepad_SaveFile(gNotepad);
    }
}

/*
 * Handle window events
 */
void Notepad_HandleEvent(EventRecord *event) {
    WindowPtr window;
    short part;

    if (gNotepad == NULL || gNotepad->window == NULL) {
        return;
    }

    switch (event->what) {
        case mouseDown:
            part = FindWindow(event->where, &window);
            if (window == gNotepad->window) {
                switch (part) {
                    case inContent:
                    {
                        /* Check for page navigation clicks */
                        Point localPt = event->where;
                        GlobalToLocal(&localPt);

                        /* Check if clicking on page navigation area */
                        if (localPt.v < 35) {
                            /* Page indicator area - check for prev/next */
                            if (localPt.h < 50 && gNotepad->currentPage > 0) {
                                Notepad_PreviousPage();
                            } else if (localPt.h > 350 && gNotepad->currentPage < NOTEPAD_MAX_PAGES - 1) {
                                Notepad_NextPage();
                            }
                        } else {
                            /* Click in text area */
                            TEClick(localPt, (event->modifiers & shiftKey) != 0,
                                   gNotepad->teRecord);
                        }
                        break;
                    }

                    case inDrag:
                        DragWindow(window, event->where, &qd.screenBits.bounds);
                        break;

                    case inGoAway:
                        if (TrackGoAway(window, event->where)) {
                            Notepad_Close();
                        }
                        break;
                }
            }
            break;

        case keyDown:
        case autoKey:
            {
                char key = event->message & charCodeMask;

                /* Check for command key shortcuts */
                if (event->modifiers & cmdKey) {
                    switch (key) {
                        case 'x':
                        case 'X':
                            Notepad_HandleEdit(gNotepad, MENU_CUT);
                            break;
                        case 'c':
                        case 'C':
                            Notepad_HandleEdit(gNotepad, MENU_COPY);
                            break;
                        case 'v':
                        case 'V':
                            Notepad_HandleEdit(gNotepad, MENU_PASTE);
                            break;
                        case 'z':
                        case 'Z':
                            Notepad_HandleEdit(gNotepad, MENU_UNDO);
                            break;
                    }
                } else if (key == '\t') {
                    /* Tab key - next page */
                    Notepad_NextPage();
                } else {
                    /* Regular key - pass to TextEdit */
                    TEKey(key, gNotepad->teRecord);
                    gNotepad->isDirty = true;

                    /* Redraw the text immediately */
                    if (gNotepad->window && gNotepad->teRecord) {
                        SetPort((GrafPtr)gNotepad->window);
                        TEUpdate(&(**gNotepad->teRecord).viewRect, gNotepad->teRecord);
                    }
                }
            }
            break;

        case updateEvt:
            if ((WindowPtr)(uintptr_t)event->message == gNotepad->window) {
                BeginUpdate(gNotepad->window);
                Notepad_Draw();
                EndUpdate(gNotepad->window);
            }
            break;

        case activateEvt:
            if ((WindowPtr)(uintptr_t)event->message == gNotepad->window) {
                if (event->modifiers & activeFlag) {
                    TEActivate(gNotepad->teRecord);
                } else {
                    TEDeactivate(gNotepad->teRecord);
                }
            }
            break;
    }
}

/*
 * Draw Notepad window contents
 */
void Notepad_Draw(void) {
    if (gNotepad == NULL || gNotepad->window == NULL) {
        return;
    }

    SetPort((GrafPtr)gNotepad->window);  /* Safe now: BeginUpdate swapped portBits to GWorld */

    /* Clear window */
    EraseRect(&gNotepad->window->port.portRect);

    /* Draw page indicator */
    Notepad_DrawPageIndicator(gNotepad);

    /* Draw text */
    if (gNotepad->teRecord != NULL) {
        TEUpdate(&gNotepad->window->port.portRect, gNotepad->teRecord);
    }

    /* Draw frame around text area */
    Rect textFrame;
    SetRect(&textFrame, 8, 38, 392, 272);
    FrameRect(&textFrame);
}

/*
 * Go to next page
 */
void Notepad_NextPage(void) {
    if (gNotepad == NULL) return;

    if (gNotepad->currentPage < NOTEPAD_MAX_PAGES - 1) {
        Notepad_SaveCurrentPage(gNotepad);
        gNotepad->currentPage++;
        Notepad_LoadPage(gNotepad, gNotepad->currentPage);
        Notepad_UpdatePageDisplay(gNotepad);
    }
}

/*
 * Go to previous page
 */
void Notepad_PreviousPage(void) {
    if (gNotepad == NULL) return;

    if (gNotepad->currentPage > 0) {
        Notepad_SaveCurrentPage(gNotepad);
        gNotepad->currentPage--;
        Notepad_LoadPage(gNotepad, gNotepad->currentPage);
        Notepad_UpdatePageDisplay(gNotepad);
    }
}

/*
 * Go to specific page
 */
void Notepad_GotoPage(short pageNum) {
    if (gNotepad == NULL) return;

    if (pageNum >= 0 && pageNum < NOTEPAD_MAX_PAGES) {
        if (pageNum != gNotepad->currentPage) {
            Notepad_SaveCurrentPage(gNotepad);
            gNotepad->currentPage = pageNum;
            Notepad_LoadPage(gNotepad, pageNum);
            Notepad_UpdatePageDisplay(gNotepad);
        }
    }
}

/*
 * Save current page to memory
 */
static void Notepad_SaveCurrentPage(NotePadGlobals *notepad) {
    if (notepad == NULL || notepad->teRecord == NULL) {
        return;
    }

    /* Get text from TextEdit */
    Handle textHandle = TEGetText(notepad->teRecord);
    long textLength = (*notepad->teRecord)->teLength;

    /* Resize page data handle */
    SetHandleSize(notepad->pageData[notepad->currentPage], textLength + 1);

    if (MemError() == noErr) {
        /* Copy text to page data */
        HLock(textHandle);
        HLock(notepad->pageData[notepad->currentPage]);

        BlockMoveData(*textHandle, *notepad->pageData[notepad->currentPage], textLength);
        (*notepad->pageData[notepad->currentPage])[textLength] = '\0';

        HUnlock(notepad->pageData[notepad->currentPage]);
        HUnlock(textHandle);
    }
}

/*
 * Load page into TextEdit
 */
static void Notepad_LoadPage(NotePadGlobals *notepad, short pageNum) {
    if (notepad == NULL || notepad->teRecord == NULL) {
        return;
    }

    if (pageNum < 0 || pageNum >= NOTEPAD_MAX_PAGES) {
        return;
    }

    /* Clear current text */
    TESetSelect(0, 32767, notepad->teRecord);
    TEDelete(notepad->teRecord);

    /* Load new page text */
    long textLength = GetHandleSize(notepad->pageData[pageNum]);
    if (textLength > 0) {
        HLock(notepad->pageData[pageNum]);
        TEInsert(*notepad->pageData[pageNum], textLength - 1, notepad->teRecord);
        HUnlock(notepad->pageData[pageNum]);
    }

    /* Reset selection */
    TESetSelect(0, 0, notepad->teRecord);
}

/*
 * Update page display
 */
static void Notepad_UpdatePageDisplay(NotePadGlobals *notepad) {
    if (notepad == NULL || notepad->window == NULL) {
        return;
    }

    SetPort((GrafPtr)notepad->window);

    /* Redraw page indicator */
    Notepad_DrawPageIndicator(notepad);

    /* Update text display */
    if (notepad->teRecord) {
        InvalRect(&(**notepad->teRecord).viewRect);
    }
}

/*
 * Draw page indicator
 */
static void Notepad_DrawPageIndicator(NotePadGlobals *notepad) {
    Rect indicatorRect;
    unsigned char pageText[32];
    int len;

    SetPort((GrafPtr)notepad->window);

    /* Clear indicator area */
    SetRect(&indicatorRect, 0, 0, 400, 35);
    EraseRect(&indicatorRect);

    /* Draw separator line */
    MoveTo(0, 35);
    LineTo(400, 35);

    /* Draw page number as Pascal string */
    len = snprintf((char*)pageText + 1, sizeof(pageText) - 1, "Page %d of %d", notepad->currentPage + 1, NOTEPAD_MAX_PAGES);
    if (len >= sizeof(pageText) - 1) len = sizeof(pageText) - 2;
    pageText[0] = len;  /* Pascal string length prefix */
    MoveTo(170, 20);
    DrawString((ConstStr255Param)pageText);

    /* Draw navigation arrows if applicable */
    if (notepad->currentPage > 0) {
        /* Previous arrow */
        MoveTo(20, 20);
        DrawString("<");  /* Simple < instead of unicode arrow */
    }

    if (notepad->currentPage < NOTEPAD_MAX_PAGES - 1) {
        /* Next arrow */
        MoveTo(360, 20);
        DrawString(">");  /* Simple > instead of unicode arrow */
    }
}

/*
 * Handle edit menu commands
 */
static void Notepad_HandleEdit(NotePadGlobals *notepad, short item) {
    if (notepad == NULL || notepad->teRecord == NULL) {
        return;
    }

    switch (item) {
        case MENU_UNDO:
            /* TextEdit doesn't have built-in undo */
            SysBeep(1);
            break;

        case MENU_CUT:
            TECut(notepad->teRecord);
            notepad->isDirty = true;
            break;

        case MENU_COPY:
            TECopy(notepad->teRecord);
            break;

        case MENU_PASTE:
            TEPaste(notepad->teRecord);
            notepad->isDirty = true;
            break;

        case MENU_CLEAR:
            TEDelete(notepad->teRecord);
            notepad->isDirty = true;
            break;
    }
}

/*
 * Load Notepad file
 */
static OSErr Notepad_LoadFile(NotePadGlobals *notepad) {
    OSErr err;
    short refNum;
    long fileSize;
    NotePadFileHeader header;
    FSSpec fileSpec;

    /* Build file spec for Note Pad File in System Folder */
    err = FSMakeFSSpec(notepad->systemFolderVRefNum,
                      notepad->systemFolderDirID,
                      NOTEPAD_FILE_NAME,
                      &fileSpec);

    if (err != noErr) {
        return err;
    }

    /* Open file */
    err = FSpOpenDF(&fileSpec, fsRdPerm, &refNum);
    if (err != noErr) {
        return err;
    }

    /* Read header */
    fileSize = sizeof(NotePadFileHeader);
    err = FSRead(refNum, &fileSize, &header);
    if (err != noErr || header.signature != NOTEPAD_SIGNATURE) {
        FSClose(refNum);
        return err != noErr ? err : -1;
    }

    /* Read each page */
    for (short i = 0; i < NOTEPAD_MAX_PAGES; i++) {
        long pageSize;

        /* Read page size */
        fileSize = sizeof(long);
        err = FSRead(refNum, &fileSize, &pageSize);
        if (err != noErr) {
            break;
        }

        /* Allocate and read page data */
        if (pageSize > 0) {
            long originalSize = pageSize;
            SetHandleSize(notepad->pageData[i], pageSize);
            if (MemError() == noErr) {
                HLock(notepad->pageData[i]);
                err = FSRead(refNum, &pageSize, *notepad->pageData[i]);
                HUnlock(notepad->pageData[i]);
                /* If read failed or short read, clear page data */
                if (err != noErr || pageSize != originalSize) {
                    SetHandleSize(notepad->pageData[i], 1);
                    *(*notepad->pageData[i]) = 0;
                }
            }
        }
    }

    FSClose(refNum);
    notepad->currentPage = header.currentPage;
    notepad->isDirty = false;

    return noErr;
}

/*
 * Save Notepad file
 */
static OSErr Notepad_SaveFile(NotePadGlobals *notepad) {
    OSErr err;
    short refNum;
    long count;
    NotePadFileHeader header;
    FSSpec fileSpec;

    /* Save current page first */
    Notepad_SaveCurrentPage(notepad);

    /* Build file spec */
    err = FSMakeFSSpec(notepad->systemFolderVRefNum,
                      notepad->systemFolderDirID,
                      NOTEPAD_FILE_NAME,
                      &fileSpec);

    if (err == fnfErr) {
        /* Create file */
        err = FSpCreate(&fileSpec, NOTEPAD_CREATOR, NOTEPAD_FILE_TYPE, 0);
        if (err != noErr) {
            return err;
        }
    }

    /* Open file */
    err = FSpOpenDF(&fileSpec, fsWrPerm, &refNum);
    if (err != noErr) {
        return err;
    }

    /* Write header */
    header.signature = NOTEPAD_SIGNATURE;
    header.version = 1;
    header.pageCount = NOTEPAD_MAX_PAGES;
    header.currentPage = notepad->currentPage;

    count = sizeof(NotePadFileHeader);
    err = FSWrite(refNum, &count, &header);

    /* Write each page */
    for (short i = 0; i < NOTEPAD_MAX_PAGES; i++) {
        long pageSize = GetHandleSize(notepad->pageData[i]);

        /* Write page size */
        count = sizeof(long);
        err = FSWrite(refNum, &count, &pageSize);

        /* Write page data */
        if (pageSize > 0) {
            HLock(notepad->pageData[i]);
            err = FSWrite(refNum, &pageSize, *notepad->pageData[i]);
            HUnlock(notepad->pageData[i]);
        }
    }

    FSClose(refNum);
    notepad->isDirty = false;

    return noErr;
}

/*
 * Get Notepad window
 */
WindowPtr Notepad_GetWindow(void) {
    return gNotepad ? gNotepad->window : NULL;
}

/*
 * Check if Notepad is dirty
 */
Boolean Notepad_IsDirty(void) {
    return gNotepad ? gNotepad->isDirty : false;
}

/*
 * Get current page number
 */
short Notepad_GetCurrentPage(void) {
    return gNotepad ? gNotepad->currentPage : 0;
}
