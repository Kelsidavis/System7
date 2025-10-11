/* HFS B-Tree Management */
#pragma once
#include "hfs_types.h"
#include "hfs_volume.h"
#include <stdbool.h>

/* B-Tree types */
typedef enum {
    kBTreeCatalog,
    kBTreeExtents
} HFS_BTreeType;

/* B-Tree structure */
typedef struct {
    HFS_Volume*  vol;           /* Volume this B-tree belongs to */
    HFS_BTreeType type;         /* Catalog or Extents */

    /* B-tree parameters */
    uint32_t     fileSize;      /* Total file size */
    HFS_Extent   extents[3];    /* First 3 extents */
    uint16_t     nodeSize;      /* Size of each node */
    uint32_t     rootNode;      /* Root node number */
    uint32_t     firstLeaf;     /* First leaf node */
    uint32_t     lastLeaf;      /* Last leaf node */
    uint32_t     totalNodes;    /* Total number of nodes */
    uint16_t     treeDepth;     /* Tree depth */

    /* Node buffer for operations */
    void*        nodeBuffer;    /* Buffer for reading nodes */
} HFS_BTree;

/* Initialize a B-tree */
bool HFS_BT_Init(HFS_BTree* bt, HFS_Volume* vol, HFS_BTreeType type);

/* Close a B-tree */
void HFS_BT_Close(HFS_BTree* bt);

/* Read a node from the B-tree */
bool HFS_BT_ReadNode(HFS_BTree* bt, uint32_t nodeNum, void* buffer);

/* Write a node to the B-tree */
bool HFS_BT_WriteNode(HFS_BTree* bt, uint32_t nodeNum, const void* buffer);

/* Get record from a node */
bool HFS_BT_GetRecord(void* node, uint16_t nodeSize, uint16_t recordNum,
                      void** recordPtr, uint16_t* recordLen);

/* Find a record by key */
bool HFS_BT_FindRecord(HFS_BTree* bt, const void* key, uint16_t keyLen,
                       void* recordBuffer, uint16_t* recordLen);

/* Insert a record into the B-tree (simplified - assumes space available) */
bool HFS_BT_InsertRecord(HFS_BTree* bt, const void* key, uint16_t keyLen,
                         const void* data, uint16_t dataLen);

/* Iterate through leaf nodes */
typedef bool (*HFS_BT_IteratorFunc)(void* key, uint16_t keyLen,
                                    void* data, uint16_t dataLen,
                                    void* context);

bool HFS_BT_IterateLeaves(HFS_BTree* bt, HFS_BT_IteratorFunc func, void* context);

/* Key comparison functions */
int HFS_CompareCatalogKeys(const void* key1, const void* key2);
int HFS_CompareExtentsKeys(const void* key1, const void* key2);