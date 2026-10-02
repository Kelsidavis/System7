/*
 * M68KToolbox.c - answering a 68K application's traps
 *
 * Each handler reads the call's arguments where the application left them,
 * does the work with the native managers, and leaves the result where the
 * application expects it. The application's memory is big-endian whatever
 * the machine running this is, and is only ever touched through the
 * interpreter's accessors.
 *
 * Two calling conventions:
 *   OS traps ($A0xx-$A7xx) take their arguments in registers - usually A0
 *     and D0 - and answer in A0 and D0. The dispatcher in M68KOpcodes.c
 *     keeps the registers the caller is owed and sets the condition codes.
 *   Toolbox traps ($A8xx-$AFxx) are Pascal procedures: arguments pushed left
 *     to right, the space for a function's result pushed before them, and
 *     the callee pops the arguments. A Boolean is a word whose high byte
 *     holds the value.
 */

#include <string.h>

#include "CPU/M68KToolbox.h"
#include "CPU/M68KInterp.h"
#include "CPU/M68KHeap.h"
#include "M68KToolboxInternal.h"
#include "CPU/LowMemGlobals.h"
#include "ResourceManager.h"
#include "MenuManager/MenuManager.h"
#include "EventManager/EventManager.h"
#include "QuickDraw/QuickDraw.h"
#include "System71StdLib.h"

extern QDGlobals qd;
extern UInt32 TickCount(void);
extern UInt32 GetDblTime(void);
extern void SysBeep(short duration);
extern void InitCursor(void);

/* Low-memory globals this module keeps that LowMemGlobals.h does not name */
enum {
    kLM_ScreenRow   = 0x0106,
    kLM_TheZone     = 0x0118,
    kLM_MemErr      = 0x0220,
    kLM_ROM85       = 0x028E,
    kLM_ScrnBase    = 0x0824,
    kLM_CurApRefNum = 0x0900,
    kLM_CurApName   = 0x0910,   /* Str31 */
    kLM_ResErr      = 0x0A60
};

/* The application being answered; one runs at a time */
M68KAddressSpace* gM68KApp;
#define gAS gM68KApp
static UInt32 gStackBase;
static GrafPtr gPortStack[16];      /* the port across each Toolbox call, nested */
static int gPortDepth;
static UInt32 gScreenBase;
static Boolean gMenusTaken;

static void SetMemErr(OSErr err) {
    W16(kLM_MemErr, (UInt16)err);
    D(0) = (UInt32)(SInt32)err;
}

/* ------------------------------------------------------------------------
 * Memory Manager (OS traps)
 * ------------------------------------------------------------------------ */

static Boolean ClearFlag(void) { return (gAS->currentTrap & 0x0200) != 0; }

TRAP(Trap_NewPtr) {
    UNUSED;
    A(0) = M68KHeap_NewPtr(D(0), ClearFlag());
    SetMemErr(A(0) ? noErr : memFullErr);
    return noErr;
}

TRAP(Trap_DisposePtr) { UNUSED; SetMemErr(M68KHeap_DisposePtr(A(0))); return noErr; }

TRAP(Trap_GetPtrSize) {
    UNUSED;
    UInt32 n = M68KHeap_GetPtrSize(A(0));
    OSErr err = M68KHeap_LastError();
    W16(kLM_MemErr, (UInt16)err);
    D(0) = err ? (UInt32)(SInt32)err : n;
    return noErr;
}

TRAP(Trap_SetPtrSize) { UNUSED; SetMemErr(M68KHeap_SetPtrSize(A(0), D(0))); return noErr; }

TRAP(Trap_NewHandle) {
    UNUSED;
    A(0) = M68KHeap_NewHandle(D(0), ClearFlag());
    SetMemErr(A(0) ? noErr : memFullErr);
    return noErr;
}

TRAP(Trap_NewEmptyHandle) {
    UNUSED;
    A(0) = M68KHeap_NewEmptyHandle();
    SetMemErr(A(0) ? noErr : memFullErr);
    return noErr;
}

TRAP(Trap_DisposeHandle) { UNUSED; SetMemErr(M68KHeap_DisposeHandle(A(0))); return noErr; }

TRAP(Trap_GetHandleSize) {
    UNUSED;
    UInt32 n = M68KHeap_GetHandleSize(A(0));
    OSErr err = M68KHeap_LastError();
    W16(kLM_MemErr, (UInt16)err);
    D(0) = err ? (UInt32)(SInt32)err : n;
    return noErr;
}

TRAP(Trap_SetHandleSize) { UNUSED; SetMemErr(M68KHeap_SetHandleSize(A(0), D(0))); return noErr; }
TRAP(Trap_ReallocHandle) { UNUSED; SetMemErr(M68KHeap_ReallocHandle(A(0), D(0))); return noErr; }
TRAP(Trap_EmptyHandle)   { UNUSED; SetMemErr(M68KHeap_EmptyHandle(A(0))); return noErr; }

TRAP(Trap_RecoverHandle) {
    UNUSED;
    A(0) = M68KHeap_RecoverHandle(A(0));
    W16(kLM_MemErr, (UInt16)M68KHeap_LastError());
    return noErr;
}

/* Handle state: HLock, HUnlock, HPurge, HNoPurge, HSetRBit, HClrRBit */
static OSErr StateBits(UInt8 set, UInt8 clear) {
    if (!A(0)) {
        SetMemErr(nilHandleErr);
        return noErr;
    }
    M68KHeap_SetState(A(0), (UInt8)((M68KHeap_GetState(A(0)) | set) & ~clear));
    SetMemErr(noErr);
    return noErr;
}
TRAP(Trap_HLock)    { UNUSED; return StateBits(0x80, 0); }
TRAP(Trap_HUnlock)  { UNUSED; return StateBits(0, 0x80); }
TRAP(Trap_HPurge)   { UNUSED; return StateBits(0x40, 0); }
TRAP(Trap_HNoPurge) { UNUSED; return StateBits(0, 0x40); }
TRAP(Trap_HSetRBit) { UNUSED; return StateBits(0x20, 0); }
TRAP(Trap_HClrRBit) { UNUSED; return StateBits(0, 0x20); }

TRAP(Trap_HGetState) {
    UNUSED;
    D(0) = M68KHeap_GetState(A(0));
    W16(kLM_MemErr, 0);
    return noErr;
}

TRAP(Trap_HSetState) {
    UNUSED;
    M68KHeap_SetState(A(0), (UInt8)D(0));
    SetMemErr(noErr);
    return noErr;
}

/* BlockMove: A0 to A1, D0 bytes, overlapping or not */
TRAP(Trap_BlockMove) {
    UNUSED;
    UInt32 src = A(0), dst = A(1), n = D(0);
    if (dst > src && dst < src + n) {
        for (UInt32 i = n; i > 0; i--) W8(dst + i - 1, R8(src + i - 1));
    } else {
        for (UInt32 i = 0; i < n; i++) W8(dst + i, R8(src + i));
    }
    D(0) = 0;
    return noErr;
}

static UInt32 ZoneAddress(void) {
    UInt32 start, end;
    M68KHeap_Bounds(&start, &end);
    return start;
}

TRAP(Trap_FreeMem)   { UNUSED; D(0) = M68KHeap_FreeBytes(); return noErr; }
TRAP(Trap_MaxMem)    { UNUSED; D(0) = M68KHeap_LargestFree(); A(0) = 0; return noErr; }
TRAP(Trap_MaxBlock)  { UNUSED; D(0) = M68KHeap_LargestFree(); return noErr; }
TRAP(Trap_CompactMem){ UNUSED; D(0) = M68KHeap_LargestFree(); return noErr; }
TRAP(Trap_PurgeSpace){ UNUSED; D(0) = M68KHeap_FreeBytes(); A(0) = M68KHeap_LargestFree(); return noErr; }
TRAP(Trap_StackSpace){ UNUSED; D(0) = A(7) - gStackBase; return noErr; }
TRAP(Trap_StripAddress) { UNUSED; D(0) &= 0x00FFFFFF; return noErr; }
TRAP(Trap_SwapMMUMode)  { UNUSED; D(0) = 0; return noErr; }   /* 24-bit, always */

/* Calls that rearrange or limit a heap that never moves anything: done */
TRAP(Trap_MemNoOp) { UNUSED; SetMemErr(noErr); return noErr; }

TRAP(Trap_GetZone)  { UNUSED; A(0) = R32(kLM_TheZone); D(0) = 0; return noErr; }
TRAP(Trap_SetZone)  { UNUSED; W32(kLM_TheZone, A(0)); D(0) = 0; return noErr; }
TRAP(Trap_HandleZone) { UNUSED; A(0) = ZoneAddress(); D(0) = 0; return noErr; }

/* HandToHand: a copy of the handle in A0, in A0 */
TRAP(Trap_HandToHand) {
    UNUSED;
    UInt32 src = A(0), n = M68KHeap_GetHandleSize(src);
    UInt32 h = M68KHeap_NewHandle(n, false);
    if (h) {
        UInt32 s = M68KHeap_Deref(src), d = M68KHeap_Deref(h);
        for (UInt32 i = 0; i < n; i++) W8(d + i, R8(s + i));
    }
    A(0) = h;
    SetMemErr(h ? noErr : memFullErr);
    return noErr;
}

/* PtrToHand: D0 bytes at A0 into a new handle, in A0 */
TRAP(Trap_PtrToHand) {
    UNUSED;
    UInt32 src = A(0), n = D(0);
    UInt32 h = M68KHeap_NewHandle(n, false);
    if (h) {
        UInt32 d = M68KHeap_Deref(h);
        for (UInt32 i = 0; i < n; i++) W8(d + i, R8(src + i));
    }
    A(0) = h;
    SetMemErr(h ? noErr : memFullErr);
    return noErr;
}

/* PtrToXHand: D0 bytes at A0 into the existing handle A1 */
TRAP(Trap_PtrToXHand) {
    UNUSED;
    UInt32 src = A(0), h = A(1), n = D(0);
    OSErr err = M68KHeap_SetHandleSize(h, n);
    if (!err) {
        UInt32 d = M68KHeap_Deref(h);
        for (UInt32 i = 0; i < n; i++) W8(d + i, R8(src + i));
    }
    A(0) = h;
    SetMemErr(err);
    return noErr;
}

/* HandAndHand: append handle A0's contents to handle A1 */
TRAP(Trap_HandAndHand) {
    UNUSED;
    UInt32 a = A(0), b = A(1);
    UInt32 na = M68KHeap_GetHandleSize(a), nb = M68KHeap_GetHandleSize(b);
    OSErr err = M68KHeap_SetHandleSize(b, na + nb);
    if (!err) {
        UInt32 s = M68KHeap_Deref(a), d = M68KHeap_Deref(b) + nb;
        for (UInt32 i = 0; i < na; i++) W8(d + i, R8(s + i));
    }
    A(0) = b;
    SetMemErr(err);
    return noErr;
}

/* PtrAndHand: append D0 bytes at A0 to handle A1 */
TRAP(Trap_PtrAndHand) {
    UNUSED;
    UInt32 src = A(0), h = A(1), n = D(0);
    UInt32 old = M68KHeap_GetHandleSize(h);
    OSErr err = M68KHeap_SetHandleSize(h, old + n);
    if (!err) {
        UInt32 d = M68KHeap_Deref(h) + old;
        for (UInt32 i = 0; i < n; i++) W8(d + i, R8(src + i));
    }
    A(0) = h;
    SetMemErr(err);
    return noErr;
}

/* ------------------------------------------------------------------------
 * Resources
 *
 * The native Resource Manager reads the resource; the application gets a
 * copy in its own heap, marked as a resource. The same native resource
 * always answers with the same 68K handle, so ReleaseResource and
 * comparisons of handles work as a program expects.
 * ------------------------------------------------------------------------ */

enum { kMaxResMap = 1024 };
static struct { Handle native; UInt32 h; } gResMap[kMaxResMap];
static int gResCount;

static int ResIndexOf68K(UInt32 h) {
    for (int i = 0; i < gResCount; i++) if (gResMap[i].h == (h & 0x00FFFFFF)) return i;
    return -1;
}

static void ForgetRes(int i) {
    gResMap[i] = gResMap[--gResCount];
}

static void SetResErr(void) {
    W16(kLM_ResErr, (UInt16)ResError());
}

/* The application's handle for a native resource handle; 0 for none */
UInt32 M68KTB_ResHandleFor(Handle native) {
    if (!native) return 0;
    for (int i = 0; i < gResCount; i++) {
        if (gResMap[i].native == native) return gResMap[i].h;
    }
    Size n = GetHandleSize(native);
    UInt32 h = M68KHeap_NewHandle((UInt32)n, false);
    if (!h) return 0;
    UInt32 p = M68KHeap_Deref(h);
    HLock(native);
    const UInt8* src = (const UInt8*)*native;
    for (Size i = 0; i < n; i++) W8(p + (UInt32)i, src[i]);
    HUnlock(native);
    M68KHeap_SetState(h, 0x20);
    if (gResCount < kMaxResMap) {
        gResMap[gResCount].native = native;
        gResMap[gResCount].h = h;
        gResCount++;
    }
    return h;
}

static Handle NativeFor(UInt32 h) {
    int i = ResIndexOf68K(h);
    return i >= 0 ? gResMap[i].native : NULL;
}

/* FUNCTION GetResource(theType: ResType; theID: INTEGER): Handle */
TRAP(Trap_GetResource) {
    UNUSED;
    SInt16 id = (SInt16)Pop16();
    ResType type = Pop32();
    Boolean one = (gAS->currentTrap & 0x03FF) == (0xA81F & 0x03FF);
    Handle n = one ? Get1Resource(type, id) : GetResource(type, id);
    SetResErr();
    Result32(M68KTB_ResHandleFor(n));
    return noErr;
}

/* FUNCTION GetNamedResource(theType: ResType; name: Str255): Handle */
TRAP(Trap_GetNamedResource) {
    UNUSED;
    Str255 name;
    ReadPString(Pop32(), name);
    ResType type = Pop32();
    Boolean one = (gAS->currentTrap & 0x03FF) == (0xA820 & 0x03FF);
    Handle n = one ? Get1NamedResource(type, name) : GetNamedResource(type, name);
    SetResErr();
    Result32(M68KTB_ResHandleFor(n));
    return noErr;
}

/* FUNCTION GetIndResource(theType: ResType; index: INTEGER): Handle */
TRAP(Trap_GetIndResource) {
    UNUSED;
    SInt16 index = (SInt16)Pop16();
    ResType type = Pop32();
    Boolean one = (gAS->currentTrap & 0x03FF) == (0xA80E & 0x03FF);
    Handle n = one ? Get1IndResource(type, index) : GetIndResource(type, index);
    SetResErr();
    Result32(M68KTB_ResHandleFor(n));
    return noErr;
}

/* FUNCTION CountResources(theType: ResType): INTEGER */
TRAP(Trap_CountResources) {
    UNUSED;
    ResType type = Pop32();
    Boolean one = (gAS->currentTrap & 0x03FF) == (0xA80D & 0x03FF);
    Result16((UInt16)(one ? Count1Resources(type) : CountResources(type)));
    return noErr;
}

/* PROCEDURE ReleaseResource(theResource: Handle) */
TRAP(Trap_ReleaseResource) {
    UNUSED;
    UInt32 h = Pop32();
    int i = ResIndexOf68K(h);
    if (i >= 0) {
        Handle native = gResMap[i].native;
        ForgetRes(i);
        M68KHeap_DisposeHandle(h);
        ReleaseResource(native);
    }
    SetResErr();
    return noErr;
}

/* PROCEDURE DetachResource(theResource: Handle): the handle becomes the
 * application's own; the next GetResource reads the resource afresh */
TRAP(Trap_DetachResource) {
    UNUSED;
    UInt32 h = Pop32();
    int i = ResIndexOf68K(h);
    if (i >= 0) {
        Handle native = gResMap[i].native;
        ForgetRes(i);
        M68KHeap_SetState(h, (UInt8)(M68KHeap_GetState(h) & ~0x20));
        ReleaseResource(native);
    }
    W16(kLM_ResErr, 0);
    return noErr;
}

TRAP(Trap_LoadResource)     { UNUSED; (void)Pop32(); W16(kLM_ResErr, 0); return noErr; }
TRAP(Trap_ResError)         { UNUSED; Result16((UInt16)R16(kLM_ResErr)); return noErr; }
TRAP(Trap_CurResFile)       { UNUSED; Result16((UInt16)CurResFile()); return noErr; }
TRAP(Trap_UseResFile)       { UNUSED; UseResFile((SInt16)Pop16()); SetResErr(); return noErr; }
TRAP(Trap_SetResLoad)       { UNUSED; SetResLoad(PopBool()); return noErr; }

TRAP(Trap_HomeResFile) {
    UNUSED;
    Handle n = NativeFor(Pop32());
    Result16((UInt16)(n ? HomeResFile(n) : -1));
    SetResErr();
    return noErr;
}

TRAP(Trap_SizeResource) {
    UNUSED;
    UInt32 h = Pop32();
    Handle n = NativeFor(h);
    Result32(n ? (UInt32)GetHandleSize(n) : M68KHeap_GetHandleSize(h));
    return noErr;
}

/* PROCEDURE GetResInfo(theResource: Handle; VAR theID: INTEGER;
 *                      VAR theType: ResType; VAR name: Str255) */
TRAP(Trap_GetResInfo) {
    UNUSED;
    UInt32 nameAddr = Pop32(), typeAddr = Pop32(), idAddr = Pop32();
    Handle n = NativeFor(Pop32());
    if (!n) {
        W16(kLM_ResErr, (UInt16)resNotFound);
        return noErr;
    }
    ResID id;
    ResType type;
    Str255 name;
    name[0] = 0;
    GetResInfo(n, &id, &type, (char*)name);
    if (idAddr) W16(idAddr, (UInt16)id);
    if (typeAddr) W32(typeAddr, type);
    if (nameAddr) WritePString(nameAddr, name);
    SetResErr();
    return noErr;
}

/* Changes to resources stay in memory: the application's file is not
 * written. These answer as if they had been. */
TRAP(Trap_ResNoOp1) { UNUSED; (void)Pop32(); W16(kLM_ResErr, 0); return noErr; }
TRAP(Trap_UpdateResFile) { UNUSED; (void)Pop16(); W16(kLM_ResErr, 0); return noErr; }

/* FUNCTION GetString(stringID: INTEGER): StringHandle, and the others that
 * are GetResource with the type implied */
static OSErr TypedResource(ResType type) {
    SInt16 id = (SInt16)Pop16();
    Handle n = GetResource(type, id);
    SetResErr();
    Result32(M68KTB_ResHandleFor(n));
    return noErr;
}
TRAP(Trap_GetString)  { UNUSED; return TypedResource('STR '); }
TRAP(Trap_GetCursor)  { UNUSED; return TypedResource('CURS'); }
TRAP(Trap_GetPattern) { UNUSED; return TypedResource('PAT '); }
TRAP(Trap_GetIcon)    { UNUSED; return TypedResource('ICON'); }
TRAP(Trap_GetPicture) { UNUSED; return TypedResource('PICT'); }

/* ------------------------------------------------------------------------
 * Starting up
 * ------------------------------------------------------------------------ */

static const UInt16 kArrow[34] = {
    0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7C00, 0x7E00, 0x7F00,
    0x7F80, 0x7C00, 0x6C00, 0x4600, 0x0600, 0x0300, 0x0300, 0x0000,
    0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00, 0xFF80,
    0xFFC0, 0xFFE0, 0xFE00, 0xEF00, 0xCF00, 0x8780, 0x0780, 0x0380,
    0x0001, 0x0001,                     /* hot spot (1,1) */
};

/* PROCEDURE InitGraf(globalPtr: Ptr): globalPtr is the address of
 * QuickDraw's thePort, the last of its globals, which the application keeps
 * below A5. Fills them in, and leaves the pointer to them at 0(A5). */
TRAP(Trap_InitGraf) {
    UNUSED;
    UInt32 g = Pop32();
    W32(A(5), g);
    W32(g, 0);                                          /* thePort */
    for (int i = 0; i < 8; i++) {
        W8(g - 8 + i, 0x00);                            /* white */
        W8(g - 16 + i, 0xFF);                           /* black */
        W8(g - 24 + i, (i & 1) ? 0x55 : 0xAA);          /* gray */
        W8(g - 32 + i, (i & 1) ? 0x22 : 0x88);          /* ltGray */
        W8(g - 40 + i, (i & 1) ? 0xDD : 0x77);          /* dkGray */
    }
    for (int i = 0; i < 34; i++) W16(g - 108 + 2 * i, kArrow[i]);   /* arrow */
    Rect screen = qd.screenBits.bounds;
    UInt16 rowBytes = (UInt16)(((screen.right - screen.left) + 15) / 16 * 2);
    W32(g - 122, gScreenBase);                          /* screenBits */
    W16(g - 118, rowBytes);
    W16(g - 116, screen.top);
    W16(g - 114, screen.left);
    W16(g - 112, screen.bottom);
    W16(g - 110, screen.right);
    W32(g - 126, 1);                                    /* randSeed */
    W32(kLM_ScrnBase, gScreenBase);
    W16(kLM_ScreenRow, rowBytes);
    return noErr;
}

/* Managers that are already running: nothing to start */
TRAP(Trap_NoOp) { UNUSED; return noErr; }
TRAP(Trap_PopLong) { UNUSED; (void)Pop32(); return noErr; }   /* InitDialogs(resumeProc) */

/* InitMenus: the menu bar is the application's now */
TRAP(Trap_InitMenus) {
    UNUSED;
    ClearMenuBar();
    DrawMenuBar();
    gMenusTaken = true;
    return noErr;
}

TRAP(Trap_InitCursor) { UNUSED; InitCursor(); return noErr; }

/* FlushEvents: D0 low word the events to remove, high word where to stop */
TRAP(Trap_FlushEvents) {
    UNUSED;
    FlushEvents((EventMask)(D(0) & 0xFFFF), (EventMask)(D(0) >> 16));
    D(0) = 0;
    return noErr;
}

TRAP(Trap_TickCount) { UNUSED; Result32(TickCount()); return noErr; }
TRAP(Trap_SysBeep)   { UNUSED; SysBeep((short)Pop16()); return noErr; }

/* SysEnvirons: D0 the version wanted, A0 the record to fill (IM V-5) */
TRAP(Trap_SysEnvirons) {
    UNUSED;
    UInt32 r = A(0);
    W16(r + 0, 2);              /* environsVersion */
    W16(r + 2, 4);              /* machineType: a Macintosh Plus, by its traps */
    W16(r + 4, 0x0710);         /* systemVersion */
    W16(r + 6, 1);              /* processor: 68000 */
    W8(r + 8, 0);               /* hasFPU */
    W8(r + 9, 0);               /* hasColorQD */
    W16(r + 10, 4);             /* keyBoardType: Mac Plus keyboard */
    W16(r + 12, 0);             /* atDrvrVersNum */
    W16(r + 14, (UInt16)-1);    /* sysVRefNum */
    D(0) = 0;
    return noErr;
}

/* Gestalt: D0 the selector, the answer in A0 */
TRAP(Trap_Gestalt) {
    UNUSED;
    switch (D(0)) {
        case 'sysv': A(0) = 0x0710; D(0) = 0; break;
        case 'qd  ': A(0) = 0x0000; D(0) = 0; break;   /* original QuickDraw */
        case 'proc': A(0) = 1;      D(0) = 0; break;   /* 68000 */
        case 'fpu ': A(0) = 0;      D(0) = 0; break;
        case 'addr': A(0) = 0;      D(0) = 0; break;   /* 24-bit */
        case 'ram ': A(0) = M68K_MAX_ADDR; D(0) = 0; break;
        default:     A(0) = 0;      D(0) = (UInt32)(SInt32)-5551; break;  /* undefined */
    }
    return noErr;
}

/* _ExitToShell: the application is done. Its address space stops, and
 * LaunchApplication, which started it, returns to the Finder. */
TRAP(Trap_ExitToShell) {
    UNUSED;
    gAS->halted = true;
    gAS->lastException = 0;
    gAS->faultReason = NULL;
    return noErr;
}

/* ------------------------------------------------------------------------ */

static const M68KTrapEntry kTraps[] = {
    /* Memory Manager */
    { 0xA11E, Trap_NewPtr },        { 0xA01F, Trap_DisposePtr },   { 0xA021, Trap_GetPtrSize },
    { 0xA020, Trap_SetPtrSize },    { 0xA122, Trap_NewHandle },    { 0xA166, Trap_NewEmptyHandle },
    { 0xA023, Trap_DisposeHandle }, { 0xA025, Trap_GetHandleSize },{ 0xA024, Trap_SetHandleSize },
    { 0xA027, Trap_ReallocHandle }, { 0xA02B, Trap_EmptyHandle },  { 0xA128, Trap_RecoverHandle },
    { 0xA029, Trap_HLock },         { 0xA02A, Trap_HUnlock },      { 0xA049, Trap_HPurge },
    { 0xA04A, Trap_HNoPurge },      { 0xA067, Trap_HSetRBit },     { 0xA068, Trap_HClrRBit },
    { 0xA069, Trap_HGetState },     { 0xA06A, Trap_HSetState },    { 0xA02E, Trap_BlockMove },
    { 0xA01C, Trap_FreeMem },       { 0xA11D, Trap_MaxMem },       { 0xA061, Trap_MaxBlock },
    { 0xA04C, Trap_CompactMem },    { 0xA162, Trap_PurgeSpace },   { 0xA065, Trap_StackSpace },
    { 0xA055, Trap_StripAddress },  { 0xA05D, Trap_SwapMMUMode },
    { 0xA036, Trap_MemNoOp },       /* MoreMasters */
    { 0xA063, Trap_MemNoOp },       /* MaxApplZone */
    { 0xA04D, Trap_MemNoOp },       /* PurgeMem */
    { 0xA040, Trap_MemNoOp },       /* ResrvMem */
    { 0xA02D, Trap_MemNoOp },       /* SetApplLimit */
    { 0xA04B, Trap_MemNoOp },       /* SetGrowZone */
    { 0xA064, Trap_MemNoOp },       /* MoveHHi */
    { 0xA11A, Trap_GetZone },       { 0xA01B, Trap_SetZone },      { 0xA126, Trap_HandleZone },
    { 0xA148, Trap_HandleZone },    /* PtrZone */
    { 0xA9E1, Trap_HandToHand },    { 0xA9E3, Trap_PtrToHand },    { 0xA9E2, Trap_PtrToXHand },
    { 0xA9E4, Trap_HandAndHand },   { 0xA9EF, Trap_PtrAndHand },

    /* Resource Manager */
    { 0xA9A0, Trap_GetResource },   { 0xA81F, Trap_GetResource },
    { 0xA9A1, Trap_GetNamedResource }, { 0xA820, Trap_GetNamedResource },
    { 0xA99D, Trap_GetIndResource },{ 0xA80E, Trap_GetIndResource },
    { 0xA99C, Trap_CountResources },{ 0xA80D, Trap_CountResources },
    { 0xA9A3, Trap_ReleaseResource },{ 0xA992, Trap_DetachResource },
    { 0xA9A2, Trap_LoadResource },  { 0xA9AF, Trap_ResError },     { 0xA994, Trap_CurResFile },
    { 0xA998, Trap_UseResFile },    { 0xA99B, Trap_SetResLoad },   { 0xA9A4, Trap_HomeResFile },
    { 0xA9A5, Trap_SizeResource },  { 0xA9A8, Trap_GetResInfo },
    { 0xA9AA, Trap_ResNoOp1 },      /* ChangedResource */
    { 0xA9B0, Trap_ResNoOp1 },      /* WriteResource */
    { 0xA999, Trap_UpdateResFile },
    { 0xA9BA, Trap_GetString },     { 0xA9B9, Trap_GetCursor },    { 0xA9B8, Trap_GetPattern },
    { 0xA9BB, Trap_GetIcon },       { 0xA9BC, Trap_GetPicture },

    /* Starting up */
    { 0xA86E, Trap_InitGraf },      { 0xA8FE, Trap_NoOp },         /* InitFonts */
    { 0xA912, Trap_NoOp },          /* InitWindows */
    { 0xA930, Trap_InitMenus },     { 0xA9CC, Trap_NoOp },         /* TEInit */
    { 0xA97B, Trap_PopLong },       /* InitDialogs */
    { 0xA850, Trap_InitCursor },    { 0xA032, Trap_FlushEvents },
    { 0xA975, Trap_TickCount },     { 0xA9C8, Trap_SysBeep },
    { 0xA090, Trap_SysEnvirons },   { 0xA1AD, Trap_Gestalt },
    { 0xA9F4, Trap_ExitToShell },
};

OSErr M68KToolbox_Prepare(SegmentLoaderContext* ctx, ConstStr255Param appName,
                          SInt16 resRefNum, CPUAddr stackBase, CPUAddr stackTop,
                          VRefNum appVRef, DirID appDir)
{
    if (!ctx || !ctx->cpuAS || !ctx->cpuBackend) return paramErr;
    gAS = (M68KAddressSpace*)ctx->cpuAS;
    gStackBase = stackBase;
    gPortDepth = 0;
    gResCount = 0;
    gMenusTaken = false;

    /* The application heap, and a 1-bit screen for programs that look at
     * screenBits.baseAddr; what is drawn on the real screen goes there
     * through QuickDraw */
    enum { kHeapSize = 4 * 1024 * 1024 };
    CPUAddr heap = 0;
    OSErr err = ctx->cpuBackend->AllocateMemory(ctx->cpuAS, kHeapSize, kCPUMapA5World, &heap);
    if (err != noErr) return err;
    M68KHeap_Init(gAS, heap, kHeapSize);
    Rect screen = qd.screenBits.bounds;
    UInt32 screenBytes = (UInt32)(((screen.right - screen.left) + 15) / 16 * 2) *
                         (UInt32)(screen.bottom - screen.top);
    err = ctx->cpuBackend->AllocateMemory(ctx->cpuAS, screenBytes, kCPUMapA5World, &gScreenBase);
    if (err != noErr) return err;

    /* The globals a program of the period reads for itself */
    W32(LMG_CurrentA5, ctx->a5World.a5Base);
    W32(LMG_CurStackBase, stackTop);
    W32(LMG_MemTop, M68K_MAX_ADDR);
    W32(LMG_ApplZone, heap);
    W32(LMG_SysZone, heap);
    W32(kLM_TheZone, heap);
    W32(LMG_HeapEnd, heap + kHeapSize);
    W32(LMG_ApplLimit, heap + kHeapSize);
    W16(kLM_ROM85, 0x7FFF);                 /* 128K ROM, no Color QuickDraw */
    W16(kLM_CurApRefNum, (UInt16)resRefNum);
    UInt8 len = appName ? appName[0] : 0;
    if (len > 31) len = 31;
    W8(kLM_CurApName, len);
    for (int i = 0; i < len; i++) W8(kLM_CurApName + 1 + i, appName[1 + i]);
    LMSetTicks(TickCount());
    M68KFiles_Prepare(appVRef, appDir);
    W32(0x02F0, GetDblTime());              /* DoubleTime */
    W32(0x02F4, 32);                        /* CaretTime: half a second */

    const struct { const M68KTrapEntry* t; int n; } tables[] = {
        { kTraps, (int)(sizeof(kTraps) / sizeof(kTraps[0])) },
        { kM68KQuickDrawTraps, kM68KQuickDrawTrapCount },
        { kM68KPortTraps, kM68KPortTrapCount },
        { kM68KWindowTraps, kM68KWindowTrapCount },
        { kM68KMenuTraps, kM68KMenuTrapCount },
        { kM68KEventTraps, kM68KEventTrapCount },
        { kM68KDialogTraps, kM68KDialogTrapCount },
        { kM68KControlTraps, kM68KControlTrapCount },
        { kM68KTextEditTraps, kM68KTextEditTrapCount },
        { kM68KUtilityTraps, kM68KUtilityTrapCount },
        { kM68KFileTraps, kM68KFileTrapCount },
    };
    for (size_t k = 0; k < sizeof(tables) / sizeof(tables[0]); k++) {
        for (int i = 0; i < tables[k].n; i++) {
            err = ctx->cpuBackend->InstallTrap(ctx->cpuAS, tables[k].t[i].trap,
                                               tables[k].t[i].handler, gAS);
            if (err != noErr) return err;
        }
    }
    return noErr;
}

/*
 * The current port across a Toolbox call. The Macintosh's own calls put the
 * caller's port back when they use another - MenuSelect, the alerts,
 * ModalDialog, Standard File, DragWindow - and programs count on it: an
 * InvalRect after choosing a menu item is in the window the program last
 * set, not wherever the menu was drawn. These native calls do not all do
 * that, so it is done around every one, except the calls whose work is to
 * change the port, and those that may dispose of it.
 */
static Boolean ChangesPort(UInt16 trap) {
    switch (trap & 0xFBFF) {
        case 0xA873: case 0xA86F: case 0xA86D: case 0xA86E: case 0xA87D:   /* SetPort, OpenPort,
                                                         InitPort, InitGraf, ClosePort */
        case 0xA914: case 0xA92D: case 0xA983: case 0xA982:   /* the disposes */
            return true;
        default:
            return false;
    }
}

void M68KTB_TrapEnter(UInt16 trap) {
    (void)trap;
    if (gPortDepth < 16) GetPort(&gPortStack[gPortDepth]);
    gPortDepth++;
}

void M68KTB_TrapLeave(UInt16 trap) {
    if (gPortDepth <= 0) return;
    gPortDepth--;
    if (gPortDepth >= 16 || ChangesPort(trap)) return;
    GrafPtr now;
    GetPort(&now);
    if (now != gPortStack[gPortDepth] && gPortStack[gPortDepth]) SetPort(gPortStack[gPortDepth]);
}

UInt32 M68KTB_ScreenBase(void) {
    return gScreenBase;
}

UInt32 M68KTB_QDGlobals(void) {
    return gAS ? R32(gAS->regs.a[5]) : 0;
}

void M68KToolbox_Finish(void) {
    M68KTE_Finish();
    M68KDialogs_Finish();
    M68KFiles_Finish();
    M68KUtils_Finish();
    Ports_Finish();
    M68KQD_Finish();
    Obj_Finish();
    M68KMenus_Finish();
    /* Native resources the application still held */
    for (int i = 0; i < gResCount; i++) ReleaseResource(gResMap[i].native);
    gResCount = 0;
    if (gMenusTaken) {
        extern void Finder_InstallMenuBar(void);
        ClearMenuBar();
        Finder_InstallMenuBar();
        gMenusTaken = false;
    }
    InitCursor();
    gAS = NULL;
}
