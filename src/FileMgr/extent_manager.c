// #include "CompatibilityFix.h" // Removed
#include <stdlib.h>
#include <string.h>
/*
 * RE-AGENT-BANNER
 * File eXtent Management (FXM) Implementation
 * Reverse-engineered from Mac OS System 7.1 ROM extent manager code
 * Original evidence: ROM extent manager code extent allocation and mapping functions
 * Implements HFS file allocation block management and extent operations
 * Provenance: evidence.file_manager.json -> ROM extent manager code functions
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "FileMgr/file_manager.h"
#include "FileMgr/hfs_structs.h"


/*
 * FXMFindExtent - Find extent record for file allocation block

 * Returns extent record containing the specified file block
 */
OSErr FXMFindExtent(FCB* fcb, UInt32 file_block, ExtentRecord* extent_rec) {
    UInt32 current_block = 0;
    int extent_index;

    if (fcb == NULL || extent_rec == NULL) return paramErr;

    /* Check first extent record in FCB - Evidence: FCB extent checking pattern */
    for (extent_index = 0; extent_index < NUM_EXTENTS_PER_RECORD; extent_index++) {
        ExtentDescriptor* extent = &(fcb)->extent[extent_index];

        if (extent->blockCount == 0) break;  /* End of extents */

        if (file_block >= current_block && file_block < current_block + extent->blockCount) {
            /* Found extent containing the block */
            memcpy(extent_rec, &fcb->fcbExtRec, sizeof(ExtentRecord));
            return noErr;
        }

        current_block += extent->blockCount;
    }

    /* Block not found in FCB extents - would search extent overflow B-Tree */
    return fnfErr;
}

/*
 * FXMAllocateExtent - Allocate new extent for file

 */
OSErr FXMAllocateExtent(VCB* vcb, FCB* fcb, UInt32 bytes_needed, ExtentRecord* extent_rec) {
    UInt32 blocks_needed;
    UInt32 start_block = 0;
    UInt32 blocks_found = 0;

    if (vcb == NULL || fcb == NULL || extent_rec == NULL) return paramErr;

    /* Calculate blocks needed - Evidence: byte to block conversion */
    blocks_needed = CalculateAllocationBlocks(bytes_needed, vcb->vcbAlBlkSiz);

    /* Check if volume has enough free blocks */
    if (blocks_needed > vcb->vcbFreeBks) {
        return memFullErr;
    }

    /* Simulate volume bitmap scan - Evidence: ROM extent manager code bitmap scanning */
    /* In real implementation, would scan volume allocation bitmap */
    start_block = vcb->vcbAllocPtr;  /* Start search at allocation pointer */

    /* Simple allocation - find contiguous free blocks (overflow-safe comparisons) */
    if (blocks_needed <= vcb->vcbNmAlBlks &&
        start_block <= vcb->vcbNmAlBlks - blocks_needed) {
        blocks_found = blocks_needed;
    } else {
        /* Not enough contiguous space at allocation pointer */
        start_block = vcb->vcbAlBlSt;  /* Start from beginning */
        if (blocks_needed <= vcb->vcbNmAlBlks &&
            start_block <= vcb->vcbNmAlBlks - blocks_needed) {
            blocks_found = blocks_needed;
        }
    }

    if (blocks_found < blocks_needed) {
        return memFullErr;  /* Not enough contiguous space */
    }

    /* Validate values fit in UInt16 for extent descriptor */
    if (start_block > 0xFFFF || blocks_found > 0xFFFF) {
        return paramErr;
    }

    /* Create extent record - Evidence: extent record creation pattern */
    memset(extent_rec, 0, sizeof(ExtentRecord));
    extent_rec->extent[0].startBlock = (UInt16)start_block;
    extent_rec->extent[0].blockCount = (UInt16)blocks_found;

    /* Update volume free block count - Evidence: free block maintenance */
    vcb->vcbFreeBks -= (UInt16)blocks_found;

    /* Update allocation pointer - Evidence: allocation pointer advancement */
    UInt32 newAllocPtr = start_block + blocks_found;
    vcb->vcbAllocPtr = (newAllocPtr < vcb->vcbNmAlBlks) ? (UInt16)newAllocPtr : vcb->vcbAlBlSt;

    return noErr;
}

/*
 * FXMDeallocateExtent - Deallocate file extent

 */
OSErr FXMDeallocateExtent(VCB* vcb, ExtentRecord* extent_rec) {
    int extent_index;

    if (vcb == NULL || extent_rec == NULL) return paramErr;

    /* Deallocate each extent in the record - Evidence: extent deallocation loop */
    for (extent_index = 0; extent_index < NUM_EXTENTS_PER_RECORD; extent_index++) {
        ExtentDescriptor* extent = &extent_rec->extent[extent_index];

        if (extent->blockCount == 0) break;  /* End of extents */

        /* Mark blocks as free in volume bitmap */
        /* In real implementation, would clear bits in volume allocation bitmap */

        /* Update volume free block count */
        vcb->vcbFreeBks += extent->blockCount;

        /* Clear extent */
        extent->startBlock = 0;
        extent->blockCount = 0;
    }

    return noErr;
}

/*
 * FXMExtendFile - Extend file by allocating more blocks

 */
OSErr FXMExtendFile(FCB* fcb, UInt32 bytes_to_add) {
    ExtentRecord new_extent;
    OSErr result;
    int extent_index;

    if (fcb == NULL || fcb->fcbVPtr == NULL) return paramErr;

    /* Try to allocate new extent - Evidence: extent allocation for growth */
    result = FXMAllocateExtent(fcb->fcbVPtr, fcb, bytes_to_add, &new_extent);
    if (result != noErr) {
        return result;
    }

    /* Try to add extent to FCB extent record first */
    for (extent_index = 0; extent_index < NUM_EXTENTS_PER_RECORD; extent_index++) {
        ExtentDescriptor* extent = &(fcb)->extent[extent_index];

        if (extent->blockCount == 0) {
            /* Found empty slot - Evidence: FCB extent slot filling */
            *extent = new_extent.extent[0];

            /* Update file physical length */
            fcb->fcbPLen += bytes_to_add;

            return noErr;
        }
    }

    /* No room in FCB - would need to add to extent overflow B-Tree */
    /* For now, just update physical length */
    fcb->fcbPLen += bytes_to_add;

    return noErr;
}

/*
 * FXMTruncateFile - Truncate file to specified length

 */
OSErr FXMTruncateFile(FCB* fcb, UInt32 new_length) {
    UInt32 blocks_needed;
    UInt32 current_block = 0;
    int extent_index;

    if (fcb == NULL || fcb->fcbVPtr == NULL) return paramErr;

    /* Calculate blocks needed for new length */
    blocks_needed = CalculateAllocationBlocks(new_length, fcb->fcbVPtr->vcbAlBlkSiz);

    /* Traverse extents and deallocate excess blocks */
    for (extent_index = 0; extent_index < NUM_EXTENTS_PER_RECORD; extent_index++) {
        ExtentDescriptor* extent = &(fcb)->extent[extent_index];

        if (extent->blockCount == 0) break;  /* End of extents */

        if (current_block >= blocks_needed) {
            /* This entire extent is beyond new length - deallocate it */
            fcb->fcbVPtr->vcbFreeBks += extent->blockCount;
            extent->startBlock = 0;
            extent->blockCount = 0;
        } else if (current_block + extent->blockCount > blocks_needed) {
            /* This extent partially extends beyond new length - truncate it */
            UInt32 excess_blocks = current_block + extent->blockCount - blocks_needed;
            fcb->fcbVPtr->vcbFreeBks += (UInt16)excess_blocks;
            extent->blockCount -= (UInt16)excess_blocks;
            break;
        }

        current_block += extent->blockCount;
    }

    /* Update file lengths - Evidence: FCB length field updates */
    fcb->fcbPLen = new_length;
    if (fcb->fcbEOF > new_length) {
        fcb->fcbEOF = new_length;
    }

    return noErr;
}

/*
 * Utility function implementations
 */

/*
 * CalculateAllocationBlocks - Convert byte size to allocation blocks

 */
UInt32 CalculateAllocationBlocks(UInt32 file_size, UInt32 block_size) {
    if (block_size == 0) return 0;
    return (file_size + block_size - 1) / block_size;  /* Round up */
}

/*
 */
