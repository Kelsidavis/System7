/*
 * A5World.c - Portable A5 World Construction
 *
 * Builds the classic 68K A5 world layout:
 * - Below A5: Application globals
 * - A5: Base pointer
 * - Above A5: Jump table and parameters
 */

#include "SegmentLoader/SegmentLoader.h"
#include "SegmentLoader/CodeParser.h"
#include "SegmentLoader/SegmentLoaderLogging.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include <string.h>

/*
 * InstallA5World - Set up A5 world memory layout
 *
 * Classic Mac A5 world layout:
 *
 *   [Below A5 area]    <-- a5BelowBase (app globals, QD globals)
 *   ...
 *   [A5]               <-- a5Base = a5BelowBase + a5BelowSize
 *   ...
 *   [Jump Table]       <-- jtBase = a5Base + jtOffsetFromA5
 *   [Params]
 *   [Above A5 area]    <-- a5AboveBase (stack growth area)
 */
OSErr InstallA5World(SegmentLoaderContext* ctx, const CODE0Info* info)
{
    OSErr err;
    CPUAddr belowBase = 0, aboveBase = 0, a5;

    if (!ctx || !info) {
        return paramErr;
    }

    if (!ctx->cpuBackend) {
        return segmentA5WorldErr;
    }

    /* Allocate below-A5 area (application globals) */
    if (info->a5BelowSize > 0) {
        err = ctx->cpuBackend->AllocateMemory(ctx->cpuAS,
                                             info->a5BelowSize,
                                             kCPUMapA5World,
                                             &belowBase);
        if (err != noErr) {
            return err;
        }
    } else {
        belowBase = 0;
    }

    /* Calculate A5 register value */
    a5 = belowBase + info->a5BelowSize;

    /* Allocate above-A5 area (jump table + params) */
    if (info->a5AboveSize > 0) {
        err = ctx->cpuBackend->AllocateMemory(ctx->cpuAS,
                                             info->a5AboveSize,
                                             kCPUMapA5World,
                                             &aboveBase);
        if (err != noErr) {
            /* Below-A5 was allocated but above-A5 failed.
             * DestroyAddressSpace will reclaim all allocations. */
            return err;
        }
    } else {
        aboveBase = a5;
    }

    /* Verify above-A5 base is immediately after A5 */
    if (aboveBase != a5) {
        /* Adjust if needed (simple allocator may not give contiguous) */
        /* For MVP, we'll accept non-contiguous and use offset */
    }

    /* Store A5 world layout in context */
    ctx->a5World.a5BelowBase = belowBase;
    ctx->a5World.a5BelowSize = info->a5BelowSize;
    ctx->a5World.a5Base = a5;
    /*
     * Load A5 into the register it is named for.
     *
     * The A5 world was laid out and its base recorded, and nothing in the
     * system ever called SetRegisterA5 - so A5 held zero while every jump
     * table entry was addressed as an offset from it. A JSR through the table
     * went to the offset itself, low in memory, and execution wandered into
     * unmapped pages. The layout is this code's to build, so loading the
     * register that addresses it belongs here rather than with each caller.
     */
    if (ctx->cpuBackend && ctx->cpuBackend->SetRegisterA5) {
        OSErr a5Err = ctx->cpuBackend->SetRegisterA5(ctx->cpuAS, a5);
        if (a5Err != noErr) {
            SEG_LOG_ERROR("Failed to load A5 = 0x%08X: %d", a5, a5Err);
            return a5Err;
        }
    }

    ctx->a5World.a5AboveBase = aboveBase;
    ctx->a5World.a5AboveSize = info->a5AboveSize;

    /* Calculate jump table base */
    ctx->a5World.jtBase = a5 + info->jtOffsetFromA5;
    ctx->a5World.jtCount = info->jtCount;
    ctx->a5World.jtEntrySize = info->jtEntrySize;

    /* Set A5 register in CPU */
    err = ctx->cpuBackend->SetRegisterA5(ctx->cpuAS, a5);
    if (err != noErr) {
        return err;
    }

    /* Initialize QuickDraw globals area (below A5, offset -0xA00) */
    /* For MVP, just zero the area */
    if (info->a5BelowSize > 0) {
        UInt8* zeroBuffer = (UInt8*)NewPtr(info->a5BelowSize);
        if (zeroBuffer) {
            memset(zeroBuffer, 0, info->a5BelowSize);
            ctx->cpuBackend->WriteMemory(ctx->cpuAS, belowBase,
                                        zeroBuffer, info->a5BelowSize);
            DisposePtr((Ptr)zeroBuffer);
        }
    }

    ctx->a5World.initialized = true;

    /* A5 Invariant Assertions (smoke checks) */
    if (belowBase + info->a5BelowSize != a5) {
        SEG_LOG_ERROR("FATAL: a5BelowBase(0x%08X) + size(0x%X) != a5(0x%08X)",
                      belowBase, info->a5BelowSize, a5);
        return segmentA5WorldErr;
    }

    if (ctx->a5World.jtBase != a5 + info->jtOffsetFromA5) {
        SEG_LOG_ERROR("FATAL: jtBase(0x%08X) != a5(0x%08X) + offset(0x%X)",
                      ctx->a5World.jtBase, a5, info->jtOffsetFromA5);
        return segmentA5WorldErr;
    }

    SEG_LOG_INFO("A5 world constructed successfully:");
    SEG_LOG_INFO("  a5BelowBase = 0x%08X, size = 0x%X", belowBase, info->a5BelowSize);
    SEG_LOG_INFO("  a5Base      = 0x%08X", a5);
    SEG_LOG_INFO("  a5AboveBase = 0x%08X, size = 0x%X", aboveBase, info->a5AboveSize);
    SEG_LOG_INFO("  jtBase      = 0x%08X, count = %d", ctx->a5World.jtBase, info->jtCount);

    return noErr;
}

/*
 * BuildJumpTable - Construct jump table with lazy-loading stubs
 */
OSErr BuildJumpTable(SegmentLoaderContext* ctx, const void* jtData, Size jtBytes)
{
    OSErr err;
    CPUAddr jtBase;
    UInt16 jtCount;
    const UInt8* src = (const UInt8*)jtData;

    if (!ctx || !ctx->a5World.initialized) {
        return segmentA5WorldErr;
    }

    jtBase = ctx->a5World.jtBase;
    jtCount = ctx->a5World.jtCount;

    if (jtCount == 0) {
        SEG_LOG_DEBUG("No jump table entries (jtCount=0)");
        return noErr;
    }
    if (!src || jtBytes < (Size)jtCount * JT_ENTRY_SIZE) {
        SEG_LOG_ERROR("Jump table runs past the end of CODE 0");
        return segmentBadFormat;
    }

    SEG_LOG_INFO("Copying %d jump table entries to 0x%08X", jtCount, jtBase);

    /*
     * The jump table is the application's, not ours to invent.
     *
     * It is stored in CODE 0 with every entry already in unloaded form,
     * carrying the segment it belongs to and the offset of the routine within
     * that segment. Earlier this loader synthesized the entries instead, and
     * had to guess the segment from the slot number - a rule that the segment
     * headers were free to contradict, and did. Reading what CODE 0 says
     * removes the guess and, with it, the possibility of disagreeing.
     */
    for (UInt16 i = 0; i < jtCount; i++) {
        const UInt8* entry = src + (i * JT_ENTRY_SIZE);
        CPUAddr slotAddr = jtBase + (i * ctx->a5World.jtEntrySize);

        UInt16 routineOffset = BE_Read16(entry + 0);
        UInt16 pushOpcode    = BE_Read16(entry + 2);
        SInt16 segID         = (SInt16)BE_Read16(entry + 4);
        UInt16 trapWord      = BE_Read16(entry + 6);

        /* An entry that is not a MOVE.W #seg,-(SP) followed by _LoadSeg is
         * not something this loader can honour, and jumping into it would run
         * whatever bytes happen to be there. Say so instead. */
        if (pushOpcode != 0x3F3C || trapWord != 0xA9F0) {
            SEG_LOG_ERROR("JT[%d] is not an unloaded entry (%04X %04X)",
                          i, pushOpcode, trapWord);
            return segmentBadFormat;
        }

        err = ctx->cpuBackend->MakeLazyJTStub(ctx->cpuAS, slotAddr,
                                             segID, routineOffset);
        if (err != noErr) {
            SEG_LOG_ERROR("Failed to write JT[%d]", i);
            return err;
        }

        SEG_LOG_INFO("  JT[%d] = A5%+d (0x%08X) -> CODE %d +%u", i,
                     (int)(ctx->code0Info.jtOffsetFromA5 + i * ctx->a5World.jtEntrySize),
                     slotAddr, segID, routineOffset);
    }

    SEG_LOG_INFO("All %d entries installed", jtCount);
    return noErr;
}
