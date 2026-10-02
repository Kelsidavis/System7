/*
 * Keeper - a program that keeps its data in a resource file of its own,
 * the way preferences and documents were kept: the file made if it is not
 * there, a resource read, changed and written back, others added, renamed
 * and removed. It counts its launches in the file, so the count going up
 * from one run to the next shows the file was written.
 *
 * Each check is a line in the window saying whether it held.
 */

#include "Toolbox.h"

enum { kAppleMenu = 128, kFileMenu = 129 };

static char gLines[16][56];
static int gLineCount;
static long gLaunches;

static void Note(const char* text, int ok) {
    if (gLineCount >= 16) return;
    char* d = gLines[gLineCount++];
    int n = 0;
    while (text[n] && n < 44) { d[n] = text[n]; n++; }
    const char* tail = ok ? "  OK" : "  FAILED";
    for (int i = 0; tail[i]; i++) d[n++] = tail[i];
    d[n] = 0;
}

static int Same(const unsigned char* p, const char* c) {
    int n = 0;
    while (c[n]) n++;
    if (p[0] != n) return 0;
    for (int i = 0; i < n; i++) if (p[i + 1] != (unsigned char)c[i]) return 0;
    return 1;
}

static void Run(void) {
    unsigned char name[64], s[64];
    PStr(name, "Keeper Data");

    /* Made the first time; after that it is there (dupFNErr) */
    CreateResFile(name);
    short err = ResError();
    Note(err == 0 ? "CreateResFile made the file" : "CreateResFile kept the file",
         err == 0 || err == -48);

    short ref = OpenResFile(name);
    Note("OpenResFile, and it is the current file", ref > 0 && CurResFile() == ref);
    if (ref <= 0) return;

    /* The launch count: added the first time, changed after */
    Handle count = Get1Resource(FOURCC('K', 'E', 'E', 'P'), 128);
    if (!count) {
        count = NewHandle(4);
        *(long*)*count = 1;
        AddResource(count, FOURCC('K', 'E', 'E', 'P'), 128, PStr(s, "launches"));
        Note("AddResource", ResError() == 0);
    } else {
        *(long*)*count += 1;
        ChangedResource(count);
        Note("ChangedResource", ResError() == 0);
    }
    gLaunches = *(long*)*count;
    UpdateResFile(ref);
    Note("UpdateResFile", ResError() == 0);

    /* One added under a free ID, renamed, then removed */
    short id = Unique1ID(FOURCC('T', 'M', 'P', 'x'));
    Handle tmp = NewHandle(6);
    for (int i = 0; i < 6; i++) (*tmp)[i] = (char)('a' + i);
    AddResource(tmp, FOURCC('T', 'M', 'P', 'x'), id, PStr(s, "temp"));
    Note("Unique1ID, AddResource, Count1Resources",
         ResError() == 0 && Count1Resources(FOURCC('T', 'M', 'P', 'x')) == 1);

    int found = 0;
    short types = Count1Types();
    for (short i = 1; i <= types; i++) {
        ResType t = 0;
        Get1IxType(&t, i);
        if (t == FOURCC('T', 'M', 'P', 'x')) found = 1;
    }
    Note("Count1Types and Get1IxType", types >= 2 && found);

    SetResInfo(tmp, (short)(id + 1), PStr(s, "renamed"));
    short gotID = 0;
    ResType gotType = 0;
    unsigned char gotName[256];
    GetResInfo(tmp, &gotID, &gotType, gotName);
    Note("SetResInfo, read back with GetResInfo",
         gotID == id + 1 && gotType == FOURCC('T', 'M', 'P', 'x') && Same(gotName, "renamed"));

    RmveResource(tmp);
    Note("RmveResource", ResError() == 0 && Count1Resources(FOURCC('T', 'M', 'P', 'x')) == 0);
    DisposHandle(tmp);

    CloseResFile(ref);
    Note("CloseResFile", ResError() == 0);

    /* Opened again: the count is what was written */
    ref = OpenResFile(name);
    Handle back = ref > 0 ? Get1Resource(FOURCC('K', 'E', 'E', 'P'), 128) : 0;
    Note("The count came back from the file", back && *(long*)*back == gLaunches);
    if (ref > 0) CloseResFile(ref);
}

static void Draw(WindowPtr w) {
    unsigned char s[64], num[16];
    Rect r = w->portRect;
    EraseRect(&r);
    TextFont(0);
    PStr(s, "Launched ");
    NumToString(gLaunches, num);
    for (int i = 1; i <= num[0]; i++) s[++s[0]] = num[i];
    const char* times = gLaunches == 1 ? " time" : " times";
    for (int i = 0; times[i]; i++) s[++s[0]] = (unsigned char)times[i];
    MoveTo(10, 20);
    DrawString(s);
    for (int i = 0; i < gLineCount; i++) {
        MoveTo(10, (short)(44 + 16 * i));
        DrawString(PStr(s, gLines[i]));
    }
}

void main(void) {
    QDGlobals qd;
    unsigned char s[64];

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(0);
    InitCursor();
    FlushEvents(everyEvent, 0);

    MenuHandle apple = NewMenu(kAppleMenu, PStr(s, "\x14"));
    AppendMenu(apple, PStr(s, "About Keeper;(-"));
    InsertMenu(apple, 0);
    MenuHandle file = NewMenu(kFileMenu, PStr(s, "File"));
    AppendMenu(file, PStr(s, "Quit/Q"));
    InsertMenu(file, 0);
    DrawMenuBar();

    Run();

    Rect bounds = { 60, 40, 300, 440 };
    WindowPtr w = NewWindow(0, &bounds, PStr(s, "Keeper"), 1, documentProc,
                            (WindowPtr)-1, 1, 0);
    SetPort(w);

    for (Boolean quit = 0; !quit;) {
        EventRecord e;
        WindowPtr ww;
        if (!WaitNextEvent(everyEvent, &e, 10, 0)) continue;
        switch (e.what) {
            case mouseDown:
                switch (FindWindow(e.where, &ww)) {
                    case inMenuBar: {
                        long choice = MenuSelect(e.where);
                        if ((short)(choice >> 16) == kFileMenu) quit = 1;
                        HiliteMenu(0);
                        break;
                    }
                    case inDrag:    DragWindow(ww, e.where, &qd.screenBits.bounds); break;
                    case inGoAway:  if (TrackGoAway(ww, e.where)) quit = 1; break;
                }
                break;
            case keyDown:
                if ((e.modifiers & cmdKey) && (char)(e.message & charCodeMask) == 'q') quit = 1;
                break;
            case updateEvt:
                ww = (WindowPtr)e.message;
                SetPort(ww);
                BeginUpdate(ww);
                Draw(ww);
                EndUpdate(ww);
                break;
        }
    }
}
