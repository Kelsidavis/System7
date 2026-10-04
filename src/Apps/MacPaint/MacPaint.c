/*
 * MacPaint.c - the application: its window, menus, event loop and files
 *
 * One document in one window, as MacPaint was. The Finder starts it with
 * MacPaint_Launch, or with MacPaint_OpenDocument for a painting, and it runs
 * until Quit or Close; its menus replace the Finder's while it does.
 *
 * Paintings are MacPaint files: type 'PNTG', creator 'MPNT', a 512-byte
 * header (a version and the 38 patterns) and then the 720 rows of the page,
 * each packed with PackBits on its own.
 */

#include <string.h>

#include "MacPaintInternal.h"
#include "Apps/MacPaint.h"
#include "EventManager/EventManagerInternal.h"
#include "Finder/finder.h"
#include "WindowManager/WindowManager.h"
#include "MenuManager/MenuManager.h"
#include "EventManager/EventManager.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DITLBuilder.h"
#include "DialogManager/DialogHelpers.h"
#include "DeskManager/DeskManager.h"
#include "StandardFile/StandardFile.h"
#include "FS/vfs.h"
#include "MemoryMgr/MemoryManager.h"
#include "SoundManager/SoundManager.h"
#include "QuickDraw.h"
#include "QuickDrawConstants.h"
#include "System71StdLib.h"

/* ------------------------------------------------------------------------
 * Menus
 *
 * Each menu's items, text and number, are one list, so a separator line -
 * an item like any other - cannot put the numbers out of step with the text.
 * "\311" is the ellipsis; a leading "(" disables an item.
 * ------------------------------------------------------------------------ */

/* Numbers of its own: NewMenu refuses one already in use, and the Finder's
 * menus, out of the bar or not, still hold 128 to 133 */
enum { mApple = 400, mFile = 401, mEdit = 402, mGoodies = 403 };

#define MP_APPLE_ITEMS(X) \
    X(iAbout,      "About MacPaint\311") \
    X(iAppleSep,   "(-")

#define MP_FILE_ITEMS(X) \
    X(iNew,        "New/N") \
    X(iOpen,       "Open\311/O") \
    X(iFileSep1,   "(-") \
    X(iClose,      "Close/W") \
    X(iSave,       "Save/S") \
    X(iSaveAs,     "Save As\311") \
    X(iRevert,     "Revert") \
    X(iFileSep2,   "(-") \
    X(iQuit,       "Quit/Q")

#define MP_EDIT_ITEMS(X) \
    X(iUndo,       "Undo/Z") \
    X(iEditSep1,   "(-") \
    X(iCut,        "Cut/X") \
    X(iCopy,       "Copy/C") \
    X(iPaste,      "Paste/V") \
    X(iClear,      "Clear") \
    X(iEditSep2,   "(-") \
    X(iInvert,     "Invert") \
    X(iFill,       "Fill") \
    X(iFlipH,      "Flip Horizontal") \
    X(iFlipV,      "Flip Vertical") \
    X(iEditSep3,   "(-") \
    X(iSelectAll,  "Select All/A")

#define MP_GOODIES_ITEMS(X) \
    X(iGrid,       "Grid") \
    X(iFatBits,    "FatBits")

#define MP_ENUM(name, text) name,
#define MP_TEXT(name, text) text ";"

enum { iAppleNone, MP_APPLE_ITEMS(MP_ENUM) };
enum { iFileNone, MP_FILE_ITEMS(MP_ENUM) };
enum { iEditNone, MP_EDIT_ITEMS(MP_ENUM) };
enum { iGoodiesNone, MP_GOODIES_ITEMS(MP_ENUM) };

static MenuHandle gAppleMenu, gFileMenu, gEditMenu, gGoodiesMenu;

static void AppendItems(MenuHandle menu, const char* items) {
    Str255 p;
    size_t len = strlen(items);
    while (len > 0 && items[len - 1] == ';') len--;
    if (len > 255) len = 255;
    p[0] = (unsigned char)len;
    memcpy(&p[1], items, len);
    AppendMenu(menu, p);
}

static MenuHandle MakeMenu(short id, const char* title, const char* items) {
    Str255 t;
    c2pstrcpy(t, title);
    MenuHandle m = NewMenu(id, t);
    if (m) AppendItems(m, items);
    return m;
}

static void BuildMenus(void) {
    if (gFileMenu) return;
    static unsigned char appleTitle[] = { 1, 0x14 };
    gAppleMenu = NewMenu(mApple, appleTitle);
    if (gAppleMenu) {
        AppendItems(gAppleMenu, MP_APPLE_ITEMS(MP_TEXT));
        AddResMenu(gAppleMenu, FOURCC('D','R','V','R'));
    }
    gFileMenu = MakeMenu(mFile, "File", MP_FILE_ITEMS(MP_TEXT));
    gEditMenu = MakeMenu(mEdit, "Edit", MP_EDIT_ITEMS(MP_TEXT));
    gGoodiesMenu = MakeMenu(mGoodies, "Goodies", MP_GOODIES_ITEMS(MP_TEXT));
}

/* The menu bar is the active application's: the Finder's come out while
 * MacPaint runs, and go back when it quits. */
static void InstallMenus(void) {
    ClearMenuBar();
    if (gAppleMenu) InsertMenu(gAppleMenu, 0);
    if (gFileMenu) InsertMenu(gFileMenu, 0);
    if (gEditMenu) InsertMenu(gEditMenu, 0);
    if (gGoodiesMenu) InsertMenu(gGoodiesMenu, 0);
    DrawMenuBar();
}

static void RemoveMenus(void) {
    ClearMenuBar();
    Finder_InstallMenuBar();
}

/* ------------------------------------------------------------------------
 * The document
 * ------------------------------------------------------------------------ */

static WindowPtr gWindow;
static Boolean gRunning;
static Boolean gDirty;
static Boolean gHasFile;
static char gDocName[64];
static VRefNum gDocVRef;
static DirID gDocDir;

WindowPtr MacPaint_Window(void) {
    return gWindow;
}

void MacPaint_SetDirty(void) {
    gDirty = true;
}

static void SetTitle(void) {
    Str255 t;
    c2pstrcpy(t, gDocName);
    if (gWindow) SetWTitle(gWindow, t);
}

/* Draw what has changed on the page since it was last drawn */
static void Flush(void) {
    Rect r;
    if (Page_TakeDirty(&r)) Draw_PageArea(&r);
}

static void RedrawView(void) {
    Rect all = { 0, 0, kPageH, kPageW };
    if (!gWindow) return;
    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)gWindow);
    Rect view;
    Draw_ViewRect(&view);
    EraseRect(&view);
    SetPort(save);
    Page_TakeDirty(&all);
    Draw_PageArea(&all);
}

/* ------------------------------------------------------------------------
 * Alerts
 * ------------------------------------------------------------------------ */

static DialogPtr MakeAlert(DITLBuilder* b, short height, short width) {
    Handle ditl = DITL_Finish(b);
    if (!ditl) return NULL;
    Rect bounds = { 0, 0, height, width };
    DialogPtr dlg = NewDialog(NULL, &bounds, (ConstStr255Param)"\0", false, dBoxProc,
                              (WindowPtr)-1, false, 0, ditl);
    if (!dlg) {
        DisposeHandle(ditl);
        return NULL;
    }
    CenterDialogOnScreen(dlg);
    ShowWindow((WindowPtr)dlg);
    return dlg;
}

static void Alert_(const char* message) {
    DITLBuilder b;
    if (!DITL_Begin(&b, 256)) return;
    DITL_AddButton(&b, 70, 220, 90, 290, "OK");
    DITL_AddText(&b, 14, 20, 60, 290, message);
    DialogPtr dlg = MakeAlert(&b, 104, 310);
    if (!dlg) return;
    RunModalDialogBox(dlg, 1, 1);
    DisposeDialog(dlg);
}

static void About(void) {
    DITLBuilder b;
    if (!DITL_Begin(&b, 512)) return;
    DITL_AddButton(&b, 96, 250, 116, 320, "OK");
    DITL_AddText(&b, 14, 20, 34, 320, "MacPaint");
    DITL_AddText(&b, 40, 20, 86, 320,
                 "Bill Atkinson's paint program for the Macintosh, drawing on "
                 "an 8 by 10 inch page at 72 dots an inch.");
    DialogPtr dlg = MakeAlert(&b, 130, 340);
    if (!dlg) return;
    RunModalDialogBox(dlg, 1, 1);
    DisposeDialog(dlg);
}

enum { kAnswerSave = 1, kAnswerCancel = 2, kAnswerDiscard = 3 };

static int AskSaveChanges(const char* when) {
    char message[160];
    snprintf(message, sizeof(message), "Save changes to \322%s\323 before %s?", gDocName, when);
    DITLBuilder b;
    if (!DITL_Begin(&b, 512)) return kAnswerCancel;
    DITL_AddButton(&b, 76, 250, 96, 320, "Save");
    DITL_AddButton(&b, 76, 166, 96, 236, "Cancel");
    DITL_AddButton(&b, 76, 20, 96, 110, "Don\325t Save");
    DITL_AddText(&b, 14, 20, 66, 320, message);
    DialogPtr dlg = MakeAlert(&b, 110, 340);
    if (!dlg) return kAnswerCancel;
    int hit = RunModalDialogBox(dlg, 1, 2);
    DisposeDialog(dlg);
    return hit == 1 ? kAnswerSave : hit == 3 ? kAnswerDiscard : kAnswerCancel;
}

/* ------------------------------------------------------------------------
 * Files
 * ------------------------------------------------------------------------ */

enum { kHeaderSize = 512, kFileMax = kHeaderSize + kPageH * (kPageRowBytes + 2) };
static UInt8 gFileBuf[kFileMax];
static UInt8 gLoadPage[kPageBytes];

/* PackBits, one row: a run of 2 to 128 equal bytes is a count byte of
 * 1 - n and the byte; anything else goes literally, a count of n - 1 first */
static int PackRow(const UInt8* src, int n, UInt8* dst) {
    int i = 0, o = 0;
    while (i < n) {
        int run = 1;
        while (i + run < n && run < 128 && src[i + run] == src[i]) run++;
        if (run >= 2) {
            dst[o++] = (UInt8)(1 - run);
            dst[o++] = src[i];
            i += run;
        } else {
            int start = i, len = 0;
            while (i < n && len < 128 && !(i + 1 < n && src[i + 1] == src[i])) {
                i++;
                len++;
            }
            dst[o++] = (UInt8)(len - 1);
            memcpy(&dst[o], &src[start], (size_t)len);
            o += len;
        }
    }
    return o;
}

static Boolean UnpackRow(const UInt8* src, int srcLen, int* used, UInt8* dst, int n) {
    int i = 0, o = 0;
    while (o < n) {
        if (i >= srcLen) return false;
        SInt8 h = (SInt8)src[i++];
        if (h >= 0) {
            int len = h + 1;
            if (o + len > n || i + len > srcLen) return false;
            memcpy(&dst[o], &src[i], (size_t)len);
            i += len;
            o += len;
        } else if (h != -128) {
            int len = 1 - h;
            if (o + len > n || i >= srcLen) return false;
            memset(&dst[o], src[i++], (size_t)len);
            o += len;
        }
    }
    *used = i;
    return true;
}

static Boolean WriteDocument(VRefNum vref, DirID dir, const char* name) {
    memset(gFileBuf, 0, kHeaderSize);
    gFileBuf[3] = 2;                        /* version 2: patterns included */
    memcpy(&gFileBuf[4], kPatterns, sizeof(kPatterns));
    int len = kHeaderSize;
    for (int y = 0; y < kPageH; y++)
        len += PackRow(&gPage[y * kPageRowBytes], kPageRowBytes, &gFileBuf[len]);

    /* Replace any file of the name: the file system cannot shorten one */
    CatEntry old;
    if (VFS_Lookup(vref, dir, name, &old)) {
        if (old.kind == kNodeDir || !VFS_Delete(vref, old.id)) return false;
    }
    FileID id;
    if (!VFS_CreateFile(vref, dir, name, FOURCC('P','N','T','G'), FOURCC('M','P','N','T'), &id)) return false;
    VFSFile* f = VFS_OpenFile(vref, id, false);
    if (!f) return false;
    uint32_t written = 0;
    Boolean ok = VFS_WriteFile(f, gFileBuf, (uint32_t)len, &written) && written == (uint32_t)len;
    VFS_CloseFile(f);
    return ok;
}

static Boolean ReadDocument(VRefNum vref, DirID dir, const char* name) {
    CatEntry e;
    if (!VFS_Lookup(vref, dir, name, &e)) return false;
    VFSFile* f = VFS_OpenFile(vref, e.id, false);
    if (!f) return false;
    uint32_t size = VFS_GetFileSize(f);
    if (size > sizeof(gFileBuf)) size = sizeof(gFileBuf);
    uint32_t got = 0;
    Boolean ok = VFS_ReadFile(f, gFileBuf, size, &got);
    VFS_CloseFile(f);
    if (!ok || got <= kHeaderSize) return false;

    int at = kHeaderSize;
    for (int y = 0; y < kPageH; y++) {
        int used;
        if (!UnpackRow(&gFileBuf[at], (int)got - at, &used, &gLoadPage[y * kPageRowBytes],
                       kPageRowBytes)) {
            return false;
        }
        at += used;
    }
    Tools_Reset();
    memcpy(gPage, gLoadPage, kPageBytes);
    return true;
}

static void SetDocument(VRefNum vref, DirID dir, const char* name) {
    strncpy(gDocName, name, sizeof(gDocName) - 1);
    gDocName[sizeof(gDocName) - 1] = '\0';
    gDocVRef = vref;
    gDocDir = dir;
    gHasFile = true;
    SetTitle();
}

static Boolean SaveAs(void) {
    Str255 prompt, defName;
    c2pstrcpy(prompt, "Save picture as:");
    c2pstrcpy(defName, gDocName);
    StandardFileReply reply;
    StandardPutFile(prompt, defName, &reply);
    if (!reply.sfGood || reply.sfFile.name[0] == 0) return false;

    char name[64];
    int n = reply.sfFile.name[0] < sizeof(name) - 1 ? reply.sfFile.name[0] : (int)sizeof(name) - 1;
    memcpy(name, &reply.sfFile.name[1], (size_t)n);
    name[n] = '\0';

    /* Where the dialog was looking, or the startup disk's top level */
    VolumeControlBlock vcb;
    VRefNum vref = (VRefNum)reply.sfFile.vRefNum;
    if (!VFS_GetVolumeInfo(vref, &vcb)) {
        vref = VFS_GetBootVRef();
        if (!VFS_GetVolumeInfo(vref, &vcb)) return false;
    }
    DirID dir = reply.sfFile.parID > 0 ? reply.sfFile.parID : vcb.rootID;

    if (!WriteDocument(vref, dir, name)) {
        Alert_("The picture could not be saved.");
        return false;
    }
    SetDocument(vref, dir, name);
    gDirty = false;
    return true;
}

static Boolean Save(void) {
    if (!gHasFile) return SaveAs();
    if (!WriteDocument(gDocVRef, gDocDir, gDocName)) {
        Alert_("The picture could not be saved.");
        return false;
    }
    gDirty = false;
    return true;
}

/* May the picture be put aside? Asks to save it first if it has changed */
static Boolean ConfirmDiscard(const char* when) {
    Tool_Finish();
    Flush();
    if (!gDirty) return true;
    switch (AskSaveChanges(when)) {
        case kAnswerSave:    return Save();
        case kAnswerDiscard: return true;
        default:             return false;
    }
}

static void NewDocument(void) {
    Tools_Reset();
    strcpy(gDocName, "Untitled");
    gHasFile = false;
    gDirty = false;
    gPaint.viewX = gPaint.viewY = 0;
    SetTitle();
}

static void OpenDialog(void) {
    if (!ConfirmDiscard("opening another picture")) return;
    StandardFileReply reply;
    OSType types[1] = { FOURCC('P','N','T','G') };
    StandardGetFile(NULL, 1, types, &reply);
    if (!reply.sfGood || reply.sfFile.name[0] == 0) return;

    char name[64];
    int n = reply.sfFile.name[0] < sizeof(name) - 1 ? reply.sfFile.name[0] : (int)sizeof(name) - 1;
    memcpy(name, &reply.sfFile.name[1], (size_t)n);
    name[n] = '\0';
    VolumeControlBlock vcb;
    VRefNum vref = (VRefNum)reply.sfFile.vRefNum;
    if (!VFS_GetVolumeInfo(vref, &vcb)) {
        vref = VFS_GetBootVRef();
        if (!VFS_GetVolumeInfo(vref, &vcb)) return;
    }
    DirID dir = reply.sfFile.parID > 0 ? reply.sfFile.parID : vcb.rootID;
    if (!ReadDocument(vref, dir, name)) {
        Alert_("The picture could not be opened.");
        return;
    }
    SetDocument(vref, dir, name);
    gDirty = false;
    Draw_ScrollTo(0, 0);
    RedrawView();
}

/* ------------------------------------------------------------------------
 * Commands
 * ------------------------------------------------------------------------ */

static void EnableIf(MenuHandle m, short item, Boolean on) {
    if (on) EnableItem(m, item);
    else DisableItem(m, item);
}

static void AdjustMenus(void) {
    Boolean sel = Edit_HasSelection();
    EnableIf(gFileMenu, iRevert, gHasFile && gDirty);
    EnableIf(gEditMenu, iUndo, Edit_CanUndo());
    EnableIf(gEditMenu, iCut, sel);
    EnableIf(gEditMenu, iCopy, sel);
    EnableIf(gEditMenu, iPaste, Edit_HasClipboard());
    EnableIf(gEditMenu, iClear, sel);
    EnableIf(gEditMenu, iInvert, sel);
    EnableIf(gEditMenu, iFill, sel);
    EnableIf(gEditMenu, iFlipH, sel);
    EnableIf(gEditMenu, iFlipV, sel);
    CheckItem(gGoodiesMenu, iGrid, gPaint.grid);
    CheckItem(gGoodiesMenu, iFatBits, gPaint.fatBits);
}

static void SelectTool(int tool) {
    if (tool == gPaint.tool) return;
    Tool_Finish();
    gPaint.tool = tool;
    Draw_Palettes();
}

/* FatBits on or off, keeping the middle of the view where it was */
static void SetFatBits(Boolean on) {
    if (on == gPaint.fatBits) return;
    int z = Draw_Zoom();
    int cx = gPaint.viewX + (kViewRight - kViewLeft) / z / 2;
    int cy = gPaint.viewY + (kViewBottom - kViewTop) / z / 2;
    gPaint.fatBits = on;
    z = Draw_Zoom();
    Draw_ScrollTo(cx - (kViewRight - kViewLeft) / z / 2, cy - (kViewBottom - kViewTop) / z / 2);
    RedrawView();
}

static void DoMenu(long result) {
    short menu = HiWord(result), item = LoWord(result);
    switch (menu) {
        case mApple:
            if (item == iAbout) {
                About();
            } else if (item > iAppleSep) {
                Str255 p;
                char name[256];
                GetMenuItemText(gAppleMenu, item, p);
                memcpy(name, &p[1], p[0]);
                name[p[0]] = '\0';
                OpenDeskAcc(name);
            }
            break;

        case mFile:
            switch (item) {
                case iNew:
                    if (ConfirmDiscard("starting a new picture")) {
                        NewDocument();
                        RedrawView();
                    }
                    break;
                case iOpen:   OpenDialog(); break;
                case iClose:  if (ConfirmDiscard("closing")) gRunning = false; break;
                case iSave:   Tool_Finish(); Save(); break;
                case iSaveAs: Tool_Finish(); SaveAs(); break;
                case iRevert:
                    if (gHasFile && ReadDocument(gDocVRef, gDocDir, gDocName)) {
                        gDirty = false;
                        RedrawView();
                    }
                    break;
                case iQuit:   if (ConfirmDiscard("quitting")) gRunning = false; break;
            }
            break;

        case mEdit:
            switch (item) {
                case iUndo:      Edit_Undo(); break;
                case iCut:       Edit_Cut(); break;
                case iCopy:      Edit_Copy(); break;
                case iPaste:     Edit_Paste(); break;
                case iClear:     Edit_Clear(); break;
                case iInvert:    Edit_Invert(); break;
                case iFill:      Edit_Fill(); break;
                case iFlipH:     Edit_FlipHorizontal(); break;
                case iFlipV:     Edit_FlipVertical(); break;
                case iSelectAll: Edit_SelectAll(); break;
            }
            break;

        case mGoodies:
            if (item == iGrid) gPaint.grid = !gPaint.grid;
            else if (item == iFatBits) SetFatBits(!gPaint.fatBits);
            break;
    }
    HiliteMenu(0);
    if (gRunning) {
        Draw_Palettes();        /* Paste and Select All choose the selection tool */
        Flush();
    }
}

/* ------------------------------------------------------------------------
 * Clicks
 * ------------------------------------------------------------------------ */

static UInt32 gLastClickTime;
static int gLastClickTool = -1;

/* The grabber slides the page under the view */
static void TrackGrabber(Point start) {
    int z = Draw_Zoom();
    int vx = gPaint.viewX, vy = gPaint.viewY;
    while (StillDown()) {
        Point m;
        GetMouseLocal(&m);
        int nx = vx - (m.h - start.h) / z, ny = vy - (m.v - start.v) / z;
        int ox = gPaint.viewX, oy = gPaint.viewY;
        Draw_ScrollTo(nx, ny);
        if (gPaint.viewX != ox || gPaint.viewY != oy) RedrawView();
    }
}

static void ContentClick(const EventRecord* ev) {
    SetPort((GrafPtr)gWindow);
    Point pt = ev->where;
    GlobalToLocal(&pt);

    int tool = Draw_ToolAt(pt.h, pt.v);
    if (tool >= 0) {
        /* A double-click on the eraser erases what the view shows */
        Boolean twice = tool == gLastClickTool && ev->when - gLastClickTime <= GetDblTime();
        SelectTool(tool);
        if (twice && tool == kToolEraser) Edit_EraseView();
        gLastClickTool = twice ? -1 : tool;
        gLastClickTime = ev->when;
        Flush();
        return;
    }
    gLastClickTool = -1;

    int lw = Draw_LineWidthAt(pt.h, pt.v);
    if (lw >= 0) {
        gPaint.lineWidth = lw;
        Draw_Palettes();
        return;
    }
    int pat = Draw_PatternAt(pt.h, pt.v);
    if (pat >= 0) {
        gPaint.pattern = pat;
        Draw_Palettes();
        return;
    }

    Rect view;
    Draw_ViewRect(&view);
    if (!PtInRect(pt, &view)) return;
    if (gPaint.tool == kToolGrabber) {
        TrackGrabber(pt);
        return;
    }

    int px, py;
    Draw_LocalToPage(pt.h, pt.v, &px, &py);
    Tool_Begin(px, py, ev->modifiers);
    Flush();
    while (StillDown()) {
        Point m;
        GetMouseLocal(&m);
        Draw_LocalToPage(m.h, m.v, &px, &py);
        Tool_Move(px, py);
        Flush();
    }
    Tool_End(px, py);
    Flush();
}

static void MouseDown(const EventRecord* ev) {
    WindowPtr win;
    short part = FindWindow(ev->where, &win);
    switch (part) {
        case inMenuBar:
            AdjustMenus();
            DoMenu(MenuSelect(ev->where));
            return;
        case inSysWindow:
            SystemClick(ev, (WindowRecord*)win);
            return;
    }
    if (win != gWindow) return;          /* the Finder's windows wait */
    switch (part) {
        case inGoAway:
            if (TrackGoAway(win, ev->where) && ConfirmDiscard("closing")) gRunning = false;
            break;
        case inDrag:
            DragWindow(win, ev->where, &qd.screenBits.bounds);
            break;
        case inContent:
            if (FrontWindow() != gWindow) SelectWindow(gWindow);
            else ContentClick(ev);
            break;
    }
}

static void KeyDown(const EventRecord* ev) {
    unsigned char ch = (unsigned char)(ev->message & charCodeMask);
    if (ev->modifiers & cmdKey) {
        AdjustMenus();
        DoMenu(MenuKey(ch));
        return;
    }
    WindowPtr front = FrontWindow();
    if (front && front != gWindow) {
        SystemEvent(ev);                 /* a desk accessory's */
        return;
    }
    if (!Tool_Key(ch) && (ch == 8 || ch == 127)) Edit_Clear();
    Flush();
}

static void DoEvent(EventRecord* ev) {
    switch (ev->what) {
        case mouseDown:
            MouseDown(ev);
            break;
        case keyDown:
        case autoKey:
            KeyDown(ev);
            break;
        case updateEvt:
            if ((WindowPtr)(uintptr_t)ev->message == gWindow) {
                BeginUpdate(gWindow);
                Draw_Window();
                EndUpdate(gWindow);
            } else {
                HandleUpdate(ev);
            }
            break;
    }
}

/* Between events: the polygon tool's last side follows the pointer */
static void Idle(void) {
    if (!Tool_WantsHover() || FrontWindow() != gWindow) return;
    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)gWindow);
    Point m;
    GetMouseLocal(&m);
    SetPort(save);
    Rect view;
    Draw_ViewRect(&view);
    if (!PtInRect(m, &view)) return;
    int px, py;
    Draw_LocalToPage(m.h, m.v, &px, &py);
    Tool_Hover(px, py);
    Flush();
}

/* ------------------------------------------------------------------------
 * Starting and stopping
 * ------------------------------------------------------------------------ */

static Boolean OpenWindow(void) {
    Rect screen = qd.screenBits.bounds;
    Rect r;
    r.left = (short)((screen.right - screen.left - kWinW) / 2);
    r.top = 44;
    r.right = (short)(r.left + kWinW);
    r.bottom = (short)(r.top + kWinH);
    Str255 title;
    c2pstrcpy(title, gDocName);
    gWindow = NewWindow(NULL, &r, title, false, documentProc, (WindowPtr)-1, true, 0);
    if (!gWindow) return false;
    /* The layout needs exactly this much content; the rectangle above is
     * the frame here, title bar and all */
    SizeWindow(gWindow, kWinW, kWinH, false);
    ShowWindow(gWindow);
    return true;
}

void MacPaint_OpenDocument(VRefNum vref, DirID dir, const char* name) {
    if (gRunning) return;               /* already open, and in front */

    BuildMenus();
    NewDocument();
    if (name && ReadDocument(vref, dir, name)) SetDocument(vref, dir, name);
    if (!OpenWindow()) return;
    InstallMenus();
    gRunning = true;

    while (gRunning) {
        EventRecord ev;
        if (WaitNextEvent(everyEvent, &ev, Tool_WantsHover() ? 0 : 5, NULL)) DoEvent(&ev);
        else Idle();
        Flush();
    }

    DisposeWindow(gWindow);
    gWindow = NULL;
    RemoveMenus();
}

void MacPaint_Launch(void) {
    MacPaint_OpenDocument(0, 0, NULL);
}
