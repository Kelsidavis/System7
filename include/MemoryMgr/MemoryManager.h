/* Classic Mac Memory Manager - System 7.1 Style */
#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../SystemTypes.h"  /* Use existing Handle and Zone types */

/* Type aliases for clarity */
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

/* Block flags */
enum {
    BF_FREE      = 1<<0,       /* On free list */
    BF_PTR       = 1<<1,       /* Non-relocatable (Ptr) */
    BF_HANDLE    = 1<<2,       /* Relocatable (Handle) data block */
    BF_LOCKED    = 1<<3,       /* Handle data pinned */
    BF_PURGEABLE = 1<<4,       /* Handle can be discarded */
    BF_RESOURCE  = 1<<5        /* Resource handle */
};

/* Block header - precedes every allocation */
#ifndef BLOCKHEADER_DEFINED
#define BLOCKHEADER_DEFINED
typedef struct BlockHeader {
    u32     size;           /* Total size including header (aligned) */
    u16     flags;          /* BF_* flags */
    u16     lockCount;      /* Handles: lock nesting depth; Ptrs: canary size */
    u32     prevSize;       /* Size of previous block (0 if first) */
    Handle  masterPtr;      /* For handles: backpointer to master pointer */
    /* Data follows immediately after */
} BlockHeader;
#endif

/* Free list node - lives in data area of free blocks */
typedef struct FreeNode {
    struct FreeNode* next;
    struct FreeNode* prev;
} FreeNode;

/* Size class configuration for segregated freelists */
#define NUM_SIZE_CLASSES 8

/* Extended zone info for our implementation */
typedef struct ZoneInfo {
    u8*         base;           /* Start of zone memory */
    u8*         limit;          /* End (exclusive) */

    /* Segregated freelists by size class:
     * [0] 0-64B, [1] 65-128B, [2] 129-256B, [3] 257-512B,
     * [4] 513-1KB, [5] 1KB-2KB, [6] 2KB-4KB, [7] 4KB+ */
    FreeNode*   freelists[NUM_SIZE_CLASSES];

    u32         bytesUsed;      /* Bytes allocated */
    u32         bytesFree;      /* Bytes available */

    /* Master pointer table for handles */
    void**      mpBase;         /* Array of master pointers */
    u32         mpCount;        /* Capacity */
    u32         mpNextFree;     /* Next free slot hint */

    /* M68K virtual address mapping */
    UInt32      m68kBase;       /* Base address in M68K space */
    UInt32      m68kLimit;      /* End (exclusive) in M68K space */

    /* Zone info */
    char        name[32];       /* Zone name */
    bool        growable;       /* Can zone grow? */
} ZoneInfo;

/* Additional Memory Manager error codes not in SystemTypes.h */
enum {
    memPurgedErr   = -112       /* Handle was purged */
};

/* Classic Mac Memory Manager API - Ptr operations */
void*   NewPtr(u32 byteCount);
void*   NewPtrClear(u32 byteCount);
void    DisposePtr(void* p);
u32     GetPtrSize(void* p);
bool    SetPtrSize(void* p, u32 newSize);

/* Classic Mac Memory Manager API - Handle operations */
Handle  NewHandle(u32 byteCount);
Handle  NewHandleClear(u32 byteCount);
void    DisposeHandle(Handle h);
u32     GetHandleSize(Handle h);
bool    SetHandleSize(Handle h, u32 newSize);
void    HLock(Handle h);
void    HUnlock(Handle h);
void    HPurge(Handle h);
void    HNoPurge(Handle h);
void    MoveHHi(Handle h);
void    EmptyHandle(Handle h);
bool    RecoverHandle(void* p, Handle* h);

/* Handle utility functions - System 7 */
OSErr   HandToHand(Handle* theHndl);
OSErr   PtrToHand(const void* srcPtr, Handle* dstHndl, Size size);
OSErr   PtrAndHand(const void* srcPtr, Handle dstHndl, Size size);
OSErr   HandAndHand(Handle aHndl, Handle bHndl);

/* Zone operations */
void    InitZone(ZoneInfo* zone, void* memory, u32 size, void** masterTable, u32 masterCount);
ZoneInfo* GetZone(void);
void    SetZone(ZoneInfo* zone);
u32     FreeMem(void);
u32     MaxMem(void);
u32     CompactMem(u32 cbNeeded);
void    PurgeMem(u32 cbNeeded);

/* Memory utility functions */
void    BlockMoveData(const void* srcPtr, void* destPtr, Size byteCount);
SInt16  MemError(void);

/* Standard C library interface */
void*   malloc(size_t size);
void    free(void* ptr);
void*   calloc(size_t nmemb, size_t size);
void*   realloc(void* ptr, size_t size);

/* Memory Manager initialization */
void    InitMemoryManager(void);
void    init_memory_manager(ZonePtr sysZone, ZonePtr applZone);

/* Internal implementation functions */
bool    SetHandleSize_MemMgr(Handle h, u32 newSize);
OSErr   memory_manager_handle_prologue(Handle handle, ZonePtr* outZone);
OSErr   set_handle_size_24bit(Handle h, Size newSize);
OSErr   set_handle_size_32bit(Handle h, Size newSize);

/* Map Memory Manager zones into M68K interpreter address space */
struct M68KAddressSpace;
OSErr   MemoryManager_MapToM68K(struct M68KAddressSpace* as);

/* Synchronize key low-memory globals with current zone state */
void    MemoryManager_SyncLowMemGlobals(void);

/* Utility to detect if a pointer belongs to a managed heap */
bool    MemoryManager_IsHeapPointer(const void* p);

/* Debugging */
void    CheckHeap(ZoneInfo* zone);
void    DumpHeap(ZoneInfo* zone);
void    MemoryManager_CheckSuspectBlock(const char* tag);

#endif /* MEMORY_MANAGER_H */
