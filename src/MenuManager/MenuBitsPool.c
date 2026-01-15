/**
 * @file MenuBitsPool.c
 * @brief Menu Bits Memory Pool Implementation
 *
 * Pool-based buffer management for menu background save/restore operations.
 * Eliminates heap fragmentation from repeated 50KB+ allocations by reusing
 * a fixed set of preallocated buffers.
 *
 * Copyright (c) 2025 System 7.1 Portable Project
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include "MenuManager/MenuBitsPool.h"
#include <string.h>

/*---------------------------------------------------------------------------
 * Pool Structure
 *---------------------------------------------------------------------------*/

/* Saved bits record structure - matches what SaveBits uses */
typedef struct {
    Rect bounds;        /* Rectangle that was saved */
    SInt16 mode;        /* Save mode flags */
    void* bitsData;     /* Saved pixel data (preallocated from pool) */
    SInt32 dataSize;    /* Size of saved data */
    Boolean valid;      /* Handle is valid */
    Boolean fromPool;   /* This buffer came from the pool */
} SavedBitsRec, *SavedBitsPtr, **SavedBitsHandle;

/* Pool entry - tracks a preallocated buffer */
typedef struct {
    void* pixelBuffer;      /* Preallocated pixel data (fixed size) */
    Boolean inUse;          /* Currently borrowed */
    Handle owningHandle;    /* Handle that owns this buffer (for cleanup tracking) */
} PoolEntry;

/* Global pool state */
typedef struct {
    PoolEntry* entries;
    SInt16 numEntries;
    SInt32 bufferSize;      /* Size of each pixel buffer */
    Boolean initialized;
} MenuBitsPoolState;

static MenuBitsPoolState gMenuBitsPool = {0};

/* External framebuffer access */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;

/*---------------------------------------------------------------------------
 * Pool Initialization and Shutdown
 *---------------------------------------------------------------------------*/

/**
 * Initialize the menu bits pool with preallocated buffers
 */
OSErr MenuBitsPool_Init(SInt16 numBuffers, SInt32 bufferSize) {
    extern void serial_printf(const char* fmt, ...);
    extern void serial_puts(const char*);
    serial_puts("[MBPOOL] MenuBitsPool_Init enter\n");

    if (gMenuBitsPool.initialized) {
        serial_puts("[MBPOOL] Already initialized\n");
        return noErr;
    }
    serial_puts("[MBPOOL] Param check\n");

    if (numBuffers <= 0 || bufferSize <= 0) {
        serial_puts("[MBPOOL] Invalid params\n");
        return paramErr;
    }

    /* Validate parameters to prevent integer overflow */
    if (numBuffers > 1000 || bufferSize > 1024 * 1024) {
        serial_puts("[MBPOOL] Params too large\n");
        return paramErr;
    }
    serial_puts("[MBPOOL] Params OK\n");

    /* Check for integer overflow in allocation size */
    if (numBuffers > SIZE_MAX / sizeof(PoolEntry)) {
        serial_puts("[MBPOOL] Overflow\n");
        return memFullErr;
    }
    serial_puts("[MBPOOL] NewPtr entries\n");

    /* Allocate pool entry array */
    gMenuBitsPool.entries = (PoolEntry*)NewPtr(numBuffers * sizeof(PoolEntry));
    serial_puts("[MBPOOL] NewPtr entries done\n");
    if (!gMenuBitsPool.entries) {
        serial_puts("[MBPOOL] entries alloc failed\n");
        return memFullErr;
    }
    serial_puts("[MBPOOL] Starting buffer loop\n");

    /* Initialize each pool entry */
    for (SInt16 i = 0; i < numBuffers; i++) {
        serial_puts("[MBPOOL] Buffer alloc\n");
        PoolEntry* entry = &gMenuBitsPool.entries[i];

        /* Allocate pixel buffer */
        entry->pixelBuffer = (void*)NewPtr(bufferSize);
        if (!entry->pixelBuffer) {
            serial_puts("[MBPOOL] Buffer alloc failed\n");

            /* Free previously allocated buffers */
            for (SInt16 j = 0; j < i; j++) {
                DisposePtr((Ptr)gMenuBitsPool.entries[j].pixelBuffer);
            }
            DisposePtr((Ptr)gMenuBitsPool.entries);
            gMenuBitsPool.entries = NULL;

            return memFullErr;
        }
        serial_puts("[MBPOOL] Buffer OK\n");

        /* Initialize entry */
        entry->inUse = false;
        entry->owningHandle = NULL;
    }

    gMenuBitsPool.numEntries = numBuffers;
    gMenuBitsPool.bufferSize = bufferSize;
    gMenuBitsPool.initialized = true;

    serial_puts("[MBPOOL] Pool done\n");
    return noErr;
}

/**
 * Shutdown the pool and free all resources
 */
OSErr MenuBitsPool_Shutdown(void) {
    extern void serial_printf(const char* fmt, ...);

    if (!gMenuBitsPool.initialized) {
        return noErr;
    }

    serial_printf("[MBPOOL] Shutting down pool\n");

    if (gMenuBitsPool.entries) {
        for (SInt16 i = 0; i < gMenuBitsPool.numEntries; i++) {
            PoolEntry* entry = &gMenuBitsPool.entries[i];
            if (entry->pixelBuffer) {
                DisposePtr((Ptr)entry->pixelBuffer);
            }
        }
        DisposePtr((Ptr)gMenuBitsPool.entries);
        gMenuBitsPool.entries = NULL;
    }

    gMenuBitsPool.initialized = false;
    serial_printf("[MBPOOL] Pool shutdown complete\n");

    return noErr;
}

/*---------------------------------------------------------------------------
 * Pool Allocation and Deallocation
 *---------------------------------------------------------------------------*/

/**
 * Get pointer to available pool buffer (internal use)
 * Returns pointer to pixel buffer or NULL if none available
 */
static void* MenuBitsPool_GetBuffer(SInt16* outIndex) {
    extern void serial_printf(const char* fmt, ...);

    if (!gMenuBitsPool.initialized) {
        return NULL;
    }

    /* Find first available buffer */
    for (SInt16 i = 0; i < gMenuBitsPool.numEntries; i++) {
        PoolEntry* entry = &gMenuBitsPool.entries[i];

        if (!entry->inUse) {
            /* Found available buffer - mark as in use */
            entry->inUse = true;
            entry->owningHandle = NULL;  /* Will be set by SaveBits */

            if (outIndex) {
                *outIndex = i;
            }

            serial_printf("[MBPOOL] Got buffer %d at %p\n", i, entry->pixelBuffer);
            return entry->pixelBuffer;
        }
    }

    /* No available buffers */
    serial_printf("[MBPOOL] No available buffers (all %d in use)\n", gMenuBitsPool.numEntries);
    return NULL;
}

/**
 * Allocate a buffer from the pool (returns pixel buffer pointer, not a handle)
 * Caller must create SavedBitsRec with NewHandle, with bitsData pointing to returned buffer
 */
Handle MenuBitsPool_Allocate(const Rect* bounds) {
    extern void serial_printf(const char* fmt, ...);
    SInt16 poolIndex = -1;

    if (!gMenuBitsPool.initialized) {
        serial_printf("[MBPOOL] Pool not initialized\n");
        return NULL;
    }

    if (!bounds) {
        serial_printf("[MBPOOL] NULL bounds\n");
        return NULL;
    }

    /* Get a pool buffer */
    void* pixelBuffer = MenuBitsPool_GetBuffer(&poolIndex);
    if (!pixelBuffer) {
        serial_printf("[MBPOOL] No pool buffers available\n");
        return NULL;
    }

    /* Create a proper SavedBitsRec handle in the heap */
    SavedBitsHandle handle = (SavedBitsHandle)NewHandle(sizeof(SavedBitsRec));
    if (!handle) {
        serial_printf("[MBPOOL] Failed to allocate SavedBitsRec handle\n");
        gMenuBitsPool.entries[poolIndex].inUse = false;
        return NULL;
    }

    HLock((Handle)handle);
    SavedBitsPtr record = *handle;

    /* Initialize the record to point to pool buffer */
    record->bounds = *bounds;
    record->mode = 0;
    record->bitsData = pixelBuffer;
    record->dataSize = gMenuBitsPool.bufferSize;
    record->valid = false;  /* Will be set after data copied */
    record->fromPool = true;  /* Mark as from pool */

    /* Store pool index in the pool entry for later recovery */
    gMenuBitsPool.entries[poolIndex].owningHandle = (Handle)handle;

    HUnlock((Handle)handle);

    serial_printf("[MBPOOL] Allocated pool buffer %d, handle=%p\n", poolIndex, handle);
    return (Handle)handle;
}

/**
 * Return a buffer to the pool
 */
OSErr MenuBitsPool_Free(Handle poolHandle) {
    extern void serial_printf(const char* fmt, ...);

    if (!gMenuBitsPool.initialized || !poolHandle) {
        return paramErr;
    }

    HLock(poolHandle);
    SavedBitsPtr record = (SavedBitsPtr)*((SavedBitsHandle)poolHandle);

    /* Find the pool entry that owns this handle */
    Boolean found = false;
    for (SInt16 i = 0; i < gMenuBitsPool.numEntries; i++) {
        PoolEntry* entry = &gMenuBitsPool.entries[i];

        if (entry->owningHandle == poolHandle && entry->inUse && record->fromPool) {
            /* Found it - mark buffer as available (but don't free pixel data) */
            entry->inUse = false;
            entry->owningHandle = NULL;

            serial_printf("[MBPOOL] Freed buffer %d\n", i);
            found = true;
            break;
        }
    }

    HUnlock(poolHandle);

    /* Dispose the SavedBitsRec handle (proper heap management) */
    DisposeHandle(poolHandle);

    if (!found) {
        serial_printf("[MBPOOL] Warning: Freed handle didn't belong to pool\n");
        return paramErr;
    }

    return noErr;
}

/*---------------------------------------------------------------------------
 * Pool Statistics
 *---------------------------------------------------------------------------*/

/**
 * Get pool statistics
 */
Boolean MenuBitsPool_GetStats(SInt16* outTotal, SInt16* outInUse) {
    if (!gMenuBitsPool.initialized) {
        return false;
    }

    if (outTotal) {
        *outTotal = gMenuBitsPool.numEntries;
    }

    if (outInUse) {
        SInt16 inUse = 0;
        for (SInt16 i = 0; i < gMenuBitsPool.numEntries; i++) {
            if (gMenuBitsPool.entries[i].inUse) {
                inUse++;
            }
        }
        *outInUse = inUse;
    }

    return true;
}
