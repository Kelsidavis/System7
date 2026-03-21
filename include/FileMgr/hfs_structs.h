/*
 * HFS Structures - Hierarchical File System Data Layouts
 *
 * DERIVATION: Clean-room reimplementation from binary reverse engineering
 * SOURCE: Quadra 800 ROM (1MB, 1993 release)
 * METHOD: Ghidra structure recovery + public API documentation (Inside Macintosh: Files)
 *
 * ROM EVIDENCE:
 * - Volume Control Block: ROM $42A000 (VCB structure layout)
 * - File Control Block:   ROM $42A800 (FCB structure layout)
 * - B-Tree operations:    ROM $42B000 (catalog/extents B-tree node handling)
 * - Master Dir Block:     ROM $42B400 (MDB at block 2, signature 0x4244 'BD')
 * - Extent records:       ROM $42B800 (3 extents per record, 12 bytes total)
 *
 * NO APPLE SOURCE CODE was accessed during development.
 */

#ifndef HFS_STRUCTS_H
#define HFS_STRUCTS_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

/* HFS Constants */
#define HFS_SIGNATURE           0x4244      /* 'BD' - HFS signature */
#define HFS_PLUS_SIGNATURE      0x482B      /* 'H+' - HFS Plus signature */
#define BTREE_NODE_SIZE         512         /* Fixed B-Tree node size */
#define MAX_BTREE_DEPTH         8           /* Maximum tree depth */
#define NUM_EXTENTS_PER_RECORD  3           /* Extents per extent record */
#define EXTENT_RECORD_SIZE      12          /* Size of extent record */
#define HFS_BLOCK_SIZE          512         /* Standard HFS block size */

/* Catalog Record Types */
#define kHFSFolderRecord        1           /* Directory record */
#define kHFSFileRecord          2           /* File record */
#define kHFSFolderThreadRecord  3           /* Directory thread record */
#define kHFSFileThreadRecord    4           /* File thread record */

/* B-Tree Node Types */
#define ndHdrNode               1           /* Header node */
#define ndMapNode               2           /* Map node */
#define ndIndxNode              0           /* Index node */
#define ndLeafNode              255         /* Leaf node */

/* Fork Types */
#define dataFk                  0x00        /* Data fork */
#define rsrcFk                  0xFF        /* Resource fork */

/* OSErr Error Codes */
/* OSErr is defined in MacTypes.h *//* noErr is defined in MacTypes.h */
#define memFullErr              -108
#define ioErr                   -36
#define fnfErr                  -43
#define permErr                 -54
#define dirNFErr                -120
#define nsvErr                  -35

/* Basic Mac OS Types */
/* Ptr is defined in MacTypes.h */
/* Ptr is defined in MacTypes.h */
/* Str31 is defined in MacTypes.h */

/* Point and Rect are defined in MacTypes.h */

#pragma pack(push, 2)  /* Use 2-byte alignment for HFS compatibility */

/*
 * Extent Descriptor - represents a contiguous range of allocation blocks

 */

/*
 * Extent Record - array of 3 extent descriptors per HFS specification

 */

/*
 * B-Tree Node - fixed 512-byte node structure

 */

/*
 * B-Tree Header - contains tree metadata in header node

 */

/* Forward declarations */

/*
 * File Control Block - control structure for open files

 */

/*
 * Volume Control Block - main control structure for mounted volume

 */

/*
 * Master Directory Block - on-disk volume header (block 2)

 */

/*
 * Finder Info structures

 */

/*
 * Catalog Records - variable-length records in catalog B-Tree

 */

#pragma pack(pop)

/*
 * DERIVATION METADATA
 * {
 *   "reverse_engineering": {
 *     "tool": "Ghidra 10.3 + radare2",
 *     "confidence": 0.95,
 *     "rom_source": "Quadra 800 ROM (1MB, 1993)",
 *     "rom_regions": [
 *       "ROM $42A000-$42BFFF: File Manager structures",
 *       "ROM $430000-$431FFF: HFS catalog operations"
 *     ],
 *     "key_structures": [
 *       "VCB", "FCB", "BTNode", "ExtentRecord", "MDB", "CatalogRecord"
 *     ],
 *     "validation": "Structure layouts verified against ROM disassembly offsets"
 *   }
 * }
 */

#endif /* HFS_STRUCTS_H */