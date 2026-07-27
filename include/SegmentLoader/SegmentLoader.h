/*
 * SegmentLoader.h - Portable 68K Segment Loader Interface
 *
 * Implements classic Mac OS System 7.1 segment loading semantics:
 * - CODE resource parsing (CODE 0 = A5 world metadata + jump table)
 * - A5 world construction (below/above A5 layout)
 * - Jump table management with lazy loading
 * - Segment relocation and fixups
 *
 * Design:
 * - Completely ISA-agnostic (works through ICPUBackend interface)
 * - Big-endian safe CODE parsing
 * - Portable relocation table representation
 * - Clean error handling with OSErr
 */

#ifndef SEGMENT_LOADER_H
#define SEGMENT_LOADER_H

#include "SystemTypes.h"
#include "CPU/CPUBackend.h"
#include "ProcessMgr/ProcessMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Maximum segments per application
 */
#define kMaxSegments 256

/*
 * Segment Loader Error Codes
 */
enum {
    segmentLoaderErr    = -700,  /* Generic segment loader error */
    segmentNotFound     = -701,  /* CODE resource not found */
    segmentBadFormat    = -702,  /* Malformed CODE resource */
    segmentRelocErr     = -703,  /* Relocation failed */
    segmentA5WorldErr   = -704,  /* A5 world setup failed */
    segmentJTErr        = -705   /* Jump table error */
};

/*
 * CODE 0 Information (A5 World Metadata)
 *
 * CODE 0 contains critical metadata for A5 world setup:
 * - AboveA5Size: Size of area above A5 (jump table + params)
 * - BelowA5Size: Size of area below A5 (app globals)
 * - JTOffsetFromA5: Offset from A5 to jump table start
 * - JTCount: Number of jump table entries
 */
typedef struct CODE0Info {
    UInt32 a5BelowSize;       /* Size below A5 (application globals) */
    UInt32 a5AboveSize;       /* Size above A5 (jump table + params) */
    UInt32 jtOffsetFromA5;    /* Jump table offset from A5 */
    UInt16 jtCount;           /* Number of jump table entries */
    UInt16 jtEntrySize;       /* Size per JT entry (typically 8 bytes) */

    /* Future fields for advanced features */
    UInt16 flags;             /* CODE 0 flags */
    UInt16 reserved;
} CODE0Info;

/*
 * CODE N Information (Segment Metadata)
 *
 * Each CODE resource (1, 2, ...) contains executable code with:
 * - Entry offset (where to jump to start execution)
 * - Relocation table (how to fix up addresses)
 * - Prologue skip (linker-generated header bytes to skip)
 */
typedef struct CODEInfo {
    UInt32 entryOffset;       /* Offset to entry point within segment */
    UInt32 prologueSkip;      /* Bytes to skip before real code */
    RelocTable relocTable;    /* Relocation entries */
    UInt32 codeSize;          /* Size of code data */
    UInt16 segID;             /* Segment ID */
    UInt16 flags;             /* Segment flags */
} CODEInfo;

/*
 * Segment State
 */
typedef enum {
    kSegmentUnloaded   = 0,
    kSegmentLoading    = 1,
    kSegmentLoaded     = 2,
    kSegmentPurgeable  = 3
} SegmentState;

/*
 * Code Segment Descriptor (per-process segment table entry)
 */
typedef struct CodeSegment {
    CPUCodeHandle handle;     /* CPU-specific code handle */
    CPUAddr baseAddr;         /* Base address in CPU space */
    CPUAddr entryAddr;        /* Entry point address */
    UInt32 size;              /* Segment size */
    SegmentState state;       /* Current state */
    Boolean purgeable;        /* Can be unloaded */
    UInt16 segID;             /* Segment ID */
    UInt16 refCount;          /* Reference count */
} CodeSegment;

/*
 * A5 World Layout (per-process)
 */
typedef struct A5World {
    CPUAddr a5Base;           /* A5 register value */
    CPUAddr a5BelowBase;      /* Start of below-A5 area */
    CPUAddr a5AboveBase;      /* Start of above-A5 area */
    UInt32 a5BelowSize;       /* Size of below-A5 area */
    UInt32 a5AboveSize;       /* Size of above-A5 area */
    CPUAddr jtBase;           /* Jump table base address */
    UInt16 jtCount;           /* Number of JT entries */
    UInt16 jtEntrySize;       /* Size per JT entry */
    Boolean initialized;      /* A5 world set up */
} A5World;

/*
 * Segment Loader Context (per-process)
 *
 * Extends ProcessControlBlock with segment loader state
 */
typedef struct SegmentLoaderContext {
    ProcessControlBlock* pcb;      /* Parent process */
    CPUAddressSpace cpuAS;         /* CPU address space */
    const ICPUBackend* cpuBackend; /* CPU backend interface */

    A5World a5World;               /* A5 world layout */
    CodeSegment segments[kMaxSegments]; /* Segment table */
    UInt16 numSegments;            /* Number of loaded segments */

    SInt16 resFileRefNum;          /* Resource file ref for app */
    CODE0Info code0Info;           /* Parsed CODE 0 metadata */

    Boolean initialized;           /* Loader initialized */
    UInt32 launchTime;             /* Launch timestamp */
} SegmentLoaderContext;

/*
 * Segment Loader Public API (ISA-agnostic)
 */

/*
 * SegmentLoader_Initialize - Initialize segment loader for process
 *
 * @param pcb               Process control block
 * @param cpuBackendName    Name of CPU backend ("m68k_interp", "ppc_jit", etc.)
 * @param ctx               Output loader context
 * @return                  OSErr (noErr on success)
 */
OSErr SegmentLoader_Initialize(ProcessControlBlock* pcb,
                               const char* cpuBackendName,
                               SegmentLoaderContext** ctx);

/*
 * SegmentLoader_Cleanup - Clean up segment loader
 */
OSErr SegmentLoader_Cleanup(SegmentLoaderContext* ctx);

/*
 * EnsureEntrySegmentsLoaded - Load CODE 0 and CODE 1 for launch
 *
 * This is the main entry point called by LaunchApplication to:
 * 1. Load CODE 0 and parse A5 world metadata
 * 2. Construct A5 world (allocate below/above A5, build jump table)
 * 3. Load CODE 1 (main entry segment)
 * 4. Apply relocations
 * 5. Set up initial stack frame
 *
 * @param ctx               Segment loader context
 * @return                  OSErr (noErr on success)
 */
OSErr EnsureEntrySegmentsLoaded(SegmentLoaderContext* ctx);

/*
 * LoadSegment - Load CODE segment on demand
 *
 * Called by:
 * - EnsureEntrySegmentsLoaded for CODE 1
 * - _LoadSeg trap handler for lazy segment loads
 *
 * @param ctx               Segment loader context
 * @param segID             Segment ID to load
 * @return                  OSErr (noErr on success)
 */
OSErr LoadSegment(SegmentLoaderContext* ctx, SInt16 segID);

/*
 * UnloadSegment - Unload segment (mark purgeable)
 *
 * @param ctx               Segment loader context
 * @param segID             Segment ID to unload
 * @return                  OSErr
 */
OSErr UnloadSegment(SegmentLoaderContext* ctx, SInt16 segID);

/*
 * ResolveJumpIndex - Resolve jump table index to executable address
 *
 * @param ctx               Segment loader context
 * @param jtIndex           Jump table index
 * @param outAddr           Output executable address
 * @return                  OSErr
 */
OSErr ResolveJumpIndex(SegmentLoaderContext* ctx, SInt16 jtIndex,
                      CPUAddr* outAddr);

/*
 * InstallLoadSegTrap - Install _LoadSeg trap handler
 *
 * Installs trap handler that intercepts jump table misses and
 * loads segments on demand.
 *
 * @param ctx               Segment loader context
 * @return                  OSErr
 */
OSErr InstallLoadSegTrap(SegmentLoaderContext* ctx);

/*
 * GetSegmentEntryPoint - Get entry point for segment
 *
 * @param ctx               Segment loader context
 * @param segID             Segment ID
 * @param outEntry          Output entry address
 * @return                  OSErr
 */
OSErr GetSegmentEntryPoint(SegmentLoaderContext* ctx, SInt16 segID,
                          CPUAddr* outEntry);

/*
 * Internal API (exposed for testing)
 */

/*
 * ParseCODE0 - Parse CODE 0 resource to extract A5 world metadata
 *
 * @param code0Data         CODE 0 resource data
 * @param size              Size of CODE 0 data
 * @param info              Output CODE 0 info
 * @return                  OSErr
 */
OSErr ParseCODE0(const void* code0Data, Size size, CODE0Info* info);

/*
 * ParseCODEN - Parse CODE N resource to extract segment metadata
 *
 * @param codeData          CODE N resource data
 * @param size              Size of CODE data
 * @param segID             Segment ID
 * @param info              Output CODE info
 * @return                  OSErr
 */
OSErr ParseCODEN(const void* codeData, Size size, SInt16 segID,
                CODEInfo* info);

/*
 * InstallA5World - Set up A5 world memory layout
 *
 * @param ctx               Segment loader context
 * @param info              CODE 0 info
 * @return                  OSErr
 */
OSErr InstallA5World(SegmentLoaderContext* ctx, const CODE0Info* info);

/*
 * BuildJumpTable - Construct jump table with lazy stubs
 *
 * @param ctx               Segment loader context
 * @return                  OSErr
 */
OSErr BuildJumpTable(SegmentLoaderContext* ctx);

#ifdef __cplusplus
}
#endif

/*
 * Which jump table slot belongs to a segment, and which segment a slot
 * belongs to.
 *
 * These were answered in two places that disagreed: the stub installer gave
 * each segment sixteen consecutive slots, and _LoadSeg looked for one slot
 * per segment. So a stub installed for one segment was patched - or not
 * patched, when the index fell outside the table - somewhere else entirely,
 * and a lazily loaded segment could never be reached through its entry.
 *
 * A real application's layout comes from its own jump table rather than a
 * rule; this is the rule for the ones built here, and having it in one place
 * is what keeps the two ends agreeing.
 */
static inline SInt16 SegLoader_SlotForSegment(SInt16 segID)
{
    return (SInt16)(segID - 1);
}

static inline SInt16 SegLoader_SegmentForSlot(SInt16 slot)
{
    return (SInt16)(slot + 1);
}

#endif /* SEGMENT_LOADER_H */
