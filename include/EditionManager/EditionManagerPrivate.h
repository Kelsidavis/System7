/*
 * EditionManagerPrivate.h
 *
 * Private Edition Manager structures and internal API
 * Contains implementation details not exposed to applications
 */

#ifndef __EDITION_MANAGER_PRIVATE_H__
#define __EDITION_MANAGER_PRIVATE_H__

#include "EditionManager/EditionManager.h"
#include <pthread.h>

/* Internal data structures */

/* Global Edition Manager state */
typedef struct {
    SectionHandle firstSection;      /* Head of section linked list */
    SectionHandle lastSection;       /* Tail of section linked list */
    int32_t sectionCount;           /* Number of registered sections */
    int32_t nextSectionID;          /* Next available section ID */
    AppRefNum currentApp;           /* Current application reference */
    EditionOpenerProcPtr editionOpenerProc; /* Custom edition opener */
    pthread_mutex_t sectionMutex;   /* Thread safety for section list */

    /* Platform-specific data sharing handles */
    void* platformData;             /* Platform-specific data */
    void* notificationHandle;       /* Platform notification system */
} EditionManagerGlobals;

/* Edition file control block */
typedef struct {
    EditionRefNum refNum;           /* Unique reference number */
    FSSpec fileSpec;                /* File specification */
    SectionHandle ownerSection;     /* Section that owns this edition */
    bool isOpen;                    /* Whether edition is currently open */
    bool isWritable;                /* Whether opened for writing */
    int32_t openCount;              /* Reference count */

    /* Format information */
    FormatType* supportedFormats;   /* Array of supported formats */
    int32_t formatCount;            /* Number of supported formats */

    /* I/O state */
    uint32_t currentMark;           /* Current file position */
    void* ioBuffer;                 /* Internal I/O buffer */
    Size bufferSize;                /* Size of I/O buffer */

    /* Platform-specific handles */
    void* platformHandle;           /* Platform file handle */
    void* formatConverter;          /* Format conversion context */
} EditionFileBlock;

/* Format converter structure */
typedef struct {
    FormatType fromFormat;
    FormatType toFormat;
    OSErr (*convertProc)(const void* inputData, Size inputSize,
                        void** outputData, Size* outputSize);
} FormatConverter;

/* Publisher control block (based on original Mac OS structure) */
typedef struct PubCBRecord {
    struct PubCBRecord** nextPubCB;     /* Double linked list */
    struct PubCBRecord** prevPubCB;     /* Double linked list */
    Handle usageInfo;                   /* Usage information */
    int32_t volumeInfo;                 /* Volume attributes */
    int32_t pubCNodeID;                 /* File ID of container */
    TimeStamp lastVolMod;               /* Last volume modification */
    TimeStamp lastDirMod;               /* Last directory modification */
    struct PubCBRecord** oldPubCB;      /* Previous publisher CB */
    AppRefNum publisherApp;             /* Publisher application */
    SectionHandle publisher;            /* Publisher section */
    Handle publisherAlias;              /* Publisher alias */
    int16_t publisherCount;             /* Number of publishers */
    SectionType publisherKind;          /* Publisher type */
    bool fileMissing;                   /* File missing flag */
    int16_t fileRefNum;                 /* File reference number */
    int16_t openMode;                   /* File open mode */
    uint32_t fileMark;                  /* Current file position */
    int16_t rangeLockStart;             /* Range lock start */
    int16_t rangeLockLen;               /* Range lock length */
    Handle allocMap;                    /* Allocation map */
    Handle formats;                     /* Format list */
    EditionInfoRecord info;             /* Edition information */
} PubCBRecord;

/* Internal function prototypes */

/* Platform abstraction layer */
OSErr InitDataSharingPlatform(void);
void CleanupDataSharingPlatform(void);

OSErr RegisterSectionWithPlatform(SectionHandle sectionH, const FSSpec* document);
void UnregisterSectionFromPlatform(SectionHandle sectionH);
OSErr AssociateSectionWithPlatform(SectionHandle sectionH, const FSSpec* document);

OSErr PostSectionEventToPlatform(SectionHandle sectionH, AppRefNum toApp, ResType classID);
OSErr ProcessDataSharingBackground(void);

/* File system operations */
OSErr CreateAliasFromFSSpec(const FSSpec* fileSpec, Handle* alias);
OSErr UpdateAlias(const FSSpec* fileSpec, Handle alias, bool* wasUpdated);
OSErr GetEditionFileInfo(const FSSpec* fileSpec, EditionInfoRecord* info);

/* Edition file management */
OSErr CreateEditionFileInternal(const FSSpec* fileSpec, OSType creator);
OSErr OpenEditionFileInternal(const FSSpec* fileSpec, bool forWriting, EditionFileBlock** fileBlock);
OSErr CloseEditionFileInternal(EditionFileBlock* fileBlock);

/* Format conversion */
OSErr RegisterFormatConverter(FormatType fromFormat, FormatType toFormat,
                             OSErr (*convertProc)(const void*, Size, void**, Size*));
OSErr ConvertFormat(FormatType fromFormat, FormatType toFormat,
                   const void* inputData, Size inputSize,
                   void** outputData, Size* outputSize);

/* Data synchronization */
OSErr SynchronizePublisherData(SectionHandle publisherH);
OSErr SynchronizeSubscriberData(SectionHandle subscriberH);
OSErr NotifySubscribers(SectionHandle publisherH);

/* Thread safety */
void LockSectionList(void);
void UnlockSectionList(void);

/* Memory management */
Handle NewHandleFromData(const void* data, Size size);
OSErr ResizeHandle(Handle handle, Size newSize);
void DisposeHandleData(Handle handle);

/* Utility functions */
TimeStamp GetCurrentTimeStamp(void);
bool CompareFSSpec(const FSSpec* spec1, const FSSpec* spec2);
OSErr CopyFSSpec(const FSSpec* source, FSSpec* dest);

/* Error handling */
void LogEditionError(const char* function, OSErr error, const char* message);

/* Constants for internal use */
#define kMaxSupportedFormats 32
#define kDefaultIOBufferSize 8192
#define kEditionMagicNumber 0x45444954  /* 'EDIT' */
#define kClosedFile -1

/* Platform-specific constants */
#ifdef _WIN32
    #define kPlatformPathSeparator '\\'
    #define kPlatformMaxPath 260
#else
    #define kPlatformPathSeparator '/'
    #define kPlatformMaxPath 1024
#endif

/* Debug macros */
#ifdef DEBUG_EDITION_MANAGER
    #define EDITION_DEBUG(fmt, ...) printf("[EditionMgr] " fmt "\n", ##__VA_ARGS__)
    #define EDITION_ERROR(fmt, ...) fprintf(stderr, "[EditionMgr ERROR] " fmt "\n", ##__VA_ARGS__)
#else
    #define EDITION_DEBUG(fmt, ...)
    #define EDITION_ERROR(fmt, ...)
#endif

/* Global variables (defined in EditionManagerCore.c) */
extern EditionManagerGlobals* gEditionGlobals;
extern bool gEditionPackInitialized;

#endif /* __EDITION_MANAGER_PRIVATE_H__ */