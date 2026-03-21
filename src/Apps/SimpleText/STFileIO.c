/*
 * STFileIO.c - SimpleText File I/O Operations
 *
 * Handles file reading/writing and file dialogs
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "Apps/SimpleText.h"
#include "MemoryMgr/MemoryManager.h"
#include "FS/vfs.h"

/* Forward declaration */
void STView_Draw(STDocument* doc);
extern void serial_puts(const char* str);

#define STIO_MAX_HFS_NAME   31
#define STIO_MAX_CACHED_DOCS 16

typedef struct {
    char     path[512];
    char*    data;
    uint32_t length;
} STIOSavedDocEntry;

static STIOSavedDocEntry g_stioSavedDocs[STIO_MAX_CACHED_DOCS];

static const char* STIO_LastSlash(const char* path) {
    if (!path) return NULL;
    const char* last = NULL;
    for (const char* p = path; *p; ++p) {
        if (*p == '/') {
            last = p;
        }
    }
    return last;
}

static VRefNum STIO_GetBootVRef(void) {
    VRefNum vref = VFS_GetBootVRef();
    if (vref == 0) {
        /* Boot volume defaults to 1 in our VFS implementation */
        vref = 1;
    }
    return vref;
}

static Boolean STIO_GetBootVolumeInfo(VolumeControlBlock* vcbOut) {
    VolumeControlBlock vcb;
    VRefNum vref = STIO_GetBootVRef();
    if (!VFS_GetVolumeInfo(vref, &vcb)) {
        return false;
    }
    if (vcbOut) {
        *vcbOut = vcb;
    }
    return true;
}

static SInt32 STIO_SetText(STDocument* doc, const char* text, SInt32 length) {
    if (!doc || !doc->hTE) return 0;
    GrafPtr oldPort;
    GetPort(&oldPort);
    SetPort((GrafPtr)doc->window);

    TESetSelect(0, 32767, doc->hTE);
    TEDelete(doc->hTE);
    if (text && length > 0) {
        /* Normalize line endings to classic Mac CR */
        char* normalized = (char*)NewPtr((size_t)length);
        if (!normalized) {
            return 0;
        }
        SInt32 outLen = 0;
        for (SInt32 i = 0; i < length; i++) {
            char ch = text[i];
            if (ch == '\n') {
                normalized[outLen++] = '\r';
            } else if (ch == '\r') {
                normalized[outLen++] = '\r';
            } else {
                normalized[outLen++] = ch;
            }
        }
        TEInsert(normalized, outLen, doc->hTE);
        DisposePtr((Ptr)normalized);
        length = outLen;
    }
    TESetSelect(0, 0, doc->hTE);
    TECalText(doc->hTE);
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[STIO] SetText in=%d out=%d\n",
             (int)length, (int)((*doc->hTE)->teLength));
    serial_puts(logBuf);
    /* Force immediate redraw */
    STView_ForceDraw(doc);

    SetPort(oldPort);
    return length > 0 ? length : 0;
}

static char STIO_ToLower(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

static Boolean STIO_EqualsIgnoreCase(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (STIO_ToLower(*a) != STIO_ToLower(*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static SInt32 STIO_LoadBuiltinDocument(STDocument* doc, const char* leafName) {
    /* Provide default content for known sample files bundled with the system */
    if (!leafName || !leafName[0] || !doc) return 0;

    ST_Log("STIO_LoadBuiltinDocument: leafName='%s'\n", leafName);

    const char* text = NULL;

    if (STIO_EqualsIgnoreCase(leafName, "Read Me")) {
        text =
            "Welcome to System 7.1 Portable!\n"
            "\n"
            "This early build includes:\n"
            "• Finder with desktop icons\n"
            "• SimpleText for viewing documents\n"
            "• Partial Toolbox implementations\n"
            "\n"
            "Try opening the \"About This Mac\" document for system stats.\n";
    } else if (STIO_EqualsIgnoreCase(leafName, "About This Mac")) {
        text =
            "About This Macintosh\n"
            "---------------------\n"
            "\n"
            "System Version: 7.1 Portable Preview\n"
            "Memory: 4 MB (simulated)\n"
            "Processor: 80386 (emulated)\n"
            "\n"
            "This build focuses on windowing, Finder UI, and classic\n"
            "Toolbox behaviours needed for early software bring-up.\n";
    } else if (STIO_EqualsIgnoreCase(leafName, "Sample Document")) {
        text =
            "Sample Document\n"
            "\n"
            "This file demonstrates SimpleText's ability to open and\n"
            "display text files sourced from the virtual HFS volume.\n"
            "\n"
            "Feel free to experiment by editing this file and saving it.\n";
    } else if (STIO_EqualsIgnoreCase(leafName, "Notes")) {
        text =
            "Notes\n"
            "-----\n"
            "\n"
            "- Drag windows by the title bar\n"
            "- Close windows with the top-left box\n"
            "- Use the Finder desktop to open documents\n"
            "- SimpleText currently saves within this session only\n";
    }

    if (!text) {
        return 0;
    }

    SInt32 length = (SInt32)strlen(text);
    return STIO_SetText(doc, text, length);
}

static void STIO_UpdateDocumentMetadata(STDocument* doc,
                                        const char* path,
                                        const char* leafName,
                                        const CatEntry* entry,
                                        SInt32 textLen) {
    if (!doc) return;

    if (path) {
        strncpy(doc->filePath, path, sizeof(doc->filePath) - 1);
        doc->filePath[sizeof(doc->filePath) - 1] = '\0';
    }

    if (leafName && leafName[0]) {
        size_t len = strlen(leafName);
        if (len > 255) len = 255;
        doc->fileName[0] = (unsigned char)len;
        memcpy(&doc->fileName[1], leafName, len);
    } else if (path) {
        /* Derive from path if not explicitly provided */
        const char* lastSlash = STIO_LastSlash(path);
        const char* name = lastSlash ? lastSlash + 1 : path;
        size_t len = strlen(name);
        if (len > 255) len = 255;
        doc->fileName[0] = (unsigned char)len;
        memcpy(&doc->fileName[1], name, len);
    }

    if (entry) {
        doc->fileType = entry->type ? entry->type : 'TEXT';
        doc->fileCreator = entry->creator ? entry->creator : 'ttxt';
    } else {
        doc->fileType = 'TEXT';
        doc->fileCreator = 'ttxt';
    }

    doc->lastSaveLen = textLen;
    doc->dirty = false;
    doc->untitled = false;
}

static STIOSavedDocEntry* STIO_FindSavedDocument(const char* path) {
    if (!path || !path[0]) return NULL;
    for (int i = 0; i < STIO_MAX_CACHED_DOCS; i++) {
        if (g_stioSavedDocs[i].path[0] == '\0') {
            continue;
        }
        if (strcmp(g_stioSavedDocs[i].path, path) == 0) {
            return &g_stioSavedDocs[i];
        }
    }
    return NULL;
}

static STIOSavedDocEntry* STIO_AllocateSavedSlot(const char* path) {
    if (!path || !path[0]) return NULL;

    STIOSavedDocEntry* freeSlot = NULL;
    for (int i = 0; i < STIO_MAX_CACHED_DOCS; i++) {
        if (g_stioSavedDocs[i].path[0] == '\0') {
            if (!freeSlot) {
                freeSlot = &g_stioSavedDocs[i];
            }
        } else if (strcmp(g_stioSavedDocs[i].path, path) == 0) {
            return &g_stioSavedDocs[i];
        }
    }

    if (!freeSlot) {
        /* Simple LRU: reuse first slot */
        freeSlot = &g_stioSavedDocs[0];
        if (freeSlot->data) {
            DisposePtr((Ptr)freeSlot->data);
        }
    }

    memset(freeSlot, 0, sizeof(*freeSlot));
    strncpy(freeSlot->path, path, sizeof(freeSlot->path) - 1);
    return freeSlot;
}

static Boolean STIO_SplitPath(VRefNum vref,
                              DirID rootDir,
                              const char* path,
                              DirID* outParentDir,
                              char* leafName,
                              size_t leafCapacity,
                              CatEntry* outLeafEntry,
                              Boolean* outLeafExists) {
    if (!path || !path[0]) {
        return false;
    }

    DirID currentDir = rootDir;
    const char* cursor = path;

    /* Skip leading slashes */
    while (*cursor == '/') cursor++;

    if (*cursor == '\0') {
        if (outParentDir) *outParentDir = currentDir;
        if (leafName && leafCapacity) leafName[0] = '\0';
        if (outLeafEntry) memset(outLeafEntry, 0, sizeof(CatEntry));
        if (outLeafExists) *outLeafExists = false;
        return true;
    }

    while (*cursor != '\0') {
        while (*cursor == '/') cursor++;
        if (*cursor == '\0') break;

        const char* nextSlash = cursor;
        while (*nextSlash != '\0' && *nextSlash != '/') {
            nextSlash++;
        }

        size_t len = (size_t)(nextSlash - cursor);
        if (len == 0 || len > STIO_MAX_HFS_NAME) {
            return false;
        }

        char component[STIO_MAX_HFS_NAME + 1];
        memcpy(component, cursor, len);
        component[len] = '\0';

        Boolean isLast = (*nextSlash == '\0');
        CatEntry entry;
        if (!VFS_Lookup(vref, currentDir, component, &entry)) {
            if (!isLast) {
                return false;  /* Intermediate directory missing */
            }

            if (outParentDir) *outParentDir = currentDir;
            if (leafName && leafCapacity) {
                if (len >= leafCapacity) len = leafCapacity - 1;
                memcpy(leafName, component, len);
                leafName[len] = '\0';
            }
            if (outLeafEntry) memset(outLeafEntry, 0, sizeof(CatEntry));
            if (outLeafExists) *outLeafExists = false;
            return true;
        }

        if (isLast) {
            if (outParentDir) *outParentDir = currentDir;
            if (leafName && leafCapacity) {
                if (len >= leafCapacity) len = leafCapacity - 1;
                memcpy(leafName, component, len);
                leafName[len] = '\0';
            }
            if (outLeafEntry) *outLeafEntry = entry;
            if (outLeafExists) *outLeafExists = true;
            return true;
        }

        if (entry.kind != kNodeDir) {
            return false;
        }

        currentDir = (DirID)entry.id;
        cursor = nextSlash;
    }

    if (outParentDir) *outParentDir = currentDir;
    if (leafName && leafCapacity) leafName[0] = '\0';
    if (outLeafEntry) memset(outLeafEntry, 0, sizeof(CatEntry));
    if (outLeafExists) *outLeafExists = false;
    return true;
}

static void STIO_LoadSampleText(STDocument* doc) {
    const char* sampleText =
        "Welcome to SimpleText!\n"
        "\n"
        "This is a simple text editor for System 7.1.\n"
        "You can:\n"
        "- Type and edit text\n"
        "- Use standard keyboard shortcuts\n"
        "- Save and open files\n"
        "- Change fonts and styles\n"
        "\n"
        "Enjoy using SimpleText!";

    STIO_SetText(doc, sampleText, (SInt32)strlen(sampleText));
}


/* Read file into document */
Boolean STIO_ReadFile(STDocument* doc, const char* path)
{
    ST_Log("STIO_ReadFile: %s", path ? path : "(null)");

    if (!doc || !path || !doc->hTE) {
        return false;
    }

    VolumeControlBlock vcb;
    if (!STIO_GetBootVolumeInfo(&vcb)) {
        ST_Log("STIO_ReadFile: VFS not available, falling back to sample text");
        STIO_LoadSampleText(doc);
        doc->untitled = true;
        doc->dirty = false;
        doc->fileName[0] = 0;
        doc->filePath[0] = '\0';
        doc->lastSaveLen = 0;
        doc->fileType = 'TEXT';
        doc->fileCreator = 'ttxt';
        return true;
    }

    char leafName[STIO_MAX_HFS_NAME + 1] = {0};
    CatEntry entry = {0};
    DirID parentDir = vcb.rootID;
    Boolean entryExists = false;

    if (!STIO_SplitPath(vcb.vRefNum, vcb.rootID, path,
                        &parentDir, leafName, sizeof(leafName),
                        &entry, &entryExists)) {
        ST_Log("STIO_ReadFile: Invalid path '%s'", path);
        goto fallback;
    }

    if (entryExists && entry.kind == kNodeFile) {
        VFSFile* file = VFS_OpenFile(vcb.vRefNum, entry.id, false);
        if (file) {
            uint32_t fileSize = VFS_GetFileSize(file);
            Handle textHandle = NewHandle(fileSize);
            Boolean success = true;

            if (!textHandle && fileSize > 0) {
                success = false;
            }

            if (success && fileSize > 0) {
                HLock(textHandle);
                char* buffer = *textHandle;
                uint32_t totalRead = 0;

                while (totalRead < fileSize) {
                    uint32_t chunk = 0;
                    if (!VFS_ReadFile(file, buffer + totalRead, fileSize - totalRead, &chunk)) {
                        success = false;
                        break;
                    }
                    if (chunk == 0) {
                        break;
                    }
                    totalRead += chunk;
                }

                if (totalRead != fileSize) {
                    success = false;
                }
                HUnlock(textHandle);
            }

            VFS_CloseFile(file);

            if (success) {
                SInt32 textLen = (SInt32)fileSize;
                SInt32 finalLen = textLen;

                if (textHandle) {
                    HLock(textHandle);
                }

                finalLen = STIO_SetText(doc, (textHandle && textLen > 0) ? *textHandle : NULL, textLen);

                if (textHandle) {
                    HUnlock(textHandle);
                    DisposeHandle(textHandle);
                }

                if (finalLen == 0) {
                    SInt32 builtinLen = STIO_LoadBuiltinDocument(doc, leafName);
                    if (builtinLen > 0) {
                        finalLen = builtinLen;
                    }
                }

                STIO_UpdateDocumentMetadata(doc, path, leafName, &entry, finalLen);
                doc->lastSaveLen = finalLen;
                doc->dirty = false;
                char logBuf[128];
                snprintf(logBuf, sizeof(logBuf), "[STIO] VFS '%s' len=%d teLen=%d\n",
                         leafName, (int)finalLen, (*doc->hTE)->teLength);
                serial_puts(logBuf);
                return true;
            }

            if (textHandle) {
                DisposeHandle(textHandle);
            }
        } else {
            SInt32 builtinLen = STIO_LoadBuiltinDocument(doc, leafName);
            if (builtinLen > 0) {
                STIO_UpdateDocumentMetadata(doc, path, leafName, &entry, builtinLen);
                doc->lastSaveLen = builtinLen;
                doc->dirty = false;
                char logBuf[128];
                snprintf(logBuf, sizeof(logBuf), "[STIO] Builtin '%s' len=%d teLen=%d\n",
                         leafName, (int)builtinLen, (*doc->hTE)->teLength);
                serial_puts(logBuf);
                return true;
            }
        }
    }

    /* Check in-memory cache for documents saved during this session */
    {
        STIOSavedDocEntry* cached = STIO_FindSavedDocument(path);
        if (cached && (cached->data || cached->length == 0)) {
            SInt32 cachedLen = 0;
            if (cached->length > 0 && cached->data) {
                cachedLen = STIO_SetText(doc, cached->data, (SInt32)cached->length);
            } else {
                STIO_SetText(doc, NULL, 0);
            }
             STIO_UpdateDocumentMetadata(doc, path, leafName, entryExists ? &entry : NULL, cachedLen);
             doc->lastSaveLen = cachedLen;
             doc->dirty = false;
             char logBuf[128];
             snprintf(logBuf, sizeof(logBuf), "[STIO] Cached '%s' len=%d teLen=%d\n",
                      leafName, (int)cachedLen, (*doc->hTE)->teLength);
             serial_puts(logBuf);
            return true;
        }
    }

fallback:
    ST_Log("STIO_ReadFile: Falling back to sample text for %s", path);
    SInt32 builtinLen = (leafName[0] != '\0') ? STIO_LoadBuiltinDocument(doc, leafName) : 0;
    if (builtinLen <= 0) {
        STIO_LoadSampleText(doc);
        builtinLen = (*doc->hTE)->teLength;
    }
    STIO_UpdateDocumentMetadata(doc, path, (leafName[0] != '\0') ? leafName : path, entryExists ? &entry : NULL, builtinLen);
    doc->untitled = false;
    doc->dirty = false;
    doc->lastSaveLen = builtinLen;
    {
        char logBuf[128];
        snprintf(logBuf, sizeof(logBuf), "[STIO] Fallback '%s' len=%d teLen=%d\n",
                 (leafName[0] != '\0') ? leafName : path,
                 (int)builtinLen, (*doc->hTE)->teLength);
        serial_puts(logBuf);
    }
    return true;
}

/* Write document to file */
Boolean STIO_WriteFile(STDocument* doc, const char* path)
{
    ST_Log("STIO_WriteFile: %s", path ? path : "(null)");

    if (!doc || !doc->hTE || !path) {
        return false;
    }

    CharsHandle textHandle = TEGetText(doc->hTE);
    if (!textHandle) {
        return false;
    }

    SInt32 textLen = (*doc->hTE)->teLength;
    char* newData = NULL;

    if (textLen > 0) {
        HLock((Handle)textHandle);
        newData = (char*)NewPtr((size_t)textLen);
        if (newData) {
            memcpy(newData, *textHandle, (size_t)textLen);
        }
        HUnlock((Handle)textHandle);

        if (!newData) {
            ST_Log("STIO_WriteFile: Failed to allocate buffer for save");
            return false;
        }
    }

    STIOSavedDocEntry* slot = STIO_AllocateSavedSlot(path);
    if (!slot) {
        if (newData) {
            DisposePtr((Ptr)newData);
        }
        return false;
    }

    if (slot->data) {
        DisposePtr((Ptr)slot->data);
        slot->data = NULL;
        slot->length = 0;
    }

    slot->data = newData;
    slot->length = (newData ? (uint32_t)textLen : 0);

    VolumeControlBlock vcb;
    char leafName[STIO_MAX_HFS_NAME + 1] = {0};
    DirID parentDir = 0;
    CatEntry leafEntry = {0};
    Boolean entryExists = false;

    if (STIO_GetBootVolumeInfo(&vcb)) {
        if (STIO_SplitPath(vcb.vRefNum, vcb.rootID, path,
                           &parentDir, leafName, sizeof(leafName),
                           &leafEntry, &entryExists)) {
            if (!entryExists && leafName[0] != '\0') {
                if (VFS_CreateFile(vcb.vRefNum, parentDir, leafName, 'TEXT', 'ttxt', &leafEntry.id)) {
                    leafEntry.kind = kNodeFile;
                    leafEntry.type = 'TEXT';
                    leafEntry.creator = 'ttxt';
                    entryExists = true;
                }
            }

            /* Write file data to VFS */
            if (entryExists && newData && textLen > 0) {
                VFSFile* vf = VFS_OpenFile(vcb.vRefNum, leafEntry.id, false);
                if (vf) {
                    VFS_WriteFile(vf, newData, (uint32_t)textLen, NULL);
                    VFS_CloseFile(vf);
                }
            }
        } else {
            /* Fallback: derive leaf name for metadata */
            const char* lastSlash = STIO_LastSlash(path);
            const char* name = lastSlash ? lastSlash + 1 : path;
            strncpy(leafName, name, sizeof(leafName) - 1);
            leafName[sizeof(leafName) - 1] = '\0';
        }
    } else {
        const char* lastSlash = STIO_LastSlash(path);
        const char* name = lastSlash ? lastSlash + 1 : path;
        strncpy(leafName, name, sizeof(leafName) - 1);
        leafName[sizeof(leafName) - 1] = '\0';
    }

    STIO_SetFileInfo(path, 'TEXT', 'ttxt');
    STIO_UpdateDocumentMetadata(doc, path, leafName, entryExists ? &leafEntry : NULL, textLen);
    doc->dirty = false;
    return true;
}

/* Save dialog - returns path in pathOut */
Boolean STIO_SaveDialog(STDocument* doc, char* pathOut)
{
    ST_Log("STIO_SaveDialog");

    if (!doc || !pathOut) {
        return false;
    }

    #define MAX_DIALOG_PATH 512

    /* Build prompt and default name as Pascal strings */
    static unsigned char prompt[] = {8, 'S','a','v','e',' ','a','s',':'};
    unsigned char defaultName[256];

    if (doc->untitled || !doc->filePath[0]) {
        static const char* defName = "Untitled";
        int len = strlen(defName);
        defaultName[0] = (unsigned char)len;
        memcpy(&defaultName[1], defName, len);
    } else {
        /* Extract leaf name from path */
        const char* lastSlash = STIO_LastSlash(doc->filePath);
        const char* name = lastSlash ? lastSlash + 1 : doc->filePath;
        int len = strlen(name);
        if (len > 255) len = 255;
        defaultName[0] = (unsigned char)len;
        memcpy(&defaultName[1], name, len);
    }

    /* Show the Standard File save dialog */
    StandardFileReply reply;
    extern void StandardPutFile(const unsigned char* prompt,
                                const unsigned char* defaultName,
                                StandardFileReply* reply);
    StandardPutFile(prompt, defaultName, &reply);

    if (!reply.sfGood) {
        ST_Log("Save dialog cancelled");
        return false;
    }

    /* Convert FSSpec name (Pascal) to C path */
    /* Build path: /<name> */
    int nameLen = reply.sfFile.name[0];
    if (nameLen <= 0 || nameLen >= MAX_DIALOG_PATH - 2) {
        return false;
    }
    pathOut[0] = '/';
    memcpy(&pathOut[1], &reply.sfFile.name[1], nameLen);
    pathOut[1 + nameLen] = '\0';

    ST_Log("Save dialog returned: %s", pathOut);
    return true;
}

/* Open dialog - returns path in pathOut */
Boolean STIO_OpenDialog(char* pathOut)
{
    ST_Log("STIO_OpenDialog");

    if (!pathOut) {
        return false;
    }

    /* Show the Standard File open dialog - accept TEXT files */
    StandardFileReply reply;
    OSType typeList[1] = { 'TEXT' };
    extern void StandardGetFile(void* fileFilter, short numTypes,
                                const OSType* typeList, StandardFileReply* reply);
    StandardGetFile(NULL, 1, typeList, &reply);

    if (!reply.sfGood) {
        ST_Log("Open dialog cancelled");
        return false;
    }

    /* Convert FSSpec name (Pascal) to C path */
    int nameLen = reply.sfFile.name[0];
    if (nameLen <= 0 || nameLen >= MAX_DIALOG_PATH - 2) {
        return false;
    }
    pathOut[0] = '/';
    memcpy(&pathOut[1], &reply.sfFile.name[1], nameLen);
    pathOut[1 + nameLen] = '\0';

    ST_Log("Open dialog returned: %s", pathOut);
    return true;
}

/* Set file type and creator codes */
void STIO_SetFileInfo(const char* path, OSType type, OSType creator)
{
    ST_Log("STIO_SetFileInfo: %s type='%.4s' creator='%.4s'",
           path, (char*)&type, (char*)&creator);

    /* In real implementation, would use File Manager to set file info */
    /* For now, just log the operation */
}
