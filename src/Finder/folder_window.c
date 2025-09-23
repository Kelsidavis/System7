/*
 * folder_window.c - Folder window content display
 *
 * Displays folder contents in windows opened from desktop icons
 */

#include "SystemTypes.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"

extern void serial_printf(const char* fmt, ...);
extern void DrawString(const unsigned char* str);
extern void MoveTo(short h, short v);
extern void LineTo(short h, short v);
extern void FrameRect(const Rect* r);
extern void DrawText(const void* textBuf, short firstByte, short byteCount);

/* Draw a simple file/folder icon */
static void DrawFileIcon(short x, short y, Boolean isFolder)
{
    Rect iconRect;
    SetRect(&iconRect, x, y, x + 32, y + 32);

    if (isFolder) {
        /* Draw folder shape */
        FrameRect(&iconRect);
        /* Tab on top */
        Rect tabRect;
        SetRect(&tabRect, x, y - 4, x + 12, y);
        FrameRect(&tabRect);
    } else {
        /* Draw document shape */
        FrameRect(&iconRect);
        /* Folded corner */
        MoveTo(x + 24, y);
        LineTo(x + 32, y + 8);
        LineTo(x + 24, y + 8);
        LineTo(x + 24, y);
    }
}

/* Draw folder window contents - Content Only, No Chrome */
void DrawFolderWindowContents(WindowPtr window, Boolean isTrash)
{
    if (!window) return;

    serial_printf("Finder: Drawing contents of '%s'\n",
                  isTrash ? "Trash" : "Macintosh HD");

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(window);

    /* Get window content area (skip title bar area) */
    Rect contentRect;
    SetRect(&contentRect, 1, 21, 399, 299);  /* Content area only, inside frame */

    /* Clear content area */
    EraseRect(&contentRect);

    if (isTrash) {
        /* Draw trash contents */
        MoveTo(contentRect.left + 10, contentRect.top + 30);
        DrawText("Trash is empty", 0, 14);

        MoveTo(contentRect.left + 10, contentRect.top + 50);
        DrawText("Drag items here to delete them", 0, 30);
    } else {
        /* Draw volume contents - sample items */
        short x = contentRect.left + 20;
        short y = contentRect.top + 20;

        /* System Folder */
        DrawFileIcon(x, y, true);
        MoveTo(x, y + 45);
        DrawText("System Folder", 0, 13);

        x += 80;

        /* Applications folder */
        DrawFileIcon(x, y, true);
        MoveTo(x - 5, y + 45);
        DrawText("Applications", 0, 12);

        x += 80;

        /* Documents folder */
        DrawFileIcon(x, y, true);
        MoveTo(x + 2, y + 45);
        DrawText("Documents", 0, 9);

        /* Second row */
        x = contentRect.left + 20;
        y += 70;

        /* SimpleText document */
        DrawFileIcon(x, y, false);
        MoveTo(x - 8, y + 45);
        DrawText("ReadMe.txt", 0, 10);

        x += 80;

        /* TeachText document */
        DrawFileIcon(x, y, false);
        MoveTo(x - 12, y + 45);
        DrawText("About System 7", 0, 14);

        /* Show disk space at bottom */
        MoveTo(contentRect.left + 10, contentRect.bottom - 20);
        DrawText("5 items     42.3 MB in disk     193.7 MB available", 0, 52);
    }

    SetPort(savePort);
}

/* Update window proc for folder windows */
void FolderWindowProc(WindowPtr window, short message, long param)
{
    switch (message) {
        case 0:  /* wDraw = 0, draw content only */
            /* Draw window contents - NO CHROME! */
            serial_printf("Finder: FolderWindowProc drawing content\n");
            if (window->refCon == 'TRSH') {
                DrawFolderWindowContents(window, true);
            } else {
                DrawFolderWindowContents(window, false);
            }
            break;

        case 1:  /* wHit = 1, handle click in content */
            /* Handle click in content */
            serial_printf("Click in folder window at (%d,%d)\n",
                         (short)(param >> 16), (short)(param & 0xFFFF));
            break;

        default:
            break;
    }
}