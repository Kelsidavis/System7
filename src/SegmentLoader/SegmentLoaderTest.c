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
    UInt8 code0[16 + 16]; // Header + 2 JT entries
    UInt8 code1[18];      // Entry segment
    UInt8 code2[24];      // Trace segment
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
     *   +16  8   JT entry 0 -> CODE 1
     *   +24  8   JT entry 1 -> CODE 2
     */
    BE_Write32_Ptr(code0 + 0,  0x200);  // a5AboveSize
    BE_Write32_Ptr(code0 + 4,  0x200);  // a5BelowSize
    BE_Write32_Ptr(code0 + 8, 16);      // jtSize: two 8-byte entries
    BE_Write32_Ptr(code0 + 12, 0);      // jtOffsetFromA5 (JT at A5)

    /*
     * The jump table, in the unloaded form CODE 0 really stores:
     *
     *   +0  offset of the routine in its segment
     *   +2  3F3C  MOVE.W #seg,-(SP)
     *   +4  segment number
     *   +6  A9F0  _LoadSeg
     *
     * Each entry names its own segment, so the loader has nothing to work out.
     */
    for (int i = 0; i < 2; i++) {
        UInt8* e = code0 + 16 + (i * 8);
        BE_Write16_Ptr(e + 0, 0);            // routine at the segment's start
        e[2] = 0x3F; e[3] = 0x3C;            // MOVE.W #imm,-(SP)
        BE_Write16_Ptr(e + 4, (UInt16)(i + 1));  // CODE 1, then CODE 2
        e[6] = 0xA9; e[7] = 0xF0;            // _LoadSeg
    }

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
    BE_Write16_Ptr(code1 + 0, 0);       // first JT entry: byte 0 of the table
    BE_Write16_Ptr(code1 + 2, 1);       // one entry belongs to this segment
    /*
     * Call CODE 2 the way an application does: one JSR through the jump
     * table, with no idea whether the segment is loaded.
     *
     * The entry it calls is still unloaded, so the JSR lands on the
     * MOVE.W/_LoadSeg inside the entry, which loads CODE 2, rewrites the
     * entry to jump straight at it, and returns into the routine. CODE 2's
     * own RTS then returns here. Nothing in this program mentions loading.
     *
     * Entries are entered two bytes in, past the offset word - hence 10 for
     * the second entry rather than 8.
     */
    code1[4] = 0x4E; code1[5] = 0xAD;   // JSR d16(A5)
    BE_Write16_Ptr(code1 + 6, 10);      // JT[1], two bytes into the entry
    code1[8] = 0x4E; code1[9] = 0x75;   // RTS
    code1[10] = 0x4E; code1[11] = 0x71; // NOP padding
    code1[12] = 0x4E; code1[13] = 0x71;
    code1[14] = 0x4E; code1[15] = 0x71;
    code1[16] = 0x4E; code1[17] = 0x71;

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
    /* Every CODE resource begins with a four-byte header; this one did not,
     * so its first instruction word was read as the entry offset. $A800 as an
     * offset put the entry forty-three thousand bytes past the segment, at an
     * address holding nothing, and the jump table was patched to point there. */
    BE_Write16_Ptr(code2 + 0, 8);       // first JT entry: byte 8, the second slot
    BE_Write16_Ptr(code2 + 2, 1);       // one entry belongs to this segment
    code2[4] = 0xA8; code2[5] = 0x00;   // TRAP #$A800
    code2[6] = 0x4E; code2[7] = 0x75;   // RTS

    /* Everything past the RTS is data for the loader to look at, not code to
     * run.
     *
     * MOVE.L $00000904,D0 reads CurrentA5 - an ordinary low memory global,
     * and the sort of thing real 68K code is full of. Its address must come
     * through untouched.
     *
     * The JSR's absolute target is the one thing here that should be
     * relocated by the segment base. */
    code2[8] = 0x20; code2[9] = 0x39;   // MOVE.L abs.L,D0
    BE_Write32_Ptr(code2 + 10, 0x00000904);
    code2[14] = 0x4E; code2[15] = 0xB9; // JSR abs.L
    BE_Write32_Ptr(code2 + 16, 0x00020000);
    code2[20] = 0x4E; code2[21] = 0x71; // NOP
    code2[22] = 0x4E; code2[23] = 0x71; // NOP

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

        /*
         * Every entry should be the unloaded form CODE 0 supplied, naming its
         * own segment. Checking all of them, not just the first, is what
         * catches the table being built from a rule instead of copied - a
         * rule that gets slot zero right can still get the rest wrong.
         */
        for (UInt16 i = 0; i < a5->jtCount; i++) {
            UInt8 slotData[8];
            CPUAddr slotAddr = a5->jtBase + (i * a5->jtEntrySize);
            if (ctx->cpuBackend->ReadMemory(ctx->cpuAS, slotAddr, slotData, 8) != noErr) {
                SEG_TEST_FAILED("could not read back a jump table entry");
                break;
            }
            if (BE_Read16(slotData + 2) != 0x3F3C || BE_Read16(slotData + 6) != 0xA9F0) {
                SEG_TEST_FAILED("a jump table entry is not in unloaded form");
                break;
            }
            if (BE_Read16(slotData + 4) != (UInt16)(i + 1)) {
                SEG_TEST_FAILED("a jump table entry names the wrong segment");
                break;
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

    /* Put every entry the segment owns into loaded form. This is the loader's
     * own routine, so the test cannot drift away from what it does. */
    err = PatchSegmentJumpTable(ctx, segID);
    if (err != noErr) {
        return err;
    }

    /*
     * Return into the routine that was called, not back to the caller.
     *
     * The call came through a jump table entry: JSR to entry+2, then the
     * MOVE.W and _LoadSeg inside it, so the PC now sits eight bytes past the
     * entry's start. Reading the entry back gives the address just patched
     * into it, and continuing there makes the load invisible to the caller -
     * which is the whole point of a lazily loaded segment. A _LoadSeg reached
     * any other way is just a load, and returns normally.
     */
    CPUAddr jtLow = ctx->a5World.jtBase;
    CPUAddr jtHigh = jtLow + (CPUAddr)ctx->a5World.jtCount * ctx->a5World.jtEntrySize;
    CPUAddr entryAddr = *pc - JT_ENTRY_SIZE;

    if (entryAddr >= jtLow && entryAddr < jtHigh &&
        ((entryAddr - jtLow) % ctx->a5World.jtEntrySize) == 0) {
        UInt8 slot[8];
        if (ctx->cpuBackend->ReadMemory(ctx->cpuAS, entryAddr, slot, 8) == noErr &&
            BE_Read16(slot + 2) == 0x4EF9) {
            CPUAddr target = ((CPUAddr)slot[4] << 24) | ((CPUAddr)slot[5] << 16) |
                             ((CPUAddr)slot[6] << 8) | (CPUAddr)slot[7];
            SEG_LOG_INFO("_LoadSeg: continuing into 0x%08X", target);
            *pc = target;
        }
    }

    return noErr;
}

/*
 * Trace Trap Handler (0xA800)
 *
 * Test trap to prove CODE 2 executed
 */
/* Set when CODE 2 runs, so the test can assert it rather than log it. */
static Boolean gTraceSegmentRan = false;

OSErr Trace_TrapHandler(void* context, CPUAddr* pc, CPUAddr* registers)
{
    (void)context;
    (void)pc;
    (void)registers;

    gTraceSegmentRan = true;

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
    /* A process needs a stack before it can run anything; the entry segment
     * pushes a segment number as its first act. */
    ctx->cpuBackend->SetStacks(ctx->cpuAS, 0x00040000, 0);

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

    /* The trap the loaded segment fires is the only thing that says the
     * segment actually ran. Logging it is not checking it - the log line is
     * filtered out of an ordinary boot, so a run that loaded nothing looked
     * exactly like a run that worked. */
    if (!gTraceSegmentRan) {
        SEG_TEST_FAILED("the loaded segment never executed");
    }

    /* The JSR target in segment 2 should have been relocated by its base. */
    {
        CPUAddr seg2 = 0;
        if (GetSegmentEntryPoint(ctx, 2, &seg2) == noErr) {
            unsigned char patched[4];
            /* The low memory reference must survive unchanged. */
            if (ctx->cpuBackend->ReadMemory(ctx->cpuAS, seg2 + 6, patched, 4) == noErr) {
                UInt32 got = ((UInt32)patched[0] << 24) | ((UInt32)patched[1] << 16) |
                             ((UInt32)patched[2] << 8) | patched[3];
                if (got != 0x00000904) {
                    char rb[150];
                    snprintf(rb, sizeof(rb),
                             "[SegmentLoader] smoke test FAILED: low memory reference "
                             "rewritten to 0x%08X, was 0x00000904\n", (unsigned)got);
                    serial_puts(rb);
                }
            }
            /* And so must the JSR's, for the same reason: a near-model
             * segment is loaded as it was written. */
            if (ctx->cpuBackend->ReadMemory(ctx->cpuAS, seg2 + 12, patched, 4) == noErr) {
                UInt32 got = ((UInt32)patched[0] << 24) | ((UInt32)patched[1] << 16) |
                             ((UInt32)patched[2] << 8) | patched[3];
                if (got != 0x00020000) {
                    char rb[150];
                    snprintf(rb, sizeof(rb),
                             "[SegmentLoader] smoke test FAILED: absolute operand "
                             "rewritten to 0x%08X, was 0x00020000\n", (unsigned)got);
                    serial_puts(rb);
                }
            }
        }
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
