/*
 * Toolbox.h - the Macintosh Toolbox for the 68K test applications
 *
 * Each call is the trap itself, with the Pascal calling convention done by
 * hand: arguments pushed left to right, a function's result space pushed
 * first, Booleans in the high byte of a word, the callee popping the
 * arguments. Arguments go through registers so that no operand is addressed
 * off a stack pointer the pushes are moving.
 */

#ifndef TOOLBOX_H
#define TOOLBOX_H

typedef unsigned char Boolean;
typedef unsigned char Str255[256];
typedef const unsigned char* ConstStr255Param;
typedef char* Ptr;
typedef Ptr* Handle;
typedef struct { short v, h; } Point;
typedef struct { short top, left, bottom, right; } Rect;
typedef struct GrafPort* GrafPtr;
typedef GrafPtr WindowPtr;
typedef Handle MenuHandle;
typedef Handle RgnHandle;

typedef struct {
    unsigned short what;
    unsigned long message;
    unsigned long when;
    Point where;
    unsigned short modifiers;
} EventRecord;

/* The part of a GrafPort these programs read */
struct GrafPort {
    short device;
    struct { Ptr baseAddr; short rowBytes; Rect bounds; } portBits;
    Rect portRect;
};

/* QuickDraw's globals, which end at the address InitGraf is given */
typedef struct {
    char privates[76];
    long randSeed;
    struct { Ptr baseAddr; short rowBytes; Rect bounds; } screenBits;
    char arrow[68];
    char dkGray[8], ltGray[8], gray[8], black[8], white[8];
    GrafPtr thePort;
} QDGlobals;

enum { nullEvent, mouseDown, mouseUp, keyDown, keyUp, autoKey, updateEvt, diskEvt,
       activateEvt, osEvt = 15 };
enum { inDesk, inMenuBar, inSysWindow, inContent, inDrag, inGrow, inGoAway };
enum { cmdKey = 0x0100, everyEvent = 0xFFFF, documentProc = 0, charCodeMask = 0xFF };

#define CLOBBERS "d0", "d1", "d2", "a0", "a1", "cc", "memory"
#define TRAP(w) ".short " #w "\n\t"

static inline long PointLong(Point p) { union { Point p; long l; } u; u.p = p; return u.l; }

/* Procedures with no arguments */
#define PROC0(name, trap) \
    static inline void name(void) { __asm__ volatile (TRAP(trap) ::: CLOBBERS); }
PROC0(InitFonts, 0xA8FE)
PROC0(InitWindows, 0xA912)
PROC0(InitMenus, 0xA930)
PROC0(TEInit, 0xA9CC)
PROC0(InitCursor, 0xA850)
PROC0(DrawMenuBar, 0xA937)
PROC0(SystemTask, 0xA9B4)
PROC0(ExitToShell, 0xA9F4)
PROC0(PenNormal, 0xA89E)

/* Procedures of one long (a pointer) */
#define PROC_L(name, type, trap) \
    static inline void name(type x) { \
        __asm__ volatile ("move.l %0,-(%%sp)\n\t" TRAP(trap) :: "r"(x) : CLOBBERS); }
PROC_L(InitGraf, void*, 0xA86E)
PROC_L(InitDialogs, void*, 0xA97B)
PROC_L(SetPort, GrafPtr, 0xA873)
PROC_L(GetPort, GrafPtr*, 0xA874)
PROC_L(DisposeWindow, WindowPtr, 0xA914)
PROC_L(SelectWindow, WindowPtr, 0xA91F)
PROC_L(ShowWindow, WindowPtr, 0xA915)
PROC_L(BeginUpdate, WindowPtr, 0xA922)
PROC_L(EndUpdate, WindowPtr, 0xA923)
PROC_L(InvalRect, const Rect*, 0xA928)
PROC_L(EraseRect, const Rect*, 0xA8A3)
PROC_L(FrameRect, const Rect*, 0xA8A1)
PROC_L(PaintRect, const Rect*, 0xA8A2)
PROC_L(InvertRect, const Rect*, 0xA8A4)
PROC_L(FrameOval, const Rect*, 0xA8B7)
PROC_L(PaintOval, const Rect*, 0xA8B8)
PROC_L(DrawString, ConstStr255Param, 0xA884)
PROC_L(GlobalToLocal, Point*, 0xA871)
PROC_L(GetMouse, Point*, 0xA972)
PROC_L(SetWTitle_, void*, 0xA91A)

/* Procedures of one word */
#define PROC_W(name, trap) \
    static inline void name(short x) { \
        __asm__ volatile ("move.w %0,-(%%sp)\n\t" TRAP(trap) :: "d"(x) : CLOBBERS); }
PROC_W(TextFont, 0xA887)
PROC_W(TextSize, 0xA88A)
PROC_W(HiliteMenu, 0xA938)
PROC_W(SysBeep, 0xA9C8)
PROC_W(DrawChar, 0xA883)

/* Procedures of two words */
#define PROC_WW(name, trap) \
    static inline void name(short a, short b) { \
        __asm__ volatile ("move.w %0,-(%%sp)\n\tmove.w %1,-(%%sp)\n\t" TRAP(trap) \
                          :: "d"(a), "d"(b) : CLOBBERS); }
PROC_WW(MoveTo, 0xA893)
PROC_WW(LineTo, 0xA891)
PROC_WW(PenSize, 0xA89B)

static inline void AppendMenu(MenuHandle m, ConstStr255Param s) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA933)
                      :: "r"(m), "r"(s) : CLOBBERS);
}
static inline void AddResMenu(MenuHandle m, long type) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA94D)
                      :: "r"(m), "r"(type) : CLOBBERS);
}
static inline void InsertMenu(MenuHandle m, short before) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.w %1,-(%%sp)\n\t" TRAP(0xA935)
                      :: "r"(m), "d"(before) : CLOBBERS);
}
static inline void GetMenuItemText(MenuHandle m, short item, unsigned char* s) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      TRAP(0xA946) :: "r"(m), "d"(item), "r"(s) : CLOBBERS);
}
static inline void DragWindow(WindowPtr w, Point p, const Rect* bounds) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      TRAP(0xA925) :: "r"(w), "d"(PointLong(p)), "r"(bounds) : CLOBBERS);
}
static inline void SystemClick(EventRecord* e, WindowPtr w) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA9B3)
                      :: "r"(e), "r"(w) : CLOBBERS);
}

/* Functions */
static inline MenuHandle NewMenu(short id, ConstStr255Param title) {
    MenuHandle r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      TRAP(0xA931) "move.l (%%sp)+,%0"
                      : "=r"(r) : "d"(id), "r"(title) : CLOBBERS);
    return r;
}
static inline long MenuSelect(Point p) {
    long r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA93D) "move.l (%%sp)+,%0"
                      : "=r"(r) : "d"(PointLong(p)) : CLOBBERS);
    return r;
}
static inline long MenuKey(short ch) {
    long r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.w %1,-(%%sp)\n\t" TRAP(0xA93E) "move.l (%%sp)+,%0"
                      : "=r"(r) : "d"(ch) : CLOBBERS);
    return r;
}
static inline short OpenDeskAcc(ConstStr255Param name) {
    short r;
    __asm__ volatile ("clr.w -(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA9B6) "move.w (%%sp)+,%0"
                      : "=d"(r) : "r"(name) : CLOBBERS);
    return r;
}
static inline WindowPtr NewWindow(void* storage, const Rect* bounds, ConstStr255Param title,
                                  Boolean visible, short procID, WindowPtr behind,
                                  Boolean goAway, long refCon) {
    WindowPtr r;
    __asm__ volatile ("clr.l -(%%sp)\n\t"
                      "move.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                      "move.b %4,-(%%sp)\n\tmove.w %5,-(%%sp)\n\tmove.l %6,-(%%sp)\n\t"
                      "move.b %7,-(%%sp)\n\tmove.l %8,-(%%sp)\n\t"
                      TRAP(0xA913) "move.l (%%sp)+,%0"
                      : "=r"(r)
                      : "r"(storage), "r"(bounds), "r"(title), "d"(visible), "d"(procID),
                        "r"(behind), "d"(goAway), "r"(refCon)
                      : CLOBBERS);
    return r;
}
static inline WindowPtr FrontWindow(void) {
    WindowPtr r;
    __asm__ volatile ("clr.l -(%%sp)\n\t" TRAP(0xA924) "move.l (%%sp)+,%0" : "=r"(r) :: CLOBBERS);
    return r;
}
static inline short FindWindow(Point p, WindowPtr* w) {
    short r;
    __asm__ volatile ("clr.w -(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      TRAP(0xA92C) "move.w (%%sp)+,%0"
                      : "=d"(r) : "d"(PointLong(p)), "r"(w) : CLOBBERS);
    return r;
}
static inline Boolean TrackGoAway(WindowPtr w, Point p) {
    short r;
    __asm__ volatile ("clr.w -(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      TRAP(0xA91E) "move.w (%%sp)+,%0"
                      : "=d"(r) : "r"(w), "d"(PointLong(p)) : CLOBBERS);
    return (r >> 8) != 0;
}
static inline Boolean WaitNextEvent(short mask, EventRecord* e, long sleep, RgnHandle rgn) {
    short r;
    __asm__ volatile ("clr.w -(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      "move.l %3,-(%%sp)\n\tmove.l %4,-(%%sp)\n\t"
                      TRAP(0xA860) "move.w (%%sp)+,%0"
                      : "=d"(r) : "d"(mask), "r"(e), "r"(sleep), "r"(rgn) : CLOBBERS);
    return (r >> 8) != 0;
}
static inline Boolean StillDown(void) {
    short r;
    __asm__ volatile ("clr.w -(%%sp)\n\t" TRAP(0xA973) "move.w (%%sp)+,%0" : "=d"(r) :: CLOBBERS);
    return (r >> 8) != 0;
}
static inline unsigned long TickCount(void) {
    unsigned long r;
    __asm__ volatile ("clr.l -(%%sp)\n\t" TRAP(0xA975) "move.l (%%sp)+,%0" : "=r"(r) :: CLOBBERS);
    return r;
}

static inline void FlushEvents(short mask, short stop) {
    long d0 = ((long)stop << 16) | (unsigned short)mask;
    __asm__ volatile ("move.l %0,%%d0\n\t" TRAP(0xA032) :: "r"(d0) : CLOBBERS);
}

/* C strings into Pascal ones, for the calls that take them */
static inline const unsigned char* PStr(unsigned char* buf, const char* s) {
    int n = 0;
    while (s[n] && n < 255) { buf[n + 1] = (unsigned char)s[n]; n++; }
    buf[0] = (unsigned char)n;
    return buf;
}

#endif /* TOOLBOX_H */
