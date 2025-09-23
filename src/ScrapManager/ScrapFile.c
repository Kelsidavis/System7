/* #include "SystemTypes.h" */
#include <string.h>
#include <stdio.h>
/*
 * ScrapFile.c - Scrap File Handling and Disk Storage
 * System 7.1 Portable - Scrap Manager Component
 *
 * Implements scrap file management, disk storage, and persistence
 * for large clipboard data that doesn't fit in memory.
 */

// #include "CompatibilityFix.h" // Removed

#include "ScrapManager/ScrapManager.h"
#include "ScrapManager/ScrapTypes.h"
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "FileManager.h"
/* #include "ErrorCodes.h"
 - error codes in MacTypes.h */

/* File management state */
typedef struct {
    Boolean         hasScrapFile;
    FSSpec          scrapFileSpec;
    Boolean         isDirty;
    time_t          lastSaved;
    SInt32         fileSize;
    FILE            *fileHandle;
} ScrapFileState;

static ScrapFileState gFileState = {0};
static char gScrapDirectory[256] = {0};
static Boolean gFileInitialized = false;

/* Internal function prototypes */
static OSErr InitializeScrapFile(void);
static OSErr CreateScrapFile(const FSSpec *fileSpec);
static OSErr OpenScrapFile(const FSSpec *fileSpec, Boolean forWriting);
static OSErr CloseScrapFile(void);
static OSErr WriteScrapFileHeader(const ScrapFileHeader *header);
static OSErr ReadScrapFileHeader(ScrapFileHeader *header);
static OSErr WriteFormatEntry(const ScrapFormatEntry *entry);
static OSErr ReadFormatEntry(ScrapFormatEntry *entry);
static OSErr WriteScrapData(const void *data, SInt32 size);
static OSErr ReadScrapData(void *data, SInt32 size);
static OSErr ValidateScrapFile(const FSSpec *fileSpec);
static void GetDefaultScrapPath(char *path, size_t maxLen);
static OSErr ConvertPathToFSSpec(const char *path, FSSpec *spec);
static OSErr ConvertFSSpecToPath(const FSSpec *spec, char *path, size_t maxLen);

/*
 * Scrap File Management Functions
 */

OSErr SetScrapFile(ConstStr255Param fileName, SInt16 vRefNum, SInt32 dirID)
{
    OSErr err = noErr;
    char path[256];

    if (!gFileInitialized) {
        InitializeScrapFile();
    }

    /* Close current file if open */
    if (gFileState.fileHandle) {
        CloseScrapFile();
    }

    /* Convert parameters to file path */
    if (fileName && fileName[0] > 0) {
        /* Copy Pascal string to C string */
        memcpy(path, &fileName[1], fileName[0]);
        path[fileName[0]] = '\0';

        /* If it's not an absolute path, use scrap directory */
        if (path[0] != '/') {
            char fullPath[256];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", gScrapDirectory, path);
            /* Safe copy back to path buffer */
            size_t len = strlen(fullPath);
            if (len >= sizeof(path)) len = sizeof(path) - 1;
            memcpy(path, fullPath, len);
            path[len] = '\0';
        }
    } else {
        GetDefaultScrapPath(path, sizeof(path));
    }

    /* Convert to FSSpec */
    err = ConvertPathToFSSpec(path, &gFileState.scrapFileSpec);
    if (err != noErr) {
        return err;
    }

    gFileState.hasScrapFile = true;
    gFileState.isDirty = false;
    gFileState.lastSaved = 0;

    return noErr;
}

OSErr GetScrapFile(Str255 fileName, SInt16 *vRefNum, SInt32 *dirID)
{
    char path[256];

    if (!gFileInitialized) {
        InitializeScrapFile();
    }

    if (!gFileState.hasScrapFile) {
        return fnfErr;
    }

    /* Convert FSSpec back to path and filename */
    ConvertFSSpecToPath(&gFileState.scrapFileSpec, path, sizeof(path));

    /* Extract filename from path */
    char *lastSlash = strrchr(path, '/');
    if (lastSlash && fileName) {
        const char *name = lastSlash + 1;
        size_t nameLen = strlen(name);
        if (nameLen > 255) nameLen = 255;

        fileName[0] = (unsigned char)nameLen;
        memcpy(&fileName[1], name, nameLen);
    }

    /* For simplicity, set vRefNum and dirID to defaults */
    if (vRefNum) *vRefNum = 0;
    if (dirID) *dirID = 0;

    return noErr;
}

OSErr SaveScrapToFile(void)
{
    OSErr err = noErr;
    ScrapFileHeader header;
    PScrapStuff scrapInfo;
    SInt16 i;

    if (!gFileInitialized) {
        InitializeScrapFile();
    }

    if (!gFileState.hasScrapFile) {
        return fnfErr;
    }

    scrapInfo = InfoScrap();
    if (!scrapInfo || !scrapInfo->scrapHandle) {
        return scrapNoScrap;
    }

    /* Create or open file for writing */
    err = OpenScrapFile(&gFileState.scrapFileSpec, true);
    if (err != noErr) {
        err = CreateScrapFile(&gFileState.scrapFileSpec);
        if (err != noErr) {
            return err;
        }
        err = OpenScrapFile(&gFileState.scrapFileSpec, true);
        if (err != noErr) {
            return err;
        }
    }

    /* Prepare file header */
    header.signature = 'SCRF';
    header.version = 1;
    header.flags = 0;
    header.createTime = (UInt32)time(NULL);
    header.modifyTime = header.createTime;
    header.dataSize = scrapInfo->scrapSize;
    header.formatCount = scrapInfo->formatTable ? scrapInfo->formatTable->count : 0;
    header.reserved = 0;

    /* Write file header */
    err = WriteScrapFileHeader(&header);
    if (err != noErr) {
        CloseScrapFile();
        return err;
    }

    /* Write format table */
    if (scrapInfo->formatTable) {
        for (i = 0; i < scrapInfo->formatTable->count; i++) {
            err = WriteFormatEntry(&scrapInfo->formatTable->formats[i]);
            if (err != noErr) {
                CloseScrapFile();
                return err;
            }
        }
    }

    /* Write scrap data */
    if (scrapInfo->scrapSize > 0) {
        HLock(scrapInfo->scrapHandle);
        err = WriteScrapData(*scrapInfo->scrapHandle, scrapInfo->scrapSize);
        HUnlock(scrapInfo->scrapHandle);

        if (err != noErr) {
            CloseScrapFile();
            return err;
        }
    }

    CloseScrapFile();

    gFileState.isDirty = false;
    gFileState.lastSaved = time(NULL);
    gFileState.fileSize = header.dataSize + sizeof(header) +
                         (header.formatCount * sizeof(ScrapFormatEntry));

    return noErr;
}

OSErr LoadScrapFromFile(ConstStr255Param fileName, SInt16 vRefNum, SInt32 dirID)
{
    OSErr err = noErr;
    FSSpec fileSpec;
    ScrapFileHeader header;
    Handle scrapHandle = NULL;
    ScrapFormatEntry *formatEntries = NULL;
    SInt16 i;

    if (!gFileInitialized) {
        InitializeScrapFile();
    }

    /* Convert parameters to FSSpec */
    if (fileName && fileName[0] > 0) {
        char path[256];
        memcpy(path, &fileName[1], fileName[0]);
        path[fileName[0]] = '\0';

        if (path[0] != '/') {
            char fullPath[256];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", gScrapDirectory, path);
            /* Safe copy back to path buffer */
            size_t len = strlen(fullPath);
            if (len >= sizeof(path)) len = sizeof(path) - 1;
            memcpy(path, fullPath, len);
            path[len] = '\0';
        }

        err = ConvertPathToFSSpec(path, &fileSpec);
        if (err != noErr) {
            return err;
        }
    } else {
        fileSpec = gFileState.scrapFileSpec;
    }

    /* Validate file */
    err = ValidateScrapFile(&fileSpec);
    if (err != noErr) {
        return err;
    }

    /* Open file for reading */
    err = OpenScrapFile(&fileSpec, false);
    if (err != noErr) {
        return err;
    }

    /* Read file header */
    err = ReadScrapFileHeader(&header);
    if (err != noErr) {
        CloseScrapFile();
        return err;
    }

    /* Validate header */
    if (header.signature != 'SCRF' || header.version > 1) {
        CloseScrapFile();
        return scrapCorruptError;
    }

    /* Clear current scrap */
    ZeroScrap();

    /* Read format entries */
    if (header.formatCount > 0) {
        formatEntries = (ScrapFormatEntry *)NewPtrClear(header.formatCount * sizeof(ScrapFormatEntry));
        if (!formatEntries) {
            CloseScrapFile();
            return memFullErr;
        }

        for (i = 0; i < header.formatCount; i++) {
            err = ReadFormatEntry(&formatEntries[i]);
            if (err != noErr) {
                DisposePtr((Ptr)formatEntries);
                CloseScrapFile();
                return err;
            }
        }
    }

    /* Read scrap data */
    if (header.dataSize > 0) {
        scrapHandle = NewHandle(header.dataSize);
        if (!scrapHandle) {
            if (formatEntries) DisposePtr((Ptr)formatEntries);
            CloseScrapFile();
            return memFullErr;
        }

        HLock(scrapHandle);
        err = ReadScrapData(*scrapHandle, header.dataSize);
        HUnlock(scrapHandle);

        if (err != noErr) {
            DisposeHandle(scrapHandle);
            if (formatEntries) DisposePtr((Ptr)formatEntries);
            CloseScrapFile();
            return err;
        }
    }

    CloseScrapFile();

    /* Reconstruct scrap from loaded data */
    if (scrapHandle && formatEntries) {
        PScrapStuff scrapInfo = InfoScrap();

        /* Set scrap handle */
        if (scrapInfo->scrapHandle) {
            DisposeHandle(scrapInfo->scrapHandle);
        }
        scrapInfo->scrapHandle = scrapHandle;
        scrapInfo->scrapSize = header.dataSize;
        scrapInfo->scrapState = SCRAP_STATE_LOADED;
        scrapInfo->lastModified = header.modifyTime;

        /* Reconstruct format table */
        if (scrapInfo->formatTable) {
            DisposePtr((Ptr)scrapInfo->formatTable);
        }

        size_t tableSize = sizeof(ScrapFormatTable) +
                          (header.formatCount - 1) * sizeof(ScrapFormatEntry);
        scrapInfo->formatTable = (ScrapFormatTable *)NewPtrClear(tableSize);

        if (scrapInfo->formatTable) {
            scrapInfo->formatTable->count = header.formatCount;
            scrapInfo->formatTable->maxCount = header.formatCount;

            for (i = 0; i < header.formatCount; i++) {
                scrapInfo->formatTable->formats[i] = formatEntries[i];
            }
        }
    }

    if (formatEntries) {
        DisposePtr((Ptr)formatEntries);
    }

    return noErr;
}

/*
 * Memory Management Integration
 */

OSErr SetScrapMemoryPrefs(SInt32 memoryThreshold, SInt32 diskThreshold)
{
    /* Store preferences for when to use disk vs memory */
    /* This would integrate with ScrapMemory.c */

    return noErr;
}

OSErr GetScrapMemoryInfo(SInt32 *memoryUsed, SInt32 *diskUsed, SInt32 *totalSize)
{
    PScrapStuff scrapInfo;

    if (!gFileInitialized) {
        InitializeScrapFile();
    }

    scrapInfo = InfoScrap();

    if (memoryUsed) {
        *memoryUsed = scrapInfo && scrapInfo->scrapHandle ?
                     GetHandleSize(scrapInfo->scrapHandle) : 0;
    }

    if (diskUsed) {
        *diskUsed = gFileState.hasScrapFile ? gFileState.fileSize : 0;
    }

    if (totalSize) {
        *totalSize = scrapInfo ? scrapInfo->scrapSize : 0;
    }

    return noErr;
}

/*
 * Internal Helper Functions
 */

static OSErr InitializeScrapFile(void)
{
    if (gFileInitialized) {
        return noErr;
    }

    memset(&gFileState, 0, sizeof(gFileState));

    /* Set up default scrap directory */
    GetDefaultScrapPath(gScrapDirectory, sizeof(gScrapDirectory));

    /* Remove filename from path to get directory */
    char *lastSlash = strrchr(gScrapDirectory, '/');
    if (lastSlash) {
        *lastSlash = '\0';
    }

    /* Create directory if it doesn't exist */
    mkdir(gScrapDirectory, 0755);

    gFileInitialized = true;
    return noErr;
}

static OSErr CreateScrapFile(const FSSpec *fileSpec)
{
    char path[256];
    FILE *file;

    ConvertFSSpecToPath(fileSpec, path, sizeof(path));

    file = fopen(path, "wb");
    if (!file) {
        return ioErr;
    }

    fclose(file);
    return noErr;
}

static OSErr OpenScrapFile(const FSSpec *fileSpec, Boolean forWriting)
{
    char path[256];

    if (gFileState.fileHandle) {
        CloseScrapFile();
    }

    ConvertFSSpecToPath(fileSpec, path, sizeof(path));

    gFileState.fileHandle = fopen(path, forWriting ? "r+b" : "rb");
    if (!gFileState.fileHandle) {
        return fnfErr;
    }

    return noErr;
}

static OSErr CloseScrapFile(void)
{
    if (gFileState.fileHandle) {
        fclose(gFileState.fileHandle);
        gFileState.fileHandle = NULL;
    }

    return noErr;
}

static OSErr WriteScrapFileHeader(const ScrapFileHeader *header)
{
    if (!gFileState.fileHandle || !header) {
        return paramErr;
    }

    if (fwrite(header, sizeof(ScrapFileHeader), 1, gFileState.fileHandle) != 1) {
        return ioErr;
    }

    return noErr;
}

static OSErr ReadScrapFileHeader(ScrapFileHeader *header)
{
    if (!gFileState.fileHandle || !header) {
        return paramErr;
    }

    if (fread(header, sizeof(ScrapFileHeader), 1, gFileState.fileHandle) != 1) {
        return eofErr;
    }

    return noErr;
}

static OSErr WriteFormatEntry(const ScrapFormatEntry *entry)
{
    if (!gFileState.fileHandle || !entry) {
        return paramErr;
    }

    if (fwrite(entry, sizeof(ScrapFormatEntry), 1, gFileState.fileHandle) != 1) {
        return ioErr;
    }

    return noErr;
}

static OSErr ReadFormatEntry(ScrapFormatEntry *entry)
{
    if (!gFileState.fileHandle || !entry) {
        return paramErr;
    }

    if (fread(entry, sizeof(ScrapFormatEntry), 1, gFileState.fileHandle) != 1) {
        return eofErr;
    }

    return noErr;
}

static OSErr WriteScrapData(const void *data, SInt32 size)
{
    if (!gFileState.fileHandle || !data || size <= 0) {
        return paramErr;
    }

    if (fwrite(data, 1, size, gFileState.fileHandle) != (size_t)size) {
        return ioErr;
    }

    return noErr;
}

static OSErr ReadScrapData(void *data, SInt32 size)
{
    if (!gFileState.fileHandle || !data || size <= 0) {
        return paramErr;
    }

    if (fread(data, 1, size, gFileState.fileHandle) != (size_t)size) {
        return eofErr;
    }

    return noErr;
}

static OSErr ValidateScrapFile(const FSSpec *fileSpec)
{
    char path[256];
    struct stat statBuf;

    ConvertFSSpecToPath(fileSpec, path, sizeof(path));

    if (stat(path, &statBuf) != 0) {
        return fnfErr;
    }

    if (!S_ISREG(statBuf.st_mode)) {
        return paramErr;
    }

    if (statBuf.st_size < sizeof(ScrapFileHeader)) {
        return scrapCorruptError;
    }

    return noErr;
}

static void GetDefaultScrapPath(char *path, size_t maxLen)
{
    const char *homeDir = getenv("HOME");
    if (homeDir) {
        snprintf(path, maxLen, "%s/.system71/scrap/clipboard", homeDir);
    } else {
        snprintf(path, maxLen, "/tmp/system71_scrap");
    }
}

static OSErr ConvertPathToFSSpec(const char *path, FSSpec *spec)
{
    if (!path || !spec) {
        return paramErr;
    }

    /* Simple conversion - in real implementation this would be more complex */
    memset(spec, 0, sizeof(FSSpec));

    /* Extract filename */
    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++; /* Skip the slash */
        size_t len = strlen(filename);
        if (len > 63) len = 63; /* Mac filename limit */

        spec->name[0] = (unsigned char)len;
        memcpy(&spec->name[1], filename, len);
    }

    return noErr;
}

static OSErr ConvertFSSpecToPath(const FSSpec *spec, char *path, size_t maxLen)
{
    if (!spec || !path || maxLen == 0) {
        return paramErr;
    }

    /* Simple conversion using default directory */
    char filename[64];
    memcpy(filename, &spec->name[1], spec->name[0]);
    filename[spec->name[0]] = '\0';

    snprintf(path, maxLen, "%s/%s", gScrapDirectory, filename);

    return noErr;
}
