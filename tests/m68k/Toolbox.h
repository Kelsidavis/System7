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

/* ---- More of the Toolbox: menus from resources, TextEdit, controls,
 *      dialogs, packages, files ---- */

typedef Handle TEHandle;
typedef Handle ControlHandle;
typedef GrafPtr DialogPtr;

/* The part of a TERec these programs read (IM I-377) */
typedef struct {
    Rect destRect, viewRect, selRect;
    short lineHeight, fontAscent;
    Point selPoint;
    short selStart, selEnd, active;
    long wordBreak, clikLoop, clickTime;
    short clickLoc;
    long caretTime;
    short caretState, just, teLength;
    Handle hText;
    short recalBack, recalLines, clikStuff, crOnly, txFont;
    char txFace, filler;
    short txMode, txSize;
    GrafPtr inPort;
    long highHook, caretHook;
    short nLines;
    short lineStarts[1];
} TERec;

typedef struct {
    Boolean good, copy;
    long fType;
    short vRefNum, version;
    unsigned char fName[64];
} SFReply;

enum { scrollBarProc = 16, inUpButton = 20, inDownButton = 21, inPageUp = 22, inPageDown = 23,
       inThumb = 129 };

static inline Handle GetNewMBar(short id) {
    Handle r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.w %1,-(%%sp)\n\t" TRAP(0xA9C0) "move.l (%%sp)+,%0"
                      : "=r"(r) : "d"(id) : CLOBBERS);
    return r;
}
PROC_L(SetMenuBar, Handle, 0xA93C)
static inline MenuHandle GetMHandle(short id) {
    MenuHandle r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.w %1,-(%%sp)\n\t" TRAP(0xA949) "move.l (%%sp)+,%0"
                      : "=r"(r) : "d"(id) : CLOBBERS);
    return r;
}
#define ITEM_PROC(name, trap) \
    static inline void name(MenuHandle m, short item) { \
        __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.w %1,-(%%sp)\n\t" TRAP(trap) \
                          :: "r"(m), "d"(item) : CLOBBERS); }
ITEM_PROC(EnableItem, 0xA939)
ITEM_PROC(DisableItem, 0xA93A)

static inline WindowPtr GetNewWindow(short id, void* storage, WindowPtr behind) {
    WindowPtr r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                      TRAP(0xA9BD) "move.l (%%sp)+,%0"
                      : "=r"(r) : "d"(id), "r"(storage), "r"(behind) : CLOBBERS);
    return r;
}

/* TextEdit */
static inline TEHandle TENew(const Rect* dest, const Rect* view) {
    TEHandle r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      TRAP(0xA9D2) "move.l (%%sp)+,%0"
                      : "=r"(r) : "r"(dest), "r"(view) : CLOBBERS);
    return r;
}
PROC_L(TEIdle, TEHandle, 0xA9DA)
PROC_L(TECut, TEHandle, 0xA9D6)
PROC_L(TECopy, TEHandle, 0xA9D5)
PROC_L(TEPaste, TEHandle, 0xA9DB)
PROC_L(TEDelete, TEHandle, 0xA9D7)
PROC_L(TEActivate, TEHandle, 0xA9D8)
PROC_L(TEDeactivate, TEHandle, 0xA9D9)
PROC_L(TECalText, TEHandle, 0xA9D0)
static inline void TEKey(short ch, TEHandle te) {
    __asm__ volatile ("move.w %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA9DC)
                      :: "d"(ch), "r"(te) : CLOBBERS);
}
static inline void TEClick(Point p, Boolean extend, TEHandle te) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.b %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t" TRAP(0xA9D4)
                      :: "d"(PointLong(p)), "d"(extend), "r"(te) : CLOBBERS);
}
static inline void TEUpdate(const Rect* r, TEHandle te) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA9D3)
                      :: "r"(r), "r"(te) : CLOBBERS);
}
static inline void TESetText(const void* text, long len, TEHandle te) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t" TRAP(0xA9CF)
                      :: "r"(text), "r"(len), "r"(te) : CLOBBERS);
}
static inline Handle TEGetText(TEHandle te) {
    Handle r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA9CB) "move.l (%%sp)+,%0"
                      : "=r"(r) : "r"(te) : CLOBBERS);
    return r;
}
static inline void TESetSelect(long start, long end, TEHandle te) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t" TRAP(0xA9D1)
                      :: "r"(start), "r"(end), "r"(te) : CLOBBERS);
}
static inline void TEScroll(short dh, short dv, TEHandle te) {
    __asm__ volatile ("move.w %0,-(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t" TRAP(0xA9DD)
                      :: "d"(dh), "d"(dv), "r"(te) : CLOBBERS);
}

/* Controls */
static inline ControlHandle NewControl(WindowPtr w, const Rect* r, ConstStr255Param title,
                                       Boolean visible, short value, short min, short max,
                                       short procID, long refCon) {
    ControlHandle c;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                      "move.b %4,-(%%sp)\n\tmove.w %5,-(%%sp)\n\tmove.w %6,-(%%sp)\n\t"
                      "move.w %7,-(%%sp)\n\tmove.w %8,-(%%sp)\n\tmove.l %9,-(%%sp)\n\t"
                      TRAP(0xA954) "move.l (%%sp)+,%0"
                      : "=r"(c)
                      : "r"(w), "r"(r), "r"(title), "d"(visible), "d"(value), "d"(min), "d"(max),
                        "d"(procID), "r"(refCon)
                      : CLOBBERS);
    return c;
}
#define CTL_SET(name, trap) \
    static inline void name(ControlHandle c, short v) { \
        __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.w %1,-(%%sp)\n\t" TRAP(trap) \
                          :: "r"(c), "d"(v) : CLOBBERS); }
CTL_SET(SetCtlValue, 0xA963)
CTL_SET(SetCtlMax, 0xA965)
#define CTL_GET(name, trap) \
    static inline short name(ControlHandle c) { short r; \
        __asm__ volatile ("clr.w -(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(trap) "move.w (%%sp)+,%0" \
                          : "=d"(r) : "r"(c) : CLOBBERS); return r; }
CTL_GET(GetCtlValue, 0xA960)
CTL_GET(GetCtlMax, 0xA962)
PROC_L(DrawControls, WindowPtr, 0xA969)
static inline short FindControl(Point p, WindowPtr w, ControlHandle* c) {
    short r;
    __asm__ volatile ("clr.w -(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                      TRAP(0xA96C) "move.w (%%sp)+,%0"
                      : "=d"(r) : "d"(PointLong(p)), "r"(w), "r"(c) : CLOBBERS);
    return r;
}
static inline short TrackControl(ControlHandle c, Point p, void* action) {
    short r;
    __asm__ volatile ("clr.w -(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                      TRAP(0xA968) "move.w (%%sp)+,%0"
                      : "=d"(r) : "r"(c), "d"(PointLong(p)), "r"(action) : CLOBBERS);
    return r;
}

/* Dialogs and alerts */
#define ALERT(name, trap) \
    static inline short name(short id, void* filter) { short r; \
        __asm__ volatile ("clr.w -(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t" TRAP(trap) \
                          "move.w (%%sp)+,%0" : "=d"(r) : "d"(id), "r"(filter) : CLOBBERS); return r; }
ALERT(Alert, 0xA985)
ALERT(CautionAlert, 0xA988)
static inline void ParamText(ConstStr255Param a, ConstStr255Param b, ConstStr255Param c,
                             ConstStr255Param d) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                      TRAP(0xA98B) :: "r"(a), "r"(b), "r"(c), "r"(d) : CLOBBERS);
}
static inline DialogPtr GetNewDialog(short id, void* storage, WindowPtr behind) {
    DialogPtr r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                      TRAP(0xA97C) "move.l (%%sp)+,%0"
                      : "=r"(r) : "d"(id), "r"(storage), "r"(behind) : CLOBBERS);
    return r;
}
static inline void ModalDialog(void* filter, short* item) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA991)
                      :: "r"(filter), "r"(item) : CLOBBERS);
}
PROC_L(DisposeDialog, DialogPtr, 0xA983)
static inline void GetDItem(DialogPtr d, short item, short* type, Handle* h, Rect* box) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      "move.l %3,-(%%sp)\n\tmove.l %4,-(%%sp)\n\t" TRAP(0xA98D)
                      :: "r"(d), "d"(item), "r"(type), "r"(h), "r"(box) : CLOBBERS);
}
static inline void GetIText(Handle h, unsigned char* text) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA990)
                      :: "r"(h), "r"(text) : CLOBBERS);
}
static inline void SelIText(DialogPtr d, short item, short start, short end) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.w %2,-(%%sp)\n\tmove.w %3,-(%%sp)\n\t"
                      TRAP(0xA97E) :: "r"(d), "d"(item), "d"(start), "d"(end) : CLOBBERS);
}

/* Packages */
static inline void NumToString(long n, unsigned char* s) {
    __asm__ volatile ("move.l %0,%%d0\n\tmove.l %1,%%a0\n\tclr.w -(%%sp)\n\t" TRAP(0xA9EE)
                      :: "r"(n), "r"(s) : CLOBBERS);
}
static inline void SFGetFile(Point where, ConstStr255Param prompt, void* filter, short numTypes,
                             const long* types, void* hook, SFReply* reply) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      "move.w %3,-(%%sp)\n\tmove.l %4,-(%%sp)\n\tmove.l %5,-(%%sp)\n\t"
                      "move.l %6,-(%%sp)\n\tmove.w #2,-(%%sp)\n\t" TRAP(0xA9EA)
                      :: "d"(PointLong(where)), "r"(prompt), "r"(filter), "d"(numTypes),
                         "r"(types), "r"(hook), "r"(reply) : CLOBBERS);
}
static inline void SFPutFile(Point where, ConstStr255Param prompt, ConstStr255Param orig,
                             void* hook, SFReply* reply) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      "move.l %3,-(%%sp)\n\tmove.l %4,-(%%sp)\n\tmove.w #1,-(%%sp)\n\t" TRAP(0xA9EA)
                      :: "d"(PointLong(where)), "r"(prompt), "r"(orig), "r"(hook), "r"(reply)
                      : CLOBBERS);
}

/* Memory */
static inline long GetHandleSize(Handle h) {
    long r;
    __asm__ volatile ("move.l %1,%%a0\n\t" TRAP(0xA025) "move.l %%d0,%0"
                      : "=r"(r) : "r"(h) : CLOBBERS);
    return r;
}
static inline long Munger(Handle h, long offset, const void* p1, long l1, const void* p2, long l2) {
    long r;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                      "move.l %4,-(%%sp)\n\tmove.l %5,-(%%sp)\n\tmove.l %6,-(%%sp)\n\t"
                      TRAP(0xA9E0) "move.l (%%sp)+,%0"
                      : "=r"(r) : "r"(h), "r"(offset), "r"(p1), "r"(l1), "r"(p2), "r"(l2) : CLOBBERS);
    return r;
}
static inline void SetWTitle(WindowPtr w, ConstStr255Param s) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA91A)
                      :: "r"(w), "r"(s) : CLOBBERS);
}

/* The File Manager, through parameter blocks (IM IV-115) */
typedef struct {
    long qLink;
    short qType, ioTrap;
    long ioCmdAddr, ioCompletion;
    short ioResult;
    const unsigned char* ioNamePtr;
    short ioVRefNum, ioRefNum;
    char ioVersNum, ioPermssn;
    long ioMisc;
    void* ioBuffer;
    long ioReqCount, ioActCount;
    short ioPosMode;
    long ioPosOffset;
    long finderInfo[8];         /* room for the FileParam fields */
} ParamBlock;

#define PB_CALL(name, trap) \
    static inline short name(ParamBlock* pb) { short r; \
        __asm__ volatile ("move.l %1,%%a0\n\t" TRAP(trap) "move.w %%d0,%0" \
                          : "=d"(r) : "r"(pb) : CLOBBERS); return r; }
PB_CALL(PBOpen, 0xA000)
PB_CALL(PBClose, 0xA001)
PB_CALL(PBRead, 0xA002)
PB_CALL(PBWrite, 0xA003)
PB_CALL(PBCreate, 0xA008)
PB_CALL(PBGetFInfo, 0xA00C)
PB_CALL(PBSetFInfo, 0xA00D)
PB_CALL(PBGetEOF, 0xA011)
PB_CALL(PBSetEOF, 0xA012)


/* ---- QuickDraw: ports of one's own, CopyBits, polygons, mapping ---- */

typedef struct { Ptr baseAddr; short rowBytes; Rect bounds; } BitMap;
typedef Handle PolyHandle;
enum { srcCopy, srcOr, srcXor, srcBic };

PROC_L(OpenPort, GrafPtr, 0xA86F)
PROC_L(ClosePort, GrafPtr, 0xA87D)
PROC_L(SetPortBits, const BitMap*, 0xA875)
PROC_WW(PortSize, 0xA876)
PROC0(ClosePoly, 0xA8CC)
PROC_L(KillPoly, PolyHandle, 0xA8CD)
PROC_L(FramePoly, PolyHandle, 0xA8C6)
PROC_L(PaintPoly, PolyHandle, 0xA8C7)
PROC_L(InvertPoly, PolyHandle, 0xA8C9)
PROC_L(SetStdProcs, void*, 0xA8EA)

static inline Ptr NewPtrClear(long size) {
    Ptr r;
    __asm__ volatile ("move.l %1,%%d0\n\t" TRAP(0xA31E) "move.l %%a0,%0"
                      : "=r"(r) : "r"(size) : CLOBBERS);
    return r;
}
static inline void CopyBits(const void* src, const void* dst, const Rect* sr, const Rect* dr,
                            short mode, RgnHandle mask) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      "move.l %3,-(%%sp)\n\tmove.w %4,-(%%sp)\n\tmove.l %5,-(%%sp)\n\t"
                      TRAP(0xA8EC)
                      :: "r"(src), "r"(dst), "r"(sr), "r"(dr), "d"(mode), "r"(mask) : CLOBBERS);
}
static inline PolyHandle OpenPoly(void) {
    PolyHandle r;
    __asm__ volatile ("clr.l -(%%sp)\n\t" TRAP(0xA8CB) "move.l (%%sp)+,%0" : "=r"(r) :: CLOBBERS);
    return r;
}
static inline void OffsetPoly(PolyHandle p, short dh, short dv) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.w %1,-(%%sp)\n\tmove.w %2,-(%%sp)\n\t"
                      TRAP(0xA8CE) :: "r"(p), "d"(dh), "d"(dv) : CLOBBERS);
}
static inline void PackBits(Ptr* src, Ptr* dst, short n) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.w %2,-(%%sp)\n\t"
                      TRAP(0xA8CF) :: "r"(src), "r"(dst), "d"(n) : CLOBBERS);
}
static inline void UnpackBits(Ptr* src, Ptr* dst, short n) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.w %2,-(%%sp)\n\t"
                      TRAP(0xA8D0) :: "r"(src), "r"(dst), "d"(n) : CLOBBERS);
}
static inline void MapRect(Rect* r, const Rect* src, const Rect* dst) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      TRAP(0xA8FA) :: "r"(r), "r"(src), "r"(dst) : CLOBBERS);
}
static inline long PinRect(const Rect* r, Point p) {
    long v;
    __asm__ volatile ("clr.l -(%%sp)\n\tmove.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\t"
                      TRAP(0xA94E) "move.l (%%sp)+,%0" : "=r"(v) : "r"(r), "d"(PointLong(p)) : CLOBBERS);
    return v;
}
static inline void GetFNum(ConstStr255Param name, short* num) {
    __asm__ volatile ("move.l %0,-(%%sp)\n\tmove.l %1,-(%%sp)\n\t" TRAP(0xA900)
                      :: "r"(name), "r"(num) : CLOBBERS);
}
/* A QDProcs record's rectProc, called as QuickDraw would */
static inline void CallRectProc(void* proc, unsigned char verb, const Rect* r) {
    __asm__ volatile ("move.b %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %0,%%a0\n\tjsr (%%a0)"
                      :: "r"(proc), "d"(verb), "r"(r) : CLOBBERS);
}

/* C strings into Pascal ones, for the calls that take them */
static inline const unsigned char* PStr(unsigned char* buf, const char* s) {
    int n = 0;
    while (s[n] && n < 255) { buf[n + 1] = (unsigned char)s[n]; n++; }
    buf[0] = (unsigned char)n;
    return buf;
}

#endif /* TOOLBOX_H */
