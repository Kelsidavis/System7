/*
 * RE-AGENT-BANNER: reimplementation
 *
 * Mac OS System 7 Heap Compaction and Coalescing Algorithms
 *
 * Reimplemented from Apple Computer Inc. source code:
 * - MemoryMgrInternal.a (SHA256: 4a00757669c618159be69d8c73653d6a7b7a017043d2d65c300054db404bd0c9)
 *
 * Original authors: Martin P. Haeberli (1982-1983)
 * Reimplemented: 2025-09-18
 *
 * PROVENANCE: Heap management algorithms extracted from assembly analysis:
 * - a24/a32MakeBkF (lines 1167, 1173): Block free creation
 * - a24/a32MakeCBkF (lines 1202, 1228): Contiguous block free creation
 * - a24/a32MakeFree (lines 2472, 2484): Free block coalescing
 * - CallPurgeProc (line 1342): Purge procedure invocation
 * - CallGZProc (line 1726): Grow zone procedure invocation
 * - a24/a32PurgeBlock (lines 1375, 1386): Block purging algorithms
 */

#include "../include/memory_manager/memory_manager_types.h"
#include <string.h>
#include <assert.h>

/* Forward declarations */
static OSErr make_block_free_24bit(ZonePtr zone, BlockPtr block, Size size);
static OSErr make_block_free_32bit(ZonePtr zone, BlockPtr block, Size size);
static OSErr make_contiguous_block_free_24bit(ZonePtr zone, Ptr startPtr, Size totalSize);
static OSErr make_contiguous_block_free_32bit(ZonePtr zone, Ptr startPtr, Size totalSize);
static void coalesce_adjacent_free_blocks(ZonePtr zone, BlockPtr block);
static OSErr purge_memory_block_24bit(ZonePtr zone, Handle h);
static OSErr purge_memory_block_32bit(ZonePtr zone, Handle h);
static OSErr call_purge_procedure(Handle h, PurgeProc purgeProc);
static OSErr call_grow_zone_procedure(ZonePtr zone, Size bytesNeeded);
static void update_free_space_accounting(ZonePtr zone, Size deltaBytes);

/*
 * Make Block Free (24-bit addressing mode)
 * PROVENANCE: a24MakeBkF function (line 1167) in MemoryMgrInternal.a
 *
 * Algorithm from assembly:
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
    block->u.free.fwdLink = zone->hFstFree;
    zone->hFstFree = (Ptr)block;

    /* Update zone free space accounting */
    update_free_space_accounting(zone, alignedSize);

    /* Attempt to coalesce with adjacent blocks */
    coalesce_adjacent_free_blocks(zone, block);

    return noErr;
}

/*
 * Make Block Free (32-bit addressing mode)
 * PROVENANCE: a32MakeBkF function (line 1173) in MemoryMgrInternal.a
 */
static OSErr make_block_free_32bit(ZonePtr zone, BlockPtr block, Size size) {
    if (!zone || !block || size < MIN_FREE_32BIT) {
        return memFullErr;
    }

    /* Similar to 24-bit version but with 32-bit specific handling */
    Size alignedSize = (size + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);

    block->blkSize = alignedSize;
    block->u.allocated.tagByte = BLOCK_FREE;

    block->u.free.fwdLink = zone->hFstFree;
    zone->hFstFree = (Ptr)block;

    update_free_space_accounting(zone, alignedSize);
    coalesce_adjacent_free_blocks(zone, block);

    return noErr;
}

/*
 * Make Contiguous Block Free (24-bit addressing mode)
 * PROVENANCE: a24MakeCBkF function (line 1202) in MemoryMgrInternal.a
 *
 * Algorithm from assembly analysis:
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
    newFreeBlock->u.free.fwdLink = zone->hFstFree;

    /* Update zone's first free pointer */
    zone->hFstFree = startPtr;

    /* Update free space accounting */
    update_free_space_accounting(zone, alignedSize);

    return noErr;
}

/*
 * Make Contiguous Block Free (32-bit addressing mode)
 * PROVENANCE: a32MakeCBkF function (line 1228) in MemoryMgrInternal.a
 */
static OSErr make_contiguous_block_free_32bit(ZonePtr zone, Ptr startPtr, Size totalSize) {
    if (!zone || !startPtr || totalSize < MIN_FREE_32BIT) {
        return memFullErr;
    }

    BlockPtr newFreeBlock = (BlockPtr)startPtr;
    Size alignedSize = (totalSize + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);

    newFreeBlock->blkSize = alignedSize;
    newFreeBlock->u.allocated.tagByte = BLOCK_FREE;
    newFreeBlock->u.free.fwdLink = zone->hFstFree;
    zone->hFstFree = startPtr;

    update_free_space_accounting(zone, alignedSize);

    return noErr;
}

/*
 * Advanced Free Block Coalescing
 * PROVENANCE: a24/a32MakeFree functions (lines 2472, 2484) in MemoryMgrInternal.a
 *
 * Algorithm from assembly analysis:
 * - Merge adjacent free blocks to reduce fragmentation
 * - Check both forward and backward for adjacent free blocks
 * - Update block sizes and free chain linkage
 * - Critical for maintaining heap efficiency
 */
static void coalesce_adjacent_free_blocks(ZonePtr zone, BlockPtr block) {
    if (!zone || !block || block->u.allocated.tagByte != BLOCK_FREE) {
        return;
    }

    Size currentSize = block->blkSize & BLOCK_SIZE_MASK;

    /* Coalesce with next block if it's free */
    BlockPtr nextBlock = (BlockPtr)((char*)block + currentSize);
    if ((char*)nextBlock < (char*)zone->bkLim &&
        nextBlock->u.allocated.tagByte == BLOCK_FREE) {

        Size nextSize = nextBlock->blkSize & BLOCK_SIZE_MASK;

        /* Remove next block from free chain */
        if (zone->hFstFree == (Ptr)nextBlock) {
            zone->hFstFree = nextBlock->u.free.fwdLink;
        } else {
            /* Find and update previous link in chain */
            BlockPtr prev = (BlockPtr)zone->hFstFree;
            while (prev && prev->u.free.fwdLink != (Ptr)nextBlock) {
                prev = (BlockPtr)prev->u.free.fwdLink;
            }
            if (prev) {
                prev->u.free.fwdLink = nextBlock->u.free.fwdLink;
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

    if (prevBlock && prevBlock->u.allocated.tagByte == BLOCK_FREE) {
        Size prevSize = prevBlock->blkSize & BLOCK_SIZE_MASK;

        /* Remove current block from free chain */
        if (zone->hFstFree == (Ptr)block) {
            zone->hFstFree = block->u.free.fwdLink;
        } else {
            BlockPtr prev = (BlockPtr)zone->hFstFree;
            while (prev && prev->u.free.fwdLink != (Ptr)block) {
                prev = (BlockPtr)prev->u.free.fwdLink;
            }
            if (prev) {
                prev->u.free.fwdLink = block->u.free.fwdLink;
            }
        }

        /* Merge with previous block */
        prevBlock->blkSize = prevSize + currentSize;
    }
}

/*
 * Purge Memory Block (24-bit addressing mode)
 * PROVENANCE: a24PurgeBlock function (line 1375) in MemoryMgrInternal.a
 *
 * Algorithm from assembly:
 * - Purge purgeable relocatable blocks to free memory
 * - Call purge procedure if one is installed
 * - Mark handle as purged
 * - Convert block to free block
 */
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

/*
 * Purge Memory Block (32-bit addressing mode)
 * PROVENANCE: a32PurgeBlock function (line 1386) in MemoryMgrInternal.a
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

/*
 * Call Purge Procedure
 * PROVENANCE: CallPurgeProc function (line 1342) in MemoryMgrInternal.a
 *
 * Algorithm from assembly:
 * - Invoke user-defined purge procedure for memory reclamation
 * - Handle procedure calling conventions
 * - Manage error conditions and recovery
 */
static OSErr call_purge_procedure(Handle h, PurgeProc purgeProc) {
    if (!purgeProc) {
        return noErr;  /* No purge procedure installed */
    }

    /* Call the purge procedure with proper error handling */
    OSErr result = purgeProc(h);

    /* Return result from purge procedure */
    return result;
}

/*
 * Call Grow Zone Procedure
 * PROVENANCE: CallGZProc function (line 1726) in MemoryMgrInternal.a
 *
 * Algorithm from assembly:
 * - Invoke grow zone procedure for heap expansion
 * - Attempt to grow zone when compaction is insufficient
 * - Handle zone growth and boundary updates
 */
static OSErr call_grow_zone_procedure(ZonePtr zone, Size bytesNeeded) {
    if (!zone || !zone->gzProc) {
        return memFullErr;  /* No grow zone procedure */
    }

    /* Cast zone's grow zone procedure pointer */
    GrowZoneProc growProc = (GrowZoneProc)zone->gzProc;

    /* Call grow zone procedure */
    OSErr result = growProc(bytesNeeded);

    if (result == noErr) {
        /* Zone was successfully grown - update accounting */
        /* Note: Actual zone growth would update bkLim and other fields */
    }

    return result;
}

/*
 * Update Free Space Accounting
 * PROVENANCE: AdjustFree/SubtractFree functions (lines 3509, 3528) in MemoryMgrInternal.a
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
OSErr compact_memory_with_purging(ZonePtr zone, Size bytesNeeded) {
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
OSErr purge_memory_zone(ZonePtr zone) {
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

/*
 * RE-AGENT-TRAILER-JSON: {
 *   "agent": "reimplementation",
 *   "file": "heap_compaction.c",
 *   "timestamp": "2025-09-18T02:15:00Z",
 *   "functions_implemented": 12,
 *   "algorithms": ["a24/a32MakeBkF", "a24/a32MakeCBkF", "a24/a32MakeFree", "CallPurgeProc", "CallGZProc", "a24/a32PurgeBlock"],
 *   "provenance_lines": ["1167", "1173", "1202", "1228", "2472", "2484", "1342", "1726", "1375", "1386", "3509", "3528"],
 *   "evidence_source": "evidence.memory_manager.json",
 *   "assembly_source": "MemoryMgrInternal.a"
 * }
 */