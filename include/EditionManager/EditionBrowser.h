/*
 * EditionBrowser.h
 *
 * Edition Browser API for Edition Manager
 * Provides UI for finding, selecting, and managing edition containers
 */

#ifndef __EDITION_BROWSER_H__
#define __EDITION_BROWSER_H__

#include "EditionManager/EditionManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Edition Preview Information
 */
typedef struct {
    TimeStamp creationDate;         /* Edition creation date */
    TimeStamp modificationDate;     /* Last modification date */
    OSType fileCreator;             /* File creator */
    OSType fileType;                /* File type */
    Size fileSize;                  /* File size in bytes */

    FormatType* supportedFormats;   /* Array of supported formats */
    int32_t formatCount;            /* Number of supported formats */

    bool hasPreview;                /* Whether preview is available */
    FormatType previewFormat;       /* Format of preview data */
    Size previewSize;               /* Size of preview data */
    uint8_t previewData[1024];      /* Preview data (up to 1KB) */

    char publisherName[64];         /* Publisher application name */
    char documentName[256];         /* Source document name */
    char description[512];          /* Edition description */
} EditionPreviewInfo;

/*
 * Edition Browser Filter
 */
typedef struct {
    FormatType requiredFormat;      /* Required format (0 = any) */
    OSType creatorFilter;           /* Creator filter (0 = any) */
    OSType typeFilter;              /* Type filter (0 = any) */
    TimeStamp newerThan;            /* Show only newer editions */
    TimeStamp olderThan;            /* Show only older editions */
    Size minSize;                   /* Minimum file size */
    Size maxSize;                   /* Maximum file size */
    bool showHidden;                /* Show hidden editions */
    bool showPreview;               /* Show preview pane */
} EditionBrowserFilter;

/*
 * Edition Browser Callback
 */
typedef void (*EditionBrowserCallback)(const EditionContainerSpec* selectedEdition,
                                      const EditionPreviewInfo* previewInfo,
                                      void* userData);

/*
 * Edition Selection Result
 */
typedef struct {
    bool userCanceled;              /* User canceled selection */
    EditionContainerSpec container; /* Selected edition container */
    EditionPreviewInfo previewInfo; /* Preview information */
    FormatType selectedFormat;      /* User-selected format */
    uint8_t formatsMask;            /* Selected formats mask */
} EditionSelectionResult;

/*
 * Core Browser Functions
 */

/* Browse for edition containers */
OSErr BrowseForEdition(const char* startPath, EditionContainerSpec* selectedContainer);

/* Browse with filter criteria */
OSErr BrowseForEditionWithFilter(const char* startPath,
                                const EditionBrowserFilter* filter,
                                EditionSelectionResult* result);

/* Get list of available editions in directory */
OSErr GetAvailableEditions(const char* directoryPath,
                          EditionContainerSpec** editions,
                          int32_t* editionCount);

/* Filter editions by format support */
OSErr FilterEditionsByFormat(const EditionContainerSpec* editions,
                            int32_t editionCount,
                            FormatType format,
                            EditionContainerSpec** filteredEditions,
                            int32_t* filteredCount);

/* Get preview information for edition */
OSErr GetEditionPreviewInfo(const EditionContainerSpec* container,
                           EditionPreviewInfo* previewInfo);

/*
 * Dialog Functions
 */

/* Show standard subscriber dialog */
OSErr ShowSubscriberBrowserDialog(EditionSelectionResult* result);

/* Show standard publisher dialog */
OSErr ShowPublisherBrowserDialog(NewPublisherReply* reply);

/* Show custom edition browser dialog */
OSErr ShowCustomEditionBrowser(const char* title,
                              const EditionBrowserFilter* filter,
                              EditionSelectionResult* result);

/*
 * Browser Configuration
 */

/* Set browser filter criteria */
OSErr SetEditionBrowserFilter(FormatType format, bool showPreview);

/* Set browser callback */
OSErr SetEditionBrowserCallback(EditionBrowserCallback callback, void* userData);

/* Set browser window properties */
typedef struct {
    int32_t x, y;                   /* Window position */
    int32_t width, height;          /* Window size */
    bool resizable;                 /* Window can be resized */
    bool modal;                     /* Modal dialog */
    char title[128];                /* Window title */
} BrowserWindowProps;

OSErr SetBrowserWindowProperties(const BrowserWindowProps* props);

/* Get browser window properties */
OSErr GetBrowserWindowProperties(BrowserWindowProps* props);

/*
 * Recent Editions Management
 */

/* Add edition to recent list */
OSErr AddToRecentEditions(const EditionContainerSpec* container);

/* Get recent editions list */
OSErr GetRecentEditions(EditionContainerSpec** recentEditions,
                       int32_t* editionCount);

/* Clear recent editions list */
OSErr ClearRecentEditions(void);

/* Set maximum recent editions count */
OSErr SetMaxRecentEditions(int32_t maxCount);

/*
 * Edition Search and Discovery
 */

/* Search for editions by name pattern */
OSErr SearchEditionsByName(const char* searchPath,
                          const char* namePattern,
                          EditionContainerSpec** foundEditions,
                          int32_t* editionCount);

/* Search for editions by content */
OSErr SearchEditionsByContent(const char* searchPath,
                             FormatType format,
                             const void* searchData,
                             Size dataSize,
                             EditionContainerSpec** foundEditions,
                             int32_t* editionCount);

/* Search for editions by metadata */
OSErr SearchEditionsByMetadata(const char* searchPath,
                              OSType creator,
                              OSType type,
                              TimeStamp newerThan,
                              EditionContainerSpec** foundEditions,
                              int32_t* editionCount);

/*
 * Edition Organization
 */

/* Get edition categories */
typedef struct {
    char categoryName[64];          /* Category name */
    EditionContainerSpec* editions; /* Editions in category */
    int32_t editionCount;           /* Number of editions */
} EditionCategory;

OSErr GetEditionCategories(const char* searchPath,
                          EditionCategory** categories,
                          int32_t* categoryCount);

/* Create edition alias/shortcut */
OSErr CreateEditionAlias(const EditionContainerSpec* container,
                        const char* aliasPath);

/* Resolve edition alias */
OSErr ResolveEditionAlias(const char* aliasPath,
                         EditionContainerSpec* container);

/*
 * Edition Validation and Repair
 */

/* Validate edition accessibility */
OSErr ValidateEditionAccess(const EditionContainerSpec* container,
                           bool* isAccessible,
                           char* errorMessage,
                           Size messageSize);

/* Check for broken edition links */
OSErr CheckEditionLinks(const EditionContainerSpec* editions,
                       int32_t editionCount,
                       bool** brokenLinks);

/* Repair broken edition links */
OSErr RepairEditionLinks(EditionContainerSpec* editions,
                        int32_t editionCount,
                        int32_t* repairedCount);

/*
 * Preview Generation
 */

/* Generate preview for edition */
OSErr GenerateEditionPreview(const EditionContainerSpec* container,
                            FormatType previewFormat,
                            Handle* previewData);

/* Update edition preview */
OSErr UpdateEditionPreview(const EditionContainerSpec* container,
                          FormatType previewFormat,
                          const void* previewData,
                          Size dataSize);

/* Get preview thumbnail */
OSErr GetPreviewThumbnail(const EditionContainerSpec* container,
                         int32_t thumbnailWidth,
                         int32_t thumbnailHeight,
                         Handle* thumbnailData);

/*
 * Edition Import/Export
 */

/* Import edition from external format */
OSErr ImportEditionFromFile(const char* filePath,
                           FormatType sourceFormat,
                           EditionContainerSpec* newContainer);

/* Export edition to external format */
OSErr ExportEditionToFile(const EditionContainerSpec* container,
                         const char* outputPath,
                         FormatType targetFormat);

/* Batch import multiple files */
OSErr BatchImportEditions(const char** filePaths,
                         const FormatType* sourceFormats,
                         int32_t fileCount,
                         EditionContainerSpec** newContainers,
                         int32_t* successCount);

/*
 * Platform-Specific UI Functions
 */

/* Show platform-specific subscriber dialog */
OSErr ShowPlatformSubscriberDialog(NewSubscriberReply* reply);

/* Show platform-specific publisher dialog */
OSErr ShowPlatformPublisherDialog(NewPublisherReply* reply);

/* Show platform-specific section options dialog */
OSErr ShowPlatformSectionOptionsDialog(SectionOptionsReply* reply);

/* Show platform-specific edition browser interface */
OSErr ShowEditionBrowserInterface(EditionContainerSpec* selectedContainer);

/*
 * Accessibility and Localization
 */

/* Set browser UI language */
OSErr SetBrowserLanguage(const char* languageCode);

/* Get localized string for browser UI */
OSErr GetLocalizedBrowserString(const char* stringKey,
                               char* localizedString,
                               Size stringSize);

/* Set accessibility options */
typedef struct {
    bool useHighContrast;           /* High contrast mode */
    bool useLargeText;              /* Large text mode */
    bool useScreenReader;           /* Screen reader support */
    bool useKeyboardNavigation;     /* Keyboard-only navigation */
} AccessibilityOptions;

OSErr SetBrowserAccessibility(const AccessibilityOptions* options);

/*
 * Performance and Caching
 */

/* Enable/disable preview caching */
OSErr SetPreviewCaching(bool enable, Size maxCacheSize);

/* Clear preview cache */
OSErr ClearPreviewCache(void);

/* Set scan performance options */
typedef struct {
    bool useAsyncScanning;          /* Asynchronous directory scanning */
    int32_t maxScanDepth;           /* Maximum subdirectory depth */
    int32_t scanTimeoutMs;          /* Timeout for directory scan */
    bool cacheResults;              /* Cache scan results */
} ScanOptions;

OSErr SetScanOptions(const ScanOptions* options);

/*
 * Events and Notifications
 */

/* Browser event types */
typedef enum {
    kBrowserEventSelectionChanged,  /* Selection changed */
    kBrowserEventPreviewUpdated,    /* Preview updated */
    kBrowserEventFilterChanged,     /* Filter changed */
    kBrowserEventScanCompleted,     /* Directory scan completed */
    kBrowserEventError              /* Error occurred */
} BrowserEventType;

/* Browser event callback */
typedef void (*BrowserEventCallback)(BrowserEventType eventType,
                                    void* eventData,
                                    void* userData);

/* Set browser event callback */
OSErr SetBrowserEventCallback(BrowserEventCallback callback, void* userData);

/* Post browser event */
OSErr PostBrowserEvent(BrowserEventType eventType, void* eventData);

/*
 * Constants and Limits
 */

#define kMaxEditionsInBrowser 1000      /* Maximum editions to display */
#define kMaxRecentEditions 20           /* Maximum recent editions */
#define kMaxPreviewSize 32768           /* Maximum preview data size */
#define kDefaultThumbnailSize 64        /* Default thumbnail size */
#define kMaxSearchResults 500           /* Maximum search results */

/* Browser view modes */
enum {
    kBrowserViewList,               /* List view */
    kBrowserViewIcons,              /* Icon view */
    kBrowserViewDetails,            /* Detailed list view */
    kBrowserViewPreview             /* Preview view */
};

/* Sort options */
enum {
    kBrowserSortByName,             /* Sort by name */
    kBrowserSortByDate,             /* Sort by modification date */
    kBrowserSortBySize,             /* Sort by file size */
    kBrowserSortByType,             /* Sort by file type */
    kBrowserSortByCreator           /* Sort by creator */
};

/* Error codes specific to browser */
enum {
    browserInitErr = -500,          /* Browser initialization failed */
    browserCanceledErr = -501,      /* User canceled browser */
    browserAccessErr = -502,        /* Access denied to directory */
    browserCorruptErr = -503        /* Corrupt edition data */
};

#ifdef __cplusplus
}
#endif

#endif /* __EDITION_BROWSER_H__ */