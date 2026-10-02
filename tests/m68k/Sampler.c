/*
 * Sampler - an application the way one was written in 1990: menus, a
 * window, an event loop, drawing in response to update events.
 *
 * Each thing it does is something a real application does, so each is
 * something the system has to answer for one to run.
 */

#include "Toolbox.h"

enum { kAppleMenu = 128, kFileMenu = 129 };
enum { kNewItem = 1, kCloseItem = 2, kQuitItem = 4 };

static void Draw(WindowPtr w) {
    unsigned char s[64];
    Rect r = w->portRect;
    EraseRect(&r);
    TextFont(0);
    MoveTo(20, 30);
    DrawString(PStr(s, "Hello from a 68K application."));
    TextFont(3);
    TextSize(12);
    MoveTo(20, 55);
    DrawString(PStr(s, "Click in the window to draw."));
    Rect box = { 80, 20, 160, 140 };
    FrameRect(&box);
    Rect oval = { 80, 160, 160, 280 };
    PaintOval(&oval);
    PenSize(3, 3);
    MoveTo(300, 80);
    LineTo(380, 160);
    PenNormal();
}

static WindowPtr NewDocument(void) {
    static short count;
    unsigned char title[32];
    Rect bounds = { 60, 40, 300, 440 };
    bounds.top += 20 * count;
    bounds.left += 20 * count;
    bounds.bottom += 20 * count;
    bounds.right += 20 * count;
    count = (short)((count + 1) % 6);
    return NewWindow(0, &bounds, PStr(title, "Sampler"), 1, documentProc,
                     (WindowPtr)-1, 1, 0);
}

static Boolean DoMenu(long choice) {
    short menu = (short)(choice >> 16), item = (short)choice;
    Boolean quit = 0;
    if (menu == kAppleMenu && item > 2) {
        unsigned char name[256];
        extern MenuHandle gApple;
        GetMenuItemText(gApple, item, name);
        OpenDeskAcc(name);
    } else if (menu == kFileMenu) {
        if (item == kNewItem) {
            NewDocument();
        } else if (item == kCloseItem) {
            WindowPtr w = FrontWindow();
            if (w) DisposeWindow(w);
        } else if (item == kQuitItem) {
            quit = 1;
        }
    }
    HiliteMenu(0);
    return quit;
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
    AppendMenu(gApple, PStr(s, "About Sampler;(-"));
    AddResMenu(gApple, FOURCC('D', 'R', 'V', 'R'));
    InsertMenu(gApple, 0);
    MenuHandle file = NewMenu(kFileMenu, PStr(s, "File"));
    AppendMenu(file, PStr(s, "New/N;Close/W;(-;Quit/Q"));
    InsertMenu(file, 0);
    DrawMenuBar();

    NewDocument();

    for (Boolean quit = 0; !quit;) {
        EventRecord e;
        WindowPtr w;
        if (!WaitNextEvent(everyEvent, &e, 10, 0)) continue;
        switch (e.what) {
            case mouseDown:
                switch (FindWindow(e.where, &w)) {
                    case inMenuBar:   quit = DoMenu(MenuSelect(e.where)); break;
                    case inSysWindow: SystemClick(&e, w); break;
                    case inDrag:      DragWindow(w, e.where, &qd.screenBits.bounds); break;
                    case inGoAway:    if (TrackGoAway(w, e.where)) DisposeWindow(w); break;
                    case inContent:
                        if (w != FrontWindow()) {
                            SelectWindow(w);
                        } else {
                            Point p = e.where;
                            SetPort(w);
                            GlobalToLocal(&p);
                            Rect dot = { (short)(p.v - 3), (short)(p.h - 3),
                                         (short)(p.v + 3), (short)(p.h + 3) };
                            PaintRect(&dot);
                        }
                        break;
                }
                break;
            case keyDown:
            case autoKey:
                if (e.modifiers & cmdKey) quit = DoMenu(MenuKey((short)(e.message & charCodeMask)));
                break;
            case updateEvt:
                w = (WindowPtr)e.message;
                SetPort(w);
                BeginUpdate(w);
                Draw(w);
                EndUpdate(w);
                break;
        }
    }
}
