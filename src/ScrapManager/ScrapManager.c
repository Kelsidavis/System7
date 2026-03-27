/*
 * ScrapManager.c - Classic Mac OS Scrap Manager Implementation
 *
 * System 7.1-compatible clipboard with multiple flavors and persistence.
 * Uses Handle-based storage for memory management.
 */

/* Standard includes */
#include <string.h>
#include "SystemTypes.h"
#include "ScrapManager/ScrapManager.h"
#include "ScrapManager/ScrapTypes.h"
#include "MemoryMgr/MemoryManager.h"
#ifdef ENABLE_PROCESS_COOP
#include "ProcessMgr/ProcessTypes.h"
#endif
#include "Gestalt/Gestalt.h"
#include "System71StdLib.h"
#include "ScrapManager/ScrapLogging.h"
#include "FileManager.h"

/* Additional function declarations */
extern OSErr Gestalt_RegisterSelector(OSType selector, SInt32 value);
/* Note: BlockMoveData and MemError declared in MemoryMgr/MemoryManager.h */

/* Debug logging control */
#define SCRAP_DEBUG 1

#if SCRAP_DEBUG
#define SCRAP_LOG(...) SCRAP_LOG_DEBUG("[Scrap] " __VA_ARGS__)
#else
#define SCRAP_LOG(...)
#endif

/* Maximum number of scrap types we support */
#define MAX_SCRAP_ITEMS 16

/* File format constants */
#define SCRAP_FILE_MAGIC    0x434C4950  /* 'CLIP' */
#define SCRAP_FILE_VERSION  1
#define SCRAP_FILE_PATH     "/Clipboard"

/* Error codes */
#ifndef noTypeErr
#define noTypeErr (-102)  /* No object of that type in scrap */
#endif

/* Scrap Manager global state */
static struct {
    UInt32 changeCnt;      /* Change counter (increments on change) */
    short count;           /* Number of items in scrap */
    ProcessID owner;       /* Process that owns current scrap */
    ScrapItem items[MAX_SCRAP_ITEMS];
    Boolean inited;        /* Has been initialized */
    Boolean dirty;         /* Needs saving to disk */
} gScrap = {0};

/* Forward declarations */
static void InitScrapIfNeeded(void);
static ScrapItem* FindScrapItem(ResType type);
static ScrapItem* AllocateScrapItem(ResType type);
static OSErr ScrapGestaltProc(long *response);
static void Scrap_SaveToVFS(void);
static void Scrap_LoadFromVFS(void);

/*
 * ScrapGestaltProc - Gestalt function for scrap manager capabilities
 *
 * Returns a bitmask indicating scrap manager features:
 * bit 0: Scrap Manager present
 * bit 1: TEXT format supported
 * bit 2: PICT format supported
 * bit 3: Advanced clipboard features available
 */
OSErr ScrapGestaltProc(long *response) {
    if (!response) {
        return paramErr;
    }

    /* Return capability bits */
    *response = 0x07;  /* bits 0, 1, 2 set = present, TEXT, PICT supported */

    return noErr;
}

/*
 * InitScrapIfNeeded - Initialize scrap on first use
 */
static void InitScrapIfNeeded(void)
{
    int i;

    if (gScrap.inited) {
        return;
    }

    /* Zero the scrap table */
    gScrap.changeCnt = 0;
    gScrap.count = 0;
    gScrap.owner = 0;
    gScrap.dirty = false;

    for (i = 0; i < MAX_SCRAP_ITEMS; i++) {
        gScrap.items[i].type = 0;
        gScrap.items[i].data = NULL;
    }

    /* Register with Gestalt */
    /* 'scra' - bit0: present, bit1: TEXT supported, bit2: PICT supported */
    extern OSErr NewGestalt(OSType selector, OSErr (*proc)(long *response));
    extern OSErr ScrapGestaltProc(long *response);

    OSErr err = NewGestalt('scra', ScrapGestaltProc);
    if (err == noErr) {
        SCRAP_LOG("Registered with Gestalt\n");
    } else if (err == gestaltDupSelectorErr) {
        SCRAP_LOG("Scrap already registered with Gestalt\n");
    }

    gScrap.inited = true;

    /* Try to load persisted scrap data from VFS */
    Scrap_LoadFromVFS();
}

/*
 * FindScrapItem - Find item by type
 */
static ScrapItem* FindScrapItem(ResType type)
{
    int i;
    for (i = 0; i < MAX_SCRAP_ITEMS; i++) {
        if (gScrap.items[i].type == type && gScrap.items[i].data != NULL) {
            return &gScrap.items[i];
        }
    }
    return NULL;
}

/*
 * AllocateScrapItem - Allocate or find slot for type
 */
static ScrapItem* AllocateScrapItem(ResType type)
{
    ScrapItem* item;
    int i;

    /* First check if type already exists */
    item = FindScrapItem(type);
    if (item) {
        return item;
    }

    /* Find empty slot */
    for (i = 0; i < MAX_SCRAP_ITEMS; i++) {
        if (gScrap.items[i].type == 0 || gScrap.items[i].data == NULL) {
            gScrap.items[i].type = type;
            return &gScrap.items[i];
        }
    }

    return NULL;  /* Table full */
}

/*
 * Scrap_Zero - Clear the scrap
 */
void Scrap_Zero(void)
{
    int i;
    InitScrapIfNeeded();

    SCRAP_LOG("Scrap_Zero called\n");

    /* Dispose all existing handles */
    for (i = 0; i < MAX_SCRAP_ITEMS; i++) {
        if (gScrap.items[i].data != NULL) {
            DisposeHandle(gScrap.items[i].data);
            gScrap.items[i].data = NULL;
        }
        gScrap.items[i].type = 0;
    }

    /* Reset state */
    gScrap.count = 0;
    gScrap.changeCnt++;  /* Increment change counter */
    gScrap.dirty = true;

    /* Set owner to current process if ProcessMgr is available */
#ifdef ENABLE_PROCESS_COOP
    extern ProcessID Proc_GetCurrent(void);
    gScrap.owner = Proc_GetCurrent();
#else
    gScrap.owner = 1;  /* Default process ID */
#endif

    SCRAP_LOG("Zeroed, changeCnt=%lu owner=%d\n",
             (unsigned long)gScrap.changeCnt, gScrap.owner);
}

/*
 * Scrap_Put - Put data into scrap
 */
OSErr Scrap_Put(Size size, ResType type, const void* src)
{
    InitScrapIfNeeded();

    /* Validate parameters */
    if ((src == NULL && size > 0) || (src != NULL && size == 0)) {
        return paramErr;
    }

    /* Find or allocate item */
    ScrapItem* item = AllocateScrapItem(type);
    if (!item) {
        return memFullErr;  /* No space in scrap table */
    }

    /* Allocate or resize handle */
    if (item->data == NULL) {
        item->data = NewHandle(size);
        if (!item->data) {
            return memFullErr;
        }
        gScrap.count++;  /* New item */
    } else {
        SetHandleSize(item->data, size);
        if (MemError() != noErr) {
            return memFullErr;
        }
    }

    /* Copy data into handle */
    if (size > 0 && src != NULL) {
        HLock(item->data);
        BlockMoveData(src, *item->data, size);
        HUnlock(item->data);
    }

    /* Update state and owner */
    gScrap.changeCnt++;
    gScrap.dirty = true;
#ifdef ENABLE_PROCESS_COOP
    extern ProcessID Proc_GetCurrent(void);
    gScrap.owner = Proc_GetCurrent();
#else
    gScrap.owner = 1;
#endif

    SCRAP_LOG("Put type='%c%c%c%c' size=%ld changeCnt=%lu\n",
             (char)(type >> 24), (char)(type >> 16),
             (char)(type >> 8), (char)type,
             (long)size, (unsigned long)gScrap.changeCnt);

    /* Persist to VFS */
    Scrap_SaveToVFS();

    return noErr;
}

/*
 * Scrap_Get - Get data from scrap
 */
Size Scrap_Get(void* dest, ResType type)
{
    InitScrapIfNeeded();

    /* Find item */
    ScrapItem* item = FindScrapItem(type);
    if (!item || !item->data) {
        return 0;  /* Not found */
    }

    /* Get size */
    Size size = GetHandleSize(item->data);

    /* Copy data if destination provided */
    if (dest != NULL && size > 0) {
        HLock(item->data);
        BlockMoveData(*item->data, dest, size);
        HUnlock(item->data);
    }

    SCRAP_LOG("Get type='%c%c%c%c' size=%ld\n",
             (char)(type >> 24), (char)(type >> 16),
             (char)(type >> 8), (char)type,
             (long)size);

    return size;
}

/*
 * Scrap_Info - Get scrap information
 */
void Scrap_Info(short* count, short* state)
{
    InitScrapIfNeeded();

    if (count) {
        *count = gScrap.count;
    }
    if (state) {
        *state = (short)(gScrap.changeCnt & 0xFFFF);  /* Return low 16 bits for compatibility */
    }
}

/*
 * Scrap_Unload - Unload scrap from memory (no-op for MVP)
 */
void Scrap_Unload(void)
{
    /* No-op for now - could write to disk in future */
}

/*
 * Scrap_GetOwner - Get process that owns current scrap
 */
ProcessID Scrap_GetOwner(void)
{
    InitScrapIfNeeded();
    return gScrap.owner;
}

/* ============================================================================
 * File I/O - Scrap persistence via VFS
 * ============================================================================ */

#include "FS/vfs.h"

/**
 * Scrap_SaveToVFS - Persist scrap data to VFS
 * Format: [magic:4][version:2][count:2] then per item [type:4][size:4][data:size]
 */
static void Scrap_SaveToVFS(void)
{
    if (!gScrap.dirty || gScrap.count == 0) return;

    VRefNum vref = VFS_GetBootVRef();
    FileID fid = 0;

    /* Create or open the clipboard file */
    if (!VFS_CreateFile(vref, 2, "Clipboard", 'CLIP', 'MACS', &fid)) {
        CatEntry entry;
        if (VFS_Lookup(vref, 2, "Clipboard", &entry)) {
            fid = entry.id;
        } else {
            return;
        }
    }

    VFSFile *file = VFS_OpenFile(vref, fid, false);
    if (!file) return;

    /* Write header */
    uint32_t magic = SCRAP_FILE_MAGIC;
    uint16_t version = SCRAP_FILE_VERSION;
    uint16_t count = (uint16_t)gScrap.count;
    uint32_t written;
    VFS_WriteFile(file, &magic, 4, &written);
    VFS_WriteFile(file, &version, 2, &written);
    VFS_WriteFile(file, &count, 2, &written);

    /* Write each scrap item */
    for (int i = 0; i < MAX_SCRAP_ITEMS; i++) {
        if (gScrap.items[i].type == 0 || gScrap.items[i].data == NULL) continue;

        uint32_t type = gScrap.items[i].type;
        uint32_t size = (uint32_t)GetHandleSize(gScrap.items[i].data);
        VFS_WriteFile(file, &type, 4, &written);
        VFS_WriteFile(file, &size, 4, &written);

        if (size > 0) {
            HLock(gScrap.items[i].data);
            VFS_WriteFile(file, *gScrap.items[i].data, size, &written);
            HUnlock(gScrap.items[i].data);
        }
    }

    VFS_CloseFile(file);
    gScrap.dirty = false;
}

/**
 * Scrap_LoadFromVFS - Load scrap data from VFS on startup
 */
static void Scrap_LoadFromVFS(void)
{
    VRefNum vref = VFS_GetBootVRef();
    CatEntry entry;
    if (!VFS_Lookup(vref, 2, "Clipboard", &entry)) return;

    VFSFile *file = VFS_OpenFile(vref, entry.id, false);
    if (!file) return;

    /* Read and validate header */
    uint32_t magic = 0;
    uint16_t version = 0, count = 0;
    uint32_t bytesRead;

    if (!VFS_ReadFile(file, &magic, 4, &bytesRead) || magic != SCRAP_FILE_MAGIC) {
        VFS_CloseFile(file);
        return;
    }
    VFS_ReadFile(file, &version, 2, &bytesRead);
    VFS_ReadFile(file, &count, 2, &bytesRead);

    if (version != SCRAP_FILE_VERSION || count > MAX_SCRAP_ITEMS) {
        VFS_CloseFile(file);
        return;
    }

    /* Read each item */
    for (uint16_t i = 0; i < count; i++) {
        uint32_t type = 0, size = 0;
        if (!VFS_ReadFile(file, &type, 4, &bytesRead) || bytesRead < 4) break;
        if (!VFS_ReadFile(file, &size, 4, &bytesRead) || bytesRead < 4) break;

        if (size > 1024 * 1024) break;  /* Sanity limit: 1MB per item */

        ScrapItem *item = AllocateScrapItem(type);
        if (!item) break;

        item->data = NewHandle(size);
        if (!item->data) break;

        if (size > 0) {
            HLock(item->data);
            VFS_ReadFile(file, *item->data, size, &bytesRead);
            HUnlock(item->data);
        }
        gScrap.count++;
    }

    VFS_CloseFile(file);
    gScrap.dirty = false;
}

/* ============================================================================
 * Mac OS Classic API Compatibility Functions
 * ============================================================================ */

/*
 * ZeroScrap - Clear the scrap (Classic Mac OS API)
 */
void ZeroScrap(void) {
    Scrap_Zero();
}

/*
 * PutScrap - Put data into scrap (Classic Mac OS API)
 */
void PutScrap(long byteCount, OSType theType, const void* sourcePtr) {
    if (byteCount < 0) return;
    Scrap_Put((Size)byteCount, theType, sourcePtr);
}

/*
 * GetScrap - Get data from scrap (Classic Mac OS API)
 * Returns size on success, 0 if type not found
 * Appends data to hDest at *offset if provided
 */
long GetScrap(Handle hDest, OSType theType, long* offset) {
    ScrapItem* item;
    Size flavorSize;
    Size currentSize;
    Ptr srcPtr;
    Ptr destPtr;

    InitScrapIfNeeded();

    /* Find the item */
    item = FindScrapItem(theType);
    if (!item || !item->data) {
        SCRAP_LOG("GetScrap: type '%.4s' not found\n", (char*)&theType);
        return 0;
    }

    /* Get flavor size */
    flavorSize = GetHandleSize(item->data);
    if (flavorSize == 0) {
        return 0;
    }

    /* If no destination, just return size */
    if (!hDest) {
        return (long)flavorSize;
    }

    /* Get current size and grow handle */
    currentSize = GetHandleSize(hDest);
    SetHandleSize(hDest, currentSize + flavorSize);
    if (MemError() != noErr) {
        SCRAP_LOG("GetScrap: failed to grow handle\n");
        return 0;
    }

    /* Lock both handles for copying */
    HLock(item->data);
    HLock(hDest);

    /* Copy data at offset */
    srcPtr = *item->data;
    destPtr = *hDest + (offset ? *offset : 0);
    BlockMoveData(srcPtr, destPtr, flavorSize);

    /* Unlock handles */
    HUnlock(item->data);
    HUnlock(hDest);

    /* Update offset if provided */
    if (offset) {
        *offset += flavorSize;
    }

    SCRAP_LOG("GetScrap: copied %ld bytes of '%.4s'\n",
              (long)flavorSize, (char*)&theType);

    return (long)flavorSize;
}

/*
 * LoadScrap - Load scrap from disk using File Manager
 */
void LoadScrap(void) {
    Str255 fileName = {9, 'C','l','i','p','b','o','a','r','d'};  /* Pascal string "Clipboard" */
    FileRefNum refNum;
    OSErr err;
    UInt32 magic, version, count;
    UInt32 readCount;
    int i;

    InitScrapIfNeeded();

    /* Try to open the clipboard file */
    err = FSOpen(fileName, 0, &refNum);
    if (err != noErr) {
        SCRAP_LOG("LoadScrap: Clipboard file not found (first run)\n");
        return;  /* No clipboard file yet */
    }

    /* Read magic number */
    readCount = sizeof(magic);
    err = FSRead(refNum, &readCount, &magic);
    if (err != noErr || readCount != sizeof(magic) || magic != SCRAP_FILE_MAGIC) {
        SCRAP_LOG("LoadScrap: Invalid clipboard file format\n");
        FSClose(refNum);
        return;
    }

    /* Read version */
    readCount = sizeof(version);
    err = FSRead(refNum, &readCount, &version);
    if (err != noErr || version != SCRAP_FILE_VERSION) {
        SCRAP_LOG("LoadScrap: Unsupported clipboard version\n");
        FSClose(refNum);
        return;
    }

    /* Read item count */
    readCount = sizeof(count);
    err = FSRead(refNum, &readCount, &count);
    if (err != noErr || count > MAX_SCRAP_ITEMS) {
        SCRAP_LOG("LoadScrap: Invalid item count\n");
        FSClose(refNum);
        return;
    }

    /* Clear existing scrap */
    Scrap_Zero();

    /* Read each scrap item */
    for (i = 0; i < (int)count; i++) {
        OSType type;
        UInt32 size;
        Handle data;

        /* Read type */
        readCount = sizeof(type);
        err = FSRead(refNum, &readCount, &type);
        if (err != noErr) break;

        /* Read size */
        readCount = sizeof(size);
        err = FSRead(refNum, &readCount, &size);
        if (err != noErr || size > 1024 * 1024) break;  /* Sanity check: max 1MB */

        /* Allocate handle for data */
        data = NewHandle(size);
        if (!data) {
            SCRAP_LOG("LoadScrap: Failed to allocate %lu bytes\n", (unsigned long)size);
            break;
        }

        /* Read data */
        HLock(data);
        readCount = size;
        err = FSRead(refNum, &readCount, *data);
        HUnlock(data);

        if (err != noErr || readCount != size) {
            DisposeHandle(data);
            break;
        }

        /* Add to scrap */
        if (gScrap.count < MAX_SCRAP_ITEMS) {
            gScrap.items[gScrap.count].type = type;
            gScrap.items[gScrap.count].data = data;
            gScrap.count++;
            SCRAP_LOG("LoadScrap: Loaded '%.4s' (%lu bytes)\n", (char*)&type, (unsigned long)size);
        } else {
            DisposeHandle(data);
        }
    }

    FSClose(refNum);
    gScrap.dirty = false;
    SCRAP_LOG("LoadScrap: Loaded %d items from clipboard\n", gScrap.count);
}

/*
 * UnloadScrap - Save scrap to disk using File Manager
 */
void UnloadScrap(void) {
    Str255 fileName = {9, 'C','l','i','p','b','o','a','r','d'};  /* Pascal string "Clipboard" */
    FileRefNum refNum;
    OSErr err;
    UInt32 magic = SCRAP_FILE_MAGIC;
    UInt32 version = SCRAP_FILE_VERSION;
    UInt32 count;
    UInt32 writeCount;
    int i;

    InitScrapIfNeeded();

    if (!gScrap.dirty) {
        SCRAP_LOG("UnloadScrap: Scrap not dirty, skipping save\n");
        return;  /* Nothing to save */
    }

    /* Delete old clipboard file if it exists */
    FSDelete(fileName, 0);

    /* Create new clipboard file */
    err = FSCreate(fileName, 0, 'CLIP', 'SYSL');
    if (err != noErr) {
        SCRAP_LOG("UnloadScrap: Failed to create clipboard file (err=%d)\n", err);
        return;
    }

    /* Open for writing */
    err = FSOpen(fileName, 0, &refNum);
    if (err != noErr) {
        SCRAP_LOG("UnloadScrap: Failed to open clipboard file (err=%d)\n", err);
        return;
    }

    /* Write magic number */
    writeCount = sizeof(magic);
    err = FSWrite(refNum, &writeCount, &magic);
    if (err != noErr) goto cleanup;

    /* Write version */
    writeCount = sizeof(version);
    err = FSWrite(refNum, &writeCount, &version);
    if (err != noErr) goto cleanup;

    /* Write item count */
    count = gScrap.count;
    writeCount = sizeof(count);
    err = FSWrite(refNum, &writeCount, &count);
    if (err != noErr) goto cleanup;

    /* Write each scrap item */
    for (i = 0; i < gScrap.count; i++) {
        OSType type = gScrap.items[i].type;
        UInt32 size;

        if (!gScrap.items[i].data) continue;

        size = GetHandleSize(gScrap.items[i].data);

        /* Write type */
        writeCount = sizeof(type);
        err = FSWrite(refNum, &writeCount, &type);
        if (err != noErr) break;

        /* Write size */
        writeCount = sizeof(size);
        err = FSWrite(refNum, &writeCount, &size);
        if (err != noErr) break;

        /* Write data */
        HLock(gScrap.items[i].data);
        writeCount = size;
        err = FSWrite(refNum, &writeCount, *gScrap.items[i].data);
        HUnlock(gScrap.items[i].data);

        if (err != noErr) break;

        SCRAP_LOG("UnloadScrap: Saved '%.4s' (%lu bytes)\n", (char*)&type, (unsigned long)size);
    }

cleanup:
    FSClose(refNum);

    if (err == noErr) {
        gScrap.dirty = false;
        SCRAP_LOG("UnloadScrap: Saved %d items to clipboard\n", gScrap.count);
    } else {
        SCRAP_LOG("UnloadScrap: Save failed (err=%d)\n", err);
    }
}
/*
 * InfoScrap - Get scrap change count
 */
long InfoScrap(void) {
    InitScrapIfNeeded();
    return (long)gScrap.changeCnt;
}

/*
 * ScrapHasFlavor - Check if clipboard has specified flavor
 */
Boolean ScrapHasFlavor(OSType theType) {
    InitScrapIfNeeded();
    return FindScrapItem(theType) != NULL;
}

/*
 * ScrapGetFlavorSize - Get size of specified flavor
 */
long ScrapGetFlavorSize(OSType theType) {
    ScrapItem* item;

    InitScrapIfNeeded();

    item = FindScrapItem(theType);
    if (!item || !item->data) {
        return 0;
    }

    return (long)GetHandleSize(item->data);
}

#ifdef SCRAP_SELFTEST
/*
 * Scrap_RunSelfTest - Run self-test (called from main.c)
 */
void Scrap_RunSelfTest(void)
{
    extern void serial_puts(const char* str);
    serial_puts("[Scrap] Running self-test...\n");

    /* Test 1: Zero scrap */
    Scrap_Zero();

    /* Test 2: Put TEXT */
    const char* testText = "hello";
    OSErr putErr = Scrap_Put(5, kScrapTypeTEXT, testText);

    /* Test 3: Get TEXT */
    char buffer[32] = {0};
    Size size = Scrap_Get(buffer, kScrapTypeTEXT);

    /* Test 4: Info */
    short count, state;
    Scrap_Info(&count, &state);

    ProcessID owner = Scrap_GetOwner();

    /* Use serial_puts for simpler output */
    serial_puts("[Scrap] Self-test completed: ");
    serial_puts(buffer);
    serial_puts(" (scrap working)\n");

    (void)putErr; (void)size; (void)state; (void)owner;  /* Suppress warnings */
}
#endif