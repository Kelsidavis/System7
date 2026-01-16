#include "MemoryMgr/MemoryManager.h"
/*
 * FileManagerStubs.c - Stub implementations for File Manager dependencies
 *
 * This file provides minimal stub implementations for File Manager
 * functions and types that are not yet fully implemented.
 */

#include "SystemTypes.h"
#include "FileManager.h"
#include "FileManager_Internal.h"
#include <stdlib.h>
#include <string.h>
#include "FS/FSLogging.h"

/* Forward declaration for POSIX stub */
int sched_yield(void);

/* ============================================================================
 * Global variables
 * ============================================================================ */

/* Global state and hooks are now defined in FileManager.c */
extern FSGlobals g_FSGlobals;
extern PlatformHooks g_PlatformHooks;

/* ============================================================================
 * Stub Implementations
 * ============================================================================ */

/* Volume Management */
VCB* VCB_Alloc(void) {
    FS_LOG_DEBUG("VCB_Alloc stub\n");
    return (VCB*)NewPtrClear(sizeof(VCB));
}

void VCB_Free(VCB* vcb) {
    FS_LOG_DEBUG("VCB_Free stub\n");
    if (vcb) DisposePtr((Ptr)vcb);
}

VCB* VCB_Find(VolumeRefNum vRefNum) {
    FS_LOG_DEBUG("VCB_Find stub: vRefNum=%d\n", vRefNum);
    VCB* vcb = g_FSGlobals.vcbQueue;
    while (vcb) {
        if (vcb->base.vcbVRefNum == vRefNum) {
            return vcb;
        }
        vcb = vcb->vcbNext;
    }
    return NULL;
}

VCB* VCB_FindByName(const UInt8* name) {
    FS_LOG_DEBUG("VCB_FindByName stub\n");
    return NULL;
}

OSErr VCB_Mount(UInt16 drvNum, VCB** newVCB) {
    FS_LOG_DEBUG("VCB_Mount stub: drvNum=%d\n", drvNum);
    return nsvErr;
}

OSErr VCB_Unmount(VCB* vcb) {
    FS_LOG_DEBUG("VCB_Unmount stub\n");
    return noErr;
}

OSErr VCB_Flush(VCB* vcb) {
    FS_LOG_DEBUG("VCB_Flush stub\n");
    return noErr;
}

OSErr VCB_Update(VCB* vcb) {
    FS_LOG_DEBUG("VCB_Update stub\n");
    return noErr;
}

/* File Control Block Management */
FCB* FCB_Alloc(void) {
    FS_LOG_DEBUG("FCB_Alloc stub\n");
    if (g_FSGlobals.fcbFree < g_FSGlobals.fcbCount) {
        FCB* fcb = &g_FSGlobals.fcbArray[g_FSGlobals.fcbFree];
        g_FSGlobals.fcbFree = fcb->fcbRefNum;
        fcb->fcbRefNum = g_FSGlobals.fcbFree + 1;
        return fcb;
    }
    return NULL;
}

void FCB_Free(FCB* fcb) {
    FS_LOG_DEBUG("FCB_Free stub\n");
    if (fcb) {
        memset(fcb, 0, sizeof(FCB));
    }
}

FCB* FCB_Find(FileRefNum refNum) {
    FS_LOG_DEBUG("FCB_Find stub: refNum=%d\n", refNum);
    if (refNum > 0 && refNum <= g_FSGlobals.fcbCount) {
        FCB* fcb = &g_FSGlobals.fcbArray[refNum - 1];
        if (fcb->base.fcbFlNm != 0) {
            return fcb;
        }
    }
    return NULL;
}

FCB* FCB_FindByID(VCB* vcb, UInt32 fileID) {
    FS_LOG_DEBUG("FCB_FindByID stub: fileID=%d\n", fileID);
    return NULL;
}

OSErr FCB_Open(VCB* vcb, UInt32 dirID, const UInt8* name, UInt8 permission, FCB** newFCB) {
    FS_LOG_DEBUG("FCB_Open stub\n");
    return fnfErr;
}

OSErr FCB_Close(FCB* fcb) {
    FS_LOG_DEBUG("FCB_Close stub\n");
    return noErr;
}

OSErr FCB_Flush(FCB* fcb) {
    FS_LOG_DEBUG("FCB_Flush stub\n");
    return noErr;
}

/* Working Directory Management */
WDCB* WDCB_Alloc(void) {
    FS_LOG_DEBUG("WDCB_Alloc stub\n");
    return (WDCB*)NewPtrClear(sizeof(WDCB));
}

void WDCB_Free(WDCB* wdcb) {
    FS_LOG_DEBUG("WDCB_Free stub\n");
    if (wdcb) DisposePtr((Ptr)wdcb);
}

WDCB* WDCB_Find(WDRefNum wdRefNum) {
    FS_LOG_DEBUG("WDCB_Find stub: wdRefNum=%d\n", wdRefNum);
    return NULL;
}

OSErr WDCB_Create(VCB* vcb, UInt32 dirID, UInt32 procID, WDCB** newWDCB) {
    FS_LOG_DEBUG("WDCB_Create stub\n");
    return tmwdoErr;
}

/* B-tree Operations */
OSErr BTree_Open(VCB* vcb, UInt32 fileID, BTCB** btcb) {
    FS_LOG_DEBUG("BTree_Open stub: fileID=%d\n", fileID);
    return btNoErr;
}

OSErr BTree_Close(BTCB* btcb) {
    FS_LOG_DEBUG("BTree_Close stub\n");
    return btNoErr;
}

OSErr BTree_Search(BTCB* btcb, const void* key, void* record, UInt16* recordSize, UInt32* hint) {
    FS_LOG_DEBUG("BTree_Search stub\n");
    return btRecNotFnd;
}

OSErr BTree_Insert(BTCB* btcb, const void* key, const void* record, UInt16 recordSize) {
    FS_LOG_DEBUG("BTree_Insert stub\n");
    return btNoErr;
}

OSErr BTree_Delete(BTCB* btcb, const void* key) {
    FS_LOG_DEBUG("BTree_Delete stub\n");
    return btNoErr;
}

OSErr BTree_GetNode(BTCB* btcb, UInt32 nodeNum, void** nodePtr) {
    FS_LOG_DEBUG("BTree_GetNode stub: nodeNum=%d\n", nodeNum);
    return btNoErr;
}

OSErr BTree_ReleaseNode(BTCB* btcb, UInt32 nodeNum) {
    FS_LOG_DEBUG("BTree_ReleaseNode stub: nodeNum=%d\n", nodeNum);
    return btNoErr;
}

OSErr BTree_FlushNode(BTCB* btcb, UInt32 nodeNum) {
    FS_LOG_DEBUG("BTree_FlushNode stub: nodeNum=%d\n", nodeNum);
    return btNoErr;
}

/* Catalog Operations */
OSErr Cat_Open(VCB* vcb) {
    FS_LOG_DEBUG("Cat_Open stub\n");
    return noErr;
}

OSErr Cat_Close(VCB* vcb) {
    FS_LOG_DEBUG("Cat_Close stub\n");
    return noErr;
}

OSErr Cat_Lookup(VCB* vcb, UInt32 dirID, const UInt8* name, void* catData, UInt32* hint) {
    FS_LOG_DEBUG("Cat_Lookup stub: dirID=%d\n", dirID);
    return fnfErr;
}

OSErr Cat_Create(VCB* vcb, UInt32 dirID, const UInt8* name, UInt8 type, void* catData) {
    FS_LOG_DEBUG("Cat_Create stub\n");
    return noErr;
}

OSErr Cat_Delete(VCB* vcb, UInt32 dirID, const UInt8* name) {
    FS_LOG_DEBUG("Cat_Delete stub\n");
    return noErr;
}

OSErr Cat_Rename(VCB* vcb, UInt32 dirID, const UInt8* oldName, const UInt8* newName) {
    FS_LOG_DEBUG("Cat_Rename stub\n");
    return noErr;
}

OSErr Cat_Move(VCB* vcb, UInt32 srcDirID, const UInt8* name, UInt32 dstDirID) {
    FS_LOG_DEBUG("Cat_Move stub\n");
    return noErr;
}

OSErr Cat_GetInfo(VCB* vcb, UInt32 dirID, const UInt8* name, CInfoPBRec* pb) {
    FS_LOG_DEBUG("Cat_GetInfo stub\n");
    return fnfErr;
}

OSErr Cat_SetInfo(VCB* vcb, UInt32 dirID, const UInt8* name, const CInfoPBRec* pb) {
    FS_LOG_DEBUG("Cat_SetInfo stub\n");
    return noErr;
}

CNodeID Cat_GetNextID(VCB* vcb) {
    FS_LOG_DEBUG("Cat_GetNextID stub\n");
    static CNodeID nextID = 100;
    return nextID++;
}

/* Extent Management */
OSErr Ext_Open(VCB* vcb) {
    FS_LOG_DEBUG("Ext_Open stub\n");
    return noErr;
}

OSErr Ext_Close(VCB* vcb) {
    FS_LOG_DEBUG("Ext_Close stub\n");
    return noErr;
}

OSErr Ext_Allocate(VCB* vcb, UInt32 fileID, UInt8 forkType, UInt32 blocks, ExtDataRec extents) {
    FS_LOG_DEBUG("Ext_Allocate stub: fileID=%d, blocks=%d\n", fileID, blocks);
    return noErr;
}

OSErr Ext_Deallocate(VCB* vcb, UInt32 fileID, UInt8 forkType, UInt32 startBlock) {
    FS_LOG_DEBUG("Ext_Deallocate stub\n");
    return noErr;
}

OSErr Ext_Map(VCB* vcb, FCB* fcb, UInt32 fileBlock, UInt32* physBlock, UInt32* contiguous) {
    ExtentRecord* extents;
    UInt32 currentBlock = 0;
    UInt32 i;

    if (!vcb || !fcb || !physBlock) {
        return paramErr;
    }

    /* Use data fork extent record from FCB */
    extents = &fcb->base.fcbExtRec;

    /* Search through extent records */
    for (i = 0; i < 3; i++) {
        if ((*extents).extent[i].blockCount == 0) {
            break;  /* End of extents */
        }

        if (fileBlock < currentBlock + (*extents).extent[i].blockCount) {
            /* Found the extent containing the file block */
            *physBlock = (*extents).extent[i].startBlock + (fileBlock - currentBlock);

            /* Calculate contiguous blocks if requested */
            if (contiguous) {
                *contiguous = (*extents).extent[i].blockCount - (fileBlock - currentBlock);
            }

            FS_LOG_DEBUG("Ext_Map: fileBlock=%u -> physBlock=%u (extent %u)\n",
                         fileBlock, *physBlock, i);
            return noErr;
        }

        currentBlock += (*extents).extent[i].blockCount;
    }

    /* File block is beyond extents in FCB - search overflow B-tree */
    FS_LOG_DEBUG("Ext_Map: searching overflow for fileBlock=%u (currentBlock=%u)\n",
                 fileBlock, currentBlock);

    /* Search overflow extents starting from first overflow FABN */
    UInt32 overflowFABN = currentBlock;  /* Start of overflow extents */
    ExtDataRec overflowExtents;
    OSErr err;

    while (currentBlock < fileBlock) {
        /* Search for extent record starting at this FABN */
        err = Ext_SearchOverflow(vcb, fcb->base.fcbFlNm, kDataFork,
                                overflowFABN, &overflowExtents);
        if (err != noErr) {
            FS_LOG_DEBUG("Ext_Map: overflow extent not found at FABN=%u\n", overflowFABN);
            return ioErr;
        }

        /* Search through this overflow extent record */
        for (i = 0; i < 3; i++) {
            if (overflowExtents[i].xdrNumABlks == 0) {
                break;  /* End of extents in this record */
            }

            if (fileBlock < currentBlock + overflowExtents[i].xdrNumABlks) {
                /* Found the extent containing the file block */
                *physBlock = overflowExtents[i].xdrStABN + (fileBlock - currentBlock);

                /* Calculate contiguous blocks if requested */
                if (contiguous) {
                    *contiguous = overflowExtents[i].xdrNumABlks - (fileBlock - currentBlock);
                }

                FS_LOG_DEBUG("Ext_Map: fileBlock=%u -> physBlock=%u (overflow extent FABN=%u[%u])\n",
                           fileBlock, *physBlock, overflowFABN, i);
                return noErr;
            }

            currentBlock += overflowExtents[i].xdrNumABlks;
        }

        /* Move to next overflow extent record */
        overflowFABN = currentBlock;
    }

    FS_LOG_DEBUG("Ext_Map: fileBlock=%u not found in overflow extents\n", fileBlock);
    return ioErr;
}

/* ============================================================================
 * Extent Management - File Extension/Truncation
 * ============================================================================ */

OSErr Ext_Extend(VCB* vcb, FCB* fcb, UInt32 newSize) {
    UInt32 currentBlocks;
    UInt32 neededBlocks;
    UInt32 allocStart;
    UInt32 allocCount;
    UInt32 clumpSize;
    UInt32 blocksToAlloc;
    UInt32 lastBlock = 0;
    UInt32 i;
    OSErr err;

    if (!vcb || !fcb) {
        return paramErr;
    }

    /* Calculate current and needed blocks */
    currentBlocks = (fcb->base.fcbPLen + vcb->base.vcbAlBlkSiz - 1) / vcb->base.vcbAlBlkSiz;
    neededBlocks = (newSize + vcb->base.vcbAlBlkSiz - 1) / vcb->base.vcbAlBlkSiz;

    if (neededBlocks <= currentBlocks) {
        /* No additional blocks needed */
        return noErr;
    }

    /* Calculate allocation size using clump size */
    clumpSize = (fcb->base.fcbClpSiz != 0) ? fcb->base.fcbClpSiz : vcb->base.vcbClpSiz;
    UInt32 clumpBlocks = (clumpSize + vcb->base.vcbAlBlkSiz - 1) / vcb->base.vcbAlBlkSiz;
    blocksToAlloc = neededBlocks - currentBlocks;
    if (blocksToAlloc < clumpBlocks) {
        blocksToAlloc = clumpBlocks;  /* Allocate at least one clump */
    }

    /* Find last allocated block for contiguous allocation hint */
    if (currentBlocks > 0) {
        for (i = 0; i < 3; i++) {
            if (fcb->base.fcbExtRec.extent[i].blockCount > 0) {
                lastBlock = fcb->base.fcbExtRec.extent[i].startBlock +
                           fcb->base.fcbExtRec.extent[i].blockCount;
            }
        }
    }

    /* Try to allocate contiguously after current end */
    err = Alloc_Blocks(vcb, lastBlock, blocksToAlloc, blocksToAlloc,
                      &allocStart, &allocCount);
    if (err != noErr) {
        /* Try allocating minimum needed */
        blocksToAlloc = neededBlocks - currentBlocks;
        err = Alloc_Blocks(vcb, 0, blocksToAlloc, blocksToAlloc,
                          &allocStart, &allocCount);
        if (err != noErr) {
            FS_LOG_ERROR("Ext_Extend: allocation failed, needed %u blocks\n",
                        blocksToAlloc);
            return err;
        }
    }

    /* Add to extent record (simplified - assumes fits in first 3 extents) */
    Boolean extentAdded = false;
    for (i = 0; i < 3; i++) {
        if (fcb->base.fcbExtRec.extent[i].blockCount == 0) {
            /* Empty extent slot */
            fcb->base.fcbExtRec.extent[i].startBlock = (UInt16)allocStart;
            fcb->base.fcbExtRec.extent[i].blockCount = (UInt16)allocCount;
            extentAdded = true;
            FS_LOG_DEBUG("Ext_Extend: added new extent[%u]: start=%u count=%u\n",
                        i, allocStart, allocCount);
            break;
        } else if (fcb->base.fcbExtRec.extent[i].startBlock +
                   fcb->base.fcbExtRec.extent[i].blockCount == allocStart) {
            /* Can merge with existing extent */
            fcb->base.fcbExtRec.extent[i].blockCount += (UInt16)allocCount;
            extentAdded = true;
            FS_LOG_DEBUG("Ext_Extend: merged with extent[%u]: new count=%u\n",
                        i, fcb->base.fcbExtRec.extent[i].blockCount);
            break;
        }
    }

    if (!extentAdded) {
        /* FCB extent record is full - use overflow B-tree */
        FS_LOG_DEBUG("Ext_Extend: FCB extents full, using overflow B-tree\n");

        /* Calculate starting FABN for overflow extent */
        UInt32 overflowFABN = currentBlocks;

        /* Create overflow extent record */
        ExtDataRec overflowExtents;
        memset(&overflowExtents, 0, sizeof(ExtDataRec));
        overflowExtents[0].xdrStABN = (UInt16)allocStart;
        overflowExtents[0].xdrNumABlks = (UInt16)allocCount;

        /* Add to overflow B-tree */
        err = Ext_AddOverflow(vcb, fcb->base.fcbFlNm, kDataFork,
                             overflowFABN, &overflowExtents);
        if (err != noErr) {
            FS_LOG_ERROR("Ext_Extend: failed to add overflow extent: %d\n", err);
            Alloc_Free(vcb, allocStart, allocCount);
            return err;
        }

        FS_LOG_DEBUG("Ext_Extend: added overflow extent at FABN=%u: start=%u count=%u\n",
                    overflowFABN, allocStart, allocCount);
    }

    /* Update FCB */
    fcb->base.fcbPLen = (currentBlocks + allocCount) * vcb->base.vcbAlBlkSiz;
    fcb->base.fcbFlags |= FCB_DIRTY;

    FS_LOG_DEBUG("Ext_Extend: extended from %u to %u blocks (pLen=%u)\n",
                currentBlocks, currentBlocks + allocCount, fcb->base.fcbPLen);

    return noErr;
}

OSErr Ext_Truncate(VCB* vcb, FCB* fcb, UInt32 newSize) {
    UInt32 currentBlocks;
    UInt32 neededBlocks;
    UInt32 blocksToFree;
    SInt32 i;
    OSErr err;

    if (!vcb || !fcb) {
        return paramErr;
    }

    /* Calculate current and needed blocks */
    currentBlocks = (fcb->base.fcbPLen + vcb->base.vcbAlBlkSiz - 1) / vcb->base.vcbAlBlkSiz;
    neededBlocks = (newSize + vcb->base.vcbAlBlkSiz - 1) / vcb->base.vcbAlBlkSiz;

    if (neededBlocks >= currentBlocks) {
        /* No blocks to free */
        return noErr;
    }

    blocksToFree = currentBlocks - neededBlocks;

    /* First, check if we have overflow extents to free */
    UInt32 fcbBlocks = 0;
    for (i = 0; i < 3; i++) {
        fcbBlocks += fcb->base.fcbExtRec.extent[i].blockCount;
    }

    /* Free overflow extents if file extends beyond FCB extents */
    if (currentBlocks > fcbBlocks && blocksToFree > 0) {
        UInt32 overflowFABN = currentBlocks;  /* Start from end of file */
        ExtDataRec overflowExtents;

        /* Work backwards through overflow extents */
        while (overflowFABN > fcbBlocks && blocksToFree > 0) {
            /* Find the overflow extent that contains this FABN */
            /* We need to search backwards from currentBlocks to find extent boundaries */
            UInt32 searchFABN = fcbBlocks;
            UInt32 lastFoundFABN = fcbBlocks;

            while (searchFABN < overflowFABN) {
                err = Ext_SearchOverflow(vcb, fcb->base.fcbFlNm, kDataFork,
                                        searchFABN, &overflowExtents);
                if (err != noErr) {
                    break;  /* No more overflow extents */
                }

                /* Calculate end of this extent record */
                UInt32 extentBlocks = 0;
                for (i = 0; i < 3; i++) {
                    extentBlocks += overflowExtents[i].xdrNumABlks;
                }

                lastFoundFABN = searchFABN;
                searchFABN += extentBlocks;
            }

            /* Now lastFoundFABN points to the last overflow extent record */
            if (lastFoundFABN >= fcbBlocks) {
                err = Ext_SearchOverflow(vcb, fcb->base.fcbFlNm, kDataFork,
                                        lastFoundFABN, &overflowExtents);
                if (err != noErr) {
                    break;
                }

                /* Free extents from this record (working backwards) */
                for (i = 2; i >= 0; i--) {
                    if (overflowExtents[i].xdrNumABlks > 0) {
                        if (blocksToFree >= overflowExtents[i].xdrNumABlks) {
                            /* Free entire extent */
                            err = Alloc_Free(vcb, overflowExtents[i].xdrStABN,
                                           overflowExtents[i].xdrNumABlks);
                            if (err != noErr) {
                                FS_LOG_ERROR("Ext_Truncate: failed to free overflow extent\n");
                                return err;
                            }

                            FS_LOG_DEBUG("Ext_Truncate: freed overflow extent[%d]: %u blocks\n",
                                        i, overflowExtents[i].xdrNumABlks);

                            blocksToFree -= overflowExtents[i].xdrNumABlks;
                            overflowExtents[i].xdrStABN = 0;
                            overflowExtents[i].xdrNumABlks = 0;
                        } else {
                            /* Free partial extent */
                            UInt32 keepBlocks = overflowExtents[i].xdrNumABlks - blocksToFree;
                            err = Alloc_Free(vcb,
                                           overflowExtents[i].xdrStABN + keepBlocks,
                                           blocksToFree);
                            if (err != noErr) {
                                FS_LOG_ERROR("Ext_Truncate: failed to free partial overflow extent\n");
                                return err;
                            }

                            FS_LOG_DEBUG("Ext_Truncate: freed %u blocks from overflow extent[%d]\n",
                                        blocksToFree, i);

                            overflowExtents[i].xdrNumABlks = (UInt16)keepBlocks;
                            blocksToFree = 0;
                        }
                    }
                }

                /* Check if we should delete this overflow extent record entirely */
                Boolean allZero = true;
                for (i = 0; i < 3; i++) {
                    if (overflowExtents[i].xdrNumABlks > 0) {
                        allZero = false;
                        break;
                    }
                }

                if (allZero) {
                    /* Delete the entire overflow extent record */
                    err = Ext_DeleteOverflow(vcb, fcb->base.fcbFlNm, kDataFork, lastFoundFABN);
                    if (err != noErr) {
                        FS_LOG_ERROR("Ext_Truncate: failed to delete overflow extent record\n");
                        return err;
                    }
                    FS_LOG_DEBUG("Ext_Truncate: deleted overflow extent record at FABN=%u\n",
                                lastFoundFABN);
                }

                /* Move to previous overflow extent */
                overflowFABN = lastFoundFABN;
            } else {
                break;  /* No more overflow extents */
            }

            if (blocksToFree == 0) {
                break;
            }
        }
    }

    /* Free blocks from FCB extents if still needed (work backwards) */
    for (i = 2; i >= 0; i--) {
        if (fcb->base.fcbExtRec.extent[i].blockCount > 0) {
            if (blocksToFree >= fcb->base.fcbExtRec.extent[i].blockCount) {
                /* Free entire extent */
                err = Alloc_Free(vcb, fcb->base.fcbExtRec.extent[i].startBlock,
                               fcb->base.fcbExtRec.extent[i].blockCount);
                if (err != noErr) {
                    FS_LOG_ERROR("Ext_Truncate: failed to free extent[%d]\n", i);
                    return err;
                }

                FS_LOG_DEBUG("Ext_Truncate: freed entire extent[%d]: %u blocks\n",
                            i, fcb->base.fcbExtRec.extent[i].blockCount);

                blocksToFree -= fcb->base.fcbExtRec.extent[i].blockCount;
                fcb->base.fcbExtRec.extent[i].startBlock = 0;
                fcb->base.fcbExtRec.extent[i].blockCount = 0;
            } else {
                /* Free partial extent */
                UInt32 keepBlocks = fcb->base.fcbExtRec.extent[i].blockCount - blocksToFree;
                err = Alloc_Free(vcb,
                               fcb->base.fcbExtRec.extent[i].startBlock + keepBlocks,
                               blocksToFree);
                if (err != noErr) {
                    FS_LOG_ERROR("Ext_Truncate: failed to free partial extent[%d]\n", i);
                    return err;
                }

                FS_LOG_DEBUG("Ext_Truncate: freed %u blocks from extent[%d]\n",
                            blocksToFree, i);

                fcb->base.fcbExtRec.extent[i].blockCount = (UInt16)keepBlocks;
                blocksToFree = 0;
            }

            if (blocksToFree == 0) {
                break;
            }
        }
    }

    /* Update FCB */
    fcb->base.fcbPLen = neededBlocks * vcb->base.vcbAlBlkSiz;
    fcb->base.fcbFlags |= FCB_DIRTY;

    FS_LOG_DEBUG("Ext_Truncate: truncated from %u to %u blocks (pLen=%u)\n",
                currentBlocks, neededBlocks, fcb->base.fcbPLen);

    return noErr;
}

/* ============================================================================
 * Extent Overflow B-tree Operations
 * ============================================================================ */

/**
 * Search extent overflow B-tree for extent record
 */
OSErr Ext_SearchOverflow(VCB* vcb, UInt32 fileID, UInt8 forkType, UInt32 startFABN,
                        ExtDataRec* extents)
{
    ExtentKey key;
    ExtDataRec record;
    UInt16 recordSize;
    OSErr err;

    if (!vcb || !extents) {
        return paramErr;
    }

    /* Build extent key */
    memset(&key, 0, sizeof(key));
    key.xkrKeyLen = sizeof(ExtentKey) - 1;  /* Don't count key length byte */
    key.xkrFkType = forkType;
    key.xkrFNum = fileID;
    key.xkrFABN = (UInt16)startFABN;

    /* Search extents B-tree */
    recordSize = sizeof(ExtDataRec);
    err = BTree_Search((BTCB*)(uintptr_t)vcb->base.vcbXTRef, &key, &record, &recordSize, NULL);
    if (err != noErr) {
        FS_LOG_DEBUG("Ext_SearchOverflow: not found for fileID=%u FABN=%u\n",
                    fileID, startFABN);
        return err;
    }

    /* Copy extent record */
    memcpy(extents, &record, sizeof(ExtDataRec));

    FS_LOG_DEBUG("Ext_SearchOverflow: found extent for fileID=%u FABN=%u\n",
                fileID, startFABN);
    return noErr;
}

/**
 * Add extent record to overflow B-tree
 */
OSErr Ext_AddOverflow(VCB* vcb, UInt32 fileID, UInt8 forkType, UInt32 startFABN,
                     const ExtDataRec* extents)
{
    ExtentKey key;
    OSErr err;

    if (!vcb || !extents) {
        return paramErr;
    }

    FS_LockVolume(vcb);

    /* Build extent key */
    memset(&key, 0, sizeof(key));
    key.xkrKeyLen = sizeof(ExtentKey) - 1;  /* Don't count key length byte */
    key.xkrFkType = forkType;
    key.xkrFNum = fileID;
    key.xkrFABN = (UInt16)startFABN;

    /* Insert into extents B-tree */
    err = BTree_Insert((BTCB*)(uintptr_t)vcb->base.vcbXTRef, &key, extents, sizeof(ExtDataRec));
    if (err != noErr) {
        FS_LOG_ERROR("Ext_AddOverflow: insert failed for fileID=%u FABN=%u: %d\n",
                    fileID, startFABN, err);
        FS_UnlockVolume(vcb);
        return err;
    }

    /* Mark volume as dirty */
    vcb->base.vcbFlags |= VCB_DIRTY;

    FS_UnlockVolume(vcb);

    FS_LOG_DEBUG("Ext_AddOverflow: added extent for fileID=%u FABN=%u\n",
                fileID, startFABN);
    return noErr;
}

/**
 * Delete extent record from overflow B-tree
 */
OSErr Ext_DeleteOverflow(VCB* vcb, UInt32 fileID, UInt8 forkType, UInt32 startFABN)
{
    ExtentKey key;
    OSErr err;

    if (!vcb) {
        return paramErr;
    }

    FS_LockVolume(vcb);

    /* Build extent key */
    memset(&key, 0, sizeof(key));
    key.xkrKeyLen = sizeof(ExtentKey) - 1;  /* Don't count key length byte */
    key.xkrFkType = forkType;
    key.xkrFNum = fileID;
    key.xkrFABN = (UInt16)startFABN;

    /* Delete from extents B-tree */
    err = BTree_Delete((BTCB*)(uintptr_t)vcb->base.vcbXTRef, &key);
    if (err != noErr) {
        FS_LOG_ERROR("Ext_DeleteOverflow: delete failed for fileID=%u FABN=%u: %d\n",
                    fileID, startFABN, err);
        FS_UnlockVolume(vcb);
        return err;
    }

    /* Mark volume as dirty */
    vcb->base.vcbFlags |= VCB_DIRTY;

    FS_UnlockVolume(vcb);

    FS_LOG_DEBUG("Ext_DeleteOverflow: deleted extent for fileID=%u FABN=%u\n",
                fileID, startFABN);
    return noErr;
}

/* ============================================================================
 * Allocation Bitmap Management
 * ============================================================================ */

/* Bitmap bit manipulation helpers */
#define BITS_PER_BYTE 8

static Boolean TestBit(UInt8* bitmap, UInt32 bitNum) {
    UInt32 byteNum = bitNum / BITS_PER_BYTE;
    UInt32 bitPos = bitNum % BITS_PER_BYTE;
    return (bitmap[byteNum] & (1 << (7 - bitPos))) != 0;
}

static void SetBit(UInt8* bitmap, UInt32 bitNum) {
    UInt32 byteNum = bitNum / BITS_PER_BYTE;
    UInt32 bitPos = bitNum % BITS_PER_BYTE;
    bitmap[byteNum] |= (1 << (7 - bitPos));
}

static void ClearBit(UInt8* bitmap, UInt32 bitNum) {
    UInt32 byteNum = bitNum / BITS_PER_BYTE;
    UInt32 bitPos = bitNum % BITS_PER_BYTE;
    bitmap[byteNum] &= ~(1 << (7 - bitPos));
}

/* Find a run of free blocks in the bitmap */
static UInt32 FindFreeRun(UInt8* bitmap, UInt32 totalBlocks,
                          UInt32 startHint, UInt32 minBlocks) {
    UInt32 start = startHint;
    UInt32 count = 0;
    UInt32 firstFree = 0;
    Boolean wrapped = false;

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

OSErr Alloc_Init(VCB* vcb) {
    UInt32 bitmapBytes;
    UInt32 bitmapBlocks;
    OSErr err;

    if (!vcb) {
        return paramErr;
    }

    /* Calculate bitmap size */
    bitmapBytes = (vcb->base.vcbNmAlBlks + 7) / 8;  /* Round up to byte boundary */
    bitmapBlocks = (bitmapBytes + 511) / 512;  /* 512 bytes per block */

    /* Allocate bitmap cache */
    vcb->base.vcbMAdr = NewPtrClear((Size)bitmapBytes);
    if (!vcb->base.vcbMAdr) {
        return memFullErr;
    }

    /* Read bitmap from disk */
    err = IO_ReadBlocks(vcb, (UInt32)vcb->base.vcbVBMSt, bitmapBlocks, vcb->base.vcbMAdr);
    if (err != noErr) {
        DisposePtr(vcb->base.vcbMAdr);
        vcb->base.vcbMAdr = NULL;
        return err;
    }

    FS_LOG_DEBUG("Alloc_Init: loaded %u blocks of bitmap (%u bytes)\n",
                 bitmapBlocks, bitmapBytes);
    return noErr;
}

OSErr Alloc_Close(VCB* vcb) {
    if (!vcb) {
        return paramErr;
    }

    /* Free bitmap cache */
    if (vcb->base.vcbMAdr) {
        DisposePtr(vcb->base.vcbMAdr);
        vcb->base.vcbMAdr = NULL;
    }

    FS_LOG_DEBUG("Alloc_Close: freed bitmap cache\n");
    return noErr;
}

OSErr Alloc_Blocks(VCB* vcb, UInt32 startHint, UInt32 minBlocks, UInt32 maxBlocks,
                   UInt32* actualStart, UInt32* actualCount) {
    UInt8* bitmap;
    UInt32 foundStart;
    UInt32 foundCount;
    UInt32 i;
    UInt32 bitmapBytes;
    UInt32 bitmapBlocks;
    OSErr err;

    if (!vcb || !actualStart || !actualCount) {
        return paramErr;
    }

    if (minBlocks == 0 || minBlocks > maxBlocks) {
        return paramErr;
    }

    FS_LockVolume(vcb);

    /* Ensure bitmap is loaded */
    if (!vcb->base.vcbMAdr) {
        FS_UnlockVolume(vcb);
        return ioErr;
    }

    bitmap = (UInt8*)vcb->base.vcbMAdr;

    /* Check if enough free blocks available */
    if (vcb->base.vcbFreeBks < minBlocks) {
        FS_UnlockVolume(vcb);
        return dskFulErr;
    }

    /* Use allocation pointer as hint if no hint provided */
    if (startHint == 0) {
        startHint = (UInt32)vcb->base.vcbAllocPtr;
    }

    /* Find a run of free blocks */
    foundStart = FindFreeRun(bitmap, (UInt32)vcb->base.vcbNmAlBlks, startHint, minBlocks);
    if (foundStart == 0xFFFFFFFF) {
        /* Try again from beginning */
        foundStart = FindFreeRun(bitmap, (UInt32)vcb->base.vcbNmAlBlks, 0, minBlocks);
        if (foundStart == 0xFFFFFFFF) {
            FS_UnlockVolume(vcb);
            return dskFulErr;
        }
    }

    /* Count available blocks in the run */
    foundCount = 0;
    for (i = foundStart; i < (UInt32)vcb->base.vcbNmAlBlks && foundCount < maxBlocks; i++) {
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
    vcb->base.vcbFreeBks -= (UInt16)foundCount;
    vcb->base.vcbAllocPtr = (SInt16)(foundStart + foundCount);
    if (vcb->base.vcbAllocPtr >= vcb->base.vcbNmAlBlks) {
        vcb->base.vcbAllocPtr = 0;
    }
    vcb->base.vcbFlags |= VCB_DIRTY;

    /* Write updated bitmap back to disk */
    bitmapBytes = (vcb->base.vcbNmAlBlks + 7) / 8;
    bitmapBlocks = (bitmapBytes + 511) / 512;
    err = IO_WriteBlocks(vcb, (UInt32)vcb->base.vcbVBMSt, bitmapBlocks, bitmap);
    if (err != noErr) {
        FS_LOG_ERROR("Alloc_Blocks: failed to write bitmap: %d\n", err);
        FS_UnlockVolume(vcb);
        return err;
    }

    *actualStart = foundStart;
    *actualCount = foundCount;

    FS_LOG_DEBUG("Alloc_Blocks: allocated %u blocks at %u (free=%u)\n",
                 foundCount, foundStart, vcb->base.vcbFreeBks);

    FS_UnlockVolume(vcb);
    return noErr;
}

OSErr Alloc_Free(VCB* vcb, UInt32 startBlock, UInt32 blockCount) {
    UInt8* bitmap;
    UInt32 i;
    UInt32 bitmapBytes;
    UInt32 bitmapBlocks;
    OSErr err;

    if (!vcb || blockCount == 0) {
        return paramErr;
    }

    /* Check bounds */
    if (startBlock + blockCount > (UInt32)vcb->base.vcbNmAlBlks) {
        return paramErr;
    }

    FS_LockVolume(vcb);

    /* Ensure bitmap is loaded */
    if (!vcb->base.vcbMAdr) {
        FS_UnlockVolume(vcb);
        return ioErr;
    }

    bitmap = (UInt8*)vcb->base.vcbMAdr;

    /* Mark blocks as free */
    for (i = 0; i < blockCount; i++) {
        if (TestBit(bitmap, startBlock + i)) {
            ClearBit(bitmap, startBlock + i);
            vcb->base.vcbFreeBks++;
        }
    }

    /* Update allocation pointer to freed area for next search */
    if (startBlock < (UInt32)vcb->base.vcbAllocPtr) {
        vcb->base.vcbAllocPtr = (SInt16)startBlock;
    }

    vcb->base.vcbFlags |= VCB_DIRTY;

    /* Write updated bitmap back to disk */
    bitmapBytes = (vcb->base.vcbNmAlBlks + 7) / 8;
    bitmapBlocks = (bitmapBytes + 511) / 512;
    err = IO_WriteBlocks(vcb, (UInt32)vcb->base.vcbVBMSt, bitmapBlocks, bitmap);
    if (err != noErr) {
        FS_LOG_ERROR("Alloc_Free: failed to write bitmap: %d\n", err);
        FS_UnlockVolume(vcb);
        return err;
    }

    FS_LOG_DEBUG("Alloc_Free: freed %u blocks at %u (free=%u)\n",
                 blockCount, startBlock, vcb->base.vcbFreeBks);

    FS_UnlockVolume(vcb);
    return noErr;
}

UInt32 Alloc_CountFree(VCB* vcb) {
    FS_LOG_DEBUG("Alloc_CountFree stub\n");
    return 0;
}

Boolean Alloc_Check(VCB* vcb, UInt32 startBlock, UInt32 blockCount) {
    FS_LOG_DEBUG("Alloc_Check stub\n");
    return false;
}

/* Cache Management */
OSErr Cache_Init(UInt32 cacheSize) {
    FS_LOG_DEBUG("Cache_Init stub: size=%d\n", cacheSize);
    return noErr;
}

void Cache_Shutdown(void) {
    FS_LOG_DEBUG("Cache_Shutdown stub\n");
}

OSErr Cache_GetBlock(VCB* vcb, UInt32 blockNum, CacheBuffer** buffer) {
    FS_LOG_DEBUG("Cache_GetBlock stub: blockNum=%d\n", blockNum);
    return ioErr;
}

OSErr Cache_ReleaseBlock(CacheBuffer* buffer, Boolean dirty) {
    FS_LOG_DEBUG("Cache_ReleaseBlock stub\n");
    return noErr;
}

OSErr Cache_FlushVolume(VCB* vcb) {
    /* Since IO_WriteFork writes directly without caching,
     * we just need to ensure filesystem metadata is synced.
     * For now, this is a no-op as writes go straight to disk. */
    FS_LOG_DEBUG("Cache_FlushVolume: volume flushed\n");
    (void)vcb;
    return noErr;
}

OSErr Cache_FlushAll(void) {
    /* Flush all volumes - currently a no-op since writes are direct */
    FS_LOG_DEBUG("Cache_FlushAll: all volumes flushed\n");
    return noErr;
}

void Cache_Invalidate(VCB* vcb) {
    FS_LOG_DEBUG("Cache_Invalidate stub\n");
}

/* I/O Operations */
OSErr IO_ReadBlocks(VCB* vcb, UInt32 startBlock, UInt32 blockCount, void* buffer) {
    FS_LOG_DEBUG("IO_ReadBlocks stub: start=%d, count=%d\n", startBlock, blockCount);
    return ioErr;
}

OSErr IO_WriteBlocks(VCB* vcb, UInt32 startBlock, UInt32 blockCount, const void* buffer) {
    FS_LOG_DEBUG("IO_WriteBlocks stub: start=%d, count=%d\n", startBlock, blockCount);
    return ioErr;
}

/* I/O Operations */

OSErr IO_ReadFork(FCB* fcb, UInt32 offset, UInt32 count, void* buffer, UInt32* actual) {
    VCB* vcb;
    VCBExt* vcbExt;
    UInt32 fileBlock;
    UInt32 physBlock;
    UInt32 blockOffset;
    UInt32 contiguous;
    UInt32 toRead;
    UInt32 totalRead = 0;
    UInt8* dst;
    UInt8* blockBuffer = NULL;
    OSErr err;
    UInt64 diskOffset;

    if (!fcb || !buffer || !actual) {
        return paramErr;
    }

    *actual = 0;
    vcb = (VCB*)fcb->base.fcbVPtr;
    if (!vcb) {
        return rfNumErr;
    }

    vcbExt = (VCBExt*)vcb;  /* Cast to extended VCB for device access */

    /* Check if offset is beyond EOF */
    if (offset >= fcb->base.fcbEOF) {
        return eofErr;
    }

    /* Limit read to EOF */
    if (offset + count > fcb->base.fcbEOF) {
        count = fcb->base.fcbEOF - offset;
    }

    /* Check if we have device read capability */
    if (!g_PlatformHooks.DeviceRead) {
        FS_LOG_DEBUG("IO_ReadFork: DeviceRead not available\n");
        return ioErr;
    }

    dst = (UInt8*)buffer;

    /* Read loop: handle partial blocks */
    while (count > 0) {
        /* Calculate file block and offset within block */
        fileBlock = offset / vcb->base.vcbAlBlkSiz;
        blockOffset = offset % vcb->base.vcbAlBlkSiz;

        /* Map file block to physical block using extents */
        err = Ext_Map(vcb, fcb, fileBlock, &physBlock, &contiguous);
        if (err != noErr) {
            FS_LOG_DEBUG("IO_ReadFork: Ext_Map failed for fileBlock %u\n", fileBlock);
            if (totalRead > 0) {
                *actual = totalRead;
                return noErr;  /* Partial read */
            }
            return err;
        }

        /* Calculate amount to read from this block */
        toRead = vcb->base.vcbAlBlkSiz - blockOffset;
        if (toRead > count) {
            toRead = count;
        }

        /* Calculate physical disk offset */
        diskOffset = (UInt64)(vcb->base.vcbAlBlSt + physBlock) * vcb->base.vcbAlBlkSiz + blockOffset;

        /* Check if we need to read partial block */
        if (blockOffset != 0 || toRead < vcb->base.vcbAlBlkSiz) {
            /* Partial block read - read full block then copy subset */
            blockBuffer = (UInt8*)NewPtr(vcb->base.vcbAlBlkSiz);
            if (!blockBuffer) {
                if (totalRead > 0) {
                    *actual = totalRead;
                    return noErr;  /* Partial read */
                }
                return memFullErr;
            }

            /* Read full block */
            UInt64 blockDiskOffset = (UInt64)(vcb->base.vcbAlBlSt + physBlock) * vcb->base.vcbAlBlkSiz;
            err = g_PlatformHooks.DeviceRead(vcbExt->vcbDevice, blockDiskOffset,
                                             vcb->base.vcbAlBlkSiz, blockBuffer);
            if (err != noErr) {
                DisposePtr((Ptr)blockBuffer);
                if (totalRead > 0) {
                    *actual = totalRead;
                    return noErr;  /* Partial read */
                }
                return err;
            }

            /* Copy the subset we need */
            memcpy(dst, blockBuffer + blockOffset, toRead);

            DisposePtr((Ptr)blockBuffer);
            blockBuffer = NULL;
        } else {
            /* Full block read - read directly */
            err = g_PlatformHooks.DeviceRead(vcbExt->vcbDevice, diskOffset, toRead, dst);
            if (err != noErr) {
                if (totalRead > 0) {
                    *actual = totalRead;
                    return noErr;  /* Partial read */
                }
                return err;
            }
        }

        /* Update counters */
        dst += toRead;
        offset += toRead;
        count -= toRead;
        totalRead += toRead;

        FS_LOG_DEBUG("IO_ReadFork: read %u bytes from physBlock %u\n", toRead, physBlock);
    }

    /* Update file position */
    fcb->fcbCrPs = offset;

    *actual = totalRead;

    FS_LOG_DEBUG("IO_ReadFork: successfully read %u bytes\n", totalRead);
    return noErr;
}

OSErr IO_WriteFork(FCB* fcb, UInt32 offset, UInt32 count, const void* buffer, UInt32* actual) {
    VCB* vcb;
    VCBExt* vcbExt;
    UInt32 fileBlock;
    UInt32 physBlock;
    UInt32 blockOffset;
    UInt32 contiguous;
    UInt32 toWrite;
    UInt32 totalWritten = 0;
    const UInt8* src;
    UInt8* blockBuffer = NULL;
    OSErr err;
    UInt64 diskOffset;

    if (!fcb || !buffer || !actual) {
        return paramErr;
    }

    *actual = 0;
    vcb = (VCB*)fcb->base.fcbVPtr;
    if (!vcb) {
        return rfNumErr;
    }

    vcbExt = (VCBExt*)vcb;  /* Cast to extended VCB for device access */

    /* Check write permission */
    if (!(fcb->base.fcbFlags & FCB_WRITE_PERM)) {
        return wrPermErr;
    }

    /* Check if offset + count exceeds physical length and extend if needed */
    if (offset + count > fcb->fcbPLen) {
        UInt32 newSize = offset + count;
        err = Ext_Extend(vcb, fcb, newSize);
        if (err != noErr) {
            FS_LOG_ERROR("IO_WriteFork: Ext_Extend failed for size %u\n", newSize);
            if (offset >= fcb->fcbPLen) {
                /* Can't extend and offset is beyond EOF */
                return err;
            }
            /* Partial write possible up to current extent */
            count = fcb->fcbPLen - offset;
        }
    }

    /* Check if we have device write capability */
    if (!g_PlatformHooks.DeviceWrite) {
        FS_LOG_DEBUG("IO_WriteFork: DeviceWrite not available\n");
        return ioErr;
    }

    src = (const UInt8*)buffer;

    /* Write loop: handle partial blocks with read-modify-write */
    while (count > 0) {
        /* Calculate file block and offset within block */
        fileBlock = offset / vcb->base.vcbAlBlkSiz;
        blockOffset = offset % vcb->base.vcbAlBlkSiz;

        /* Map file block to physical block using extents */
        err = Ext_Map(vcb, fcb, fileBlock, &physBlock, &contiguous);
        if (err != noErr) {
            FS_LOG_DEBUG("IO_WriteFork: Ext_Map failed for fileBlock %u\n", fileBlock);
            if (totalWritten > 0) {
                *actual = totalWritten;
                fcb->base.fcbFlags |= FCB_DIRTY;
                return noErr;  /* Partial write */
            }
            return err;
        }

        /* Calculate amount to write to this block */
        toWrite = vcb->base.vcbAlBlkSiz - blockOffset;
        if (toWrite > count) {
            toWrite = count;
        }

        /* Calculate physical disk offset */
        diskOffset = (UInt64)(vcb->base.vcbAlBlSt + physBlock) * vcb->base.vcbAlBlkSiz + blockOffset;

        /* Check if we need read-modify-write for partial block */
        if (blockOffset != 0 || toWrite < vcb->base.vcbAlBlkSiz) {
            /* Partial block write - need read-modify-write */
            blockBuffer = (UInt8*)NewPtr(vcb->base.vcbAlBlkSiz);
            if (!blockBuffer) {
                if (totalWritten > 0) {
                    *actual = totalWritten;
                    fcb->base.fcbFlags |= FCB_DIRTY;
                    return noErr;  /* Partial write */
                }
                return memFullErr;
            }

            /* Read existing block */
            UInt64 blockDiskOffset = (UInt64)(vcb->base.vcbAlBlSt + physBlock) * vcb->base.vcbAlBlkSiz;
            if (g_PlatformHooks.DeviceRead) {
                err = g_PlatformHooks.DeviceRead(vcbExt->vcbDevice, blockDiskOffset,
                                                 vcb->base.vcbAlBlkSiz, blockBuffer);
                if (err != noErr) {
                    DisposePtr((Ptr)blockBuffer);
                    if (totalWritten > 0) {
                        *actual = totalWritten;
                        fcb->base.fcbFlags |= FCB_DIRTY;
                        return noErr;  /* Partial write */
                    }
                    return err;
                }
            } else {
                /* No DeviceRead - zero the buffer */
                memset(blockBuffer, 0, vcb->base.vcbAlBlkSiz);
            }

            /* Modify the block with new data */
            memcpy(blockBuffer + blockOffset, src, toWrite);

            /* Write the entire block back */
            err = g_PlatformHooks.DeviceWrite(vcbExt->vcbDevice, blockDiskOffset,
                                              vcb->base.vcbAlBlkSiz, blockBuffer);

            DisposePtr((Ptr)blockBuffer);
            blockBuffer = NULL;

            if (err != noErr) {
                if (totalWritten > 0) {
                    *actual = totalWritten;
                    fcb->base.fcbFlags |= FCB_DIRTY;
                    return noErr;  /* Partial write */
                }
                return err;
            }
        } else {
            /* Full block write - write directly */
            err = g_PlatformHooks.DeviceWrite(vcbExt->vcbDevice, diskOffset, toWrite, src);
            if (err != noErr) {
                if (totalWritten > 0) {
                    *actual = totalWritten;
                    fcb->base.fcbFlags |= FCB_DIRTY;
                    return noErr;  /* Partial write */
                }
                return err;
            }
        }

        /* Update counters */
        src += toWrite;
        offset += toWrite;
        count -= toWrite;
        totalWritten += toWrite;

        FS_LOG_DEBUG("IO_WriteFork: wrote %u bytes to physBlock %u\n", toWrite, physBlock);
    }

    /* Update file position and EOF if extended */
    if (fcb->fcbCrPs < offset) {
        fcb->fcbCrPs = offset;
    }
    if (offset > fcb->base.fcbEOF) {
        fcb->base.fcbEOF = offset;
    }

    /* Mark FCB as dirty */
    fcb->base.fcbFlags |= FCB_DIRTY;

    *actual = totalWritten;

    FS_LOG_DEBUG("IO_WriteFork: successfully wrote %u bytes\n", totalWritten);
    return noErr;
}

/* All locking functions are now defined in FileManager.c */

/* POSIX stubs for bare metal kernel */
int sched_yield(void) {
    /* In a bare metal kernel, just return success */
    return 0;
}

time_t time(time_t* t) {
    /* Return a fake timestamp */
    time_t fake_time = 0x60000000;  /* Some arbitrary value */
    if (t) *t = fake_time;
    return fake_time;
}