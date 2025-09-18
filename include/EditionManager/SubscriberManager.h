/*
 * SubscriberManager.h
 *
 * Subscriber Manager API for Edition Manager
 * Handles creation, registration, and data consumption functionality
 */

#ifndef __SUBSCRIBER_MANAGER_H__
#define __SUBSCRIBER_MANAGER_H__

#include "EditionManager/EditionManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Subscriber Creation and Management
 */

/* Create a new subscriber section */
OSErr CreateNewSubscriber(const EditionContainerSpec* container,
                         const FSSpec* subscriberDocument,
                         int32_t sectionID,
                         UpdateMode initialMode,
                         SectionHandle* subscriberH);

/* Register a subscriber with the system */
OSErr RegisterSubscriber(SectionHandle subscriberH,
                        const FSSpec* subscriberDocument,
                        uint8_t formatsMask);

/* Unregister a subscriber */
OSErr UnregisterSubscriber(SectionHandle subscriberH);

/* Subscribe to an existing edition */
OSErr SubscribeToEdition(const EditionContainerSpec* container,
                        const FSSpec* subscriberDocument,
                        int32_t sectionID,
                        uint8_t formatsMask,
                        SectionHandle* subscriberH);

/* Unsubscribe from an edition */
OSErr UnsubscribeFromEdition(SectionHandle subscriberH);

/*
 * Data Consumption
 */

/* Read data in a specific format */
OSErr ReadSubscriberData(SectionHandle subscriberH,
                        FormatType format,
                        void* buffer,
                        Size* bufferSize);

/* Read data in the best available format */
OSErr ReadBestFormat(SectionHandle subscriberH,
                    FormatType* actualFormat,
                    void** data,
                    Size* dataSize);

/* Read multiple formats at once */
OSErr ReadMultipleFormats(SectionHandle subscriberH,
                         FormatType* requestedFormats,
                         int32_t formatCount,
                         void*** dataBuffers,
                         Size** dataSizes,
                         FormatType** actualFormats,
                         int32_t* returnedCount);

/* Get available data formats */
OSErr GetAvailableFormats(SectionHandle subscriberH,
                         FormatType** formats,
                         int32_t* formatCount);

/* Check if specific format is available */
OSErr IsFormatAvailable(SectionHandle subscriberH,
                       FormatType format,
                       bool* isAvailable,
                       Size* dataSize);

/*
 * Update Management
 */

/* Set subscriber update mode */
OSErr SetSubscriberUpdateMode(SectionHandle subscriberH, UpdateMode mode);

/* Get subscriber update mode */
OSErr GetSubscriberUpdateMode(SectionHandle subscriberH, UpdateMode* mode);

/* Check for updates manually */
OSErr CheckForUpdates(SectionHandle subscriberH, bool* updatesAvailable);

/* Force immediate update */
OSErr UpdateSubscriberData(SectionHandle subscriberH);

/* Get last update time */
OSErr GetLastUpdateTime(SectionHandle subscriberH, TimeStamp* updateTime);

/*
 * Subscriber State Management
 */

/* Check if subscriber is active */
OSErr IsSubscriberActive(SectionHandle subscriberH, bool* isActive);

/* Suspend/resume subscriber */
OSErr SuspendSubscriber(SectionHandle subscriberH);
OSErr ResumeSubscriber(SectionHandle subscriberH);

/* Get subscriber connection status */
OSErr GetSubscriberStatus(SectionHandle subscriberH,
                         int32_t* status,
                         char* statusMessage,
                         Size messageSize);

/*
 * Edition Container Access
 */

/* Get subscriber's edition container spec */
OSErr GetSubscriberContainer(SectionHandle subscriberH,
                            EditionContainerSpec* container);

/* Update subscriber's edition container */
OSErr UpdateSubscriberContainer(SectionHandle subscriberH,
                               const EditionContainerSpec* newContainer);

/* Get edition information */
OSErr GetSubscriberEditionInfo(SectionHandle subscriberH,
                              EditionInfoRecord* editionInfo);

/* Navigate to publisher */
OSErr GoToPublisherFromSubscriber(SectionHandle subscriberH);

/*
 * Format Management
 */

/* Set accepted formats mask */
OSErr SetAcceptedFormats(SectionHandle subscriberH, uint8_t formatsMask);

/* Get accepted formats mask */
OSErr GetAcceptedFormats(SectionHandle subscriberH, uint8_t* formatsMask);

/* Add accepted format */
OSErr AddAcceptedFormat(SectionHandle subscriberH, FormatType format);

/* Remove accepted format */
OSErr RemoveAcceptedFormat(SectionHandle subscriberH, FormatType format);

/* Check if format is accepted */
OSErr IsFormatAccepted(SectionHandle subscriberH,
                      FormatType format,
                      bool* isAccepted);

/*
 * Notification and Events
 */

/* Set update notification callback */
typedef void (*SubscriberUpdateCallback)(SectionHandle subscriberH,
                                        FormatType updatedFormat,
                                        void* userData);

OSErr SetUpdateNotificationCallback(SectionHandle subscriberH,
                                   SubscriberUpdateCallback callback,
                                   void* userData);

/* Remove update notification callback */
OSErr RemoveUpdateNotificationCallback(SectionHandle subscriberH);

/* Send acknowledgment to publisher */
OSErr AcknowledgeUpdate(SectionHandle subscriberH, FormatType format);

/*
 * Subscriber Dialog and UI
 */

/* Show subscriber options dialog */
OSErr ShowSubscriberOptionsDialog(SectionHandle subscriberH,
                                 SectionOptionsReply* reply);

/* Show subscribe dialog for new subscribers */
OSErr ShowSubscribeDialog(NewSubscriberReply* reply);

/* Browse for editions to subscribe to */
OSErr BrowseForEdition(EditionContainerSpec* selectedContainer);

/*
 * Data Caching and Performance
 */

/* Enable/disable local data caching */
OSErr SetDataCaching(SectionHandle subscriberH, bool enableCaching);

/* Get data caching status */
OSErr GetDataCaching(SectionHandle subscriberH, bool* isCachingEnabled);

/* Clear cached data */
OSErr ClearDataCache(SectionHandle subscriberH);

/* Set cache size limit */
OSErr SetCacheSizeLimit(SectionHandle subscriberH, Size maxCacheSize);

/*
 * Error Handling and Diagnostics
 */

/* Validate subscriber section */
OSErr ValidateSubscriber(SectionHandle subscriberH);

/* Get detailed subscriber information */
OSErr GetSubscriberInfo(SectionHandle subscriberH,
                       AppRefNum* ownerApp,
                       TimeStamp* creationTime,
                       TimeStamp* lastAccessTime,
                       int32_t* readCount);

/* Platform abstraction functions (implemented in platform-specific files) */

/* Platform-specific subscriber dialog */
OSErr ShowNewSubscriberDialog(NewSubscriberReply* reply);

/* Platform-specific section options dialog */
OSErr ShowSectionOptionsDialog(SectionOptionsReply* reply);

/* Platform-specific data sharing registration */
OSErr RegisterSubscriberWithPlatform(SectionHandle subscriberH,
                                    const EditionContainerSpec* container);

/* Notification system */
OSErr RegisterForEditionNotifications(SectionHandle subscriberH,
                                     const EditionContainerSpec* container);
OSErr UnregisterFromEditionNotifications(SectionHandle subscriberH);

/* Edition file operations */
OSErr ReadDataFromEditionFile(void* fileBlock, FormatType format,
                             void* buffer, Size* bufferSize);
OSErr CheckFormatInEditionFile(void* fileBlock, FormatType format, Size* size);
OSErr GetEditionModificationTime(const EditionContainerSpec* container,
                                TimeStamp* modTime);

/* Constants */
#define kMaxSubscribersPerApp 128
#define kDefaultSubscriberUpdateMode sumAutomatic
#define kSubscriberCheckInterval 2000  /* milliseconds */
#define kDefaultCacheSize (64 * 1024)  /* 64KB default cache */

/* Subscriber status codes */
enum {
    kSubscriberStatusDisconnected = 0,
    kSubscriberStatusConnected = 1,
    kSubscriberStatusUpdating = 2,
    kSubscriberStatusSuspended = 3,
    kSubscriberStatusError = -1
};

/* Format preference priorities */
enum {
    kFormatPriorityHigh = 3,
    kFormatPriorityMedium = 2,
    kFormatPriorityLow = 1,
    kFormatPriorityNone = 0
};

#ifdef __cplusplus
}
#endif

#endif /* __SUBSCRIBER_MANAGER_H__ */