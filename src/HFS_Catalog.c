/*
 * HFS_Catalog.c - HFS Catalog B-tree Management Implementation
 *
 * This file implements catalog operations for the HFS file system,
 * including B-tree operations for file and directory management.
 *
 * Copyright (c) 2024 - Implementation for System 7.1 Portable
 * Based on Apple System Software 7.1 HFS catalog structure
 */

#include "FileManager.h"
#include "FileManager_Internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* External globals */
extern FSGlobals g_FSGlobals;

/* Catalog node ID counter */
static CNodeID g_nextCNodeID = 16;  /* Start after reserved IDs */

/* ============================================================================
 * B-tree Key Comparison
 * ============================================================================ */

/**
 * Compare two catalog keys
 * Returns: <0 if key1 < key2, 0 if equal, >0 if key1 > key2
 */
static int CatalogKeyCompare(const void* key1, const void* key2)
{
    const CatalogKey* ck1 = (const CatalogKey*)key1;
    const CatalogKey* ck2 = (const CatalogKey*)key2;
    int result;

    /* Compare parent directory IDs first */
    if (ck1->ckrParID < ck2->ckrParID) return -1;
    if (ck1->ckrParID > ck2->ckrParID) return 1;

    /* Compare names (case-insensitive) */
    uint8_t len1 = ck1->ckrCName[0];
    uint8_t len2 = ck2->ckrCName[0];
    uint8_t minLen = (len1 < len2) ? len1 : len2;

    for (int i = 1; i <= minLen; i++) {
        uint8_t c1 = ck1->ckrCName[i];
        uint8_t c2 = ck2->ckrCName[i];

        /* Convert to uppercase for comparison */
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;

        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
    }

    /* If names are equal up to minLen, shorter name comes first */
    if (len1 < len2) return -1;
    if (len1 > len2) return 1;

    return 0;  /* Keys are equal */
}

/* ============================================================================
 * Catalog Operations
 * ============================================================================ */

/**
 * Open the catalog B-tree
 */
OSErr Cat_Open(VCB* vcb)
{
    BTCB* btcb;
    OSErr err;

    if (!vcb) {
        return paramErr;
    }

    /* Check if already open */
    if (vcb->vcbCTRef) {
        return noErr;
    }

    /* Open the catalog B-tree */
    err = BTree_Open(vcb, CATALOG_FILE_ID, &btcb);
    if (err != noErr) {
        return err;
    }

    /* Set key comparison function */
    btcb->btcKeyCmp = CatalogKeyCompare;

    /* Store reference in VCB */
    vcb->vcbCTRef = btcb;

    return noErr;
}

/**
 * Close the catalog B-tree
 */
OSErr Cat_Close(VCB* vcb)
{
    if (!vcb || !vcb->vcbCTRef) {
        return paramErr;
    }

    /* Close the B-tree */
    BTree_Close((BTCB*)vcb->vcbCTRef);
    vcb->vcbCTRef = NULL;

    return noErr;
}

/**
 * Look up a catalog entry
 */
OSErr Cat_Lookup(VCB* vcb, uint32_t dirID, const uint8_t* name, void* catData, uint32_t* hint)
{
    CatalogKey key;
    uint16_t recordSize;
    OSErr err;

    if (!vcb || !name || !catData) {
        return paramErr;
    }

    /* Build catalog key */
    memset(&key, 0, sizeof(key));
    key.ckrKeyLen = sizeof(uint32_t) + 1 + name[0];
    key.ckrParID = dirID;
    memcpy(key.ckrCName, name, name[0] + 1);

    /* Search B-tree */
    recordSize = 512;  /* Max record size */
    err = BTree_Search((BTCB*)vcb->vcbCTRef, &key, catData, &recordSize, hint);

    return err;
}

/**
 * Create a new catalog entry
 */
OSErr Cat_Create(VCB* vcb, uint32_t dirID, const uint8_t* name, uint8_t type, void* catData)
{
    CatalogKey key;
    CatalogThreadRec thread;
    uint16_t recordSize;
    OSErr err;

    if (!vcb || !name || !catData) {
        return paramErr;
    }

    /* Build catalog key */
    memset(&key, 0, sizeof(key));
    key.ckrKeyLen = sizeof(uint32_t) + 1 + name[0];
    key.ckrParID = dirID;
    memcpy(key.ckrCName, name, name[0] + 1);

    /* Determine record size based on type */
    switch (type) {
        case REC_FIL:
            recordSize = sizeof(CatalogFileRec);
            break;
        case REC_FLDR:
            recordSize = sizeof(CatalogDirRec);
            break;
        default:
            return paramErr;
    }

    /* Insert into B-tree */
    err = BTree_Insert((BTCB*)vcb->vcbCTRef, &key, catData, recordSize);
    if (err != noErr) {
        return err;
    }

    /* Create thread record for directories and files */
    if (type == REC_FLDR || type == REC_FIL) {
        CNodeID cnid;

        /* Get CNode ID from record */
        if (type == REC_FLDR) {
            cnid = ((CatalogDirRec*)catData)->dirDirID;
        } else {
            cnid = ((CatalogFileRec*)catData)->filFlNum;
        }

        /* Build thread record */
        memset(&thread, 0, sizeof(thread));
        thread.cdrType = (type == REC_FLDR) ? REC_FLDR_THREAD : REC_FIL_THREAD;
        thread.thdParID = dirID;
        memcpy(thread.thdCName, name, name[0] + 1);

        /* Build thread key (using CNode ID as parent) */
        memset(&key, 0, sizeof(key));
        key.ckrKeyLen = sizeof(uint32_t) + 1;
        key.ckrParID = cnid;
        key.ckrCName[0] = 0;  /* Empty name for thread records */

        /* Insert thread record */
        err = BTree_Insert((BTCB*)vcb->vcbCTRef, &key, &thread, sizeof(thread));
    }

    return err;
}

/**
 * Delete a catalog entry
 */
OSErr Cat_Delete(VCB* vcb, uint32_t dirID, const uint8_t* name)
{
    CatalogKey key;
    CatalogKey threadKey;
    CatalogFileRec fileRec;
    CatalogDirRec dirRec;
    uint16_t recordSize;
    uint32_t hint = 0;
    CNodeID cnid = 0;
    uint8_t recType;
    OSErr err;

    if (!vcb || !name) {
        return paramErr;
    }

    /* Look up the entry first to get its CNode ID */
    err = Cat_Lookup(vcb, dirID, name, &fileRec, &hint);
    if (err != noErr) {
        return err;
    }

    /* Determine record type and get CNode ID */
    recType = *(uint8_t*)&fileRec;
    if (recType == REC_FIL) {
        cnid = fileRec.filFlNum;
    } else if (recType == REC_FLDR) {
        memcpy(&dirRec, &fileRec, sizeof(dirRec));
        cnid = dirRec.dirDirID;
    } else {
        return paramErr;
    }

    /* Build catalog key */
    memset(&key, 0, sizeof(key));
    key.ckrKeyLen = sizeof(uint32_t) + 1 + name[0];
    key.ckrParID = dirID;
    memcpy(key.ckrCName, name, name[0] + 1);

    /* Delete from B-tree */
    err = BTree_Delete((BTCB*)vcb->vcbCTRef, &key);
    if (err != noErr) {
        return err;
    }

    /* Delete thread record if it exists */
    if (cnid != 0) {
        memset(&threadKey, 0, sizeof(threadKey));
        threadKey.ckrKeyLen = sizeof(uint32_t) + 1;
        threadKey.ckrParID = cnid;
        threadKey.ckrCName[0] = 0;

        /* Try to delete thread (ignore errors) */
        BTree_Delete((BTCB*)vcb->vcbCTRef, &threadKey);
    }

    return noErr;
}

/**
 * Rename a catalog entry
 */
OSErr Cat_Rename(VCB* vcb, uint32_t dirID, const uint8_t* oldName, const uint8_t* newName)
{
    CatalogKey oldKey, newKey;
    CatalogFileRec fileRec;
    CatalogDirRec dirRec;
    CatalogThreadRec thread;
    uint16_t recordSize;
    uint32_t hint = 0;
    CNodeID cnid = 0;
    uint8_t recType;
    void* recPtr;
    OSErr err;

    if (!vcb || !oldName || !newName) {
        return paramErr;
    }

    /* Check if new name already exists */
    err = Cat_Lookup(vcb, dirID, newName, &fileRec, &hint);
    if (err == noErr) {
        return dupFNErr;
    }

    /* Look up the old entry */
    hint = 0;
    err = Cat_Lookup(vcb, dirID, oldName, &fileRec, &hint);
    if (err != noErr) {
        return err;
    }

    /* Determine record type */
    recType = *(uint8_t*)&fileRec;
    if (recType == REC_FIL) {
        cnid = fileRec.filFlNum;
        recPtr = &fileRec;
        recordSize = sizeof(CatalogFileRec);
    } else if (recType == REC_FLDR) {
        memcpy(&dirRec, &fileRec, sizeof(dirRec));
        cnid = dirRec.dirDirID;
        recPtr = &dirRec;
        recordSize = sizeof(CatalogDirRec);
    } else {
        return paramErr;
    }

    /* Build old key */
    memset(&oldKey, 0, sizeof(oldKey));
    oldKey.ckrKeyLen = sizeof(uint32_t) + 1 + oldName[0];
    oldKey.ckrParID = dirID;
    memcpy(oldKey.ckrCName, oldName, oldName[0] + 1);

    /* Build new key */
    memset(&newKey, 0, sizeof(newKey));
    newKey.ckrKeyLen = sizeof(uint32_t) + 1 + newName[0];
    newKey.ckrParID = dirID;
    memcpy(newKey.ckrCName, newName, newName[0] + 1);

    /* Delete old entry */
    err = BTree_Delete((BTCB*)vcb->vcbCTRef, &oldKey);
    if (err != noErr) {
        return err;
    }

    /* Insert with new name */
    err = BTree_Insert((BTCB*)vcb->vcbCTRef, &newKey, recPtr, recordSize);
    if (err != noErr) {
        /* Try to restore old entry */
        BTree_Insert((BTCB*)vcb->vcbCTRef, &oldKey, recPtr, recordSize);
        return err;
    }

    /* Update thread record if it exists */
    if (cnid != 0) {
        CatalogKey threadKey;

        /* Build thread key */
        memset(&threadKey, 0, sizeof(threadKey));
        threadKey.ckrKeyLen = sizeof(uint32_t) + 1;
        threadKey.ckrParID = cnid;
        threadKey.ckrCName[0] = 0;

        /* Try to read thread record */
        recordSize = sizeof(thread);
        err = BTree_Search((BTCB*)vcb->vcbCTRef, &threadKey, &thread, &recordSize, NULL);
        if (err == noErr) {
            /* Update thread with new name */
            memcpy(thread.thdCName, newName, newName[0] + 1);

            /* Delete old thread */
            BTree_Delete((BTCB*)vcb->vcbCTRef, &threadKey);

            /* Insert updated thread */
            BTree_Insert((BTCB*)vcb->vcbCTRef, &threadKey, &thread, sizeof(thread));
        }
    }

    return noErr;
}

/**
 * Move a catalog entry to a different directory
 */
OSErr Cat_Move(VCB* vcb, uint32_t srcDirID, const uint8_t* name, uint32_t dstDirID)
{
    CatalogKey srcKey, dstKey;
    CatalogFileRec fileRec;
    CatalogDirRec dirRec;
    CatalogThreadRec thread;
    uint16_t recordSize;
    uint32_t hint = 0;
    CNodeID cnid = 0;
    uint8_t recType;
    void* recPtr;
    OSErr err;

    if (!vcb || !name) {
        return paramErr;
    }

    /* Can't move to same directory */
    if (srcDirID == dstDirID) {
        return paramErr;
    }

    /* Check if name already exists in destination */
    err = Cat_Lookup(vcb, dstDirID, name, &fileRec, &hint);
    if (err == noErr) {
        return dupFNErr;
    }

    /* Look up the source entry */
    hint = 0;
    err = Cat_Lookup(vcb, srcDirID, name, &fileRec, &hint);
    if (err != noErr) {
        return err;
    }

    /* Determine record type */
    recType = *(uint8_t*)&fileRec;
    if (recType == REC_FIL) {
        cnid = fileRec.filFlNum;
        recPtr = &fileRec;
        recordSize = sizeof(CatalogFileRec);
    } else if (recType == REC_FLDR) {
        memcpy(&dirRec, &fileRec, sizeof(dirRec));
        cnid = dirRec.dirDirID;

        /* Check for circular move (can't move directory into itself or its children) */
        /* This would require traversing up the directory tree - simplified here */
        if (dstDirID == cnid) {
            return badMovErr;
        }

        recPtr = &dirRec;
        recordSize = sizeof(CatalogDirRec);
    } else {
        return paramErr;
    }

    /* Build source key */
    memset(&srcKey, 0, sizeof(srcKey));
    srcKey.ckrKeyLen = sizeof(uint32_t) + 1 + name[0];
    srcKey.ckrParID = srcDirID;
    memcpy(srcKey.ckrCName, name, name[0] + 1);

    /* Build destination key */
    memset(&dstKey, 0, sizeof(dstKey));
    dstKey.ckrKeyLen = sizeof(uint32_t) + 1 + name[0];
    dstKey.ckrParID = dstDirID;
    memcpy(dstKey.ckrCName, name, name[0] + 1);

    /* Delete from source directory */
    err = BTree_Delete((BTCB*)vcb->vcbCTRef, &srcKey);
    if (err != noErr) {
        return err;
    }

    /* Insert into destination directory */
    err = BTree_Insert((BTCB*)vcb->vcbCTRef, &dstKey, recPtr, recordSize);
    if (err != noErr) {
        /* Try to restore to source directory */
        BTree_Insert((BTCB*)vcb->vcbCTRef, &srcKey, recPtr, recordSize);
        return err;
    }

    /* Update thread record with new parent */
    if (cnid != 0) {
        CatalogKey threadKey;

        /* Build thread key */
        memset(&threadKey, 0, sizeof(threadKey));
        threadKey.ckrKeyLen = sizeof(uint32_t) + 1;
        threadKey.ckrParID = cnid;
        threadKey.ckrCName[0] = 0;

        /* Try to read thread record */
        recordSize = sizeof(thread);
        err = BTree_Search((BTCB*)vcb->vcbCTRef, &threadKey, &thread, &recordSize, NULL);
        if (err == noErr) {
            /* Update thread with new parent */
            thread.thdParID = dstDirID;

            /* Delete old thread */
            BTree_Delete((BTCB*)vcb->vcbCTRef, &threadKey);

            /* Insert updated thread */
            BTree_Insert((BTCB*)vcb->vcbCTRef, &threadKey, &thread, sizeof(thread));
        }
    }

    /* Update directory valences */
    /* Source directory: decrement valence */
    /* Destination directory: increment valence */
    /* This would require updating the directory records */

    return noErr;
}

/**
 * Get catalog information
 */
OSErr Cat_GetInfo(VCB* vcb, uint32_t dirID, const uint8_t* name, CInfoPBRec* pb)
{
    CatalogFileRec fileRec;
    CatalogDirRec dirRec;
    uint32_t hint = 0;
    uint8_t recType;
    OSErr err;

    if (!vcb || !pb) {
        return paramErr;
    }

    /* Handle index-based enumeration */
    if (pb->ioDirIndex > 0) {
        /* Enumerate by index - would need to iterate through B-tree */
        /* Simplified: not implemented in this version */
        return paramErr;
    }

    /* Handle lookup by name */
    if (name && name[0] > 0) {
        err = Cat_Lookup(vcb, dirID, name, &fileRec, &hint);
        if (err != noErr) {
            return err;
        }
    } else {
        /* Look up directory itself by ID */
        /* Would need to search by thread record */
        /* Simplified: return root directory info */
        if (dirID != 2) {
            return dirNFErr;
        }

        /* Fill in root directory info */
        pb->u.dirInfo.ioDrAttrib = kioFlAttribDir;
        pb->u.dirInfo.ioDrDirID = 2;
        pb->u.dirInfo.ioDrNmFls = vcb->vcbNmFls;
        pb->u.dirInfo.ioDrCrDat = vcb->vcbCrDate;
        pb->u.dirInfo.ioDrMdDat = vcb->vcbLsMod;
        pb->u.dirInfo.ioDrParID = 1;  /* Parent of root is 1 */

        return noErr;
    }

    /* Determine record type */
    recType = *(uint8_t*)&fileRec;

    if (recType == REC_FIL) {
        /* Fill in file information */
        pb->u.hFileInfo.ioFlAttrib = 0;
        pb->u.hFileInfo.ioFlFndrInfo = fileRec.filUsrWds;
        pb->u.hFileInfo.ioFlParID = dirID;
        pb->u.hFileInfo.ioFlStBlk = fileRec.filStBlk;
        pb->u.hFileInfo.ioFlLgLen = fileRec.filLgLen;
        pb->u.hFileInfo.ioFlPyLen = fileRec.filPyLen;
        pb->u.hFileInfo.ioFlRStBlk = fileRec.filRStBlk;
        pb->u.hFileInfo.ioFlRLgLen = fileRec.filRLgLen;
        pb->u.hFileInfo.ioFlRPyLen = fileRec.filRPyLen;
        pb->u.hFileInfo.ioFlCrDat = fileRec.filCrDat;
        pb->u.hFileInfo.ioFlMdDat = fileRec.filMdDat;
        pb->u.hFileInfo.ioFlBkDat = fileRec.filBkDat;
        pb->u.hFileInfo.ioFlXFndrInfo = fileRec.filFndrInfo;
        pb->u.hFileInfo.ioFlParID2 = fileRec.filFlNum;
        pb->u.hFileInfo.ioFlClpSiz = fileRec.filClpSize;
        memcpy(pb->u.hFileInfo.ioFlExtRec, fileRec.filExtRec, sizeof(ExtDataRec));
        memcpy(pb->u.hFileInfo.ioFlRExtRec, fileRec.filRExtRec, sizeof(ExtDataRec));

    } else if (recType == REC_FLDR) {
        memcpy(&dirRec, &fileRec, sizeof(dirRec));

        /* Fill in directory information */
        pb->u.dirInfo.ioDrAttrib = kioFlAttribDir;
        pb->u.dirInfo.ioDrDirID = dirRec.dirDirID;
        pb->u.dirInfo.ioDrNmFls = dirRec.dirVal;
        memcpy(pb->u.dirInfo.ioDrFndrInfo, &dirRec.dirUsrInfo, 16);
        pb->u.dirInfo.ioDrCrDat = dirRec.dirCrDat;
        pb->u.dirInfo.ioDrMdDat = dirRec.dirMdDat;
        pb->u.dirInfo.ioDrBkDat = dirRec.dirBkDat;
        memcpy(pb->u.dirInfo.ioDrXFndrInfo, &dirRec.dirFndrInfo, 16);
        pb->u.dirInfo.ioDrParID = dirID;

    } else {
        return paramErr;
    }

    return noErr;
}

/**
 * Set catalog information
 */
OSErr Cat_SetInfo(VCB* vcb, uint32_t dirID, const uint8_t* name, const CInfoPBRec* pb)
{
    CatalogKey key;
    CatalogFileRec fileRec;
    CatalogDirRec dirRec;
    uint16_t recordSize;
    uint32_t hint = 0;
    uint8_t recType;
    OSErr err;

    if (!vcb || !name || !pb) {
        return paramErr;
    }

    /* Look up the entry */
    err = Cat_Lookup(vcb, dirID, name, &fileRec, &hint);
    if (err != noErr) {
        return err;
    }

    /* Determine record type */
    recType = *(uint8_t*)&fileRec;

    /* Build catalog key */
    memset(&key, 0, sizeof(key));
    key.ckrKeyLen = sizeof(uint32_t) + 1 + name[0];
    key.ckrParID = dirID;
    memcpy(key.ckrCName, name, name[0] + 1);

    if (recType == REC_FIL) {
        /* Update file information */
        fileRec.filUsrWds = pb->u.hFileInfo.ioFlFndrInfo;
        fileRec.filCrDat = pb->u.hFileInfo.ioFlCrDat;
        fileRec.filMdDat = pb->u.hFileInfo.ioFlMdDat;
        fileRec.filBkDat = pb->u.hFileInfo.ioFlBkDat;
        fileRec.filFndrInfo = pb->u.hFileInfo.ioFlXFndrInfo;

        /* Delete old record */
        err = BTree_Delete((BTCB*)vcb->vcbCTRef, &key);
        if (err != noErr) {
            return err;
        }

        /* Insert updated record */
        err = BTree_Insert((BTCB*)vcb->vcbCTRef, &key, &fileRec, sizeof(fileRec));

    } else if (recType == REC_FLDR) {
        memcpy(&dirRec, &fileRec, sizeof(dirRec));

        /* Update directory information */
        memcpy(&dirRec.dirUsrInfo, pb->u.dirInfo.ioDrFndrInfo, 16);
        dirRec.dirCrDat = pb->u.dirInfo.ioDrCrDat;
        dirRec.dirMdDat = pb->u.dirInfo.ioDrMdDat;
        dirRec.dirBkDat = pb->u.dirInfo.ioDrBkDat;
        memcpy(&dirRec.dirFndrInfo, pb->u.dirInfo.ioDrXFndrInfo, 16);

        /* Delete old record */
        err = BTree_Delete((BTCB*)vcb->vcbCTRef, &key);
        if (err != noErr) {
            return err;
        }

        /* Insert updated record */
        err = BTree_Insert((BTCB*)vcb->vcbCTRef, &key, &dirRec, sizeof(dirRec));

    } else {
        return paramErr;
    }

    return err;
}

/**
 * Get next available catalog node ID
 */
CNodeID Cat_GetNextID(VCB* vcb)
{
    CNodeID cnid;

    if (!vcb) {
        return 0;
    }

    FS_LockVolume(vcb);

    /* Get and increment next CNode ID */
    cnid = vcb->vcbNxtCNID++;

    /* Mark volume as dirty */
    vcb->vcbFlags |= VCB_DIRTY;

    FS_UnlockVolume(vcb);

    return cnid;
}

/* ============================================================================
 * B-tree Operations (Simplified Implementation)
 * ============================================================================ */

/**
 * Open a B-tree file
 */
OSErr BTree_Open(VCB* vcb, uint32_t fileID, BTCB** btcb)
{
    BTCB* newBTCB;
    BTHeaderRec header;
    CacheBuffer* buffer;
    OSErr err;

    if (!vcb || !btcb) {
        return paramErr;
    }

    *btcb = NULL;

    /* Allocate BTCB */
    newBTCB = (BTCB*)calloc(1, sizeof(BTCB));
    if (!newBTCB) {
        return memFullErr;
    }

    /* Initialize mutex */
#ifdef _WIN32
    InitializeCriticalSection(&newBTCB->btcMutex);
#else
    pthread_mutex_init(&newBTCB->btcMutex, NULL);
#endif

    /* Set file reference */
    newBTCB->btcRefNum = (FileRefNum)fileID;

    /* Read header node (simplified - would read from actual B-tree file) */
    /* For now, initialize with defaults */
    newBTCB->btcDepth = 1;
    newBTCB->btcRoot = 1;
    newBTCB->btcNRecs = 0;
    newBTCB->btcFNode = 2;
    newBTCB->btcLNode = 2;
    newBTCB->btcNodeSize = BTREE_NODE_SIZE;
    newBTCB->btcKeyLen = BTREE_MAX_KEY_LEN;
    newBTCB->btcNNodes = 100;
    newBTCB->btcFree = 50;

    /* Allocate cache for nodes */
    newBTCB->btcCache = calloc(10, BTREE_NODE_SIZE);
    if (!newBTCB->btcCache) {
        free(newBTCB);
        return memFullErr;
    }

    *btcb = newBTCB;

    return noErr;
}

/**
 * Close a B-tree
 */
OSErr BTree_Close(BTCB* btcb)
{
    if (!btcb) {
        return paramErr;
    }

    /* Free cache */
    if (btcb->btcCache) {
        free(btcb->btcCache);
    }

    /* Destroy mutex */
#ifdef _WIN32
    DeleteCriticalSection(&btcb->btcMutex);
#else
    pthread_mutex_destroy(&btcb->btcMutex);
#endif

    free(btcb);

    return noErr;
}

/**
 * Search for a record in the B-tree
 */
OSErr BTree_Search(BTCB* btcb, const void* key, void* record, uint16_t* recordSize, uint32_t* hint)
{
    /* Simplified implementation */
    /* In a real implementation, this would:
     * 1. Navigate from root to leaf following key comparisons
     * 2. Search within leaf node for exact key match
     * 3. Return record data if found
     */

    if (!btcb || !key || !record || !recordSize) {
        return paramErr;
    }

    /* For now, return not found */
    return btRecNotFnd;
}

/**
 * Insert a record into the B-tree
 */
OSErr BTree_Insert(BTCB* btcb, const void* key, const void* record, uint16_t recordSize)
{
    /* Simplified implementation */
    /* In a real implementation, this would:
     * 1. Search for insertion point
     * 2. Insert into leaf node
     * 3. Split nodes if necessary
     * 4. Update parent nodes
     */

    if (!btcb || !key || !record || recordSize == 0) {
        return paramErr;
    }

    /* Update record count */
    btcb->btcNRecs++;

    /* Mark as dirty */
    btcb->btcFlags |= BTCDirty;

    return noErr;
}

/**
 * Delete a record from the B-tree
 */
OSErr BTree_Delete(BTCB* btcb, const void* key)
{
    /* Simplified implementation */
    /* In a real implementation, this would:
     * 1. Search for the key
     * 2. Delete from leaf node
     * 3. Merge/redistribute nodes if necessary
     * 4. Update parent nodes
     */

    if (!btcb || !key) {
        return paramErr;
    }

    /* Update record count */
    if (btcb->btcNRecs > 0) {
        btcb->btcNRecs--;
    }

    /* Mark as dirty */
    btcb->btcFlags |= BTCDirty;

    return noErr;
}

/**
 * Get a B-tree node
 */
OSErr BTree_GetNode(BTCB* btcb, uint32_t nodeNum, void** nodePtr)
{
    /* Simplified implementation */
    /* Would read node from disk cache */

    if (!btcb || !nodePtr) {
        return paramErr;
    }

    if (nodeNum >= btcb->btcNNodes) {
        return btbadNode;
    }

    /* Return pointer to cached node */
    *nodePtr = (uint8_t*)btcb->btcCache + (nodeNum * BTREE_NODE_SIZE);

    return noErr;
}

/**
 * Release a B-tree node
 */
OSErr BTree_ReleaseNode(BTCB* btcb, uint32_t nodeNum)
{
    /* Simplified implementation */
    /* Would release node from cache */

    if (!btcb) {
        return paramErr;
    }

    if (nodeNum >= btcb->btcNNodes) {
        return btbadNode;
    }

    return noErr;
}

/**
 * Flush a B-tree node to disk
 */
OSErr BTree_FlushNode(BTCB* btcb, uint32_t nodeNum)
{
    /* Simplified implementation */
    /* Would write node to disk */

    if (!btcb) {
        return paramErr;
    }

    if (nodeNum >= btcb->btcNNodes) {
        return btbadNode;
    }

    /* Clear dirty flag */
    btcb->btcFlags &= ~BTCDirty;

    return noErr;
}