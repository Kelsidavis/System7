/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * PublisherManager.c
 *
 * Publisher management for Edition Manager
 * Handles creation, registration, and data publishing for publisher sections
 *
 * Publishers are data sources that make their content available to subscribers
 * through edition containers (files that store shared data)
 */

#include "EditionManager/EditionManager.h"
#include "EditionManager/EditionManagerPrivate.h"
#include "EditionManager/PublisherManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Internal publisher management state */
static PubCBRecord** gFirstPublisher = NULL;
static PubCBRecord** gLastPublisher = NULL;
static int32_t gPublisherCount = 0;

/* Internal helper functions */
static OSErr CreatePublisherCB(SectionHandle publisherH, PubCBRecord*** pubCB);
static OSErr LinkPublisherCB(PubCBRecord** pubCB);
static void UnlinkPublisherCB(PubCBRecord** pubCB);
static PubCBRecord** FindPublisherCB(SectionHandle publisherH);
static OSErr ValidatePublisherSection(SectionHandle publisherH);
static OSErr CreateEditionForPublisher(SectionHandle publisherH, const FSSpec* editionFile);

/*
 * CreateNewPublisher
 *
 * Create a new publisher section that will share data through an edition container.
 */
OSErr CreateNewPublisher(const EditionContainerSpec* container,
                        const FSSpec* publisherDocument,
                        int32_t sectionID,
                        UpdateMode initialMode,
                        SectionHandle* publisherH)
{
    if (!container || !publisherDocument || !publisherH) {
        return badSectionErr;
    }

    /* Create the section as a publisher */
    OSErr err = NewSection(container, publisherDocument, stPublisher,
                          sectionID, initialMode, publisherH);
    if (err != noErr) {
        return err;
    }

    /* Create publisher control block */
    PubCBRecord** pubCB;
    err = CreatePublisherCB(*publisherH, &pubCB);
    if (err != noErr) {
        UnRegisterSection(*publisherH);
        return err;
    }

    /* Initialize publisher-specific data */
    PubCBRecord* pub = *pubCB;
    pub->publisherKind = stPublisher;
    pub->publisherApp = gEditionGlobals->currentApp;
    pub->publisher = *publisherH;
    pub->publisherCount = 1;
    pub->fileMissing = false;
    pub->fileRefNum = kClosedFile;

    /* Store publisher CB reference in section's control block */
    if ((**publisherH)->controlBlock) {
        free((**publisherH)->controlBlock);
    }
    (**publisherH)->controlBlock = (Handle)pubCB;

    return noErr;
}

/*
 * RegisterPublisher
 *
 * Register a publisher with the system and create the edition container file.
 */
OSErr RegisterPublisher(SectionHandle publisherH,
                       const FSSpec* publisherDocument,
                       const FSSpec* editionFile,
                       OSType editionCreator)
{
    OSErr err = ValidatePublisherSection(publisherH);
    if (err != noErr) {
        return err;
    }

    if (!publisherDocument || !editionFile) {
        return badSectionErr;
    }

    /* Register the section with the system */
    bool aliasUpdated;
    err = RegisterSection(publisherDocument, publisherH, &aliasUpdated);
    if (err != noErr) {
        return err;
    }

    /* Create the edition container file */
    err = CreateEditionForPublisher(publisherH, editionFile);
    if (err != noErr) {
        return err;
    }

    /* Update publisher CB with file information */
    PubCBRecord** pubCB = FindPublisherCB(publisherH);
    if (pubCB) {
        PubCBRecord* pub = *pubCB;
        pub->lastVolMod = GetCurrentTimeStamp();
        pub->lastDirMod = GetCurrentTimeStamp();

        /* Copy edition file info to the CB */
        CopyFSSpec(editionFile, &pub->info.container.theFile);
        pub->info.fdCreator = editionCreator;
        pub->info.fdType = kUnknownEditionFileType;  /* Will be determined by format */
        pub->info.crDate = GetCurrentTimeStamp();
        pub->info.mdDate = GetCurrentTimeStamp();
    }

    return noErr;
}

/*
 * OpenNewEdition
 *
 * Open an edition for writing by a publisher.
 */
OSErr OpenNewEdition(SectionHandle publisherSectionH,
                    OSType fdCreator,
                    const FSSpec* publisherSectionDocument,
                    EditionRefNum* refNum)
{
    OSErr err = ValidatePublisherSection(publisherSectionH);
    if (err != noErr) {
        return err;
    }

    if (!publisherSectionDocument || !refNum) {
        return badSectionErr;
    }

    /* Find publisher control block */
    PubCBRecord** pubCB = FindPublisherCB(publisherSectionH);
    if (!pubCB) {
        return badSectionErr;
    }

    PubCBRecord* pub = *pubCB;

    /* Check if we already have an open edition */
    if ((*publisherSectionH)->refNum) {
        return containerAlreadyOpenWrn;
    }

    /* Open the edition file for writing */
    EditionFileBlock* fileBlock;
    err = OpenEditionFileInternal(&pub->info.container.theFile, true, &fileBlock);
    if (err != noErr) {
        return err;
    }

    /* Create edition reference */
    *refNum = (EditionRefNum)fileBlock;
    (*publisherSectionH)->refNum = *refNum;
    fileBlock->ownerSection = publisherSectionH;

    /* Update publisher state */
    pub->fileRefNum = (int16_t)(intptr_t)fileBlock->platformHandle;
    pub->openMode = 1;  /* Read/write mode */
    pub->fileMark = 0;

    return noErr;
}

/*
 * WriteEdition
 *
 * Write data to an open edition in the specified format.
 */
OSErr WriteEdition(EditionRefNum whichEdition,
                  FormatType whichFormat,
                  const void* buffPtr,
                  Size buffLen)
{
    if (!whichEdition || !buffPtr || buffLen <= 0) {
        return badSectionErr;
    }

    EditionFileBlock* fileBlock = (EditionFileBlock*)whichEdition;

    /* Verify this is a valid, writable edition */
    if (!fileBlock->isOpen || !fileBlock->isWritable) {
        return badEditionFileErr;
    }

    /* Write data to the edition file */
    OSErr err = WriteDataToEditionFile(fileBlock, whichFormat, buffPtr, buffLen);
    if (err != noErr) {
        return err;
    }

    /* Update file mark */
    fileBlock->currentMark += buffLen;

    /* Notify any subscribers that data has been updated */
    if (fileBlock->ownerSection) {
        NotifySubscribers(fileBlock->ownerSection);
    }

    return noErr;
}

/*
 * SetEditionFormatMark
 *
 * Set the current position for a specific format in the edition.
 */
OSErr SetEditionFormatMark(EditionRefNum whichEdition,
                          FormatType whichFormat,
                          uint32_t setMarkTo)
{
    if (!whichEdition) {
        return badSectionErr;
    }

    EditionFileBlock* fileBlock = (EditionFileBlock*)whichEdition;

    if (!fileBlock->isOpen) {
        return badEditionFileErr;
    }

    /* Set the mark for the specified format */
    OSErr err = SetFormatMarkInEditionFile(fileBlock, whichFormat, setMarkTo);
    if (err != noErr) {
        return err;
    }

    fileBlock->currentMark = setMarkTo;
    return noErr;
}

/*
 * GetEditionFormatMark
 *
 * Get the current position for a specific format in the edition.
 */
OSErr GetEditionFormatMark(EditionRefNum whichEdition,
                          FormatType whichFormat,
                          uint32_t* currentMark)
{
    if (!whichEdition || !currentMark) {
        return badSectionErr;
    }

    EditionFileBlock* fileBlock = (EditionFileBlock*)whichEdition;

    if (!fileBlock->isOpen) {
        return badEditionFileErr;
    }

    /* Get the mark for the specified format */
    OSErr err = GetFormatMarkFromEditionFile(fileBlock, whichFormat, currentMark);
    return err;
}

/*
 * CloseEdition
 *
 * Close an open edition, optionally committing changes.
 */
OSErr CloseEdition(EditionRefNum whichEdition, bool successful)
{
    if (!whichEdition) {
        return badSectionErr;
    }

    EditionFileBlock* fileBlock = (EditionFileBlock*)whichEdition;

    if (!fileBlock->isOpen) {
        return noErr;  /* Already closed */
    }

    /* If this was a successful close and we were writing, commit the changes */
    if (successful && fileBlock->isWritable) {
        /* Update edition file metadata */
        UpdateEditionFileMetadata(fileBlock);

        /* Sync to storage */
        SyncEditionFile(fileBlock);
    }

    /* Close the edition file */
    OSErr err = CloseEditionFileInternal(fileBlock);

    /* Clear the refNum from the owner section */
    if (fileBlock->ownerSection) {
        (*fileBlock->ownerSection)->refNum = NULL;
    }

    return err;
}

/*
 * NewPublisherDialog
 *
 * Display dialog for creating a new publisher.
 */
OSErr NewPublisherDialog(NewPublisherReply* reply)
{
    if (!reply) {
        return badSectionErr;
    }

    /* Initialize reply structure */
    memset(reply, 0, sizeof(NewPublisherReply));
    reply->canceled = true;  /* Default to canceled */

    /* Display platform-specific publisher creation dialog */
    return ShowNewPublisherDialog(reply);
}

/*
 * SynchronizePublisherData
 *
 * Synchronize publisher data with its edition container.
 */
OSErr SynchronizePublisherData(SectionHandle publisherH)
{
    OSErr err = ValidatePublisherSection(publisherH);
    if (err != noErr) {
        return err;
    }

    /* Find publisher control block */
    PubCBRecord** pubCB = FindPublisherCB(publisherH);
    if (!pubCB) {
        return badSectionErr;
    }

    /* Check if the publisher's data needs to be updated */
    SectionPtr section = *publisherH;
    TimeStamp currentTime = GetCurrentTimeStamp();

    /* Only sync if in automatic mode or if manually requested */
    if (section->mode == pumManual) {
        return noErr;  /* Manual mode - don't auto-sync */
    }

    /* Trigger synchronization with platform data sharing system */
    return TriggerPublisherSync(publisherH);
}

/* Internal helper functions */

static OSErr CreatePublisherCB(SectionHandle publisherH, PubCBRecord*** pubCB)
{
    *pubCB = (PubCBRecord**)malloc(sizeof(PubCBRecord*));
    if (!*pubCB) {
        return editionMgrInitErr;
    }

    **pubCB = (PubCBRecord*)malloc(sizeof(PubCBRecord));
    if (!**pubCB) {
        free(*pubCB);
        return editionMgrInitErr;
    }

    /* Initialize publisher control block */
    memset(**pubCB, 0, sizeof(PubCBRecord));

    PubCBRecord* pub = **pubCB;
    pub->nextPubCB = NULL;
    pub->prevPubCB = NULL;
    pub->usageInfo = NULL;
    pub->volumeInfo = 0;
    pub->pubCNodeID = 0;
    pub->lastVolMod = 0;
    pub->lastDirMod = 0;
    pub->oldPubCB = NULL;
    pub->publisherApp = NULL;
    pub->publisher = publisherH;
    pub->publisherAlias = NULL;
    pub->publisherCount = 0;
    pub->publisherKind = stPublisher;
    pub->fileMissing = false;
    pub->fileRefNum = kClosedFile;
    pub->openMode = 0;
    pub->fileMark = 0;
    pub->rangeLockStart = 0;
    pub->rangeLockLen = 0;
    pub->allocMap = NULL;
    pub->formats = NULL;

    /* Link into global publisher list */
    LinkPublisherCB(*pubCB);

    return noErr;
}

static OSErr LinkPublisherCB(PubCBRecord** pubCB)
{
    (*pubCB)->nextPubCB = NULL;
    (*pubCB)->prevPubCB = gLastPublisher;

    if (gLastPublisher) {
        (*gLastPublisher)->nextPubCB = pubCB;
    } else {
        gFirstPublisher = pubCB;
    }

    gLastPublisher = pubCB;
    gPublisherCount++;

    return noErr;
}

static void UnlinkPublisherCB(PubCBRecord** pubCB)
{
    if ((*pubCB)->prevPubCB) {
        (*(*pubCB)->prevPubCB)->nextPubCB = (*pubCB)->nextPubCB;
    } else {
        gFirstPublisher = (*pubCB)->nextPubCB;
    }

    if ((*pubCB)->nextPubCB) {
        (*(*pubCB)->nextPubCB)->prevPubCB = (*pubCB)->prevPubCB;
    } else {
        gLastPublisher = (*pubCB)->prevPubCB;
    }

    gPublisherCount--;
}

static PubCBRecord** FindPublisherCB(SectionHandle publisherH)
{
    PubCBRecord** current = gFirstPublisher;
    while (current) {
        if ((*current)->publisher == publisherH) {
            return current;
        }
        current = (*current)->nextPubCB;
    }
    return NULL;
}

static OSErr ValidatePublisherSection(SectionHandle publisherH)
{
    OSErr err = ValidateSectionHandle(publisherH);
    if (err != noErr) {
        return err;
    }

    if ((*publisherH)->kind != stPublisher) {
        return badSectionErr;
    }

    return noErr;
}

static OSErr CreateEditionForPublisher(SectionHandle publisherH, const FSSpec* editionFile)
{
    /* Create the edition container file */
    OSErr err = CreateEditionContainerFile(editionFile, 'EDIT', 0);
    if (err != noErr && err != dupFNErr) {  /* OK if file already exists */
        return err;
    }

    /* Initialize the edition file with publisher information */
    err = InitializeEditionFile(editionFile, publisherH);
    return err;
}