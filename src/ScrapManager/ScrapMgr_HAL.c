/**
 * Scrap Manager Hardware Abstraction Layer
 * Provides platform-independent implementation of system-wide clipboard functionality
 *
 * This HAL layer bridges the System 7.1 Scrap Manager with modern platforms,
 * providing complete clipboard support including multi-format data storage,
 * inter-application clipboard sharing, and persistent scrap files.
 */

#include "../../include/ScrapManager/scrap_manager.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include "../../include/FileManager/FileManager.h"
#include "../../include/ResourceManager/ResourceManager.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

/* Scrap file location */
#define SCRAP_FILE_NAME "System 7 Scrapbook"
#define SCRAP_TEMP_DIR "/tmp/"

/* Maximum scrap size (1MB default) */
#define MAX_SCRAP_SIZE (1024 * 1024)

/* Scrap Manager Global State */
typedef struct ScrapMgrState {
    bool initialized;

    /* In-memory scrap storage */
    struct ScrapEntry {
        OSType type;              /* Data type (e.g., 'TEXT', 'PICT') */
        Handle data;              /* Handle to data */
        int32_t size;            /* Size of data */
        struct ScrapEntry* next; /* Next entry in chain */
    } *scrapList;

    /* Scrap metadata */
    int32_t scrapCount;          /* Number of items in scrap */
    int32_t scrapSize;           /* Total size of scrap */
    int32_t scrapState;          /* Internal state counter */

    /* Scrap file information */
    char scrapFileName[256];     /* Path to scrap file */
    bool scrapInMemory;          /* Is scrap loaded in memory? */
    bool scrapDirty;             /* Has scrap been modified? */

    /* Platform-specific clipboard handles */
#ifdef __linux__
    Display* display;
    Window window;
    Atom clipboardAtom;
    Atom targetsAtom;
    Atom textAtom;
    Atom incrAtom;
#endif

#ifdef __APPLE__
    PasteboardRef pasteboard;
#endif

} ScrapMgrState;

static ScrapMgrState gScrapMgr = {0};

/* Forward declarations */
static OSErr ScrapMgr_HAL_SaveToFile(void);
static OSErr ScrapMgr_HAL_LoadFromFile(void);
static void ScrapMgr_HAL_ClearEntries(void);
static struct ScrapEntry* ScrapMgr_HAL_FindEntry(OSType theType);
static OSErr ScrapMgr_HAL_SyncWithSystem(void);

/**
 * Initialize Scrap Manager HAL
 */
void ScrapMgr_HAL_Init(void)
{
    if (gScrapMgr.initialized) {
        return;
    }

    printf("Scrap Manager HAL: Initializing clipboard system...\n");

    /* Set up scrap file path */
    snprintf(gScrapMgr.scrapFileName, sizeof(gScrapMgr.scrapFileName),
             "%s%s", SCRAP_TEMP_DIR, SCRAP_FILE_NAME);

    /* Initialize platform-specific clipboard */
#ifdef __linux__
    gScrapMgr.display = XOpenDisplay(NULL);
    if (gScrapMgr.display) {
        gScrapMgr.window = XCreateSimpleWindow(gScrapMgr.display,
                                               DefaultRootWindow(gScrapMgr.display),
                                               0, 0, 1, 1, 0, 0, 0);
        gScrapMgr.clipboardAtom = XInternAtom(gScrapMgr.display, "CLIPBOARD", False);
        gScrapMgr.targetsAtom = XInternAtom(gScrapMgr.display, "TARGETS", False);
        gScrapMgr.textAtom = XInternAtom(gScrapMgr.display, "UTF8_STRING", False);
        gScrapMgr.incrAtom = XInternAtom(gScrapMgr.display, "INCR", False);
    }
#endif

#ifdef __APPLE__
    OSStatus err = PasteboardCreate(kPasteboardClipboard, &gScrapMgr.pasteboard);
    if (err != noErr) {
        printf("Scrap Manager HAL: Failed to create pasteboard\n");
    }
#endif

    /* Initialize scrap state */
    gScrapMgr.scrapList = NULL;
    gScrapMgr.scrapCount = 0;
    gScrapMgr.scrapSize = 0;
    gScrapMgr.scrapState = 0;
    gScrapMgr.scrapInMemory = true;
    gScrapMgr.scrapDirty = false;

    /* Try to load existing scrap file */
    ScrapMgr_HAL_LoadFromFile();

    gScrapMgr.initialized = true;

    printf("Scrap Manager HAL: Clipboard system initialized\n");
}

/**
 * Zero the scrap (clear clipboard)
 */
OSErr ScrapMgr_HAL_ZeroScrap(void)
{
    if (!gScrapMgr.initialized) {
        ScrapMgr_HAL_Init();
    }

    /* Clear all entries */
    ScrapMgr_HAL_ClearEntries();

    /* Reset state */
    gScrapMgr.scrapCount = 0;
    gScrapMgr.scrapSize = 0;
    gScrapMgr.scrapState++;
    gScrapMgr.scrapDirty = true;

    /* Clear system clipboard */
#ifdef __linux__
    if (gScrapMgr.display) {
        XSetSelectionOwner(gScrapMgr.display, gScrapMgr.clipboardAtom,
                          None, CurrentTime);
        XFlush(gScrapMgr.display);
    }
#endif

#ifdef __APPLE__
    if (gScrapMgr.pasteboard) {
        PasteboardClear(gScrapMgr.pasteboard);
    }
#endif

    return noErr;
}

/**
 * Get information about the scrap
 */
PScrapStuff ScrapMgr_HAL_InfoScrap(void)
{
    static ScrapStuff scrapInfo;

    if (!gScrapMgr.initialized) {
        ScrapMgr_HAL_Init();
    }

    /* Fill in scrap information */
    scrapInfo.scrapSize = gScrapMgr.scrapSize;
    scrapInfo.scrapHandle = NULL;  /* Not used in modern implementation */
    scrapInfo.scrapCount = gScrapMgr.scrapCount;
    scrapInfo.scrapState = gScrapMgr.scrapState;

    /* Set file name as Pascal string */
    int len = strlen(gScrapMgr.scrapFileName);
    if (len > 255) len = 255;
    scrapInfo.scrapName[0] = len;
    memcpy(&scrapInfo.scrapName[1], gScrapMgr.scrapFileName, len);

    return &scrapInfo;
}

/**
 * Put data into the scrap
 */
OSErr ScrapMgr_HAL_PutScrap(int32_t length, OSType theType, void* source)
{
    if (!gScrapMgr.initialized) {
        ScrapMgr_HAL_Init();
    }

    if (!source || length <= 0) {
        return paramErr;
    }

    if (gScrapMgr.scrapSize + length > MAX_SCRAP_SIZE) {
        return memFullErr;
    }

    /* Find existing entry of same type */
    struct ScrapEntry* entry = ScrapMgr_HAL_FindEntry(theType);

    if (entry) {
        /* Replace existing entry */
        SetHandleSize(entry->data, length);
        if (MemError() != noErr) {
            return memFullErr;
        }

        /* Update total size */
        gScrapMgr.scrapSize -= entry->size;
        gScrapMgr.scrapSize += length;
        entry->size = length;
    } else {
        /* Create new entry */
        entry = (struct ScrapEntry*)NewPtr(sizeof(struct ScrapEntry));
        if (!entry) {
            return memFullErr;
        }

        entry->type = theType;
        entry->size = length;
        entry->data = NewHandle(length);
        if (!entry->data) {
            DisposePtr((Ptr)entry);
            return memFullErr;
        }

        /* Add to list */
        entry->next = gScrapMgr.scrapList;
        gScrapMgr.scrapList = entry;

        gScrapMgr.scrapCount++;
        gScrapMgr.scrapSize += length;
    }

    /* Copy data */
    HLock(entry->data);
    BlockMove(source, *entry->data, length);
    HUnlock(entry->data);

    gScrapMgr.scrapState++;
    gScrapMgr.scrapDirty = true;

    /* Update system clipboard for TEXT type */
    if (theType == 'TEXT') {
#ifdef __linux__
        if (gScrapMgr.display) {
            XSetSelectionOwner(gScrapMgr.display, gScrapMgr.clipboardAtom,
                              gScrapMgr.window, CurrentTime);
            XFlush(gScrapMgr.display);
        }
#endif

#ifdef __APPLE__
        if (gScrapMgr.pasteboard) {
            PasteboardClear(gScrapMgr.pasteboard);

            CFDataRef cfData = CFDataCreate(NULL, (UInt8*)source, length);
            if (cfData) {
                PasteboardPutItemFlavor(gScrapMgr.pasteboard,
                                       (PasteboardItemID)1,
                                       kUTTypeUTF8PlainText,
                                       cfData, 0);
                CFRelease(cfData);
            }
        }
#endif
    }

    return noErr;
}

/**
 * Get data from the scrap
 */
int32_t ScrapMgr_HAL_GetScrap(Handle hDest, OSType theType, int32_t* offset)
{
    if (!gScrapMgr.initialized) {
        ScrapMgr_HAL_Init();
    }

    /* Sync with system clipboard first */
    ScrapMgr_HAL_SyncWithSystem();

    /* Find entry of requested type */
    struct ScrapEntry* entry = ScrapMgr_HAL_FindEntry(theType);
    if (!entry) {
        return noTypeErr;
    }

    /* Set offset if requested */
    if (offset) {
        int32_t currentOffset = 0;
        struct ScrapEntry* e = gScrapMgr.scrapList;
        while (e && e != entry) {
            currentOffset += e->size;
            e = e->next;
        }
        *offset = currentOffset;
    }

    /* If no destination handle provided, just return size */
    if (!hDest) {
        return entry->size;
    }

    /* Resize destination handle */
    SetHandleSize(hDest, entry->size);
    if (MemError() != noErr) {
        return memFullErr;
    }

    /* Copy data */
    HLock(entry->data);
    HLock(hDest);
    BlockMove(*entry->data, *hDest, entry->size);
    HUnlock(hDest);
    HUnlock(entry->data);

    return entry->size;
}

/**
 * Load scrap from disk
 */
OSErr ScrapMgr_HAL_LoadScrap(void)
{
    if (!gScrapMgr.initialized) {
        ScrapMgr_HAL_Init();
    }

    if (gScrapMgr.scrapInMemory) {
        return noErr;  /* Already loaded */
    }

    OSErr err = ScrapMgr_HAL_LoadFromFile();
    if (err == noErr) {
        gScrapMgr.scrapInMemory = true;
    }

    return err;
}

/**
 * Unload scrap to disk
 */
OSErr ScrapMgr_HAL_UnloadScrap(void)
{
    if (!gScrapMgr.initialized) {
        ScrapMgr_HAL_Init();
    }

    if (!gScrapMgr.scrapInMemory) {
        return noErr;  /* Already unloaded */
    }

    if (gScrapMgr.scrapDirty) {
        OSErr err = ScrapMgr_HAL_SaveToFile();
        if (err != noErr) {
            return err;
        }
    }

    /* Clear in-memory entries but keep metadata */
    struct ScrapEntry* entry = gScrapMgr.scrapList;
    while (entry) {
        if (entry->data) {
            DisposeHandle(entry->data);
            entry->data = NULL;
        }
        entry = entry->next;
    }

    gScrapMgr.scrapInMemory = false;

    return noErr;
}

/* ===== Internal Helper Functions ===== */

/**
 * Clear all scrap entries
 */
static void ScrapMgr_HAL_ClearEntries(void)
{
    struct ScrapEntry* entry = gScrapMgr.scrapList;
    while (entry) {
        struct ScrapEntry* next = entry->next;
        if (entry->data) {
            DisposeHandle(entry->data);
        }
        DisposePtr((Ptr)entry);
        entry = next;
    }
    gScrapMgr.scrapList = NULL;
}

/**
 * Find entry by type
 */
static struct ScrapEntry* ScrapMgr_HAL_FindEntry(OSType theType)
{
    struct ScrapEntry* entry = gScrapMgr.scrapList;
    while (entry) {
        if (entry->type == theType) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * Save scrap to file
 */
static OSErr ScrapMgr_HAL_SaveToFile(void)
{
    FILE* file = fopen(gScrapMgr.scrapFileName, "wb");
    if (!file) {
        return ioErr;
    }

    /* Write header */
    int32_t magic = 'SCRP';
    fwrite(&magic, sizeof(int32_t), 1, file);
    fwrite(&gScrapMgr.scrapCount, sizeof(int32_t), 1, file);
    fwrite(&gScrapMgr.scrapSize, sizeof(int32_t), 1, file);

    /* Write each entry */
    struct ScrapEntry* entry = gScrapMgr.scrapList;
    while (entry) {
        fwrite(&entry->type, sizeof(OSType), 1, file);
        fwrite(&entry->size, sizeof(int32_t), 1, file);

        if (entry->data) {
            HLock(entry->data);
            fwrite(*entry->data, 1, entry->size, file);
            HUnlock(entry->data);
        }

        entry = entry->next;
    }

    fclose(file);
    gScrapMgr.scrapDirty = false;

    return noErr;
}

/**
 * Load scrap from file
 */
static OSErr ScrapMgr_HAL_LoadFromFile(void)
{
    FILE* file = fopen(gScrapMgr.scrapFileName, "rb");
    if (!file) {
        return fnfErr;  /* File not found */
    }

    /* Read header */
    int32_t magic;
    fread(&magic, sizeof(int32_t), 1, file);
    if (magic != 'SCRP') {
        fclose(file);
        return dataVerErr;
    }

    int32_t count, size;
    fread(&count, sizeof(int32_t), 1, file);
    fread(&size, sizeof(int32_t), 1, file);

    /* Clear existing entries */
    ScrapMgr_HAL_ClearEntries();

    /* Read each entry */
    for (int i = 0; i < count; i++) {
        OSType type;
        int32_t entrySize;

        if (fread(&type, sizeof(OSType), 1, file) != 1) break;
        if (fread(&entrySize, sizeof(int32_t), 1, file) != 1) break;

        /* Allocate entry */
        struct ScrapEntry* entry = (struct ScrapEntry*)NewPtr(sizeof(struct ScrapEntry));
        if (!entry) {
            fclose(file);
            return memFullErr;
        }

        entry->type = type;
        entry->size = entrySize;
        entry->data = NewHandle(entrySize);
        if (!entry->data) {
            DisposePtr((Ptr)entry);
            fclose(file);
            return memFullErr;
        }

        /* Read data */
        HLock(entry->data);
        fread(*entry->data, 1, entrySize, file);
        HUnlock(entry->data);

        /* Add to list */
        entry->next = gScrapMgr.scrapList;
        gScrapMgr.scrapList = entry;
    }

    gScrapMgr.scrapCount = count;
    gScrapMgr.scrapSize = size;
    gScrapMgr.scrapState++;
    gScrapMgr.scrapDirty = false;

    fclose(file);
    return noErr;
}

/**
 * Sync with system clipboard
 */
static OSErr ScrapMgr_HAL_SyncWithSystem(void)
{
#ifdef __linux__
    if (gScrapMgr.display) {
        /* Check if clipboard has changed */
        Window owner = XGetSelectionOwner(gScrapMgr.display, gScrapMgr.clipboardAtom);
        if (owner != None && owner != gScrapMgr.window) {
            /* Request clipboard contents */
            XConvertSelection(gScrapMgr.display, gScrapMgr.clipboardAtom,
                            gScrapMgr.textAtom, gScrapMgr.clipboardAtom,
                            gScrapMgr.window, CurrentTime);

            /* Wait for selection notify event */
            XEvent event;
            while (XCheckTypedWindowEvent(gScrapMgr.display, gScrapMgr.window,
                                        SelectionNotify, &event)) {
                if (event.xselection.property != None) {
                    Atom type;
                    int format;
                    unsigned long nitems, bytes_after;
                    unsigned char* data = NULL;

                    XGetWindowProperty(gScrapMgr.display, gScrapMgr.window,
                                     gScrapMgr.clipboardAtom, 0, ~0, False,
                                     AnyPropertyType, &type, &format,
                                     &nitems, &bytes_after, &data);

                    if (data) {
                        /* Add text to scrap */
                        ScrapMgr_HAL_PutScrap(nitems, 'TEXT', data);
                        XFree(data);
                    }
                }
            }
        }
    }
#endif

#ifdef __APPLE__
    if (gScrapMgr.pasteboard) {
        PasteboardSynchronize(gScrapMgr.pasteboard);

        ItemCount itemCount;
        OSStatus err = PasteboardGetItemCount(gScrapMgr.pasteboard, &itemCount);
        if (err == noErr && itemCount > 0) {
            PasteboardItemID itemID;
            err = PasteboardGetItemIdentifier(gScrapMgr.pasteboard, 1, &itemID);
            if (err == noErr) {
                CFDataRef data;
                err = PasteboardCopyItemFlavorData(gScrapMgr.pasteboard, itemID,
                                                  kUTTypeUTF8PlainText, &data);
                if (err == noErr && data) {
                    CFIndex length = CFDataGetLength(data);
                    const UInt8* bytes = CFDataGetBytePtr(data);

                    /* Add to scrap if different */
                    struct ScrapEntry* textEntry = ScrapMgr_HAL_FindEntry('TEXT');
                    if (!textEntry || textEntry->size != length ||
                        memcmp(*textEntry->data, bytes, length) != 0) {
                        ScrapMgr_HAL_PutScrap(length, 'TEXT', (void*)bytes);
                    }

                    CFRelease(data);
                }
            }
        }
    }
#endif

    return noErr;
}

/**
 * Convert between scrap types
 */
OSErr ScrapMgr_HAL_ConvertScrap(OSType fromType, OSType toType, Handle* result)
{
    if (!result) return paramErr;

    struct ScrapEntry* entry = ScrapMgr_HAL_FindEntry(fromType);
    if (!entry) return noTypeErr;

    /* Handle common conversions */
    if (fromType == 'TEXT' && toType == 'styl') {
        /* Convert plain text to styled text */
        /* For now, just create minimal style record */
        *result = NewHandle(16);
        if (!*result) return memFullErr;

        /* Simple style run with default formatting */
        HLock(*result);
        int16_t* styleData = (int16_t*)**result;
        styleData[0] = 1;  /* Number of style runs */
        styleData[1] = 0;  /* Start of run */
        styleData[2] = 0;  /* Style index */
        HUnlock(*result);

        return noErr;
    }

    return cantConvertScrapErr;
}

/**
 * Get list of available scrap types
 */
OSErr ScrapMgr_HAL_GetScrapTypes(OSType** types, int32_t* count)
{
    if (!types || !count) return paramErr;

    *count = gScrapMgr.scrapCount;
    if (gScrapMgr.scrapCount == 0) {
        *types = NULL;
        return noErr;
    }

    *types = (OSType*)NewPtr(sizeof(OSType) * gScrapMgr.scrapCount);
    if (!*types) return memFullErr;

    struct ScrapEntry* entry = gScrapMgr.scrapList;
    int i = 0;
    while (entry && i < gScrapMgr.scrapCount) {
        (*types)[i++] = entry->type;
        entry = entry->next;
    }

    return noErr;
}

#ifdef __linux__
/**
 * Handle X11 selection requests
 */
void ScrapMgr_HAL_HandleSelectionRequest(XSelectionRequestEvent* request)
{
    if (!gScrapMgr.display) return;

    XSelectionEvent response;
    response.type = SelectionNotify;
    response.requestor = request->requestor;
    response.selection = request->selection;
    response.target = request->target;
    response.time = request->time;
    response.property = None;

    if (request->target == gScrapMgr.targetsAtom) {
        /* Return list of supported targets */
        Atom targets[] = { gScrapMgr.textAtom, gScrapMgr.targetsAtom };
        XChangeProperty(gScrapMgr.display, request->requestor,
                       request->property, XA_ATOM, 32,
                       PropModeReplace, (unsigned char*)targets, 2);
        response.property = request->property;
    } else if (request->target == gScrapMgr.textAtom) {
        /* Return text data */
        struct ScrapEntry* entry = ScrapMgr_HAL_FindEntry('TEXT');
        if (entry && entry->data) {
            HLock(entry->data);
            XChangeProperty(gScrapMgr.display, request->requestor,
                          request->property, gScrapMgr.textAtom, 8,
                          PropModeReplace, (unsigned char*)*entry->data,
                          entry->size);
            HUnlock(entry->data);
            response.property = request->property;
        }
    }

    XSendEvent(gScrapMgr.display, request->requestor, False, 0,
              (XEvent*)&response);
    XFlush(gScrapMgr.display);
}
#endif

/**
 * Cleanup Scrap Manager HAL
 */
void ScrapMgr_HAL_Cleanup(void)
{
    if (!gScrapMgr.initialized) {
        return;
    }

    /* Save to disk if dirty */
    if (gScrapMgr.scrapDirty) {
        ScrapMgr_HAL_SaveToFile();
    }

    /* Clear all entries */
    ScrapMgr_HAL_ClearEntries();

    /* Clean up platform resources */
#ifdef __linux__
    if (gScrapMgr.display) {
        if (gScrapMgr.window) {
            XDestroyWindow(gScrapMgr.display, gScrapMgr.window);
        }
        XCloseDisplay(gScrapMgr.display);
    }
#endif

#ifdef __APPLE__
    if (gScrapMgr.pasteboard) {
        CFRelease(gScrapMgr.pasteboard);
    }
#endif

    /* Reset state */
    memset(&gScrapMgr, 0, sizeof(gScrapMgr));

    printf("Scrap Manager HAL: Cleaned up\n");
}