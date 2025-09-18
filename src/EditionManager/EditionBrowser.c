/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * EditionBrowser.c
 *
 * Edition browsing and selection interface for Edition Manager
 * Provides UI for finding, selecting, and managing edition containers
 *
 * This module implements the classic Mac OS edition browser dialogs
 * with modern platform abstraction for cross-platform compatibility
 */

#include "EditionManager/EditionManager.h"
#include "EditionManager/EditionManagerPrivate.h"
#include "EditionManager/EditionBrowser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

/* Edition browser state */
typedef struct {
    EditionContainerSpec* editions;     /* Array of found editions */
    int32_t editionCount;              /* Number of editions */
    int32_t maxEditions;               /* Maximum editions in array */
    char currentPath[1024];            /* Current browse path */
    FormatType filterFormat;           /* Format filter (0 = all) */
    bool showPreview;                  /* Show preview pane */
    EditionBrowserCallback callback;   /* Selection callback */
    void* userData;                    /* User data for callback */
} EditionBrowserState;

/* Global browser state */
static EditionBrowserState* gBrowserState = NULL;

/* Internal helper functions */
static OSErr InitializeBrowserState(void);
static void CleanupBrowserState(void);
static OSErr ScanDirectoryForEditions(const char* path);
static bool IsEditionFile(const char* filePath);
static OSErr GetEditionPreview(const EditionContainerSpec* container,
                              Handle* preview, FormatType* previewFormat);
static OSErr CreateEditionFromPath(const char* filePath, EditionContainerSpec* container);

/*
 * ShowNewSubscriberDialog
 *
 * Display the standard "New Subscriber" dialog for selecting an edition.
 */
OSErr ShowNewSubscriberDialog(NewSubscriberReply* reply)
{
    if (!reply) {
        return badSectionErr;
    }

    /* Initialize reply */
    memset(reply, 0, sizeof(NewSubscriberReply));
    reply->canceled = true;

    /* Initialize browser state */
    OSErr err = InitializeBrowserState();
    if (err != noErr) {
        return err;
    }

    /* Scan current directory for editions */
    err = ScanDirectoryForEditions(".");
    if (err != noErr) {
        CleanupBrowserState();
        return err;
    }

    /* Display platform-specific subscriber dialog */
    err = ShowPlatformSubscriberDialog(reply);

    CleanupBrowserState();
    return err;
}

/*
 * ShowNewPublisherDialog
 *
 * Display the standard "New Publisher" dialog for creating an edition.
 */
OSErr ShowNewPublisherDialog(NewPublisherReply* reply)
{
    if (!reply) {
        return badSectionErr;
    }

    /* Initialize reply */
    memset(reply, 0, sizeof(NewPublisherReply));
    reply->canceled = true;

    /* Display platform-specific publisher dialog */
    return ShowPlatformPublisherDialog(reply);
}

/*
 * ShowSectionOptionsDialog
 *
 * Display the section options dialog for configuring sections.
 */
OSErr ShowSectionOptionsDialog(SectionOptionsReply* reply)
{
    if (!reply || !reply->sectionH) {
        return badSectionErr;
    }

    /* Initialize reply */
    reply->canceled = true;
    reply->changed = false;
    reply->action = 0;

    /* Display platform-specific section options dialog */
    return ShowPlatformSectionOptionsDialog(reply);
}

/*
 * BrowseForEdition
 *
 * Browse for edition containers in the file system.
 */
OSErr BrowseForEdition(const char* startPath, EditionContainerSpec* selectedContainer)
{
    if (!selectedContainer) {
        return badSectionErr;
    }

    /* Initialize browser state */
    OSErr err = InitializeBrowserState();
    if (err != noErr) {
        return err;
    }

    /* Set starting path */
    if (startPath) {
        strncpy(gBrowserState->currentPath, startPath, sizeof(gBrowserState->currentPath) - 1);
        gBrowserState->currentPath[sizeof(gBrowserState->currentPath) - 1] = '\0';
    } else {
        strcpy(gBrowserState->currentPath, ".");
    }

    /* Scan for editions */
    err = ScanDirectoryForEditions(gBrowserState->currentPath);
    if (err != noErr) {
        CleanupBrowserState();
        return err;
    }

    /* Display browser interface */
    err = ShowEditionBrowserInterface(selectedContainer);

    CleanupBrowserState();
    return err;
}

/*
 * GetAvailableEditions
 *
 * Get list of available edition containers in a directory.
 */
OSErr GetAvailableEditions(const char* directoryPath,
                          EditionContainerSpec** editions,
                          int32_t* editionCount)
{
    if (!editions || !editionCount) {
        return badSectionErr;
    }

    *editions = NULL;
    *editionCount = 0;

    /* Initialize temporary browser state */
    OSErr err = InitializeBrowserState();
    if (err != noErr) {
        return err;
    }

    /* Scan directory */
    const char* scanPath = directoryPath ? directoryPath : ".";
    err = ScanDirectoryForEditions(scanPath);
    if (err != noErr) {
        CleanupBrowserState();
        return err;
    }

    /* Copy results */
    if (gBrowserState->editionCount > 0) {
        Size arraySize = sizeof(EditionContainerSpec) * gBrowserState->editionCount;
        *editions = (EditionContainerSpec*)malloc(arraySize);
        if (*editions) {
            memcpy(*editions, gBrowserState->editions, arraySize);
            *editionCount = gBrowserState->editionCount;
        } else {
            err = editionMgrInitErr;
        }
    }

    CleanupBrowserState();
    return err;
}

/*
 * FilterEditionsByFormat
 *
 * Filter edition list by supported format.
 */
OSErr FilterEditionsByFormat(const EditionContainerSpec* editions,
                            int32_t editionCount,
                            FormatType format,
                            EditionContainerSpec** filteredEditions,
                            int32_t* filteredCount)
{
    if (!editions || !filteredEditions || !filteredCount) {
        return badSectionErr;
    }

    *filteredEditions = NULL;
    *filteredCount = 0;

    /* Count matching editions */
    int32_t matchCount = 0;
    for (int32_t i = 0; i < editionCount; i++) {
        /* Check if edition supports the format */
        bool supportsFormat = false;
        OSErr err = EditionSupportsFormat((EditionRefNum)&editions[i], format, &supportsFormat);
        if (err == noErr && supportsFormat) {
            matchCount++;
        }
    }

    if (matchCount == 0) {
        return noErr;  /* No matching editions */
    }

    /* Allocate filtered array */
    *filteredEditions = (EditionContainerSpec*)malloc(sizeof(EditionContainerSpec) * matchCount);
    if (!*filteredEditions) {
        return editionMgrInitErr;
    }

    /* Copy matching editions */
    int32_t copyIndex = 0;
    for (int32_t i = 0; i < editionCount && copyIndex < matchCount; i++) {
        bool supportsFormat = false;
        OSErr err = EditionSupportsFormat((EditionRefNum)&editions[i], format, &supportsFormat);
        if (err == noErr && supportsFormat) {
            (*filteredEditions)[copyIndex++] = editions[i];
        }
    }

    *filteredCount = copyIndex;
    return noErr;
}

/*
 * GetEditionPreviewInfo
 *
 * Get preview information for an edition.
 */
OSErr GetEditionPreviewInfo(const EditionContainerSpec* container,
                           EditionPreviewInfo* previewInfo)
{
    if (!container || !previewInfo) {
        return badSectionErr;
    }

    memset(previewInfo, 0, sizeof(EditionPreviewInfo));

    /* Get edition info */
    EditionInfoRecord editionInfo;
    OSErr err = GetEditionInfo((SectionHandle)container, &editionInfo);
    if (err != noErr) {
        return err;
    }

    /* Fill preview info */
    previewInfo->creationDate = editionInfo.crDate;
    previewInfo->modificationDate = editionInfo.mdDate;
    previewInfo->fileCreator = editionInfo.fdCreator;
    previewInfo->fileType = editionInfo.fdType;

    /* Get format information */
    EditionRefNum refNum;
    err = OpenEditionFileInternal(&container->theFile, false, (EditionFileBlock**)&refNum);
    if (err == noErr) {
        /* Get supported formats */
        err = GetEditionFormats(refNum, &previewInfo->supportedFormats, &previewInfo->formatCount);

        /* Get file size */
        struct stat fileStat;
        if (stat(container->theFile.path, &fileStat) == 0) {
            previewInfo->fileSize = fileStat.st_size;
        }

        /* Try to get preview data */
        Size previewSize = 0;
        if (CheckFormatInEditionFile((void*)refNum, kPreviewFormat, &previewSize) == noErr) {
            previewInfo->hasPreview = true;
            previewInfo->previewFormat = kPreviewFormat;
            previewInfo->previewSize = previewSize;

            /* Read preview data if small enough */
            if (previewSize <= sizeof(previewInfo->previewData)) {
                Size actualSize = previewSize;
                ReadDataFromEditionFile((void*)refNum, kPreviewFormat,
                                      previewInfo->previewData, &actualSize);
            }
        }

        CloseEditionFileInternal((EditionFileBlock*)refNum);
    }

    return noErr;
}

/*
 * SetEditionBrowserFilter
 *
 * Set filter criteria for edition browser.
 */
OSErr SetEditionBrowserFilter(FormatType format, bool showPreview)
{
    if (!gBrowserState) {
        OSErr err = InitializeBrowserState();
        if (err != noErr) {
            return err;
        }
    }

    gBrowserState->filterFormat = format;
    gBrowserState->showPreview = showPreview;

    return noErr;
}

/*
 * SetEditionBrowserCallback
 *
 * Set callback for edition selection events.
 */
OSErr SetEditionBrowserCallback(EditionBrowserCallback callback, void* userData)
{
    if (!gBrowserState) {
        OSErr err = InitializeBrowserState();
        if (err != noErr) {
            return err;
        }
    }

    gBrowserState->callback = callback;
    gBrowserState->userData = userData;

    return noErr;
}

/* Internal helper functions */

static OSErr InitializeBrowserState(void)
{
    if (gBrowserState) {
        return noErr;  /* Already initialized */
    }

    gBrowserState = (EditionBrowserState*)malloc(sizeof(EditionBrowserState));
    if (!gBrowserState) {
        return editionMgrInitErr;
    }

    memset(gBrowserState, 0, sizeof(EditionBrowserState));

    /* Initialize state */
    gBrowserState->maxEditions = 64;
    gBrowserState->editions = (EditionContainerSpec*)malloc(
        sizeof(EditionContainerSpec) * gBrowserState->maxEditions);

    if (!gBrowserState->editions) {
        free(gBrowserState);
        gBrowserState = NULL;
        return editionMgrInitErr;
    }

    gBrowserState->editionCount = 0;
    strcpy(gBrowserState->currentPath, ".");
    gBrowserState->filterFormat = 0;  /* No filter */
    gBrowserState->showPreview = true;
    gBrowserState->callback = NULL;
    gBrowserState->userData = NULL;

    return noErr;
}

static void CleanupBrowserState(void)
{
    if (!gBrowserState) {
        return;
    }

    if (gBrowserState->editions) {
        free(gBrowserState->editions);
    }

    free(gBrowserState);
    gBrowserState = NULL;
}

static OSErr ScanDirectoryForEditions(const char* path)
{
    if (!gBrowserState) {
        return badSectionErr;
    }

    DIR* dir = opendir(path);
    if (!dir) {
        return fnfErr;
    }

    gBrowserState->editionCount = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && gBrowserState->editionCount < gBrowserState->maxEditions) {
        /* Skip hidden files and directories */
        if (entry->d_name[0] == '.') {
            continue;
        }

        /* Build full path */
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        /* Check if it's an edition file */
        if (IsEditionFile(fullPath)) {
            EditionContainerSpec* container = &gBrowserState->editions[gBrowserState->editionCount];
            OSErr err = CreateEditionFromPath(fullPath, container);
            if (err == noErr) {
                /* Apply format filter if set */
                if (gBrowserState->filterFormat == 0 ||
                    EditionSupportsFormat((EditionRefNum)container, gBrowserState->filterFormat, NULL) == noErr) {
                    gBrowserState->editionCount++;
                }
            }
        }
    }

    closedir(dir);
    return noErr;
}

static bool IsEditionFile(const char* filePath)
{
    /* Check file extension */
    const char* ext = strrchr(filePath, '.');
    if (ext) {
        if (strcmp(ext, ".edition") == 0 ||
            strcmp(ext, ".edtt") == 0 ||
            strcmp(ext, ".edtp") == 0 ||
            strcmp(ext, ".edts") == 0 ||
            strcmp(ext, ".edtu") == 0) {
            return true;
        }
    }

    /* Check file header */
    FILE* file = fopen(filePath, "rb");
    if (file) {
        uint32_t signature;
        if (fread(&signature, sizeof(signature), 1, file) == 1) {
            fclose(file);
            return (signature == 0x45444954);  /* 'EDIT' */
        }
        fclose(file);
    }

    return false;
}

static OSErr CreateEditionFromPath(const char* filePath, EditionContainerSpec* container)
{
    if (!container) {
        return badSectionErr;
    }

    memset(container, 0, sizeof(EditionContainerSpec));

    /* Fill file spec */
    strncpy(container->theFile.path, filePath, sizeof(container->theFile.path) - 1);
    container->theFile.path[sizeof(container->theFile.path) - 1] = '\0';

    /* Extract file name */
    const char* fileName = strrchr(filePath, '/');
    if (fileName) {
        fileName++;  /* Skip the '/' */
    } else {
        fileName = filePath;
    }

    strncpy(container->theFile.name, fileName, sizeof(container->theFile.name) - 1);
    container->theFile.name[sizeof(container->theFile.name) - 1] = '\0';

    /* Set default values */
    container->theFileScript = 0;
    container->thePart = 0;
    strcpy(container->thePartName, "Main");
    container->thePartScript = 0;

    return noErr;
}

/* Platform-specific dialog implementations (to be implemented per platform) */

OSErr ShowPlatformSubscriberDialog(NewSubscriberReply* reply)
{
    /* Placeholder implementation - would be platform-specific */
    printf("Edition Browser: Select Edition to Subscribe\n");
    printf("Available editions:\n");

    if (gBrowserState && gBrowserState->editionCount > 0) {
        for (int32_t i = 0; i < gBrowserState->editionCount; i++) {
            printf("  %d. %s\n", i + 1, gBrowserState->editions[i].theFile.name);
        }

        /* For this implementation, automatically select the first edition */
        reply->canceled = false;
        reply->formatsMask = kTEXTformatMask | kPICTformatMask;
        reply->container = gBrowserState->editions[0];
    } else {
        printf("  No editions found.\n");
        reply->canceled = true;
    }

    return noErr;
}

OSErr ShowPlatformPublisherDialog(NewPublisherReply* reply)
{
    /* Placeholder implementation - would be platform-specific */
    printf("Edition Browser: Create New Edition\n");

    /* For this implementation, create a default edition spec */
    reply->canceled = false;
    reply->replacing = false;
    reply->usePart = false;
    reply->preview = NULL;
    reply->previewFormat = 0;

    /* Set up default container */
    strcpy(reply->container.theFile.path, "./NewEdition.edition");
    strcpy(reply->container.theFile.name, "NewEdition.edition");
    reply->container.theFileScript = 0;
    reply->container.thePart = 0;
    strcpy(reply->container.thePartName, "Main");
    reply->container.thePartScript = 0;

    return noErr;
}

OSErr ShowPlatformSectionOptionsDialog(SectionOptionsReply* reply)
{
    /* Placeholder implementation - would be platform-specific */
    printf("Section Options Dialog\n");
    printf("Section ID: %d\n", (int)(*reply->sectionH)->sectionID);
    printf("Section Type: %s\n", (*reply->sectionH)->kind == stPublisher ? "Publisher" : "Subscriber");
    printf("Update Mode: %s\n", (*reply->sectionH)->mode == sumAutomatic ? "Automatic" : "Manual");

    /* For this implementation, just return unchanged */
    reply->canceled = false;
    reply->changed = false;
    reply->action = 0;

    return noErr;
}

OSErr ShowEditionBrowserInterface(EditionContainerSpec* selectedContainer)
{
    /* Placeholder implementation - would be platform-specific GUI */
    printf("Edition Browser Interface\n");
    printf("Current path: %s\n", gBrowserState->currentPath);
    printf("Found %d editions\n", (int)gBrowserState->editionCount);

    if (gBrowserState->editionCount > 0) {
        /* Select first edition for this implementation */
        *selectedContainer = gBrowserState->editions[0];
        return noErr;
    }

    return fnfErr;
}