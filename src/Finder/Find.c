/*
 * Find.c - File > Find and Find Again
 *
 * As the System 7 Finder does it: Find asks for a name in a modal dialog,
 * searches the startup disk, and shows the first item whose name contains the
 * text by opening the folder that holds it with the item selected. Find Again
 * shows the next one, and beeps when there are no more.
 *
 * This used to open a document window of instructions with a "> (enter search
 * term)" prompt that nothing ever filled in: its key handler had no caller, so
 * no search could be typed and none was ever run.
 */

#include <string.h>

#include "SystemTypes.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogHelpers.h"
#include "DialogManager/DITLBuilder.h"
#include "FS/vfs.h"
#include "FS/hfs_types.h"
#include "Finder/FinderLogging.h"
#include "Finder/finder.h"
#include "SoundManager/SoundManager.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"

#define MAX_RESULTS 100
typedef struct {
    char name[32];
    DirID parentID;
    VRefNum vref;
} SearchResult;

static char sSearchTerm[256] = "";
static SearchResult sResults[MAX_RESULTS];
static int sResultCount = 0;
static int sNextResult = 0;     /* the one Find Again shows */

/* Case-insensitive: does filename contain term? */
static Boolean Find_Matches(const char* filename, const char* term) {
    size_t n = strlen(filename), m = strlen(term);
    if (m == 0 || m > n) return false;
    for (size_t i = 0; i + m <= n; i++) {
        size_t j = 0;
        while (j < m) {
            char a = filename[i + j], b = term[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if (a != b) break;
            j++;
        }
        if (j == m) return true;
    }
    return false;
}

/* Every match in dirID and the folders inside it, in catalog order. */
static void Find_SearchDirectory(VRefNum vref, DirID dirID) {
    CatEntry entries[64];
    int count = 0;

    if (sResultCount >= MAX_RESULTS) return;
    if (!VFS_Enumerate(vref, dirID, entries, 64, &count)) return;

    for (int i = 0; i < count && sResultCount < MAX_RESULTS; i++) {
        if (Find_Matches(entries[i].name, sSearchTerm)) {
            SearchResult* r = &sResults[sResultCount++];
            strncpy(r->name, entries[i].name, sizeof(r->name) - 1);
            r->name[sizeof(r->name) - 1] = '\0';
            r->parentID = entries[i].parent;
            r->vref = vref;
        }
        if (entries[i].kind == kNodeDir) {
            Find_SearchDirectory(vref, entries[i].id);
        }
    }
}

static void Find_PerformSearch(void) {
    VolumeControlBlock vcb;
    VRefNum vref = VFS_GetBootVRef();

    sResultCount = 0;
    sNextResult = 0;
    if (vref != 0 && VFS_GetVolumeInfo(vref, &vcb)) {
        Find_SearchDirectory(vref, vcb.rootID);
    }
    FINDER_LOG_DEBUG("Find: '%s' matched %d items\n", sSearchTerm, sResultCount);
}

/* Open the folder holding the next match with it selected; false if none. */
static Boolean Find_ShowNext(void) {
    if (sNextResult >= sResultCount) return false;
    SearchResult* r = &sResults[sNextResult++];

    /* The folder's window is titled with its name - the disk's, at the top */
    Str255 title;
    VolumeControlBlock vcb;
    CatEntry folder;
    const char* name = "";
    if (VFS_GetVolumeInfo(r->vref, &vcb) && r->parentID == vcb.rootID) {
        name = vcb.name;
    } else if (VFS_GetByID(r->vref, r->parentID, &folder)) {
        name = folder.name;
    }
    c2pstrcpy(title, name);

    WindowPtr w = FolderWindow_OpenFolder(r->vref, r->parentID, title);
    if (!w) return false;
    FolderWindow_SelectByName(w, r->name);

    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)w);
    InvalRect(&w->port.portRect);
    SetPort(save);
    return true;
}

/* Ask for the text to find; false if cancelled or left empty. */
static Boolean Find_AskForName(void) {
    DITLBuilder b;
    if (!DITL_Begin(&b, 512)) return false;
    DITL_AddButton(&b, 70, 196, 90, 276, "Find");
    DITL_AddButton(&b, 70, 104, 90, 184, "Cancel");
    DITL_AddEditText(&b, 36, 60, 52, 276, sSearchTerm);
    DITL_AddText(&b, 36, 14, 52, 56, "Find:");
    Handle ditl = DITL_Finish(&b);
    if (!ditl) return false;

    Rect bounds = { 0, 0, 104, 290 };
    DialogPtr dlg = NewDialog(NULL, &bounds, (ConstStr255Param)"\004Find", false,
                              dBoxProc, (WindowPtr)-1, false, 0, ditl);
    if (!dlg) {
        DisposeHandle(ditl);
        return false;
    }
    CenterDialogOnScreen(dlg);
    ShowWindow((WindowPtr)dlg);
    SelectDialogItemText(dlg, 3, 0, 32767);

    SInt16 hit = RunModalDialogBox(dlg, 1, 2);
    if (hit == 1) {
        SInt16 type;
        Handle h;
        Rect box;
        unsigned char text[256];
        GetDialogItem(dlg, 3, &type, &h, &box);
        GetDialogItemText(h, text);
        memcpy(sSearchTerm, &text[1], text[0]);
        sSearchTerm[text[0]] = '\0';
    }
    DisposeDialog(dlg);
    return hit == 1 && sSearchTerm[0] != '\0';
}

OSErr ShowFind(void) {
    if (!Find_AskForName()) return noErr;
    Find_PerformSearch();
    if (!Find_ShowNext()) SysBeep(1);
    return noErr;
}

OSErr FindAgain(void) {
    if (sSearchTerm[0] == '\0') return ShowFind();
    if (!Find_ShowNext()) SysBeep(1);
    return noErr;
}
