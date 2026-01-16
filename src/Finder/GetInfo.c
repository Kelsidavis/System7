/*
 * GetInfo.c - Get Info Window Implementation
 *
 * Displays file/folder information dialog similar to System 7.1 Get Info.
 * Shows name, kind, size, location, dates, type/creator codes.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "SystemTypes.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "FS/vfs.h"
#include "FS/hfs_types.h"
#include "Finder/FinderLogging.h"
#include "Finder/GetInfo.h"
#include "EventManager/EventManager.h"

/* Forward declarations */
extern void MoveTo(short h, short v);
extern void DrawText(const void* textBuf, short firstByte, short byteCount);
extern void EraseRect(const Rect* r);
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern void SelectWindow(WindowPtr w);
extern void BringToFront(WindowPtr w);
extern void DisposeWindow(WindowPtr w);
extern WindowPtr NewWindow(void* wStorage, const Rect* boundsRect, ConstStr255Param title,
                           Boolean visible, short procID, WindowPtr behind, Boolean goAwayFlag, long refCon);

#define kGetInfoRefCon 0x47494E46  /* 'GINF' */

/* Global Get Info window */
static WindowPtr sGetInfoWin = NULL;
static CatEntry sCurrentEntry = {0};
static Boolean sHasEntry = false;

/*
 * FormatFileSize - Format file size as human-readable string
 */
static void FormatFileSize(uint32_t bytes, char* out, size_t outSize) {
    if (bytes < 1024) {
        snprintf(out, outSize, "%u bytes", (unsigned int)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(out, outSize, "%u KB", (unsigned int)(bytes / 1024));
    } else {
        snprintf(out, outSize, "%.1f MB", bytes / (1024.0 * 1024.0));
    }
}

/*
 * FormatOSType - Format OSType (4-char code) as string
 */
static void FormatOSType(uint32_t type, char* out) {
    out[0] = (type >> 24) & 0xFF;
    out[1] = (type >> 16) & 0xFF;
    out[2] = (type >> 8) & 0xFF;
    out[3] = type & 0xFF;
    out[4] = '\0';
}

/*
 * GetInfo_CreateWindow - Create Get Info window
 */
static void GetInfo_CreateWindow(void) {
    Rect bounds;
    unsigned char title[64];

    if (sGetInfoWin) {
        return;
    }

    /* Position window (smaller than About box) */
    bounds.top = 80;
    bounds.left = 100;
    bounds.bottom = bounds.top + 300;  /* Height: 300 pixels */
    bounds.right = bounds.left + 400;  /* Width: 400 pixels */

    /* Create window title */
    const char* titleText = "Info";
    title[0] = (unsigned char)strlen(titleText);
    strcpy((char*)&title[1], titleText);

    sGetInfoWin = NewWindow(NULL, &bounds, title,
                           1,  /* visible */
                           0,  /* documentProc */
                           (WindowPtr)-1,
                           1,  /* Has close box */
                           kGetInfoRefCon);

    if (!sGetInfoWin) {
        FINDER_LOG_DEBUG("GetInfo: Failed to create window\n");
        return;
    }

    FINDER_LOG_DEBUG("GetInfo: Created window at 0x%08x\n", (unsigned int)P2UL(sGetInfoWin));
}

/*
 * GetInfo_Show - Show Get Info for a catalog entry
 */
void GetInfo_Show(VRefNum vref, FileID fileID) {
    CatEntry entry;

    FINDER_LOG_DEBUG("GetInfo: Show for fileID=%d vref=%d\n", (int)fileID, (int)vref);

    /* Fetch catalog entry from VFS */
    if (!VFS_GetByID(vref, fileID, &entry)) {
        FINDER_LOG_DEBUG("GetInfo: Failed to get catalog entry\n");
        return;
    }

    /* Store entry data */
    sCurrentEntry = entry;
    sHasEntry = true;

    /* Create window if needed */
    if (!sGetInfoWin) {
        GetInfo_CreateWindow();
    }

    if (!sGetInfoWin) {
        return;
    }

    /* Bring to front */
    BringToFront(sGetInfoWin);
    SelectWindow(sGetInfoWin);

    /* Request update */
    PostEvent(updateEvt, (UInt32)(uintptr_t)sGetInfoWin);

    FINDER_LOG_DEBUG("GetInfo: Window shown\n");
}

/*
 * GetInfo_CloseIf - Close Get Info window if it matches
 */
void GetInfo_CloseIf(WindowPtr w) {
    if (!w || w != sGetInfoWin) {
        return;
    }

    FINDER_LOG_DEBUG("GetInfo: Closing window\n");
    DisposeWindow(sGetInfoWin);
    sGetInfoWin = NULL;
    sHasEntry = false;
}

/*
 * GetInfo_HandleUpdate - Redraw Get Info window contents
 */
Boolean GetInfo_HandleUpdate(WindowPtr w) {
    GrafPtr savedPort;
    Rect contentRect;
    char buf[256];
    short y;

    if (!w || w != sGetInfoWin || !sHasEntry) {
        return false;
    }

    FINDER_LOG_DEBUG("GetInfo: HandleUpdate called\n");

    /* Save current port and set to our window */
    GetPort(&savedPort);
    SetPort((GrafPtr)w);

    /* Get content rect */
    contentRect = w->port.portRect;
    contentRect.top = 20;  /* Skip title bar */

    /* Clear background */
    EraseRect(&contentRect);

    /* Draw information */
    y = contentRect.top + 20;

    /* Name */
    MoveTo(20, y);
    snprintf(buf, sizeof(buf), "Name: %s", sCurrentEntry.name);
    DrawText(buf, 0, strlen(buf));
    y += 20;

    /* Kind */
    MoveTo(20, y);
    snprintf(buf, sizeof(buf), "Kind: %s",
             sCurrentEntry.kind == kNodeDir ? "Folder" : "Document");
    DrawText(buf, 0, strlen(buf));
    y += 20;

    /* Size (for files) */
    if (sCurrentEntry.kind == kNodeFile) {
        MoveTo(20, y);
        char sizeStr[64];
        FormatFileSize(sCurrentEntry.size, sizeStr, sizeof(sizeStr));
        snprintf(buf, sizeof(buf), "Size: %s", sizeStr);
        DrawText(buf, 0, strlen(buf));
        y += 20;

        MoveTo(20, y);
        snprintf(buf, sizeof(buf), "     (%u bytes)", (unsigned int)sCurrentEntry.size);
        DrawText(buf, 0, strlen(buf));
        y += 20;
    }

    /* Separator */
    y += 10;

    /* Type (for files) */
    if (sCurrentEntry.kind == kNodeFile && sCurrentEntry.type != 0) {
        MoveTo(20, y);
        char typeStr[8];
        FormatOSType(sCurrentEntry.type, typeStr);
        snprintf(buf, sizeof(buf), "Type: %s", typeStr);
        DrawText(buf, 0, strlen(buf));
        y += 20;
    }

    /* Creator (for files) */
    if (sCurrentEntry.kind == kNodeFile && sCurrentEntry.creator != 0) {
        MoveTo(20, y);
        char creatorStr[8];
        FormatOSType(sCurrentEntry.creator, creatorStr);
        snprintf(buf, sizeof(buf), "Creator: %s", creatorStr);
        DrawText(buf, 0, strlen(buf));
        y += 20;
    }

    /* File ID */
    if (y + 20 < contentRect.bottom) {
        y += 10;
        MoveTo(20, y);
        snprintf(buf, sizeof(buf), "File ID: %u", (unsigned int)sCurrentEntry.id);
        DrawText(buf, 0, strlen(buf));
        y += 20;
    }

    /* Restore previous port */
    SetPort(savedPort);

    FINDER_LOG_DEBUG("GetInfo: Update complete\n");
    return true;
}

/*
 * GetInfo_IsInfoWindow - Check if window is the Get Info window
 */
Boolean GetInfo_IsInfoWindow(WindowPtr w) {
    return (w && w == sGetInfoWin);
}
