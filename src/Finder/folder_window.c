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
extern void ClipRect(const Rect* r);

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

    /* Use window's portRect which is in LOCAL (port-relative) coordinates */
    /* In Mac Toolbox, portRect should always start at (0,0) */
    Rect localBounds = window->port.portRect;

    /* Calculate content area in LOCAL coordinates */
    /* Content = full port minus title bar (20px) */
    Rect contentRect;
    contentRect.left = 0;
    contentRect.top = 20;  /* Skip title bar */
    contentRect.right = localBounds.right - localBounds.left;
    contentRect.bottom = localBounds.bottom - localBounds.top;

    serial_printf("Finder: portRect (local) = (%d,%d,%d,%d)\n",
                  localBounds.left, localBounds.top, localBounds.right, localBounds.bottom);

    serial_printf("Finder: contentRect (local) = (%d,%d,%d,%d)\n",
                  contentRect.left, contentRect.top, contentRect.right, contentRect.bottom);

    /* Set clipping to content area to prevent drawing outside bounds */
    ClipRect(&contentRect);

    /* Fill background with white using EraseRect (uses port's background pattern) */
    extern void EraseRect(const Rect* r);
    EraseRect(&contentRect);

    serial_printf("Finder: Erased content rect (%d,%d,%d,%d)\n",
                  contentRect.left, contentRect.top, contentRect.right, contentRect.bottom);

    if (isTrash) {
        /* Draw trash contents */
        MoveTo(contentRect.left + 10, contentRect.top + 30);
        DrawText("Trash is empty", 0, 14);

        MoveTo(contentRect.left + 10, contentRect.top + 50);
        DrawText("Drag items here to delete them", 0, 30);
    } else {
        /* Draw volume contents - sample items in icon grid */
        /* Ensure margins: 80px left (room for labels), 30px top */
        short x = contentRect.left + 80;
        short y = contentRect.top + 30;
        short iconSpacing = 100;
        short rowHeight = 90;

        /* System Folder - icon 32px wide, label ~78px wide */
        DrawFileIcon(x, y, true);
        MoveTo(x - 23, y + 40);
        DrawText("System Folder", 0, 13);

        x += iconSpacing;

        /* Applications folder - label ~72px wide */
        DrawFileIcon(x, y, true);
        MoveTo(x - 20, y + 40);
        DrawText("Applications", 0, 12);

        x += iconSpacing;

        /* Documents folder - label ~54px wide */
        DrawFileIcon(x, y, true);
        MoveTo(x - 11, y + 40);
        DrawText("Documents", 0, 9);

        /* Second row */
        x = contentRect.left + 80;
        y += rowHeight;

        /* SimpleText document - label ~60px wide */
        DrawFileIcon(x, y, false);
        MoveTo(x - 14, y + 40);
        DrawText("ReadMe.txt", 0, 10);

        x += iconSpacing;

        /* TeachText document - label ~84px wide */
        DrawFileIcon(x, y, false);
        MoveTo(x - 26, y + 40);
        DrawText("About System 7", 0, 14);

        /* Show disk space at bottom */
        MoveTo(contentRect.left + 10, contentRect.bottom - 10);
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