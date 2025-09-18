/*
 * HFS_Allocation.c - HFS Block Allocation and I/O Implementation
 *
 * This file implements block allocation, extent management, cache operations,
 * and low-level I/O for the HFS file system.
 *
 * Copyright (c) 2024 - Implementation for System 7.1 Portable
 * Based on Apple System Software 7.1 HFS allocation architecture
 */

#include "FileManager.h"
#include "FileManager_Internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* External globals */
extern FSGlobals g_FSGlobals;
extern PlatformHooks g_PlatformHooks;

/* Allocation bitmap constants */
#define BITS_PER_BYTE       8
#define BLOCKS_PER_WORD     16
#define BITMAP_CACHE_SIZE   4096  /* Cache 4KB of bitmap at a time */

/* ============================================================================
 * Allocation Bitmap Management
 * ============================================================================ */

/**
 * Initialize the allocation bitmap
 */
OSErr Alloc_Init(VCB* vcb)
{
    uint32_t bitmapBytes;
    uint32_t bitmapBlocks;
    OSErr err;

    if (!vcb) {
        return paramErr;
    }

    /* Calculate bitmap size */
    bitmapBytes = (vcb->vcbNmAlBlks + 7) / 8;  /* Round up to byte boundary */
    bitmapBlocks = (bitmapBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;

    /* Allocate bitmap cache */
    vcb->vcbVBMCache = calloc(1, bitmapBytes);
    if (!vcb->vcbVBMCache) {
        return memFullErr;
    }

    /* Read bitmap from disk */
    err = IO_ReadBlocks(vcb, vcb->vcbVBMSt, bitmapBlocks, vcb->vcbVBMCache);
    if (err != noErr) {
        free(vcb->vcbVBMCache);
        vcb->vcbVBMCache = NULL;
        return err;
    }

    return noErr;
}

/**
 * Close the allocation bitmap
 */
OSErr Alloc_Close(VCB* vcb)
{
    if (!vcb) {
        return paramErr;
    }

    /* Free bitmap cache */
    if (vcb->vcbVBMCache) {
        free(vcb->vcbVBMCache);
        vcb->vcbVBMCache = NULL;
    }

    return noErr;
}

/**
 * Test if a bit is set in the bitmap
 */
static bool TestBit(uint8_t* bitmap, uint32_t bitNum)
{
    uint32_t byteNum = bitNum / BITS_PER_BYTE;
    uint32_t bitPos = bitNum % BITS_PER_BYTE;
    return (bitmap[byteNum] & (1 << (7 - bitPos))) != 0;
}

/**
 * Set a bit in the bitmap
 */
static void SetBit(uint8_t* bitmap, uint32_t bitNum)
{
    uint32_t byteNum = bitNum / BITS_PER_BYTE;
    uint32_t bitPos = bitNum % BITS_PER_BYTE;
    bitmap[byteNum] |= (1 << (7 - bitPos));
}

/**
 * Clear a bit in the bitmap
 */
static void ClearBit(uint8_t* bitmap, uint32_t bitNum)
{
    uint32_t byteNum = bitNum / BITS_PER_BYTE;
    uint32_t bitPos = bitNum % BITS_PER_BYTE;
    bitmap[byteNum] &= ~(1 << (7 - bitPos));
}

/**
 * Find a run of free blocks in the bitmap
 */
static uint32_t FindFreeRun(uint8_t* bitmap, uint32_t totalBlocks,
                            uint32_t startHint, uint32_t minBlocks)
{
    uint32_t start = startHint;
    uint32_t count = 0;
    uint32_t firstFree = 0;
    bool wrapped = false;

    while (true) {
        /* Check if block is free */
        if (!TestBit(bitmap, start)) {
            if (count == 0) {
                firstFree = start;
            }
            count++;

            /* Found enough blocks? */
            if (count >= minBlocks) {
                return firstFree;
            }
        } else {
            /* Block is allocated, reset count */
            count = 0;
        }

        /* Move to next block */
        start++;
        if (start >= totalBlocks) {
            if (wrapped) {
                break;  /* Searched entire bitmap */
            }
            start = 0;
            wrapped = true;
        }

        /* Stop if we've come back to hint */
        if (wrapped && start >= startHint) {
            break;
        }
    }

    return 0xFFFFFFFF;  /* No run found */
}

/**
 * Allocate blocks from the volume
 */
OSErr Alloc_Blocks(VCB* vcb, uint32_t startHint, uint32_t minBlocks, uint32_t maxBlocks,
                  uint32_t* actualStart, uint32_t* actualCount)
{
    uint8_t* bitmap;
    uint32_t foundStart;
    uint32_t foundCount;
    uint32_t i;

    if (!vcb || !actualStart || !actualCount) {
        return paramErr;
    }

    if (minBlocks == 0 || minBlocks > maxBlocks) {
        return paramErr;
    }

    FS_LockVolume(vcb);

    bitmap = (uint8_t*)vcb->vcbVBMCache;
    if (!bitmap) {
        FS_UnlockVolume(vcb);
        return ioErr;
    }

    /* Check if enough free blocks available */
    if (vcb->vcbFreeBks < minBlocks) {
        FS_UnlockVolume(vcb);
        return dskFulErr;
    }

    /* Use allocation pointer as hint if no hint provided */
    if (startHint == 0) {
        startHint = vcb->vcbAllocPtr;
    }

    /* Find a run of free blocks */
    foundStart = FindFreeRun(bitmap, vcb->vcbNmAlBlks, startHint, minBlocks);
    if (foundStart == 0xFFFFFFFF) {
        /* Try again from beginning */
        foundStart = FindFreeRun(bitmap, vcb->vcbNmAlBlks, 0, minBlocks);
        if (foundStart == 0xFFFFFFFF) {
            FS_UnlockVolume(vcb);
            return dskFulErr;
        }
    }

    /* Count available blocks in the run */
    foundCount = 0;
    for (i = foundStart; i < vcb->vcbNmAlBlks && foundCount < maxBlocks; i++) {
        if (!TestBit(bitmap, i)) {
            foundCount++;
        } else {
            break;
        }
    }

    /* Mark blocks as allocated */
    for (i = 0; i < foundCount; i++) {
        SetBit(bitmap, foundStart + i);
    }

    /* Update VCB */
    vcb->vcbFreeBks -= (uint16_t)foundCount;
    vcb->vcbAllocPtr = (uint16_t)(foundStart + foundCount);
    if (vcb->vcbAllocPtr >= vcb->vcbNmAlBlks) {
        vcb->vcbAllocPtr = 0;
    }
    vcb->vcbFlags |= VCB_DIRTY;

    *actualStart = foundStart;
    *actualCount = foundCount;

    FS_UnlockVolume(vcb);

    return noErr;
}

/**
 * Free blocks on the volume
 */
OSErr Alloc_Free(VCB* vcb, uint32_t startBlock, uint32_t blockCount)
{
    uint8_t* bitmap;
    uint32_t i;

    if (!vcb || blockCount == 0) {
        return paramErr;
    }

    /* Check bounds */
    if (startBlock + blockCount > vcb->vcbNmAlBlks) {
        return paramErr;
    }

    FS_LockVolume(vcb);

    bitmap = (uint8_t*)vcb->vcbVBMCache;
    if (!bitmap) {
        FS_UnlockVolume(vcb);
        return ioErr;
    }

    /* Mark blocks as free */
    for (i = 0; i < blockCount; i++) {
        if (TestBit(bitmap, startBlock + i)) {
            ClearBit(bitmap, startBlock + i);
            vcb->vcbFreeBks++;
        }
    }

    /* Update allocation pointer to freed area for next search */
    if (startBlock < vcb->vcbAllocPtr) {
        vcb->vcbAllocPtr = (uint16_t)startBlock;
    }

    vcb->vcbFlags |= VCB_DIRTY;

    FS_UnlockVolume(vcb);

    return noErr;
}

/**
 * Count free blocks on the volume
 */
uint32_t Alloc_CountFree(VCB* vcb)
{
    uint8_t* bitmap;
    uint32_t count = 0;
    uint32_t i;

    if (!vcb || !vcb->vcbVBMCache) {
        return 0;
    }

    FS_LockVolume(vcb);

    bitmap = (uint8_t*)vcb->vcbVBMCache;

    /* Count free blocks */
    for (i = 0; i < vcb->vcbNmAlBlks; i++) {
        if (!TestBit(bitmap, i)) {
            count++;
        }
    }

    FS_UnlockVolume(vcb);

    return count;
}

/**
 * Check if blocks are allocated
 */
bool Alloc_Check(VCB* vcb, uint32_t startBlock, uint32_t blockCount)
{
    uint8_t* bitmap;
    uint32_t i;

    if (!vcb || !vcb->vcbVBMCache || blockCount == 0) {
        return false;
    }

    /* Check bounds */
    if (startBlock + blockCount > vcb->vcbNmAlBlks) {
        return false;
    }

    FS_LockVolume(vcb);

    bitmap = (uint8_t*)vcb->vcbVBMCache;

    /* Check if all blocks are allocated */
    for (i = 0; i < blockCount; i++) {
        if (!TestBit(bitmap, startBlock + i)) {
            FS_UnlockVolume(vcb);
            return false;
        }
    }

    FS_UnlockVolume(vcb);

    return true;
}

/* ============================================================================
 * Extent Management
 * ============================================================================ */

/**
 * Open the extents B-tree
 */
OSErr Ext_Open(VCB* vcb)
{
    BTCB* btcb;
    OSErr err;

    if (!vcb) {
        return paramErr;
    }

    /* Check if already open */
    if (vcb->vcbXTRef) {
        return noErr;
    }

    /* Open the extents B-tree */
    err = BTree_Open(vcb, EXTENTS_FILE_ID, &btcb);
    if (err != noErr) {
        return err;
    }

    /* Store reference in VCB */
    vcb->vcbXTRef = btcb;

    return noErr;
}

/**
 * Close the extents B-tree
 */
OSErr Ext_Close(VCB* vcb)
{
    if (!vcb || !vcb->vcbXTRef) {
        return paramErr;
    }

    /* Close the B-tree */
    BTree_Close((BTCB*)vcb->vcbXTRef);
    vcb->vcbXTRef = NULL;

    return noErr;
}

/**
 * Map a file block to a physical block
 */
OSErr Ext_Map(VCB* vcb, FCB* fcb, uint32_t fileBlock, uint32_t* physBlock, uint32_t* contiguous)
{
    ExtDataRec* extents;
    uint32_t currentBlock = 0;
    uint32_t i;

    if (!vcb || !fcb || !physBlock) {
        return paramErr;
    }

    /* Use appropriate extent record based on fork */
    if (fcb->fcbFlags & FCB_RESOURCE) {
        /* Resource fork - would need to look up resource extents */
        /* Simplified: not implemented */
        return fxRangeErr;
    } else {
        /* Data fork */
        extents = &fcb->fcbExtRec;
    }

    /* Search through extent records */
    for (i = 0; i < 3; i++) {
        if ((*extents)[i].blockCount == 0) {
            break;  /* End of extents */
        }

        if (fileBlock < currentBlock + (*extents)[i].blockCount) {
            /* Found the extent containing the file block */
            *physBlock = (*extents)[i].startBlock + (fileBlock - currentBlock);

            /* Calculate contiguous blocks if requested */
            if (contiguous) {
                *contiguous = (*extents)[i].blockCount - (fileBlock - currentBlock);
            }

            return noErr;
        }

        currentBlock += (*extents)[i].blockCount;
    }

    /* File block is beyond extents in FCB */
    /* Would need to search extents B-tree for overflow extents */
    return fxRangeErr;
}

/**
 * Extend a file
 */
OSErr Ext_Extend(VCB* vcb, FCB* fcb, uint32_t newSize)
{
    uint32_t currentBlocks;
    uint32_t neededBlocks;
    uint32_t allocStart;
    uint32_t allocCount;
    uint32_t clumpSize;
    OSErr err;

    if (!vcb || !fcb) {
        return paramErr;
    }

    FS_LockFCB(fcb);

    /* Calculate current and needed blocks */
    currentBlocks = (fcb->fcbPLen + vcb->vcbAlBlkSiz - 1) / vcb->vcbAlBlkSiz;
    neededBlocks = (newSize + vcb->vcbAlBlkSiz - 1) / vcb->vcbAlBlkSiz;

    if (neededBlocks <= currentBlocks) {
        /* No additional blocks needed */
        fcb->fcbEOF = newSize;
        FS_UnlockFCB(fcb);
        return noErr;
    }

    /* Calculate allocation size using clump size */
    clumpSize = fcb->fcbClmpSize ? fcb->fcbClmpSize : vcb->vcbClpSiz;
    uint32_t clumpBlocks = (clumpSize + vcb->vcbAlBlkSiz - 1) / vcb->vcbAlBlkSiz;
    uint32_t blocksToAlloc = neededBlocks - currentBlocks;
    if (blocksToAlloc < clumpBlocks) {
        blocksToAlloc = clumpBlocks;
    }

    /* Try to allocate contiguously after current end */
    uint32_t lastBlock = 0;
    if (currentBlocks > 0) {
        /* Find last allocated block */
        for (int i = 0; i < 3; i++) {
            if (fcb->fcbExtRec[i].blockCount > 0) {
                lastBlock = fcb->fcbExtRec[i].startBlock + fcb->fcbExtRec[i].blockCount;
            }
        }
    }

    /* Allocate blocks */
    err = Alloc_Blocks(vcb, lastBlock, blocksToAlloc, blocksToAlloc,
                      &allocStart, &allocCount);
    if (err != noErr) {
        /* Try allocating minimum needed */
        blocksToAlloc = neededBlocks - currentBlocks;
        err = Alloc_Blocks(vcb, 0, blocksToAlloc, blocksToAlloc,
                          &allocStart, &allocCount);
        if (err != noErr) {
            FS_UnlockFCB(fcb);
            return err;
        }
    }

    /* Add to extent record (simplified - assumes fits in first 3 extents) */
    for (int i = 0; i < 3; i++) {
        if (fcb->fcbExtRec[i].blockCount == 0) {
            /* Empty extent slot */
            fcb->fcbExtRec[i].startBlock = (uint16_t)allocStart;
            fcb->fcbExtRec[i].blockCount = (uint16_t)allocCount;
            break;
        } else if (fcb->fcbExtRec[i].startBlock + fcb->fcbExtRec[i].blockCount == allocStart) {
            /* Can merge with existing extent */
            fcb->fcbExtRec[i].blockCount += (uint16_t)allocCount;
            break;
        }
    }

    /* Update FCB */
    fcb->fcbPLen = (currentBlocks + allocCount) * vcb->vcbAlBlkSiz;
    if (newSize > fcb->fcbEOF) {
        fcb->fcbEOF = newSize;
    }
    fcb->fcbFlags |= FCB_DIRTY;

    FS_UnlockFCB(fcb);

    return noErr;
}

/**
 * Truncate a file
 */
OSErr Ext_Truncate(VCB* vcb, FCB* fcb, uint32_t newSize)
{
    uint32_t currentBlocks;
    uint32_t neededBlocks;
    uint32_t blocksToFree;
    OSErr err;

    if (!vcb || !fcb) {
        return paramErr;
    }

    FS_LockFCB(fcb);

    /* Calculate current and needed blocks */
    currentBlocks = (fcb->fcbPLen + vcb->vcbAlBlkSiz - 1) / vcb->vcbAlBlkSiz;
    neededBlocks = (newSize + vcb->vcbAlBlkSiz - 1) / vcb->vcbAlBlkSiz;

    if (neededBlocks >= currentBlocks) {
        /* No blocks to free */
        fcb->fcbEOF = newSize;
        FS_UnlockFCB(fcb);
        return noErr;
    }

    blocksToFree = currentBlocks - neededBlocks;

    /* Free blocks from end of extents (simplified) */
    /* Would need to properly handle extent overflow */
    for (int i = 2; i >= 0; i--) {
        if (fcb->fcbExtRec[i].blockCount > 0) {
            if (blocksToFree >= fcb->fcbExtRec[i].blockCount) {
                /* Free entire extent */
                err = Alloc_Free(vcb, fcb->fcbExtRec[i].startBlock,
                               fcb->fcbExtRec[i].blockCount);
                if (err != noErr) {
                    FS_UnlockFCB(fcb);
                    return err;
                }
                blocksToFree -= fcb->fcbExtRec[i].blockCount;
                fcb->fcbExtRec[i].startBlock = 0;
                fcb->fcbExtRec[i].blockCount = 0;
            } else {
                /* Free partial extent */
                uint32_t keepBlocks = fcb->fcbExtRec[i].blockCount - blocksToFree;
                err = Alloc_Free(vcb, fcb->fcbExtRec[i].startBlock + keepBlocks,
                               blocksToFree);
                if (err != noErr) {
                    FS_UnlockFCB(fcb);
                    return err;
                }
                fcb->fcbExtRec[i].blockCount = (uint16_t)keepBlocks;
                blocksToFree = 0;
            }

            if (blocksToFree == 0) {
                break;
            }
        }
    }

    /* Update FCB */
    fcb->fcbPLen = neededBlocks * vcb->vcbAlBlkSiz;
    fcb->fcbEOF = newSize;
    fcb->fcbFlags |= FCB_DIRTY;

    FS_UnlockFCB(fcb);

    return noErr;
}

/* ============================================================================
 * Cache Management
 * ============================================================================ */

/**
 * Initialize the cache system
 */
OSErr Cache_Init(uint32_t cacheSize)
{
    uint32_t numBuffers;
    CacheBuffer* buffers;
    uint32_t hashSize;

    if (g_FSGlobals.cacheBuffers) {
        return noErr;  /* Already initialized */
    }

    /* Calculate number of buffers */
    numBuffers = cacheSize;
    if (numBuffers < 32) {
        numBuffers = 32;
    }

    /* Allocate cache buffers */
    buffers = (CacheBuffer*)calloc(numBuffers, sizeof(CacheBuffer));
    if (!buffers) {
        return memFullErr;
    }

    /* Initialize free list */
    for (uint32_t i = 0; i < numBuffers - 1; i++) {
        buffers[i].cbFreeNext = &buffers[i + 1];
        buffers[i + 1].cbFreePrev = &buffers[i];
    }

    /* Allocate hash table */
    hashSize = numBuffers * 2;  /* Make hash table 2x buffer count */
    g_FSGlobals.cacheHash = (CacheBuffer**)calloc(hashSize, sizeof(CacheBuffer*));
    if (!g_FSGlobals.cacheHash) {
        free(buffers);
        return memFullErr;
    }

    /* Store in globals */
    g_FSGlobals.cacheBuffers = buffers;
    g_FSGlobals.cacheSize = numBuffers;
    g_FSGlobals.cacheFreeList = &buffers[0];
    g_FSGlobals.cacheHashSize = hashSize;

    return noErr;
}

/**
 * Shutdown the cache system
 */
void Cache_Shutdown(void)
{
    /* Flush all dirty buffers */
    Cache_FlushAll();

    /* Free cache buffers */
    if (g_FSGlobals.cacheBuffers) {
        free(g_FSGlobals.cacheBuffers);
        g_FSGlobals.cacheBuffers = NULL;
    }

    /* Free hash table */
    if (g_FSGlobals.cacheHash) {
        free(g_FSGlobals.cacheHash);
        g_FSGlobals.cacheHash = NULL;
    }

    g_FSGlobals.cacheSize = 0;
    g_FSGlobals.cacheFreeList = NULL;
}

/**
 * Get a block from cache
 */
OSErr Cache_GetBlock(VCB* vcb, uint32_t blockNum, CacheBuffer** buffer)
{
    CacheBuffer* cb;
    uint32_t hashIndex;
    OSErr err;

    if (!vcb || !buffer) {
        return paramErr;
    }

    *buffer = NULL;

    /* Calculate hash index */
    hashIndex = (((uintptr_t)vcb ^ blockNum) * 2654435761U) % g_FSGlobals.cacheHashSize;

    FS_LockGlobal();

    /* Search hash chain */
    cb = g_FSGlobals.cacheHash[hashIndex];
    while (cb) {
        if (cb->cbVCB == vcb && cb->cbBlkNum == blockNum) {
            /* Found in cache */
            cb->cbRefCnt++;
            cb->cbLastUse = DateTime_Current();
            g_FSGlobals.cacheHits++;
            *buffer = cb;
            FS_UnlockGlobal();
            return noErr;
        }
        cb = cb->cbNext;
    }

    /* Not in cache - need to read from disk */
    g_FSGlobals.cacheMisses++;

    /* Get a free buffer */
    cb = g_FSGlobals.cacheFreeList;
    if (!cb) {
        /* No free buffers - would need to implement LRU eviction */
        FS_UnlockGlobal();
        return memFullErr;
    }

    /* Remove from free list */
    g_FSGlobals.cacheFreeList = cb->cbFreeNext;
    if (cb->cbFreeNext) {
        cb->cbFreeNext->cbFreePrev = NULL;
    }

    /* Initialize buffer */
    cb->cbVCB = vcb;
    cb->cbBlkNum = blockNum;
    cb->cbFlags = CACHE_IN_USE;
    cb->cbRefCnt = 1;
    cb->cbLastUse = DateTime_Current();

    /* Add to hash chain */
    cb->cbNext = g_FSGlobals.cacheHash[hashIndex];
    if (cb->cbNext) {
        cb->cbNext->cbPrev = cb;
    }
    cb->cbPrev = NULL;
    g_FSGlobals.cacheHash[hashIndex] = cb;

    FS_UnlockGlobal();

    /* Read block from disk */
    err = IO_ReadBlocks(vcb, blockNum, 1, cb->cbData);
    if (err != noErr) {
        /* Return buffer to free list */
        Cache_ReleaseBlock(cb, false);
        return err;
    }

    *buffer = cb;
    return noErr;
}

/**
 * Release a cache block
 */
OSErr Cache_ReleaseBlock(CacheBuffer* buffer, bool dirty)
{
    if (!buffer) {
        return paramErr;
    }

    FS_LockGlobal();

    /* Mark as dirty if requested */
    if (dirty) {
        buffer->cbFlags |= CACHE_DIRTY;
    }

    /* Decrement reference count */
    if (buffer->cbRefCnt > 0) {
        buffer->cbRefCnt--;
    }

    /* If no longer referenced, can be reused */
    if (buffer->cbRefCnt == 0) {
        buffer->cbFlags &= ~CACHE_LOCKED;
    }

    FS_UnlockGlobal();

    return noErr;
}

/**
 * Flush all dirty buffers for a volume
 */
OSErr Cache_FlushVolume(VCB* vcb)
{
    CacheBuffer* cb;
    OSErr err;

    if (!vcb) {
        return paramErr;
    }

    FS_LockGlobal();

    /* Scan all cache buffers */
    for (uint32_t i = 0; i < g_FSGlobals.cacheSize; i++) {
        cb = &g_FSGlobals.cacheBuffers[i];

        if (cb->cbVCB == vcb && (cb->cbFlags & CACHE_DIRTY)) {
            /* Write dirty buffer */
            err = IO_WriteBlocks(vcb, cb->cbBlkNum, 1, cb->cbData);
            if (err != noErr) {
                FS_UnlockGlobal();
                return err;
            }

            /* Clear dirty flag */
            cb->cbFlags &= ~CACHE_DIRTY;
        }
    }

    FS_UnlockGlobal();

    return noErr;
}

/**
 * Flush all dirty buffers
 */
OSErr Cache_FlushAll(void)
{
    CacheBuffer* cb;
    OSErr err;

    FS_LockGlobal();

    /* Scan all cache buffers */
    for (uint32_t i = 0; i < g_FSGlobals.cacheSize; i++) {
        cb = &g_FSGlobals.cacheBuffers[i];

        if (cb->cbVCB && (cb->cbFlags & CACHE_DIRTY)) {
            /* Write dirty buffer */
            err = IO_WriteBlocks(cb->cbVCB, cb->cbBlkNum, 1, cb->cbData);
            if (err != noErr) {
                /* Continue flushing other buffers */
            }

            /* Clear dirty flag */
            cb->cbFlags &= ~CACHE_DIRTY;
        }
    }

    FS_UnlockGlobal();

    return noErr;
}

/**
 * Invalidate cache for a volume
 */
void Cache_Invalidate(VCB* vcb)
{
    CacheBuffer* cb;
    uint32_t hashIndex;

    if (!vcb) {
        return;
    }

    FS_LockGlobal();

    /* Scan all cache buffers */
    for (uint32_t i = 0; i < g_FSGlobals.cacheSize; i++) {
        cb = &g_FSGlobals.cacheBuffers[i];

        if (cb->cbVCB == vcb) {
            /* Calculate hash index */
            hashIndex = (((uintptr_t)vcb ^ cb->cbBlkNum) * 2654435761U) %
                       g_FSGlobals.cacheHashSize;

            /* Remove from hash chain */
            if (cb->cbPrev) {
                cb->cbPrev->cbNext = cb->cbNext;
            } else {
                g_FSGlobals.cacheHash[hashIndex] = cb->cbNext;
            }
            if (cb->cbNext) {
                cb->cbNext->cbPrev = cb->cbPrev;
            }

            /* Clear buffer */
            memset(cb, 0, sizeof(CacheBuffer));

            /* Add to free list */
            cb->cbFreeNext = g_FSGlobals.cacheFreeList;
            if (g_FSGlobals.cacheFreeList) {
                g_FSGlobals.cacheFreeList->cbFreePrev = cb;
            }
            g_FSGlobals.cacheFreeList = cb;
        }
    }

    FS_UnlockGlobal();
}

/* ============================================================================
 * I/O Operations
 * ============================================================================ */

/**
 * Read blocks from disk
 */
OSErr IO_ReadBlocks(VCB* vcb, uint32_t startBlock, uint32_t blockCount, void* buffer)
{
    uint64_t offset;
    uint32_t bytes;
    OSErr err;

    if (!vcb || !buffer || blockCount == 0) {
        return paramErr;
    }

    if (!g_PlatformHooks.DeviceRead) {
        return extFSErr;
    }

    /* Calculate byte offset and count */
    offset = (uint64_t)startBlock * BLOCK_SIZE;
    bytes = blockCount * BLOCK_SIZE;

    /* Read from device */
    err = g_PlatformHooks.DeviceRead(vcb->vcbDevice, offset, bytes, buffer);

    return err;
}

/**
 * Write blocks to disk
 */
OSErr IO_WriteBlocks(VCB* vcb, uint32_t startBlock, uint32_t blockCount, const void* buffer)
{
    uint64_t offset;
    uint32_t bytes;
    OSErr err;

    if (!vcb || !buffer || blockCount == 0) {
        return paramErr;
    }

    if (!g_PlatformHooks.DeviceWrite) {
        return extFSErr;
    }

    /* Check write protection */
    if (vcb->vcbAtrb & kioVAtrbSoftwareLock) {
        return wPrErr;
    }

    /* Calculate byte offset and count */
    offset = (uint64_t)startBlock * BLOCK_SIZE;
    bytes = blockCount * BLOCK_SIZE;

    /* Write to device */
    err = g_PlatformHooks.DeviceWrite(vcb->vcbDevice, offset, bytes, buffer);

    if (err == noErr) {
        /* Update write count */
        vcb->vcbWrCnt++;
        vcb->vcbLsMod = DateTime_Current();
    }

    return err;
}

/**
 * Read from a file fork
 */
OSErr IO_ReadFork(FCB* fcb, uint32_t offset, uint32_t count, void* buffer, uint32_t* actual)
{
    VCB* vcb;
    uint32_t fileBlock;
    uint32_t physBlock;
    uint32_t blockOffset;
    uint32_t contiguous;
    uint32_t toRead;
    uint32_t totalRead = 0;
    uint8_t* dest;
    CacheBuffer* cb;
    OSErr err;

    if (!fcb || !buffer || !actual) {
        return paramErr;
    }

    *actual = 0;
    vcb = fcb->fcbVPtr;

    /* Check bounds */
    if (offset >= fcb->fcbEOF) {
        return eofErr;
    }

    /* Limit read to EOF */
    if (offset + count > fcb->fcbEOF) {
        count = fcb->fcbEOF - offset;
    }

    dest = (uint8_t*)buffer;

    while (count > 0) {
        /* Calculate file block and offset within block */
        fileBlock = offset / vcb->vcbAlBlkSiz;
        blockOffset = offset % vcb->vcbAlBlkSiz;

        /* Map to physical block */
        err = Ext_Map(vcb, fcb, fileBlock, &physBlock, &contiguous);
        if (err != noErr) {
            if (totalRead > 0) {
                *actual = totalRead;
                return noErr;  /* Partial read */
            }
            return err;
        }

        /* Calculate amount to read from this block */
        toRead = vcb->vcbAlBlkSiz - blockOffset;
        if (toRead > count) {
            toRead = count;
        }

        /* Read through cache */
        err = Cache_GetBlock(vcb, physBlock, &cb);
        if (err != noErr) {
            if (totalRead > 0) {
                *actual = totalRead;
                return noErr;  /* Partial read */
            }
            return err;
        }

        /* Copy data from cache buffer */
        memcpy(dest, cb->cbData + blockOffset, toRead);

        /* Release cache buffer */
        Cache_ReleaseBlock(cb, false);

        /* Update counters */
        dest += toRead;
        offset += toRead;
        count -= toRead;
        totalRead += toRead;
    }

    /* Update file position */
    fcb->fcbCrPs = offset;
    *actual = totalRead;

    return noErr;
}

/**
 * Write to a file fork
 */
OSErr IO_WriteFork(FCB* fcb, uint32_t offset, uint32_t count, const void* buffer, uint32_t* actual)
{
    VCB* vcb;
    uint32_t fileBlock;
    uint32_t physBlock;
    uint32_t blockOffset;
    uint32_t contiguous;
    uint32_t toWrite;
    uint32_t totalWritten = 0;
    const uint8_t* src;
    CacheBuffer* cb;
    OSErr err;

    if (!fcb || !buffer || !actual) {
        return paramErr;
    }

    *actual = 0;
    vcb = fcb->fcbVPtr;

    /* Check write permission */
    if (!(fcb->fcbFlags & FCB_WRITE_PERM)) {
        return wrPermErr;
    }

    /* Extend file if necessary */
    if (offset + count > fcb->fcbPLen) {
        err = Ext_Extend(vcb, fcb, offset + count);
        if (err != noErr) {
            return err;
        }
    }

    src = (const uint8_t*)buffer;

    while (count > 0) {
        /* Calculate file block and offset within block */
        fileBlock = offset / vcb->vcbAlBlkSiz;
        blockOffset = offset % vcb->vcbAlBlkSiz;

        /* Map to physical block */
        err = Ext_Map(vcb, fcb, fileBlock, &physBlock, &contiguous);
        if (err != noErr) {
            if (totalWritten > 0) {
                *actual = totalWritten;
                fcb->fcbFlags |= FCB_DIRTY;
                return noErr;  /* Partial write */
            }
            return err;
        }

        /* Calculate amount to write to this block */
        toWrite = vcb->vcbAlBlkSiz - blockOffset;
        if (toWrite > count) {
            toWrite = count;
        }

        /* Get block from cache */
        err = Cache_GetBlock(vcb, physBlock, &cb);
        if (err != noErr) {
            if (totalWritten > 0) {
                *actual = totalWritten;
                fcb->fcbFlags |= FCB_DIRTY;
                return noErr;  /* Partial write */
            }
            return err;
        }

        /* Copy data to cache buffer */
        memcpy(cb->cbData + blockOffset, src, toWrite);

        /* Release cache buffer (mark as dirty) */
        Cache_ReleaseBlock(cb, true);

        /* Update counters */
        src += toWrite;
        offset += toWrite;
        count -= toWrite;
        totalWritten += toWrite;
    }

    /* Update file position and EOF if extended */
    fcb->fcbCrPs = offset;
    if (offset > fcb->fcbEOF) {
        fcb->fcbEOF = offset;
    }

    /* Mark FCB as dirty */
    fcb->fcbFlags |= FCB_DIRTY;

    *actual = totalWritten;

    return noErr;
}