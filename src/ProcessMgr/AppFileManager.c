/*
 * AppFileManager.c - Application File Management
 *
 * Implements System 7 Process Manager functions for passing files to applications
 * at launch time. These functions allow applications to discover which documents
 * were opened with them.
 *
 * Based on Inside Macintosh: Processes, Chapter 2
 */

#include "SystemTypes.h"
#include "ProcessMgr/ProcessMgr.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include <string.h>

/* Internal function prototypes */
void InitAppFileManager(const char* name, SInt16 refNum);
OSErr AddAppFile(SInt16 vRefNum, OSType fType, const char* fileName);
void SetAppFileMessage(SInt16 message);

/* Debug logging */
#define APP_FILE_DEBUG 1

#if APP_FILE_DEBUG
#define APP_FILE_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[AppFile] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define APP_FILE_LOG(...)
#endif

/* Maximum number of files that can be opened with an application */
#define MAX_APP_FILES 32

/* App file entry */
typedef struct AppFileEntry {
    AppFile fileInfo;
    Boolean valid;
    Boolean processed;  /* Set by ClrAppFiles */
} AppFileEntry;

/* Global state for application file management */
static struct {
    Str255 appName;
    SInt16 appRefNum;
    Handle appParam;
    AppFileEntry files[MAX_APP_FILES];
    SInt16 fileCount;
    SInt16 message;     /* 0 = appOpen (files to open), 1 = appPrint (files to print) */
    Boolean initialized;
} gAppFileState = {
    .appName = {0},
    .appRefNum = 0,
    .appParam = NULL,
    .fileCount = 0,
    .message = 0,
    .initialized = false
};

/*
 * InitAppFileManager - Initialize the app file manager
 * This is called internally when the application is launched
 */
void InitAppFileManager(const char* name, SInt16 refNum) {
    if (gAppFileState.initialized) {
        return;
    }

    /* Copy application name */
    if (name) {
        size_t len = strlen(name);
        if (len > 255) len = 255;
        gAppFileState.appName[0] = (unsigned char)len;
        memcpy(&gAppFileState.appName[1], name, len);
    } else {
        gAppFileState.appName[0] = 0;
    }

    gAppFileState.appRefNum = refNum;
    gAppFileState.appParam = NULL;
    gAppFileState.fileCount = 0;
    gAppFileState.message = 0;
    gAppFileState.initialized = true;

    /* Clear all file entries */
    for (int i = 0; i < MAX_APP_FILES; i++) {
        gAppFileState.files[i].valid = false;
        gAppFileState.files[i].processed = false;
    }

    APP_FILE_LOG("Initialized for app: %.*s, refNum: %d\n",
                 (int)gAppFileState.appName[0], &gAppFileState.appName[1],
                 refNum);
}

/*
 * AddAppFile - Add a file to the app file list
 * This is called internally when files are passed to the application
 */
OSErr AddAppFile(SInt16 vRefNum, OSType fType, const char* fileName) {
    if (!gAppFileState.initialized) {
        InitAppFileManager("Unknown", 0);
    }

    if (gAppFileState.fileCount >= MAX_APP_FILES) {
        APP_FILE_LOG("AddAppFile: Maximum files reached\n");
        return memFullErr;
    }

    /* Find next available slot */
    SInt16 index = -1;
    for (int i = 0; i < MAX_APP_FILES; i++) {
        if (!gAppFileState.files[i].valid) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return memFullErr;
    }

    /* Fill in file info */
    AppFileEntry* entry = &gAppFileState.files[index];
    entry->fileInfo.vRefNum = vRefNum;
    entry->fileInfo.fType = fType;
    entry->fileInfo.versNum = 0;  /* Unused in System 7 */

    /* Copy filename as Pascal string */
    if (fileName) {
        size_t len = strlen(fileName);
        if (len > 255) len = 255;
        entry->fileInfo.fName[0] = (unsigned char)len;
        memcpy(&entry->fileInfo.fName[1], fileName, len);
    } else {
        entry->fileInfo.fName[0] = 0;
    }

    entry->valid = true;
    entry->processed = false;
    gAppFileState.fileCount++;

    APP_FILE_LOG("AddAppFile: Added '%.*s' (vRefNum=%d, type='%.4s') at index %d\n",
                 (int)entry->fileInfo.fName[0], &entry->fileInfo.fName[1],
                 vRefNum, (char*)&fType, index);

    return noErr;
}

/*
 * SetAppFileMessage - Set the message type (appOpen or appPrint)
 */
void SetAppFileMessage(SInt16 message) {
    gAppFileState.message = message;
}

/*
 * GetAppParms - Get application parameters
 *
 * Returns the application name, reference number, and optional parameter handle.
 * This is one of the first calls an application makes at startup.
 */
void GetAppParms(Str255 apName, SInt16* apRefNum, Handle* apParam) {
    if (!gAppFileState.initialized) {
        InitAppFileManager("Application", 0);
    }

    /* Copy application name */
    if (apName) {
        memcpy(apName, gAppFileState.appName, gAppFileState.appName[0] + 1);
    }

    /* Return reference number */
    if (apRefNum) {
        *apRefNum = gAppFileState.appRefNum;
    }

    /* Return parameter handle (usually NULL in System 7) */
    if (apParam) {
        *apParam = gAppFileState.appParam;
    }

    APP_FILE_LOG("GetAppParms: name='%.*s', refNum=%d\n",
                 (int)gAppFileState.appName[0], &gAppFileState.appName[1],
                 gAppFileState.appRefNum);
}

/*
 * CountAppFiles - Count application files
 *
 * Returns the number of files that were opened with the application
 * and the message type (0 = appOpen, 1 = appPrint).
 */
void CountAppFiles(SInt16* message, SInt16* count) {
    if (!gAppFileState.initialized) {
        InitAppFileManager("Application", 0);
    }

    if (message) {
        *message = gAppFileState.message;
    }

    if (count) {
        /* Count only valid, unprocessed files */
        SInt16 validCount = 0;
        for (int i = 0; i < MAX_APP_FILES; i++) {
            if (gAppFileState.files[i].valid && !gAppFileState.files[i].processed) {
                validCount++;
            }
        }
        *count = validCount;
    }

    APP_FILE_LOG("CountAppFiles: message=%d, count=%d\n",
                 gAppFileState.message, count ? *count : 0);
}

/*
 * GetAppFiles - Get information about a file
 *
 * Returns information about the file at the specified index (1-based).
 * Applications typically call this in a loop after CountAppFiles.
 */
OSErr GetAppFiles(SInt16 index, AppFile* theFile) {
    if (!gAppFileState.initialized) {
        return paramErr;
    }

    if (!theFile) {
        return paramErr;
    }

    /* Index is 1-based */
    if (index < 1) {
        return paramErr;
    }

    /* Find the nth valid, unprocessed file */
    SInt16 currentIndex = 0;
    for (int i = 0; i < MAX_APP_FILES; i++) {
        if (gAppFileState.files[i].valid && !gAppFileState.files[i].processed) {
            currentIndex++;
            if (currentIndex == index) {
                /* Copy file info */
                memcpy(theFile, &gAppFileState.files[i].fileInfo, sizeof(AppFile));

                APP_FILE_LOG("GetAppFiles: index=%d, file='%.*s'\n",
                             index, (int)theFile->fName[0], &theFile->fName[1]);

                return noErr;
            }
        }
    }

    /* Index out of range */
    APP_FILE_LOG("GetAppFiles: index=%d out of range\n", index);
    return paramErr;
}

/*
 * ClrAppFiles - Clear a file from the list
 *
 * Marks a file as processed so it won't be returned by subsequent
 * CountAppFiles/GetAppFiles calls. Applications call this after
 * successfully opening each file.
 */
void ClrAppFiles(SInt16 index) {
    if (!gAppFileState.initialized) {
        return;
    }

    /* Index is 1-based */
    if (index < 1) {
        return;
    }

    /* Find the nth valid, unprocessed file */
    SInt16 currentIndex = 0;
    for (int i = 0; i < MAX_APP_FILES; i++) {
        if (gAppFileState.files[i].valid && !gAppFileState.files[i].processed) {
            currentIndex++;
            if (currentIndex == index) {
                /* Mark as processed */
                gAppFileState.files[i].processed = true;

                APP_FILE_LOG("ClrAppFiles: Cleared file at index %d\n", index);
                return;
            }
        }
    }

    APP_FILE_LOG("ClrAppFiles: index=%d out of range\n", index);
}
