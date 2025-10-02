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
    extern void serial_printf(const char* fmt, ...);

    Rect iconRect;
    SetRect(&iconRect, x, y, x + 32, y + 32);

    serial_printf("[ICON] res=%d at(l)={%d,%d} port=%p\n",
                  isFolder ? 1 : 2, x, y, NULL);

    if (isFolder) {
        /* Draw folder shape - paint with gray then frame */
        extern void PaintRect(const Rect* r);
        extern void InvertRect(const Rect* r);
        PaintRect(&iconRect);  /* Fill with black */
        FrameRect(&iconRect);
        /* Tab on top */
        Rect tabRect;
        SetRect(&tabRect, x, y - 4, x + 12, y);
        PaintRect(&tabRect);  /* Fill with black */
        FrameRect(&tabRect);
    } else {
        /* Draw document shape - just frame it (white background) */
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

    /* CRITICAL: Log port and coordinate mapping for debugging */
    serial_printf("DrawFolder: window=%p savePort=%p\n", window, savePort);
    serial_printf("DrawFolder: portBits.bounds(GLOBAL)=(%d,%d,%d,%d)\n",
                  window->port.portBits.bounds.left, window->port.portBits.bounds.top,
                  window->port.portBits.bounds.right, window->port.portBits.bounds.bottom);

    /* Use window's portRect which is in LOCAL (port-relative) coordinates */
    /* In Mac Toolbox, portRect should always start at (0,0) */
    Rect localBounds = window->port.portRect;
    serial_printf("DrawFolder: portRect(LOCAL)=(%d,%d,%d,%d)\n",
                  localBounds.left, localBounds.top, localBounds.right, localBounds.bottom);

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

    /* Fill background with white - EraseRect uses LOCAL coords, DrawPrimitive converts to GLOBAL */
    extern void EraseRect(const Rect* r);
    EraseRect(&contentRect);

    serial_printf("Finder: Erased contentRect (%d,%d,%d,%d) for white backfill\n",
                  contentRect.left, contentRect.top, contentRect.right, contentRect.bottom);

    /* Text drawing disabled until Font Manager is linked */

    if (isTrash) {
        /* Draw trash contents */
        MoveTo(contentRect.left + 10, contentRect.top + 30);
        DrawText("Trash is empty", 0, 14);
        serial_printf("[TEXT] 'Trash is empty' at(l)={%d,%d} font=%d size=%d\n",
                      contentRect.left + 10, contentRect.top + 30, 0, 9);

        MoveTo(contentRect.left + 10, contentRect.top + 50);
        DrawText("Drag items here to delete them", 0, 30);
        serial_printf("[TEXT] 'Drag items here to delete them' at(l)={%d,%d} font=%d size=%d\n",
                      contentRect.left + 10, contentRect.top + 50, 0, 9);
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
        serial_printf("[TEXT] 'System Folder' at(l)={%d,%d} font=%d size=%d\n", x - 23, y + 40, 0, 9);

        x += iconSpacing;

        /* Applications folder - label ~72px wide */
        DrawFileIcon(x, y, true);
        MoveTo(x - 20, y + 40);
        DrawText("Applications", 0, 12);
        serial_printf("[TEXT] 'Applications' at(l)={%d,%d} font=%d size=%d\n", x - 20, y + 40, 0, 9);

        x += iconSpacing;

        /* Documents folder - label ~54px wide */
        DrawFileIcon(x, y, true);
        MoveTo(x - 11, y + 40);
        DrawText("Documents", 0, 9);
        serial_printf("[TEXT] 'Documents' at(l)={%d,%d} font=%d size=%d\n", x - 11, y + 40, 0, 9);

        /* Second row */
        x = contentRect.left + 80;
        y += rowHeight;

        /* SimpleText document - label ~60px wide */
        DrawFileIcon(x, y, false);
        MoveTo(x - 14, y + 40);
        DrawText("ReadMe.txt", 0, 10);
        serial_printf("[TEXT] 'ReadMe.txt' at(l)={%d,%d} font=%d size=%d\n", x - 14, y + 40, 0, 9);

        x += iconSpacing;

        /* TeachText document - label ~84px wide */
        DrawFileIcon(x, y, false);
        MoveTo(x - 26, y + 40);
        DrawText("About System 7", 0, 14);
        serial_printf("[TEXT] 'About System 7' at(l)={%d,%d} font=%d size=%d\n", x - 26, y + 40, 0, 9);

        /* Show disk space at bottom */
        MoveTo(contentRect.left + 10, contentRect.bottom - 10);
        DrawText("5 items     42.3 MB in disk     193.7 MB available", 0, 52);
        serial_printf("[TEXT] 'disk info' at(l)={%d,%d} font=%d size=%d\n",
                      contentRect.left + 10, contentRect.bottom - 10, 0, 9);
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