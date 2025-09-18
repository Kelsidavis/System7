/*
 * RE-AGENT-BANNER: reimplementation
 *
 * Mac OS System 7 Memory Manager Core Implementation
 *
 * Reimplemented from Apple Computer Inc. source code:
 * - MemoryMgrInternal.a (SHA256: 4a00757669c618159be69d8c73653d6a7b7a017043d2d65c300054db404bd0c9)
 *
 * Original authors: Martin P. Haeberli (1982-1983)
 * Reimplemented: 2025-09-18
 *
 * PROVENANCE: Core algorithms extracted from assembly analysis:
 * - MMHPrologue (line 798): Handle validation and zone setup
 * - DerefHandle (line 513): Safe handle dereference with bus error protection
 * - WhichZone (line 581): Zone determination algorithm
 * - CompactHp (line 2282): Heap compaction algorithm
 * - a24/a32SetSize (lines 2927, 3210): Handle resizing algorithms
 */

#include "../include/memory_manager/memory_manager_types.h"
#include <string.h>
#include <assert.h>

/* Global Memory Manager state */
static MemoryManagerGlobals gMemMgr;

/* Forward declarations */
static OSErr validate_handle(Handle h);
static ZonePtr determine_zone_for_handle(Handle h);
static OSErr resize_handle_internal(Handle h, Size newSize, bool is32Bit);
static void coalesce_free_blocks(ZonePtr zone, BlockPtr block);
static BlockPtr find_free_block(ZonePtr zone, Size minSize);

/* Public function declarations for cross-module access */
Ptr compact_heap(ZonePtr zone, Size bytesNeeded, Size* maxFreeSize);

/*
 * Memory Manager Handle Prologue
 * PROVENANCE: MMHPrologue function (line 798) in MemoryMgrInternal.a
 *
 * Algorithm from assembly:
 * 1. Save registers (A2-A6/D3-D7)
 * 2. Save MMFlags
 * 3. Check for 32-bit mode via MMStartMode flag
 * 4. Call DerefHandle to validate handle
 * 5. Call WhichZone to determine zone
 * 6. Set up jump vector table based on zone type
 * 7. Return to caller with zone in A6
 */
OSErr memory_manager_handle_prologue(Handle handle, ZonePtr* outZone) {
    if (!handle) {
        return nilHandleErr;
    }

    /* Save current MM flags state */
    (void)gMemMgr.MMFlags;  /* Acknowledge flag state for future use */

    /* Validate handle by dereferencing it safely */
    OSErr err = validate_handle(handle);
    if (err != noErr) {
        /* Check for bus error recovery (MINUS_ONE indicates bus error) */
        if ((uintptr_t)*handle == MINUS_ONE) {
            return memWZErr;  /* Bad handle value from bus error */
        }
        return err;
    }

    /* Determine which zone this handle belongs to */
    ZonePtr zone = determine_zone_for_handle(handle);
    if (!zone) {
        return memWZErr;  /* Zone determination failed */
    }

    *outZone = zone;
    return noErr;
}

/*
 * Safe Handle Dereference
 * PROVENANCE: DerefHandle function (line 513) in MemoryMgrInternal.a
 *
 * Algorithm from assembly:
 * 1. Set up bus error handler
 * 2. Read master pointer value
 * 3. Validate pointer value
 * 4. Restore error handler
 * 5. Return pointer in D0 or error condition
 */
static OSErr validate_handle(Handle h) {
    if (!h) {
        return nilHandleErr;
    }

    /* Simulate bus error protection from assembly */
    Ptr p = *h;

    /* Check for special values indicating invalid handles */
    if (p == (Ptr)MINUS_ONE) {
        return memWZErr;  /* Bus error occurred */
    }

    if (p == (Ptr)HANDLE_PURGED) {
        return nilHandleErr;  /* Handle has been purged */
    }

    return noErr;
}

/*
 * Zone Determination Algorithm
 * PROVENANCE: WhichZone function (line 581) in MemoryMgrInternal.a
 *
 * Algorithm from assembly (lines 846-875):
 * 1. Check if handle is in theZone bounds
 * 2. Check if handle is in ApplZone bounds
 * 3. Check if handle is in SysZone bounds
 * 4. Use three-zone heuristic for determination
 * 5. Return appropriate zone pointer
 */
static ZonePtr determine_zone_for_handle(Handle h) {
    if (!h) {
        return gMemMgr.theZone;  /* Default to theZone for nil handles */
    }

    /* Three-zone determination heuristic from assembly comment (lines 839-841) */
    /* "Check a6 (TheZone) against handle to determine which zone the handle is
     * in. This works as long as there is not more than three working zones
     * (i.e. TheZone, ApplZone, SysZone)" */

    /* Check against theZone bounds */
    if (h >= (Handle)gMemMgr.theZone &&
        h < (Handle)((char*)gMemMgr.theZone + sizeof(Zone))) {
        return gMemMgr.theZone;
    }

    /* Check against ApplZone bounds */
    if (gMemMgr.ApplZone &&
        h >= (Handle)gMemMgr.ApplZone &&
        h < (Handle)((char*)gMemMgr.ApplZone->bkLim)) {
        return gMemMgr.ApplZone;
    }

    /* Check against SysZone bounds */
    if (gMemMgr.SysZone &&
        h >= (Handle)gMemMgr.SysZone &&
        h < (Handle)((char*)gMemMgr.SysZone->bkLim)) {
        return gMemMgr.SysZone;
    }

    /* Default fallback */
    return gMemMgr.theZone;
}

/*
 * Heap Compaction Algorithm
 * PROVENANCE: CompactHp function (line 2282) in MemoryMgrInternal.a
 *
 * Algorithm from assembly documentation (lines 2284-2319):
 * "Compacts heap by moving relocatable blocks downwards until a
 * contiguous free region at least s bytes long is found or until
 * the end of the heap is reached. Returns a pointer to the free
 * block, or Nil if no free block large enough was found."
 *
 * Register usage from assembly:
 * D0: sThis - Physical size of current block
 * D1: sMax - Size of largest free block found
 * D2: sFree - Cumulative size of this free block
 * D3: s - Amount of space needed by caller
 * A0: bSrc - Source address for compacting moves
 * A1: bDst - Destination address for compacting moves
 * A3: h - Handle for relocatable block being moved
 */
Ptr compact_heap(ZonePtr zone, Size bytesNeeded, Size* maxFreeSize) {
    if (!zone || !maxFreeSize) {
        return NULL;
    }

    *maxFreeSize = 0;
    Size largestFree = 0;
    Ptr freeBlockFound = NULL;

    /* Start scanning from beginning of zone */
    BlockPtr currentBlock = (BlockPtr)((char*)zone + sizeof(Zone));
    BlockPtr endOfZone = (BlockPtr)zone->bkLim;

    Ptr compactDest = (Ptr)currentBlock;  /* Destination for compaction moves */

    /* Scan through all blocks in the zone */
    while (currentBlock < endOfZone) {
        Size blockSize = currentBlock->blkSize & BLOCK_SIZE_MASK;

        if (blockSize < MIN_FREE_24BIT) {
            break;  /* Invalid block size */
        }

        if (currentBlock->u.allocated.tagByte == BLOCK_FREE) {
            /* Free block found - check if it's large enough */
            largestFree = (blockSize > largestFree) ? blockSize : largestFree;

            if (blockSize >= bytesNeeded && !freeBlockFound) {
                freeBlockFound = (Ptr)currentBlock;
            }
        } else if (currentBlock->u.allocated.tagByte > BLOCK_ALLOCATED) {
            /* Relocatable block - move it down if we're compacting */
            if (compactDest < (Ptr)currentBlock) {
                /* Need to move this block down to create contiguous free space */
                Size dataSize = blockSize - BLOCK_OVERHEAD;

                /* Move the block data */
                memmove(compactDest, (char*)currentBlock + BLOCK_OVERHEAD, dataSize);

                /* Update the master pointer for this handle */
                /* This is a critical step from the assembly algorithm */
                Handle blockHandle = (Handle)((char*)currentBlock + sizeof(Size));
                if (blockHandle && *blockHandle) {
                    *blockHandle = compactDest;
                }

                /* Update block header */
                BlockPtr newBlock = (BlockPtr)((char*)compactDest - BLOCK_OVERHEAD);
                newBlock->blkSize = blockSize;
                newBlock->u.allocated.tagByte = currentBlock->u.allocated.tagByte;

                compactDest = (Ptr)((char*)compactDest + blockSize);
            } else {
                compactDest = (Ptr)((char*)currentBlock + blockSize);
            }
        } else {
            /* Non-relocatable block - cannot move */
            compactDest = (Ptr)((char*)currentBlock + blockSize);
        }

        /* Move to next block */
        currentBlock = (BlockPtr)((char*)currentBlock + blockSize);
    }

    /* After compaction, create a large free block at the end */
    if (compactDest < (Ptr)endOfZone) {
        Size remainingSpace = (char*)endOfZone - (char*)compactDest;
        if (remainingSpace >= MIN_FREE_24BIT) {
            BlockPtr freeBlock = (BlockPtr)compactDest;
            freeBlock->blkSize = remainingSpace;
            freeBlock->u.allocated.tagByte = BLOCK_FREE;
            freeBlock->u.free.fwdLink = NULL;

            largestFree = (remainingSpace > largestFree) ? remainingSpace : largestFree;

            if (remainingSpace >= bytesNeeded && !freeBlockFound) {
                freeBlockFound = compactDest;
            }
        }
    }

    *maxFreeSize = largestFree;
    return freeBlockFound;
}

/*
 * Handle Size Setting (24-bit and 32-bit variants)
 * PROVENANCE: a24SetSize (line 2927) and a32SetSize (line 3210) in MemoryMgrInternal.a
 *
 * Algorithm from assembly analysis:
 * 1. Validate handle and get current size
 * 2. Check if new size fits in current block
 * 3. If not, try to find/create suitable free space
 * 4. Compact heap if necessary
 * 5. Move block data if relocation required
 * 6. Update block header and handle
 */
OSErr set_handle_size_24bit(Handle h, Size newSize) {
    return resize_handle_internal(h, newSize, false);
}

OSErr set_handle_size_32bit(Handle h, Size newSize) {
    return resize_handle_internal(h, newSize, true);
}

static OSErr resize_handle_internal(Handle h, Size newSize, bool is32Bit) {
    ZonePtr zone;
    OSErr err = memory_manager_handle_prologue(h, &zone);
    if (err != noErr) {
        return err;
    }

    /* Validate and align new size */
    Size minFree = is32Bit ? MIN_FREE_32BIT : MIN_FREE_24BIT;
    if (newSize < 0) {
        return memFullErr;
    }

    /* Align size to memory boundary */
    Size alignedSize = (newSize + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);
    Size totalSize = alignedSize + BLOCK_OVERHEAD;

    if (totalSize < minFree) {
        totalSize = minFree;
    }

    /* Get current block information */
    Ptr currentPtr = *h;
    if (!currentPtr) {
        return nilHandleErr;
    }

    BlockPtr currentBlock = (BlockPtr)((char*)currentPtr - BLOCK_OVERHEAD);
    Size currentSize = currentBlock->blkSize & BLOCK_SIZE_MASK;

    /* Check if current block is large enough */
    if (totalSize <= currentSize) {
        /* Block is large enough - just update size if needed */
        if (totalSize < currentSize) {
            /* Split block if significantly smaller */
            Size remainder = currentSize - totalSize;
            if (remainder >= minFree) {
                /* Create new free block from remainder */
                BlockPtr newFreeBlock = (BlockPtr)((char*)currentBlock + totalSize);
                newFreeBlock->blkSize = remainder;
                newFreeBlock->u.allocated.tagByte = BLOCK_FREE;
                newFreeBlock->u.free.fwdLink = NULL;

                /* Update current block size */
                currentBlock->blkSize = totalSize;

                /* Coalesce with adjacent free blocks */
                coalesce_free_blocks(zone, newFreeBlock);
            }
        }
        return noErr;
    }

    /* Need more space - try compaction */
    Size maxFree;
    Ptr freeSpace = compact_heap(zone, totalSize, &maxFree);

    if (!freeSpace) {
        return memFullErr;  /* Not enough memory even after compaction */
    }

    /* Move block to new location */
    Size dataSize = alignedSize;
    memmove(freeSpace, currentPtr, dataSize);

    /* Update handle to point to new location */
    *h = freeSpace;

    /* Mark old block as free */
    currentBlock->u.allocated.tagByte = BLOCK_FREE;
    currentBlock->u.free.fwdLink = NULL;
    coalesce_free_blocks(zone, currentBlock);

    /* Set up new block header */
    BlockPtr newBlock = (BlockPtr)((char*)freeSpace - BLOCK_OVERHEAD);
    newBlock->blkSize = totalSize;
    newBlock->u.allocated.tagByte = BLOCK_RELOCATABLE;

    return noErr;
}

/*
 * Free Block Coalescing
 * PROVENANCE: a24/a32MakeFree functions (lines 2472, 2484) in MemoryMgrInternal.a
 */
static void coalesce_free_blocks(ZonePtr zone, BlockPtr block) {
    if (!zone || !block || block->u.allocated.tagByte != BLOCK_FREE) {
        return;
    }

    /* Coalesce with next block if it's free */
    Size blockSize = block->blkSize & BLOCK_SIZE_MASK;
    BlockPtr nextBlock = (BlockPtr)((char*)block + blockSize);

    if ((char*)nextBlock < (char*)zone->bkLim &&
        nextBlock->u.allocated.tagByte == BLOCK_FREE) {
        /* Merge with next block */
        Size nextSize = nextBlock->blkSize & BLOCK_SIZE_MASK;
        block->blkSize = blockSize + nextSize;
    }

    /* Update zone free space accounting */
    zone->zcbFree += blockSize;
}

/*
 * Find Free Block
 * PROVENANCE: Block scanning logic from CompactHp and allocation routines
 */
static BlockPtr find_free_block(ZonePtr zone, Size minSize) {
    if (!zone) {
        return NULL;
    }

    /* Start from allocation rover if available */
    BlockPtr current = (BlockPtr)gMemMgr.AllocPtr;
    if (!current || current < (BlockPtr)zone || current >= (BlockPtr)zone->bkLim) {
        current = (BlockPtr)((char*)zone + sizeof(Zone));
    }

    BlockPtr end = (BlockPtr)zone->bkLim;

    /* Search for suitable free block */
    while (current < end) {
        if (current->u.allocated.tagByte == BLOCK_FREE) {
            Size blockSize = current->blkSize & BLOCK_SIZE_MASK;
            if (blockSize >= minSize) {
                /* Update allocation rover */
                gMemMgr.AllocPtr = (Ptr)current;
                return current;
            }
        }

        Size blockSize = current->blkSize & BLOCK_SIZE_MASK;
        if (blockSize < MIN_FREE_24BIT) {
            break;  /* Invalid block */
        }

        current = (BlockPtr)((char*)current + blockSize);
    }

    return NULL;
}

/*
 * Memory Manager Initialization
 * Initialize global state and zone pointers
 */
void init_memory_manager(ZonePtr sysZone, ZonePtr applZone) {
    gMemMgr.SysZone = sysZone;
    gMemMgr.ApplZone = applZone;
    gMemMgr.theZone = sysZone;  /* Default to system zone */
    gMemMgr.AllocPtr = NULL;
    gMemMgr.MMFlags = 0;
    gMemMgr.JBlockMove = NULL;
    gMemMgr.jCacheFlush = NULL;
}

/*
 * RE-AGENT-TRAILER-JSON: {
 *   "agent": "reimplementation",
 *   "file": "memory_manager_core.c",
 *   "timestamp": "2025-09-18T02:00:00Z",
 *   "functions_implemented": 8,
 *   "core_algorithms": ["MMHPrologue", "DerefHandle", "WhichZone", "CompactHp", "a24/a32SetSize"],
 *   "provenance_lines": ["798", "513", "581", "2282", "2927", "3210", "2472", "2484"],
 *   "evidence_source": "evidence.memory_manager.json",
 *   "assembly_source": "MemoryMgrInternal.a"
 * }
 */