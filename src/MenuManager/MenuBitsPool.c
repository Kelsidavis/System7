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
#include "Platform/Framebuffer.h"
#include "MemoryMgr/MemoryManager.h"
#include "MenuManager/MenuBitsPool.h"
#include "MenuManager/MenuBitsPrivate.h"
#include <string.h>

/*---------------------------------------------------------------------------
 * Pool Structure
 *---------------------------------------------------------------------------*/

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
/*---------------------------------------------------------------------------
 * Pool Initialization and Shutdown
 *---------------------------------------------------------------------------*/

/**
 * Initialize the menu bits pool with preallocated buffers
 */
OSErr MenuBitsPool_Init(SInt16 numBuffers, SInt32 bufferSize) {
    if (gMenuBitsPool.initialized) {
        return noErr;
    }

    if (numBuffers <= 0 || bufferSize <= 0) {
        return paramErr;
    }

    /* Validate parameters to prevent integer overflow */
    if (numBuffers > 1000 || bufferSize > 1024 * 1024) {
        return paramErr;
    }

    /* Check for integer overflow in allocation size */
    if ((size_t)numBuffers > SIZE_MAX / sizeof(PoolEntry)) {
        return memFullErr;
    }

    /* Allocate pool entry array */
    gMenuBitsPool.entries = (PoolEntry*)NewPtr(numBuffers * sizeof(PoolEntry));
    if (!gMenuBitsPool.entries) {
        return memFullErr;
    }

    /* Initialize each pool entry */
    for (SInt16 i = 0; i < numBuffers; i++) {
        PoolEntry* entry = &gMenuBitsPool.entries[i];

        /* Allocate pixel buffer */
        entry->pixelBuffer = (void*)NewPtr(bufferSize);
        if (!entry->pixelBuffer) {
            /* Free previously allocated buffers */
            for (SInt16 j = 0; j < i; j++) {
                DisposePtr((Ptr)gMenuBitsPool.entries[j].pixelBuffer);
            }
            DisposePtr((Ptr)gMenuBitsPool.entries);
            gMenuBitsPool.entries = NULL;

            return memFullErr;
        }

        /* Initialize entry */
        entry->inUse = false;
        entry->owningHandle = NULL;
    }

    gMenuBitsPool.numEntries = numBuffers;
    gMenuBitsPool.bufferSize = bufferSize;
    gMenuBitsPool.initialized = true;

    return noErr;
}

/**
 * Shutdown the pool and free all resources
 */
OSErr MenuBitsPool_Shutdown(void) {
    if (!gMenuBitsPool.initialized) {
        return noErr;
    }

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

            return entry->pixelBuffer;
        }
    }

    /* No available buffers */
    return NULL;
}

/**
 * Allocate a buffer from the pool (returns pixel buffer pointer, not a handle)
 * Caller must create SavedBitsRec with NewHandle, with bitsData pointing to returned buffer
 */
Handle MenuBitsPool_Allocate(const Rect* bounds) {
    SInt16 poolIndex = -1;

    if (!gMenuBitsPool.initialized) {
        return NULL;
    }

    if (!bounds) {
        return NULL;
    }

    /* A rectangle larger than a pool buffer is left to the caller's heap
     * path; the pixels are copied in without a bound, so it must fit. */
    SInt32 w = bounds->right - bounds->left, h = bounds->bottom - bounds->top;
    if (w <= 0 || h <= 0 || (SInt64)w * h * 4 > gMenuBitsPool.bufferSize) {
        return NULL;
    }

    /* Get a pool buffer */
    void* pixelBuffer = MenuBitsPool_GetBuffer(&poolIndex);
    if (!pixelBuffer) {
        return NULL;
    }

    /* Create a proper SavedBitsRec handle in the heap */
    SavedBitsHandle handle = (SavedBitsHandle)NewHandle(sizeof(SavedBitsRec));
    if (!handle) {
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

    return (Handle)handle;
}

/**
 * Return a buffer to the pool
 */
OSErr MenuBitsPool_Free(Handle poolHandle) {
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

            found = true;
            break;
        }
    }

    HUnlock(poolHandle);

    /* Dispose the SavedBitsRec handle (proper heap management) */
    DisposeHandle(poolHandle);

    if (!found) {
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
