/*
 * Gallery - QuickDraw the way applications of the period used it beyond
 * plain drawing: a port of its own drawing into a bitmap in memory, that
 * bitmap copied into the window - at size, scaled, combined - polygons,
 * PackBits, rectangle mapping, a font found by name, and a drawing call made
 * through the standard procedures record.
 *
 * Each panel says what it checked, so a screenshot shows what worked.
 */

#include "Toolbox.h"

enum { kAppleMenu = 128, kFileMenu = 129 };

static union { struct GrafPort port; char raw[108]; } gOff;
static BitMap gBits;
static PolyHandle gPoly;
static long gProcs[13];

static void Label(short h, short v, const char* text) {
    unsigned char s[80];
    MoveTo(h, v);
    DrawString(PStr(s, text));
}

static void LabelNum(short h, short v, const char* text, long n) {
    unsigned char s[80], num[16];
    PStr(s, text);
    NumToString(n, num);
    for (int i = 1; i <= num[0] && s[0] < 79; i++) s[++s[0]] = num[i];
    MoveTo(h, v);
    DrawString(s);
}

/* A 64 by 48 bitmap of the program's own, drawn into through a port */
static void MakeOffscreen(void) {
    gBits.rowBytes = 8;
    gBits.baseAddr = NewPtrClear(8 * 48);
    gBits.bounds.top = 0;
    gBits.bounds.left = 0;
    gBits.bounds.bottom = 48;
    gBits.bounds.right = 64;

    GrafPtr save;
    GetPort(&save);
    OpenPort(&gOff.port);
    /* Set directly, as programs did, rather than through SetPortBits */
    gOff.port.portBits.baseAddr = gBits.baseAddr;
    gOff.port.portBits.rowBytes = gBits.rowBytes;
    gOff.port.portBits.bounds = gBits.bounds;
    gOff.port.portRect = gBits.bounds;
    Rect all = gBits.bounds;
    EraseRect(&all);
    FrameRect(&all);
    Rect o = { 6, 6, 42, 42 };
    PaintOval(&o);
    MoveTo(44, 10);
    LineTo(60, 40);
    SetPort(save);
}

static void Draw(WindowPtr w) {
    Rect r = w->portRect;
    EraseRect(&r);
    TextFont(0);

    /* 1. The bitmap, copied at size, at twice its size, and XORed */
    Label(10, 16, "CopyBits from a port of its own");
    Rect src = gBits.bounds;
    Rect d1 = { 24, 10, 72, 74 };
    CopyBits(&gBits, &w->portBits, &src, &d1, srcCopy, 0);
    Rect d2 = { 24, 84, 120, 212 };
    CopyBits(&gBits, &w->portBits, &src, &d2, srcCopy, 0);
    Rect d3 = { 76, 10, 124, 74 };
    PaintRect(&d3);
    CopyBits(&gBits, &w->portBits, &src, &d3, srcXor, 0);

    /* 2. Window to window: the first copy copied again */
    Rect d4 = { 24, 222, 72, 286 };
    CopyBits(&w->portBits, &w->portBits, &d1, &d4, srcCopy, 0);

    /* 3. A polygon, framed, then moved and painted */
    Label(300, 16, "Polygons");
    FramePoly(gPoly);
    OffsetPoly(gPoly, 70, 0);
    PaintPoly(gPoly);
    OffsetPoly(gPoly, -70, 0);

    /* 4. PackBits and back */
    char raw[64], packed[80], unpacked[64];
    for (int i = 0; i < 64; i++) raw[i] = (char)(i < 20 ? 0 : i < 40 ? i : 0x55);
    Ptr s = raw, d = packed;
    PackBits(&s, &d, 64);
    long packedLen = d - packed;
    s = packed;
    d = unpacked;
    UnpackBits(&s, &d, 64);
    int same = 1;
    for (int i = 0; i < 64; i++) if (raw[i] != unpacked[i]) same = 0;
    LabelNum(10, 150, same ? "PackBits round trip OK, bytes " : "PackBits FAILED, bytes ",
             packedLen);

    /* 5. MapRect and PinRect */
    Rect from = { 0, 0, 10, 10 }, to = { 160, 300, 180, 340 }, m = { 2, 2, 8, 8 };
    MapRect(&m, &from, &to);
    FrameRect(&to);
    PaintRect(&m);
    Point far = { 500, -20 };
    long pinned = PinRect(&to, far);
    LabelNum(350, 175, "PinRect v ", pinned >> 16);

    /* 6. A font by name */
    short num = -1;
    unsigned char name[16];
    GetFNum(PStr(name, "Geneva"), &num);
    LabelNum(10, 170, "GetFNum Geneva = ", num);

    /* 7. Through the standard procedures */
    Rect pr = { 200, 10, 230, 120 };
    CallRectProc((void*)gProcs[2], 1, &pr);    /* paint */
    Label(130, 220, "StdRect through QDProcs");

    /* 8. Drawn with the origin moved, as a scrolled document is */
    SetOrigin(1000, 1000);
    Rect q = { 1205, 1300, 1215, 1400 };
    PaintRect(&q);
    Label(1300, 1235, "After SetOrigin");
    SetOrigin(0, 0);
}

MenuHandle gApple;

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

    gApple = NewMenu(kAppleMenu, PStr(s, "\x14"));
    AppendMenu(gApple, PStr(s, "About Gallery;(-"));
    InsertMenu(gApple, 0);
    MenuHandle file = NewMenu(kFileMenu, PStr(s, "File"));
    AppendMenu(file, PStr(s, "Quit/Q"));
    InsertMenu(file, 0);
    DrawMenuBar();

    Rect bounds = { 50, 20, 300, 500 };
    WindowPtr w = NewWindow(0, &bounds, PStr(s, "Gallery"), 1, documentProc,
                            (WindowPtr)-1, 1, 0);
    SetPort(w);
    MakeOffscreen();
    SetPort(w);
    gPoly = OpenPoly();
    MoveTo(300, 40);
    LineTo(350, 90);
    LineTo(300, 120);
    LineTo(330, 80);
    LineTo(300, 40);
    ClosePoly();
    SetStdProcs(gProcs);

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
    KillPoly(gPoly);
    ClosePort(&gOff.port);
}
