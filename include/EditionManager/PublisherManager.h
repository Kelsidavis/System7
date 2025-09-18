/*
 * PublisherManager.h
 *
 * Publisher Manager API for Edition Manager
 * Handles creation, registration, and data publishing functionality
 */

#ifndef __PUBLISHER_MANAGER_H__
#define __PUBLISHER_MANAGER_H__

#include "EditionManager/EditionManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Publisher Creation and Management
 */

/* Create a new publisher section */
OSErr CreateNewPublisher(const EditionContainerSpec* container,
                        const FSSpec* publisherDocument,
                        int32_t sectionID,
                        UpdateMode initialMode,
                        SectionHandle* publisherH);

/* Register a publisher with the system */
OSErr RegisterPublisher(SectionHandle publisherH,
                       const FSSpec* publisherDocument,
                       const FSSpec* editionFile,
                       OSType editionCreator);

/* Unregister a publisher */
OSErr UnregisterPublisher(SectionHandle publisherH);

/*
 * Data Publishing
 */

/* Publish data in a specific format */
OSErr PublishData(SectionHandle publisherH,
                 FormatType format,
                 const void* data,
                 Size dataSize);

/* Publish multiple formats at once */
OSErr PublishMultipleFormats(SectionHandle publisherH,
                           FormatType* formats,
                           void** dataBuffers,
                           Size* dataSizes,
                           int32_t formatCount);

/* Check if publisher has subscribers */
OSErr HasSubscribers(SectionHandle publisherH, bool* hasSubscribers);

/* Get list of subscriber applications */
OSErr GetSubscriberList(SectionHandle publisherH,
                       AppRefNum** subscriberApps,
                       int32_t* subscriberCount);

/*
 * Publisher State Management
 */

/* Set publisher update mode */
OSErr SetPublisherUpdateMode(SectionHandle publisherH, UpdateMode mode);

/* Get publisher update mode */
OSErr GetPublisherUpdateMode(SectionHandle publisherH, UpdateMode* mode);

/* Check if publisher is active */
OSErr IsPublisherActive(SectionHandle publisherH, bool* isActive);

/* Suspend/resume publisher */
OSErr SuspendPublisher(SectionHandle publisherH);
OSErr ResumePublisher(SectionHandle publisherH);

/*
 * Edition Container Management
 */

/* Get publisher's edition container spec */
OSErr GetPublisherContainer(SectionHandle publisherH,
                           EditionContainerSpec* container);

/* Update publisher's edition container */
OSErr UpdatePublisherContainer(SectionHandle publisherH,
                              const EditionContainerSpec* newContainer);

/* Get publisher edition file info */
OSErr GetPublisherEditionInfo(SectionHandle publisherH,
                             EditionInfoRecord* editionInfo);

/*
 * Data Synchronization
 */

/* Force immediate data synchronization */
OSErr SyncPublisherData(SectionHandle publisherH);

/* Set publisher data modification time */
OSErr SetPublisherModificationTime(SectionHandle publisherH, TimeStamp modTime);

/* Get publisher data modification time */
OSErr GetPublisherModificationTime(SectionHandle publisherH, TimeStamp* modTime);

/*
 * Format Management
 */

/* Add supported format to publisher */
OSErr AddPublisherFormat(SectionHandle publisherH, FormatType format);

/* Remove supported format from publisher */
OSErr RemovePublisherFormat(SectionHandle publisherH, FormatType format);

/* Get list of supported formats */
OSErr GetPublisherFormats(SectionHandle publisherH,
                         FormatType** formats,
                         int32_t* formatCount);

/* Check if format is supported */
OSErr PublisherSupportsFormat(SectionHandle publisherH,
                             FormatType format,
                             bool* isSupported);

/*
 * Notification and Events
 */

/* Send notification to subscribers */
OSErr NotifySubscribersOfChange(SectionHandle publisherH);

/* Send custom event to subscribers */
OSErr SendEventToSubscribers(SectionHandle publisherH,
                            ResType eventClass,
                            ResType eventID,
                            const void* eventData,
                            Size dataSize);

/*
 * Publisher Dialog and UI
 */

/* Show publisher options dialog */
OSErr ShowPublisherOptionsDialog(SectionHandle publisherH,
                                SectionOptionsReply* reply);

/* Show publish dialog for new publishers */
OSErr ShowPublishDialog(const FSSpec* defaultFile,
                       NewPublisherReply* reply);

/*
 * Error Handling and Diagnostics
 */

/* Validate publisher section */
OSErr ValidatePublisher(SectionHandle publisherH);

/* Get publisher status information */
OSErr GetPublisherStatus(SectionHandle publisherH,
                        int32_t* status,
                        char* statusMessage,
                        Size messageSize);

/* Platform abstraction functions (implemented in platform-specific files) */

/* Platform-specific publisher dialog */
OSErr ShowNewPublisherDialog(NewPublisherReply* reply);

/* Platform-specific data sharing registration */
OSErr TriggerPublisherSync(SectionHandle publisherH);

/* Edition file operations */
OSErr WriteDataToEditionFile(void* fileBlock, FormatType format,
                           const void* data, Size dataSize);
OSErr SetFormatMarkInEditionFile(void* fileBlock, FormatType format, uint32_t mark);
OSErr GetFormatMarkFromEditionFile(void* fileBlock, FormatType format, uint32_t* mark);
OSErr UpdateEditionFileMetadata(void* fileBlock);
OSErr SyncEditionFile(void* fileBlock);
OSErr InitializeEditionFile(const FSSpec* fileSpec, SectionHandle publisherH);

/* Constants */
#define kMaxPublishersPerApp 64
#define kDefaultPublisherUpdateMode pumOnSave
#define kPublisherActiveCheckInterval 1000  /* milliseconds */

/* Publisher status codes */
enum {
    kPublisherStatusInactive = 0,
    kPublisherStatusActive = 1,
    kPublisherStatusSuspended = 2,
    kPublisherStatusError = -1
};

#ifdef __cplusplus
}
#endif

#endif /* __PUBLISHER_MANAGER_H__ */