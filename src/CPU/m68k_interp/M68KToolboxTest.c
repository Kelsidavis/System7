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
#include "CPU/M68KOpcodes.h"
#include "CPU/LowMemGlobals.h"
#include "M68KToolboxInternal.h"
#include "ResourceManager.h"
#include "System71StdLib.h"

/* Called by IntegrationTests.c */
Boolean M68KToolbox_RunTrapTest(const char** why);
Boolean M68KToolbox_RunCMPFlagsTest(const char** why);
Boolean M68KToolbox_RunSANETest(const char** why);
Boolean M68KToolbox_RunListTest(const char** why);
Boolean M68KToolbox_RunWindowTest(const char** why);
Boolean M68KToolbox_RunMenuTest(const char** why);
Boolean M68KToolbox_RunTimerTest(const char** why);
Boolean M68KToolbox_RunIconTest(const char** why);
Boolean M68KToolbox_Run68020Test(const char** why);

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
    kDataSize = 0x400
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

/* ------------------------------------------------------------------------
 * KeyTrans through a KCHR; SetWindowPic and GetWindowPic; DragGrayRgn
 * ------------------------------------------------------------------------ */

enum {
    kMWindow = 0x00, kMBounds = 0x04, kMTitle = 0x0C, kMRgn = 0x10, kMLimit = 0x14,
    kMSlop = 0x1C, kMState = 0x24, kMPic = 0x28, kMDrag = 0x2C,
    kMKeys = 0x30,                              /* five LONGINT answers */
    kMKCHR = 0x100                              /* 526 bytes */
};

/* A two-table KCHR: shift picks table 1; 'u' (key $22) is dead, and with
 * 'e' (key $24) makes $8E */
static void BuildKCHR(UInt32 k) {
    M68K_Write16(gM68KApp, k, 0);                               /* version */
    for (int m = 0; m < 256; m++) M68K_Write8(gM68KApp, k + 2 + (UInt32)m, (m & 0x02) ? 1 : 0);
    M68K_Write16(gM68KApp, k + 258, 2);                         /* two tables */
    for (int i = 0; i < 256; i++) M68K_Write8(gM68KApp, k + 260 + (UInt32)i, 0);
    M68K_Write8(gM68KApp, k + 260 + 0x00, 'a');
    M68K_Write8(gM68KApp, k + 260 + 0x22, 'u');
    M68K_Write8(gM68KApp, k + 260 + 0x24, 'e');
    M68K_Write8(gM68KApp, k + 260 + 128 + 0x00, 'A');
    UInt32 dead = k + 260 + 256;
    M68K_Write16(gM68KApp, dead, 1);                            /* one dead key */
    M68K_Write8(gM68KApp, dead + 2, 0);                         /* table 0 */
    M68K_Write8(gM68KApp, dead + 3, 0x22);                      /* key $22 */
    M68K_Write16(gM68KApp, dead + 4, 1);                        /* one completor */
    M68K_Write8(gM68KApp, dead + 6, 'e');
    M68K_Write8(gM68KApp, dead + 7, 0x8E);
    M68K_Write8(gM68KApp, dead + 8, 0);                         /* no match: */
    M68K_Write8(gM68KApp, dead + 9, '\'');                       /* the accent alone */
}

static void KeyTransCall(Asm* a, UInt32 d, UInt16 keycode, int answer) {
    W(a, 0x42A7);                                               /* result */
    PushAddr(a, d + kMKCHR); PushW(a, keycode); PushAddr(a, d + kMState);
    W(a, 0xA9C3);
    PopL(a, d + kMKeys + 4 * (UInt32)answer);
}

Boolean M68KToolbox_RunWindowTest(const char** why)
{
    World w;
    if (!WorldBegin(&w, why)) return false;
    UInt32 d = w.data;
    Rect bounds = { 50, 50, 250, 250 }, point = { 10, 10, 10, 10 }, slop = { 0, 0, 100, 100 };
    WriteRect(d + kMBounds, &bounds);
    WriteRect(d + kMLimit, &point);
    WriteRect(d + kMSlop, &slop);
    M68K_Write8(gM68KApp, d + kMTitle, 0);
    M68K_Write32(gM68KApp, d + kMState, 0);
    BuildKCHR(d + kMKCHR);

    Asm a;
    a.n = 0;
    KeyTransCall(&a, d, 0x0000, 0);                             /* a */
    KeyTransCall(&a, d, 0x0200, 1);                             /* shift-a */
    KeyTransCall(&a, d, 0x0022, 2);                             /* dead u */
    KeyTransCall(&a, d, 0x0024, 3);                             /* then e */
    KeyTransCall(&a, d, 0x0022, 4);                             /* dead u again... */
    KeyTransCall(&a, d, 0x0000, 4);                             /* ...then a */
    /* window := NewWindow(...); SetWindowPic(window, $00ABCDE0); pic := GetWindowPic */
    W(&a, 0x42A7);
    PushL(&a, 0); PushAddr(&a, d + kMBounds); PushAddr(&a, d + kMTitle);
    PushW(&a, 0x0100); PushW(&a, 0); PushL(&a, 0xFFFFFFFF); PushW(&a, 0); PushL(&a, 0);
    W(&a, 0xA913);
    PopL(&a, d + kMWindow);
    PushVar(&a, d + kMWindow); PushL(&a, 0x00ABCDE0); W(&a, 0xA92E);
    W(&a, 0x42A7); PushVar(&a, d + kMWindow); W(&a, 0xA92F); PopL(&a, d + kMPic);
    PushVar(&a, d + kMWindow); PushL(&a, 0); W(&a, 0xA92E);   /* no picture again */
    /* rgn := NewRgn; drag := DragGrayRgn(rgn, (10,10), point, slop, 0, NIL) */
    W(&a, 0x42A7); W(&a, 0xA8D8); PopL(&a, d + kMRgn);
    W(&a, 0x42A7);
    PushVar(&a, d + kMRgn); PushL(&a, CellArg(10, 10)); PushAddr(&a, d + kMLimit); PushAddr(&a, d + kMSlop);
    PushW(&a, 0); PushL(&a, 0);
    W(&a, 0xA905);
    PopL(&a, d + kMDrag);
    PushVar(&a, d + kMRgn); W(&a, 0xA8D9);                      /* DisposeRgn */
    PushVar(&a, d + kMWindow); W(&a, 0xA914);
    W(&a, 0xA9F4);

    OSErr ran = WorldRun(&w, &a);
    M68KAddressSpace* as = gM68KApp;
    UInt32 k[5];
    for (int i = 0; i < 5; i++) k[i] = M68K_Read32(as, d + kMKeys + 4 * (UInt32)i);
    Boolean keys = k[0] == 'a' && k[1] == 'A' && k[2] == 0 && k[3] == 0x8E &&
                   k[4] == (((UInt32)'\'' << 16) | 'a') && M68K_Read32(as, d + kMState) == 0;
    Boolean pic = M68K_Read32(as, d + kMPic) == 0x00ABCDE0;
    Boolean drag = M68K_Read32(as, d + kMDrag) == 0;
    WorldEnd(&w);

    if (ran != noErr) { *why = "the program stopped with a fault"; return false; }
    if (!keys) { *why = "KeyTrans did not translate through the KCHR"; return false; }
    if (!pic)  { *why = "GetWindowPic did not answer what SetWindowPic set"; return false; }
    if (!drag) { *why = "DragGrayRgn pinned to its start did not answer no movement"; return false; }
    *why = "";
    return true;
}

Boolean M68KToolbox_RunMenuTest(const char** why)
{
    World w;
    if (!WorldBegin(&w, why)) return false;
    UInt32 d = w.data;
    enum { kTitle = 0x100, kMenu = 0x120, kBefore = 0x124,
           kKeyResult = 0x128, kAfter = 0x12C, kClear = 0x130 };
    M68K_Write8(gM68KApp, d + kTitle, 4);
    PutBytes(d + kTitle + 1, "Test", 4);

    Asm a;
    a.n = 0;
    /* NewMenu(128, title), then set and read TheMenu through the traps. */
    W(&a, 0x42A7);
    PushW(&a, 128); PushAddr(&a, d + kTitle); W(&a, 0xA931);
    PopL(&a, d + kMenu);
    PushW(&a, 128); W(&a, 0xA938);
    W(&a, 0x3039); L(&a, LMG_TheMenu); W(&a, 0x33C0); L(&a, d + kBefore);
    /* A key with no matching menu command must not clear current menu state. */
    W(&a, 0x42A7); PushW(&a, 0x00FE); W(&a, 0xA93E); PopL(&a, d + kKeyResult);
    W(&a, 0x3039); L(&a, LMG_TheMenu); W(&a, 0x33C0); L(&a, d + kAfter);
    PushW(&a, 0); W(&a, 0xA938);
    W(&a, 0x3039); L(&a, LMG_TheMenu); W(&a, 0x33C0); L(&a, d + kClear);
    W(&a, 0xA9F4);

    OSErr ran = WorldRun(&w, &a);
    Boolean menuCreated = M68K_Read32(gM68KApp, d + kMenu) != 0;
    UInt16 before = M68K_Read16(gM68KApp, d + kBefore);
    UInt32 keyResult = M68K_Read32(gM68KApp, d + kKeyResult);
    UInt16 after = M68K_Read16(gM68KApp, d + kAfter);
    UInt16 clear = M68K_Read16(gM68KApp, d + kClear);
    WorldEnd(&w);

    if (ran != noErr) { *why = "the menu trap program stopped with a fault"; return false; }
    if (!menuCreated) { *why = "NewMenu did not return an application menu handle"; return false; }
    if (before != 128) { *why = "HiliteMenu did not set TheMenu to the application menu ID"; return false; }
    if (keyResult != 0 || after != before) { *why = "an unmatched MenuKey changed TheMenu"; return false; }
    if (clear != 0) { *why = "HiliteMenu(0) did not clear TheMenu"; return false; }
    *why = "";
    return true;
}

/* ------------------------------------------------------------------------
 * VBL and Time Manager tasks, and _Microseconds
 * ------------------------------------------------------------------------ */

enum {
    kTVBL = 0x00,                   /* VBLTask: 14 bytes */
    kTTM = 0x10,                    /* extended TMTask: 20 bytes */
    kTTicks = 0x30, kTVInst = 0x32, kTVRem = 0x34, kTVRemAgain = 0x36,
    kTFired = 0x38, kTFiredA1 = 0x3C, kTUs1 = 0x40, kTUs2 = 0x44,
    kTRoutines = 0x380              /* in the code area, after the program */
};

/* A wait for the word at addr to reach at least value, given up after a
 * couple of million turns so that a timer that never fires fails the test
 * rather than hanging it: MOVE.L #n,D7; loop: CMPI.W #v,addr; BGE done;
 * SUBQ.L #1,D7; BNE loop; done: */
static void WaitFor(Asm* a, UInt32 addr, UInt16 value) {
    W(a, 0x2E3C); L(a, 2000000);
    W(a, 0x0C79); W(a, value); L(a, addr);
    W(a, 0x6C04);
    W(a, 0x5387);
    W(a, 0x66F2);
}

Boolean M68KToolbox_RunTimerTest(const char** why)
{
    World w;
    if (!WorldBegin(&w, why)) return false;
    UInt32 d = w.data, vblProc = w.code + kTRoutines, tmProc = vblProc + 0x20;
    for (UInt32 i = 0; i < 0x50; i += 2) M68K_Write16(gM68KApp, d + i, 0);

    /* The VBL task counts, and asks to be called again next tick:
     * ADDQ.W #1,ticks; MOVE.W #1,10(A0); RTS */
    UInt16 vbl[] = { 0x5279, 0, 0, 0x317C, 1, 10, 0x4E75 };
    vbl[1] = (UInt16)((d + kTTicks) >> 16); vbl[2] = (UInt16)(d + kTTicks);
    for (int i = 0; i < 7; i++) M68K_Write16(gM68KApp, vblProc + 2 * (UInt32)i, vbl[i]);
    /* The Time Manager task says it ran, and with what in A1:
     * MOVE.W #1,fired; MOVE.L A1,firedA1; RTS */
    UInt16 tm[] = { 0x33FC, 1, 0, 0, 0x23C9, 0, 0, 0x4E75 };
    tm[2] = (UInt16)((d + kTFired) >> 16); tm[3] = (UInt16)(d + kTFired);
    tm[5] = (UInt16)((d + kTFiredA1) >> 16); tm[6] = (UInt16)(d + kTFiredA1);
    for (int i = 0; i < 8; i++) M68K_Write16(gM68KApp, tmProc + 2 * (UInt32)i, tm[i]);

    M68K_Write16(gM68KApp, d + kTVBL + 4, 1);                  /* qType: vType */
    M68K_Write32(gM68KApp, d + kTVBL + 6, vblProc);
    M68K_Write16(gM68KApp, d + kTVBL + 10, 1);                 /* vblCount */
    M68K_Write32(gM68KApp, d + kTTM + 6, tmProc);

    Asm a;
    a.n = 0;
    /* VInstall, three ticks, VRemove twice */
    W(&a, 0x41F9); L(&a, d + kTVBL); W(&a, 0xA033);
    W(&a, 0x33C0); L(&a, d + kTVInst);
    WaitFor(&a, d + kTTicks, 3);
    W(&a, 0x41F9); L(&a, d + kTVBL); W(&a, 0xA034);
    W(&a, 0x33C0); L(&a, d + kTVRem);
    W(&a, 0x41F9); L(&a, d + kTVBL); W(&a, 0xA034);
    W(&a, 0x33C0); L(&a, d + kTVRemAgain);
    /* InsXTime, PrimeTime 5 ms, wait, RmvTime */
    W(&a, 0x41F9); L(&a, d + kTTM); W(&a, 0xA458);
    W(&a, 0x41F9); L(&a, d + kTTM); W(&a, 0x7005); W(&a, 0xA05A);
    WaitFor(&a, d + kTFired, 1);
    W(&a, 0x41F9); L(&a, d + kTTM); W(&a, 0xA059);
    /* Microseconds, twice */
    W(&a, 0xA193); W(&a, 0x23C0); L(&a, d + kTUs1);
    W(&a, 0xA193); W(&a, 0x23C0); L(&a, d + kTUs2);
    W(&a, 0xA9F4);

    OSErr ran = (UInt32)a.n * 2 > kTRoutines ? paramErr : WorldRun(&w, &a);
    M68KAddressSpace* as = gM68KApp;
    Boolean vbl3 = M68K_Read16(as, d + kTVInst) == 0 && (SInt16)M68K_Read16(as, d + kTTicks) >= 3;
    Boolean removed = M68K_Read16(as, d + kTVRem) == 0 && M68K_Read16(as, d + kTVRemAgain) == 0xFFFF &&
                      M68K_Read32(as, 0x0162) == 0;
    Boolean fired = M68K_Read16(as, d + kTFired) == 1 && M68K_Read32(as, d + kTFiredA1) == d + kTTM &&
                    (M68K_Read16(as, d + kTTM + 4) & 0x8000) == 0;
    UInt32 us1 = M68K_Read32(as, d + kTUs1), us2 = M68K_Read32(as, d + kTUs2);
    Boolean clock = us1 != 0 && us2 - us1 < 1000000;
    WorldEnd(&w);

    if (ran != noErr) { *why = "the program stopped with a fault"; return false; }
    if (!vbl3)    { *why = "the VBL task did not run each tick"; return false; }
    if (!removed) { *why = "VRemove did not take the task out, then answer qErr"; return false; }
    if (!fired)   { *why = "the Time Manager task did not run, with A1 the task"; return false; }
    if (!clock)   { *why = "Microseconds did not answer a running count"; return false; }
    *why = "";
    return true;
}

/* ------------------------------------------------------------------------
 * _IconDispatch: an icon and its mask, into the program's own bits
 * ------------------------------------------------------------------------ */

enum {
    kIPort = 0x00,                  /* GrafPort: 108 bytes */
    kIBitMap1 = 0x70, kIBitMap2 = 0x80, kIRect = 0x90, kIHandle = 0x98,
    kIErr1 = 0x9C, kIErr2 = 0x9E,
    kIBits1 = 0x100, kIBits2 = 0x180
};

static void PutBitMap(UInt32 a, UInt32 bits) {
    M68K_Write32(gM68KApp, a, bits);
    M68K_Write16(gM68KApp, a + 4, 4);                   /* rowBytes */
    Rect r = { 0, 0, 32, 32 };
    WriteRect(a + 6, &r);
}

static void PlotIconHandleCall(Asm* a, UInt32 d, UInt32 bitmap, UInt16 transform, UInt32 errAt) {
    PushAddr(a, d + bitmap); W(a, 0xA875);              /* SetPortBits */
    W(a, 0x4267);                                       /* result */
    PushAddr(a, d + kIRect); PushW(a, 0); PushW(a, transform); PushVar(a, d + kIHandle);
    W(a, 0x303C); W(a, 0x061D);                         /* MOVE.W #PlotIconHandle,D0 */
    W(a, 0xABC9);
    PopW(a, d + errAt);
}

Boolean M68KToolbox_RunIconTest(const char** why)
{
    World w;
    if (!WorldBegin(&w, why)) return false;
    UInt32 d = w.data;
    /* ICN#: the image a checkerboard, the mask the top half */
    UInt32 icon = M68KHeap_NewHandle(256, false);
    if (!icon) { WorldEnd(&w); *why = "no memory for the icon"; return false; }
    UInt32 ip = M68KHeap_Deref(icon);
    for (UInt32 row = 0; row < 32; row++) {
        M68K_Write32(gM68KApp, ip + row * 4, 0xAAAAAAAA);
        M68K_Write32(gM68KApp, ip + 128 + row * 4, row < 16 ? 0xFFFFFFFF : 0);
    }
    M68K_Write32(gM68KApp, d + kIHandle, icon);
    PutBitMap(d + kIBitMap1, d + kIBits1);
    PutBitMap(d + kIBitMap2, d + kIBits2);
    for (UInt32 i = 0; i < 128; i += 4) {
        M68K_Write32(gM68KApp, d + kIBits1 + i, 0xFFFFFFFF);   /* black */
        M68K_Write32(gM68KApp, d + kIBits2 + i, 0);            /* white */
    }
    Rect r = { 0, 0, 32, 32 };
    WriteRect(d + kIRect, &r);

    Asm a;
    a.n = 0;
    PushAddr(&a, d + kIPort); W(&a, 0xA86F);            /* OpenPort */
    PlotIconHandleCall(&a, d, kIBitMap1, 0, kIErr1);            /* plain, onto black */
    PlotIconHandleCall(&a, d, kIBitMap2, 0x4000, kIErr2);       /* selected, onto white */
    PushAddr(&a, d + kIPort); W(&a, 0xA87D);            /* ClosePort */
    W(&a, 0xA9F4);

    OSErr ran = WorldRun(&w, &a);
    M68KAddressSpace* as = gM68KApp;
    Boolean plain = true, selected = true;
    static char detail[96];
    for (UInt32 row = 0; row < 32; row++) {
        UInt32 p = M68K_Read32(as, d + kIBits1 + row * 4), q = M68K_Read32(as, d + kIBits2 + row * 4);
        if (plain && p != (row < 16 ? 0xAAAAAAAA : 0xFFFFFFFF)) {
            plain = false;
            snprintf(detail, sizeof(detail), "plain icon wrong from row %lu: %08lX",
                     (unsigned long)row, (unsigned long)p);
        }
        if (selected && q != (row < 16 ? 0x55555555 : 0)) {
            selected = false;
            if (plain) snprintf(detail, sizeof(detail), "selected icon wrong from row %lu: %08lX",
                                (unsigned long)row, (unsigned long)q);
        }
    }
    Boolean errs = M68K_Read16(as, d + kIErr1) == 0 && M68K_Read16(as, d + kIErr2) == 0;
    WorldEnd(&w);

    if (ran != noErr) { *why = "the program stopped with a fault"; return false; }
    if (!errs)        { *why = "PlotIconHandle answered an error"; return false; }
    if (!plain || !selected) { *why = detail; return false; }
    *why = "";
    return true;
}

/* ------------------------------------------------------------------------
 * The 68020's instructions and addressing modes
 * ------------------------------------------------------------------------ */

enum {
    kXTable = 0x00,                 /* 4 longs: 0x11111111, 0x22222222, ... */
    kXPtr = 0x10,                   /* the table's address */
    kXPtrs = 0x14,                  /* 4 longs; [3] = &table[1] */
    kXBits = 0x24,                  /* 0x0F 0xF0 */
    kXBits2 = 0x28,                 /* 4 zero bytes, a field put into them */
    kXCas = 0x2C, kXCas2 = 0x30,
    kXBounds = 0x34,                /* 10, 20 */
    kXOdd = 0x3C,                   /* 0x11223344 at kXOdd + 1 */
    kXOut = 0x80                    /* results, a long each */
};

enum {
    rScaled, rPost, rPre, rMulLo, rMulHi, rMulsSR, rDivQ, rDivR, rDivuQ, rDivuR,
    rExtb, rBfextu, rBfexts, rBfffo, rBfset, rCasMem, rCasDc, rCmpIn, rCmpOut,
    rPack, rUnpk, rSkipped, rLinkSP, rLinkSP0, rRtdSP, rRtdSP0, rOdd, rProc, rCount
};

static void StoreD(Asm* a, UInt32 d, int dn, int slot) {     /* MOVE.L Dn,out[slot] */
    W(a, (UInt16)(0x23C0 | dn)); L(a, d + kXOut + 4 * (UInt32)slot);
}
static void StoreSR(Asm* a, UInt32 d, int slot) {            /* MOVE SR,D7; MOVE.L D7,slot */
    W(a, 0x40C7); StoreD(a, d, 7, slot);
}
static void MoveqTo(Asm* a, int dn, SInt8 v) { W(a, (UInt16)(0x7000 | (dn << 9) | (UInt8)v)); }
static void MoveL(Asm* a, int dn, UInt32 v) { W(a, (UInt16)(0x203C | (dn << 9))); L(a, v); }

Boolean M68KToolbox_RunCMPFlagsTest(const char** why)
{
    enum { kOverflowFlags = 0x200, kNoOverflowFlags = 0x204, kCmpmStack = 0x208 };
    World w;
    if (!WorldBegin(&w, why)) return false;
    UInt32 testStack = w.stack + 0x1000;
    M68K_Write8(gM68KApp, testStack, 0x7F);
    M68K_Write8(gM68KApp, testStack + 2, 0x80);

    Asm a;
    a.n = 0;
    MoveqTo(&a, 0, -128);
    MoveqTo(&a, 1, 127);
    W(&a, 0x9200);                  /* SUB.B D0,D1 sets X on the unsigned borrow */
    MoveqTo(&a, 0, -128);
    MoveqTo(&a, 1, 127);
    W(&a, 0xB200);                  /* CMP.B D0,D1: 127 - (-128) overflows */
    W(&a, 0x40C2);                  /* MOVE SR,D2 */
    W(&a, 0x23C2); L(&a, w.data + kOverflowFlags);
    MoveqTo(&a, 0, 2);
    MoveqTo(&a, 1, 1);
    W(&a, 0xB200);                  /* CMP.B D0,D1: 1 - 2 does not overflow */
    W(&a, 0x40C2);
    W(&a, 0x23C2); L(&a, w.data + kNoOverflowFlags);
    MoveL(&a, 0, testStack);
    W(&a, 0x2E40);                  /* MOVEA.L D0,A7 */
    W(&a, 0xBF0F);                  /* CMPM.B (A7)+,(A7)+ */
    W(&a, 0x260F);                  /* MOVE.L A7,D3 */
    W(&a, 0x23C3); L(&a, w.data + kCmpmStack);
    W(&a, 0xA9F4);                  /* _ExitToShell */

    OSErr runResult = WorldRun(&w, &a);
    /* MOVE.L saved D2; the status bits occupy its low word. */
    UInt16 overflowFlags = (UInt16)M68K_Read32(gM68KApp, w.data + kOverflowFlags);
    UInt16 noOverflowFlags = (UInt16)M68K_Read32(gM68KApp, w.data + kNoOverflowFlags);
    UInt32 cmpmStack = M68K_Read32(gM68KApp, w.data + kCmpmStack);
    WorldEnd(&w);

    if (runResult != noErr) {
        *why = "the comparison program stopped with a fault";
        return false;
    }
    if ((overflowFlags & (CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C)) !=
        (CCR_X | CCR_N | CCR_V | CCR_C)) {
        *why = "CMP did not set N, V, and C or preserve X for signed overflow";
        return false;
    }
    if ((noOverflowFlags & (CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C)) !=
        (CCR_X | CCR_N | CCR_C)) {
        *why = "CMP did not clear V or preserve X for a non-overflowing subtraction";
        return false;
    }
    if (cmpmStack != testStack + 4) {
        *why = "CMPM.B did not advance A7 by two bytes per operand";
        return false;
    }

    *why = "";
    return true;
}

Boolean M68KToolbox_Run68020Test(const char** why)
{
    World w;
    if (!WorldBegin(&w, why)) return false;
    UInt32 d = w.data, sub = w.code + 0x3C0;
    for (UInt32 i = 0; i < 0x80 + 4 * rCount; i += 4) M68K_Write32(gM68KApp, d + i, 0);
    for (UInt32 i = 0; i < 4; i++) M68K_Write32(gM68KApp, d + kXTable + 4 * i, 0x11111111u * (i + 1));
    M68K_Write32(gM68KApp, d + kXPtr, d + kXTable);
    M68K_Write32(gM68KApp, d + kXPtrs + 12, d + kXTable + 4);
    M68K_Write8(gM68KApp, d + kXBits, 0x0F);
    M68K_Write8(gM68KApp, d + kXBits + 1, 0xF0);
    M68K_Write32(gM68KApp, d + kXCas, 5);
    M68K_Write32(gM68KApp, d + kXCas2, 9);
    M68K_Write32(gM68KApp, d + kXBounds, 10);
    M68K_Write32(gM68KApp, d + kXBounds + 4, 20);
    M68K_Write8(gM68KApp, d + kXOdd + 1, 0x11);
    M68K_Write8(gM68KApp, d + kXOdd + 2, 0x22);
    M68K_Write8(gM68KApp, d + kXOdd + 3, 0x33);
    M68K_Write8(gM68KApp, d + kXOdd + 4, 0x44);
    /* The RTD subroutine: RTD #4 */
    M68K_Write16(gM68KApp, sub, 0x4E74);
    M68K_Write16(gM68KApp, sub + 2, 4);

    Asm a;
    a.n = 0;
    /* (0,A0,D1.L*4): table[3] */
    W(&a, 0x207C); L(&a, d + kXTable);                  /* MOVEA.L #table,A0 */
    MoveqTo(&a, 1, 3);
    W(&a, 0x2430); W(&a, 0x1C00);                       /* MOVE.L (0,A0,D1.L*4),D2 */
    StoreD(&a, d, 2, rScaled);
    /* ([4,A1],D1.L*2,2): through the pointer, then index and outer: table[2] */
    W(&a, 0x227C); L(&a, d + kXPtr - 4);                /* MOVEA.L #ptr-4,A1 */
    W(&a, 0x2631); W(&a, 0x1B26); W(&a, 4); W(&a, 2);   /* MOVE.L ([4,A1],D1.L*2,2),D3 */
    StoreD(&a, d, 3, rPost);
    /* ([A2,D1.L*4]): ptrs[3], which is &table[1] */
    W(&a, 0x247C); L(&a, d + kXPtrs);                   /* MOVEA.L #ptrs,A2 */
    W(&a, 0x2832); W(&a, 0x1D11);                       /* MOVE.L ([A2,D1.L*4]),D4 */
    StoreD(&a, d, 4, rPre);
    /* MULU.L #$9ABCDEF0,D6:D5 with D5 = $12345678 */
    MoveL(&a, 5, 0x12345678);
    W(&a, 0x4C3C); W(&a, 0x5406); L(&a, 0x9ABCDEF0);
    StoreD(&a, d, 5, rMulLo); StoreD(&a, d, 6, rMulHi);
    /* MULS.L #-2,D0 with D0 = $40000001: overflows, V */
    MoveL(&a, 0, 0x40000001);
    W(&a, 0x4C3C); W(&a, 0x0800); L(&a, 0xFFFFFFFE);
    StoreSR(&a, d, rMulsSR);
    /* DIVS.L #7,D1:D0 with D1:D0 = -100: -14 remainder -2 */
    MoveL(&a, 1, 0xFFFFFFFF); MoveL(&a, 0, 0xFFFFFF9C);
    W(&a, 0x4C7C); W(&a, 0x0C01); L(&a, 7);
    StoreD(&a, d, 0, rDivQ); StoreD(&a, d, 1, rDivR);
    /* DIVUL.L #10,D3:D2 with D2 = 1234: 123 remainder 4 */
    MoveL(&a, 2, 1234);
    W(&a, 0x4C7C); W(&a, 0x2003); L(&a, 10);
    StoreD(&a, d, 2, rDivuQ); StoreD(&a, d, 3, rDivuR);
    /* EXTB.L D0 with D0 = $F0 */
    MoveL(&a, 0, 0xF0);
    W(&a, 0x49C0);
    StoreD(&a, d, 0, rExtb);
    /* BFEXTU D0{4:8},D1 with D0 = $12345678: $23 */
    MoveL(&a, 0, 0x12345678);
    W(&a, 0xE9C0); W(&a, 0x1108);
    StoreD(&a, d, 1, rBfextu);
    /* BFEXTS bits{4:8},D2 where bits are $0F $F0: $FF, signed */
    W(&a, 0xEBF9); W(&a, 0x2108); L(&a, d + kXBits);
    StoreD(&a, d, 2, rBfexts);
    /* BFINS D3,bits2{12:8} with D3 = $AB: 00 0A B0 00 */
    MoveL(&a, 3, 0xAB);
    W(&a, 0xEFF9); W(&a, 0x3308); L(&a, d + kXBits2);
    /* BFFFO D4{0:32},D5 with D4 = $00400000: 9 */
    MoveL(&a, 4, 0x00400000);
    W(&a, 0xEDC4); W(&a, 0x5000);
    StoreD(&a, d, 5, rBfffo);
    /* BFSET D6{28:8} with D6 = 0: round the end, $F000000F */
    MoveqTo(&a, 6, 0);
    W(&a, 0xEEC6); W(&a, 0x0708);
    StoreD(&a, d, 6, rBfset);
    /* CAS.L D0,D1,cas: 5 equals 5, 77 goes in. CAS.L D2,D3,cas2: 1 is not
     * 9, so D2 gets 9 */
    MoveqTo(&a, 0, 5); MoveqTo(&a, 1, 77);
    W(&a, 0x0EF9); W(&a, 0x0040); L(&a, d + kXCas);
    MoveqTo(&a, 2, 1); MoveqTo(&a, 3, 50);
    W(&a, 0x0EF9); W(&a, 0x00C2); L(&a, d + kXCas2);
    StoreD(&a, d, 2, rCasDc);
    /* CMP2.L bounds,D0: 15 within [10,20]; 25 not */
    MoveqTo(&a, 0, 15);
    W(&a, 0x04F9); W(&a, 0x0000); L(&a, d + kXBounds);
    StoreSR(&a, d, rCmpIn);
    MoveqTo(&a, 0, 25);
    W(&a, 0x04F9); W(&a, 0x0000); L(&a, d + kXBounds);
    StoreSR(&a, d, rCmpOut);
    /* PACK D0,D1,#0 with D0 = $0305: $35. UNPK D1,D2,#$3030: $3335 */
    MoveL(&a, 0, 0x0305); MoveqTo(&a, 1, 0);
    W(&a, 0x8340); W(&a, 0);
    StoreD(&a, d, 1, rPack);
    MoveqTo(&a, 2, 0);
    W(&a, 0x8581); W(&a, 0x3030);
    StoreD(&a, d, 2, rUnpk);
    /* BRA.L over a MOVE that would mark it not taken */
    W(&a, 0x60FF); L(&a, 12);
    W(&a, 0x33FC); W(&a, 1); L(&a, d + kXOut + 4 * rSkipped);
    /* LINK.L A6,#-$100, the SP it leaves, UNLK */
    W(&a, 0x23CF); L(&a, d + kXOut + 4 * rLinkSP0);     /* MOVE.L A7,before */
    W(&a, 0x480E); L(&a, 0xFFFFFF00);
    W(&a, 0x23CF); L(&a, d + kXOut + 4 * rLinkSP);
    W(&a, 0x4E5E);                                      /* UNLK A6 */
    /* An argument pushed, JSR to RTD #4: the stack as it was */
    W(&a, 0x23CF); L(&a, d + kXOut + 4 * rRtdSP0);
    W(&a, 0x2F3C); L(&a, 0);                            /* MOVE.L #0,-(SP) */
    W(&a, 0x4EB9); L(&a, sub);                          /* JSR sub */
    W(&a, 0x23CF); L(&a, d + kXOut + 4 * rRtdSP);
    /* TRAPF: never traps */
    W(&a, 0x51FC);
    /* A long at an odd address */
    W(&a, 0x2039); L(&a, d + kXOdd + 1);                /* MOVE.L odd,D0 */
    StoreD(&a, d, 0, rOdd);
    /* Gestalt('proc') */
    MoveL(&a, 0, 0x70726F63);
    W(&a, 0xA1AD);
    W(&a, 0x2008);                                      /* MOVE.L A0,D0 */
    StoreD(&a, d, 0, rProc);
    W(&a, 0xA9F4);

    OSErr ran = (UInt32)a.n * 2 > 0x3C0 ? paramErr : WorldRun(&w, &a);
    M68KAddressSpace* as = gM68KApp;
    UInt32 r[rCount];
    for (int i = 0; i < rCount; i++) r[i] = M68K_Read32(as, d + kXOut + 4 * (UInt32)i);
    UInt64 product = (UInt64)0x12345678 * 0x9ABCDEF0;
    UInt32 bits2 = M68K_Read32(as, d + kXBits2);
    UInt32 cas = M68K_Read32(as, d + kXCas);
    WorldEnd(&w);

    static char detail[96];
    struct { Boolean ok; const char* what; } checks[] = {
        { r[rScaled] == 0x44444444, "scaled index (0,A0,D1.L*4)" },
        { r[rPost] == 0x33333333, "postindexed memory indirect ([4,A1],D1.L*2,2)" },
        { r[rPre] == 0x22222222, "preindexed memory indirect ([A2,D1.L*4])" },
        { r[rMulLo] == (UInt32)product && r[rMulHi] == (UInt32)(product >> 32), "MULU.L 64-bit" },
        { (r[rMulsSR] & 0x02) != 0, "MULS.L overflow did not set V" },
        { r[rDivQ] == (UInt32)-14 && r[rDivR] == (UInt32)-2, "DIVS.L 64-bit dividend" },
        { r[rDivuQ] == 123 && r[rDivuR] == 4, "DIVUL.L" },
        { r[rExtb] == 0xFFFFFFF0, "EXTB.L" },
        { r[rBfextu] == 0x23, "BFEXTU from a register" },
        { r[rBfexts] == 0xFFFFFFFF, "BFEXTS from memory" },
        { bits2 == 0x000AB000, "BFINS into memory" },
        { r[rBfffo] == 9, "BFFFO" },
        { r[rBfset] == 0xF000000F, "BFSET wrapping round a register" },
        { cas == 77 && r[rCasDc] == 9, "CAS" },
        { (r[rCmpIn] & 0x05) == 0 && (r[rCmpOut] & 0x01) != 0, "CMP2" },
        { (r[rPack] & 0xFF) == 0x35 && (r[rUnpk] & 0xFFFF) == 0x3335, "PACK and UNPK" },
        { r[rSkipped] == 0, "BRA.L did not branch over" },
        { r[rLinkSP] == r[rLinkSP0] - 4 - 0x100, "LINK.L" },
        { r[rRtdSP] == r[rRtdSP0], "RTD did not take its argument off" },
        { r[rOdd] == 0x11223344, "a long at an odd address" },
        { r[rProc] == 3, "Gestalt does not say a 68020" },
    };
    if (ran != noErr) { *why = "the program stopped with a fault"; return false; }
    for (unsigned i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
        if (!checks[i].ok) {
            snprintf(detail, sizeof(detail), "%s", checks[i].what);
            *why = detail;
            return false;
        }
    }
    *why = "";
    return true;
}
