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

    FS_LOG_DEBUG("read_btree_data: offset=%d length=%d fileSize=%d vol=%08x bd.data=%08x\n",
                 (int)offset, (int)length, (int)bt->fileSize,
                 (unsigned int)bt->vol, (unsigned int)bt->vol->bd.data);

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
        uint8_t* tempBuffer = malloc(bt->vol->alBlkSize * 2);
        if (!tempBuffer) return false;

        uint32_t blocksToRead = (toRead + byteOffset + bt->vol->alBlkSize - 1) / bt->vol->alBlkSize;
        if (!HFS_ReadAllocBlocks(bt->vol, startBlock + blockOffset, blocksToRead, tempBuffer)) {
            free(tempBuffer);
            return false;
        }

        memcpy((uint8_t*)buffer + bytesRead, tempBuffer + byteOffset, toRead);
        free(tempBuffer);

        bytesRead += toRead;
        currentOffset = 0;  /* Reset for next extent */
    }

    return bytesRead == length;
}

/* Write data to B-tree file using extents */
static bool write_btree_data(HFS_BTree* bt, uint32_t offset, const void* buffer, uint32_t length) {
    if (!bt || !buffer) return false;

    FS_LOG_DEBUG("write_btree_data: offset=%d length=%d fileSize=%d\n",
                 (int)offset, (int)length, (int)bt->fileSize);

    uint32_t bytesWritten = 0;
    uint32_t currentOffset = offset;

    /* Write to first 3 extents */
    for (int i = 0; i < 3 && bytesWritten < length; i++) {
        if (bt->extents[i].blockCount == 0) {
            break;
        }

        uint32_t extentBytes = bt->extents[i].blockCount * bt->vol->alBlkSize;

        if (currentOffset >= extentBytes) {
            currentOffset -= extentBytes;
            continue;
        }

        uint32_t startBlock = bt->extents[i].startBlock;
        uint32_t blockOffset = currentOffset / bt->vol->alBlkSize;
        uint32_t byteOffset = currentOffset % bt->vol->alBlkSize;

        uint32_t toWrite = extentBytes - currentOffset;
        if (toWrite > (length - bytesWritten)) {
            toWrite = length - bytesWritten;
        }

        /* If not block-aligned, need read-modify-write */
        if (byteOffset != 0 || (toWrite % bt->vol->alBlkSize) != 0) {
            uint8_t* tempBuffer = malloc(bt->vol->alBlkSize * 2);
            if (!tempBuffer) return false;

            uint32_t blocksToAccess = (toWrite + byteOffset + bt->vol->alBlkSize - 1) / bt->vol->alBlkSize;

            /* Read existing blocks */
            if (!HFS_ReadAllocBlocks(bt->vol, startBlock + blockOffset, blocksToAccess, tempBuffer)) {
                free(tempBuffer);
                return false;
            }

            /* Modify with new data */
            memcpy(tempBuffer + byteOffset, (const uint8_t*)buffer + bytesWritten, toWrite);

            /* Write back */
            if (!HFS_WriteAllocBlocks(bt->vol, startBlock + blockOffset, blocksToAccess, tempBuffer)) {
                free(tempBuffer);
                return false;
            }

            free(tempBuffer);
        } else {
            /* Block-aligned write - direct */
            uint32_t blocksToWrite = toWrite / bt->vol->alBlkSize;
            if (!HFS_WriteAllocBlocks(bt->vol, startBlock + blockOffset, blocksToWrite,
                                     (const uint8_t*)buffer + bytesWritten)) {
                return false;
            }
        }

        bytesWritten += toWrite;
        currentOffset = 0;  /* Reset for next extent */
    }

    return bytesWritten == length;
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

    /* For the header node, the header record always starts at offset 14 */
    /* (immediately after the 14-byte node descriptor) */
    HFS_BTHeaderRec* header = (HFS_BTHeaderRec*)(headerNode + sizeof(HFS_BTNodeDesc));

    /* Debug: dump first 32 bytes of header record */
    FS_LOG_DEBUG("HFS_BT_Init: Header bytes: ");
    for (int i = 0; i < 32 && i < sizeof(HFS_BTHeaderRec); i++) {
        FS_LOG_DEBUG("%02x ", ((uint8_t*)header)[i]);
    }
    FS_LOG_DEBUG("\n");

    bt->treeDepth   = be16_read(&header->depth);
    bt->rootNode    = be32_read(&header->rootNode);
    bt->firstLeaf   = be32_read(&header->firstLeafNode);
    bt->lastLeaf    = be32_read(&header->lastLeafNode);
    bt->nodeSize    = be16_read(&header->nodeSize);
    bt->totalNodes  = be32_read(&header->totalNodes);

    FS_LOG_DEBUG("HFS_BT_Init: Read header - depth=%d root=%d firstLeaf=%d lastLeaf=%d nodeSize=%d totalNodes=%d\n",
                 bt->treeDepth, bt->rootNode, bt->firstLeaf, bt->lastLeaf, bt->nodeSize, bt->totalNodes);

    /* Allocate node buffer */
    bt->nodeBuffer = malloc(bt->nodeSize);
    if (!bt->nodeBuffer) {
        FS_LOG_DEBUG("HFS BTree: Failed to allocate node buffer (nodeSize=%d)\n", bt->nodeSize);
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
        free(bt->nodeBuffer);
        bt->nodeBuffer = NULL;
    }

    memset(bt, 0, sizeof(HFS_BTree));
}

bool HFS_BT_ReadNode(HFS_BTree* bt, uint32_t nodeNum, void* buffer) {
    if (!bt || !buffer || nodeNum >= bt->totalNodes) return false;

    uint32_t offset = nodeNum * bt->nodeSize;
    return read_btree_data(bt, offset, buffer, bt->nodeSize);
}

bool HFS_BT_WriteNode(HFS_BTree* bt, uint32_t nodeNum, const void* buffer) {
    if (!bt || !buffer || nodeNum >= bt->totalNodes) return false;

    FS_LOG_DEBUG("HFS_BT_WriteNode: writing node %d (nodeSize=%d)\n", nodeNum, bt->nodeSize);

    uint32_t offset = nodeNum * bt->nodeSize;
    return write_btree_data(bt, offset, buffer, bt->nodeSize);
}

/* Insert a record into a leaf node (simplified - assumes space available) */
bool HFS_BT_InsertRecord(HFS_BTree* bt, const void* key, uint16_t keyLen,
                         const void* data, uint16_t dataLen) {
    FS_LOG_DEBUG("HFS_BT_InsertRecord: ENTRY bt=%08x key=%08x keyLen=%d data=%08x dataLen=%d\n",
                 (unsigned int)bt, (unsigned int)key, keyLen, (unsigned int)data, dataLen);

    if (!bt || !key || !data) {
        FS_LOG_DEBUG("HFS_BT_InsertRecord: NULL parameter check failed\n");
        return false;
    }

    /* For now, assume simple single-leaf tree structure */
    /* In a real implementation, we'd navigate the tree to find the right leaf */

    /* Use the root node as the leaf (depth=1 tree) */
    uint32_t leafNodeNum = bt->rootNode;
    FS_LOG_DEBUG("HFS_BT_InsertRecord: Using root/leaf node %d\n", leafNodeNum);

    /* Allocate node buffer */
    FS_LOG_DEBUG("HFS_BT_InsertRecord: About to malloc nodeSize=%d\n", bt->nodeSize);
    uint8_t* nodeBuffer = malloc(bt->nodeSize);
    if (!nodeBuffer) {
        FS_LOG_DEBUG("HFS_BT_InsertRecord: Failed to allocate node buffer (size=%d)\n", bt->nodeSize);
        return false;
    }
    FS_LOG_DEBUG("HFS_BT_InsertRecord: Successfully allocated node buffer\n");

    /* Read the leaf node */
    FS_LOG_DEBUG("HFS_BT_InsertRecord: About to read node %d\n", leafNodeNum);
    if (!HFS_BT_ReadNode(bt, leafNodeNum, nodeBuffer)) {
        FS_LOG_DEBUG("HFS_BT_InsertRecord: Failed to read node %d\n", leafNodeNum);
        free(nodeBuffer);
        return false;
    }
    FS_LOG_DEBUG("HFS_BT_InsertRecord: Successfully read node %d\n", leafNodeNum);

    HFS_BTNodeDesc* nodeDesc = (HFS_BTNodeDesc*)nodeBuffer;
    uint16_t numRecords = be16_read(&nodeDesc->numRecords);

    /* Build full record (key + data) */
    uint16_t recordSize = 1 + keyLen + dataLen;  /* 1 byte for keyLength + key + data */
    uint8_t* fullRecord = malloc(recordSize);
    if (!fullRecord) {
        free(nodeBuffer);
        return false;
    }

    /* Copy key and data into record */
    memcpy(fullRecord, key, 1 + keyLen);  /* keyLength + key data */
    memcpy(fullRecord + 1 + keyLen, data, dataLen);

    /* Find insertion point using key comparison */
    int insertPos = 0;
    for (int i = 0; i < numRecords; i++) {
        void* existingRec;
        uint16_t existingLen;
        if (!HFS_BT_GetRecord(nodeBuffer, bt->nodeSize, i, &existingRec, &existingLen)) {
            continue;
        }

        /* Compare keys */
        int cmp;
        if (bt->type == kBTreeCatalog) {
            cmp = HFS_CompareCatalogKeys(fullRecord, existingRec);
        } else {
            cmp = HFS_CompareExtentsKeys(fullRecord, existingRec);
        }

        if (cmp < 0) {
            /* Found insertion point */
            break;
        }
        insertPos++;
    }

    FS_LOG_DEBUG("HFS_BT_InsertRecord: inserting at position %d of %d\n", insertPos, numRecords);

    /* Calculate offsets */
    uint16_t* offsets = (uint16_t*)(nodeBuffer + bt->nodeSize - 2);

    /* Get offset where new record should go */
    uint16_t insertOffset;
    if (insertPos == 0) {
        insertOffset = sizeof(HFS_BTNodeDesc);
    } else {
        /* Get the end of the previous record */
        void* prevRec;
        uint16_t prevLen;
        HFS_BT_GetRecord(nodeBuffer, bt->nodeSize, insertPos - 1, &prevRec, &prevLen);
        insertOffset = ((uint8_t*)prevRec - nodeBuffer) + prevLen;
    }

    /* Calculate how much data needs to be shifted */
    uint16_t lastOffset;
    if (numRecords > 0) {
        void* lastRec;
        uint16_t lastLen;
        HFS_BT_GetRecord(nodeBuffer, bt->nodeSize, numRecords - 1, &lastRec, &lastLen);
        lastOffset = ((uint8_t*)lastRec - nodeBuffer) + lastLen;
    } else {
        lastOffset = sizeof(HFS_BTNodeDesc);
    }

    /* Shift existing records and offsets to make space */
    if (insertPos < numRecords) {
        /* Shift record data */
        uint16_t bytesToShift = lastOffset - insertOffset;
        memmove(nodeBuffer + insertOffset + recordSize,
                nodeBuffer + insertOffset,
                bytesToShift);

        /* Shift offset table entries */
        for (int i = numRecords; i > insertPos; i--) {
            uint16_t offset = be16_read(&offsets[-(i-1)]);
            be16_write(&offsets[-i], offset + recordSize);
        }
    }

    /* Insert new record */
    memcpy(nodeBuffer + insertOffset, fullRecord, recordSize);
    be16_write(&offsets[-insertPos], insertOffset);

    /* Update offset table for new record count */
    be16_write(&offsets[-(numRecords)], insertOffset + recordSize);

    /* Update node descriptor */
    be16_write(&nodeDesc->numRecords, numRecords + 1);

    /* Write node back */
    bool success = HFS_BT_WriteNode(bt, leafNodeNum, nodeBuffer);

    free(fullRecord);
    free(nodeBuffer);

    FS_LOG_DEBUG("HFS_BT_InsertRecord: %s\n", success ? "SUCCESS" : "FAILED");
    return success;
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
        FS_LOG_DEBUG("HFS_BT_GetRecord: rec=%d offset=%d offsetAddr=%08x\n",
                     recordNum, offset, (unsigned int)&offsets[-(recordNum)]);
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
    void* nodeBuffer = malloc(bt->nodeSize);
    if (!nodeBuffer) return false;

    while (currentNode != 0) {
        /* Read the leaf node */
        if (!HFS_BT_ReadNode(bt, currentNode, nodeBuffer)) {
            free(nodeBuffer);
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
                    free(nodeBuffer);
                    return true;  /* Iterator requested stop */
                }
            }
        }

        /* Move to next leaf node */
        currentNode = be32_read(&nodeDesc->fLink);
    }

    free(nodeBuffer);
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