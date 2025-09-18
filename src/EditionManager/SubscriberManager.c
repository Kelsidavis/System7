/*
 * SubscriberManager.c
 *
 * Subscriber management for Edition Manager
 * Handles creation, registration, and data consumption for subscriber sections
 *
 * Subscribers are data consumers that receive updated content from publishers
 * through edition containers (shared data files)
 */

#include "EditionManager/EditionManager.h"
#include "EditionManager/EditionManagerPrivate.h"
#include "EditionManager/SubscriberManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Internal subscriber management state */
typedef struct SubscriberCB {
    struct SubscriberCB* nextSubscriber;     /* Linked list of subscribers */
    struct SubscriberCB* prevSubscriber;     /* Linked list of subscribers */
    SectionHandle subscriberSection;         /* Associated section */
    AppRefNum subscriberApp;                 /* Owner application */
    EditionContainerSpec container;          /* Edition container spec */
    TimeStamp lastUpdateTime;                /* Last time data was updated */
    FormatType* acceptedFormats;             /* Array of accepted formats */
    int32_t formatCount;                     /* Number of accepted formats */
    uint8_t formatsMask;                     /* Bitmask of standard formats */
    bool autoUpdate;                         /* Automatic update enabled */
    bool isActive;                           /* Subscriber is active */
    Handle notificationHandle;               /* Platform notification handle */
} SubscriberCB;

static SubscriberCB* gFirstSubscriber = NULL;
static SubscriberCB* gLastSubscriber = NULL;
static int32_t gSubscriberCount = 0;

/* Internal helper functions */
static OSErr CreateSubscriberCB(SectionHandle subscriberH, SubscriberCB** subCB);
static void LinkSubscriberCB(SubscriberCB* subCB);
static void UnlinkSubscriberCB(SubscriberCB* subCB);
static SubscriberCB* FindSubscriberCB(SectionHandle subscriberH);
static OSErr ValidateSubscriberSection(SectionHandle subscriberH);
static OSErr ConnectToEdition(SectionHandle subscriberH, const EditionContainerSpec* container);

/*
 * CreateNewSubscriber
 *
 * Create a new subscriber section that will receive data from an edition container.
 */
OSErr CreateNewSubscriber(const EditionContainerSpec* container,
                         const FSSpec* subscriberDocument,
                         int32_t sectionID,
                         UpdateMode initialMode,
                         SectionHandle* subscriberH)
{
    if (!container || !subscriberDocument || !subscriberH) {
        return badSectionErr;
    }

    /* Create the section as a subscriber */
    OSErr err = NewSection(container, subscriberDocument, stSubscriber,
                          sectionID, initialMode, subscriberH);
    if (err != noErr) {
        return err;
    }

    /* Create subscriber control block */
    SubscriberCB* subCB;
    err = CreateSubscriberCB(*subscriberH, &subCB);
    if (err != noErr) {
        UnRegisterSection(*subscriberH);
        return err;
    }

    /* Initialize subscriber-specific data */
    subCB->subscriberApp = gEditionGlobals->currentApp;
    subCB->container = *container;
    subCB->lastUpdateTime = 0;
    subCB->autoUpdate = (initialMode == sumAutomatic);
    subCB->isActive = true;
    subCB->formatsMask = 0;  /* Will be set during subscription */

    /* Store subscriber CB reference in section's control block */
    if ((**subscriberH)->controlBlock) {
        free((**subscriberH)->controlBlock);
    }
    (**subscriberH)->controlBlock = (Handle)subCB;

    return noErr;
}

/*
 * RegisterSubscriber
 *
 * Register a subscriber with the system and connect to the edition.
 */
OSErr RegisterSubscriber(SectionHandle subscriberH,
                        const FSSpec* subscriberDocument,
                        uint8_t formatsMask)
{
    OSErr err = ValidateSubscriberSection(subscriberH);
    if (err != noErr) {
        return err;
    }

    if (!subscriberDocument) {
        return badSectionErr;
    }

    /* Register the section with the system */
    bool aliasUpdated;
    err = RegisterSection(subscriberDocument, subscriberH, &aliasUpdated);
    if (err != noErr) {
        return err;
    }

    /* Get subscriber control block */
    SubscriberCB* subCB = FindSubscriberCB(subscriberH);
    if (!subCB) {
        return badSectionErr;
    }

    /* Set format mask */
    subCB->formatsMask = formatsMask;

    /* Connect to the edition container */
    err = ConnectToEdition(subscriberH, &subCB->container);
    if (err != noErr) {
        return err;
    }

    /* Register for notifications from the edition */
    err = RegisterForEditionNotifications(subscriberH, &subCB->container);
    if (err != noErr) {
        return err;
    }

    return noErr;
}

/*
 * OpenEdition
 *
 * Open an edition for reading by a subscriber.
 */
OSErr OpenEdition(SectionHandle subscriberSectionH, EditionRefNum* refNum)
{
    OSErr err = ValidateSubscriberSection(subscriberSectionH);
    if (err != noErr) {
        return err;
    }

    if (!refNum) {
        return badSectionErr;
    }

    /* Get subscriber control block */
    SubscriberCB* subCB = FindSubscriberCB(subscriberSectionH);
    if (!subCB) {
        return badSectionErr;
    }

    /* Check if we already have an open edition */
    if ((*subscriberSectionH)->refNum) {
        return containerAlreadyOpenWrn;
    }

    /* Open the edition file for reading */
    EditionFileBlock* fileBlock;
    err = OpenEditionFileInternal(&subCB->container.theFile, false, &fileBlock);
    if (err != noErr) {
        return err;
    }

    /* Create edition reference */
    *refNum = (EditionRefNum)fileBlock;
    (*subscriberSectionH)->refNum = *refNum;
    fileBlock->ownerSection = subscriberSectionH;

    return noErr;
}

/*
 * ReadEdition
 *
 * Read data from an open edition in the specified format.
 */
OSErr ReadEdition(EditionRefNum whichEdition,
                 FormatType whichFormat,
                 void* buffPtr,
                 Size* buffLen)
{
    if (!whichEdition || !buffPtr || !buffLen) {
        return badSectionErr;
    }

    EditionFileBlock* fileBlock = (EditionFileBlock*)whichEdition;

    /* Verify this is a valid, readable edition */
    if (!fileBlock->isOpen) {
        return badEditionFileErr;
    }

    /* Read data from the edition file */
    OSErr err = ReadDataFromEditionFile(fileBlock, whichFormat, buffPtr, buffLen);
    if (err != noErr) {
        return err;
    }

    /* Update last read time for the subscriber */
    if (fileBlock->ownerSection) {
        SubscriberCB* subCB = FindSubscriberCB(fileBlock->ownerSection);
        if (subCB) {
            subCB->lastUpdateTime = GetCurrentTimeStamp();
        }
    }

    return noErr;
}

/*
 * EditionHasFormat
 *
 * Check if an edition contains data in the specified format.
 */
OSErr EditionHasFormat(EditionRefNum whichEdition,
                      FormatType whichFormat,
                      Size* formatSize)
{
    if (!whichEdition) {
        return badSectionErr;
    }

    EditionFileBlock* fileBlock = (EditionFileBlock*)whichEdition;

    if (!fileBlock->isOpen) {
        return badEditionFileErr;
    }

    /* Check if the format exists in the edition file */
    return CheckFormatInEditionFile(fileBlock, whichFormat, formatSize);
}

/*
 * NewSubscriberDialog
 *
 * Display dialog for creating a new subscriber.
 */
OSErr NewSubscriberDialog(NewSubscriberReply* reply)
{
    if (!reply) {
        return badSectionErr;
    }

    /* Initialize reply structure */
    memset(reply, 0, sizeof(NewSubscriberReply));
    reply->canceled = true;  /* Default to canceled */

    /* Display platform-specific subscriber creation dialog */
    return ShowNewSubscriberDialog(reply);
}

/*
 * SectionOptionsDialog
 *
 * Display section options dialog for subscriber configuration.
 */
OSErr SectionOptionsDialog(SectionOptionsReply* reply)
{
    if (!reply || !reply->sectionH) {
        return badSectionErr;
    }

    /* Initialize reply structure */
    reply->canceled = true;
    reply->changed = false;
    reply->action = 0;

    /* Validate that this is a subscriber section */
    OSErr err = ValidateSubscriberSection(reply->sectionH);
    if (err != noErr) {
        return err;
    }

    /* Display platform-specific section options dialog */
    return ShowSectionOptionsDialog(reply);
}

/*
 * SubscribeToEdition
 *
 * Subscribe to an existing edition container.
 */
OSErr SubscribeToEdition(const EditionContainerSpec* container,
                        const FSSpec* subscriberDocument,
                        int32_t sectionID,
                        uint8_t formatsMask,
                        SectionHandle* subscriberH)
{
    if (!container || !subscriberDocument || !subscriberH) {
        return badSectionErr;
    }

    /* Create new subscriber */
    OSErr err = CreateNewSubscriber(container, subscriberDocument, sectionID,
                                   sumAutomatic, subscriberH);
    if (err != noErr) {
        return err;
    }

    /* Register with the specified formats */
    err = RegisterSubscriber(*subscriberH, subscriberDocument, formatsMask);
    if (err != noErr) {
        UnRegisterSection(*subscriberH);
        return err;
    }

    return noErr;
}

/*
 * UnsubscribeFromEdition
 *
 * Unsubscribe from an edition and clean up resources.
 */
OSErr UnsubscribeFromEdition(SectionHandle subscriberH)
{
    OSErr err = ValidateSubscriberSection(subscriberH);
    if (err != noErr) {
        return err;
    }

    /* Get subscriber control block */
    SubscriberCB* subCB = FindSubscriberCB(subscriberH);
    if (subCB) {
        /* Unregister from notifications */
        UnregisterFromEditionNotifications(subscriberH);

        /* Unlink from subscriber list */
        UnlinkSubscriberCB(subCB);

        /* Free subscriber resources */
        if (subCB->acceptedFormats) {
            free(subCB->acceptedFormats);
        }
        if (subCB->notificationHandle) {
            free(subCB->notificationHandle);
        }
        free(subCB);
    }

    /* Unregister the section */
    return UnRegisterSection(subscriberH);
}

/*
 * SetSubscriberUpdateMode
 *
 * Set the update mode for a subscriber (automatic or manual).
 */
OSErr SetSubscriberUpdateMode(SectionHandle subscriberH, UpdateMode mode)
{
    OSErr err = ValidateSubscriberSection(subscriberH);
    if (err != noErr) {
        return err;
    }

    if (mode != sumAutomatic && mode != sumManual) {
        return badSectionErr;
    }

    /* Update section mode */
    (*subscriberH)->mode = mode;

    /* Update subscriber control block */
    SubscriberCB* subCB = FindSubscriberCB(subscriberH);
    if (subCB) {
        subCB->autoUpdate = (mode == sumAutomatic);

        /* Update notification registration */
        if (subCB->autoUpdate) {
            RegisterForEditionNotifications(subscriberH, &subCB->container);
        } else {
            UnregisterFromEditionNotifications(subscriberH);
        }
    }

    return noErr;
}

/*
 * GetSubscriberUpdateMode
 *
 * Get the current update mode for a subscriber.
 */
OSErr GetSubscriberUpdateMode(SectionHandle subscriberH, UpdateMode* mode)
{
    OSErr err = ValidateSubscriberSection(subscriberH);
    if (err != noErr) {
        return err;
    }

    if (!mode) {
        return badSectionErr;
    }

    *mode = (*subscriberH)->mode;
    return noErr;
}

/*
 * CheckForUpdates
 *
 * Manually check for updates to subscribed editions.
 */
OSErr CheckForUpdates(SectionHandle subscriberH, bool* updatesAvailable)
{
    OSErr err = ValidateSubscriberSection(subscriberH);
    if (err != noErr) {
        return err;
    }

    if (!updatesAvailable) {
        return badSectionErr;
    }

    *updatesAvailable = false;

    /* Get subscriber control block */
    SubscriberCB* subCB = FindSubscriberCB(subscriberH);
    if (!subCB) {
        return badSectionErr;
    }

    /* Check if the edition has been modified since our last update */
    TimeStamp editionModTime;
    err = GetEditionModificationTime(&subCB->container, &editionModTime);
    if (err != noErr) {
        return err;
    }

    *updatesAvailable = (editionModTime > subCB->lastUpdateTime);
    return noErr;
}

/* Internal helper functions */

static OSErr CreateSubscriberCB(SectionHandle subscriberH, SubscriberCB** subCB)
{
    *subCB = (SubscriberCB*)malloc(sizeof(SubscriberCB));
    if (!*subCB) {
        return editionMgrInitErr;
    }

    /* Initialize subscriber control block */
    memset(*subCB, 0, sizeof(SubscriberCB));
    (*subCB)->nextSubscriber = NULL;
    (*subCB)->prevSubscriber = NULL;
    (*subCB)->subscriberSection = subscriberH;
    (*subCB)->subscriberApp = NULL;
    (*subCB)->lastUpdateTime = 0;
    (*subCB)->acceptedFormats = NULL;
    (*subCB)->formatCount = 0;
    (*subCB)->formatsMask = 0;
    (*subCB)->autoUpdate = true;
    (*subCB)->isActive = true;
    (*subCB)->notificationHandle = NULL;

    /* Link into global subscriber list */
    LinkSubscriberCB(*subCB);

    return noErr;
}

static void LinkSubscriberCB(SubscriberCB* subCB)
{
    subCB->nextSubscriber = NULL;
    subCB->prevSubscriber = gLastSubscriber;

    if (gLastSubscriber) {
        gLastSubscriber->nextSubscriber = subCB;
    } else {
        gFirstSubscriber = subCB;
    }

    gLastSubscriber = subCB;
    gSubscriberCount++;
}

static void UnlinkSubscriberCB(SubscriberCB* subCB)
{
    if (subCB->prevSubscriber) {
        subCB->prevSubscriber->nextSubscriber = subCB->nextSubscriber;
    } else {
        gFirstSubscriber = subCB->nextSubscriber;
    }

    if (subCB->nextSubscriber) {
        subCB->nextSubscriber->prevSubscriber = subCB->prevSubscriber;
    } else {
        gLastSubscriber = subCB->prevSubscriber;
    }

    gSubscriberCount--;
}

static SubscriberCB* FindSubscriberCB(SectionHandle subscriberH)
{
    SubscriberCB* current = gFirstSubscriber;
    while (current) {
        if (current->subscriberSection == subscriberH) {
            return current;
        }
        current = current->nextSubscriber;
    }
    return NULL;
}

static OSErr ValidateSubscriberSection(SectionHandle subscriberH)
{
    OSErr err = ValidateSectionHandle(subscriberH);
    if (err != noErr) {
        return err;
    }

    if ((*subscriberH)->kind != stSubscriber) {
        return badSectionErr;
    }

    return noErr;
}

static OSErr ConnectToEdition(SectionHandle subscriberH, const EditionContainerSpec* container)
{
    /* Verify that the edition container exists and is accessible */
    OSErr err = ValidateEditionContainer(container);
    if (err != noErr) {
        return err;
    }

    /* Register subscriber with platform-specific data sharing system */
    err = RegisterSubscriberWithPlatform(subscriberH, container);
    if (err != noErr) {
        return err;
    }

    return noErr;
}