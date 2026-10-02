/*
 * M68KToolboxTest.c - traps called by a 68K program, for the integration tests
 *
 * Built only with INTEGRATION_TESTS=1. The program is assembled here by hand
 * - there is no 68K compiler in the kernel build - and run the way an
 * application runs: M68KToolbox_Prepare's traps, entered with kEnterApp,
 * until it stops. What the traps left in its memory is then checked.
 *
 * Here rather than in IntegrationTests.c because SegmentLoader.h, which a
 * context needs, cannot be included beside that file's File Manager
 * declarations.
 */

#include <string.h>
#include "CPU/M68KToolbox.h"
#include "CPU/M68KInterp.h"
#include "M68KToolboxInternal.h"
#include "ResourceManager.h"

/* Called by IntegrationTests.c */
Boolean M68KToolbox_RunTrapTest(const char** why);
Boolean M68KToolbox_RunSANETest(const char** why);
Boolean M68KToolbox_RunListTest(const char** why);

enum { kAsmWords = 512 };           /* the code area's size, in words */
typedef struct {
    UInt16 words[kAsmWords];
    int n;                          /* past kAsmWords: the program did not fit */
} Asm;

static void W(Asm* a, UInt16 w) { if (a->n < kAsmWords) a->words[a->n] = w; a->n++; }
static void L(Asm* a, UInt32 l) { W(a, (UInt16)(l >> 16)); W(a, (UInt16)l); }

/* The data the program works on, at fixed offsets from 'data' */
enum {
    kHdr = 0x00,        /* QHdr: qFlags, qHead, qTail */
    kE1 = 0x10, kE2 = 0x20, kE3 = 0x30,
    kDequeued = 0x40, kDequeuedAgain = 0x42, kOSAvail = 0x44,
    kSPBefore = 0x48, kSPAfter = 0x4C,
    kName = 0x50,       /* Str255 */
    kRefNum = 0x150, kParam = 0x154,
    kEvent = 0x160,
    kDataSize = 0x200
};

/* A program's world: its memory, prepared as a launch prepares it */
typedef struct {
    const ICPUBackend* be;
    CPUAddressSpace cas;
    CPUAddr code, data, stack;
    SInt16 refNum;
} World;

enum { kStack = 16 * 1024, kCodeSize = kAsmWords * 2 };

static Boolean WorldBegin(World* w, const char** why) {
    memset(w, 0, sizeof(*w));
    w->be = CPUBackend_Get("m68k_interp");
    *why = "no 68K backend";
    if (!w->be || w->be->CreateAddressSpace(NULL, &w->cas) != noErr) return false;

    SegmentLoaderContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cpuAS = w->cas;
    ctx.cpuBackend = w->be;
    Boolean ok = w->be->AllocateMemory(w->cas, kCodeSize, kCPUMapA5World, &w->code) == noErr &&
                 w->be->AllocateMemory(w->cas, kDataSize, kCPUMapA5World, &w->data) == noErr &&
                 w->be->AllocateMemory(w->cas, kStack, kCPUMapA5World, &w->stack) == noErr &&
                 w->be->SetStacks(w->cas, w->stack + kStack, 0) == noErr;
    static const UInt8 kAppName[] = "\x05ITest";
    w->refNum = CurResFile();
    if (ok) ok = M68KToolbox_Prepare(&ctx, kAppName, w->refNum, w->stack, w->stack + kStack, 0, 0) == noErr;
    if (!ok) {
        *why = "could not prepare the program";
        if (gM68KApp) M68KToolbox_Finish();
        w->be->DestroyAddressSpace(w->cas);
        return false;
    }
    for (UInt32 i = 0; i < kDataSize; i += 4) M68K_Write32(gM68KApp, w->data + i, 0xDEADBEEF);
    return true;
}

static OSErr WorldRun(World* w, const Asm* a) {
    if (a->n > kAsmWords) return paramErr;          /* not all of it was kept */
    for (int i = 0; i < a->n; i++) M68K_Write16(gM68KApp, w->code + 2 * (UInt32)i, a->words[i]);
    return w->be->EnterAt(w->cas, w->code, kEnterApp);
}

static void WorldEnd(World* w) {
    M68KToolbox_Finish();
    w->be->DestroyAddressSpace(w->cas);
}

Boolean M68KToolbox_RunTrapTest(const char** why)
{
    World w;
    if (!WorldBegin(&w, why)) return false;
    CPUAddr code = w.code, data = w.data;
    SInt16 refNum = w.refNum;

    Asm a;
    a.n = 0;
    W(&a, 0x23CF); L(&a, data + kSPBefore);             /* MOVE.L A7,spBefore */
    /* Three into the queue, the middle one out, then out again */
    W(&a, 0x227C); L(&a, data + kHdr);                  /* MOVEA.L #hdr,A1 */
    M68K_Write32(gM68KApp, data + kHdr + 2, 0);         /* an empty queue */
    M68K_Write32(gM68KApp, data + kHdr + 6, 0);
    W(&a, 0x207C); L(&a, data + kE1); W(&a, 0xA96F);    /* _Enqueue e1 */
    W(&a, 0x207C); L(&a, data + kE2); W(&a, 0xA96F);    /* _Enqueue e2 */
    W(&a, 0x207C); L(&a, data + kE3); W(&a, 0xA96F);    /* _Enqueue e3 */
    W(&a, 0x207C); L(&a, data + kE2); W(&a, 0xA96E);    /* _Dequeue e2 */
    W(&a, 0x33C0); L(&a, data + kDequeued);             /* MOVE.W D0,dequeued */
    W(&a, 0xA96E);                                      /* _Dequeue e2 again */
    W(&a, 0x33C0); L(&a, data + kDequeuedAgain);
    /* GetAppParms(name, refNum, param) */
    W(&a, 0x4879); L(&a, data + kName);                 /* PEA name */
    W(&a, 0x4879); L(&a, data + kRefNum);
    W(&a, 0x4879); L(&a, data + kParam);
    W(&a, 0xA9F5);
    /* Calls with nothing to do must still take their arguments */
    W(&a, 0x4879); L(&a, code);                         /* PEA: UnloadSeg(code) */
    W(&a, 0xA9F1);
    W(&a, 0xA9E6);                                      /* _InitAllPacks */
    W(&a, 0x3F3C); W(&a, 4); W(&a, 0xA9E5);             /* InitPack(4) */
    W(&a, 0xA0BD);                                      /* _FlushCodeCache */
    W(&a, 0x23CF); L(&a, data + kSPAfter);              /* MOVE.L A7,spAfter */
    /* OSEventAvail for no kind of event: a null one */
    W(&a, 0x7000);                                      /* MOVEQ #0,D0 */
    W(&a, 0x207C); L(&a, data + kEvent); W(&a, 0xA030);
    W(&a, 0x33C0); L(&a, data + kOSAvail);
    /* And stop the way a program in trouble does */
    W(&a, 0x303C); W(&a, 28); W(&a, 0xA9C9);            /* MOVE.W #28,D0; _SysError */
    W(&a, 0xA9F4);                                      /* _ExitToShell, if it did not */
    M68KAddressSpace* as = gM68KApp;
    OSErr ran = WorldRun(&w, &a);
    const char* fault = as->faultReason;

    UInt32 hdr = data + kHdr;
    Boolean queued = M68K_Read32(as, hdr + 2) == data + kE1 &&
                     M68K_Read32(as, hdr + 6) == data + kE3 &&
                     M68K_Read32(as, data + kE1) == data + kE3 &&
                     M68K_Read32(as, data + kE3) == 0;
    Boolean dequeued = M68K_Read16(as, data + kDequeued) == 0 &&
                       M68K_Read16(as, data + kDequeuedAgain) == 0xFFFF;
    Boolean named = M68K_Read8(as, data + kName) == 5 &&
                    M68K_Read8(as, data + kName + 1) == 'I' &&
                    M68K_Read8(as, data + kName + 5) == 't';
    Boolean refOK = (SInt16)M68K_Read16(as, data + kRefNum) == refNum;
    UInt32 parms = M68K_Read32(as, data + kParam);
    Boolean parmsOK = parms != 0 && parms == M68K_Read32(as, 0x0AEC) &&
                      M68K_Read32(as, M68KHeap_Deref(parms)) == 0;   /* appOpen, no files */
    Boolean stackOK = M68K_Read32(as, data + kSPBefore) == M68K_Read32(as, data + kSPAfter);
    Boolean nullOK = M68K_Read16(as, data + kOSAvail) == 0xFFFF;
    Boolean sysErr = ran != noErr && fault && strcmp(fault, "system error 28") == 0;

    WorldEnd(&w);

    if (!queued)   { *why = "Enqueue did not link the elements in order"; return false; }
    if (!dequeued) { *why = "Dequeue did not answer noErr then qErr"; return false; }
    if (!named || !refOK) { *why = "GetAppParms did not give the name and refNum"; return false; }
    if (!parmsOK)  { *why = "AppParmHandle not an empty appOpen message"; return false; }
    if (!stackOK)  { *why = "a trap did not take its arguments off the stack"; return false; }
    if (!nullOK)   { *why = "OSEventAvail with no mask did not answer -1"; return false; }
    if (!sysErr)   { *why = "SysError did not stop the program with its number"; return false; }
    *why = "";
    return true;
}

/* ------------------------------------------------------------------------
 * SANE: _FP68K, and _Pack7's decimal formatter
 * ------------------------------------------------------------------------ */

enum {
    kOne = 0x00, kThree = 0x10, kTwo = 0x20,    /* extendeds */
    kAsDouble = 0x30,
    kForm = 0x38,                               /* decform: floating, 10 digits */
    kDec = 0x40,                                /* decimal record */
    kText = 0x60,                               /* DecStr */
    kCCR = 0xB8
};

static void PutExt(UInt32 a, UInt16 se, UInt32 hi, UInt32 lo) {
    M68K_Write16(gM68KApp, a, se);
    M68K_Write32(gM68KApp, a + 2, hi);
    M68K_Write32(gM68KApp, a + 6, lo);
}

static Boolean ExtIs(UInt32 a, UInt16 se, UInt32 hi, UInt32 lo) {
    return M68K_Read16(gM68KApp, a) == se && M68K_Read32(gM68KApp, a + 2) == hi &&
           M68K_Read32(gM68KApp, a + 6) == lo;
}

Boolean M68KToolbox_RunSANETest(const char** why)
{
    World w;
    if (!WorldBegin(&w, why)) return false;
    CPUAddr data = w.data;
    PutExt(data + kOne, 0x3FFF, 0x80000000, 0);
    PutExt(data + kThree, 0x4000, 0xC0000000, 0);
    PutExt(data + kTwo, 0x4000, 0x80000000, 0);
    M68K_Write16(gM68KApp, data + kForm, 0);            /* FLOATDECIMAL */
    M68K_Write16(gM68KApp, data + kForm + 2, 10);

    Asm a;
    a.n = 0;
    /* one := one / three (FDIVX) */
    W(&a, 0x4879); L(&a, data + kThree);
    W(&a, 0x4879); L(&a, data + kOne);
    W(&a, 0x3F3C); W(&a, 0x0006); W(&a, 0xA9EB);
    /* asDouble := one (FX2D) */
    W(&a, 0x4879); L(&a, data + kOne);
    W(&a, 0x4879); L(&a, data + kAsDouble);
    W(&a, 0x3F3C); W(&a, 0x0810); W(&a, 0xA9EB);
    /* compare one with asDouble (FCMPD), and keep the condition codes */
    W(&a, 0x4879); L(&a, data + kAsDouble);
    W(&a, 0x4879); L(&a, data + kOne);
    W(&a, 0x3F3C); W(&a, 0x0808); W(&a, 0xA9EB);
    W(&a, 0x40C1);                                      /* MOVE SR,D1 */
    W(&a, 0x33C1); L(&a, data + kCCR);                  /* MOVE.W D1,ccr */
    /* two := sqrt(two) (FSQRTX) */
    W(&a, 0x4879); L(&a, data + kTwo);
    W(&a, 0x3F3C); W(&a, 0x0012); W(&a, 0xA9EB);
    /* dec := one, by form (FX2DEC) */
    W(&a, 0x4879); L(&a, data + kForm);
    W(&a, 0x4879); L(&a, data + kOne);
    W(&a, 0x4879); L(&a, data + kDec);
    W(&a, 0x3F3C); W(&a, 0x000B); W(&a, 0xA9EB);
    /* Dec2Str(form, dec, text): the form itself on the stack */
    W(&a, 0x2F3C); L(&a, 10);                           /* MOVE.L #$0000000A,-(SP) */
    W(&a, 0x4879); L(&a, data + kDec);
    W(&a, 0x4879); L(&a, data + kText);
    W(&a, 0x3F3C); W(&a, 3); W(&a, 0xA9EE);
    W(&a, 0xA9F4);

    OSErr ran = WorldRun(&w, &a);
    M68KAddressSpace* as = gM68KApp;
    Boolean third = ExtIs(data + kOne, 0x3FFD, 0xAAAAAAAA, 0xAAAAAAAB);
    Boolean dbl = M68K_Read32(as, data + kAsDouble) == 0x3FD55555 &&
                  M68K_Read32(as, data + kAsDouble + 4) == 0x55555555;
    Boolean greater = (M68K_Read16(as, data + kCCR) & 0x1F) == 0;
    Boolean root = ExtIs(data + kTwo, 0x3FFF, 0xB504F333, 0xF9DE6484);
    static const char kWant[] = " 3.333333333e-1";
    Boolean text = M68K_Read8(as, data + kText) == sizeof(kWant) - 1;
    for (UInt32 i = 0; text && i < sizeof(kWant) - 1; i++)
        text = M68K_Read8(as, data + kText + 1 + i) == (UInt8)kWant[i];
    WorldEnd(&w);

    if (ran != noErr) { *why = "the program stopped with a fault"; return false; }
    if (!third)   { *why = "1/3 in extended is not 3FFD AAAAAAAAAAAAAAAB"; return false; }
    if (!dbl)     { *why = "1/3 to double is not 3FD5555555555555"; return false; }
    if (!greater) { *why = "comparing 1/3 with its double did not say greater"; return false; }
    if (!root)    { *why = "sqrt(2) in extended is not 3FFF B504F333F9DE6484"; return false; }
    if (!text)    { *why = "Dec2Str of 1/3 to 10 digits is not ' 3.333333333e-1'"; return false; }
    *why = "";
    return true;
}

/* ------------------------------------------------------------------------
 * The List Manager: _Pack0 on a list in a window
 * ------------------------------------------------------------------------ */

static void PushL(Asm* a, UInt32 v)    { W(a, 0x2F3C); L(a, v); }      /* MOVE.L #v,-(SP) */
static void PushW(Asm* a, UInt16 v)    { W(a, 0x3F3C); W(a, v); }      /* MOVE.W #v,-(SP) */
static void PushAddr(Asm* a, UInt32 v) { W(a, 0x4879); L(a, v); }      /* PEA v */
static void PushVar(Asm* a, UInt32 v)  { W(a, 0x2F39); L(a, v); }      /* MOVE.L v,-(SP) */
static void PopW(Asm* a, UInt32 to)    { W(a, 0x33DF); L(a, to); }     /* MOVE.W (SP)+,to */
static void PopL(Asm* a, UInt32 to)    { W(a, 0x23DF); L(a, to); }     /* MOVE.L (SP)+,to */
static void Pack0(Asm* a, UInt16 sel)  { PushW(a, sel); W(a, 0xA9E7); }
static UInt32 CellArg(SInt16 v, SInt16 h) { return ((UInt32)(UInt16)v << 16) | (UInt16)h; }

enum {
    kLWindow = 0x00, kLList = 0x04, kLBounds = 0x08, kLView = 0x10, kLData = 0x18,
    kLTitle = 0x20, kLApple = 0x24, kLPear = 0x2C, kLS = 0x30, kLPears = 0x34,
    kLBuf = 0x40, kLBufLen = 0x60, kLCell = 0x64, kLRectOut = 0x68,
    kLFirstRow = 0x70, kLGotSel = 0x72, kLFound = 0x74, kLNext = 0x76,
    kLOff = 0x78, kLLen = 0x7A, kLFoundCell = 0x7C, kLSelCell = 0x80, kLRowsAfter = 0x84
};

static void PutBytes(UInt32 a, const char* s, UInt32 n) {
    for (UInt32 i = 0; i < n; i++) M68K_Write8(gM68KApp, a + i, (UInt8)s[i]);
}

Boolean M68KToolbox_RunListTest(const char** why)
{
    World w;
    if (!WorldBegin(&w, why)) return false;
    UInt32 d = w.data;
    Rect bounds = { 50, 50, 250, 250 }, view = { 10, 10, 110, 110 }, data = { 0, 0, 0, 1 };
    WriteRect(d + kLBounds, &bounds);
    WriteRect(d + kLView, &view);
    WriteRect(d + kLData, &data);
    M68K_Write8(gM68KApp, d + kLTitle, 0);
    PutBytes(d + kLApple, "Apple", 5);
    PutBytes(d + kLPear, "Pear", 4);
    PutBytes(d + kLS, "s", 1);
    PutBytes(d + kLPears, "pears", 5);

    Asm a;
    a.n = 0;
    /* window := NewWindow(NIL, bounds, '', TRUE, 0, -1, FALSE, 0) */
    W(&a, 0x42A7);                                      /* CLR.L -(SP): result */
    PushL(&a, 0); PushAddr(&a, d + kLBounds); PushAddr(&a, d + kLTitle);
    PushW(&a, 0x0100); PushW(&a, 0); PushL(&a, 0xFFFFFFFF); PushW(&a, 0); PushL(&a, 0);
    W(&a, 0xA913);
    PopL(&a, d + kLWindow);
    /* list := LNew(view, data, (0,0), 0, window, TRUE, FALSE, FALSE, TRUE) */
    W(&a, 0x42A7);
    PushAddr(&a, d + kLView); PushAddr(&a, d + kLData); PushL(&a, 0); PushW(&a, 0);
    PushVar(&a, d + kLWindow);
    PushW(&a, 0x0100); PushW(&a, 0); PushW(&a, 0); PushW(&a, 0x0100);
    Pack0(&a, 68);
    PopL(&a, d + kLList);
    /* firstRow := LAddRow(10, 0, list) */
    W(&a, 0x4267);                                      /* CLR.W -(SP) */
    PushW(&a, 10); PushW(&a, 0); PushVar(&a, d + kLList); Pack0(&a, 8);
    PopW(&a, d + kLFirstRow);
    /* LSetCell('Apple', (2,0)); LSetCell('Pear', (3,0)); LAddToCell('s', (3,0)) */
    PushAddr(&a, d + kLApple); PushW(&a, 5); PushL(&a, CellArg(2, 0)); PushVar(&a, d + kLList); Pack0(&a, 88);
    PushAddr(&a, d + kLPear); PushW(&a, 4); PushL(&a, CellArg(3, 0)); PushVar(&a, d + kLList); Pack0(&a, 88);
    PushAddr(&a, d + kLS); PushW(&a, 1); PushL(&a, CellArg(3, 0)); PushVar(&a, d + kLList); Pack0(&a, 12);
    /* bufLen := 32; LGetCell(buf, bufLen, (3,0)) */
    W(&a, 0x33FC); W(&a, 32); L(&a, d + kLBufLen);     /* MOVE.W #32,bufLen */
    PushAddr(&a, d + kLBuf); PushAddr(&a, d + kLBufLen); PushL(&a, CellArg(3, 0)); PushVar(&a, d + kLList);
    Pack0(&a, 56);
    /* LSetSelect(TRUE, (5,0)); selCell := (0,0); gotSel := LGetSelect(TRUE, selCell) */
    PushW(&a, 0x0100); PushL(&a, CellArg(5, 0)); PushVar(&a, d + kLList); Pack0(&a, 92);
    W(&a, 0x42B9); L(&a, d + kLSelCell);                /* CLR.L selCell */
    W(&a, 0x4267); PushW(&a, 0x0100); PushAddr(&a, d + kLSelCell); PushVar(&a, d + kLList); Pack0(&a, 60);
    PopW(&a, d + kLGotSel);
    /* foundCell := (0,0); found := LSearch('pears', 5, NIL, foundCell) */
    W(&a, 0x42B9); L(&a, d + kLFoundCell);
    W(&a, 0x4267); PushAddr(&a, d + kLPears); PushW(&a, 5); PushL(&a, 0); PushAddr(&a, d + kLFoundCell);
    PushVar(&a, d + kLList); Pack0(&a, 84);
    PopW(&a, d + kLFound);
    /* cell := (9,0); next := LNextCell(TRUE, TRUE, cell): there is none */
    W(&a, 0x23FC); L(&a, CellArg(9, 0)); L(&a, d + kLCell);   /* MOVE.L #(9,0),cell */
    W(&a, 0x4267); PushW(&a, 0x0100); PushW(&a, 0x0100); PushAddr(&a, d + kLCell); PushVar(&a, d + kLList);
    Pack0(&a, 72);
    PopW(&a, d + kLNext);
    /* LRect(rectOut, (1,0)) */
    PushAddr(&a, d + kLRectOut); PushL(&a, CellArg(1, 0)); PushVar(&a, d + kLList); Pack0(&a, 76);
    /* LDelRow(1, 2): Apple goes, Pears moves up to row 2; LFind(off, len, (2,0)) */
    PushW(&a, 1); PushW(&a, 2); PushVar(&a, d + kLList); Pack0(&a, 36);
    PushAddr(&a, d + kLOff); PushAddr(&a, d + kLLen); PushL(&a, CellArg(2, 0)); PushVar(&a, d + kLList);
    Pack0(&a, 52);
    /* rowsAfter := list^^.dataBounds.bottom */
    W(&a, 0x2079); L(&a, d + kLList);                   /* MOVEA.L list,A0 */
    W(&a, 0x2050);                                      /* MOVEA.L (A0),A0 */
    W(&a, 0x33E8); W(&a, 72 + 4); L(&a, d + kLRowsAfter);   /* MOVE.W 76(A0),rowsAfter */
    /* LDispose(list); DisposeWindow(window) */
    PushVar(&a, d + kLList); Pack0(&a, 40);
    PushVar(&a, d + kLWindow); W(&a, 0xA914);
    W(&a, 0xA9F4);

    OSErr ran = WorldRun(&w, &a);
    M68KAddressSpace* as = gM68KApp;
    char got[8] = { 0 };
    for (int i = 0; i < 5; i++) got[i] = (char)M68K_Read8(as, d + kLBuf + (UInt32)i);
    Rect r;
    r.top = (SInt16)M68K_Read16(as, d + kLRectOut);
    r.left = (SInt16)M68K_Read16(as, d + kLRectOut + 2);
    r.bottom = (SInt16)M68K_Read16(as, d + kLRectOut + 4);
    r.right = (SInt16)M68K_Read16(as, d + kLRectOut + 6);
    Boolean listMade = M68K_Read32(as, d + kLList) != 0;
    Boolean firstRow = M68K_Read16(as, d + kLFirstRow) == 0;
    Boolean cellText = memcmp(got, "Pears", 5) == 0 && M68K_Read16(as, d + kLBufLen) == 5;
    Boolean selected = (M68K_Read16(as, d + kLGotSel) & 0xFF00) && M68K_Read32(as, d + kLSelCell) == CellArg(5, 0);
    Boolean searched = (M68K_Read16(as, d + kLFound) & 0xFF00) && M68K_Read32(as, d + kLFoundCell) == CellArg(3, 0);
    Boolean noNext = (M68K_Read16(as, d + kLNext) & 0xFF00) == 0;
    Boolean rectOK = r.top == 10 + 15 && r.left == 10 && r.bottom == 10 + 30 && r.right == 110;
    Boolean deleted = M68K_Read16(as, d + kLLen) == 5 && M68K_Read16(as, d + kLRowsAfter) == 9;
    WorldEnd(&w);

    if (ran != noErr)  { *why = "the program stopped with a fault"; return false; }
    if (!listMade)     { *why = "LNew gave no list"; return false; }
    if (!firstRow)     { *why = "LAddRow did not answer the first new row"; return false; }
    if (!cellText)     { *why = "LSetCell then LAddToCell did not read back as 'Pears'"; return false; }
    if (!selected)     { *why = "LGetSelect did not find the selected cell"; return false; }
    if (!searched)     { *why = "LSearch did not find 'pears'"; return false; }
    if (!noNext)       { *why = "LNextCell went past the last cell"; return false; }
    if (!rectOK)       { *why = "LRect is not where the cell is"; return false; }
    if (!deleted)      { *why = "LDelRow did not move the cells up"; return false; }
    *why = "";
    return true;
}
