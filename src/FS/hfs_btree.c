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

    /* offset/length/fileSize are 32-bit: %d/%d/%d would pass 4-byte
     * ints where the printf expects longs. */
    FS_LOG_DEBUG("read_btree_data: offset=%ld length=%ld fileSize=%ld vol=%p bd.data=%p\n",
                 (long)offset, (long)length, (long)bt->fileSize,
                 (void*)bt->vol, bt->vol->bd.data);

    uint32_t bytesRead = 0;
    uint32_t currentOffset = offset;

    for (int i = 0; i < bt->extentCount && bytesRead < length; i++) {
        if (bt->extents[i].blockCount == 0) {
            break;
        }
        FS_LOG_DEBUG("read_btree_data: Extent %d - startBlock=%lu, blockCount=%lu\n",
                     i, (unsigned long)bt->extents[i].startBlock,
                     (unsigned long)bt->extents[i].blockCount);

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
        uint32_t blocksToRead = (toRead + byteOffset + bt->vol->alBlkSize - 1) / bt->vol->alBlkSize;
        uint8_t* tempBuffer = NewPtr(blocksToRead * bt->vol->alBlkSize);
        if (!tempBuffer) return false;

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

/*
 * Add the catalog's extents past the MDB's three, from the extents overflow
 * tree: records of three under file ID 4, each keyed by the file allocation
 * block it starts at (Inside Macintosh: Files, 2-83). Only the first three
 * used to be read, so a catalog in more pieces than that - any volume that
 * has grown a while - was cut off where they ended.
 */
static void load_catalog_overflow(HFS_BTree* bt) {
    enum { kCatalogFileID = 4 };
    uint32_t alBlk = bt->vol->alBlkSize;
    uint32_t have = 0;
    for (int i = 0; i < bt->extentCount; i++) have += bt->extents[i].blockCount;
    uint32_t need = (bt->fileSize + alBlk - 1) / alBlk;
    if (have >= need) return;

    HFS_BTree ext;
    if (!HFS_BT_Init(&ext, bt->vol, kBTreeExtents)) {
        FS_LOG_WARN("HFS BTree: catalog needs overflow extents; no extents tree to find them\n");
        return;
    }
    while (have < need && bt->extentCount + 3 <= kBTMaxExtents) {
        uint8_t key[8];
        key[0] = 7;
        key[1] = 0x00;   /* data fork */
        be32_write(key + 2, kCatalogFileID);
        be16_write(key + 6, (uint16_t)have);

        uint8_t record[12];
        uint16_t len = sizeof(record);
        if (!HFS_BT_FindRecord(&ext, key, sizeof(key), record, &len) || len < 12) break;
        uint32_t added = 0;
        for (int i = 0; i < 3; i++) {
            HFS_Extent* e = &bt->extents[bt->extentCount + i];
            e->startBlock = be16_read(record + i * 4);
            e->blockCount = be16_read(record + i * 4 + 2);
            added += e->blockCount;
        }
        if (added == 0) break;
        bt->extentCount += 3;
        have += added;
    }
    HFS_BT_Close(&ext);
    if (have < need) {
        FS_LOG_WARN("HFS BTree: catalog extents cover %lu of %lu blocks\n", (unsigned long)have, (unsigned long)need);
    }
}

bool HFS_BT_Init(HFS_BTree* bt, HFS_Volume* vol, HFS_BTreeType type) {
    if (!bt || !vol || !vol->mounted) {
        FS_LOG_DEBUG("HFS_BT_Init: Invalid params (bt=%p, vol=%p, mounted=%ld)\n",
                     bt, vol, (long)(vol ? vol->mounted : 0));
        return false;
    }

    memset(bt, 0, sizeof(HFS_BTree));
    bt->vol = vol;
    bt->type = type;

    /* Set up extents and file size based on type */
    if (type == kBTreeCatalog) {
        bt->fileSize = vol->catFileSize;
        memcpy(bt->extents, vol->catExtents, sizeof(vol->catExtents));
        bt->extentCount = 3;
    } else if (type == kBTreeExtents) {
        bt->fileSize = vol->extFileSize;
        memcpy(bt->extents, vol->extExtents, sizeof(vol->extExtents));
        bt->extentCount = 3;
    } else {
        return false;
    }

    /* Check if we have valid extents */
    if (bt->fileSize == 0 || bt->extents[0].blockCount == 0) {
        FS_LOG_DEBUG("HFS_BT_Init: No valid extents for B-tree (fileSize=%lu, extent0.blocks=%lu)\n",
                     (unsigned long)bt->fileSize,
                     (unsigned long)bt->extents[0].blockCount);
        return false;
    }

    /* Read header node (node 0) */
    uint8_t headerNode[512];  /* Start with minimum size */
    if (!read_btree_data(bt, 0, headerNode, sizeof(headerNode))) {
        return false;
    }

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

    /* Validate node size - must be power of 2 between 512 and 32768 */
    if (bt->nodeSize < 512 || bt->nodeSize > 32768 ||
        (bt->nodeSize & (bt->nodeSize - 1)) != 0) {
        FS_LOG_DEBUG("HFS BTree: Invalid node size %u\n", bt->nodeSize);
        return false;
    }

    /* Allocate node buffer */
    if (type == kBTreeCatalog) {
        load_catalog_overflow(bt);
    }

    bt->nodeBuffer = NewPtr(bt->nodeSize);
    if (!bt->nodeBuffer) {
        return false;
    }

    /* All three fields are 32-bit: %u would pass 4-byte ints
     * where the printf expects longs. */
    FS_LOG_DEBUG("HFS BTree: Initialized %s tree (nodeSize=%lu, root=%lu, depth=%lu)\n",
                  type == kBTreeCatalog ? "Catalog" : "Extents",
                  (unsigned long)bt->nodeSize, (unsigned long)bt->rootNode,
                  (unsigned long)bt->treeDepth);

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
    uint8_t* offsetTableEnd = (uint8_t*)node + nodeSize - 2;

    /* Offsets are stored backwards from the end */
    uint8_t* offsetEntry = offsetTableEnd - (size_t)recordNum * sizeof(uint16_t);
    uint16_t offset = be16_read(offsetEntry);

    /* DEBUG: Show offset details for first few records */
    if (recordNum < 3) {
        FS_LOG_DEBUG("HFS_BT_GetRecord: rec=%d offset=%d offsetAddr=%p\n",
                     recordNum, offset, offsetEntry);
    }

    uint16_t nextOffset;

    if (recordNum + 1 < numRecords) {
        nextOffset = be16_read(offsetTableEnd - (size_t)(recordNum + 1) * sizeof(uint16_t));
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
                /* The key is its length byte and keyLen more, padded to an
                 * even length; the data follows (Inside Macintosh: Files 2-66). */
                uint16_t keySpan = (uint16_t)((1u + keyLen + 1u) & ~1u);
                void* data = (uint8_t*)record + keySpan;
                uint16_t dataLen = recordLen - keySpan;

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

/*
 * Extents keys as they are on disk (Inside Macintosh: Files, 2-83):
 *   [0] key length (7)  [1] fork type ($00 data, $FF resource)
 *   [2..5] file number  [6..7] first file allocation block of the record
 * Ordered by file number, then fork, then starting block.
 */
int HFS_CompareExtentsKeys(const void* key1, const void* key2) {
    const uint8_t* k1 = (const uint8_t*)key1;
    const uint8_t* k2 = (const uint8_t*)key2;

    uint32_t fid1 = be32_read(k1 + 2);
    uint32_t fid2 = be32_read(k2 + 2);
    if (fid1 != fid2) return (fid1 < fid2) ? -1 : 1;

    if (k1[1] != k2[1]) return (k1[1] < k2[1]) ? -1 : 1;

    uint16_t sb1 = be16_read(k1 + 6);
    uint16_t sb2 = be16_read(k2 + 6);
    if (sb1 != sb2) return (sb1 < sb2) ? -1 : 1;
    return 0;
}

/*
 * Find the leaf record whose key equals `key` and copy out its data;
 * *recordLen holds the buffer's size going in and the data's length coming
 * out.
 *
 * Earlier callers had only a mismatched stub that always failed and filled
 * nothing in. The leaf chain is walked in key order; a record's data starts
 * after its key, on a word boundary.
 */
bool HFS_BT_FindRecord(HFS_BTree* bt, const void* key, uint16_t keyLen,
                       void* recordBuffer, uint16_t* recordLen) {
    (void)keyLen;
    if (!bt || !key || !recordBuffer || !recordLen) return false;
    uint16_t capacity = *recordLen;
    *recordLen = 0;

    int (*compare)(const void*, const void*) =
        (bt->type == kBTreeExtents) ? HFS_CompareExtentsKeys : HFS_CompareCatalogKeys;

    void* node = NewPtr(bt->nodeSize);
    if (!node) return false;

    bool found = false;
    bool passed = false;
    uint32_t current = bt->firstLeaf;
    while (current != 0 && !found && !passed) {
        if (!HFS_BT_ReadNode(bt, current, node)) break;
        HFS_BTNodeDesc* desc = (HFS_BTNodeDesc*)node;
        uint16_t count = be16_read(&desc->numRecords);

        for (uint16_t i = 0; i < count; i++) {
            void* rec;
            uint16_t len;
            if (!HFS_BT_GetRecord(node, bt->nodeSize, i, &rec, &len)) continue;
            int order = compare(rec, key);
            if (order < 0) continue;
            if (order == 0) {
                uint16_t dataStart = (uint16_t)((1u + ((uint8_t*)rec)[0] + 1u) & ~1u);
                if (dataStart <= len) {
                    uint16_t dataLen = (uint16_t)(len - dataStart);
                    if (dataLen > capacity) dataLen = capacity;
                    memcpy(recordBuffer, (uint8_t*)rec + dataStart, dataLen);
                    *recordLen = dataLen;
                    found = true;
                }
            }
            passed = true;   /* keys are in order: past it, it is not there */
            break;
        }
        current = be32_read(&desc->fLink);
    }

    DisposePtr((Ptr)node);
    return found;
}
