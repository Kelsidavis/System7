/*
 * Find.c - Find File Dialog Implementation
 *
 * Provides File > Find and Find Again functionality.
 * Allows searching for files by name pattern across volumes.
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
#include "Finder/finder.h"
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

#define kFindRefCon 0x46494E44  /* 'FIND' */

/* Search result storage */
#define MAX_RESULTS 100
typedef struct {
    char name[32];
    FileID id;
    DirID parentID;
    VRefNum vref;
} SearchResult;

/* Global Find window state */
static WindowPtr sFindWin = NULL;
static char sSearchTerm[256] = "";
static Boolean sHasSearchTerm = false;
static SearchResult sResults[MAX_RESULTS];
static int sResultCount = 0;
static Boolean sSearchInProgress = false;

/*
 * Case-insensitive substring match
 */
static Boolean Find_MatchesSearchTerm(const char* filename, const char* searchTerm) {
    /* Convert both to lowercase for comparison */
    char lowerFile[256];
    char lowerTerm[256];
    int i;

    for (i = 0; filename[i] && i < 255; i++) {
        lowerFile[i] = (filename[i] >= 'A' && filename[i] <= 'Z') ?
                      (filename[i] + 32) : filename[i];
    }
    lowerFile[i] = '\0';

    for (i = 0; searchTerm[i] && i < 255; i++) {
        lowerTerm[i] = (searchTerm[i] >= 'A' && searchTerm[i] <= 'Z') ?
                      (searchTerm[i] + 32) : searchTerm[i];
    }
    lowerTerm[i] = '\0';

    /* Check if search term is substring of filename */
    return (strstr(lowerFile, lowerTerm) != NULL);
}

/*
 * Search directory recursively
 */
static void Find_SearchDirectory(VRefNum vref, DirID dirID) {
    CatEntry entries[64];
    int count = 0;
    int i;

    /* Don't overflow results array */
    if (sResultCount >= MAX_RESULTS) {
        return;
    }

    /* Enumerate this directory */
    if (!VFS_Enumerate(vref, dirID, entries, 64, &count)) {
        return;
    }

    /* Process each entry */
    for (i = 0; i < count && sResultCount < MAX_RESULTS; i++) {
        /* Check if filename matches search term */
        if (Find_MatchesSearchTerm(entries[i].name, sSearchTerm)) {
            /* Add to results */
            strncpy(sResults[sResultCount].name, entries[i].name, 31);
            sResults[sResultCount].name[31] = '\0';
            sResults[sResultCount].id = entries[i].id;
            sResults[sResultCount].parentID = entries[i].parent;
            sResults[sResultCount].vref = vref;
            sResultCount++;

            FINDER_LOG_DEBUG("Find: Match found: %s\n", entries[i].name);
        }

        /* Recursively search subdirectories */
        if (entries[i].kind == kNodeDir) {
            Find_SearchDirectory(vref, entries[i].id);
        }
    }
}

/*
 * Execute search across all mounted volumes
 */
static void Find_PerformSearch(void) {
    VRefNum bootVRef;
    VolumeControlBlock vcb;

    FINDER_LOG_DEBUG("Find: Starting search for '%s'\n", sSearchTerm);

    /* Clear previous results */
    sResultCount = 0;
    sSearchInProgress = true;

    /* Get boot volume */
    bootVRef = VFS_GetBootVRef();
    if (bootVRef != 0 && VFS_GetVolumeInfo(bootVRef, &vcb)) {
        /* Search from root directory */
        FINDER_LOG_DEBUG("Find: Searching volume '%s'\n", vcb.name);
        Find_SearchDirectory(bootVRef, vcb.rootID);
    }

    sSearchInProgress = false;

    FINDER_LOG_DEBUG("Find: Search complete, %d results found\n", sResultCount);

    /* Update window to show results */
    if (sFindWin) {
        PostEvent(updateEvt, (UInt32)(uintptr_t)sFindWin);
    }
}

/*
 * Find_CreateWindow - Create Find dialog window
 */
static void Find_CreateWindow(void) {
    Rect bounds;
    unsigned char title[64];

    if (sFindWin) {
        return;
    }

    /* Position window */
    bounds.top = 100;
    bounds.left = 120;
    bounds.bottom = bounds.top + 200;  /* Height: 200 pixels */
    bounds.right = bounds.left + 400;  /* Width: 400 pixels */

    /* Create window title */
    const char* titleText = "Find";
    title[0] = (unsigned char)strlen(titleText);
    strcpy((char*)&title[1], titleText);

    sFindWin = NewWindow(NULL, &bounds, title,
                        1,  /* visible */
                        0,  /* documentProc */
                        (WindowPtr)-1,
                        1,  /* Has close box */
                        kFindRefCon);

    if (!sFindWin) {
        FINDER_LOG_DEBUG("Find: Failed to create window\n");
        return;
    }

    FINDER_LOG_DEBUG("Find: Created window at 0x%08x\n", (unsigned int)P2UL(sFindWin));
}

/*
 * ShowFind - Show Find dialog
 */
OSErr ShowFind(void) {
    FINDER_LOG_DEBUG("ShowFind: Entry\n");

    /* Create window if needed */
    if (!sFindWin) {
        Find_CreateWindow();
    }

    if (!sFindWin) {
        return memFullErr;
    }

    /* Bring to front */
    BringToFront(sFindWin);
    SelectWindow(sFindWin);

    /* Request update */
    PostEvent(updateEvt, (UInt32)(uintptr_t)sFindWin);

    FINDER_LOG_DEBUG("ShowFind: Window shown\n");
    return noErr;
}

/*
 * Find_CloseIf - Close Find window if it matches
 */
void Find_CloseIf(WindowPtr w) {
    if (!w || w != sFindWin) {
        return;
    }

    FINDER_LOG_DEBUG("Find: Closing window\n");
    DisposeWindow(sFindWin);
    sFindWin = NULL;
}

/*
 * Find_HandleUpdate - Redraw Find window contents
 */
Boolean Find_HandleUpdate(WindowPtr w) {
    GrafPtr savedPort;
    Rect contentRect;
    char buf[256];
    short y;

    if (!w || w != sFindWin) {
        return false;
    }

    FINDER_LOG_DEBUG("Find: HandleUpdate called\n");

    /* Save current port and set to our window */
    GetPort(&savedPort);
    SetPort((GrafPtr)w);

    /* Get content rect */
    contentRect = w->port.portRect;
    contentRect.top = 20;  /* Skip title bar */

    /* Clear background */
    EraseRect(&contentRect);

    /* Draw instructions */
    y = contentRect.top + 30;
    MoveTo(20, y);
    snprintf(buf, sizeof(buf), "Find File Dialog");
    DrawText(buf, 0, strlen(buf));
    y += 25;

    MoveTo(20, y);
    snprintf(buf, sizeof(buf), "Search for:");
    DrawText(buf, 0, strlen(buf));
    y += 20;

    /* Draw search term input area (placeholder) */
    MoveTo(30, y);
    if (sHasSearchTerm && sSearchTerm[0] != '\0') {
        snprintf(buf, sizeof(buf), "> %s", sSearchTerm);
    } else {
        snprintf(buf, sizeof(buf), "> (enter search term)");
    }
    DrawText(buf, 0, strlen(buf));
    y += 25;

    /* Instructions */
    MoveTo(20, y);
    snprintf(buf, sizeof(buf), "Type text to search for files by name.");
    DrawText(buf, 0, strlen(buf));
    y += 20;

    MoveTo(20, y);
    snprintf(buf, sizeof(buf), "Press Return to search.");
    DrawText(buf, 0, strlen(buf));
    y += 30;

    /* Show search results */
    if (sSearchInProgress) {
        MoveTo(20, y);
        snprintf(buf, sizeof(buf), "Searching...");
        DrawText(buf, 0, strlen(buf));
    } else if (sResultCount > 0) {
        int i;
        int maxDisplay = (contentRect.bottom - y - 10) / 15;  /* Lines that fit */
        if (maxDisplay > sResultCount) maxDisplay = sResultCount;

        MoveTo(20, y);
        snprintf(buf, sizeof(buf), "Found %d file(s):", sResultCount);
        DrawText(buf, 0, strlen(buf));
        y += 20;

        /* Display results */
        for (i = 0; i < maxDisplay; i++) {
            MoveTo(30, y);
            snprintf(buf, sizeof(buf), "%s", sResults[i].name);
            DrawText(buf, 0, strlen(buf));
            y += 15;
        }

        if (sResultCount > maxDisplay) {
            MoveTo(30, y);
            snprintf(buf, sizeof(buf), "... and %d more", sResultCount - maxDisplay);
            DrawText(buf, 0, strlen(buf));
        }
    } else if (sHasSearchTerm && sSearchTerm[0] != '\0') {
        /* Search was performed but no results */
        MoveTo(20, y);
        snprintf(buf, sizeof(buf), "No files found matching '%s'", sSearchTerm);
        DrawText(buf, 0, strlen(buf));
    }

    /* Restore previous port */
    SetPort(savedPort);

    FINDER_LOG_DEBUG("Find: Update complete\n");
    return true;
}

/*
 * Find_IsFindWindow - Check if window is the Find window
 */
Boolean Find_IsFindWindow(WindowPtr w) {
    return (w && w == sFindWin);
}

/*
 * FindAgain - Repeat last search
 */
OSErr FindAgain(void) {
    FINDER_LOG_DEBUG("FindAgain: Entry\n");

    if (!sHasSearchTerm || sSearchTerm[0] == '\0') {
        FINDER_LOG_DEBUG("FindAgain: No previous search term\n");
        /* If no previous search, show Find dialog */
        return ShowFind();
    }

    FINDER_LOG_DEBUG("FindAgain: Repeating search for '%s'\n", sSearchTerm);

    /* Perform the search with previous term */
    Find_PerformSearch();

    /* Show the Find dialog to display results */
    return ShowFind();
}

/*
 * Find_HandleKeyPress - Handle keyboard input in Find window
 */
Boolean Find_HandleKeyPress(WindowPtr w, char key) {
    if (!w || w != sFindWin) {
        return false;
    }

    /* Handle character input */
    if (key >= 32 && key <= 126) {  /* Printable ASCII */
        size_t len = strlen(sSearchTerm);
        if (len < sizeof(sSearchTerm) - 1) {
            sSearchTerm[len] = key;
            sSearchTerm[len + 1] = '\0';
            sHasSearchTerm = true;

            /* Trigger redraw */
            PostEvent(updateEvt, (UInt32)(uintptr_t)w);

            FINDER_LOG_DEBUG("Find: Added char '%c', term now '%s'\n", key, sSearchTerm);
        }
        return true;
    }

    /* Handle backspace */
    if (key == 8 || key == 127) {  /* Backspace or Delete */
        size_t len = strlen(sSearchTerm);
        if (len > 0) {
            sSearchTerm[len - 1] = '\0';

            /* Trigger redraw */
            PostEvent(updateEvt, (UInt32)(uintptr_t)w);

            FINDER_LOG_DEBUG("Find: Backspace, term now '%s'\n", sSearchTerm);
        }
        return true;
    }

    /* Handle return key - perform search */
    if (key == 13 || key == 3) {  /* Return or Enter */
        if (sSearchTerm[0] != '\0') {
            FINDER_LOG_DEBUG("Find: Search requested for '%s'\n", sSearchTerm);
            /* Perform the search */
            Find_PerformSearch();
        }
        return true;
    }

    return false;
}
