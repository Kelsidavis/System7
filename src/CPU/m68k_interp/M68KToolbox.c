/*
 * M68KToolbox.c - answering a 68K application's traps
 *
 * Each handler reads the call's arguments off the application's stack, does
 * the work with the native managers, and leaves the result where the
 * application expects it. The application's memory is big-endian whatever
 * the machine running this is, and is only ever touched through the
 * interpreter's accessors.
 */

#include <string.h>

#include "CPU/M68KToolbox.h"
#include "CPU/M68KInterp.h"
#include "CPU/LowMemGlobals.h"
#include "System71StdLib.h"

extern UInt32 TickCount(void);

extern void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value);
extern void M68K_Write16(M68KAddressSpace* as, UInt32 addr, UInt16 value);
extern void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value);

/* Low-memory globals this module sets that LowMemGlobals.h does not name */
enum {
    kLM_ROM85      = 0x028E,   /* which ROM: bit 15 clear for 128K and later */
    kLM_CurApRefNum = 0x0900,
    kLM_CurApName  = 0x0910    /* Str31 */
};

/* _ExitToShell: the application is done. Its address space stops, and
 * LaunchApplication, which started it, returns to the Finder. */
static OSErr Trap_ExitToShell(void* context, CPUAddr* pc, CPUAddr* registers)
{
    M68KAddressSpace* as = (M68KAddressSpace*)context;
    (void)pc;
    (void)registers;
    as->halted = true;
    as->lastException = 0;
    as->faultReason = NULL;
    return noErr;
}

typedef struct {
    UInt16 trap;
    CPUTrapHandler handler;
} TrapEntry;

static const TrapEntry kTraps[] = {
    { 0xA9F4, Trap_ExitToShell },
};

OSErr M68KToolbox_Prepare(SegmentLoaderContext* ctx, ConstStr255Param appName,
                          SInt16 resRefNum, CPUAddr stackTop)
{
    if (!ctx || !ctx->cpuAS || !ctx->cpuBackend) return paramErr;
    M68KAddressSpace* as = (M68KAddressSpace*)ctx->cpuAS;

    /* The globals a program of the period reads for itself */
    M68K_Write32(as, LMG_CurrentA5, ctx->a5World.a5Base);
    M68K_Write32(as, LMG_CurStackBase, stackTop);
    M68K_Write32(as, LMG_MemTop, M68K_MAX_ADDR);
    M68K_Write16(as, kLM_ROM85, 0x7FFF);          /* 128K ROM, no Color QuickDraw */
    M68K_Write16(as, kLM_CurApRefNum, (UInt16)resRefNum);
    UInt8 len = appName ? appName[0] : 0;
    if (len > 31) len = 31;
    M68K_Write8(as, kLM_CurApName, len);
    for (int i = 0; i < len; i++) M68K_Write8(as, kLM_CurApName + 1 + i, appName[1 + i]);
    LMSetTicks(TickCount());

    for (size_t i = 0; i < sizeof(kTraps) / sizeof(kTraps[0]); i++) {
        OSErr err = ctx->cpuBackend->InstallTrap(ctx->cpuAS, kTraps[i].trap,
                                                 kTraps[i].handler, as);
        if (err != noErr) return err;
    }
    return noErr;
}
