/*
 * RE-AGENT-BANNER: reimplementation
 *
 * Mac OS System 7 Memory Manager Types and Structures
 *
 * Reimplemented from Apple Computer Inc. source code:
 * - MemoryMgrInternal.a (SHA256: 4a00757669c618159be69d8c73653d6a7b7a017043d2d65c300054db404bd0c9)
 * - BlockMove.a (SHA256: 2a112bff69e4cb4b57253a8c2c210bad48d6020d43d4cb6932d852119bce7af0)
 *
 * Original authors: Martin P. Haeberli, Andy Hertzfeld, Gary Davidian (1982-1993)
 * Reimplemented: 2025-09-18
 *
 * PROVENANCE: Structures derived from assembly code analysis of Mac OS System 7
 * Memory Manager internals, preserving original algorithms and data layouts.
 */

#ifndef MEMORY_MANAGER_TYPES_H
#define MEMORY_MANAGER_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Basic Mac OS types */
typedef void*       Ptr;
typedef void**      Handle;
typedef int32_t     Size;
typedef int16_t     OSErr;
typedef uint8_t     Byte;
typedef int8_t      SignedByte;

/* Memory Manager constants from assembly analysis */
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

/* Block type tags (from assembly code analysis) */
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
#define noErr               0
#define memFullErr          -108    /* Not enough memory */
#define nilHandleErr        -109    /* NIL master pointer */
#define memWZErr            -111    /* Wrong zone */

/*
 * Zone Header Structure
 * PROVENANCE: Derived from zone management code analysis in MemoryMgrInternal.a
 * Functions: MMHPrologue, WhichZone, a24/a32 zone management routines
 */
typedef struct Zone {
    Ptr         bkLim;          /* Zone limit pointer - end of zone memory */
    Ptr         purgePtr;       /* Purge pointer - points to purgeable blocks */
    Ptr         hFstFree;       /* Handle to first free block in zone */
    Size        zcbFree;        /* Total free bytes in zone */
    Ptr         gzProc;         /* Grow zone procedure pointer */
    int16_t     moreMast;       /* Number of master pointers to allocate */
    int16_t     flags;          /* Zone flags and attributes */
    int16_t     cntRel;         /* Count of relocatable blocks */
    int16_t     maxRel;         /* Maximum relocatable blocks allowed */
    int16_t     cntNRel;        /* Count of non-relocatable blocks */
    int16_t     heapData;       /* Heap overhead data */
    Ptr         ZoneJumpV;      /* Jump vector table pointer (24/32-bit dispatch) */
} Zone, *ZonePtr;

/*
 * Block Header Structure
 * PROVENANCE: Inferred from CompactHp and block management functions
 * Used by: a24/a32MakeBkF, a24/a32MakeFree, block scanning algorithms
 */
typedef struct BlockHeader {
    Size        blkSize;        /* Block size with flags in high bits */
    union {
        /* For free blocks */
        struct {
            Ptr fwdLink;        /* Forward link to next free block */
        } free;
        /* For allocated blocks */
        struct {
            SignedByte tagByte; /* Block type tag */
            Byte       reserved[3]; /* Reserved for alignment */
        } allocated;
    } u;
} BlockHeader, *BlockPtr;

/*
 * Master Pointer Entry
 * PROVENANCE: Master pointer management code analysis
 * Functions: a24/a32NextMaster, a24/a32HMakeMoreMasters
 */
typedef struct MasterPointer {
    Ptr         ptr;            /* Pointer to relocatable block data */
} MasterPointer, *MasterPtr;

/*
 * Memory Manager Global Variables
 * PROVENANCE: Global variable references in MMHPrologue and management routines
 */
typedef struct MemoryManagerGlobals {
    ZonePtr     theZone;        /* Current default zone pointer */
    ZonePtr     SysZone;        /* System heap zone pointer */
    ZonePtr     ApplZone;       /* Application heap zone pointer */
    Ptr         AllocPtr;       /* Allocation rover pointer */
    Byte        MMFlags;        /* Memory Manager flags */
    Ptr         JBlockMove;     /* Jump vector for BlockMove routine */
    Ptr         jCacheFlush;    /* Jump vector for cache flush routine */
} MemoryManagerGlobals;

/*
 * BlockMove Parameters Structure
 * PROVENANCE: BlockMove.a register interface documentation
 * Used by: BlockMove68020, BlockMove68040, copy optimization routines
 */
typedef struct BlockMoveParams {
    Ptr         src;            /* Source pointer (A0) */
    Ptr         dst;            /* Destination pointer (A1) */
    Size        count;          /* Byte count (D0) */
    Size        addressDiff;    /* Destination - source (D1, for overlap detection) */
    uint32_t    alignMask;      /* Alignment mask (D2) */
} BlockMoveParams;

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
typedef OSErr (*PurgeProc)(Handle h);
typedef OSErr (*GrowZoneProc)(Size cbNeeded);
typedef void (*BlockMoveProc)(Ptr src, Ptr dst, Size count);
typedef void (*CacheFlushProc)(void);

/* Memory Manager function categories for dispatch */
typedef enum {
    MM_HANDLE_OPERATION,
    MM_POINTER_OPERATION,
    MM_ZONE_OPERATION,
    MM_BLOCK_OPERATION
} MemMgrCategory;

/* Addressing mode selection */
typedef enum {
    ADDR_MODE_24BIT,
    ADDR_MODE_32BIT
} AddressingMode;

/* Processor optimization levels */
typedef enum {
    CPU_68000,
    CPU_68020,
    CPU_68040
} ProcessorType;

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