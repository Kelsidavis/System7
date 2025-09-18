/*
 * StandardFilePackage.c
 * System 7.1 Portable Standard File Package Implementation
 *
 * Implements Mac OS Standard File Package (Pack 3) for file dialog functionality.
 * Critical for applications that need file open/save dialogs.
 * Provides platform-independent abstraction layer for native file dialogs.
 */

#include "PackageManager/StandardFilePackage.h"
#include "PackageManager/PackageTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>

/* Standard File package state */
static struct {
    Boolean                 initialized;
    FSSpec                  defaultDirectory;
    int32_t                 preferences;
    PlatformFileDialogs     platformDialogs;
    void                   (*errorHandler)(OSErr error, const char *message);
    pthread_mutex_t         mutex;
    Boolean                 threadSafe;
} g_sfState = {
    .initialized = false,
    .defaultDirectory = {{0}, 0, {0}},
    .preferences = 0,
    .platformDialogs = {NULL},
    .errorHandler = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .threadSafe = true
};

/* Forward declarations */
static void SF_DefaultErrorHandler(OSErr error, const char *message);
static Boolean SF_DefaultOpenDialog(const char *title, const char *defaultPath, const char *filter,
                                   char *selectedPath, int maxPathLen);
static Boolean SF_DefaultSaveDialog(const char *title, const char *defaultPath, const char *defaultName,
                                   const char *filter, char *selectedPath, int maxPathLen);
static Boolean SF_DefaultFolderDialog(const char *title, const char *defaultPath,
                                     char *selectedPath, int maxPathLen);
static void SF_InitializeDefaults(void);
static void SF_ConvertSFReplyToStandard(const SFReply *sfReply, StandardFileReply *stdReply);
static void SF_ConvertStandardToSFReply(const StandardFileReply *stdReply, SFReply *sfReply);
static OSErr SF_ScanDirectory(const char *dirPath, DirectoryListing *listing);
static Boolean SF_ConsoleFileDialog(const char *prompt, const char *defaultPath,
                                   Boolean isSave, char *result, int maxLen);

/**
 * Initialize Standard File Package
 */
int32_t InitStandardFilePackage(void)
{
    if (g_sfState.initialized) {
        return PACKAGE_NO_ERROR;
    }

    /* Initialize thread safety */
    if (g_sfState.threadSafe) {
        pthread_mutex_init(&g_sfState.mutex, NULL);
    }

    /* Set up defaults */
    SF_InitializeDefaults();

    /* Set default platform dialogs */
    if (!g_sfState.platformDialogs.ShowOpenDialog) {
        g_sfState.platformDialogs.ShowOpenDialog = SF_DefaultOpenDialog;
        g_sfState.platformDialogs.ShowSaveDialog = SF_DefaultSaveDialog;
        g_sfState.platformDialogs.ShowFolderDialog = SF_DefaultFolderDialog;
        g_sfState.platformDialogs.SetDialogParent = NULL;
        g_sfState.platformDialogs.SetDialogOptions = NULL;
    }

    /* Set default error handler */
    if (!g_sfState.errorHandler) {
        g_sfState.errorHandler = SF_DefaultErrorHandler;
    }

    g_sfState.initialized = true;
    return PACKAGE_NO_ERROR;
}

/**
 * Standard File package dispatch function
 */
int32_t StandardFileDispatch(int16_t selector, void *params)
{
    if (!g_sfState.initialized) {
        InitStandardFilePackage();
    }

    if (!params) {
        return PACKAGE_INVALID_PARAMS;
    }

    SF_LockFileDialogs();

    switch (selector) {
        case sfSelPutFile: {
            void **args = (void**)params;
            Point where = *(Point*)args[0];
            ConstStr255Param prompt = (ConstStr255Param)args[1];
            ConstStr255Param origName = (ConstStr255Param)args[2];
            DlgHookProcPtr dlgHook = (DlgHookProcPtr)args[3];
            SFReply *reply = (SFReply*)args[4];
            SFPutFile(where, prompt, origName, dlgHook, reply);
            break;
        }

        case sfSelGetFile: {
            void **args = (void**)params;
            Point where = *(Point*)args[0];
            ConstStr255Param prompt = (ConstStr255Param)args[1];
            FileFilterProcPtr fileFilter = (FileFilterProcPtr)args[2];
            int16_t numTypes = *(int16_t*)args[3];
            SFTypeList *typeList = (SFTypeList*)args[4];
            DlgHookProcPtr dlgHook = (DlgHookProcPtr)args[5];
            SFReply *reply = (SFReply*)args[6];
            SFGetFile(where, prompt, fileFilter, numTypes, *typeList, dlgHook, reply);
            break;
        }

        case sfSelStandardPut: {
            void **args = (void**)params;
            ConstStr255Param prompt = (ConstStr255Param)args[0];
            ConstStr255Param defaultName = (ConstStr255Param)args[1];
            StandardFileReply *reply = (StandardFileReply*)args[2];
            StandardPutFile(prompt, defaultName, reply);
            break;
        }

        case sfSelStandardGet: {
            void **args = (void**)params;
            FileFilterProcPtr fileFilter = (FileFilterProcPtr)args[0];
            int16_t numTypes = *(int16_t*)args[1];
            SFTypeList *typeList = (SFTypeList*)args[2];
            StandardFileReply *reply = (StandardFileReply*)args[3];
            StandardGetFile(fileFilter, numTypes, *typeList, reply);
            break;
        }

        default:
            SF_UnlockFileDialogs();
            return PACKAGE_INVALID_SELECTOR;
    }

    SF_UnlockFileDialogs();
    return PACKAGE_NO_ERROR;
}

/**
 * Classic Standard File functions
 */
void SFPutFile(Point where, ConstStr255Param prompt, ConstStr255Param origName,
               DlgHookProcPtr dlgHook, SFReply *reply)
{
    if (!reply) return;

    /* Convert to modern interface */
    StandardFileReply stdReply;
    StandardPutFile(prompt, origName, &stdReply);
    SF_ConvertStandardToSFReply(&stdReply, reply);
}

void SFGetFile(Point where, ConstStr255Param prompt, FileFilterProcPtr fileFilter,
               int16_t numTypes, SFTypeList typeList, DlgHookProcPtr dlgHook, SFReply *reply)
{
    if (!reply) return;

    /* Convert to modern interface */
    StandardFileReply stdReply;
    StandardGetFile(fileFilter, numTypes, typeList, &stdReply);
    SF_ConvertStandardToSFReply(&stdReply, reply);
}

void SFPPutFile(Point where, ConstStr255Param prompt, ConstStr255Param origName,
                DlgHookProcPtr dlgHook, SFReply *reply, int16_t dlgID, ModalFilterProcPtr filterProc)
{
    /* For now, delegate to standard put file */
    SFPutFile(where, prompt, origName, dlgHook, reply);
}

void SFPGetFile(Point where, ConstStr255Param prompt, FileFilterProcPtr fileFilter,
                int16_t numTypes, SFTypeList typeList, DlgHookProcPtr dlgHook, SFReply *reply,
                int16_t dlgID, ModalFilterProcPtr filterProc)
{
    /* For now, delegate to standard get file */
    SFGetFile(where, prompt, fileFilter, numTypes, typeList, dlgHook, reply);
}

/**
 * System 7.0+ Standard File functions
 */
void StandardPutFile(ConstStr255Param prompt, ConstStr255Param defaultName, StandardFileReply *reply)
{
    if (!reply) return;

    /* Initialize reply */
    memset(reply, 0, sizeof(StandardFileReply));
    reply->sfGood = false;

    /* Convert Pascal strings to C strings */
    char cPrompt[256] = "Save As:";
    char cDefaultName[256] = "Untitled";
    char selectedPath[1024];

    if (prompt && prompt[0] > 0) {
        int len = (prompt[0] > 255) ? 255 : prompt[0];
        memcpy(cPrompt, prompt + 1, len);
        cPrompt[len] = '\0';
    }

    if (defaultName && defaultName[0] > 0) {
        int len = (defaultName[0] > 255) ? 255 : defaultName[0];
        memcpy(cDefaultName, defaultName + 1, len);
        cDefaultName[len] = '\0';
    }

    /* Get current directory */
    char currentPath[1024];
    if (!getcwd(currentPath, sizeof(currentPath))) {
        strcpy(currentPath, ".");
    }

    /* Show save dialog */
    Boolean result = false;
    if (g_sfState.platformDialogs.ShowSaveDialog) {
        result = g_sfState.platformDialogs.ShowSaveDialog(cPrompt, currentPath, cDefaultName,
                                                         NULL, selectedPath, sizeof(selectedPath));
    } else {
        result = SF_ConsoleFileDialog(cPrompt, currentPath, true, selectedPath, sizeof(selectedPath));
    }

    if (result) {
        reply->sfGood = true;
        reply->sfReplacing = false; /* Would need to check if file exists */
        reply->sfType = '????';     /* Unknown type */

        /* Convert path to FSSpec */
        SF_PathToFSSpec(selectedPath, &reply->sfFile);

        reply->sfScript = smRoman;
        reply->sfFlags = 0;
        reply->sfIsFolder = false;
        reply->sfIsVolume = false;
    }
}

void StandardGetFile(FileFilterProcPtr fileFilter, int16_t numTypes, SFTypeList typeList,
                     StandardFileReply *reply)
{
    if (!reply) return;

    /* Initialize reply */
    memset(reply, 0, sizeof(StandardFileReply));
    reply->sfGood = false;

    /* Get current directory */
    char currentPath[1024];
    if (!getcwd(currentPath, sizeof(currentPath))) {
        strcpy(currentPath, ".");
    }

    /* Build filter string from type list */
    char filterString[1024] = "";
    if (numTypes > 0) {
        SF_BuildTypeFilter(typeList, numTypes, filterString, sizeof(filterString));
    }

    /* Show open dialog */
    char selectedPath[1024];
    Boolean result = false;
    if (g_sfState.platformDialogs.ShowOpenDialog) {
        result = g_sfState.platformDialogs.ShowOpenDialog("Open File", currentPath,
                                                         (filterString[0] ? filterString : NULL),
                                                         selectedPath, sizeof(selectedPath));
    } else {
        result = SF_ConsoleFileDialog("Open File", currentPath, false, selectedPath, sizeof(selectedPath));
    }

    if (result) {
        reply->sfGood = true;
        reply->sfReplacing = false;
        reply->sfType = '????';     /* Would need to determine actual type */

        /* Convert path to FSSpec */
        SF_PathToFSSpec(selectedPath, &reply->sfFile);

        reply->sfScript = smRoman;
        reply->sfFlags = 0;

        /* Check if selection is folder */
        struct stat statBuf;
        if (stat(selectedPath, &statBuf) == 0) {
            reply->sfIsFolder = S_ISDIR(statBuf.st_mode);
            reply->sfIsVolume = false; /* Would need better detection */
        }
    }
}

void CustomPutFile(ConstStr255Param prompt, ConstStr255Param defaultName, StandardFileReply *reply,
                   int16_t dlgID, Point where, DlgHookYDProcPtr dlgHook,
                   ModalFilterYDProcPtr filterProc, int16_t *activeList,
                   ActivateYDProcPtr activateProc, void *yourDataPtr)
{
    /* For now, delegate to standard put file */
    StandardPutFile(prompt, defaultName, reply);
}

void CustomGetFile(FileFilterYDProcPtr fileFilter, int16_t numTypes, SFTypeList typeList,
                   StandardFileReply *reply, int16_t dlgID, Point where, DlgHookYDProcPtr dlgHook,
                   ModalFilterYDProcPtr filterProc, int16_t *activeList,
                   ActivateYDProcPtr activateProc, void *yourDataPtr)
{
    /* For now, delegate to standard get file */
    StandardGetFile((FileFilterProcPtr)fileFilter, numTypes, typeList, reply);
}

/**
 * C-style interface functions
 */
void sfputfile(Point *where, char *prompt, char *origName, DlgHookProcPtr dlgHook, SFReply *reply)
{
    if (!reply) return;

    /* Convert C strings to Pascal strings */
    Str255 pPrompt, pOrigName;
    if (prompt) {
        CopyC2PStr(prompt, (char*)pPrompt);
    } else {
        pPrompt[0] = 0;
    }

    if (origName) {
        CopyC2PStr(origName, (char*)pOrigName);
    } else {
        pOrigName[0] = 0;
    }

    Point pt = where ? *where : (Point){100, 100};
    SFPutFile(pt, pPrompt, pOrigName, dlgHook, reply);
}

void sfgetfile(Point *where, char *prompt, FileFilterProcPtr fileFilter, int16_t numTypes,
               SFTypeList typeList, DlgHookProcPtr dlgHook, SFReply *reply)
{
    if (!reply) return;

    /* Convert C string to Pascal string */
    Str255 pPrompt;
    if (prompt) {
        CopyC2PStr(prompt, (char*)pPrompt);
    } else {
        pPrompt[0] = 0;
    }

    Point pt = where ? *where : (Point){100, 100};
    SFGetFile(pt, pPrompt, fileFilter, numTypes, typeList, dlgHook, reply);
}

void sfpputfile(Point *where, char *prompt, char *origName, DlgHookProcPtr dlgHook, SFReply *reply,
                int16_t dlgID, ModalFilterProcPtr filterProc)
{
    sfputfile(where, prompt, origName, dlgHook, reply);
}

void sfpgetfile(Point *where, char *prompt, FileFilterProcPtr fileFilter, int16_t numTypes,
                SFTypeList typeList, DlgHookProcPtr dlgHook, SFReply *reply,
                int16_t dlgID, ModalFilterProcPtr filterProc)
{
    sfgetfile(where, prompt, fileFilter, numTypes, typeList, dlgHook, reply);
}

/**
 * Directory navigation functions
 */
OSErr SF_GetDirectoryListing(const FSSpec *directory, DirectoryListing *listing)
{
    if (!directory || !listing) return paramErr;

    memset(listing, 0, sizeof(DirectoryListing));

    /* Convert FSSpec to path */
    char dirPath[1024];
    SF_FSSpecToPath(directory, dirPath, sizeof(dirPath));

    return SF_ScanDirectory(dirPath, listing);
}

void SF_FreeDirectoryListing(DirectoryListing *listing)
{
    if (!listing) return;

    if (listing->files) {
        free(listing->files);
        listing->files = NULL;
    }
    listing->fileCount = 0;
}

OSErr SF_ChangeDirectory(FSSpec *currentDir, const char *newPath)
{
    if (!currentDir || !newPath) return paramErr;

    if (chdir(newPath) != 0) {
        return -1; /* Directory not found or access denied */
    }

    return SF_PathToFSSpec(newPath, currentDir);
}

OSErr SF_GetParentDirectory(const FSSpec *currentDir, FSSpec *parentDir)
{
    if (!currentDir || !parentDir) return paramErr;

    char currentPath[1024];
    SF_FSSpecToPath(currentDir, currentPath, sizeof(currentPath));

    /* Remove last path component */
    char *lastSlash = strrchr(currentPath, '/');
    if (lastSlash && lastSlash != currentPath) {
        *lastSlash = '\0';
    } else {
        strcpy(currentPath, "/");
    }

    return SF_PathToFSSpec(currentPath, parentDir);
}

Boolean SF_IsValidFilename(const char *filename)
{
    if (!filename || strlen(filename) == 0 || strlen(filename) > 255) {
        return false;
    }

    /* Check for invalid characters */
    const char *invalidChars = "/:?<>\\*|\"";
    for (const char *p = filename; *p; p++) {
        if (strchr(invalidChars, *p)) {
            return false;
        }
        if (*p < 32) { /* Control characters */
            return false;
        }
    }

    /* Check for reserved names */
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return false;
    }

    return true;
}

OSErr SF_CreateNewFolder(const FSSpec *parentDir, const char *folderName, FSSpec *newFolder)
{
    if (!parentDir || !folderName || !newFolder) return paramErr;

    if (!SF_IsValidFilename(folderName)) {
        return paramErr;
    }

    char parentPath[1024];
    SF_FSSpecToPath(parentDir, parentPath, sizeof(parentPath));

    char newFolderPath[1024];
    snprintf(newFolderPath, sizeof(newFolderPath), "%s/%s", parentPath, folderName);

    if (mkdir(newFolderPath, 0755) != 0) {
        return -1; /* Failed to create directory */
    }

    return SF_PathToFSSpec(newFolderPath, newFolder);
}

/**
 * File type and filter management
 */
Boolean SF_FileMatchesFilter(const FileInfo *fileInfo, const SFTypeList typeList, int16_t numTypes)
{
    if (!fileInfo) return false;

    /* Always show folders */
    if (fileInfo->isFolder) return true;

    /* If no filter specified, show all files */
    if (numTypes <= 0) return true;

    /* Check if file type matches any in the list */
    for (int i = 0; i < numTypes && i < 4; i++) {
        if (fileInfo->fileType == typeList[i]) {
            return true;
        }
    }

    return false;
}

void SF_BuildTypeFilter(const SFTypeList typeList, int16_t numTypes, char *filterString, int maxLen)
{
    if (!filterString || maxLen <= 0) return;

    filterString[0] = '\0';

    for (int i = 0; i < numTypes && i < 4; i++) {
        OSType type = typeList[i];
        char typeStr[8];

        /* Convert OSType to string */
        typeStr[0] = (type >> 24) & 0xFF;
        typeStr[1] = (type >> 16) & 0xFF;
        typeStr[2] = (type >> 8) & 0xFF;
        typeStr[3] = type & 0xFF;
        typeStr[4] = '\0';

        if (i > 0) {
            strncat(filterString, ";", maxLen - strlen(filterString) - 1);
        }
        strncat(filterString, typeStr, maxLen - strlen(filterString) - 1);
    }
}

OSErr SF_GetFileInfo(const FSSpec *fileSpec, FileInfo *fileInfo)
{
    if (!fileSpec || !fileInfo) return paramErr;

    memset(fileInfo, 0, sizeof(FileInfo));
    fileInfo->spec = *fileSpec;

    char filePath[1024];
    SF_FSSpecToPath(fileSpec, filePath, sizeof(filePath));

    struct stat statBuf;
    if (stat(filePath, &statBuf) != 0) {
        return -1; /* File not found */
    }

    fileInfo->isFolder = S_ISDIR(statBuf.st_mode);
    fileInfo->isVisible = (strrchr(filePath, '/') == NULL ||
                          strrchr(filePath, '/')[1] != '.'); /* Unix hidden file convention */
    fileInfo->dataSize = statBuf.st_size;
    fileInfo->modDate = statBuf.st_mtime;
    fileInfo->crDate = statBuf.st_ctime;

    /* Set default file type and creator */
    fileInfo->fileType = fileInfo->isFolder ? 'fold' : 'TEXT';
    fileInfo->creator = '????';

    /* Copy display name */
    const char *fileName = strrchr(filePath, '/');
    fileName = fileName ? fileName + 1 : filePath;
    CopyC2PStr(fileName, (char*)fileInfo->displayName);

    return noErr;
}

/**
 * Utility functions
 */
void SF_FSSpecToPath(const FSSpec *spec, char *path, int maxLen)
{
    if (!spec || !path || maxLen <= 0) return;

    /* Convert Pascal string to C string */
    int nameLen = spec->name[0];
    if (nameLen > 63) nameLen = 63;

    char fileName[64];
    memcpy(fileName, spec->name + 1, nameLen);
    fileName[nameLen] = '\0';

    /* For now, just use the file name - a full implementation would
       build the complete path using vRefNum and parID */
    strncpy(path, fileName, maxLen - 1);
    path[maxLen - 1] = '\0';
}

OSErr SF_PathToFSSpec(const char *path, FSSpec *spec)
{
    if (!path || !spec) return paramErr;

    memset(spec, 0, sizeof(FSSpec));

    /* Extract file name from path */
    const char *fileName = strrchr(path, '/');
    fileName = fileName ? fileName + 1 : path;

    /* Convert to Pascal string */
    CopyC2PStr(fileName, (char*)spec->name);

    /* Set default values for vRefNum and parID */
    spec->vRefNum = 0;      /* Default volume */
    spec->parID = 2;        /* Root directory */

    return noErr;
}

Boolean SF_FSSpecEqual(const FSSpec *spec1, const FSSpec *spec2)
{
    if (!spec1 || !spec2) return false;

    return (spec1->vRefNum == spec2->vRefNum &&
            spec1->parID == spec2->parID &&
            memcmp(spec1->name, spec2->name, spec1->name[0] + 1) == 0);
}

OSErr SF_GetFullPath(const FSSpec *spec, char *fullPath, int maxLen)
{
    if (!spec || !fullPath || maxLen <= 0) return paramErr;

    /* For now, just convert the file name */
    SF_FSSpecToPath(spec, fullPath, maxLen);
    return noErr;
}

/**
 * Thread safety
 */
void SF_LockFileDialogs(void)
{
    if (g_sfState.threadSafe) {
        pthread_mutex_lock(&g_sfState.mutex);
    }
}

void SF_UnlockFileDialogs(void)
{
    if (g_sfState.threadSafe) {
        pthread_mutex_unlock(&g_sfState.mutex);
    }
}

/**
 * Configuration functions
 */
void SF_SetDefaultDirectory(const FSSpec *directory)
{
    if (directory) {
        g_sfState.defaultDirectory = *directory;
    }
}

void SF_GetDefaultDirectory(FSSpec *directory)
{
    if (directory) {
        *directory = g_sfState.defaultDirectory;
    }
}

void SF_SetPlatformFileDialogs(const PlatformFileDialogs *dialogs)
{
    if (dialogs) {
        g_sfState.platformDialogs = *dialogs;
    }
}

void SF_SetErrorHandler(void (*handler)(OSErr error, const char *message))
{
    g_sfState.errorHandler = handler;
}

/**
 * Internal helper functions
 */
static void SF_InitializeDefaults(void)
{
    /* Set default directory to current working directory */
    char currentPath[1024];
    if (getcwd(currentPath, sizeof(currentPath))) {
        SF_PathToFSSpec(currentPath, &g_sfState.defaultDirectory);
    }

    g_sfState.preferences = 0;
}

static void SF_DefaultErrorHandler(OSErr error, const char *message)
{
    fprintf(stderr, "StandardFile Error %d: %s\n", error, message ? message : "Unknown error");
}

static Boolean SF_DefaultOpenDialog(const char *title, const char *defaultPath, const char *filter,
                                   char *selectedPath, int maxPathLen)
{
    return SF_ConsoleFileDialog(title ? title : "Open File", defaultPath, false, selectedPath, maxPathLen);
}

static Boolean SF_DefaultSaveDialog(const char *title, const char *defaultPath, const char *defaultName,
                                   const char *filter, char *selectedPath, int maxPathLen)
{
    return SF_ConsoleFileDialog(title ? title : "Save As", defaultPath, true, selectedPath, maxPathLen);
}

static Boolean SF_DefaultFolderDialog(const char *title, const char *defaultPath,
                                     char *selectedPath, int maxPathLen)
{
    return SF_ConsoleFileDialog(title ? title : "Select Folder", defaultPath, false, selectedPath, maxPathLen);
}

static void SF_ConvertSFReplyToStandard(const SFReply *sfReply, StandardFileReply *stdReply)
{
    if (!sfReply || !stdReply) return;

    memset(stdReply, 0, sizeof(StandardFileReply));
    stdReply->sfGood = sfReply->good;
    stdReply->sfReplacing = sfReply->copy;
    stdReply->sfType = sfReply->fType;

    /* Convert to FSSpec */
    stdReply->sfFile.vRefNum = sfReply->vRefNum;
    stdReply->sfFile.parID = 2; /* Default parent ID */
    memcpy(stdReply->sfFile.name, sfReply->fName, sizeof(sfReply->fName));

    stdReply->sfScript = smRoman;
    stdReply->sfFlags = 0;
    stdReply->sfIsFolder = false;
    stdReply->sfIsVolume = false;
}

static void SF_ConvertStandardToSFReply(const StandardFileReply *stdReply, SFReply *sfReply)
{
    if (!stdReply || !sfReply) return;

    memset(sfReply, 0, sizeof(SFReply));
    sfReply->good = stdReply->sfGood;
    sfReply->copy = stdReply->sfReplacing;
    sfReply->fType = stdReply->sfType;
    sfReply->vRefNum = stdReply->sfFile.vRefNum;
    sfReply->version = 0;
    memcpy(sfReply->fName, stdReply->sfFile.name, sizeof(stdReply->sfFile.name));
}

static OSErr SF_ScanDirectory(const char *dirPath, DirectoryListing *listing)
{
    if (!dirPath || !listing) return paramErr;

    DIR *dir = opendir(dirPath);
    if (!dir) return -1;

    /* Count files first */
    int fileCount = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            fileCount++;
        }
    }
    rewinddir(dir);

    /* Allocate file array */
    listing->files = (FileInfo*)malloc(fileCount * sizeof(FileInfo));
    if (!listing->files) {
        closedir(dir);
        return memFullErr;
    }

    /* Fill file information */
    listing->fileCount = 0;
    while ((entry = readdir(dir)) != NULL && listing->fileCount < fileCount) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        FileInfo *fileInfo = &listing->files[listing->fileCount];
        memset(fileInfo, 0, sizeof(FileInfo));

        /* Build full path */
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);

        /* Get file stats */
        struct stat statBuf;
        if (stat(fullPath, &statBuf) == 0) {
            fileInfo->isFolder = S_ISDIR(statBuf.st_mode);
            fileInfo->isVisible = (entry->d_name[0] != '.');
            fileInfo->dataSize = statBuf.st_size;
            fileInfo->modDate = statBuf.st_mtime;
            fileInfo->crDate = statBuf.st_ctime;
            fileInfo->fileType = fileInfo->isFolder ? 'fold' : 'TEXT';
            fileInfo->creator = '????';

            /* Set FSSpec */
            SF_PathToFSSpec(fullPath, &fileInfo->spec);

            /* Set display name */
            CopyC2PStr(entry->d_name, (char*)fileInfo->displayName);

            listing->fileCount++;
        }
    }

    closedir(dir);
    return noErr;
}

static Boolean SF_ConsoleFileDialog(const char *prompt, const char *defaultPath,
                                   Boolean isSave, char *result, int maxLen)
{
    if (!prompt || !result || maxLen <= 0) return false;

    printf("\n%s\n", prompt);
    if (defaultPath) {
        printf("Current directory: %s\n", defaultPath);
    }

    if (isSave) {
        printf("Enter filename to save: ");
    } else {
        printf("Enter filename to open (or 'ls' to list files): ");
    }

    fflush(stdout);

    char input[1024];
    if (!fgets(input, sizeof(input), stdin)) {
        return false;
    }

    /* Remove newline */
    char *newline = strchr(input, '\n');
    if (newline) *newline = '\0';

    /* Handle special commands */
    if (strcmp(input, "ls") == 0) {
        /* List current directory */
        system("ls -la");
        return SF_ConsoleFileDialog(prompt, defaultPath, isSave, result, maxLen);
    }

    if (strlen(input) == 0) {
        return false; /* User cancelled */
    }

    /* Copy result */
    strncpy(result, input, maxLen - 1);
    result[maxLen - 1] = '\0';

    /* For absolute paths, use as-is; otherwise prepend current directory */
    if (input[0] != '/' && defaultPath) {
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", defaultPath, input);
        strncpy(result, fullPath, maxLen - 1);
        result[maxLen - 1] = '\0';
    }

    return true;
}

/**
 * Memory management
 */
void *SF_AllocMem(size_t size)
{
    return malloc(size);
}

void SF_FreeMem(void *ptr)
{
    free(ptr);
}

void *SF_ReallocMem(void *ptr, size_t newSize)
{
    return realloc(ptr, newSize);
}

/**
 * Cleanup function
 */
void CleanupStandardFilePackage(void)
{
    if (g_sfState.threadSafe) {
        pthread_mutex_destroy(&g_sfState.mutex);
    }

    g_sfState.initialized = false;
}