/* Classic Mac Memory Manager Implementation */
#include "../../include/MemoryMgr/MemoryManager.h"
#include <string.h>
#include "MemoryMgr/MemoryLogging.h"
#include "CPU/M68KInterp.h"
#include "CPU/LowMemGlobals.h"
#include "System71StdLib.h"

/* Serial debug output */

/* Alignment */
#define ALIGN     8u
#define BLKHDR_SZ ((u32)sizeof(BlockHeader))

/* Minimum block size must accommodate BlockHeader + FreeNode AND be aligned */
#define MIN_BLOCK_SIZE_RAW (BLKHDR_SZ + (u32)sizeof(FreeNode))
#define MIN_BLOCK_SIZE ((MIN_BLOCK_SIZE_RAW + (ALIGN-1)) & ~(ALIGN-1))

/* Heap validation magic numbers */
#define BLOCK_MAGIC_ALLOCATED  0xA110C8ED  /* "ALLOCATED" */
#define BLOCK_MAGIC_FREE       0xFEEEFEEE  /* "FREE" */
#define FREENODE_MAGIC         0xF4EE1157  /* "FREELIST" */

static inline u32 align_up(u32 n) {
    return (n + (ALIGN-1)) & ~(ALIGN-1);
}

/* Get size class index for a block size (segregated freelists) */
static inline u32 get_size_class(u32 size) {
    if (size <= 64) return 0;
    if (size <= 128) return 1;
    if (size <= 256) return 2;
    if (size <= 512) return 3;
    if (size <= 1024) return 4;
    if (size <= 2048) return 5;
    if (size <= 4096) return 6;
    return 7;  /* 4KB+ */
}

/* Global zones */
static ZoneInfo gSystemZone;       /* System heap */
static ZoneInfo gAppZone;          /* Application heap */
static ZoneInfo* gCurrentZone = NULL;

/* Heap storage (allocated from nanokernel) */
static u8* gSystemHeap = NULL;    /* System heap (allocated dynamically) */
static u8* gAppHeap = NULL;       /* App heap (allocated dynamically) */

/* Master pointer tables */
static void* gSystemMasters[1024];         /* 1024 system handles */
static void* gAppMasters[4096];            /* 4096 app handles */

/* ======================== Free List Management ======================== */

static FreeNode* block_to_freenode(BlockHeader* b) {
    return (FreeNode*)((u8*)b + BLKHDR_SZ);
}

static BlockHeader* freenode_to_block(FreeNode* n) {
    return (BlockHeader*)((u8*)n - BLKHDR_SZ);
}

/* Validate that a FreeNode pointer is within zone bounds */
static inline bool is_valid_freenode(ZoneInfo* z, FreeNode* n) {
    if (!n) return false;
    u8* ptr = (u8*)n;
    /* FreeNode must be within zone and have room for header before it */
    return (ptr >= z->base + BLKHDR_SZ) && (ptr + sizeof(FreeNode) <= z->limit);
}

/* Validate block header integrity */
static inline bool validate_block(ZoneInfo* z, BlockHeader* b) {
    extern void serial_puts(const char* str);

    if (!b || !z) {
        serial_puts("[HEAP] validate_block: NULL block or zone\n");
        return false;
    }

    /* Block must be within zone */
    u8* bptr = (u8*)b;
    if (bptr < z->base || bptr >= z->limit) {
        serial_puts("[HEAP] validate_block: Block outside zone bounds\n");
        return false;
    }

    /* Size must be non-zero and aligned */
    if (b->size == 0) {
        serial_puts("[HEAP] validate_block: Block size is zero\n");
        return false;
    }
    if ((b->size & (ALIGN - 1)) != 0) {
        serial_puts("[HEAP] validate_block: Block size not aligned\n");
        return false;
    }

    /* Size must not exceed remaining zone space */
    if (b->size > (u32)(z->limit - bptr)) {
        serial_puts("[HEAP] validate_block: Block size exceeds zone\n");
        return false;
    }

    /* For free blocks, size must be at least MIN_BLOCK_SIZE */
    if ((b->flags & BF_FREE) && b->size < MIN_BLOCK_SIZE) {
        serial_puts("[HEAP] validate_block: Free block too small\n");
        return false;
    }

    /* prevSize must be reasonable */
    if (b->prevSize > (u32)(bptr - z->base)) {
        serial_puts("[HEAP] validate_block: Invalid prevSize\n");
        return false;
    }

    return true;
}

/* Validate freelist integrity for segregated lists */
static bool validate_freelist(ZoneInfo* z) {
    extern void serial_puts(const char* str);

    if (!z) return false;

    /* Validate each size class */
    for (u32 sc = 0; sc < NUM_SIZE_CLASSES; sc++) {
        FreeNode* head = z->freelists[sc];
        if (!head) continue;  /* Empty class is valid */

        if (!is_valid_freenode(z, head)) {
            serial_puts("[HEAP] CORRUPTION: Invalid freelist head in size class\n");
            return false;
        }

        FreeNode* it = head;
        FreeNode* start = it;
        u32 count = 0;

        do {
            /* Validate current node */
            if (!is_valid_freenode(z, it)) {
                serial_puts("[HEAP] CORRUPTION: Invalid freenode in list\n");
                return false;
            }

            /* Validate next pointer */
            if (!is_valid_freenode(z, it->next)) {
                serial_puts("[HEAP] CORRUPTION: Invalid next pointer\n");
                return false;
            }

            /* Validate block header */
            BlockHeader* b = freenode_to_block(it);
            if (!validate_block(z, b)) {
                serial_puts("[HEAP] CORRUPTION: Invalid block header\n");
                return false;
            }

            /* Check circular consistency */
            if (it->next->prev != it) {
                serial_puts("[HEAP] CORRUPTION: Broken circular link\n");
                return false;
            }

            it = it->next;
            count++;

            if (count > 10000) {
                serial_puts("[HEAP] CORRUPTION: Freelist too long or infinite loop\n");
                return false;
            }
        } while (it != start);
    }

    return true;
}

static void freelist_insert(ZoneInfo* z, BlockHeader* b) {
    /* NO LOGGING - serial_printf corrupts registers! */

    /* Bounds check - block must be within zone */
    if ((u8*)b < z->base || (u8*)b >= z->limit) {
        return;  /* Invalid block, don't insert */
    }

    /* CRITICAL: Block must be large enough to hold FreeNode! */
    if (b->size < MIN_BLOCK_SIZE) {
        return;  /* Block too small, would corrupt memory */
    }

    /* CRITICAL: Block size must be aligned! */
    if ((b->size & (ALIGN - 1)) != 0) {
        extern void serial_puts(const char* str);
        serial_puts("[FREELIST_INSERT] WARNING: block size not aligned, aligning down!\n");
        b->size = b->size & ~(ALIGN - 1);  /* Align down */
        if (b->size < MIN_BLOCK_SIZE) {
            return;  /* Too small after alignment */
        }
    }

    b->flags = BF_FREE;
    FreeNode* n = block_to_freenode(b);

    /* Determine size class for segregated freelist */
    u32 sc = get_size_class(b->size);
    FreeNode** head = &z->freelists[sc];

    /* Initialize node pointers */
    n->next = n->prev = NULL;

    if (!*head) {
        /* First block in this size class - create single-node circular list */
        *head = n;
        n->next = n->prev = n;
        return;
    }

    /* Insert at head (LIFO for better cache locality) */
    n->next = *head;
    n->prev = (*head)->prev;

    /* Defensive: validate head before dereferencing */
    if (!is_valid_freenode(z, *head)) {
        /* Corrupted list - rebuild as single node */
        *head = n;
        n->next = n->prev = n;
        return;
    }

    (*head)->prev->next = n;
    (*head)->prev = n;
    *head = n;  /* New head */
}

static void freelist_remove(ZoneInfo* z, BlockHeader* b) {
    /* NO LOGGING - serial_printf corrupts registers! */

    if (!b) return;  /* Defensive: NULL check */

    FreeNode* n = block_to_freenode(b);

    /* Defensive: NULL checks before dereferencing */
    if (!n || !n->next || !n->prev) {
        /* Corrupted node - can't safely remove */
        return;
    }

    /* Determine size class */
    u32 sc = get_size_class(b->size);
    FreeNode** head = &z->freelists[sc];

    /* Single node in this size class? */
    if (n->next == n) {
        *head = NULL;
        n->next = n->prev = NULL;
        return;
    }

    /* Remove from circular list */
    n->prev->next = n->next;
    n->next->prev = n->prev;

    /* Update head if we're removing the head node */
    if (*head == n) {
        *head = n->next;
    }

    /* Clear pointers to prevent use-after-free */
    n->next = n->prev = NULL;
}

/* ======================== Coalescing ======================== */

static BlockHeader* coalesce_forward(ZoneInfo* z, BlockHeader* b) {
    if (!b || !z) return b;  /* Defensive: NULL checks */

    /* Validate block is within zone bounds */
    if ((u8*)b < z->base || (u8*)b >= z->limit) return b;

    u8* end = (u8*)b + b->size;

    /* Bounds check: end must be within zone */
    if (end >= z->limit) return b;

    BlockHeader* next = (BlockHeader*)end;

    /* Validate next block is fully within bounds */
    if ((u8*)next + BLKHDR_SZ > z->limit) return b;

    /* Only coalesce if next block is free */
    if (next->flags & BF_FREE) {
        /* Validate next block size before coalescing */
        if (next->size == 0 || next->size > (u32)(z->limit - (u8*)next)) {
            return b;  /* Corrupted next block, don't coalesce */
        }

        freelist_remove(z, next);
        b->size += next->size;

        /* CRITICAL: Ensure coalesced size is aligned */
        if ((b->size & (ALIGN - 1)) != 0) {
            extern void serial_puts(const char* str);
            serial_puts("[COALESCE_FWD] WARNING: merged size not aligned!\n");
            b->size = b->size & ~(ALIGN - 1);  /* Align down */
        }

        /* Update following block's prevSize */
        u8* after = (u8*)b + b->size;
        if (after < z->limit && after + BLKHDR_SZ <= z->limit) {
            BlockHeader* nxt = (BlockHeader*)after;
            nxt->prevSize = b->size;
        }
    }
    return b;
}

static BlockHeader* coalesce_backward(ZoneInfo* z, BlockHeader* b) {
    if (!b || !z) return b;  /* Defensive: NULL checks */

    /* Validate block is within zone bounds */
    if ((u8*)b < z->base || (u8*)b >= z->limit) return b;

    /* Only try if there's a previous block */
    if (b->prevSize == 0) return b;

    /* Validate prevSize is reasonable */
    if (b->prevSize > (u32)((u8*)b - z->base)) {
        return b;  /* Corrupted prevSize, don't coalesce */
    }

    BlockHeader* prev = (BlockHeader*)((u8*)b - b->prevSize);

    /* Validate prev is within bounds */
    if ((u8*)prev < z->base) return b;

    /* Only coalesce if prev block is free */
    if (prev->flags & BF_FREE) {
        /* Validate prev block size */
        if (prev->size != b->prevSize) {
            return b;  /* Size mismatch, corruption detected */
        }

        freelist_remove(z, prev);
        prev->size += b->size;

        /* CRITICAL: Ensure coalesced size is aligned */
        if ((prev->size & (ALIGN - 1)) != 0) {
            extern void serial_puts(const char* str);
            serial_puts("[COALESCE_BWD] WARNING: merged size not aligned!\n");
            prev->size = prev->size & ~(ALIGN - 1);  /* Align down */
        }

        /* Update following block's prevSize */
        u8* after = (u8*)prev + prev->size;
        if (after < z->limit && after + BLKHDR_SZ <= z->limit) {
            BlockHeader* nxt = (BlockHeader*)after;
            nxt->prevSize = prev->size;
        }

        b = prev;
    }
    return b;
}

/* ======================== Zone Management ======================== */

void InitZone(ZoneInfo* zone, void* memory, u32 size, void** masterTable, u32 masterCount) {
    memset(zone, 0, sizeof(*zone));
    zone->base = (u8*)memory;
    zone->limit = zone->base + size;

    /* Initialize all segregated freelists to NULL */
    for (u32 i = 0; i < NUM_SIZE_CLASSES; i++) {
        zone->freelists[i] = NULL;
    }

    /* Create one big free block spanning the zone */
    BlockHeader* b = (BlockHeader*)zone->base;
    b->size = align_up(size);
    b->flags = BF_FREE;
    b->prevSize = 0;
    b->masterPtr = NULL;

    /* Insert initial block into appropriate size class */
    u32 sc = get_size_class(b->size);
    FreeNode* n = (FreeNode*)((u8*)b + BLKHDR_SZ);
    n->next = n->prev = n;
    zone->freelists[sc] = n;

    zone->bytesFree = size - BLKHDR_SZ;
    zone->bytesUsed = BLKHDR_SZ;

    /* Master pointer table */
    zone->mpBase = masterTable;
    zone->mpCount = masterCount;
    zone->mpNextFree = 0;
    for (u32 i = 0; i < masterCount; i++) {
        zone->mpBase[i] = NULL;
    }

    zone->m68kBase = 0;
    zone->m68kLimit = 0;
}

ZoneInfo* GetZone(void) {
    return gCurrentZone;
}

void SetZone(ZoneInfo* zone) {
    gCurrentZone = zone;
}

u32 FreeMem(void) {
    return gCurrentZone ? gCurrentZone->bytesFree : 0;
}

u32 MaxMem(void) {
    if (!gCurrentZone) return 0;

    u32 maxBlock = 0;

    /* Search all size classes for largest block */
    for (u32 sc = 0; sc < NUM_SIZE_CLASSES; sc++) {
        FreeNode* head = gCurrentZone->freelists[sc];
        if (!head) continue;

        /* Validate head pointer before use */
        if (!is_valid_freenode(gCurrentZone, head)) {
            gCurrentZone->freelists[sc] = NULL;
            continue;
        }

        FreeNode* it = head;
        FreeNode* start = it;
        u32 loop_safety = 0;

        do {
            /* Defensive: validate pointer */
            if (!is_valid_freenode(gCurrentZone, it) ||
                !is_valid_freenode(gCurrentZone, it->next)) {
                gCurrentZone->freelists[sc] = NULL;
                break;
            }

            BlockHeader* b = freenode_to_block(it);
            if (b->size > maxBlock) {
                maxBlock = b->size;
            }
            it = it->next;

            /* Safety limit */
            loop_safety++;
            if (loop_safety > 10000) {
                gCurrentZone->freelists[sc] = NULL;
                break;
            }
        } while (it != start);
    }

    return maxBlock > BLKHDR_SZ ? maxBlock - BLKHDR_SZ : 0;
}

/* ======================== Block Allocation ======================== */

static BlockHeader* find_fit(ZoneInfo* z, u32 need) {
    /* NO LOGGING - even serial_puts corrupts return value! */

    /* Determine starting size class */
    u32 start_class = get_size_class(need);

    /* Try exact size class first */
    FreeNode* head = z->freelists[start_class];
    if (head) {
        /* Validate head pointer before use */
        if (!is_valid_freenode(z, head)) {
            z->freelists[start_class] = NULL;
        } else {
            FreeNode* it = head;
            FreeNode* start = it;
            u32 loop_safety = 0;

            do {
                /* Defensive: validate pointer before dereferencing */
                if (!is_valid_freenode(z, it) || !is_valid_freenode(z, it->next)) {
                    z->freelists[start_class] = NULL;
                    break;
                }

                BlockHeader* b = freenode_to_block(it);
                if (b->size >= need) return b;

                it = it->next;
                loop_safety++;
                if (loop_safety > 10000) {
                    z->freelists[start_class] = NULL;
                    break;
                }
            } while (it != start);
        }
    }

    /* Try larger size classes */
    for (u32 sc = start_class + 1; sc < NUM_SIZE_CLASSES; sc++) {
        head = z->freelists[sc];
        if (!head) continue;

        /* Validate and return first block from larger class */
        if (!is_valid_freenode(z, head)) {
            z->freelists[sc] = NULL;
            continue;
        }

        BlockHeader* b = freenode_to_block(head);
        if (b->size >= need) return b;
    }

    return NULL;  /* No fit found */
}

static void split_block(ZoneInfo* z, BlockHeader* b, u32 need) {
    u32 remain = b->size - need;
    freelist_remove(z, b);

    /* CRITICAL FIX: Use MIN_BLOCK_SIZE instead of BLKHDR_SZ + ALIGN */
    if (remain >= MIN_BLOCK_SIZE) {
        /* CRITICAL: Ensure remain is aligned! */
        if ((remain & (ALIGN - 1)) != 0) {
            /* This should never happen - but if it does, align it down */
            extern void serial_puts(const char* str);
            serial_puts("[SPLIT] WARNING: remain not aligned, aligning down!\n");
            remain = remain & ~(ALIGN - 1);
            if (remain < MIN_BLOCK_SIZE) {
                /* Can't create tail block, just shrink main block */
                b->flags = 0;
                return;
            }
        }
        /* Create a tail free block */
        BlockHeader* tail = (BlockHeader*)((u8*)b + need);
        tail->size = remain;
        tail->flags = BF_FREE;
        tail->prevSize = need;
        tail->masterPtr = NULL;

        /* Fix next block's prevSize */
        u8* after = (u8*)tail + tail->size;
        if (after < z->limit) {
            BlockHeader* nxt = (BlockHeader*)after;
            nxt->prevSize = tail->size;
        }

        /* Shrink b */
        b->size = need;
        b->flags = 0;

        freelist_insert(z, tail);
    } else {
        b->flags = 0;
    }
}

/* ======================== Ptr Operations ======================== */

void* NewPtr(u32 byteCount) {
    /* NO LOGGING AT ALL - serial_puts corrupts registers in bare-metal! */

    ZoneInfo* z = gCurrentZone;
    if (!z) {
        return NULL;
    }

    u32 need = align_up(byteCount + BLKHDR_SZ);

    /* Enforce minimum block size to ensure blocks can be freed */
    if (need < MIN_BLOCK_SIZE) {
        need = MIN_BLOCK_SIZE;
    }
    BlockHeader* b = find_fit(z, need);

    if (!b) {
        u32 compact_result = CompactMem(need);
        if (compact_result < need) {
            return NULL;
        }
        b = find_fit(z, need);
        if (!b) {
            return NULL;
        }
    }

    split_block(z, b, need);

    b->flags |= BF_PTR;
    b->masterPtr = NULL;
    z->bytesUsed += b->size;
    z->bytesFree -= b->size;

    void* result = (u8*)b + BLKHDR_SZ;
    return result;
}

void* NewPtrClear(u32 byteCount) {
    void* p = NewPtr(byteCount);
    if (p) memset(p, 0, byteCount);
    return p;
}

void DisposePtr(void* p) {
    extern void serial_puts(const char* str);

    serial_puts("[DISPOSE] ENTRY\n");

    if (!p) {
        serial_puts("[DISPOSE] Early return: p is NULL\n");
        return;
    }

    ZoneInfo* z = gCurrentZone;
    serial_puts("[DISPOSE] gCurrentZone read\n");

    if (!z) {
        serial_puts("[DISPOSE] Early return: gCurrentZone is NULL\n");
        return;
    }

    /* Validate freelist BEFORE disposal */
    if (!validate_freelist(z)) {
        serial_puts("[DISPOSE] ERROR: Freelist already corrupted before disposal!\n");
        /* Clear all freelists to prevent crashes */
        for (u32 i = 0; i < NUM_SIZE_CLASSES; i++) {
            z->freelists[i] = NULL;
        }
        return;
    }

    BlockHeader* b = (BlockHeader*)((u8*)p - BLKHDR_SZ);
    serial_puts("[DISPOSE] BlockHeader calculated\n");

    /* Validate the block being freed */
    if (!validate_block(z, b)) {
        serial_puts("[DISPOSE] ERROR: Invalid block being freed\n");
        return;
    }

    b->flags &= ~(BF_PTR);
    z->bytesUsed -= b->size;
    z->bytesFree += b->size;

    /* Coalesce and insert */
    serial_puts("[DISPOSE] Calling coalesce_forward\n");
    b = coalesce_forward(z, b);
    serial_puts("[DISPOSE] After coalesce_forward\n");

    serial_puts("[DISPOSE] Calling coalesce_backward\n");
    b = coalesce_backward(z, b);
    serial_puts("[DISPOSE] After coalesce_backward\n");

    /* Validate coalesced block */
    if (!validate_block(z, b)) {
        serial_puts("[DISPOSE] ERROR: Coalesced block invalid\n");
        return;
    }

    serial_puts("[DISPOSE] Calling freelist_insert\n");
    freelist_insert(z, b);

    /* Validate freelist AFTER insertion */
    if (!validate_freelist(z)) {
        serial_puts("[DISPOSE] ERROR: Freelist corrupted after freelist_insert!\n");
        /* Clear all freelists to prevent crashes */
        for (u32 i = 0; i < NUM_SIZE_CLASSES; i++) {
            z->freelists[i] = NULL;
        }
    }

    serial_puts("[DISPOSE] Complete\n");
}

u32 GetPtrSize(void* p) {
    if (!p) return 0;
    BlockHeader* b = (BlockHeader*)((u8*)p - BLKHDR_SZ);
    return b->size - BLKHDR_SZ;
}

/* ======================== Handle Operations ======================== */

static void** MP_Alloc(ZoneInfo* z) {
    /* Simple linear search for free master pointer */
    for (u32 i = 0; i < z->mpCount; i++) {
        if (z->mpBase[i] == NULL) {
            /* Reserve the slot */
            z->mpBase[i] = (void*)1;  /* Temporary marker */
            return &z->mpBase[i];
        }
    }
    return NULL;
}

static void MP_Free(ZoneInfo* z, void** mp) {
    if (mp >= z->mpBase && mp < z->mpBase + z->mpCount) {
        *mp = NULL;
    }
}

Handle NewHandle(u32 byteCount) {
    ZoneInfo* z = gCurrentZone;
    if (!z) return NULL;

    void** mp = MP_Alloc(z);
    if (!mp) return NULL;

    u32 need = align_up(byteCount + BLKHDR_SZ);

    /* Enforce minimum block size to ensure blocks can be freed */
    if (need < MIN_BLOCK_SIZE) {
        need = MIN_BLOCK_SIZE;
    }
    BlockHeader* b = find_fit(z, need);

    if (!b) {
        /* Try compaction */
        if (CompactMem(need) < need) {
            MP_Free(z, mp);
            return NULL;
        }
        b = find_fit(z, need);
        if (!b) {
            MP_Free(z, mp);
            return NULL;
        }
    }

    split_block(z, b, need);
    b->flags |= BF_HANDLE;
    b->masterPtr = (Handle)mp;  /* Store backpointer */
    *mp = (u8*)b + BLKHDR_SZ;    /* Master pointer points to data */
    z->bytesUsed += b->size;
    z->bytesFree -= b->size;

    return (Handle)mp;
}

Handle NewHandleClear(u32 byteCount) {
    Handle h = NewHandle(byteCount);
    if (h && *h) memset(*h, 0, byteCount);
    return h;
}

void DisposeHandle(Handle h) {
    if (!h || !*h) {
        if (h) *h = NULL;
        return;
    }

    ZoneInfo* z = gCurrentZone;
    if (!z) return;

    /* Validate freelist before disposal */
    if (!validate_freelist(z)) {
        for (u32 i = 0; i < NUM_SIZE_CLASSES; i++) {
            z->freelists[i] = NULL;
        }
        return;
    }

    u8* p = (u8*)*h;
    BlockHeader* b = (BlockHeader*)(p - BLKHDR_SZ);

    /* Validate block being freed */
    if (!validate_block(z, b)) {
        return;
    }

    /* Clear master pointer */
    *h = NULL;

    b->flags &= ~(BF_HANDLE | BF_LOCKED | BF_PURGEABLE);
    b->masterPtr = NULL;
    z->bytesUsed -= b->size;
    z->bytesFree += b->size;

    b = coalesce_forward(z, b);
    b = coalesce_backward(z, b);

    /* Validate after coalescing */
    if (!validate_block(z, b)) {
        return;
    }

    freelist_insert(z, b);

    /* Validate freelist after insertion */
    if (!validate_freelist(z)) {
        for (u32 i = 0; i < NUM_SIZE_CLASSES; i++) {
            z->freelists[i] = NULL;
        }
    }
}

void HLock(Handle h) {
    if (h && *h) {
        BlockHeader* b = (BlockHeader*)((u8*)*h - BLKHDR_SZ);
        b->flags |= BF_LOCKED;
    }
}

void HUnlock(Handle h) {
    if (h && *h) {
        BlockHeader* b = (BlockHeader*)((u8*)*h - BLKHDR_SZ);
        b->flags &= ~BF_LOCKED;
    }
}

void HPurge(Handle h) {
    if (!h || !*h) return;
    BlockHeader* b = (BlockHeader*)((u8*)*h - BLKHDR_SZ);
    b->flags |= BF_PURGEABLE;
}

void HNoPurge(Handle h) {
    if (!h || !*h) return;
    BlockHeader* b = (BlockHeader*)((u8*)*h - BLKHDR_SZ);
    b->flags &= ~BF_PURGEABLE;
}

u32 GetHandleSize(Handle h) {
    if (!h || !*h) return 0;
    BlockHeader* b = (BlockHeader*)((u8*)*h - BLKHDR_SZ);
    return b->size - BLKHDR_SZ;
}

bool SetHandleSize_MemMgr(Handle h, u32 newSize) {
    if (!h || !*h) return false;

    ZoneInfo* z = gCurrentZone;
    if (!z) return false;

    /* Get current block */
    u8* p = (u8*)*h;
    BlockHeader* b = (BlockHeader*)(p - BLKHDR_SZ);
    u32 oldSize = b->size - BLKHDR_SZ;
    u32 newTotalSize = align_up(newSize + BLKHDR_SZ);

    /* If handle is locked, we cannot move it */
    if (b->flags & BF_LOCKED) {
        /* Can only shrink or grow in place */
        if (newTotalSize <= b->size) {
            /* Shrinking - just update size */
            return true;
        }
        /* Cannot grow locked handle */
        return false;
    }

    /* If new size fits in current block (with some slack), keep it */
    if (newTotalSize <= b->size && b->size - newTotalSize < 64) {
        /* Size fits, no need to reallocate */
        return true;
    }

    /* Need to allocate new block */
    Handle newHandle = NewHandle(newSize);
    if (!newHandle || !*newHandle) {
        return false;
    }

    /* Copy data (minimum of old and new size) */
    u32 copySize = (oldSize < newSize) ? oldSize : newSize;
    if (copySize > 0) {
        memcpy(*newHandle, *h, copySize);
    }

    /* Get new block header */
    BlockHeader* newBlock = (BlockHeader*)((u8*)*newHandle - BLKHDR_SZ);

    /* Update master pointer to point to new data */
    Handle masterPtr = b->masterPtr;
    *masterPtr = *newHandle;

    /* Copy flags from old block to new block */
    newBlock->flags = b->flags;
    newBlock->masterPtr = masterPtr;

    /* Free old block (but don't touch master pointer) */
    b->flags &= ~(BF_HANDLE | BF_LOCKED | BF_PURGEABLE);
    b->masterPtr = NULL;
    z->bytesUsed -= b->size;
    z->bytesFree += b->size;

    b = coalesce_forward(z, b);
    b = coalesce_backward(z, b);
    freelist_insert(z, b);

    /* Free the temporary master pointer from NewHandle */
    MP_Free(z, newHandle);

    return true;
}

/* ======================== Compaction ======================== */

u32 CompactMem(u32 cbNeeded) {
    /* LOGGING DISABLED - CAUSES FREEZE */
    // MEMORY_LOG_DEBUG("[CompactMem] ENTER: cbNeeded=%u\n", cbNeeded);

    ZoneInfo* z = gCurrentZone;
    if (!z) {
        // MEMORY_LOG_DEBUG("[CompactMem] FAIL: no current zone\n");
        return 0;
    }

    // MEMORY_LOG_DEBUG("[CompactMem] Zone state before: bytesUsed=%u bytesFree=%u\n", z->bytesUsed, z->bytesFree);

    /* First, try purging */
    // MEMORY_LOG_DEBUG("[CompactMem] Calling PurgeMem...\n");
    PurgeMem(cbNeeded);
    // MEMORY_LOG_DEBUG("[CompactMem] PurgeMem complete\n");

    /* Then compact: move unlocked handles together */
    u8* scan = z->base;
    u8* dest = z->base;

    // MEMORY_LOG_DEBUG("[CompactMem] Starting heap walk from %p to %p\n", scan, z->limit);
    int block_count = 0;

    while (scan < z->limit) {
        BlockHeader* b = (BlockHeader*)scan;
        block_count++;

        if (block_count % 100 == 0) {
            // MEMORY_LOG_DEBUG("[CompactMem] Processed %d blocks, scan=%p\n", block_count, scan);
        }

        /* Safety check: detect corrupted block size */
        if (b->size == 0 || b->size > (u32)(z->limit - scan)) {
            // MEMORY_LOG_DEBUG("[CompactMem] ERROR: corrupted block at %p: size=%u (remaining=%u)\n",
            //              b, b->size, (u32)(z->limit - scan));
            break;
        }

        if (b->flags & BF_FREE) {
            scan += b->size;
            continue;
        }

        /* Can we move this block? */
        if ((b->flags & BF_HANDLE) && !(b->flags & BF_LOCKED)) {
            if (scan != dest) {
                /* Move the block */
                BlockHeader* d = (BlockHeader*)dest;
                u32 dataSize = b->size - BLKHDR_SZ;

                /* Copy block header and data */
                memcpy(d, b, b->size);

                /* Update master pointer */
                if (d->masterPtr && *(d->masterPtr)) {
                    *(d->masterPtr) = (u8*)d + BLKHDR_SZ;
                }

                /* Update prevSize of following block */
                u8* after = dest + d->size;
                if (after < z->limit) {
                    BlockHeader* nxt = (BlockHeader*)after;
                    nxt->prevSize = d->size;
                }

                dest += d->size;
                scan += b->size;
                continue;
            }
        }

        /* Non-movable or locked - skip any gap and continue */
        /* CRITICAL FIX: Use MIN_BLOCK_SIZE instead of BLKHDR_SZ + ALIGN */
        u32 gap = scan - dest;
        if (scan != dest && gap >= MIN_BLOCK_SIZE) {
            /* CRITICAL: Align gap DOWN to avoid overflow! */
            u32 aligned_gap = gap & ~(ALIGN - 1);
            if (aligned_gap >= MIN_BLOCK_SIZE) {
                /* Create free block in the gap */
                BlockHeader* fb = (BlockHeader*)dest;
                fb->size = aligned_gap;
                fb->flags = BF_FREE;
                fb->prevSize = (dest > z->base) ?
                    ((BlockHeader*)(dest - ((BlockHeader*)(scan))->prevSize))->size : 0;
                fb->masterPtr = NULL;
                freelist_insert(z, fb);
            }
        }

        dest = scan + b->size;
        scan = dest;
    }

    /* Create trailing free block if needed */
    if (dest < z->limit) {
        u32 remaining = z->limit - dest;
        /* CRITICAL: Align remaining DOWN to avoid exceeding zone limit! */
        u32 aligned_remaining = remaining & ~(ALIGN - 1);
        if (aligned_remaining >= MIN_BLOCK_SIZE) {
            BlockHeader* tail = (BlockHeader*)dest;
            tail->flags = BF_FREE;
            tail->size = aligned_remaining;
            tail->prevSize = dest > z->base ?
                ((BlockHeader*)(dest - BLKHDR_SZ))->size : 0;
            tail->masterPtr = NULL;
            freelist_insert(z, tail);
        }
    }

    // MEMORY_LOG_DEBUG("[CompactMem] Heap walk complete: processed %d blocks\n", block_count);
    u32 max_free = MaxMem();
    // MEMORY_LOG_DEBUG("[CompactMem] SUCCESS: MaxMem=%u\n", max_free);
    return max_free;
}

void PurgeMem(u32 cbNeeded) {
    ZoneInfo* z = gCurrentZone;
    if (!z) return;

    /* Walk heap looking for purgeable handles */
    u8* scan = z->base;
    while (scan < z->limit) {
        BlockHeader* b = (BlockHeader*)scan;

        if ((b->flags & BF_HANDLE) &&
            (b->flags & BF_PURGEABLE) &&
            !(b->flags & BF_LOCKED)) {

            /* Purge this handle */
            Handle h = b->masterPtr;
            if (h) {
                *h = NULL;  /* Clear master pointer */
            }

            /* Free the block */
            b->flags = BF_FREE;
            z->bytesUsed -= b->size;
            z->bytesFree += b->size;

            /* Coalesce */
            b = coalesce_forward(z, b);
            b = coalesce_backward(z, b);
            freelist_insert(z, b);

            /* Check if we have enough */
            if (MaxMem() >= cbNeeded) return;
        }

        scan += b->size;
    }
}

/* ======================== C Library Interface ======================== */

void* malloc(size_t size) {
    /* NO LOGGING - serial_puts corrupts registers! */
    return NewPtr((u32)size);
}

void free(void* ptr) {
    extern void serial_puts(const char* str);
    serial_puts("[FREE] ENTRY\n");
    DisposePtr(ptr);
    serial_puts("[FREE] Complete\n");
}

void* calloc(size_t nmemb, size_t size) {
    /* NO LOGGING - serial_puts corrupts registers! */
    size_t total = nmemb * size;
    void* p = malloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    /* Get old size */
    BlockHeader* b = (BlockHeader*)((u8*)ptr - BLKHDR_SZ);
    u32 oldSize = b->size - BLKHDR_SZ;

    /* Allocate new block */
    void* newPtr = malloc(size);
    if (!newPtr) return NULL;

    /* Copy data */
    memcpy(newPtr, ptr, (size < oldSize) ? size : oldSize);

    /* Free old block */
    free(ptr);

    return newPtr;
}

/* ======================== Initialization ======================== */

void InitMemoryManager(void) {
    /* Use serial_puts for debugging */
    extern void serial_puts(const char* str);
    extern void* kmalloc(size_t size);
    extern uint32_t g_total_memory_kb;

    serial_puts("MM: InitMemoryManager started\n");

    /* Allocate heaps from nanokernel */
    const size_t system_size = 2 * 1024 * 1024;  /* 2MB system heap */
    const size_t app_size = 6 * 1024 * 1024;     /* 6MB app heap */

    gSystemHeap = (u8*)kmalloc(system_size);
    if (!gSystemHeap) {
        serial_puts("MM: FATAL - Failed to allocate system heap!\n");
        return;
    }

    gAppHeap = (u8*)kmalloc(app_size);
    if (!gAppHeap) {
        serial_puts("MM: FATAL - Failed to allocate app heap!\n");
        extern void kfree(void* ptr);
        kfree(gSystemHeap);
        gSystemHeap = NULL;
        return;
    }

    /* Initialize System Zone */
    InitZone(&gSystemZone, gSystemHeap, system_size,
             gSystemMasters, sizeof(gSystemMasters)/sizeof(void*));
    /* strcpy not available in kernel */
    gSystemZone.name[0] = 'S'; gSystemZone.name[1] = 0;
    serial_puts("MM: System Zone initialized (2048 KB from nanokernel)\n");

    /* Initialize Application Zone */
    InitZone(&gAppZone, gAppHeap, app_size,
             gAppMasters, sizeof(gAppMasters)/sizeof(void*));
    /* strcpy not available in kernel */
    gAppZone.name[0] = 'A'; gAppZone.name[1] = 0;
    serial_puts("MM: App Zone initialized (6144 KB from nanokernel)\n");

    /* Set current zone to app zone */
    gCurrentZone = &gAppZone;

    serial_puts("MM: Current zone set to App Zone\n");

    /* Report detected memory (comes from multiboot2) */
    MEMORY_LOG_DEBUG("MM: Total memory: %u KB (%u MB)\n",
                 g_total_memory_kb, g_total_memory_kb / 1024);

    serial_puts("MM: InitMemoryManager complete\n");
}

/* ======================== M68K Mapping ======================== */

static bool pointer_in_range(const void* ptr, const void* start, size_t len) {
    const u8* p = (const u8*)ptr;
    const u8* s = (const u8*)start;
    return p >= s && p < (s + len);
}

OSErr MemoryManager_MapToM68K(struct M68KAddressSpace* as)
{
    const UInt32 kSysBase = 0x0010000;  /* 0x10000 */
    const UInt32 kAppBase = 0x0220000;  /* 0x220000 */

    if (!as) {
        return paramErr;
    }

    size_t sysSize = (size_t)(gSystemZone.limit - gSystemZone.base);
    size_t appSize = (size_t)(gAppZone.limit - gAppZone.base);

    if (sysSize == 0 || appSize == 0) {
        serial_puts("[MM] MemoryManager_MapToM68K: zones not initialized\n");
        return memFullErr;
    }

    for (size_t offset = 0; offset < sysSize; offset += M68K_PAGE_SIZE) {
        UInt32 addr = kSysBase + (UInt32)offset;
        UInt32 page = addr >> M68K_PAGE_SHIFT;
        as->pageTable[page] = gSystemZone.base + offset;
    }

    for (size_t offset = 0; offset < appSize; offset += M68K_PAGE_SIZE) {
        UInt32 addr = kAppBase + (UInt32)offset;
        UInt32 page = addr >> M68K_PAGE_SHIFT;
        as->pageTable[page] = gAppZone.base + offset;
    }

    gSystemZone.m68kBase = kSysBase;
    gSystemZone.m68kLimit = kSysBase + (UInt32)sysSize;
    gAppZone.m68kBase = kAppBase;
    gAppZone.m68kLimit = kAppBase + (UInt32)appSize;

    serial_printf("[MM] System zone mapped: 0x%08X-0x%08X (%zu bytes)\n",
                  gSystemZone.m68kBase, gSystemZone.m68kLimit - 1, sysSize);
    serial_printf("[MM] Application zone mapped: 0x%08X-0x%08X (%zu bytes)\n",
                  gAppZone.m68kBase, gAppZone.m68kLimit - 1, appSize);

    MemoryManager_SyncLowMemGlobals();
    return noErr;
}

void MemoryManager_SyncLowMemGlobals(void)
{
    if (gSystemZone.m68kBase == 0 || gAppZone.m68kBase == 0) {
        serial_puts("[MM] MemoryManager_SyncLowMemGlobals skipped (zones unmapped)\n");
        return;
    }

    LMSetMemTop(gAppZone.m68kLimit);
    LMSetSysZone(gSystemZone.m68kBase);
    LMSetApplZone(gAppZone.m68kBase);

    serial_printf("[MM] Low memory globals synced: MemTop=0x%08X SysZone=0x%08X ApplZone=0x%08X\n",
                  LMGetMemTop(), LMGetSysZone(), LMGetApplZone());
}

bool MemoryManager_IsHeapPointer(const void* p)
{
    if (!p) return false;
    /* Check if pointer is within system zone */
    if (gSystemZone.base && pointer_in_range(p, gSystemZone.base,
                                             gSystemZone.limit - gSystemZone.base)) {
        return true;
    }
    /* Check if pointer is within app zone */
    if (gAppZone.base && pointer_in_range(p, gAppZone.base,
                                          gAppZone.limit - gAppZone.base)) {
        return true;
    }
    return false;
}

/* ======================== Debug Support ======================== */

void CheckHeap(ZoneInfo* zone) {
    if (!zone) zone = gCurrentZone;
    if (!zone) return;

    u32 totalSize = 0;
    u32 freeSize = 0;
    u32 usedSize = 0;
    u32 blockCount = 0;

    u8* scan = zone->base;
    while (scan < zone->limit) {
        BlockHeader* b = (BlockHeader*)scan;
        blockCount++;

        if (b->flags & BF_FREE) {
            freeSize += b->size;
        } else {
            usedSize += b->size;
        }
        totalSize += b->size;

        scan += b->size;
    }

    MEMORY_LOG_DEBUG("Heap check: %u blocks, %u used, %u free, %u total\n",
                  blockCount, usedSize, freeSize, totalSize);
}

void DumpHeap(ZoneInfo* zone) {
    if (!zone) zone = gCurrentZone;
    if (!zone) return;

    MEMORY_LOG_DEBUG("=== Heap Dump: %s ===\n", zone->name);

    u8* scan = zone->base;
    while (scan < zone->limit) {
        BlockHeader* b = (BlockHeader*)scan;

        char* type = "????";
        if (b->flags & BF_FREE) type = "FREE";
        else if (b->flags & BF_PTR) type = "PTR ";
        else if (b->flags & BF_HANDLE) {
            if (b->flags & BF_LOCKED) {
                type = (b->flags & BF_PURGEABLE) ? "HLKP" : "HLOK";
            } else {
                type = (b->flags & BF_PURGEABLE) ? "HNDP" : "HNDL";
            }
        }

        MEMORY_LOG_DEBUG("  %08x: %s size=%5u prev=%5u",
                      (u32)scan, type, b->size, b->prevSize);

        if (b->flags & BF_HANDLE && b->masterPtr) {
            MEMORY_LOG_DEBUG(" mp=%08x", (u32)b->masterPtr);
            if (*b->masterPtr) {
                MEMORY_LOG_DEBUG(" *mp=%08x", (u32)*b->masterPtr);
            }
        }
        MEMORY_LOG_DEBUG("\n");

        scan += b->size;
        if (scan > zone->limit) {
            MEMORY_LOG_DEBUG("  ERROR: Block extends past zone limit!\n");
            break;
        }
    }

    MEMORY_LOG_DEBUG("=== End Heap Dump ===\n");
}
