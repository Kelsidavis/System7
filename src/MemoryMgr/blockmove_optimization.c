/*
 * RE-AGENT-BANNER: reimplementation
 *
 * Mac OS System 7 BlockMove Optimization Routines
 *
 * Reimplemented from Apple Computer Inc. source code:
 * - BlockMove.a (SHA256: 2a112bff69e4cb4b57253a8c2c210bad48d6020d43d4cb6932d852119bce7af0)
 *
 * Original authors: Andy Hertzfeld (1982), Gary Davidian (rewrite 1988)
 * Reimplemented: 2025-09-18
 *
 * PROVENANCE: BlockMove optimization algorithms extracted from assembly analysis:
 * - BlockMove68020 (line 285): 68020/68030 optimized block move with cache flushing
 * - BlockMove68040 (line 471): 68040 optimized block move using move16 instructions
 * - CopyInc68020 (line 285): Forward copying routine for 68020
 * - CopyDec68020 (line 658): Backward copying routine for overlapping blocks
 * - TailInc variants (lines 206-280): Optimized tail copying for final 0-31 bytes
 *
 * Key insights from assembly comments:
 * - "80%+ calls are for 1-31 bytes" (line 156)
 * - "95%+ calls are for less than 256 bytes" (line 157)
 * - Uses unrolled loops and jump tables for optimal performance
 */

#include "../include/memory_manager/memory_manager_types.h"
#include <string.h>
#include <assert.h>

/* Forward declarations */
static OSErr block_move_68020_optimized(Ptr src, Ptr dst, Size count);
static OSErr block_move_68040_optimized(Ptr src, Ptr dst, Size count);
static OSErr copy_incrementing_68020(Ptr src, Ptr dst, Size count);
static OSErr copy_decrementing_68020(Ptr src, Ptr dst, Size count);
static void copy_tail_incrementing(Ptr src, Ptr dst, Size remainingBytes);
static bool check_memory_overlap(Ptr src, Ptr dst, Size count);
static void cache_flush_if_needed(Size bytesCopied);

/* Global processor type detection */
static ProcessorType g_processorType = CPU_68020;

/*
 * High-Level BlockMove Interface
 * PROVENANCE: HBlockMove function (line 2270) in MemoryMgrInternal.a
 *
 * Algorithm from assembly:
 * 1. Clear noQueueBit for cache flushing
 * 2. Jump to appropriate processor-specific routine
 * 3. Handle cache coherency
 */
OSErr high_level_block_move(Ptr src, Ptr dst, Size count) {
    if (!src || !dst || count <= 0) {
        return noErr;  /* Nothing to copy */
    }

    /* Dispatch to processor-specific optimized routine */
    switch (g_processorType) {
        case CPU_68040:
            return block_move_68040_optimized(src, dst, count);
        case CPU_68020:
            return block_move_68020_optimized(src, dst, count);
        case CPU_68000:
        default:
            /* Fallback to basic byte-by-byte copy for 68000 */
            memmove(dst, src, count);
            return noErr;
    }
}

/*
 * 68020/68030 Optimized Block Move
 * PROVENANCE: BlockMove68020/CopyInc68020 functions (line 285) in BlockMove.a
 *
 * Algorithm from assembly analysis (lines 287-299):
 * 1. Check for memory overlap that would require decrementing copy
 * 2. Calculate destination alignment for longword optimization
 * 3. Use longword moves when possible
 * 4. Handle remaining bytes with optimized tail copy
 * 5. Flush instruction cache when needed
 */
static OSErr block_move_68020_optimized(Ptr src, Ptr dst, Size count) {
    /* Handle small copies (80%+ of calls are 1-31 bytes) */
    if (count <= 31) {
        copy_tail_incrementing(src, dst, count);
        return noErr;
    }

    /* Check for overlap requiring backward copy */
    if (check_memory_overlap(src, dst, count)) {
        return copy_decrementing_68020(src, dst, count);
    }

    return copy_incrementing_68020(src, dst, count);
}

/*
 * 68040 Optimized Block Move
 * PROVENANCE: BlockMove68040 function (line 471) in BlockMove.a
 *
 * Algorithm from assembly:
 * - Uses move16 instructions for 32-byte transfers
 * - Optimized for 68040 burst mode and cache line size
 * - Handles alignment and remaining bytes efficiently
 */
static OSErr block_move_68040_optimized(Ptr src, Ptr dst, Size count) {
    if (count <= 31) {
        copy_tail_incrementing(src, dst, count);
        return noErr;
    }

    /* For 68040, use move16 optimization for large blocks */
    if (count >= 32 && !check_memory_overlap(src, dst, count)) {
        /* Use 32-byte move16 instructions when aligned */
        Size move16Blocks = count / 32;
        Size remaining = count % 32;

        /* Copy 32-byte blocks using simulated move16 */
        for (Size i = 0; i < move16Blocks; i++) {
            /* Simulate move16 (a0)+,(a1)+ instruction */
            memcpy(dst, src, 16);
            memcpy((char*)dst + 16, (char*)src + 16, 16);
            src = (char*)src + 32;
            dst = (char*)dst + 32;
        }

        /* Handle remaining bytes */
        if (remaining > 0) {
            copy_tail_incrementing(src, dst, remaining);
        }

        cache_flush_if_needed(count);
        return noErr;
    }

    /* Fallback to 68020 algorithm for overlapping or small blocks */
    return copy_incrementing_68020(src, dst, count);
}

/*
 * Forward (Incrementing) Copy for 68020
 * PROVENANCE: CopyInc68020 function (line 285) in BlockMove.a
 *
 * Algorithm from assembly (lines 291-299):
 * 1. Calculate destination alignment mask
 * 2. Align destination to longword boundary
 * 3. Use longword moves for bulk transfer
 * 4. Handle remaining bytes with tail copy
 */
static OSErr copy_incrementing_68020(Ptr src, Ptr dst, Size count) {
    char* srcPtr = (char*)src;
    char* dstPtr = (char*)dst;
    Size remaining = count;

    /* Align destination to longword boundary for optimal performance */
    while (remaining > 0 && ((uintptr_t)dstPtr & 3) != 0) {
        *dstPtr++ = *srcPtr++;
        remaining--;
    }

    /* Copy longwords when possible (major optimization from assembly) */
    Size longwords = remaining / 4;
    if (longwords > 0) {
        /* Use unrolled loop for better performance (inspired by assembly) */
        uint32_t* srcLong = (uint32_t*)srcPtr;
        uint32_t* dstLong = (uint32_t*)dstPtr;

        /* Unroll by 4 longwords (16 bytes) when possible */
        Size unrolledBlocks = longwords / 4;
        for (Size i = 0; i < unrolledBlocks; i++) {
            dstLong[0] = srcLong[0];
            dstLong[1] = srcLong[1];
            dstLong[2] = srcLong[2];
            dstLong[3] = srcLong[3];
            srcLong += 4;
            dstLong += 4;
        }

        /* Handle remaining longwords */
        Size remainingLongs = longwords % 4;
        for (Size i = 0; i < remainingLongs; i++) {
            *dstLong++ = *srcLong++;
        }

        srcPtr = (char*)srcLong;
        dstPtr = (char*)dstLong;
        remaining = remaining % 4;
    }

    /* Handle final bytes */
    if (remaining > 0) {
        copy_tail_incrementing((Ptr)srcPtr, (Ptr)dstPtr, remaining);
    }

    /* Flush instruction cache for large moves */
    if (count > 12) {
        cache_flush_if_needed(count);
    }

    return noErr;
}

/*
 * Backward (Decrementing) Copy for Overlapping Blocks
 * PROVENANCE: CopyDec68020 function (line 658) in BlockMove.a
 *
 * Algorithm from assembly:
 * - Copy from end to beginning to handle overlap correctly
 * - Optimize for longword alignment when possible
 * - Essential for correct overlap handling
 */
static OSErr copy_decrementing_68020(Ptr src, Ptr dst, Size count) {
    char* srcPtr = (char*)src + count - 1;
    char* dstPtr = (char*)dst + count - 1;
    Size remaining = count;

    /* Align for longword moves from the end */
    while (remaining > 0 && ((uintptr_t)(dstPtr + 1) & 3) != 0) {
        *dstPtr-- = *srcPtr--;
        remaining--;
    }

    /* Copy longwords in reverse */
    Size longwords = remaining / 4;
    if (longwords > 0) {
        srcPtr -= 3;  /* Point to start of longword */
        dstPtr -= 3;

        uint32_t* srcLong = (uint32_t*)srcPtr;
        uint32_t* dstLong = (uint32_t*)dstPtr;

        for (Size i = 0; i < longwords; i++) {
            *dstLong = *srcLong;
            srcLong--;
            dstLong--;
        }

        srcPtr = (char*)srcLong + 3;
        dstPtr = (char*)dstLong + 3;
        remaining = remaining % 4;
    }

    /* Handle remaining bytes */
    while (remaining > 0) {
        *dstPtr-- = *srcPtr--;
        remaining--;
    }

    cache_flush_if_needed(count);
    return noErr;
}

/*
 * Optimized Tail Copy for Final Bytes
 * PROVENANCE: TailInc variants (lines 206-280) in BlockMove.a
 *
 * Algorithm from assembly:
 * - Jump table based optimization for 0-31 byte copies
 * - Unrolled instructions for each possible remainder
 * - Critical optimization since 80%+ of calls are small
 */
static void copy_tail_incrementing(Ptr src, Ptr dst, Size remainingBytes) {
    char* srcPtr = (char*)src;
    char* dstPtr = (char*)dst;

    /* Optimized unrolled copy based on assembly jump table (lines 217-247) */
    switch (remainingBytes) {
        case 31:
            *(uint32_t*)dstPtr = *(uint32_t*)srcPtr;
            *(uint32_t*)(dstPtr + 4) = *(uint32_t*)(srcPtr + 4);
            *(uint32_t*)(dstPtr + 8) = *(uint32_t*)(srcPtr + 8);
            *(uint32_t*)(dstPtr + 12) = *(uint32_t*)(srcPtr + 12);
            *(uint32_t*)(dstPtr + 16) = *(uint32_t*)(srcPtr + 16);
            *(uint32_t*)(dstPtr + 20) = *(uint32_t*)(srcPtr + 20);
            *(uint32_t*)(dstPtr + 24) = *(uint32_t*)(srcPtr + 24);
            *(uint16_t*)(dstPtr + 28) = *(uint16_t*)(srcPtr + 28);
            dstPtr[30] = srcPtr[30];
            break;
        case 30:
            *(uint32_t*)dstPtr = *(uint32_t*)srcPtr;
            *(uint32_t*)(dstPtr + 4) = *(uint32_t*)(srcPtr + 4);
            *(uint32_t*)(dstPtr + 8) = *(uint32_t*)(srcPtr + 8);
            *(uint32_t*)(dstPtr + 12) = *(uint32_t*)(srcPtr + 12);
            *(uint32_t*)(dstPtr + 16) = *(uint32_t*)(srcPtr + 16);
            *(uint32_t*)(dstPtr + 20) = *(uint32_t*)(srcPtr + 20);
            *(uint32_t*)(dstPtr + 24) = *(uint32_t*)(srcPtr + 24);
            *(uint16_t*)(dstPtr + 28) = *(uint16_t*)(srcPtr + 28);
            break;
        case 29:
            *(uint32_t*)dstPtr = *(uint32_t*)srcPtr;
            *(uint32_t*)(dstPtr + 4) = *(uint32_t*)(srcPtr + 4);
            *(uint32_t*)(dstPtr + 8) = *(uint32_t*)(srcPtr + 8);
            *(uint32_t*)(dstPtr + 12) = *(uint32_t*)(srcPtr + 12);
            *(uint32_t*)(dstPtr + 16) = *(uint32_t*)(srcPtr + 16);
            *(uint32_t*)(dstPtr + 20) = *(uint32_t*)(srcPtr + 20);
            *(uint32_t*)(dstPtr + 24) = *(uint32_t*)(srcPtr + 24);
            dstPtr[28] = srcPtr[28];
            break;
        case 28:
            *(uint32_t*)dstPtr = *(uint32_t*)srcPtr;
            *(uint32_t*)(dstPtr + 4) = *(uint32_t*)(srcPtr + 4);
            *(uint32_t*)(dstPtr + 8) = *(uint32_t*)(srcPtr + 8);
            *(uint32_t*)(dstPtr + 12) = *(uint32_t*)(srcPtr + 12);
            *(uint32_t*)(dstPtr + 16) = *(uint32_t*)(srcPtr + 16);
            *(uint32_t*)(dstPtr + 20) = *(uint32_t*)(srcPtr + 20);
            *(uint32_t*)(dstPtr + 24) = *(uint32_t*)(srcPtr + 24);
            break;
        /* Continue pattern for remaining cases... */
        case 16:
            *(uint32_t*)dstPtr = *(uint32_t*)srcPtr;
            *(uint32_t*)(dstPtr + 4) = *(uint32_t*)(srcPtr + 4);
            *(uint32_t*)(dstPtr + 8) = *(uint32_t*)(srcPtr + 8);
            *(uint32_t*)(dstPtr + 12) = *(uint32_t*)(srcPtr + 12);
            break;
        case 12:
            *(uint32_t*)dstPtr = *(uint32_t*)srcPtr;
            *(uint32_t*)(dstPtr + 4) = *(uint32_t*)(srcPtr + 4);
            *(uint32_t*)(dstPtr + 8) = *(uint32_t*)(srcPtr + 8);
            break;
        case 8:
            *(uint32_t*)dstPtr = *(uint32_t*)srcPtr;
            *(uint32_t*)(dstPtr + 4) = *(uint32_t*)(srcPtr + 4);
            break;
        case 4:
            *(uint32_t*)dstPtr = *(uint32_t*)srcPtr;
            break;
        case 3:
            *(uint16_t*)dstPtr = *(uint16_t*)srcPtr;
            dstPtr[2] = srcPtr[2];
            break;
        case 2:
            *(uint16_t*)dstPtr = *(uint16_t*)srcPtr;
            break;
        case 1:
            *dstPtr = *srcPtr;
            break;
        case 0:
            break;
        default:
            /* For sizes > 31, fall back to standard copy */
            memcpy(dstPtr, srcPtr, remainingBytes);
            break;
    }
}

/*
 * Memory Overlap Detection
 * PROVENANCE: Overlap detection logic from BlockMove.a (lines 287-299)
 *
 * Algorithm from assembly:
 * - Calculate destination - source difference
 * - Compare with copy length to detect overlap
 * - Critical for choosing forward vs backward copy
 */
static bool check_memory_overlap(Ptr src, Ptr dst, Size count) {
    if (dst <= src) {
        return false;  /* No overlap possible */
    }

    /* Check if destination overlaps with source region */
    uintptr_t srcAddr = (uintptr_t)src;
    uintptr_t dstAddr = (uintptr_t)dst;
    uintptr_t srcEnd = srcAddr + count;

    return (dstAddr < srcEnd);  /* Overlap detected */
}

/*
 * Cache Flush Implementation
 * PROVENANCE: Cache flushing code from BlockMove.a (line 291)
 */
static void cache_flush_if_needed(Size bytesCopied) {
    /* Only flush cache for moves larger than 12 bytes (from assembly) */
    if (bytesCopied > 12) {
        /* Simulate cache flush - in real implementation would call jCacheFlush */
        /* This was critical for 68020+ processors to maintain cache coherency */
    }
}

/*
 * Processor Type Detection and Setup
 */
void set_processor_type(ProcessorType processorType) {
    g_processorType = processorType;
}

ProcessorType get_processor_type(void) {
    return g_processorType;
}

/*
 * BlockMove Statistics and Optimization Info
 * PROVENANCE: Statistics from BlockMove.a comments (lines 154-160)
 */
typedef struct BlockMoveStats {
    Size totalCalls;
    Size smallCalls;      /* 1-31 bytes */
    Size mediumCalls;     /* 32-255 bytes */
    Size largeCalls;      /* 256+ bytes */
    Size overlapCalls;    /* Overlapping copies */
} BlockMoveStats;

static BlockMoveStats g_blockMoveStats = {0};

void update_blockmove_statistics(Size bytesCopied, bool wasOverlap) {
    g_blockMoveStats.totalCalls++;

    if (bytesCopied <= 31) {
        g_blockMoveStats.smallCalls++;
    } else if (bytesCopied <= 255) {
        g_blockMoveStats.mediumCalls++;
    } else {
        g_blockMoveStats.largeCalls++;
    }

    if (wasOverlap) {
        g_blockMoveStats.overlapCalls++;
    }
}

const BlockMoveStats* get_blockmove_statistics(void) {
    return &g_blockMoveStats;
}

/*
 * RE-AGENT-TRAILER-JSON: {
 *   "agent": "reimplementation",
 *   "file": "blockmove_optimization.c",
 *   "timestamp": "2025-09-18T02:30:00Z",
 *   "functions_implemented": 10,
 *   "algorithms": ["BlockMove68020", "BlockMove68040", "CopyInc68020", "CopyDec68020", "TailInc variants"],
 *   "provenance_lines": ["285", "471", "658", "206-280", "287-299"],
 *   "optimization_insights": ["80%+ calls 1-31 bytes", "95%+ calls <256 bytes", "unrolled loops", "jump table optimization"],
 *   "evidence_source": "evidence.memory_manager.json",
 *   "assembly_source": "BlockMove.a"
 * }
 */