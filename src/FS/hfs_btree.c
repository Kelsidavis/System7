/* HFS B-Tree Implementation */
#include "../../include/FS/hfs_btree.h"
#include "../../include/FS/hfs_endian.h"
#include "../../include/MemoryMgr/MemoryManager.h"
#include <string.h>
#include "FS/FSLogging.h"

/* Serial debug output */

/* Read data from B-tree file using extents */
static bool read_btree_data(HFS_BTree* bt, uint32_t offset, void* buffer, uint32_t length) {
    if (!bt || !buffer) return false;

    FS_LOG_DEBUG("read_btree_data: offset=%d length=%d fileSize=%d vol=%p bd.data=%p\n",
                 (int)offset, (int)length, (int)bt->fileSize,
                 (void*)bt->vol, bt->vol->bd.data);

    uint32_t bytesRead = 0;
    uint32_t currentOffset = offset;

    /* Read from first 3 extents */
    for (int i = 0; i < 3 && bytesRead < length; i++) {
        if (bt->extents[i].blockCount == 0) {
            /* FS_LOG_DEBUG("read_btree_data: Extent %d has 0 blocks\n", i); */
            break;
        }
        FS_LOG_DEBUG("read_btree_data: Extent %d - startBlock=%u, blockCount=%u\n",
                     i, bt->extents[i].startBlock, bt->extents[i].blockCount);

        uint32_t extentBytes = bt->extents[i].blockCount * bt->vol->alBlkSize;

        if (currentOffset >= extentBytes) {
            currentOffset -= extentBytes;
            continue;
        }

        uint32_t startBlock = bt->extents[i].startBlock;
        uint32_t blockOffset = currentOffset / bt->vol->alBlkSize;
        uint32_t byteOffset = currentOffset % bt->vol->alBlkSize;

        uint32_t toRead = extentBytes - currentOffset;
        if (toRead > (length - bytesRead)) {
            toRead = length - bytesRead;
        }

        /* Read the blocks */
        uint8_t* tempBuffer = NewPtr(bt->vol->alBlkSize * 2);
        if (!tempBuffer) return false;

        uint32_t blocksToRead = (toRead + byteOffset + bt->vol->alBlkSize - 1) / bt->vol->alBlkSize;
        if (!HFS_ReadAllocBlocks(bt->vol, startBlock + blockOffset, blocksToRead, tempBuffer)) {
            DisposePtr((Ptr)tempBuffer);
            return false;
        }

        memcpy((uint8_t*)buffer + bytesRead, tempBuffer + byteOffset, toRead);
        DisposePtr((Ptr)tempBuffer);

        bytesRead += toRead;
        currentOffset = 0;  /* Reset for next extent */
    }

    return bytesRead == length;
}

bool HFS_BT_Init(HFS_BTree* bt, HFS_Volume* vol, HFS_BTreeType type) {
    /* FS_LOG_DEBUG("HFS_BT_Init: ENTER (bt=%p, vol=%p, type=%d)\n", bt, vol, type); */

    if (!bt || !vol || !vol->mounted) {
        FS_LOG_DEBUG("HFS_BT_Init: Invalid params (bt=%p, vol=%p, mounted=%d)\n",
                     bt, vol, vol ? vol->mounted : 0);
        return false;
    }

    memset(bt, 0, sizeof(HFS_BTree));
    bt->vol = vol;
    bt->type = type;

    /* Set up extents and file size based on type */
    if (type == kBTreeCatalog) {
        bt->fileSize = vol->catFileSize;
        memcpy(bt->extents, vol->catExtents, sizeof(bt->extents));
        /* FS_LOG_DEBUG("HFS_BT_Init: Catalog tree, fileSize=%u\n", bt->fileSize); */
    } else if (type == kBTreeExtents) {
        bt->fileSize = vol->extFileSize;
        memcpy(bt->extents, vol->extExtents, sizeof(bt->extents));
        /* FS_LOG_DEBUG("HFS_BT_Init: Extents tree, fileSize=%u\n", bt->fileSize); */
    } else {
        /* FS_LOG_DEBUG("HFS_BT_Init: Unknown tree type %d\n", type); */
        return false;
    }

    /* Check if we have valid extents */
    if (bt->fileSize == 0 || bt->extents[0].blockCount == 0) {
        FS_LOG_DEBUG("HFS_BT_Init: No valid extents for B-tree (fileSize=%u, extent0.blocks=%u)\n",
                     bt->fileSize, bt->extents[0].blockCount);
        return false;
    }

    /* Read header node (node 0) */
    uint8_t headerNode[512];  /* Start with minimum size */
    /* FS_LOG_DEBUG("HFS_BT_Init: About to read header node from offset 0, size %u\n", sizeof(headerNode)); */
    if (!read_btree_data(bt, 0, headerNode, sizeof(headerNode))) {
        /* FS_LOG_DEBUG("HFS_BT_Init: Failed to read header node\n"); */
        return false;
    }
    /* FS_LOG_DEBUG("HFS_BT_Init: Successfully read header node\n"); */

    /* Parse node descriptor */
    HFS_BTNodeDesc* nodeDesc = (HFS_BTNodeDesc*)headerNode;
    FS_LOG_DEBUG("HFS_BT_Init: Node descriptor - kind=%d, numRecords=%u\n",
                 nodeDesc->kind, be16_read(&nodeDesc->numRecords));
    if (nodeDesc->kind != kBTHeaderNode) {
        FS_LOG_DEBUG("HFS BTree: Invalid header node kind %d (expected %d)\n",
                     nodeDesc->kind, kBTHeaderNode);
        return false;
    }

    /* Parse header record (starts after node descriptor) */
    HFS_BTHeaderRec* header = (HFS_BTHeaderRec*)(headerNode + sizeof(HFS_BTNodeDesc));

    bt->treeDepth   = be16_read(&header->depth);
    bt->rootNode    = be32_read(&header->rootNode);
    bt->firstLeaf   = be32_read(&header->firstLeafNode);
    bt->lastLeaf    = be32_read(&header->lastLeafNode);
    bt->nodeSize    = be16_read(&header->nodeSize);
    bt->totalNodes  = be32_read(&header->totalNodes);

    /* Allocate node buffer */
    bt->nodeBuffer = NewPtr(bt->nodeSize);
    if (!bt->nodeBuffer) {
        /* FS_LOG_DEBUG("HFS BTree: Failed to allocate node buffer\n"); */
        return false;
    }

    FS_LOG_DEBUG("HFS BTree: Initialized %s tree (nodeSize=%u, root=%u, depth=%u)\n",
                  type == kBTreeCatalog ? "Catalog" : "Extents",
                  bt->nodeSize, bt->rootNode, bt->treeDepth);

    return true;
}

void HFS_BT_Close(HFS_BTree* bt) {
    if (!bt) return;

    if (bt->nodeBuffer) {
        DisposePtr((Ptr)bt->nodeBuffer);
        bt->nodeBuffer = NULL;
    }

    memset(bt, 0, sizeof(HFS_BTree));
}

bool HFS_BT_ReadNode(HFS_BTree* bt, uint32_t nodeNum, void* buffer) {
    if (!bt || !buffer || nodeNum >= bt->totalNodes) return false;

    uint32_t offset = nodeNum * bt->nodeSize;
    return read_btree_data(bt, offset, buffer, bt->nodeSize);
}

bool HFS_BT_GetRecord(void* node, uint16_t nodeSize, uint16_t recordNum,
                      void** recordPtr, uint16_t* recordLen) {
    if (!node || !recordPtr) return false;

    HFS_BTNodeDesc* nodeDesc = (HFS_BTNodeDesc*)node;
    uint16_t numRecords = be16_read(&nodeDesc->numRecords);

    if (recordNum >= numRecords) return false;

    /* Record offsets are at the end of the node */
    uint16_t* offsets = (uint16_t*)((uint8_t*)node + nodeSize - 2);

    /* Offsets are stored backwards from the end */
    uint16_t offset = be16_read(&offsets[-(recordNum)]);

    /* DEBUG: Show offset details for first few records */
    if (recordNum < 3) {
        FS_LOG_DEBUG("HFS_BT_GetRecord: rec=%d offset=%d offsetAddr=%p\n",
                     recordNum, offset, (void*)&offsets[-(recordNum)]);
    }

    uint16_t nextOffset;

    if (recordNum + 1 < numRecords) {
        nextOffset = be16_read(&offsets[-(recordNum + 1)]);
    } else {
        /* Last record extends to the offset table */
        nextOffset = nodeSize - (numRecords + 1) * 2;
    }

    *recordPtr = (uint8_t*)node + offset;
    if (recordLen) {
        *recordLen = nextOffset - offset;
    }

    return true;
}

bool HFS_BT_IterateLeaves(HFS_BTree* bt, HFS_BT_IteratorFunc func, void* context) {
    if (!bt || !func) return false;

    uint32_t currentNode = bt->firstLeaf;
    void* nodeBuffer = NewPtr(bt->nodeSize);
    if (!nodeBuffer) return false;

    while (currentNode != 0) {
        /* Read the leaf node */
        if (!HFS_BT_ReadNode(bt, currentNode, nodeBuffer)) {
            DisposePtr((Ptr)nodeBuffer);
            return false;
        }

        HFS_BTNodeDesc* nodeDesc = (HFS_BTNodeDesc*)nodeBuffer;
        uint16_t numRecords = be16_read(&nodeDesc->numRecords);

        /* Process each record in the leaf */
        for (uint16_t i = 0; i < numRecords; i++) {
            void* record;
            uint16_t recordLen;

            if (!HFS_BT_GetRecord(nodeBuffer, bt->nodeSize, i, &record, &recordLen)) {
                continue;
            }

            /* For catalog records, split into key and data */
            if (bt->type == kBTreeCatalog) {
                HFS_CatKey* key = (HFS_CatKey*)record;
                uint8_t keyLen = key->keyLength;
                /* keyLength excludes the keyLength byte itself, so data starts at +1 + keyLen */
                void* data = (uint8_t*)record + 1 + keyLen;
                uint16_t dataLen = recordLen - 1 - keyLen;

                if (!func(key, keyLen, data, dataLen, context)) {
                    DisposePtr((Ptr)nodeBuffer);
                    return true;  /* Iterator requested stop */
                }
            }
        }

        /* Move to next leaf node */
        currentNode = be32_read(&nodeDesc->fLink);
    }

    DisposePtr((Ptr)nodeBuffer);
    return true;
}

int HFS_CompareCatalogKeys(const void* key1, const void* key2) {
    const HFS_CatKey* k1 = (const HFS_CatKey*)key1;
    const HFS_CatKey* k2 = (const HFS_CatKey*)key2;

    /* Compare parent IDs first */
    uint32_t pid1 = be32_read(&k1->parentID);
    uint32_t pid2 = be32_read(&k2->parentID);

    if (pid1 < pid2) return -1;
    if (pid1 > pid2) return 1;

    /* Same parent - compare names (case-insensitive) */
    uint8_t len1 = k1->nameLength;
    uint8_t len2 = k2->nameLength;
    uint8_t minLen = (len1 < len2) ? len1 : len2;

    for (uint8_t i = 0; i < minLen; i++) {
        /* Simple ASCII case-insensitive compare */
        uint8_t c1 = k1->name[i];
        uint8_t c2 = k2->name[i];

        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;

        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
    }

    /* Names match up to minLen - shorter name comes first */
    if (len1 < len2) return -1;
    if (len1 > len2) return 1;

    return 0;  /* Identical */
}

int HFS_CompareExtentsKeys(const void* key1, const void* key2) {
    /* Extents key: fileID + forkType + startBlock */
    const uint8_t* k1 = (const uint8_t*)key1;
    const uint8_t* k2 = (const uint8_t*)key2;

    /* Compare file ID (4 bytes) */
    uint32_t fid1 = be32_read(k1);
    uint32_t fid2 = be32_read(k2);

    if (fid1 < fid2) return -1;
    if (fid1 > fid2) return 1;

    /* Compare fork type (1 byte) */
    if (k1[4] < k2[4]) return -1;
    if (k1[4] > k2[4]) return 1;

    /* Compare start block (2 bytes) */
    uint16_t sb1 = be16_read(k1 + 5);
    uint16_t sb2 = be16_read(k2 + 5);

    if (sb1 < sb2) return -1;
    if (sb1 > sb2) return 1;

    return 0;
}