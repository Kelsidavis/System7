// #include "CompatibilityFix.h" // Removed
#include <stdlib.h>
#include <string.h>

/* Debug output disabled - was causing major slowdown on ARM64 */
#define SAVEBITS_DEBUG 0
#if SAVEBITS_DEBUG
#define SAVEBITS_LOG(msg) serial_puts(msg)
#else
#define SAVEBITS_LOG(msg) ((void)0)
#endif

/*
 * menu_savebits.c - Menu Manager Screen Bits Save/Restore
 *
 * Implementation of screen bits save/restore functions based on
 * Uses MenuBitsPool to prevent heap fragmentation from repeated
 * allocation/deallocation of large menu background buffers.
 *
 * RE-AGENT-BANNER: Extracted from Mac OS System 7.1 SaveRestoreBits trap
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "MenuManager/menu_private.h"
#include "MenuManager/MenuDisplay.h"
#include "MenuManager/MenuBitsPool.h"
#include "MemoryMgr/MemoryManager.h"


/* Screen bits handle structure */
typedef struct {
    Rect bounds;        /* Rectangle that was saved */
    SInt16 mode;       /* Save mode flags */
    void *bitsData;     /* Saved pixel data */
    SInt32 dataSize;   /* Size of saved data */
    Boolean valid;      /* Handle is valid */
    Boolean fromPool;   /* Buffer came from pool (not dynamically allocated) */
} SavedBitsRec, *SavedBitsPtr, **SavedBitsHandle;

/* External framebuffer access */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;

/*
 * SaveBits - Save screen bits for menu display
 *
 * INTEGRATED WITH MENU BITS POOL:
 * Tries to allocate from pool first to prevent heap fragmentation.
 * Falls back to dynamic allocation if pool unavailable.
 */
Handle SaveBits(const Rect *bounds, SInt16 mode) {
    extern void serial_puts(const char* str);
    char buf[256];

    SAVEBITS_LOG("[SAVEBITS] SaveBits: ENTRY\n");

    if (!bounds || !framebuffer) {
        SAVEBITS_LOG("[SAVEBITS] SaveBits: NULL bounds or framebuffer\n");
        return NULL;
    }

    /* Calculate rectangle dimensions */
    SInt16 width = bounds->right - bounds->left;
    SInt16 height = bounds->bottom - bounds->top;

    if (width <= 0 || height <= 0) {
        snprintf(buf, sizeof(buf), "[SAVEBITS] SaveBits: Invalid dimensions %dx%d\n", width, height);
        serial_puts(buf);
        return NULL;
    }

    /* Reduced debug output - was causing slowdown */
    extern void uart_flush(void);
    SAVEBITS_LOG("[SAVEBITS] Allocating...\n");
    uart_flush();

    /*
     * TRY POOL FIRST - This prevents heap fragmentation!
     * If pool has available buffer, use it instead of dynamic allocation
     */
    Handle poolBits = MenuBitsPool_Allocate(bounds);
    if (poolBits) {
        SAVEBITS_LOG("[SAVEBITS] Using pool\n");
        uart_flush();
        HLock(poolBits);
        SavedBitsPtr savedBits = *((SavedBitsHandle)poolBits);

        savedBits->mode = mode;
        savedBits->valid = false;
        savedBits->fromPool = true;  /* Mark as from pool */

        /* Copy pixels from framebuffer to pool buffer */
        {
            uint32_t* fb = (uint32_t*)framebuffer;
            uint32_t* savePtr = (uint32_t*)savedBits->bitsData;
            int pitch = fb_pitch / 4;
            int y, x;
            int bufferIndex = 0;

            for (y = 0; y < height; y++) {
                int screenY = bounds->top + y;
                if (screenY < 0 || screenY >= (int)fb_height) {
                    for (x = 0; x < width; x++) {
                        savePtr[bufferIndex++] = 0xFF000000;
                    }
                    continue;
                }

                for (x = 0; x < width; x++) {
                    int screenX = bounds->left + x;
                    if (screenX < 0 || screenX >= (int)fb_width) {
                        savePtr[bufferIndex++] = 0xFF000000;
                    } else {
                        savePtr[bufferIndex++] = fb[screenY * pitch + screenX];
                    }
                }
            }
        }

        savedBits->valid = true;
        HUnlock(poolBits);
        SAVEBITS_LOG("[SAVEBITS] SaveBits: Pooled buffer ready\n");
        return poolBits;
    }

    SAVEBITS_LOG("[SAVEBITS] SaveBits: Pool unavailable, using dynamic allocation\n");

    /* FALLBACK: Allocate handle for saved bits record */
    SavedBitsHandle bitsHandle = (SavedBitsHandle)NewHandle(sizeof(SavedBitsRec));
    if (!bitsHandle) {
        SAVEBITS_LOG("[SAVEBITS] SaveBits: NewHandle failed for SavedBitsRec\n");
        return NULL;
    }

    snprintf(buf, sizeof(buf), "[SAVEBITS] SaveBits: bitsHandle=%p *bitsHandle=%p\n",
            bitsHandle, *bitsHandle);
    serial_puts(buf);

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
        SAVEBITS_LOG("[SAVEBITS] SaveBits: Size calculation would overflow\n");
        HUnlock((Handle)bitsHandle);
        DisposeHandle((Handle)bitsHandle);
        return NULL;
    }
    savedBits->dataSize = width * height * 4;

    snprintf(buf, sizeof(buf), "[SAVEBITS] SaveBits: Allocating %d bytes for pixel data\n",
            savedBits->dataSize);
    serial_puts(buf);

    /* CRITICAL: Allocate memory for pixel data using Memory Manager (not malloc!) */
    savedBits->bitsData = (void*)NewPtr(savedBits->dataSize);
    if (!savedBits->bitsData) {
        SAVEBITS_LOG("[SAVEBITS] SaveBits: NewPtr failed for pixel data\n");
        HUnlock((Handle)bitsHandle);
        DisposeHandle((Handle)bitsHandle);
        return NULL;
    }

    snprintf(buf, sizeof(buf), "[SAVEBITS] SaveBits: bitsData=%p size=%d\n",
            savedBits->bitsData, savedBits->dataSize);
    serial_puts(buf);

    /* Copy pixels from framebuffer to save buffer */
    /* CRITICAL: Use separate index for buffer to prevent overflow when bounds are clipped */
    {
        uint32_t* fb = (uint32_t*)framebuffer;
        uint32_t* savePtr = (uint32_t*)savedBits->bitsData;
        int pitch = fb_pitch / 4;
        int y, x;
        int bufferIndex = 0;

        for (y = 0; y < height; y++) {
            int screenY = bounds->top + y;
            /* If row is out of bounds, fill with black/transparent */
            if (screenY < 0 || screenY >= (int)fb_height) {
                for (x = 0; x < width; x++) {
                    savePtr[bufferIndex++] = 0xFF000000; /* Black */
                }
                continue;
            }

            for (x = 0; x < width; x++) {
                int screenX = bounds->left + x;
                /* If pixel is out of bounds, use black/transparent */
                if (screenX < 0 || screenX >= (int)fb_width) {
                    savePtr[bufferIndex++] = 0xFF000000; /* Black */
                } else {
                    savePtr[bufferIndex++] = fb[screenY * pitch + screenX];
                }
            }
        }
    }

    savedBits->valid = true;

    snprintf(buf, sizeof(buf), "[SAVEBITS] SaveBits: Complete. Returning handle=%p bitsData=%p\n",
            bitsHandle, savedBits->bitsData);
    serial_puts(buf);

    /* Unlock handle before returning */
    HUnlock((Handle)bitsHandle);

    return (Handle)bitsHandle;
}

/*
 * RestoreBits - Restore saved screen bits
 */
OSErr RestoreBits(Handle bitsHandle) {
    extern void serial_logf(SystemLogModule module, SystemLogLevel level, const char* fmt, ...);

    serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[SAVEBITS] RestoreBits: ENTRY bitsHandle=%p\n", bitsHandle);

    if (!bitsHandle || !*bitsHandle || !framebuffer) {
        serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[SAVEBITS] RestoreBits: Invalid params\n");
        return paramErr;
    }

    serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[SAVEBITS] RestoreBits: *bitsHandle=%p\n", *bitsHandle);

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock(bitsHandle);
    SavedBitsPtr savedBits = (SavedBitsPtr)*bitsHandle;

    serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[SAVEBITS] RestoreBits: savedBits=%p valid=%d bitsData=%p\n",
               savedBits, savedBits->valid, savedBits->bitsData);

    if (!savedBits->valid || !savedBits->bitsData) {
        serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[SAVEBITS] RestoreBits: Invalid savedBits or bitsData\n");
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

    serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[SAVEBITS] RestoreBits: EXIT\n");

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
    extern void serial_puts(const char* str);
    char buf[256];

    SAVEBITS_LOG("[SAVEBITS] DiscardBits: ENTRY\n");
    snprintf(buf, sizeof(buf), "[SAVEBITS] DiscardBits: bitsHandle=%p\n", bitsHandle);
    serial_puts(buf);

    if (!bitsHandle || !*bitsHandle) {
        SAVEBITS_LOG("[SAVEBITS] DiscardBits: NULL handle, returning paramErr\n");
        return paramErr;
    }

    snprintf(buf, sizeof(buf), "[SAVEBITS] DiscardBits: *bitsHandle=%p\n", *bitsHandle);
    serial_puts(buf);

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock(bitsHandle);
    SavedBitsPtr savedBits = (SavedBitsPtr)*bitsHandle;

    snprintf(buf, sizeof(buf), "[SAVEBITS] DiscardBits: savedBits=%p valid=%d fromPool=%d bitsData=%p\n",
            savedBits, savedBits->valid, savedBits->fromPool, savedBits->bitsData);
    serial_puts(buf);

    /* CHECK IF FROM POOL */
    if (savedBits->fromPool) {
        /* Return to pool - much simpler cleanup */
        SAVEBITS_LOG("[SAVEBITS] DiscardBits: Returning buffer to pool\n");
        HUnlock(bitsHandle);
        OSErr err = MenuBitsPool_Free(bitsHandle);
        SAVEBITS_LOG("[SAVEBITS] DiscardBits: MenuBitsPool_Free completed\n");
        return err;
    }

    /* FALLBACK: Handle dynamically allocated buffer */
    SAVEBITS_LOG("[SAVEBITS] DiscardBits: Disposing dynamic allocation\n");

    /* Validate bitsData pointer before freeing */
    if (savedBits->bitsData) {
        snprintf(buf, sizeof(buf), "[SAVEBITS] DiscardBits: About to free bitsData=%p\n",
                savedBits->bitsData);
        serial_puts(buf);

        /* Read first few bytes for debugging */
        unsigned char* bytes = (unsigned char*)savedBits->bitsData;
        snprintf(buf, sizeof(buf), "[SAVEBITS] DiscardBits: bitsData first 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
        serial_puts(buf);

        /* CRITICAL: Use DisposePtr (not free!) since we allocated with NewPtr */
        DisposePtr((Ptr)savedBits->bitsData);
        SAVEBITS_LOG("[SAVEBITS] DiscardBits: DisposePtr() completed\n");
        savedBits->bitsData = NULL;
    }

    /* Mark as invalid */
    savedBits->valid = false;

    /* Unlock handle before disposing */
    HUnlock(bitsHandle);

    SAVEBITS_LOG("[SAVEBITS] DiscardBits: About to DisposeHandle\n");

    /* Dispose the handle - after this call, bitsHandle is INVALID */
    DisposeHandle(bitsHandle);

    /* IMPORTANT: bitsHandle is now invalid and must NOT be used.
     * Callers should NULL out their copy of the handle after this call. */

    SAVEBITS_LOG("[SAVEBITS] DiscardBits: DisposeHandle completed - handle now INVALID\n");
    SAVEBITS_LOG("[SAVEBITS] DiscardBits: EXIT\n");

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
