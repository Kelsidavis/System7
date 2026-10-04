/*
 * SegmentLoader.c - Portable 68K Segment Loader Core
 *
 * ISA-agnostic segment loading logic for classic Mac OS applications.
 */

#include "SegmentLoader/SegmentLoader.h"
#include "SegmentLoader/CodeParser.h"
#include "SegmentLoader/SegmentLoaderLogging.h"
#include "ResourceManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "EventManager/EventManager.h"
#include "CPU/M68KInterp.h"
#include "System71StdLib.h"
#include <string.h>

/* Resource fetching - use standard Resource Manager */

/* Forward declarations */
static OSErr LoadCODE0AndSetupA5(SegmentLoaderContext* ctx);
static OSErr LoadCODE1(SegmentLoaderContext* ctx);

/*
 * SegmentLoader_Initialize - Initialize segment loader for process
 */
OSErr SegmentLoader_Initialize(ProcessControlBlock* pcb,
                               const char* cpuBackendName,
                               SegmentLoaderContext** outCtx)
{
    OSErr err;
    SegmentLoaderContext* ctx;
    const ICPUBackend* backend;

    if (!pcb || !outCtx) {
        return paramErr;
    }

    /* Get CPU backend */
    if (cpuBackendName) {
        backend = CPUBackend_Get(cpuBackendName);
    } else {
        backend = CPUBackend_GetDefault();
    }

    if (!backend) {
        return segmentLoaderErr;
    }

    /* Allocate context */
    ctx = (SegmentLoaderContext*)NewPtr(sizeof(SegmentLoaderContext));
    if (!ctx) {
        return memFullErr;
    }

    memset(ctx, 0, sizeof(SegmentLoaderContext));

    /* Initialize context */
    ctx->pcb = pcb;
    ctx->cpuBackend = backend;
    ctx->resFileRefNum = -1;

    /* Create CPU address space */
    err = backend->CreateAddressSpace(pcb, &ctx->cpuAS);
    if (err != noErr) {
        DisposePtr((Ptr)ctx);
        return err;
    }

    ctx->initialized = true;
    ctx->launchTime = TickCount();

    *outCtx = ctx;
    return noErr;
}

/*
 * SegmentLoader_Cleanup - Clean up segment loader
 */
OSErr SegmentLoader_Cleanup(SegmentLoaderContext* ctx)
{
    if (!ctx) {
        return paramErr;
    }

    /* Unload all segments */
    for (UInt16 i = 0; i < ctx->numSegments; i++) {
        if (ctx->segments[i].handle) {
            ctx->cpuBackend->UnmapExecutable(ctx->cpuAS,
                                            ctx->segments[i].handle);
        }
    }

    /* Destroy CPU address space */
    if (ctx->cpuAS) {
        ctx->cpuBackend->DestroyAddressSpace(ctx->cpuAS);
    }

    /* Close resource file */
    if (ctx->resFileRefNum >= 0) {
        CloseResFile(ctx->resFileRefNum);
    }

    DisposePtr((Ptr)ctx);
    return noErr;
}

/*
 * EnsureEntrySegmentsLoaded - Load CODE 0 and CODE 1
 */
OSErr EnsureEntrySegmentsLoaded(SegmentLoaderContext* ctx)
{
    OSErr err;

    if (!ctx || !ctx->initialized) {
        return paramErr;
    }

    /* Load CODE 0 and set up A5 world */
    err = LoadCODE0AndSetupA5(ctx);
    if (err != noErr) {
        return err;
    }

    /* Load CODE 1 (main entry segment) */
    err = LoadCODE1(ctx);
    if (err != noErr) {
        return err;
    }

    return noErr;
}

/*
 * LoadCODE0AndSetupA5 - Load CODE 0 and construct A5 world
 */
static OSErr LoadCODE0AndSetupA5(SegmentLoaderContext* ctx)
{
    Handle code0Handle;
    OSErr err;
    CODE0Info info;

    if (!ctx) {
        return paramErr;
    }

    /* Load CODE 0 resource */
    code0Handle = GetResource(FOURCC('C','O','D','E'), 0);
    if (!code0Handle) {
        SEG_LOG_ERROR("CODE 0 resource not found");
        return segmentNotFound;
    }
    SEG_LOG_INFO("CODE 0 resource loaded: handle=%p", code0Handle);

    /* Lock handle */
    HLock(code0Handle);

    /* Parse CODE 0 */
    err = ParseCODE0(*code0Handle, GetHandleSize(code0Handle), &info);
    if (err != noErr) {
        HUnlock(code0Handle);
        ReleaseResource(code0Handle);
        return err;
    }

    /* Defensive clamp: guard against bogus CODE 0 sizes */
    const UInt32 MAX_A5 = 1 * 1024 * 1024; /* 1MB guard */
    if (info.a5BelowSize > MAX_A5 || info.a5AboveSize > MAX_A5) {
        /* Both sizes are UInt32: %u would pass 4-byte ints to printf. */
        SEG_LOG_ERROR("CODE 0 sizes too large: below=%lu above=%lu",
                      (unsigned long)info.a5BelowSize, (unsigned long)info.a5AboveSize);
        HUnlock(code0Handle);
        ReleaseResource(code0Handle);
        return memFullErr;
    }

    /* Store CODE 0 info */
    ctx->code0Info = info;

    /* Set up A5 world */
    err = InstallA5World(ctx, &info);
    if (err != noErr) {
        HUnlock(code0Handle);
        ReleaseResource(code0Handle);
        return err;
    }

    /* Build the jump table from the copy CODE 0 carries, which follows its
     * header. The handle is still locked, so that copy is still there. */
    err = BuildJumpTable(ctx,
                         (const UInt8*)*code0Handle + CODE0_HEADER_SIZE,
                         GetHandleSize(code0Handle) - CODE0_HEADER_SIZE);
    if (err != noErr) {
        HUnlock(code0Handle);
        ReleaseResource(code0Handle);
        return err;
    }

    /* Clean up CODE 0 (not needed after A5 world is built) */
    HUnlock(code0Handle);
    ReleaseResource(code0Handle);

    return noErr;
}

/*
 * LoadCODE1 - Load CODE 1 (main entry segment)
 */
static OSErr LoadCODE1(SegmentLoaderContext* ctx)
{
    return LoadSegment(ctx, 1);
}

/*
 * LoadSegment - Load CODE segment on demand
 */
OSErr LoadSegment(SegmentLoaderContext* ctx, SInt16 segID)
{
    Handle codeHandle;
    OSErr err;
    CODEInfo info;
    CPUCodeHandle cpuHandle;
    CPUAddr baseAddr, entryAddr;
    const UInt8* codeData;
    Size codeSize;

    if (!ctx || !ctx->initialized) {
        return paramErr;
    }

    if (segID < 0 || segID >= kMaxSegments) {
        return paramErr;
    }

    if (segID < ctx->numSegments) {
        CodeSegment* segment = &ctx->segments[segID];
        if (segment->state == kSegmentLoaded) {
            SEG_LOG_DEBUG("Segment %d already loaded, skipping", segID);
            return noErr;
        }
        if (segment->state == kSegmentPurgeable && segment->handle) {
            segment->state = kSegmentLoaded;
            segment->purgeable = false;
            segment->refCount = 1;
            return PatchSegmentJumpTable(ctx, segID);
        }
    }

    SEG_LOG_INFO("Loading CODE %d...", segID);

    /* Load CODE resource */
    codeHandle = GetResource(FOURCC('C','O','D','E'), segID);
    if (!codeHandle) {
        SEG_LOG_ERROR("CODE %d resource not found", segID);
        return segmentNotFound;
    }
    /* GetHandleSize returns Size (long): %u would pass a 4-byte int. */
    SEG_LOG_INFO("CODE %d resource loaded: handle=%p size=%lu", segID, codeHandle,
                 codeHandle ? (unsigned long)GetHandleSize(codeHandle) : 0UL);

    HLock(codeHandle);
    codeData = (const UInt8*)*codeHandle;
    codeSize = GetHandleSize(codeHandle);

    /* Parse CODE N */
    err = ParseCODEN(codeData, codeSize, segID, &info);
    if (err != noErr) {
        HUnlock(codeHandle);
        ReleaseResource(codeHandle);
        return err;
    }

    /* Skip header and prologue - validate size to prevent underflow */
    Size headerAndPrologue = CODEN_HEADER_SIZE + info.prologueSkip;
    if (codeSize < headerAndPrologue) {
        SEG_LOG_ERROR("CODE %d too small: size=%lu, header+prologue=%lu",
                      segID, (unsigned long)codeSize, (unsigned long)headerAndPrologue);
        HUnlock(codeHandle);
        ReleaseResource(codeHandle);
        return segmentBadFormat;
    }
    const UInt8* executableCode = codeData + headerAndPrologue;
    Size executableSize = codeSize - headerAndPrologue;

    /* Map into CPU address space */
    err = ctx->cpuBackend->MapExecutable(ctx->cpuAS, executableCode,
                                        executableSize,
                                        kCPUMapExecutable | kCPUMapLocked,
                                        &cpuHandle, &baseAddr);
    if (err != noErr) {
        HUnlock(codeHandle);
        ReleaseResource(codeHandle);
        return err;
    }

    /* Build relocation table */
    err = BuildRelocationTable(executableCode, executableSize, segID,
                               &info.relocTable);
    if (err != noErr) {
        ctx->cpuBackend->UnmapExecutable(ctx->cpuAS, cpuHandle);
        HUnlock(codeHandle);
        ReleaseResource(codeHandle);
        return err;
    }

    /* Apply relocations */
    err = ctx->cpuBackend->Relocate(ctx->cpuAS, cpuHandle, &info.relocTable,
                                   baseAddr, ctx->a5World.jtBase,
                                   ctx->a5World.a5Base);
    if (err != noErr) {
        FreeRelocationTable(&info.relocTable);
        ctx->cpuBackend->UnmapExecutable(ctx->cpuAS, cpuHandle);
        HUnlock(codeHandle);
        ReleaseResource(codeHandle);
        return err;
    }

    /* Calculate entry point */
    /* A near model segment's code begins where the header ends; it is entered
     * through a jump table entry, not at a header-declared offset. */
    entryAddr = baseAddr;

    /* Store segment descriptor */
    if (segID >= ctx->numSegments) {
        /* Initialize any skipped segments to prevent access to uninitialized memory */
        for (UInt16 i = ctx->numSegments; i < segID; i++) {
            memset(&ctx->segments[i], 0, sizeof(CodeSegment));
        }
        ctx->numSegments = segID + 1;
    }

    ctx->segments[segID].handle = cpuHandle;
    ctx->segments[segID].baseAddr = baseAddr;
    ctx->segments[segID].entryAddr = entryAddr;
    /* Remember which jump table entries this segment owns; the segment says
     * so itself, which is what makes a rule unnecessary. */
    ctx->segments[segID].firstJTEntry = info.firstJTEntry;
    ctx->segments[segID].jtEntryCount = info.jtEntryCount;
    ctx->segments[segID].size = executableSize;
    ctx->segments[segID].state = kSegmentLoaded;
    ctx->segments[segID].purgeable = false;
    ctx->segments[segID].segID = segID;
    ctx->segments[segID].refCount = 1;

    /* Its jump table entries now go to it - which is what loading a segment
     * means to the program, whose calls all go through the table */
    err = PatchSegmentJumpTable(ctx, segID);
    if (err != noErr) {
        FreeRelocationTable(&info.relocTable);
        HUnlock(codeHandle);
        ReleaseResource(codeHandle);
        return err;
    }

    SEG_LOG_INFO("CODE %d loaded successfully:", segID);
    /* baseAddr is CPUAddr (uint32_t): %X would pass a 4-byte int. */
    SEG_LOG_INFO("  baseAddr  = 0x%08lX", (unsigned long)baseAddr);
    /* entryAddr is CPUAddr and the division/sum is Size (long):
     * %X/%u would pass 4-byte ints where printf reads 8. */
    SEG_LOG_INFO("  entryAddr = 0x%08lX, JT entries %lu..%lu",
                 (unsigned long)entryAddr,
                 (unsigned long)(info.firstJTEntry / JT_ENTRY_SIZE),
                 (unsigned long)(info.firstJTEntry / JT_ENTRY_SIZE + info.jtEntryCount));
    /* executableSize is Size (long): %X would pass a 4-byte int. */
    SEG_LOG_INFO("  size      = 0x%lX bytes", (unsigned long)executableSize);

    /* Clean up */
    FreeRelocationTable(&info.relocTable);
    HUnlock(codeHandle);
    ReleaseResource(codeHandle);

    return noErr;
}

/*
 * UnloadSegment - Unload segment (mark purgeable)
 */
OSErr UnloadSegment(SegmentLoaderContext* ctx, SInt16 segID)
{
    if (!ctx || segID < 0 || segID >= ctx->numSegments) {
        return paramErr;
    }

    CodeSegment* seg = &ctx->segments[segID];

    if (seg->state != kSegmentLoaded) {
        return noErr; /* Not loaded */
    }

    /* Decrement ref count */
    if (seg->refCount > 0) {
        seg->refCount--;
    }

    /* If ref count is zero, mark purgeable */
    if (seg->refCount == 0) {
        seg->state = kSegmentPurgeable;
        seg->purgeable = true;

        /* The mapping stays resident; purge-on-demand is not implemented. */
    }

    return noErr;
}

/*
 * ResolveJumpIndex - Resolve jump table index to address
 */
/*
 * PatchSegmentJumpTable - put a loaded segment's entries into loaded form
 *
 * Each entry still holds, at its first word, the offset of the routine it
 * points to within the segment. That word is read before it is overwritten,
 * which is what lets one segment have more than one entry point - the thing
 * the previous single-entry-per-segment scheme could not express.
 */
OSErr PatchSegmentJumpTable(SegmentLoaderContext* ctx, SInt16 segID)
{
    if (!ctx || segID < 0 || segID >= ctx->numSegments) {
        return paramErr;
    }

    const CodeSegment* seg = &ctx->segments[segID];
    UInt16 first = (UInt16)(seg->firstJTEntry / JT_ENTRY_SIZE);
    UInt16 count = (UInt16)seg->jtEntryCount;

    if (count == 0) {
        SEG_LOG_DEBUG("CODE %d owns no jump table entries", segID);
        return noErr;
    }
    if (first + count > ctx->a5World.jtCount) {
        /* first/count/jtCount are 32-bit: %u would pass 4-byte ints. */
        SEG_LOG_ERROR("CODE %d claims JT entries %lu..%lu, past the table's %lu",
                      segID, (unsigned long)first, (unsigned long)(first + count),
                      (unsigned long)ctx->a5World.jtCount);
        return segmentBadFormat;
    }

    for (UInt16 i = first; i < first + count; i++) {
        CPUAddr slotAddr = ctx->a5World.jtBase + (i * ctx->a5World.jtEntrySize);
        UInt8 head[4];
        OSErr err = ctx->cpuBackend->ReadMemory(ctx->cpuAS, slotAddr, head, 4);
        if (err != noErr) {
            return err;
        }

        /* An entry already in loaded form has a JMP where the push was, and
         * no longer holds the offset - patching it again would use the
         * segment number as one. Leave it alone. */
        if (BE_Read16(head + 2) == 0x4EF9) {
            continue;
        }

        UInt16 routineOffset = BE_Read16(head + 0);
        CPUAddr target = seg->baseAddr + routineOffset;

        err = ctx->cpuBackend->WriteJumpTableSlot(ctx->cpuAS, slotAddr,
                                                 segID, target);
        if (err != noErr) {
            SEG_LOG_ERROR("Could not patch JT[%d] for CODE %d", i, segID);
            return err;
        }
        /* routineOffset is UInt16 (int), target is CPUAddr (uint32_t):
     * %u/%X would pass 4-byte ints where printf reads 8. */
        SEG_LOG_INFO("JT[%d] -> CODE %d +%lu (0x%08lX)", i, segID,
                     (unsigned long)routineOffset, (unsigned long)target);
    }

    return noErr;
}

OSErr ResolveJumpIndex(SegmentLoaderContext* ctx, SInt16 jtIndex,
                      CPUAddr* outAddr)
{
    CPUAddr slotAddr;
    UInt8 slotData[8];
    OSErr err;

    if (!ctx || !outAddr || !ctx->a5World.initialized) {
        return paramErr;
    }

    if (jtIndex < 0 || jtIndex >= ctx->a5World.jtCount) {
        return segmentJTErr;
    }

    /* Calculate slot address */
    slotAddr = ctx->a5World.jtBase + (jtIndex * ctx->a5World.jtEntrySize);

    /* Read slot */
    err = ctx->cpuBackend->ReadMemory(ctx->cpuAS, slotAddr, slotData, 8);
    if (err != noErr) {
        return err;
    }

    /* In both 68K forms, the executable instruction begins at +2. */
    UInt16 opcode = BE_Read16(slotData + 2);
    if (opcode == 0x4EF9) {
        /* JMP absolute.L - target is already resolved */
        *outAddr = BE_Read32(slotData + 4);
    } else if (opcode == 0x3F3C) {
        /* Lazy stub - need to load segment first */
        UInt16 segID = BE_Read16(slotData + 4);
        err = LoadSegment(ctx, segID);
        if (err != noErr) {
            return err;
        }

        /* Re-read slot after loading */
        err = ctx->cpuBackend->ReadMemory(ctx->cpuAS, slotAddr, slotData, 8);
        if (err != noErr) {
            return err;
        }

        if (BE_Read16(slotData + 2) != 0x4EF9) {
            return segmentJTErr;
        }
        *outAddr = BE_Read32(slotData + 4);
    } else {
        return segmentJTErr;
    }

    return noErr;
}

/*
 * _LoadSeg (0xA9F0): load the segment whose number is on the stack.
 *
 * A call through an unloaded jump table entry runs the entry's own two
 * instructions, MOVE.W #seg,-(SP) and _LoadSeg. Loading the segment patches
 * the entry into a JMP to the routine, so execution goes back to the entry
 * and takes the JMP - on to the routine that was called, with the caller's
 * return address still on the stack. It was registered through a cast from
 * a one-argument function, so it never had the PC to go back with.
 */
static OSErr LoadSeg_TrapHandler(void* trapCtx, CPUAddr* pc, CPUAddr* registers)
{
    SegmentLoaderContext* ctx = (SegmentLoaderContext*)trapCtx;
    UInt8 word[2];
    (void)registers;

    if (!ctx || !ctx->cpuBackend || !ctx->cpuAS || !pc) {
        return segmentLoaderErr;
    }
    M68KAddressSpace* mas = (M68KAddressSpace*)ctx->cpuAS;
    if (ctx->cpuBackend->ReadMemory(ctx->cpuAS, mas->regs.a[7], word, 2) != noErr) {
        return segmentLoaderErr;
    }
    SInt16 segID = (SInt16)BE_Read16(word);
    mas->regs.a[7] += 2;

    OSErr err = LoadSegment(ctx, segID);
    if (err != noErr) {
        /* err is OSErr (long): %d would pass a 4-byte int to printf. */
        SEG_LOG_ERROR("_LoadSeg: CODE %d could not be loaded: %ld", segID, (long)err);
        return err;
    }

    /* Called from a jump table entry: take the JMP it now holds */
    CPUAddr entry = *pc - 6;
    if (entry >= ctx->a5World.jtBase &&
        entry < ctx->a5World.jtBase + ctx->a5World.jtCount * ctx->a5World.jtEntrySize &&
        ctx->cpuBackend->ReadMemory(ctx->cpuAS, entry, word, 2) == noErr &&
        BE_Read16(word) == 0x4EF9) {
        *pc = entry;
    }
    return noErr;
}

/*
 * InstallLoadSegTrap - Install _LoadSeg trap handler
 */
OSErr InstallLoadSegTrap(SegmentLoaderContext* ctx)
{
    if (!ctx) {
        return paramErr;
    }

    /* _LoadSeg is trap 0xA9F0 */
    /* Install trap handler using CPU backend if available */
    if (ctx->cpuBackend && ctx->cpuBackend->InstallTrap) {
        ctx->cpuBackend->InstallTrap(ctx->cpuAS, 0xA9F0, LoadSeg_TrapHandler, ctx);
        SEG_LOG_INFO("Installed _LoadSeg trap handler at 0xA9F0");
        return noErr;
    }

    /* Without a CPU backend that takes traps there is nowhere to install one.
     * This used to call TrapDispatcher_SetTrapAddress, which existed only as
     * a stub answering failure - there is no separate trap dispatcher. */
    SEG_LOG_ERROR("No CPU backend to install the _LoadSeg trap in");
    return segmentLoaderErr;
}

/*
 * GetSegmentEntryPoint - Get entry point for segment
 */
OSErr GetSegmentEntryPoint(SegmentLoaderContext* ctx, SInt16 segID,
                          CPUAddr* outEntry)
{
    if (!ctx || !outEntry) {
        return paramErr;
    }

    if (segID < 0 || segID >= ctx->numSegments) {
        return paramErr;
    }

    if (ctx->segments[segID].state != kSegmentLoaded) {
        return segmentNotFound;
    }

    *outEntry = ctx->segments[segID].entryAddr;
    return noErr;
}
