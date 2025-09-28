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
    SetRect(&iconRect, y, x, y + 32, x + 32);

    if (isFolder) {
        /* Draw folder shape */
        FrameRect(&iconRect);
        /* Tab on top */
        Rect tabRect;
        SetRect(&tabRect, y - 4, x, y, x + 12);
        FrameRect(&tabRect);
    } else {
        /* Draw document shape */
        FrameRect(&iconRect);
        /* Folded corner */
        MoveTo(y, x + 24);
        LineTo(y + 8, x + 32);
        LineTo(y + 8, x + 24);
        LineTo(y, x + 24);
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

    /* Get window content area from window's port.portRect */
    Rect contentRect;
    contentRect = window->port.portRect;

    /* Adjust for title bar (20 pixels) and minimal left margin */
    contentRect.top += 20;
    contentRect.left += 0;
    contentRect.right -= 1;
    contentRect.bottom -= 1;

    /* Fill background with white (classic QuickDraw color index 30) */
    ForeColor(30);  /* white */
    PaintRect(&contentRect);

    /* Reset to black for drawing text/icons (classic QuickDraw color index 33) */
    ForeColor(33);  /* black */

    if (isTrash) {
        /* Draw trash contents */
        MoveTo(contentRect.top + 30, contentRect.left + 10);
        DrawText("Trash is empty", 0, 14);

        MoveTo(contentRect.top + 50, contentRect.left + 10);
        DrawText("Drag items here to delete them", 0, 30);
    } else {
        /* Draw volume contents - sample items */
        short x = contentRect.left + 70;
        short y = contentRect.top + 20;

        /* System Folder */
        DrawFileIcon(x, y, true);
        MoveTo(y + 45, x);
        DrawText("System Folder", 0, 13);

        x += 80;

        /* Applications folder */
        DrawFileIcon(x, y, true);
        MoveTo(y + 45, x - 5);
        DrawText("Applications", 0, 12);

        x += 80;

        /* Documents folder */
        DrawFileIcon(x, y, true);
        MoveTo(y + 45, x + 2);
        DrawText("Documents", 0, 9);

        /* Second row */
        x = contentRect.left + 70;
        y += 70;

        /* SimpleText document */
        DrawFileIcon(x, y, false);
        MoveTo(y + 45, x - 8);
        DrawText("ReadMe.txt", 0, 10);

        x += 80;

        /* TeachText document */
        DrawFileIcon(x, y, false);
        MoveTo(y + 45, x - 12);
        DrawText("About System 7", 0, 14);

        /* Show disk space at bottom */
        MoveTo(contentRect.bottom - 50, contentRect.left + 20);
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