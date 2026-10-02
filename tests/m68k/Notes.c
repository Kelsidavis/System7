/*
 * Notes - a text editor the way one was written in 1990
 *
 * Menus from MBAR and MENU resources, a window from WIND, TextEdit with a
 * scroll bar that scrolls through an action procedure, Cut, Copy and Paste,
 * an About alert with ParamText and NumToString, a Find dialog from DLOG and
 * DITL, a "Save changes?" alert, and files read and written through
 * Standard File and the File Manager's parameter blocks.
 */

#include "Toolbox.h"

enum { mApple = 128, mFile = 129, mEdit = 130, mSearch = 131 };
enum { iAbout = 1 };
enum { iNew = 1, iOpen = 2, iSave = 4, iSaveAs = 5, iQuit = 7 };
enum { iUndo = 1, iCut = 3, iCopy = 4, iPaste = 5, iClear = 6 };
enum { iFind = 1, iFindAgain = 2 };
enum { kScrollW = 16, kDupFNErr = -48 };

static WindowPtr gWindow;
static TEHandle gTE;
static ControlHandle gScroll;
static Boolean gDirty, gHasFile;
static unsigned char gName[64];
static short gVRef;
static unsigned char gFind[64];

static inline TERec* TE(void) { return *(TERec**)gTE; }

static inline Ptr NewPtr(long size) {
    Ptr r;
    __asm__ volatile ("move.l %1,%%d0\n\t" TRAP(0xA11E) "move.l %%a0,%0"
                      : "=r"(r) : "r"(size) : CLOBBERS);
    return r;
}
static inline void DisposePtr(Ptr p) {
    __asm__ volatile ("move.l %0,%%a0\n\t" TRAP(0xA01F) :: "r"(p) : CLOBBERS);
}

static void CopyP(unsigned char* dst, const unsigned char* src) {
    for (int i = 0; i <= src[0]; i++) dst[i] = src[i];
}

/* ---- Scrolling ---- */

static short LinesShown(void) {
    TERec* t = TE();
    return (short)((t->viewRect.bottom - t->viewRect.top) / t->lineHeight);
}

static short TopLine(void) {
    TERec* t = TE();
    return (short)((t->viewRect.top - t->destRect.top) / t->lineHeight);
}

/* The scroll bar's range from the text, and the text where the bar says */
static void AdjustScroll(void) {
    short max = (short)(TE()->nLines - LinesShown());
    if (max < 0) max = 0;
    SetCtlMax(gScroll, max);
    if (GetCtlValue(gScroll) > max) SetCtlValue(gScroll, max);
    short want = GetCtlValue(gScroll);
    if (TopLine() != want) TEScroll(0, (short)((TopLine() - want) * TE()->lineHeight), gTE);
}

static void ScrollTo(short line) {
    short max = GetCtlMax(gScroll);
    if (line < 0) line = 0;
    if (line > max) line = max;
    SetCtlValue(gScroll, line);
    AdjustScroll();
}

/* The action procedure, called while an arrow or the paging area is held */
void ScrollAction(ControlHandle c, short part);
void ScrollAction(ControlHandle c, short part) {
    short line = GetCtlValue(c);
    switch (part) {
        case inUpButton:   line -= 1; break;
        case inDownButton: line += 1; break;
        case inPageUp:     line -= LinesShown() - 1; break;
        case inPageDown:   line += LinesShown() - 1; break;
        default: return;
    }
    ScrollTo(line);
}

/* The Control Manager calls it as a Pascal procedure - arguments left to
 * right, popped by the callee - and this is C: a stub between the two */
void ScrollActionStub(void);
__asm__(
    ".globl ScrollActionStub\n"
    "ScrollActionStub:\n"
    "    move.l  (%sp)+,%a0\n"          /* return address */
    "    move.w  (%sp)+,%d0\n"          /* part */
    "    move.l  (%sp)+,%d1\n"          /* control */
    "    move.l  %a0,-(%sp)\n"
    "    ext.l   %d0\n"
    "    move.l  %d0,-(%sp)\n"
    "    move.l  %d1,-(%sp)\n"
    "    bsr.w   ScrollAction\n"
    "    addq.l  #8,%sp\n"
    "    rts\n");

/* ---- The window ---- */

static void ViewRect(Rect* r) {
    *r = gWindow->portRect;
    r->right -= kScrollW - 1;
    r->top += 4;
    r->left += 4;
    r->bottom -= 4;
    r->right -= 4;
}

static void SetTitle(void) {
    SetWTitle(gWindow, gName);
}

static void Draw(void) {
    Rect r = gWindow->portRect;
    EraseRect(&r);
    TEUpdate(&r, gTE);
    DrawControls(gWindow);
}

/* ---- Files ---- */

static Boolean WriteDocument(void) {
    ParamBlock pb;
    char* p = (char*)&pb;
    for (unsigned i = 0; i < sizeof(pb); i++) p[i] = 0;
    pb.ioNamePtr = gName;
    pb.ioVRefNum = gVRef;
    short err = PBCreate(&pb);
    if (err && err != kDupFNErr) return 0;
    if (PBGetFInfo(&pb) == 0) {
        *(long*)(p + 32) = FOURCC('T', 'E', 'X', 'T'); /* fdType */
        *(long*)(p + 36) = FOURCC('N', 'O', 'T', 'E'); /* fdCreator */
        PBSetFInfo(&pb);
    }
    pb.ioPermssn = 3;
    if (PBOpen(&pb)) return 0;
    pb.ioMisc = 0;
    PBSetEOF(&pb);
    pb.ioBuffer = *TEGetText(gTE);
    pb.ioReqCount = TE()->teLength;
    pb.ioPosMode = 1;
    pb.ioPosOffset = 0;
    err = PBWrite(&pb);
    PBClose(&pb);
    if (err) return 0;
    gDirty = 0;
    gHasFile = 1;
    return 1;
}

static Boolean SaveAs(void) {
    unsigned char prompt[32];
    SFReply reply;
    Point where = { 100, 120 };
    SFPutFile(where, PStr(prompt, "Save note as:"), gName, 0, &reply);
    if (!reply.good) return 0;
    CopyP(gName, reply.fName);
    gVRef = reply.vRefNum;
    if (!WriteDocument()) {
        SysBeep(1);
        return 0;
    }
    SetTitle();
    return 1;
}

static Boolean Save(void) {
    if (!gHasFile) return SaveAs();
    if (WriteDocument()) return 1;
    SysBeep(1);
    return 0;
}

static void OpenDocument(void) {
    SFReply reply;
    long types[1] = { FOURCC('T', 'E', 'X', 'T') };
    Point where = { 100, 120 };
    SFGetFile(where, (ConstStr255Param)"\0", 0, 1, types, 0, &reply);
    if (!reply.good) return;
    ParamBlock pb;
    char* p = (char*)&pb;
    for (unsigned i = 0; i < sizeof(pb); i++) p[i] = 0;
    pb.ioNamePtr = reply.fName;
    pb.ioVRefNum = reply.vRefNum;
    pb.ioPermssn = 1;
    if (PBOpen(&pb)) {
        SysBeep(1);
        return;
    }
    PBGetEOF(&pb);
    long size = pb.ioMisc > 30000 ? 30000 : pb.ioMisc;
    Ptr buf = NewPtr(size ? size : 1);
    pb.ioBuffer = buf;
    pb.ioReqCount = size;
    pb.ioPosMode = 1;
    pb.ioPosOffset = 0;
    PBRead(&pb);
    PBClose(&pb);
    TESetText(buf, pb.ioActCount, gTE);
    DisposePtr(buf);
    CopyP(gName, reply.fName);
    gVRef = reply.vRefNum;
    gHasFile = 1;
    gDirty = 0;
    SetTitle();
    ScrollTo(0);
    Rect r = gWindow->portRect;
    InvalRect(&r);
}

/* Save changes first? 1 save, 2 cancel, 3 don't */
static Boolean MayDiscard(const char* when) {
    if (!gDirty) return 1;
    unsigned char w[32], empty[1] = { 0 };
    ParamText(gName, PStr(w, when), empty, empty);
    short item = CautionAlert(129, 0);
    if (item == 1) return Save();
    return item == 3;
}

/* ---- Commands ---- */

static void About(void) {
    unsigned char n[16], empty[1] = { 0 };
    NumToString(TE()->nLines, n);
    ParamText(n, empty, empty, empty);
    Alert(128, 0);
}

static void FindNext(void) {
    if (!gFind[0]) return;
    long at = Munger(TEGetText(gTE), TE()->selEnd, &gFind[1], gFind[0], 0, 0);
    if (at < 0) at = Munger(TEGetText(gTE), 0, &gFind[1], gFind[0], 0, 0);   /* round again */
    if (at < 0) {
        SysBeep(1);
        return;
    }
    TESetSelect(at, at + gFind[0], gTE);
    short line = 0;
    while (line + 1 < TE()->nLines && TE()->lineStarts[line + 1] <= at) line++;
    if (line < GetCtlValue(gScroll) || line >= GetCtlValue(gScroll) + LinesShown()) ScrollTo(line);
}

static void Find(void) {
    DialogPtr d = GetNewDialog(130, 0, (WindowPtr)-1);
    if (!d) return;
    SelIText(d, 4, 0, 32767);
    short item = 0;
    do {
        ModalDialog(0, &item);
    } while (item != 1 && item != 2);
    if (item == 1) {
        short type;
        Handle h;
        Rect box;
        GetDItem(d, 4, &type, &h, &box);
        GetIText(h, gFind);
    }
    DisposeDialog(d);
    if (item == 1) FindNext();
}

static Boolean DoMenu(long choice) {
    short menu = (short)(choice >> 16), item = (short)choice;
    Boolean quit = 0;
    if (menu == mApple && item == iAbout) {
        About();
    } else if (menu == mApple) {
        unsigned char name[256];
        GetMenuItemText(GetMHandle(mApple), item, name);
        OpenDeskAcc(name);
    } else if (menu == mFile) {
        if (item == iNew && MayDiscard("starting a new note")) {
            TESetText("", 0, gTE);
            PStr(gName, "Untitled");
            gHasFile = 0;
            gDirty = 0;
            SetTitle();
            ScrollTo(0);
            Rect r = gWindow->portRect;
            InvalRect(&r);              /* TESetText does not redraw */
        } else if (item == iOpen && MayDiscard("opening another note")) {
            OpenDocument();
        } else if (item == iSave) {
            Save();
        } else if (item == iSaveAs) {
            SaveAs();
        } else if (item == iQuit) {
            quit = MayDiscard("quitting");
        }
    } else if (menu == mEdit) {
        if (item == iCut) { TECut(gTE); gDirty = 1; }
        else if (item == iCopy) TECopy(gTE);
        else if (item == iPaste) { TEPaste(gTE); gDirty = 1; }
        else if (item == iClear) { TEDelete(gTE); gDirty = 1; }
        AdjustScroll();
    } else if (menu == mSearch) {
        if (item == iFind) Find();
        else if (item == iFindAgain) FindNext();
    }
    HiliteMenu(0);
    return quit;
}

static void ContentClick(EventRecord* e) {
    Point p = e->where;
    SetPort(gWindow);
    GlobalToLocal(&p);
    ControlHandle c;
    short part = FindControl(p, gWindow, &c);
    if (part == inThumb) {
        TrackControl(c, p, 0);
        AdjustScroll();
    } else if (part) {
        TrackControl(c, p, ScrollActionStub);
    } else {
        TEClick(p, (e->modifiers & 0x0200) != 0, gTE);
    }
}

void main(void) {
    QDGlobals qd;
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(0);
    InitCursor();
    FlushEvents(everyEvent, 0);

    SetMenuBar(GetNewMBar(128));
    AddResMenu(GetMHandle(mApple), FOURCC('D', 'R', 'V', 'R'));
    DrawMenuBar();

    gWindow = GetNewWindow(128, 0, (WindowPtr)-1);
    SetPort(gWindow);
    Rect view;
    ViewRect(&view);
    gTE = TENew(&view, &view);
    Rect bar = gWindow->portRect;
    bar.left = (short)(bar.right - kScrollW);
    bar.top -= 1;
    bar.right += 1;
    bar.bottom += 1;
    unsigned char empty[1] = { 0 };
    gScroll = NewControl(gWindow, &bar, empty, 1, 0, 0, 0, scrollBarProc, 0);
    PStr(gName, "Untitled");
    gVRef = 0;

    for (Boolean quit = 0; !quit;) {
        EventRecord e;
        WindowPtr w;
        Boolean got = WaitNextEvent(everyEvent, &e, 10, 0);
        TEIdle(gTE);
        if (!got) continue;
        switch (e.what) {
            case mouseDown:
                switch (FindWindow(e.where, &w)) {
                    case inMenuBar:   quit = DoMenu(MenuSelect(e.where)); break;
                    case inSysWindow: SystemClick(&e, w); break;
                    case inDrag:      DragWindow(w, e.where, &qd.screenBits.bounds); break;
                    case inGoAway:
                        if (TrackGoAway(w, e.where)) quit = MayDiscard("closing");
                        break;
                    case inContent:
                        if (w != FrontWindow()) SelectWindow(w);
                        else ContentClick(&e);
                        break;
                }
                break;
            case keyDown:
            case autoKey: {
                char ch = (char)(e.message & charCodeMask);
                if (e.modifiers & cmdKey) {
                    quit = DoMenu(MenuKey(ch));
                } else {
                    TEKey(ch, gTE);
                    gDirty = 1;
                    AdjustScroll();
                }
                break;
            }
            case updateEvt:
                w = (WindowPtr)e.message;
                SetPort(w);
                BeginUpdate(w);
                Draw();
                EndUpdate(w);
                break;
            case activateEvt:
                if (e.modifiers & 1) TEActivate(gTE);
                else TEDeactivate(gTE);
                break;
        }
    }
}
