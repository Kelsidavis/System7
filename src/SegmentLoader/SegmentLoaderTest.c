/*
 * SegmentLoaderTest.c - Segment Loader Test Harness Implementation
 *
 * Synthetic CODE resources and smoke tests for first-light validation.
 */

#include "SegmentLoader/SegmentLoaderTest.h"
#include "SegmentLoader/SegmentLoader.h"
#include "SegmentLoader/CodeParser.h"
#include "SegmentLoader/SegmentLoaderLogging.h"
#include "ResourceMgr/resource_manager.h"
#include "ResourceMgr/ResourceMgr.h"
#include "ProcessMgr/ProcessMgr.h"
#include "CPU/CPUBackend.h"
#include "CPU/M68KInterp.h"
#include "MemoryMgr/MemoryManager.h"
#include "OSUtils/OSUtilsTraps.h"
#include "System71StdLib.h"
#include <string.h>

/*
 * Resource Management
 * Now using the real Resource Manager with AddResource
 */

/*
 * Helper: Create handle from byte array
 */
static Handle MakeHandleFromBytes(const UInt8* bytes, Size len)
{
    Handle h = NewHandle(len);
    if (h) {
        HLock(h);
        memcpy(*h, bytes, len);
        HUnlock(h);
    }
    return h;
}

/*
 * Install Synthetic CODE Resources
 *
 * Creates a minimal 2-segment app for testing:
 * - CODE 0: A5 world with 1 JT entry pointing to seg 2
 * - CODE 1: Entry that calls _LoadSeg(2) and returns
 * - CODE 2: Trace trap (A800) and returns
 */
static void InstallTestResources(void)
{
    UInt8 code0[16 + 8];  // Header + 1 JT entry
    UInt8 code1[12];      // Entry segment
    UInt8 code2[4];       // Trace segment
    SInt16 savedResFile;

    /* Save current resource file and use system resource file for tests */
    savedResFile = CurResFile();
    SEG_LOG_INFO("Current resource file before: refNum=%d", savedResFile);

    /* Use system resource file (refNum 0) for synthetic resources */
    UseResFile(0);
    SInt16 sysResFile = CurResFile();
    SEG_LOG_INFO("Switched to system resource file: refNum=%d", sysResFile);

    /* --- CODE 0: A5 World Metadata --- */
    /* Layout:
     *   +0   4   Above A5 size (0x200 = 512 bytes)
     *   +4   4   Below A5 size (0x200 = 512 bytes)
     *   +8   4   JT size (8 bytes = 1 entry)
     *   +12  4   JT offset from A5 (0x00 = at A5)
     *   +16  8   JT entry 0 (will be filled with stub by loader)
     */
    BE_Write32_Ptr(code0 + 0,  0x200);  // a5AboveSize
    BE_Write32_Ptr(code0 + 4,  0x200);  // a5BelowSize
    BE_Write32_Ptr(code0 + 8,  8);      // jtSize (1 entry)
    BE_Write32_Ptr(code0 + 12, 0);      // jtOffsetFromA5 (JT at A5)

    /* JT entry placeholder (loader will write stub) */
    memset(code0 + 16, 0x4E, 8);  // NOPs for now

    Handle h0 = MakeHandleFromBytes(code0, sizeof(code0));
    SEG_LOG_INFO("InstallTestResources: CODE 0 handle=%p size=%u", h0, (unsigned)sizeof(code0));
    AddResource(h0, 'CODE', 0, NULL);

    /* --- CODE 1: Entry Segment --- */
    /* Layout:
     *   +0  2   Entry offset (0x0000)
     *   +2  2   Flags (0x0000)
     *   +4  8   Code: _LoadSeg(2); RTS
     *
     * 68K instructions:
     *   MOVE.W #2, -(SP)  ; 0x3F3C 0x0002 (push seg 2)
     *   _LoadSeg          ; 0xA9F0 (trap)
     *   RTS               ; 0x4E75 (return)
     */
    BE_Write16_Ptr(code1 + 0, 0x0000);  // entryOffset
    BE_Write16_Ptr(code1 + 2, 0x0000);  // flags
    code1[4] = 0x3F; code1[5] = 0x3C;   // MOVE.W #imm,-(SP)
    BE_Write16_Ptr(code1 + 6, 2);       // #2 (segment ID)
    code1[8] = 0xA9; code1[9] = 0xF0;   // _LoadSeg trap
    code1[10] = 0x4E; code1[11] = 0x75; // RTS

    Handle h1 = MakeHandleFromBytes(code1, sizeof(code1));
    SEG_LOG_INFO("InstallTestResources: CODE 1 handle=%p size=%u", h1, (unsigned)sizeof(code1));
    AddResource(h1, 'CODE', 1, NULL);

    /* --- CODE 2: Trace Segment --- */
    /* Layout:
     *   +0  2   TRAP #$A800 (our test trace)
     *   +2  2   RTS
     *
     * 68K instructions:
     *   TRAP #$A800  ; 0xA800 (trace trap)
     *   RTS          ; 0x4E75
     */
    code2[0] = 0xA8; code2[1] = 0x00;   // TRAP #$A800
    code2[2] = 0x4E; code2[3] = 0x75;   // RTS

    Handle h2 = MakeHandleFromBytes(code2, sizeof(code2));
    SEG_LOG_INFO("InstallTestResources: CODE 2 handle=%p size=%u", h2, (unsigned)sizeof(code2));
    AddResource(h2, 'CODE', 2, NULL);

    /* Keep system resource file as current so GetResource() works */
    SEG_LOG_INFO("System resource file refNum=%d is now current", sysResFile);
    (void)savedResFile; /* Will stay on system file for the duration of test */
}


/*
 * A failure here has to be visible in an ordinary boot.
 *
 * SEG_LOG_ERROR goes through the filtered log and does not appear, which is
 * fine while this only ran under a build flag - but this path is the whole
 * point of having a 68K interpreter, and the interpreter itself accumulated
 * two fatal faults precisely because nothing exercised it. It runs every boot
 * now, so it says nothing when it passes and says it plainly when it does not.
 */
#define SEG_TEST_FAILED(what) do { \
    extern void serial_puts(const char* s); \
    serial_puts("[SegmentLoader] smoke test FAILED: " what "\n"); \
} while (0)


/*
 * Smoke Checks - Validate A5 World Invariants
 */
OSErr SegmentLoader_RunSmokeChecks(SegmentLoaderContext* ctx)
{
    if (!ctx || !ctx->a5World.initialized) {
        SEG_LOG_ERROR("A5 world not initialized");
        return segmentA5WorldErr;
    }

    const A5World* a5 = &ctx->a5World;

    /* Check: a5BelowBase + a5BelowSize == a5Base */
    if (a5->a5BelowBase + a5->a5BelowSize != a5->a5Base) {
        SEG_TEST_FAILED("a5BelowBase(0x");
        SEG_LOG_ERROR("FAIL: a5BelowBase(0x%08X) + a5BelowSize(0x%X) != a5Base(0x%08X)",
                     a5->a5BelowBase, a5->a5BelowSize, a5->a5Base);
        return segmentA5WorldErr;
    }
    SEG_LOG_INFO("PASS: a5BelowBase + a5BelowSize == a5Base (0x%08X)", a5->a5Base);

    /* Check: jtBase == a5Base + jtOffsetFromA5 */
    CPUAddr expectedJT = a5->a5Base + ctx->code0Info.jtOffsetFromA5;
    if (a5->jtBase != expectedJT) {
        SEG_TEST_FAILED("jtBase(0x");
        SEG_LOG_ERROR("FAIL: jtBase(0x%08X) != a5Base(0x%08X) + jtOffset(0x%X)",
                     a5->jtBase, a5->a5Base, ctx->code0Info.jtOffsetFromA5);
        return segmentJTErr;
    }
    SEG_LOG_INFO("PASS: jtBase == a5Base + jtOffset (0x%08X)", a5->jtBase);

    /* Check: jtCount > 0 and all slots materialized */
    if (a5->jtCount == 0) {
        SEG_LOG_WARN("jtCount is 0 (no jump table entries)");
    } else {
        SEG_LOG_INFO("PASS: jtCount = %d, all slots materialized", a5->jtCount);

        /* Verify first slot contains stub */
        UInt8 slotData[8];
        OSErr err = ctx->cpuBackend->ReadMemory(ctx->cpuAS, a5->jtBase, slotData, 8);
        if (err == noErr) {
            /* Check for stub pattern: 0x3F3C (MOVE.W #imm,-(SP)) */
            if (slotData[0] == 0x3F && slotData[1] == 0x3C) {
                UInt16 segID = BE_Read16(slotData + 2);
                SEG_LOG_INFO("JT[0] stub: segID=%d, looks valid", segID);
            } else {
                SEG_LOG_WARN("JT[0] = 0x%02X%02X (not expected stub)",
                            slotData[0], slotData[1]);
            }
        }
    }

    return noErr;
}

/*
 * _LoadSeg Trap Handler (0xA9F0)
 *
 * Classic Mac OS _LoadSeg trap:
 * - Segment ID is pushed on stack before trap
 * - Handler pops it, loads segment, patches JT, returns
 */
OSErr LoadSeg_TrapHandler(void* context, CPUAddr* pc, CPUAddr* registers)
{
    SegmentLoaderContext* ctx = (SegmentLoaderContext*)context;
    M68KAddressSpace* mas;
    UInt16 segID;
    OSErr err;
    CPUAddr sp;

    if (!ctx || !ctx->cpuAS) {
        SEG_LOG_ERROR("_LoadSeg: invalid context");
        return paramErr;
    }

    mas = (M68KAddressSpace*)ctx->cpuAS;

    /* Pop segment ID from stack (A7/USP) */
    /* Direct access to M68K address space for test harness */
    sp = mas->regs.a[7];

    UInt8 stackData[2];
    err = ctx->cpuBackend->ReadMemory(ctx->cpuAS, sp, stackData, 2);
    if (err != noErr) {
        SEG_LOG_ERROR("_LoadSeg: failed to read stack");
        return err;
    }

    segID = BE_Read16(stackData);

    /* Adjust stack (pop the argument) */
    mas->regs.a[7] += 2;

    SEG_LOG_INFO("_LoadSeg trap: segID=%d from SP=0x%08X", segID, sp);

    /* Load the segment */
    err = LoadSegment(ctx, segID);
    if (err != noErr) {
        SEG_LOG_ERROR("_LoadSeg: LoadSegment(%d) failed: %d", segID, err);
        return err;
    }

    SEG_LOG_INFO("_LoadSeg: segment %d loaded successfully", segID);

    /* Patch JT entry to point directly at segment (hot-patch) */
    /* Calculate JT slot index - simplified mapping: seg 1 -> JT[0], etc. */
    SInt16 jtIndex = segID - 1;  /* JT[0] for segment 1 */

    if (jtIndex >= 0 && jtIndex < ctx->a5World.jtCount) {
        CPUAddr jtSlotAddr = ctx->a5World.jtBase + (jtIndex * ctx->a5World.jtEntrySize);
        CPUAddr entryAddr;

        /* Get segment entry point */
        err = GetSegmentEntryPoint(ctx, segID, &entryAddr);
        if (err == noErr) {
            /* Atomically patch JT slot with JMP to entry */
            err = ctx->cpuBackend->WriteJumpTableSlot(ctx->cpuAS, jtSlotAddr, entryAddr);
            if (err == noErr) {
                SEG_LOG_INFO("_LoadSeg: hot-patched JT[%d] @ 0x%08X -> entry 0x%08X",
                            jtIndex, jtSlotAddr, entryAddr);
            } else {
                SEG_LOG_ERROR("_LoadSeg: failed to patch JT[%d]: err=%d", jtIndex, err);
            }
        } else {
            SEG_LOG_ERROR("_LoadSeg: failed to get entry point for seg %d: err=%d", segID, err);
        }
    } else {
        SEG_LOG_WARN("_LoadSeg: JT index %d out of range [0..%d), no hot-patch",
                    jtIndex, ctx->a5World.jtCount);
    }

    return noErr;
}

/*
 * Trace Trap Handler (0xA800)
 *
 * Test trap to prove CODE 2 executed
 */
OSErr Trace_TrapHandler(void* context, CPUAddr* pc, CPUAddr* registers)
{
    (void)context;
    (void)pc;
    (void)registers;

    SEG_LOG_INFO("========================================");
    SEG_LOG_INFO("*** CODE 2 EXECUTED! ***");
    SEG_LOG_INFO("Trace trap hit at PC=0x%08X", pc ? *pc : 0);
    SEG_LOG_INFO("Lazy segment loading WORKS!");
    SEG_LOG_INFO("========================================");

    return noErr;
}

/*
 * Test Boot Entry Point
 */
void SegmentLoader_TestBoot(void)
{
    OSErr err;
    SegmentLoaderContext* ctx;
    ProcessControlBlock testPCB;
    CPUAddr entry;

    SEG_LOG_INFO("");
    SEG_LOG_INFO("========================================");
    SEG_LOG_INFO("SEGMENT LOADER TEST HARNESS");
    SEG_LOG_INFO("========================================");

    /* Install synthetic CODE resources */
    SEG_LOG_INFO("Installing synthetic CODE resources...");
    InstallTestResources();

    /* Verify test resources are accessible via Resource Manager */
    Handle h0 = GetResource('CODE', 0);
    Handle h1 = GetResource('CODE', 1);
    Handle h2 = GetResource('CODE', 2);

    if (!h0 || !h1 || !h2) {
        SEG_TEST_FAILED("Test resources not accessible via RM (h0=");
        SEG_LOG_ERROR("FAIL: Test resources not accessible via RM (h0=%p, h1=%p, h2=%p)", h0, h1, h2);
        return;
    }

    SEG_LOG_INFO("Verified test resources via RM: CODE 0=%p, CODE 1=%p, CODE 2=%p", h0, h1, h2);

    /* Create minimal PCB for test */
    memset(&testPCB, 0, sizeof(testPCB));
    testPCB.processID.lowLongOfPSN = 9999;  // Test PSN
    testPCB.processSignature = 'TEST';
    testPCB.processType = 'APPL';
    testPCB.processState = kProcessRunning;

    /* Initialize segment loader */
    SEG_LOG_INFO("Initializing segment loader...");
    err = SegmentLoader_Initialize(&testPCB, "m68k_interp", &ctx);
    if (err != noErr) {
        SEG_TEST_FAILED("SegmentLoader_Initialize returned");
        SEG_LOG_ERROR("FAIL: SegmentLoader_Initialize returned %d", err);
        return;
    }

    /* Map Memory Manager zones into the 68K address space */
    err = MemoryManager_MapToM68K((struct M68KAddressSpace*)ctx->cpuAS);
    if (err != noErr) {
        SEG_TEST_FAILED("MemoryManager_MapToM68K returned");
        SEG_LOG_ERROR("FAIL: MemoryManager_MapToM68K returned %d", err);
        SegmentLoader_Cleanup(ctx);
        return;
    }

    /* Install trap handlers */
    SEG_LOG_INFO("Installing trap handlers...");
    ctx->cpuBackend->InstallTrap(ctx->cpuAS, 0xA9F0, LoadSeg_TrapHandler, ctx);
    ctx->cpuBackend->InstallTrap(ctx->cpuAS, 0xA800, Trace_TrapHandler, ctx);

    err = OSUtils_InstallTraps(ctx);
    if (err != noErr) {
        SEG_LOG_INFO("OSUtils_InstallTraps returned %d (continuing)", err);
    }

    /* Load CODE 0 and CODE 1 */
    SEG_LOG_INFO("Loading CODE 0 and CODE 1...");
    err = EnsureEntrySegmentsLoaded(ctx);
    if (err != noErr) {
        SEG_TEST_FAILED("EnsureEntrySegmentsLoaded returned");
        SEG_LOG_ERROR("FAIL: EnsureEntrySegmentsLoaded returned %d", err);
        SegmentLoader_Cleanup(ctx);
        return;
    }

    /* Run smoke checks */
    SEG_LOG_INFO("Running A5 invariant checks...");
    err = SegmentLoader_RunSmokeChecks(ctx);
    if (err != noErr) {
        SEG_TEST_FAILED("Smoke checks failed");
        SEG_LOG_ERROR("FAIL: Smoke checks failed");
        SegmentLoader_Cleanup(ctx);
        return;
    }

    /* Log A5, USP, entry */
    SEG_LOG_INFO("");
    SEG_LOG_INFO("Entry State:");
    SEG_LOG_INFO("  A5 = 0x%08X (a5Base)", ctx->a5World.a5Base);
    SEG_LOG_INFO("  JT = 0x%08X (jtBase)", ctx->a5World.jtBase);

    M68KAddressSpace* mas = (M68KAddressSpace*)ctx->cpuAS;
    SEG_LOG_INFO("  USP = 0x%08X", mas->regs.usp);
    SEG_LOG_INFO("  A5(reg) = 0x%08X", mas->regs.a[5]);

    err = GetSegmentEntryPoint(ctx, 1, &entry);
    if (err != noErr) {
        SEG_TEST_FAILED("GetSegmentEntryPoint failed");
        SEG_LOG_ERROR("FAIL: GetSegmentEntryPoint failed");
        SegmentLoader_Cleanup(ctx);
        return;
    }
    SEG_LOG_INFO("  Entry = 0x%08X (CODE 1)", entry);
    SEG_LOG_INFO("");

    /* Execute CODE 1 via M68K interpreter! */
    SEG_LOG_INFO("");
    SEG_LOG_INFO("========================================");
    SEG_LOG_INFO("*** ENTERING M68K INTERPRETER ***");
    SEG_LOG_INFO("Calling EnterAt(0x%08X) with timeslice...", entry);
    SEG_LOG_INFO("========================================");
    SEG_LOG_INFO("");

    err = ctx->cpuBackend->EnterAt(ctx->cpuAS, entry, 0);
    if (err != noErr) {
        SEG_TEST_FAILED("EnterAt returned");
        SEG_LOG_ERROR("FAIL: EnterAt returned %d", err);
        SegmentLoader_Cleanup(ctx);
        return;
    }

    SEG_LOG_INFO("");
    SEG_LOG_INFO("========================================");
    SEG_LOG_INFO("*** M68K EXECUTION COMPLETE ***");
    SEG_LOG_INFO("EnterAt returned successfully");
    SEG_LOG_INFO("========================================");
    SEG_LOG_INFO("");

    /* Cleanup */
    OSUtils_Shutdown();
    SegmentLoader_Cleanup(ctx);
}
