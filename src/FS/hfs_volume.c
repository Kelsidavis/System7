#include "MemoryMgr/MemoryManager.h"
/* HFS Volume Management Implementation */
#include "../../include/FS/hfs_volume.h"
#include "../../include/FS/hfs_endian.h"
#include "../../include/FS/hfs_btree.h"
#include <string.h>
#include <stdlib.h>
#include "FS/FSLogging.h"
#include "System71StdLib.h"

/* Serial debug output */

/* Pascal string to C string conversion */
static void pstr_to_cstr(char* dst, const uint8_t* src, size_t maxlen) {
    uint8_t len = src[0];
    if (len > maxlen - 1) len = maxlen - 1;
    memcpy(dst, src + 1, len);
    dst[len] = '\0';
}

/*
 * Read a master directory block into vol->mdb and the values the volume
 * keeps at hand. False if it is not HFS or its allocation block size is not
 * a power of two a volume could have.
 *
 * The three mount paths each used to parse the MDB themselves, all with the
 * same offsets - two bytes too far from drCrDate on, which the formatters
 * here also wrote. So the volumes this code made mounted, and no HFS disk
 * made anywhere else ever did.
 */
bool HFS_VolumeFromMDB(HFS_Volume* vol, const uint8_t* b) {
    if (!vol || !b) return false;
    if (be16_read(&b[kMDB_drSigWord]) != HFS_SIGNATURE) return false;

    HFS_MDB* mdb = &vol->mdb;
    mdb->drSigWord   = HFS_SIGNATURE;
    mdb->drCrDate    = be32_read(&b[kMDB_drCrDate]);
    mdb->drLsMod     = be32_read(&b[kMDB_drLsMod]);
    mdb->drAtrb      = be16_read(&b[kMDB_drAtrb]);
    mdb->drNmFls     = be16_read(&b[kMDB_drNmFls]);
    mdb->drVBMSt     = be16_read(&b[kMDB_drVBMSt]);
    mdb->drAllocPtr  = be16_read(&b[kMDB_drAllocPtr]);
    mdb->drNmAlBlks  = be16_read(&b[kMDB_drNmAlBlks]);
    mdb->drAlBlkSiz  = be32_read(&b[kMDB_drAlBlkSiz]);
    mdb->drClpSiz    = be32_read(&b[kMDB_drClpSiz]);
    mdb->drAlBlSt    = be16_read(&b[kMDB_drAlBlSt]);
    mdb->drNxtCNID   = be32_read(&b[kMDB_drNxtCNID]);
    mdb->drFreeBks   = be16_read(&b[kMDB_drFreeBks]);
    memcpy(mdb->drVN, &b[kMDB_drVN], sizeof(mdb->drVN));
    if (mdb->drVN[0] > 27) mdb->drVN[0] = 27;
    mdb->drVolBkUp   = be32_read(&b[kMDB_drVolBkUp]);
    mdb->drVSeqNum   = be16_read(&b[kMDB_drVSeqNum]);
    mdb->drWrCnt     = be32_read(&b[kMDB_drWrCnt]);
    mdb->drXTClpSiz  = be32_read(&b[kMDB_drXTClpSiz]);
    mdb->drCTClpSiz  = be32_read(&b[kMDB_drCTClpSiz]);
    mdb->drNmRtDirs  = be16_read(&b[kMDB_drNmRtDirs]);
    mdb->drFilCnt    = be32_read(&b[kMDB_drFilCnt]);
    mdb->drDirCnt    = be32_read(&b[kMDB_drDirCnt]);
    for (int i = 0; i < 8; i++) {
        mdb->drFndrInfo[i] = be32_read(&b[kMDB_drFndrInfo + i * 4]);
    }
    mdb->drXTFlSize  = be32_read(&b[kMDB_drXTFlSize]);
    mdb->drCTFlSize  = be32_read(&b[kMDB_drCTFlSize]);
    for (int i = 0; i < 3; i++) {
        mdb->drXTExtRec[i].startBlock = be16_read(&b[kMDB_drXTExtRec + i * 4]);
        mdb->drXTExtRec[i].blockCount = be16_read(&b[kMDB_drXTExtRec + i * 4 + 2]);
        mdb->drCTExtRec[i].startBlock = be16_read(&b[kMDB_drCTExtRec + i * 4]);
        mdb->drCTExtRec[i].blockCount = be16_read(&b[kMDB_drCTExtRec + i * 4 + 2]);
    }

    /* A multiple of 512, and a power of two; HFS allows up to 2^16 blocks. */
    uint32_t bs = mdb->drAlBlkSiz;
    if (bs < 512 || (bs & (bs - 1)) != 0) {
        FS_LOG_WARN("HFS: allocation block size %lu is not one a volume can have\n", (unsigned long)bs);
        return false;
    }

    vol->alBlkSize   = mdb->drAlBlkSiz;
    vol->alBlSt      = mdb->drAlBlSt;
    vol->vbmStart    = mdb->drVBMSt;
    vol->numAlBlks   = mdb->drNmAlBlks;
    vol->catFileSize = mdb->drCTFlSize;
    memcpy(vol->catExtents, mdb->drCTExtRec, sizeof(vol->catExtents));
    vol->extFileSize = mdb->drXTFlSize;
    memcpy(vol->extExtents, mdb->drXTExtRec, sizeof(vol->extExtents));
    vol->rootDirID   = 2;
    vol->nextCNID    = mdb->drNxtCNID;
    pstr_to_cstr(vol->volName, mdb->drVN, sizeof(vol->volName));
    return true;
}

bool HFS_VolumeMountMemory(HFS_Volume* vol, void* buffer, uint64_t size, VRefNum vRefNum) {
    FS_LOG_DEBUG("HFS: VolumeMountMemory: ENTRY vol=%p buffer=%p size=%ld\n",
                 (void*)vol, buffer, (long)size);

    if (!vol || !buffer || size < 1024 * 1024) {
        /* FS_LOG_DEBUG("HFS: Invalid parameters for mount\n"); */
        return false;  /* Minimum 1MB */
    }

    FS_LOG_DEBUG("HFS: VolumeMountMemory: About to memset vol structure\n");
    memset(vol, 0, sizeof(HFS_Volume));

    /* Initialize block device from memory */
    FS_LOG_DEBUG("HFS: VolumeMountMemory: calling HFS_BD_InitMemory with vol=%p buffer=%p\n",
                 (void*)vol, buffer);
    if (!HFS_BD_InitMemory(&vol->bd, buffer, size)) {
        /* FS_LOG_DEBUG("HFS: Failed to init block device\n"); */
        return false;
    }
    FS_LOG_DEBUG("HFS: VolumeMountMemory: After BD_InitMemory, vol->bd.data=%p\n",
                 vol->bd.data);

    /* Try to read existing MDB */
    uint8_t mdbBuffer[512];
    if (HFS_BD_ReadSector(&vol->bd, HFS_MDB_SECTOR, mdbBuffer)) {
        if (HFS_VolumeFromMDB(vol, mdbBuffer)) {
            vol->vRefNum = vRefNum;
            vol->mounted = true;
            return true;
        }
    }

    /* FS_LOG_DEBUG("HFS: No valid MDB found, volume was not created properly\n"); */
    return false;
}

void HFS_VolumeUnmount(HFS_Volume* vol) {
    if (!vol) return;

    if (vol->mounted) {
        /* FS_LOG_DEBUG("HFS: Unmounting volume\n"); */
    }

    HFS_BD_Close(&vol->bd);
    memset(vol, 0, sizeof(HFS_Volume));
}

uint64_t HFS_AllocBlockToByteOffset(const HFS_Volume* vol, uint32_t allocBlock) {
    if (!vol || !vol->mounted) return 0;

    /* Calculate offset: allocation blocks start after system area */
    uint64_t systemAreaBytes = (uint64_t)vol->alBlSt * vol->alBlkSize;
    return systemAreaBytes + ((uint64_t)allocBlock * vol->alBlkSize);
}

bool HFS_ReadAllocBlocks(const HFS_Volume* vol, uint32_t startBlock, uint32_t blockCount,
                         void* buffer) {
    if (!vol || !vol->mounted || !buffer) return false;

    uint64_t offset = HFS_AllocBlockToByteOffset(vol, startBlock);
    uint32_t length = blockCount * vol->alBlkSize;

    return HFS_BD_Read(&vol->bd, offset, buffer, length);
}

bool HFS_GetVolumeInfo(const HFS_Volume* vol, VolumeControlBlock* vcb) {
    if (!vol || !vol->mounted || !vcb) return false;

    memset(vcb, 0, sizeof(VolumeControlBlock));
    strncpy(vcb->name, vol->volName, sizeof(vcb->name) - 1);
    vcb->vRefNum = vol->vRefNum;
    vcb->totalBytes = (uint64_t)vol->numAlBlks * vol->alBlkSize;
    vcb->freeBytes = (uint64_t)vol->mdb.drFreeBks * vol->alBlkSize;
    vcb->rootID = vol->rootDirID;
    vcb->mounted = vol->mounted;

    return true;
}

bool HFS_CreateBlankVolume(void* buffer, uint64_t size, const char* volName) {
    if (!buffer || size < 1024 * 1024) return false;  /* Minimum 1MB */

    /* Clear the buffer */
    memset(buffer, 0, size);

    /* Calculate volume parameters - use 32-bit to avoid libgcc dependency */
    uint32_t alBlkSize = 512;  /* Start with 512 byte allocation blocks */
    uint32_t size32 = (size > 0xFFFFFFFF) ? 0xFFFFFFFF : (uint32_t)size;
    uint32_t totalBlocks = size32 / alBlkSize;

    /* Adjust allocation block size for larger volumes */
    while (totalBlocks > 65535 && alBlkSize < 4096) {
        alBlkSize *= 2;
        totalBlocks = size32 / alBlkSize;
    }

    /* System area: boot blocks (2) + MDB (1) + VBM + reserved */
    uint16_t alBlSt = 16;  /* First allocation block for data */
    uint16_t numAlBlks = totalBlocks - alBlSt;
    uint16_t vbmStart = 3;  /* VBM starts at block 3 */
    uint16_t vbmBlocks = (numAlBlks + 4095) / 4096;  /* 1 bit per block */

    /* Create MDB at sector 2 */
    uint8_t mdb[512];
    memset(mdb, 0, sizeof(mdb));

    /* Fill MDB fields */
    be16_write(&mdb[kMDB_drSigWord], HFS_SIGNATURE);               /* drSigWord */
    be32_write(&mdb[kMDB_drCrDate], 0);                          /* drCrDate - will set later */
    be32_write(&mdb[kMDB_drLsMod], 0);                          /* drLsMod */
    be16_write(&mdb[kMDB_drAtrb], 0);                         /* drAtrb */
    be16_write(&mdb[kMDB_drNmFls], 0);                         /* drNmFls */
    be16_write(&mdb[kMDB_drVBMSt], vbmStart);                  /* drVBMSt */
    be16_write(&mdb[kMDB_drAllocPtr], 0);                         /* drAllocPtr */
    be16_write(&mdb[kMDB_drNmAlBlks], numAlBlks);                 /* drNmAlBlks */
    be32_write(&mdb[kMDB_drAlBlkSiz], alBlkSize);                 /* drAlBlkSiz */
    be32_write(&mdb[kMDB_drClpSiz], 4096);                      /* drClpSiz */
    be16_write(&mdb[kMDB_drAlBlSt], alBlSt);                    /* drAlBlSt */
    be32_write(&mdb[kMDB_drNxtCNID], 16);                        /* drNxtCNID */
    be16_write(&mdb[kMDB_drFreeBks], numAlBlks - 10);            /* drFreeBks (reserve some) */

    /* Volume name */
    uint8_t pname[28];
    memset(pname, 0, sizeof(pname));
    c2pstrcpy_bounded(pname, volName, 27);
    memcpy(&mdb[kMDB_drVN], pname, 28);                     /* drVN */

    /* Catalog file - allocate 10 blocks */
    be32_write(&mdb[kMDB_drCTFlSize], 10 * alBlkSize);           /* drCTFlSize */
    be16_write(&mdb[kMDB_drCTExtRec], 0);                        /* drCTExtRec[0].startBlock */
    be16_write(&mdb[kMDB_drCTExtRec + 2], 10);                       /* drCTExtRec[0].blockCount */

    /* Extents file - allocate 3 blocks */
    be32_write(&mdb[kMDB_drXTFlSize], 3 * alBlkSize);            /* drXTFlSize */
    be16_write(&mdb[kMDB_drXTExtRec], 10);                       /* drXTExtRec[0].startBlock */
    be16_write(&mdb[kMDB_drXTExtRec + 2], 3);                        /* drXTExtRec[0].blockCount */

    /* Write MDB */
    memcpy((uint8_t*)buffer + (HFS_MDB_SECTOR * 512), mdb, sizeof(mdb));

    /* Debug: verify MDB was written correctly */
    uint16_t check_sig = be16_read((uint8_t*)buffer + (HFS_MDB_SECTOR * 512));
    FS_LOG_DEBUG("HFS: Created blank volume, MDB signature at sector %d: 0x%04x\n",
                 HFS_MDB_SECTOR, check_sig);

    /* Initialize Volume Bitmap */
    uint8_t* vbm = (uint8_t*)buffer + (vbmStart * alBlkSize);
    memset(vbm, 0, vbmBlocks * alBlkSize);

    /* Mark system blocks as used (first 13 blocks) */
    for (int i = 0; i < 13; i++) {
        vbm[i / 8] |= (1 << (7 - (i % 8)));
    }

    /* Initialize Catalog B-tree */
    /* Catalog starts AFTER system area (at first allocation block) */
    enum { kCatalogNodeSize = 2048 };
    uint8_t* catData = (uint8_t*)buffer + (alBlSt * alBlkSize);  /* Catalog at first alloc block */
    memset(catData, 0, 10 * alBlkSize);  /* Clear all catalog blocks */

    /* Create catalog B-tree header node at first block of catalog file */
    HFS_BTNodeDesc* catNodeDesc = (HFS_BTNodeDesc*)catData;
    catNodeDesc->fLink = 0;
    catNodeDesc->bLink = 0;
    catNodeDesc->kind = kBTHeaderNode;
    catNodeDesc->height = 1;
    be16_write(&catNodeDesc->numRecords, 3);  /* Header has 3 records */
    catNodeDesc->reserved = 0;

    /* Create catalog B-tree header record */
    HFS_BTHeaderRec* catHeader = (HFS_BTHeaderRec*)(catData + sizeof(HFS_BTNodeDesc));
    be16_write(&catHeader->depth, 1);           /* One level - header + leaf */
    be32_write(&catHeader->rootNode, 1);        /* Root is first leaf node */
    be32_write(&catHeader->leafRecords, 0);     /* Set after the seed records are written */
    be32_write(&catHeader->firstLeafNode, 1);   /* First leaf at node 1 */
    be32_write(&catHeader->lastLeafNode, 1);    /* Last leaf at node 1 */
    be16_write(&catHeader->nodeSize, kCatalogNodeSize);
    be16_write(&catHeader->keyCompareType, 0); /* Case-insensitive compare */
    uint32_t catalogNodeCount = (10 * alBlkSize) / kCatalogNodeSize;
    be32_write(&catHeader->totalNodes, catalogNodeCount);
    be32_write(&catHeader->freeNodes, catalogNodeCount - 2); /* Header and leaf are used */

    /* Create the first leaf node after the B-tree header node. */
    uint8_t* leafNode = catData + kCatalogNodeSize;
    memset(leafNode, 0, kCatalogNodeSize);

    /* Leaf node descriptor */
    HFS_BTNodeDesc* leafDesc = (HFS_BTNodeDesc*)leafNode;
    leafDesc->fLink = 0;  /* No next leaf */
    leafDesc->bLink = 0;  /* No previous leaf */
    leafDesc->kind = kBTLeafNode;
    leafDesc->height = 1;
    be16_write(&leafDesc->numRecords, 0); /* Set after the seed records are written */
    leafDesc->reserved = 0;

    /* Build catalog records - write them sequentially after the node descriptor */
    uint8_t* recData = leafNode + sizeof(HFS_BTNodeDesc);
    uint8_t* offsetTableEnd = leafNode + kCatalogNodeSize - 2;
    uint16_t offset = sizeof(HFS_BTNodeDesc);
    int recNum = 0;
    int folderCount = 0;
    int fileCount = 0;

    #define CATALOG_RECORD_FITS(record_size) \
        ((uint32_t)offset + (record_size) + \
         ((uint32_t)recNum + 2u) * sizeof(uint16_t) <= kCatalogNodeSize)

    /* Every entry gets a real creation and modification date.
     *
     * These were all written as zero, so nothing on the volume had a date at
     * all - Get Info showed no "Created" line for any file, because the code
     * that draws it correctly declines to invent one. A synthesized volume
     * still comes into existence at a moment in time, and that is the honest
     * answer: the moment it was built. */
    extern void GetDateTime(uint32_t* secs);
    uint32_t buildTime = 0;
    GetDateTime(&buildTime);

    /* Helper function to add a folder record */
    #define ADD_FOLDER(parent, name_str, cnid, valence_count) do { \
        size_t name_len = strlen(name_str); \
        if (name_len > 31) name_len = 31; \
        uint16_t keySpan = (uint16_t)((1u + 6u + name_len + 1u) & ~1u); \
        uint16_t rec_size = keySpan + sizeof(HFS_CatFolderRec); \
        if (!CATALOG_RECORD_FITS(rec_size)) return false; \
        HFS_CatKey* key = (HFS_CatKey*)recData; \
        key->keyLength = 6 + name_len; \
        key->reserved = 0; \
        be32_write(&key->parentID, parent); \
        key->nameLength = name_len; \
        memcpy(key->name, name_str, name_len); \
        if (keySpan > 1u + key->keyLength) recData[1 + key->keyLength] = 0; \
        HFS_CatFolderRec* folder = (HFS_CatFolderRec*)(recData + keySpan); \
        be16_write(&folder->recordType, kHFS_FolderRecord); \
        be16_write(&folder->flags, 0); \
        be16_write(&folder->valence, valence_count); \
        be32_write(&folder->folderID, cnid); \
        be32_write(&folder->createDate, buildTime); \
        be32_write(&folder->modifyDate, buildTime); \
        be32_write(&folder->backupDate, 0); \
        memset(folder->userInfo, 0, 16); \
        memset(folder->finderInfo, 0, 16); \
        memset(folder->reserved, 0, 16); \
        be16_write(offsetTableEnd - (size_t)recNum * sizeof(uint16_t), offset); \
        recData += rec_size; \
        offset += rec_size; \
        recNum++; \
        folderCount++; \
    } while(0)

    /* Helper function to add a file record */
    #define ADD_FILE(parent, name_str, cnid, type_code, creator_code) do { \
        size_t name_len = strlen(name_str); \
        if (name_len > 31) name_len = 31; \
        uint16_t keySpan = (uint16_t)((1u + 6u + name_len + 1u) & ~1u); \
        uint16_t rec_size = keySpan + sizeof(HFS_CatFileRec); \
        if (!CATALOG_RECORD_FITS(rec_size)) return false; \
        HFS_CatKey* key = (HFS_CatKey*)recData; \
        key->keyLength = 6 + name_len; \
        key->reserved = 0; \
        be32_write(&key->parentID, parent); \
        key->nameLength = name_len; \
        memcpy(key->name, name_str, name_len); \
        if (keySpan > 1u + key->keyLength) recData[1 + key->keyLength] = 0; \
        HFS_CatFileRec* file = (HFS_CatFileRec*)(recData + keySpan); \
        be16_write(&file->recordType, kHFS_FileRecord); \
        file->flags = 0; \
        file->fileType = 0; \
        be32_write(&file->fileID, cnid); \
        be16_write(&file->dataStartBlock, 0); \
        be32_write(&file->dataLogicalSize, 0); \
        be32_write(&file->dataPhysicalSize, 0); \
        be16_write(&file->rsrcStartBlock, 0); \
        be32_write(&file->rsrcLogicalSize, 0); \
        be32_write(&file->rsrcPhysicalSize, 0); \
        be32_write(&file->createDate, buildTime); \
        be32_write(&file->modifyDate, buildTime); \
        be32_write(&file->backupDate, 0); \
        memset(file->userInfo, 0, 16); \
        memset(file->finderInfo, 0, 16); \
        be32_write(&file->userInfo[0], type_code); \
        be32_write(&file->userInfo[4], creator_code); \
        be16_write(&file->clumpSize, 0); \
        memset(file->dataExtents, 0, sizeof(file->dataExtents)); \
        memset(file->rsrcExtents, 0, sizeof(file->rsrcExtents)); \
        be32_write(&file->reserved, 0); \
        be16_write(offsetTableEnd - (size_t)recNum * sizeof(uint16_t), offset); \
        recData += rec_size; \
        offset += rec_size; \
        recNum++; \
        fileCount++; \
    } while(0)

    /* Add initial folders and files */
    ADD_FOLDER(2, "System Folder", 16, 0);
    ADD_FOLDER(2, "Documents", 17, 2);
    ADD_FOLDER(2, "Applications", 18, 3);
    ADD_FILE(2, "Read Me", 19, 0x54455854, 0x74747874);  /* 'TEXT', 'ttxt' */
    ADD_FILE(2, "About This Mac", 20, 0x54455854, 0x74747874);  /* 'TEXT', 'ttxt' */
    ADD_FILE(17, "Sample Document", 21, 0x54455854, 0x74747874);  /* 'TEXT', 'ttxt' */
    ADD_FILE(17, "Notes", 22, 0x54455854, 0x74747874);  /* 'TEXT', 'ttxt' */
    ADD_FILE(18, "SimpleText", 23, 0x4150504C, 0x74747874);  /* 'APPL', 'ttxt' */
    ADD_FILE(18, "TextEdit", 24, 0x4150504C, 0x74656474);   /* 'APPL', 'tedt' */
    ADD_FILE(18, "MacPaint", 25, 0x4150504C, 0x4D415050);   /* 'APPL', 'MAPP' */

    be32_write(&catHeader->leafRecords, recNum);
    be16_write(&leafDesc->numRecords, (uint16_t)recNum);

    #undef ADD_FOLDER
    #undef ADD_FILE
    #undef CATALOG_RECORD_FITS

    /* Update MDB to reflect created folders and files */
    be32_write(&mdb[kMDB_drNxtCNID], 26);  /* drNxtCNID - next available is 26 */
    be32_write(&mdb[kMDB_drDirCnt], folderCount);
    be32_write(&mdb[kMDB_drFilCnt], fileCount);

    /* Initialize Extents B-tree */
    /* Extents start at allocation block 10 (after catalog's 10 blocks) */
    uint8_t* extData = (uint8_t*)buffer + ((alBlSt + 10) * alBlkSize);  /* Extents after catalog */
    memset(extData, 0, 3 * alBlkSize);  /* Clear all extent blocks */

    /* Create extents B-tree header node */
    HFS_BTNodeDesc* extNodeDesc = (HFS_BTNodeDesc*)extData;
    extNodeDesc->fLink = 0;
    extNodeDesc->bLink = 0;
    extNodeDesc->kind = kBTHeaderNode;
    extNodeDesc->height = 1;
    be16_write(&extNodeDesc->numRecords, 3);
    extNodeDesc->reserved = 0;

    /* Create extents B-tree header record */
    HFS_BTHeaderRec* extHeader = (HFS_BTHeaderRec*)(extData + sizeof(HFS_BTNodeDesc));
    be16_write(&extHeader->depth, 0);           /* Empty tree */
    be32_write(&extHeader->rootNode, 0);        /* No root yet */
    be32_write(&extHeader->leafRecords, 0);
    be32_write(&extHeader->firstLeafNode, 0);
    be32_write(&extHeader->lastLeafNode, 0);
    be16_write(&extHeader->nodeSize, 512);      /* 512-byte nodes */
    be16_write(&extHeader->keyCompareType, 0);  /* Binary compare */
    be32_write(&extHeader->totalNodes, 6);      /* 3 blocks * 512 / 512 */
    be32_write(&extHeader->freeNodes, 5);       /* All but header free */

    /* FS_LOG_DEBUG("HFS: Created blank volume (%u MB) with B-trees\n", (uint32_t)(size / 1024 / 1024)); */
    return true;
}

/*
 * HFS_FormatVolume - Format a block device with HFS filesystem
 * Writes MDB, volume bitmap, catalog B-tree, and extents B-tree
 */
bool HFS_FormatVolume(HFS_BlockDev* bd, const char* volName) {
    if (!bd || !volName) return false;

    /* Calculate volume size from block device */
    uint64_t size = bd->size;
    /* size/1024/1024 is uint32_t: %u would pass a 4-byte int to printf. */
    FS_LOG_DEBUG("HFS: Formatting volume '%s' (size=%lu MB)\n",
                  volName, (unsigned long)(size / 1024 / 1024));

    /* Calculate volume parameters */
    uint32_t alBlkSize = 512;  /* Start with 512 byte allocation blocks */
    uint32_t size32 = (size > 0xFFFFFFFF) ? 0xFFFFFFFF : (uint32_t)size;
    uint32_t totalBlocks = size32 / alBlkSize;

    /* Adjust allocation block size for larger volumes */
    while (totalBlocks > 65535 && alBlkSize < 4096) {
        alBlkSize *= 2;
        totalBlocks = size32 / alBlkSize;
    }

    /* System area parameters */
    uint16_t alBlSt = 16;  /* First allocation block for data */
    uint16_t numAlBlks = totalBlocks - alBlSt;
    uint16_t vbmStart = 3;  /* VBM starts at block 3 */
    uint16_t vbmBlocks = (numAlBlks + 4095) / 4096;  /* 1 bit per block */

    /* Allocate temporary buffer for sectors */
    uint8_t* sectorBuf = (uint8_t*)NewPtr(alBlkSize);
    if (!sectorBuf) {
        FS_LOG_DEBUG("HFS: Failed to allocate sector buffer\n");
        return false;
    }

    /* Write boot blocks (sectors 0-1) - all zeros for now */
    memset(sectorBuf, 0, 512);
    if (!HFS_BD_WriteSector(bd, 0, sectorBuf) || !HFS_BD_WriteSector(bd, 1, sectorBuf)) {
        FS_LOG_DEBUG("HFS: Failed to write boot blocks\n");
        DisposePtr((Ptr)sectorBuf);
        return false;
    }

    /* Create and write MDB (sector 2) */
    uint8_t mdb[512];
    memset(mdb, 0, sizeof(mdb));

    be16_write(&mdb[kMDB_drSigWord], HFS_SIGNATURE);               /* drSigWord */
    be32_write(&mdb[kMDB_drCrDate], 0);                          /* drCrDate */
    be32_write(&mdb[kMDB_drLsMod], 0);                          /* drLsMod */
    be16_write(&mdb[kMDB_drAtrb], 0);                         /* drAtrb */
    be16_write(&mdb[kMDB_drNmFls], 0);                         /* drNmFls */
    be16_write(&mdb[kMDB_drVBMSt], vbmStart);                  /* drVBMSt */
    be16_write(&mdb[kMDB_drAllocPtr], 0);                         /* drAllocPtr */
    be16_write(&mdb[kMDB_drNmAlBlks], numAlBlks);                 /* drNmAlBlks */
    be32_write(&mdb[kMDB_drAlBlkSiz], alBlkSize);                 /* drAlBlkSiz */
    be32_write(&mdb[kMDB_drClpSiz], 4096);                      /* drClpSiz */
    be16_write(&mdb[kMDB_drAlBlSt], alBlSt);                    /* drAlBlSt */
    be32_write(&mdb[kMDB_drNxtCNID], 16);                        /* drNxtCNID */
    be16_write(&mdb[kMDB_drFreeBks], numAlBlks - 13);            /* drFreeBks */

    /* Volume name */
    uint8_t pname[28];
    memset(pname, 0, sizeof(pname));
    c2pstrcpy_bounded(pname, volName, 27);
    memcpy(&mdb[kMDB_drVN], pname, 28);                     /* drVN */

    /* Catalog file - 10 allocation blocks */
    be32_write(&mdb[kMDB_drCTFlSize], 10 * alBlkSize);           /* drCTFlSize */
    be16_write(&mdb[kMDB_drCTExtRec], 0);                        /* drCTExtRec[0].startBlock */
    be16_write(&mdb[kMDB_drCTExtRec + 2], 10);                       /* drCTExtRec[0].blockCount */

    /* Extents file - 3 allocation blocks */
    be32_write(&mdb[kMDB_drXTFlSize], 3 * alBlkSize);            /* drXTFlSize */
    be16_write(&mdb[kMDB_drXTExtRec], 10);                       /* drXTExtRec[0].startBlock */
    be16_write(&mdb[kMDB_drXTExtRec + 2], 3);                        /* drXTExtRec[0].blockCount */

    /* Directories and files count (will be 0 initially, populated later) */
    be32_write(&mdb[kMDB_drDirCnt], 0);                         /* drDirCnt */
    be32_write(&mdb[kMDB_drFilCnt], 0);                         /* drFilCnt */

    if (!HFS_BD_WriteSector(bd, HFS_MDB_SECTOR, mdb)) {
        FS_LOG_DEBUG("HFS: Failed to write MDB\n");
        DisposePtr((Ptr)sectorBuf);
        return false;
    }

    FS_LOG_DEBUG("HFS: Wrote MDB at sector %d\n", HFS_MDB_SECTOR);

    /* Write volume bitmap - clear all bits (all blocks free) */
    memset(sectorBuf, 0, alBlkSize);
    /* Mark system blocks as used (first 13 allocation blocks) */
    sectorBuf[0] = 0xFF;  /* Blocks 0-7 used */
    sectorBuf[1] = 0xF8;  /* Blocks 8-12 used (bits 7-3) */

    for (uint32_t i = 0; i < vbmBlocks; i++) {
        uint32_t sector = vbmStart * (alBlkSize / 512) + i * (alBlkSize / 512);
        for (uint32_t j = 0; j < alBlkSize / 512; j++) {
            if (!HFS_BD_WriteSector(bd, sector + j, sectorBuf + j * 512)) {
                FS_LOG_DEBUG("HFS: Failed to write VBM\n");
                DisposePtr((Ptr)sectorBuf);
                return false;
            }
        }
        /* After first block, all bits are zero */
        memset(sectorBuf, 0, alBlkSize);
    }

    FS_LOG_DEBUG("HFS: Wrote volume bitmap\n");

    /* Write catalog B-tree */
    memset(sectorBuf, 0, alBlkSize);

    /* Catalog B-tree header node (node 0) */
    HFS_BTNodeDesc* catNodeDesc = (HFS_BTNodeDesc*)sectorBuf;
    catNodeDesc->fLink = 0;
    catNodeDesc->bLink = 0;
    catNodeDesc->kind = kBTHeaderNode;
    catNodeDesc->height = 1;
    be16_write(&catNodeDesc->numRecords, 3);

    HFS_BTHeaderRec* catHeader = (HFS_BTHeaderRec*)(sectorBuf + sizeof(HFS_BTNodeDesc));
    be16_write(&catHeader->depth, 0);                /* Empty tree initially */
    be32_write(&catHeader->rootNode, 0);
    be32_write(&catHeader->leafRecords, 0);
    be32_write(&catHeader->firstLeafNode, 0);
    be32_write(&catHeader->lastLeafNode, 0);
    be16_write(&catHeader->nodeSize, 1024);
    be16_write(&catHeader->keyCompareType, 0);
    be32_write(&catHeader->totalNodes, 20);
    be32_write(&catHeader->freeNodes, 20);

    /* Write catalog header node */
    uint32_t catStart = alBlSt * (alBlkSize / 512);
    for (uint32_t i = 0; i < alBlkSize / 512; i++) {
        if (!HFS_BD_WriteSector(bd, catStart + i, sectorBuf + i * 512)) {
            FS_LOG_DEBUG("HFS: Failed to write catalog header\n");
            DisposePtr((Ptr)sectorBuf);
            return false;
        }
    }

    /* Write remaining catalog blocks as zeros */
    memset(sectorBuf, 0, alBlkSize);
    for (uint32_t block = 1; block < 10; block++) {
        for (uint32_t i = 0; i < alBlkSize / 512; i++) {
            if (!HFS_BD_WriteSector(bd, catStart + block * (alBlkSize / 512) + i, sectorBuf + i * 512)) {
                FS_LOG_DEBUG("HFS: Failed to write catalog blocks\n");
                DisposePtr((Ptr)sectorBuf);
                return false;
            }
        }
    }

    FS_LOG_DEBUG("HFS: Wrote catalog B-tree\n");

    /* Write extents B-tree */
    memset(sectorBuf, 0, alBlkSize);

    HFS_BTNodeDesc* extNodeDesc = (HFS_BTNodeDesc*)sectorBuf;
    extNodeDesc->fLink = 0;
    extNodeDesc->bLink = 0;
    extNodeDesc->kind = kBTHeaderNode;
    extNodeDesc->height = 1;
    be16_write(&extNodeDesc->numRecords, 3);

    HFS_BTHeaderRec* extHeader = (HFS_BTHeaderRec*)(sectorBuf + sizeof(HFS_BTNodeDesc));
    be16_write(&extHeader->depth, 0);
    be32_write(&extHeader->rootNode, 0);
    be32_write(&extHeader->leafRecords, 0);
    be32_write(&extHeader->firstLeafNode, 0);
    be32_write(&extHeader->lastLeafNode, 0);
    be16_write(&extHeader->nodeSize, 512);
    be16_write(&extHeader->keyCompareType, 0);
    be32_write(&extHeader->totalNodes, 6);
    be32_write(&extHeader->freeNodes, 6);

    /* Write extents header node */
    uint32_t extStart = (alBlSt + 10) * (alBlkSize / 512);
    for (uint32_t i = 0; i < alBlkSize / 512; i++) {
        if (!HFS_BD_WriteSector(bd, extStart + i, sectorBuf + i * 512)) {
            FS_LOG_DEBUG("HFS: Failed to write extents header\n");
            DisposePtr((Ptr)sectorBuf);
            return false;
        }
    }

    /* Write remaining extents blocks as zeros */
    memset(sectorBuf, 0, alBlkSize);
    for (uint32_t block = 1; block < 3; block++) {
        for (uint32_t i = 0; i < alBlkSize / 512; i++) {
            if (!HFS_BD_WriteSector(bd, extStart + block * (alBlkSize / 512) + i, sectorBuf + i * 512)) {
                FS_LOG_DEBUG("HFS: Failed to write extents blocks\n");
                DisposePtr((Ptr)sectorBuf);
                return false;
            }
        }
    }

    FS_LOG_DEBUG("HFS: Wrote extents B-tree\n");

    /* Flush cache */
    HFS_BD_Flush(bd);

    DisposePtr((Ptr)sectorBuf);
    FS_LOG_DEBUG("HFS: Format complete\n");
    return true;
}
