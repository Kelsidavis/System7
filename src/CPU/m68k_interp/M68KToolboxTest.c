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

typedef struct {
    UInt16 words[128];
    int n;
} Asm;

static void W(Asm* a, UInt16 w) { if (a->n < 128) a->words[a->n++] = w; }
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
    kDataSize = 0x180
};

Boolean M68KToolbox_RunTrapTest(const char** why)
{
    const ICPUBackend* be = CPUBackend_Get("m68k_interp");
    CPUAddressSpace cas = NULL;
    *why = "no 68K backend";
    if (!be || be->CreateAddressSpace(NULL, &cas) != noErr) return false;

    SegmentLoaderContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cpuAS = cas;
    ctx.cpuBackend = be;

    enum { kStack = 16 * 1024 };
    CPUAddr code = 0, data = 0, stack = 0;
    Boolean ok = be->AllocateMemory(cas, 1024, kCPUMapA5World, &code) == noErr &&
                 be->AllocateMemory(cas, kDataSize, kCPUMapA5World, &data) == noErr &&
                 be->AllocateMemory(cas, kStack, kCPUMapA5World, &stack) == noErr &&
                 be->SetStacks(cas, stack + kStack, 0) == noErr;
    static const UInt8 kAppName[] = "\x05ITest";
    SInt16 refNum = CurResFile();
    if (ok) ok = M68KToolbox_Prepare(&ctx, kAppName, refNum, stack, stack + kStack, 0, 0) == noErr;
    if (!ok) {
        *why = "could not prepare the program";
        if (gM68KApp) M68KToolbox_Finish();
        be->DestroyAddressSpace(cas);
        return false;
    }
    for (UInt32 i = 0; i < kDataSize; i += 4) M68K_Write32(gM68KApp, data + i, 0xDEADBEEF);

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
    for (int i = 0; i < a.n; i++) M68K_Write16(gM68KApp, code + 2 * i, a.words[i]);

    M68KAddressSpace* as = gM68KApp;
    OSErr ran = be->EnterAt(cas, code, kEnterApp);
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

    M68KToolbox_Finish();
    be->DestroyAddressSpace(cas);

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
