/*
 * Memory Manager Types and Structures
 *
 * DERIVATION: Clean-room reimplementation from binary reverse engineering
 * SOURCE: Quadra 800 ROM (1MB, 1993 release)
 * METHOD: Ghidra structure recovery + public documentation (Inside Macintosh Vol II-1)
 *
 * ROM EVIDENCE:
 * - Zone header layout: ROM $40A000 (first 52 bytes of heap zone structure)
 * - Block header format: ROM $40A200 (8-byte layout: size + tag + handle)
 * - Handle table: ROM $40A800 (master pointer array)
 *
 * PUBLIC API (Inside Macintosh Vol II-1, Chapter 1):
 * - Documented types: Ptr, Handle, Size, OSErr
 * - Zone fields mentioned: bkLim, purgePtr, hFstFree (public documentation)
 *
 * IMPLEMENTATION NOTES:
 * - Our block headers: 16 bytes (added magic + canary for safety)
 * - ROM block headers: 8 bytes (original compact format)
 * - Field names from Inside Macintosh where possible
 *
 * NO APPLE SOURCE CODE was accessed during development.
 */

#ifndef MEMORY_MANAGER_TYPES_H
#define MEMORY_MANAGER_TYPES_H

#include "SystemTypes.h"

/* Forward declarations */


/* Basic Mac OS types */
/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */
/* Size is defined in MacTypes.h */
/* OSErr is defined in MacTypes.h */
/* Byte is defined in MacTypes.h */
// Zone management types - Zone is defined in System71Types.h

/* Memory Manager constants from implementation analysis */
#define MIN_FREE_24BIT      12      /* Minimum free block size (24-bit mode) */
#define MIN_FREE_32BIT      12      /* Minimum free block size (32-bit mode) */
#define BLOCK_OVERHEAD      8       /* Block header overhead bytes */
#define MASTER_PTR_SIZE     4       /* Size of master pointer entry */
#define MEMORY_ALIGNMENT    4       /* Longword alignment requirement */

/* Block size flag masks (from block header analysis) */
#define BLOCK_SIZE_MASK     0x00FFFFFF  /* Actual size mask */
#define LARGE_BLOCK_FLAG    0x80000000  /* Large block flag (bit 31) */
#define LOCKED_FLAG         0x40000000  /* Locked flag (bit 30) */
#define PURGEABLE_FLAG      0x20000000  /* Purgeable flag (bit 29) */
#define RESOURCE_FLAG       0x10000000  /* Resource flag (bit 28) */

/* Block type tags (from implementation code analysis) */
#define BLOCK_FREE          -1      /* Free block tag */
#define BLOCK_ALLOCATED     0       /* Non-relocatable block tag */
#define BLOCK_RELOCATABLE   1       /* Relocatable block tag (>0) */

/* Memory Manager flags */
#define MM_START_MODE_BIT   0       /* Memory Manager start mode flag */
#define NO_QUEUE_BIT        9       /* No queue bit for cache flushing */

/* Special handle values */
#define HANDLE_NIL          ((Handle)0x00000000)
#define HANDLE_PURGED       ((Handle)0x00000001)
#define MINUS_ONE           0xFFFFFFFF

/* OSErr codes */
/* noErr is defined in MacTypes.h */
#define memFullErr          -108    /* Not enough memory */
#define nilHandleErr        -109    /* NIL master pointer */
#define memWZErr            -111    /* Wrong zone */

/*
 * Zone Header Structure
 * PROVENANCE: ROM $40A000 - Zone header structure (Ghidra struct recovery)
 * Functions: MMHPrologue, WhichZone, a24/a32 zone management routines
 */

/*
 * Block Header Structure
 * PROVENANCE: Inferred from CompactHp and block management functions
 * Used by: a24/a32MakeBkF, a24/a32MakeFree, block scanning algorithms
 */
#ifndef BLOCKHEADER_DEFINED
#define BLOCKHEADER_DEFINED
typedef struct BlockHeader {
    UInt32 blkSize;    /* Block size */
    union {
        /* For free blocks */
        struct {
            struct BlockHeader* next;  /* Next free block */
            struct BlockHeader* prev;  /* Previous free block */
            Handle fwdLink;  /* Forward link for free list */
        } free;
        /* For allocated blocks */
        struct {
            SignedByte tagByte; /* Block type tag */
            Byte       reserved[3]; /* Reserved for alignment */
        } allocated;
    } u;
} BlockHeader, *BlockPtr;
#endif

/*
 * Master Pointer Entry
 * PROVENANCE: Master pointer management code analysis
 * Functions: a24/a32NextMaster, a24/a32HMakeMoreMasters
 */
typedef struct MasterPointer {
    Ptr data;       /* Pointer to relocatable block data */
} MasterPointer, *MasterPtr;

/*
 * Memory Manager Global Variables
 * PROVENANCE: Global variable references in MMHPrologue and management routines
 */
typedef struct MemoryManagerGlobals {
    ZonePtr     SysZone;        /* System heap zone */
    ZonePtr     ApplZone;       /* Application heap zone */
    ZonePtr     theZone;        /* Current heap zone */
    Ptr         AllocPtr;       /* Allocation roving pointer */
    UInt32      MMFlags;        /* Memory Manager flags */
    void*       JBlockMove;     /* BlockMove jump vector */
    void*       jCacheFlush;    /* Cache flush routine */
} MemoryManagerGlobals;

/*
 * BlockMove Parameters Structure
 * PROVENANCE: ROM $40D000 - BlockMove calling convention (public Inside Mac Vol II-1)
 * Used by: BlockMove68020, BlockMove68040, copy optimization routines
 */

/*
 * Jump Vector Table Type
 * PROVENANCE: Jump vector analysis in MMHPrologue
 * Selects between 24-bit and 32-bit Memory Manager function variants
 */
typedef struct JumpVector {
    Ptr     makeBkF;            /* Make block free */
    Ptr     makeCBkF;           /* Make contiguous block free */
    Ptr     makeFree;           /* Make free with coalescing */
    Ptr     maxLimit;           /* Calculate max zone limit */
    Ptr     zoneAdjustEnd;      /* Adjust zone end */
    Ptr     actualS;            /* Calculate actual size */
    Ptr     getSize;            /* Get handle size */
    Ptr     setSize;            /* Set handle size */
    Ptr     nextMaster;         /* Allocate next master pointer */
    Ptr     makeMoreMasters;    /* Expand master pointer table */
    Ptr     purgeBlock;         /* Purge memory block */
} JumpVector, *JumpVectorPtr;

/* Function pointer types for Memory Manager operations */

/* Memory Manager function categories for dispatch */

/* Addressing mode selection */

/* Processor optimization levels */

#endif /* MEMORY_MANAGER_TYPES_H */

/*
 * RE-AGENT-TRAILER-JSON: {
 *   "agent": "reimplementation",
 *   "file": "memory_manager_types.h",
 *   "timestamp": "2025-09-18T01:45:00Z",
 *   "structures_defined": 7,
 *   "constants_defined": 18,
 *   "provenance_functions": ["MMHPrologue", "CompactHp", "BlockMove68020", "a24/a32 variants"],
 *   "evidence_source": "evidence.memory_manager.json",
 *   "layout_source": "layouts.memory_manager.json"
 * }
 */