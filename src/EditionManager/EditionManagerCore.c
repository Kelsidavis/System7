/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * EditionManagerCore.c
 *
 * Core Edition Manager implementation for System 7 Publish/Subscribe functionality
 * Provides the main API functions for edition management, section creation, and data sharing
 *
 * This is a portable implementation of Apple's System 7.1 Edition Manager (Pack11)
 */

#include "EditionManager/EditionManager.h"
#include "EditionManager/EditionManagerPrivate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* Global Edition Manager state */
static EditionManagerGlobals* gEditionGlobals = NULL;
static bool gEditionPackInitialized = false;

/* Internal helper functions */
static OSErr AllocateSectionRecord(SectionHandle* sectionH);
static OSErr ValidateSectionHandle(SectionHandle sectionH);
static OSErr ValidateEditionContainer(const EditionContainerSpec* container);
static SectionHandle FindSectionByID(int32_t sectionID);
static void LinkSection(SectionHandle sectionH);
static void UnlinkSection(SectionHandle sectionH);

/*
 * InitEditionPack
 *
 * Initialize the Edition Manager package.
 * This must be called before any other Edition Manager functions.
 */
OSErr InitEditionPack(void)
{
    if (gEditionPackInitialized) {
        return noErr;  /* Already initialized */
    }

    /* Allocate global state */
    gEditionGlobals = (EditionManagerGlobals*)malloc(sizeof(EditionManagerGlobals));
    if (!gEditionGlobals) {
        return editionMgrInitErr;
    }

    /* Initialize global state */
    memset(gEditionGlobals, 0, sizeof(EditionManagerGlobals));
    gEditionGlobals->firstSection = NULL;
    gEditionGlobals->lastSection = NULL;
    gEditionGlobals->sectionCount = 0;
    gEditionGlobals->nextSectionID = 1;
    gEditionGlobals->currentApp = NULL;
    gEditionGlobals->editionOpenerProc = NULL;

    /* Initialize platform-specific data sharing subsystem */
    OSErr err = InitDataSharingPlatform();
    if (err != noErr) {
        free(gEditionGlobals);
        gEditionGlobals = NULL;
        return err;
    }

    gEditionPackInitialized = true;
    return noErr;
}

/*
 * QuitEditionPack
 *
 * Clean up the Edition Manager package.
 */
OSErr QuitEditionPack(void)
{
    if (!gEditionPackInitialized) {
        return noErr;  /* Not initialized */
    }

    /* Clean up all sections */
    SectionHandle current = gEditionGlobals->firstSection;
    while (current) {
        SectionHandle next = (*current)->nextSection;
        UnRegisterSection(current);
        free(*current);
        free(current);
        current = next;
    }

    /* Clean up platform-specific data sharing */
    CleanupDataSharingPlatform();

    /* Free global state */
    free(gEditionGlobals);
    gEditionGlobals = NULL;
    gEditionPackInitialized = false;

    return noErr;
}

/*
 * NewSection
 *
 * Create a new section (publisher or subscriber).
 */
OSErr NewSection(const EditionContainerSpec* container,
                 const FSSpec* sectionDocument,
                 SectionType kind,
                 int32_t sectionID,
                 UpdateMode initialMode,
                 SectionHandle* sectionH)
{
    if (!gEditionPackInitialized) {
        return editionMgrInitErr;
    }

    if (!container || !sectionDocument || !sectionH) {
        return badSectionErr;
    }

    /* Validate parameters */
    if (kind != stPublisher && kind != stSubscriber) {
        return badSectionErr;
    }

    OSErr err = ValidateEditionContainer(container);
    if (err != noErr) {
        return err;
    }

    /* Check if section ID is already in use */
    if (FindSectionByID(sectionID) != NULL) {
        return badSectionErr;
    }

    /* Allocate section record */
    err = AllocateSectionRecord(sectionH);
    if (err != noErr) {
        return err;
    }

    SectionPtr section = **sectionH;

    /* Initialize section record */
    section->version = 0x01;
    section->kind = kind;
    section->mode = initialMode;
    section->mdDate = (TimeStamp)time(NULL);
    section->sectionID = sectionID;
    section->refCon = 0;
    section->alias = NULL;  /* Will be created in RegisterSection */
    section->subPart = container->thePart;
    section->nextSection = NULL;
    section->controlBlock = NULL;
    section->refNum = NULL;

    /* Store container information in control block */
    section->controlBlock = malloc(sizeof(EditionContainerSpec));
    if (!section->controlBlock) {
        free(*sectionH);
        free(sectionH);
        return editionMgrInitErr;
    }
    memcpy(section->controlBlock, container, sizeof(EditionContainerSpec));

    /* Link into global section list */
    LinkSection(*sectionH);

    return noErr;
}

/*
 * RegisterSection
 *
 * Register a section with the system and create alias if needed.
 */
OSErr RegisterSection(const FSSpec* sectionDocument,
                     SectionHandle sectionH,
                     bool* aliasWasUpdated)
{
    if (!gEditionPackInitialized) {
        return editionMgrInitErr;
    }

    OSErr err = ValidateSectionHandle(sectionH);
    if (err != noErr) {
        return err;
    }

    if (!sectionDocument || !aliasWasUpdated) {
        return badSectionErr;
    }

    SectionPtr section = *sectionH;
    *aliasWasUpdated = false;

    /* Create alias for the edition container */
    if (!section->alias) {
        err = CreateAliasFromFSSpec(&((EditionContainerSpec*)section->controlBlock)->theFile,
                                   &section->alias);
        if (err != noErr) {
            return err;
        }
        *aliasWasUpdated = true;
    } else {
        /* Update existing alias if needed */
        err = UpdateAlias(&((EditionContainerSpec*)section->controlBlock)->theFile,
                         section->alias,
                         aliasWasUpdated);
        if (err != noErr) {
            return err;
        }
    }

    /* Register with platform-specific data sharing system */
    err = RegisterSectionWithPlatform(sectionH, sectionDocument);
    if (err != noErr) {
        return err;
    }

    /* Update modification date */
    section->mdDate = (TimeStamp)time(NULL);

    return noErr;
}

/*
 * UnRegisterSection
 *
 * Unregister a section from the system.
 */
OSErr UnRegisterSection(SectionHandle sectionH)
{
    if (!gEditionPackInitialized) {
        return editionMgrInitErr;
    }

    OSErr err = ValidateSectionHandle(sectionH);
    if (err != noErr) {
        return err;
    }

    /* Unregister from platform-specific system */
    UnregisterSectionFromPlatform(sectionH);

    /* Close any open edition */
    if ((*sectionH)->refNum) {
        CloseEdition((*sectionH)->refNum, false);
    }

    /* Free alias */
    if ((*sectionH)->alias) {
        free((*sectionH)->alias);
        (*sectionH)->alias = NULL;
    }

    /* Free control block */
    if ((*sectionH)->controlBlock) {
        free((*sectionH)->controlBlock);
        (*sectionH)->controlBlock = NULL;
    }

    /* Unlink from global list */
    UnlinkSection(sectionH);

    return noErr;
}

/*
 * IsRegisteredSection
 *
 * Check if a section is currently registered.
 */
OSErr IsRegisteredSection(SectionHandle sectionH)
{
    if (!gEditionPackInitialized) {
        return editionMgrInitErr;
    }

    OSErr err = ValidateSectionHandle(sectionH);
    if (err != noErr) {
        return err;
    }

    /* Check if section is in our global list */
    SectionHandle current = gEditionGlobals->firstSection;
    while (current) {
        if (current == sectionH) {
            return noErr;  /* Found it */
        }
        current = (*current)->nextSection;
    }

    return badSectionErr;  /* Not found */
}

/*
 * AssociateSection
 *
 * Associate a section with a new document.
 */
OSErr AssociateSection(SectionHandle sectionH,
                      const FSSpec* newSectionDocument)
{
    if (!gEditionPackInitialized) {
        return editionMgrInitErr;
    }

    OSErr err = ValidateSectionHandle(sectionH);
    if (err != noErr) {
        return err;
    }

    if (!newSectionDocument) {
        return badSectionErr;
    }

    /* Update platform association */
    err = AssociateSectionWithPlatform(sectionH, newSectionDocument);
    if (err != noErr) {
        return err;
    }

    /* Update modification date */
    (*sectionH)->mdDate = (TimeStamp)time(NULL);

    return noErr;
}

/*
 * GetEditionInfo
 *
 * Get information about an edition.
 */
OSErr GetEditionInfo(const SectionHandle sectionH,
                    EditionInfoRecord* editionInfo)
{
    if (!gEditionPackInitialized) {
        return editionMgrInitErr;
    }

    OSErr err = ValidateSectionHandle(sectionH);
    if (err != noErr) {
        return err;
    }

    if (!editionInfo) {
        return badSectionErr;
    }

    SectionPtr section = *sectionH;
    EditionContainerSpec* container = (EditionContainerSpec*)section->controlBlock;

    /* Fill in edition info */
    editionInfo->crDate = section->mdDate;  /* Use section creation as creation date */
    editionInfo->mdDate = section->mdDate;
    editionInfo->fdCreator = 'EDIT';        /* Default edition creator */
    editionInfo->fdType = kUnknownEditionFileType;  /* Default type */
    editionInfo->container = *container;

    /* Get file info if available */
    GetEditionFileInfo(&container->theFile, editionInfo);

    return noErr;
}

/*
 * GetCurrentAppRefNum
 *
 * Get the current application reference number.
 */
OSErr GetCurrentAppRefNum(AppRefNum* thisApp)
{
    if (!gEditionPackInitialized) {
        return editionMgrInitErr;
    }

    if (!thisApp) {
        return badSectionErr;
    }

    *thisApp = gEditionGlobals->currentApp;
    return noErr;
}

/*
 * PostSectionEvent
 *
 * Post a section event to an application.
 */
OSErr PostSectionEvent(SectionHandle sectionH, AppRefNum toApp, ResType classID)
{
    if (!gEditionPackInitialized) {
        return editionMgrInitErr;
    }

    OSErr err = ValidateSectionHandle(sectionH);
    if (err != noErr) {
        return err;
    }

    /* Post event to platform-specific notification system */
    return PostSectionEventToPlatform(sectionH, toApp, classID);
}

/*
 * EditionBackGroundTask
 *
 * Perform background processing for edition management.
 */
OSErr EditionBackGroundTask(void)
{
    if (!gEditionPackInitialized) {
        return editionMgrInitErr;
    }

    /* Process platform-specific background tasks */
    return ProcessDataSharingBackground();
}

/* Internal helper functions */

static OSErr AllocateSectionRecord(SectionHandle* sectionH)
{
    *sectionH = (SectionHandle)malloc(sizeof(SectionPtr));
    if (!*sectionH) {
        return editionMgrInitErr;
    }

    **sectionH = (SectionPtr)malloc(sizeof(SectionRecord));
    if (!**sectionH) {
        free(*sectionH);
        return editionMgrInitErr;
    }

    memset(**sectionH, 0, sizeof(SectionRecord));
    return noErr;
}

static OSErr ValidateSectionHandle(SectionHandle sectionH)
{
    if (!sectionH || !*sectionH) {
        return badSectionErr;
    }

    SectionPtr section = *sectionH;
    if (section->version != 0x01) {
        return badSectionErr;
    }

    if (section->kind != stPublisher && section->kind != stSubscriber) {
        return badSectionErr;
    }

    return noErr;
}

static OSErr ValidateEditionContainer(const EditionContainerSpec* container)
{
    if (!container) {
        return notAnEditionContainerErr;
    }

    /* Basic validation - check if file path is reasonable */
    if (strlen(container->theFile.name) == 0 ||
        strlen(container->theFile.path) == 0) {
        return badEditionFileErr;
    }

    return noErr;
}

static SectionHandle FindSectionByID(int32_t sectionID)
{
    SectionHandle current = gEditionGlobals->firstSection;
    while (current) {
        if ((*current)->sectionID == sectionID) {
            return current;
        }
        current = (*current)->nextSection;
    }
    return NULL;
}

static void LinkSection(SectionHandle sectionH)
{
    (*sectionH)->nextSection = NULL;

    if (!gEditionGlobals->firstSection) {
        gEditionGlobals->firstSection = sectionH;
        gEditionGlobals->lastSection = sectionH;
    } else {
        (*gEditionGlobals->lastSection)->nextSection = sectionH;
        gEditionGlobals->lastSection = sectionH;
    }

    gEditionGlobals->sectionCount++;
}

static void UnlinkSection(SectionHandle sectionH)
{
    if (gEditionGlobals->firstSection == sectionH) {
        gEditionGlobals->firstSection = (*sectionH)->nextSection;
        if (gEditionGlobals->lastSection == sectionH) {
            gEditionGlobals->lastSection = NULL;
        }
    } else {
        SectionHandle current = gEditionGlobals->firstSection;
        while (current && (*current)->nextSection != sectionH) {
            current = (*current)->nextSection;
        }
        if (current) {
            (*current)->nextSection = (*sectionH)->nextSection;
            if (gEditionGlobals->lastSection == sectionH) {
                gEditionGlobals->lastSection = current;
            }
        }
    }

    gEditionGlobals->sectionCount--;
}