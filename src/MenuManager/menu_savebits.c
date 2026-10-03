#include "SystemTypes.h"
#include "SystemInternal.h"
#include "Platform/Framebuffer.h"
#include "MenuManager/menu_private.h"
#include "MenuManager/MenuDisplay.h"
#include "MenuManager/MenuBitsPool.h"
#include "MenuManager/MenuBitsPrivate.h"
#include "MemoryMgr/MemoryManager.h"

/* External framebuffer access */
static void CopyFramebufferToBuffer(const Rect *bounds, uint32_t *savePtr)
{
    const int width = bounds->right - bounds->left;
    const int height = bounds->bottom - bounds->top;
    const int pitch = fb_pitch / 4;
    uint32_t *fb = (uint32_t*)framebuffer;
    int bufferIndex = 0;

    for (int y = 0; y < height; y++) {
        const int screenY = bounds->top + y;
        for (int x = 0; x < width; x++) {
            const int screenX = bounds->left + x;
            if (screenX < 0 || screenX >= (int)fb_width ||
                screenY < 0 || screenY >= (int)fb_height) {
                savePtr[bufferIndex++] = 0xFF000000;
            } else {
                savePtr[bufferIndex++] = fb[screenY * pitch + screenX];
            }
        }
    }
}

/*
 * SaveBits - Save screen bits for menu display
 *
 * Tries the menu buffer pool before allocating a buffer from the heap.
 */
Handle SaveBits(const Rect *bounds, SInt16 mode) {
    if (!bounds || !framebuffer) {
        return NULL;
    }

    /* Calculate rectangle dimensions */
    SInt16 width = bounds->right - bounds->left;
    SInt16 height = bounds->bottom - bounds->top;

    if (width <= 0 || height <= 0) {
        return NULL;
    }

    /*
     * TRY POOL FIRST - This prevents heap fragmentation!
     * If pool has available buffer, use it instead of dynamic allocation
     */
    Handle poolBits = MenuBitsPool_Allocate(bounds);
    if (poolBits) {
        HLock(poolBits);
        SavedBitsPtr savedBits = *((SavedBitsHandle)poolBits);

        savedBits->mode = mode;
        savedBits->valid = false;
        savedBits->fromPool = true;  /* Mark as from pool */

        CopyFramebufferToBuffer(bounds, (uint32_t*)savedBits->bitsData);

        savedBits->valid = true;
        HUnlock(poolBits);
        return poolBits;
    }

    /* FALLBACK: Allocate handle for saved bits record */
    SavedBitsHandle bitsHandle = (SavedBitsHandle)NewHandle(sizeof(SavedBitsRec));
    if (!bitsHandle) {
        return NULL;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)bitsHandle);
    SavedBitsPtr savedBits = *bitsHandle;

    /* Store bounds and mode */
    savedBits->bounds = *bounds;
    savedBits->mode = mode;
    savedBits->fromPool = false;  /* Mark as NOT from pool */

    /* Calculate data size (32 bits per pixel = 4 bytes) */
    /* Check for integer overflow in size calculation */
    if (width > 0x7FFFFFFF / height / 4) {
        HUnlock((Handle)bitsHandle);
        DisposeHandle((Handle)bitsHandle);
        return NULL;
    }
    savedBits->dataSize = width * height * 4;

    /* CRITICAL: Allocate memory for pixel data using Memory Manager (not malloc!) */
    savedBits->bitsData = (void*)NewPtr(savedBits->dataSize);
    if (!savedBits->bitsData) {
        HUnlock((Handle)bitsHandle);
        DisposeHandle((Handle)bitsHandle);
        return NULL;
    }

    CopyFramebufferToBuffer(bounds, (uint32_t*)savedBits->bitsData);

    savedBits->valid = true;

    /* Unlock handle before returning */
    HUnlock((Handle)bitsHandle);

    return (Handle)bitsHandle;
}

/*
 * RestoreBits - Restore saved screen bits
 */
OSErr RestoreBits(Handle bitsHandle) {
    if (!bitsHandle || !*bitsHandle || !framebuffer) {
        return paramErr;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock(bitsHandle);
    SavedBitsPtr savedBits = (SavedBitsPtr)HandleDataAligned(bitsHandle);
    if (!savedBits) {
        HUnlock(bitsHandle);
        DisposeHandle(bitsHandle);
        return paramErr;
    }

    if (!savedBits->valid || !savedBits->bitsData) {
        HUnlock(bitsHandle);
        return paramErr;
    }

    /* Restore pixels from save buffer to framebuffer */
    /* CRITICAL: Use separate index for buffer to match SaveBits sequential write */
    {
        uint32_t* fb = (uint32_t*)framebuffer;
        uint32_t* savePtr = (uint32_t*)savedBits->bitsData;
        int pitch = fb_pitch / 4;
        SInt16 width = savedBits->bounds.right - savedBits->bounds.left;
        SInt16 height = savedBits->bounds.bottom - savedBits->bounds.top;
        int y, x;
        int bufferIndex = 0;
        Pointer_Shield(savedBits->bounds.left, savedBits->bounds.top,
                       savedBits->bounds.right, savedBits->bounds.bottom);

        for (y = 0; y < height; y++) {
            int screenY = savedBits->bounds.top + y;
            /* If row is out of bounds, skip the entire row in buffer */
            if (screenY < 0 || screenY >= (int)fb_height) {
                bufferIndex += width; /* Skip this row in buffer */
                continue;
            }

            for (x = 0; x < width; x++) {
                int screenX = savedBits->bounds.left + x;
                /* Read from buffer sequentially, only write to screen if in bounds */
                uint32_t pixel = savePtr[bufferIndex++];
                if (screenX >= 0 && screenX < (int)fb_width) {
                    fb[screenY * pitch + screenX] = pixel;
                }
            }
        }
    }

    /* Unlock handle after use */
    HUnlock(bitsHandle);

    return noErr;
}

/*
 * DiscardBits - Discard saved screen bits without restoring
 *
 * UPDATED FOR POOL INTEGRATION:
 * Checks if buffer is from pool and returns it to pool if so.
 * Otherwise uses normal disposal for dynamically allocated buffers.
 */
OSErr DiscardBits(Handle bitsHandle) {
    if (!bitsHandle || !*bitsHandle) {
        return paramErr;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock(bitsHandle);
    SavedBitsPtr savedBits = (SavedBitsPtr)HandleDataAligned(bitsHandle);
    if (!savedBits) {
        HUnlock(bitsHandle);
        DisposeHandle(bitsHandle);
        return paramErr;
    }

    /* Return pooled buffers through the pool; free other pixel buffers here. */
    if (savedBits->fromPool) {
        HUnlock(bitsHandle);
        OSErr err = MenuBitsPool_Free(bitsHandle);
        return err;
    }

    /* Validate bitsData pointer before freeing */
    if (savedBits->bitsData) {
        /* Match DisposePtr to the NewPtr allocation used by SaveBits. */
        DisposePtr((Ptr)savedBits->bitsData);
        savedBits->bitsData = NULL;
    }

    /* Mark as invalid */
    savedBits->valid = false;

    /* Unlock handle before disposing */
    HUnlock(bitsHandle);

    /* The handle is invalid after it is disposed. */
    DisposeHandle(bitsHandle);

    return noErr;
}

/*
 * SaveRestoreBitsDispatch - Dispatcher for SaveRestoreBits trap
 */
OSErr SaveRestoreBitsDispatch(SInt16 selector, void *params) {
    switch (selector) {
        case selectSaveBits: {
            /* Parameters: bounds (Rect*), mode (SInt16) */
            struct {
                const Rect *bounds;
                SInt16 mode;
                Handle *result;
            } *saveParams = params;

            if (!saveParams || !saveParams->bounds || !saveParams->result) {
                return paramErr;
            }

            *saveParams->result = SaveBits(saveParams->bounds, saveParams->mode);
            return (*saveParams->result) ? noErr : memFullErr;
        }

        case selectRestoreBits: {
            /* Parameters: bitsHandle (Handle) */
            struct {
                Handle bitsHandle;
            } *restoreParams = params;

            if (!restoreParams) {
                return paramErr;
            }

            return RestoreBits(restoreParams->bitsHandle);
        }

        case selectDiscardBits: {
            /* Parameters: bitsHandle (Handle) */
            struct {
                Handle bitsHandle;
            } *discardParams = params;

            if (!discardParams) {
                return paramErr;
            }

            return DiscardBits(discardParams->bitsHandle);
        }

        default:
            return -1; /* unimplemented */
    }
}

/*
 * Convenience wrapper functions for menu system use
 */

/*
 * SaveMenuBits - Save bits under a menu rectangle
 */
Handle SaveMenuBits(const Rect *menuRect) {
    if (!menuRect) {
        return NULL;
    }

    /* Save with default mode */
    return SaveBits(menuRect, 0);
}

/*
 * RestoreMenuBits - Restore bits under a menu
 */
OSErr RestoreMenuBits(Handle bitsHandle) {
    return RestoreBits(bitsHandle);
}

/*
 * DiscardMenuBits - Discard saved menu bits
 */
OSErr DiscardMenuBits(Handle bitsHandle) {
    return DiscardBits(bitsHandle);
}
