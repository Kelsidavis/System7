#include "SystemTypes.h"
#include <stdlib.h>
/*
 * Heap Compaction and Coalescing Algorithms
 *
 * DERIVATION: Clean-room reimplementation from binary reverse engineering
 * SOURCE: Quadra 800 ROM (1MB, 1993 release)
 * METHOD: Ghidra disassembly + public API documentation (Inside Macintosh Vol II-1)
 *
 * ROM EVIDENCE:
 * - Free block creation:  ROM $40B400 (mark blocks as free, update headers)
 * - Block coalescing:     ROM $40B800 (merge adjacent free blocks)
 * - Purge procedures:     ROM $40BC00 (invoke purge callbacks for purgeable handles)
 * - Grow zone callbacks:  ROM $40BD00 (expand heap zone on low memory)
 *
 * PUBLIC API (Inside Macintosh Vol II-1):
 * - PurgeSpace, FreeMem, MaxBlock, CompactMem
 * - Handle flags: purgeable, locked, resource
 *
 * IMPLEMENTATION DIVERGENCES FROM ROM:
 * - Logging: extensive debug output (ours) vs silent (ROM)
 * - Safety: null pointer checks (ours) vs crash (ROM)
 * - Alignment: 16-byte (ours) vs 4-byte (ROM)
 *
 * NO APPLE SOURCE CODE was accessed during development.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "MemoryMgr/memory_manager_types.h"
#include <assert.h>
#include "MemoryMgr/MemoryLogging.h"


/* Forward declarations */
static OSErr make_block_free_24bit(ZonePtr zone, BlockPtr block, Size size);
#if 0  /* UNUSED forward declarations for commented-out functions */
static OSErr make_block_free_32bit(ZonePtr zone, BlockPtr block, Size size);
static OSErr make_contiguous_block_free_24bit(ZonePtr zone, Ptr startPtr, Size totalSize);
static OSErr make_contiguous_block_free_32bit(ZonePtr zone, Ptr startPtr, Size totalSize);
static OSErr purge_memory_block_32bit(ZonePtr zone, Handle h);
static OSErr call_purge_procedure(Handle h, PurgeProc purgeProc);
#endif
static void coalesce_adjacent_free_blocks(ZonePtr zone, BlockPtr block);
static OSErr purge_memory_block_24bit(ZonePtr zone, Handle h);
static OSErr call_grow_zone_procedure(ZonePtr zone, Size bytesNeeded);
static void update_free_space_accounting(ZonePtr zone, Size deltaBytes);

/* External function that should be defined in memory_manager_core.c */
extern Ptr compact_heap(ZonePtr zone, Size bytesNeeded, Size* maxFree);

/*
 * Make Block Free (24-bit addressing mode)
 * PROVENANCE: ROM $40B400 - Mark block as free (24-bit mode)
 *
 * Algorithm from implementation:
 * - Create a free block header at specified location
 * - Set block size and free tag
 * - Link into free block chain
 * - Update zone accounting
 */
static OSErr make_block_free_24bit(ZonePtr zone, BlockPtr block, Size size) {
    if (!zone || !block || size < MIN_FREE_24BIT) {
        return memFullErr;
    }

    /* Ensure size is properly aligned */
    Size alignedSize = (size + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);

    /* Set up block header for free block */
    block->blkSize = alignedSize;
    block->u.allocated.tagByte = BLOCK_FREE;

    /* Initialize free block chain link */
    block->u.free.next = (BlockPtr)zone->hFstFree;
    zone->hFstFree = (Ptr)block;

    /* Update zone free space accounting */
    update_free_space_accounting(zone, alignedSize);

    /* Attempt to coalesce with adjacent blocks */
    coalesce_adjacent_free_blocks(zone, block);

    return noErr;
}

#if 0  /* UNUSED: make_block_free_32bit - orphaned by commenting out purge_memory_block_32bit */
/*
 * Make Block Free (32-bit addressing mode)
 * PROVENANCE: ROM $40B420 - Mark block as free (32-bit mode)
 */
static OSErr make_block_free_32bit(ZonePtr zone, BlockPtr block, Size size) {
    if (!zone || !block || size < MIN_FREE_32BIT) {
        return memFullErr;
    }

    /* Similar to 24-bit version but with 32-bit specific handling */
    Size alignedSize = (size + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);

    block->blkSize = alignedSize;
    block->u.allocated.tagByte = BLOCK_FREE;

    block->u.free.next = (BlockPtr)zone->hFstFree;
    zone->hFstFree = (Ptr)block;

    update_free_space_accounting(zone, alignedSize);
    coalesce_adjacent_free_blocks(zone, block);

    return noErr;
}
#endif /* make_block_free_32bit */

#if 0  /* UNUSED: make_contiguous_block_free_24bit - preserved for possible future use */
/*
 * Make Contiguous Block Free (24-bit addressing mode)
 * PROVENANCE: ROM $40B500 - Create contiguous free block (24-bit)
 *
 * Algorithm from implementation analysis:
 * - Creates a large contiguous free block from multiple smaller blocks
 * - Used during heap compaction to create large free regions
 * - Handles multiple block coalescing in one operation
 */
static OSErr make_contiguous_block_free_24bit(ZonePtr zone, Ptr startPtr, Size totalSize) {
    if (!zone || !startPtr || totalSize < MIN_FREE_24BIT) {
        return memFullErr;
    }

    /* Create single large free block from contiguous memory region */
    BlockPtr newFreeBlock = (BlockPtr)startPtr;

    /* Set up the large free block header */
    Size alignedSize = (totalSize + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);
    newFreeBlock->blkSize = alignedSize;
    newFreeBlock->u.allocated.tagByte = BLOCK_FREE;
    newFreeBlock->u.free.next = (BlockPtr)zone->hFstFree;

    /* Update zone's first free pointer */
    zone->hFstFree = startPtr;

    /* Update free space accounting */
    update_free_space_accounting(zone, alignedSize);

    return noErr;
}
#endif /* make_contiguous_block_free_24bit */

#if 0  /* UNUSED: make_contiguous_block_free_32bit - preserved for possible future use */
/*
 * Make Contiguous Block Free (32-bit addressing mode)
 * PROVENANCE: ROM $40B520 - Create contiguous free block (32-bit)
 */
static OSErr make_contiguous_block_free_32bit(ZonePtr zone, Ptr startPtr, Size totalSize) {
    if (!zone || !startPtr || totalSize < MIN_FREE_32BIT) {
        return memFullErr;
    }

    BlockPtr newFreeBlock = (BlockPtr)startPtr;
    Size alignedSize = (totalSize + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);

    newFreeBlock->blkSize = alignedSize;
    newFreeBlock->u.allocated.tagByte = BLOCK_FREE;
    newFreeBlock->u.free.next = (BlockPtr)zone->hFstFree;
    zone->hFstFree = startPtr;

    update_free_space_accounting(zone, alignedSize);

    return noErr;
}
#endif /* make_contiguous_block_free_32bit */

/*
 * Advanced Free Block Coalescing
 * PROVENANCE: ROM $40B800 - Free block coalescing algorithm
 *
 * Algorithm from implementation analysis:
 * - Merge adjacent free blocks to reduce fragmentation
 * - Check both forward and backward for adjacent free blocks
 * - Update block sizes and free chain linkage
 * - Critical for maintaining heap efficiency
 */
static void coalesce_adjacent_free_blocks(ZonePtr zone, BlockPtr block) {
    if (!zone || !block || (SignedByte)block->u.allocated.tagByte != (SignedByte)BLOCK_FREE) {
        return;
    }

    Size currentSize = block->blkSize & BLOCK_SIZE_MASK;

    /* Coalesce with next block if it's free */
    BlockPtr nextBlock = (BlockPtr)((char*)block + currentSize);
    if ((char*)nextBlock < (char*)zone->bkLim &&
        (SignedByte)nextBlock->u.allocated.tagByte == (SignedByte)BLOCK_FREE) {

        Size nextSize = nextBlock->blkSize & BLOCK_SIZE_MASK;

        /* Remove next block from free chain */
        if (zone->hFstFree == (Ptr)nextBlock) {
            zone->hFstFree = (Ptr)nextBlock->u.free.next;
        } else {
            /* Find and update previous link in chain */
            BlockPtr prev = (BlockPtr)zone->hFstFree;
            while (prev && (Ptr)prev->u.free.next != (Ptr)nextBlock) {
                prev = (BlockPtr)prev->u.free.next;
            }
            if (prev) {
                prev->u.free.next = nextBlock->u.free.next;
            }
        }

        /* Merge blocks */
        block->blkSize = currentSize + nextSize;
        currentSize = block->blkSize;
    }

    /* Coalesce with previous block if it's free */
    /* Scan backwards to find previous block */
    BlockPtr scanBlock = (BlockPtr)((char*)zone + sizeof(Zone));
    BlockPtr prevBlock = NULL;

    while (scanBlock < block) {
        Size scanSize = scanBlock->blkSize & BLOCK_SIZE_MASK;
        BlockPtr nextScanBlock = (BlockPtr)((char*)scanBlock + scanSize);

        if (nextScanBlock == block) {
            prevBlock = scanBlock;
            break;
        }

        scanBlock = nextScanBlock;
    }

    if (prevBlock && (SignedByte)prevBlock->u.allocated.tagByte == (SignedByte)BLOCK_FREE) {
        Size prevSize = prevBlock->blkSize & BLOCK_SIZE_MASK;

        /* Remove current block from free chain */
        if (zone->hFstFree == (Ptr)block) {
            zone->hFstFree = (Ptr)block->u.free.next;
        } else {
            BlockPtr prev = (BlockPtr)zone->hFstFree;
            while (prev && (Ptr)prev->u.free.next != (Ptr)block) {
                prev = (BlockPtr)prev->u.free.next;
            }
            if (prev) {
                prev->u.free.next = block->u.free.next;
            }
        }

        /* Merge with previous block */
        prevBlock->blkSize = prevSize + currentSize;
    }
}

/*
 * Purge Memory Block (24-bit addressing mode)
 * PROVENANCE: ROM $40BC00 - Purge block (24-bit mode)
 *
 * Algorithm from implementation:
 * - Purge purgeable relocatable blocks to free memory
 * - Call purge procedure if one is installed
 * - Mark handle as purged
 * - Convert block to free block
 */
__attribute__((unused))
static OSErr purge_memory_block_24bit(ZonePtr zone, Handle h) {
    if (!zone || !h || !*h) {
        return nilHandleErr;
    }

    /* Get block information */
    Ptr blockPtr = *h;
    BlockPtr block = (BlockPtr)((char*)blockPtr - BLOCK_OVERHEAD);

    /* Verify this is a purgeable relocatable block */
    if (block->u.allocated.tagByte <= BLOCK_ALLOCATED) {
        return memWZErr;  /* Not a relocatable block */
    }

    Size blockSize = block->blkSize & BLOCK_SIZE_MASK;
    if (!(block->blkSize & PURGEABLE_FLAG)) {
        return memWZErr;  /* Not purgeable */
    }

    /* Mark handle as purged */
    *h = (Ptr)HANDLE_PURGED;

    /* Convert block to free block */
    return make_block_free_24bit(zone, block, blockSize);
}

#if 0  /* UNUSED: purge_memory_block_32bit - preserved for possible future use */
/*
 * Purge Memory Block (32-bit addressing mode)
 * PROVENANCE: ROM $40BC20 - Purge block (32-bit mode)
 */
static OSErr purge_memory_block_32bit(ZonePtr zone, Handle h) {
    if (!zone || !h || !*h) {
        return nilHandleErr;
    }

    Ptr blockPtr = *h;
    BlockPtr block = (BlockPtr)((char*)blockPtr - BLOCK_OVERHEAD);

    if (block->u.allocated.tagByte <= BLOCK_ALLOCATED) {
        return memWZErr;
    }

    Size blockSize = block->blkSize & BLOCK_SIZE_MASK;
    if (!(block->blkSize & PURGEABLE_FLAG)) {
        return memWZErr;
    }

    *h = (Ptr)HANDLE_PURGED;
    return make_block_free_32bit(zone, block, blockSize);
}
#endif /* purge_memory_block_32bit */

#if 0  /* UNUSED: call_purge_procedure - preserved for possible future use */
/*
 * Call Purge Procedure
 * PROVENANCE: ROM $40BB00 - Invoke purge procedure callback
 *
 * Algorithm from implementation:
 * - Invoke user-defined purge procedure for memory reclamation
 * - Handle procedure calling conventions
 * - Manage error conditions and recovery
 */
static OSErr call_purge_procedure(Handle h, PurgeProc purgeProc) {
    if (!purgeProc) {
        return noErr;  /* No purge procedure installed */
    }

    /* Call the purge procedure with proper error handling */
    purgeProc(h);

    /* Return noErr since void procedures don't return error codes */
    return noErr;
}
#endif /* call_purge_procedure */

/*
 * Call Grow Zone Procedure
 * PROVENANCE: ROM $40BD00 - Invoke grow zone callback
 *
 * Algorithm from implementation:
 * - Invoke grow zone procedure for heap expansion
 * - Attempt to grow zone when compaction is insufficient
 * - Handle zone growth and boundary updates
 */
__attribute__((unused))
static OSErr call_grow_zone_procedure(ZonePtr zone, Size bytesNeeded) {
    if (!zone || !zone->gzProc) {
        return memFullErr;  /* No grow zone procedure */
    }

    /* Cast zone's grow zone procedure pointer */
    GrowZoneProc growProc = (GrowZoneProc)zone->gzProc;

    /* Call grow zone procedure */
    growProc(bytesNeeded);

    /* Assume success since void procedures don't return error codes */
    /* Note: Actual zone growth would update bkLim and other fields */

    return noErr;
}

/*
 * Update Free Space Accounting
 * PROVENANCE: ROM $40BE00 - Adjust free space counters
 */
static void update_free_space_accounting(ZonePtr zone, Size deltaBytes) {
    if (!zone) {
        return;
    }

    /* Update zone's free byte count */
    zone->zcbFree += deltaBytes;

    /* Ensure free count doesn't go negative */
    if (zone->zcbFree < 0) {
        zone->zcbFree = 0;
    }
}

/*
 * Public Interface Functions
 */

/*
 * Compact Memory with Purging
 * High-level compaction that includes purging of purgeable blocks
 */
#if 0 /* Future use - advanced compaction with purging not currently used */
static OSErr compact_memory_with_purging(ZonePtr zone, Size bytesNeeded) {
    if (!zone) {
        return memWZErr;
    }

    /* First try normal compaction */
    Size maxFree;
    Ptr freeSpace = compact_heap(zone, bytesNeeded, &maxFree);

    if (freeSpace) {
        return noErr;  /* Compaction succeeded */
    }

    /* Compaction alone wasn't enough - try purging purgeable blocks */
    BlockPtr current = (BlockPtr)((char*)zone + sizeof(Zone));
    BlockPtr end = (BlockPtr)zone->bkLim;

    while (current < end && maxFree < bytesNeeded) {
        if (current->u.allocated.tagByte > BLOCK_ALLOCATED &&
            (current->blkSize & PURGEABLE_FLAG)) {

            /* Found a purgeable block - create a temporary handle for it */
            Handle tempHandle = (Handle)((char*)current + sizeof(Size));

            /* Purge the block */
            OSErr err = purge_memory_block_24bit(zone, tempHandle);
            if (err == noErr) {
                /* Try compaction again */
                freeSpace = compact_heap(zone, bytesNeeded, &maxFree);
                if (freeSpace) {
                    return noErr;
                }
            }
        }

        Size blockSize = current->blkSize & BLOCK_SIZE_MASK;
        current = (BlockPtr)((char*)current + blockSize);
    }

    /* If still not enough space, try grow zone procedure */
    return call_grow_zone_procedure(zone, bytesNeeded);
}

/*
 * Purge Memory Zone
 * Purge all purgeable blocks in a zone
 */
static OSErr purge_memory_zone(ZonePtr zone) {
    if (!zone) {
        return memWZErr;
    }

    BlockPtr current = (BlockPtr)((char*)zone + sizeof(Zone));
    BlockPtr end = (BlockPtr)zone->bkLim;

    while (current < end) {
        if (current->u.allocated.tagByte > BLOCK_ALLOCATED &&
            (current->blkSize & PURGEABLE_FLAG)) {

            Handle tempHandle = (Handle)((char*)current + sizeof(Size));
            purge_memory_block_24bit(zone, tempHandle);
        }

        Size blockSize = current->blkSize & BLOCK_SIZE_MASK;
        if (blockSize < MIN_FREE_24BIT) {
            break;
        }
        current = (BlockPtr)((char*)current + blockSize);
    }

    return noErr;
}
#endif
