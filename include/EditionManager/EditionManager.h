/*
 * EditionManager.h
 *
 * Main Edition Manager API - System 7 Publish/Subscribe Functionality
 * Provides live data sharing and linking between applications for dynamic collaboration
 *
 * Originally based on Apple's System 7.1 Edition Manager (Pack11)
 * This is a portable implementation that abstracts the original Mac OS functionality
 */

#ifndef __EDITION_MANAGER_H__
#define __EDITION_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Forward declarations */
typedef struct SectionRecord SectionRecord;
typedef struct SectionRecord *SectionPtr;
typedef struct SectionRecord **SectionHandle;
typedef struct EditionContainerSpec EditionContainerSpec;
typedef void* EditionRefNum;
typedef void* AppRefNum;

/* Basic types compatible with original Mac OS API */
typedef uint32_t OSType;
typedef uint32_t ResType;
typedef int16_t OSErr;
typedef uint32_t FormatType;
typedef uint32_t TimeStamp;
typedef int16_t ScriptCode;
typedef int32_t Size;
typedef void* Handle;
typedef uint8_t* Ptr;
typedef char Str31[32];

/* Error codes */
enum {
    noErr = 0,
    editionMgrInitErr = -450,
    badSectionErr = -451,
    notAnEditionContainerErr = -452,
    badEditionFileErr = -453,
    badSubPartErr = -454,
    multiplePublisherWrn = -460,
    containerNotFoundWrn = -461,
    containerAlreadyOpenWrn = -462,
    notThePublisherWrn = -463
};

/* Section types */
typedef uint8_t SectionType;
enum {
    stSubscriber = 0x01,
    stPublisher = 0x0A
};

/* Update modes */
typedef int16_t UpdateMode;
enum {
    sumAutomatic = 0,    /* subscriber update mode - Automatically */
    sumManual = 1,       /* subscriber update mode - Manually */
    pumOnSave = 0,       /* publisher update mode - OnSave */
    pumManual = 1        /* publisher update mode - Manually */
};

/* File specification structure */
typedef struct {
    char name[256];      /* File name */
    char path[1024];     /* Full path */
    int32_t parentDirID; /* Parent directory ID */
    int16_t vRefNum;     /* Volume reference number */
} FSSpec;

/* Edition container specification */
struct EditionContainerSpec {
    FSSpec theFile;
    ScriptCode theFileScript;
    int32_t thePart;
    Str31 thePartName;
    ScriptCode thePartScript;
};

/* Section record - core data structure for publishers and subscribers */
struct SectionRecord {
    uint8_t version;            /* always 0x01 in system 7.0 */
    SectionType kind;           /* stSubscriber or stPublisher */
    UpdateMode mode;            /* auto or manual */
    TimeStamp mdDate;           /* last change in document */
    int32_t sectionID;          /* app. specific, unique per document */
    int32_t refCon;             /* application specific */
    Handle alias;               /* handle to Alias Record */
    int32_t subPart;            /* which part of container file */
    SectionHandle nextSection;  /* for linked list of app's Sections */
    Handle controlBlock;        /* used internally */
    EditionRefNum refNum;       /* used internally */
};

/* Edition information record */
typedef struct {
    TimeStamp crDate;           /* date EditionContainer was created */
    TimeStamp mdDate;           /* date of last change */
    OSType fdCreator;           /* file creator */
    OSType fdType;              /* file type */
    EditionContainerSpec container; /* the Edition */
} EditionInfoRecord;

/* Dialog reply structures */
typedef struct {
    bool canceled;              /* Output */
    bool replacing;
    bool usePart;               /* Input */
    Handle preview;             /* Input */
    FormatType previewFormat;   /* Input */
    EditionContainerSpec container; /* Input/Output */
} NewPublisherReply;

typedef struct {
    bool canceled;              /* Output */
    uint8_t formatsMask;
    EditionContainerSpec container; /* Input/Output */
} NewSubscriberReply;

typedef struct {
    bool canceled;              /* Output */
    bool changed;               /* Output */
    SectionHandle sectionH;     /* Input */
    ResType action;             /* Output */
} SectionOptionsReply;

/* Format I/O structures */
typedef enum {
    ioHasFormat,
    ioReadFormat,
    ioNewFormat,
    ioWriteFormat
} FormatIOVerb;

typedef struct {
    int32_t ioRefNum;
    FormatType format;
    int32_t formatIndex;
    uint32_t offset;
    Ptr buffPtr;
    uint32_t buffLen;
} FormatIOParamBlock;

/* Edition opener structures */
typedef enum {
    eoOpen,
    eoClose,
    eoOpenNew,
    eoCloseNew,
    eoCanSubscribe
} EditionOpenerVerb;

typedef struct {
    EditionInfoRecord info;
    SectionHandle sectionH;
    FSSpec* document;
    OSType fdCreator;
    int32_t ioRefNum;
    void* ioProc;              /* FormatIOProcPtr */
    bool success;
    uint8_t formatsMask;
} EditionOpenerParamBlock;

/* Function pointer types */
typedef int16_t (*FormatIOProcPtr)(FormatIOVerb selector, FormatIOParamBlock* PB);
typedef int16_t (*EditionOpenerProcPtr)(EditionOpenerVerb selector, EditionOpenerParamBlock* PB);

/* Constants */
enum {
    kFormatLengthUnknown = -1,
    kPartsNotUsed = 0,
    kPartNumberUnknown = -1,
    kPreviewWidth = 120,
    kPreviewHeight = 120
};

/* Resource types */
#define rSectionType 'sect'
#define kPublisherDocAliasFormat 'alis'
#define kPreviewFormat 'prvw'
#define kFormatListFormat 'fmts'

/* Format masks */
enum {
    kPICTformatMask = 1,
    kTEXTformatMask = 2,
    ksndFormatMask = 4
};

/* Finder types for edition files */
#define kPICTEditionFileType 'edtp'
#define kTEXTEditionFileType 'edtt'
#define ksndEditionFileType 'edts'
#define kUnknownEditionFileType 'edtu'

/* Section event constants */
#define sectionEventMsgClass 'sect'
#define sectionReadMsgID 'read'
#define sectionWriteMsgID 'writ'
#define sectionScrollMsgID 'scrl'
#define sectionCancelMsgID 'cncl'

#ifdef __cplusplus
extern "C" {
#endif

/* Core Edition Manager API */

/* Initialization and cleanup */
OSErr InitEditionPack(void);
OSErr QuitEditionPack(void);

/* Section management */
OSErr NewSection(const EditionContainerSpec* container,
                 const FSSpec* sectionDocument,
                 SectionType kind,
                 int32_t sectionID,
                 UpdateMode initialMode,
                 SectionHandle* sectionH);

OSErr RegisterSection(const FSSpec* sectionDocument,
                     SectionHandle sectionH,
                     bool* aliasWasUpdated);

OSErr UnRegisterSection(SectionHandle sectionH);
OSErr IsRegisteredSection(SectionHandle sectionH);

OSErr AssociateSection(SectionHandle sectionH,
                      const FSSpec* newSectionDocument);

/* Edition container management */
OSErr CreateEditionContainerFile(const FSSpec* editionFile,
                                OSType fdCreator,
                                ScriptCode editionFileNameScript);

OSErr DeleteEditionContainerFile(const FSSpec* editionFile);

/* Edition I/O */
OSErr OpenEdition(SectionHandle subscriberSectionH,
                 EditionRefNum* refNum);

OSErr OpenNewEdition(SectionHandle publisherSectionH,
                    OSType fdCreator,
                    const FSSpec* publisherSectionDocument,
                    EditionRefNum* refNum);

OSErr CloseEdition(EditionRefNum whichEdition, bool successful);

/* Data format operations */
OSErr EditionHasFormat(EditionRefNum whichEdition,
                      FormatType whichFormat,
                      Size* formatSize);

OSErr ReadEdition(EditionRefNum whichEdition,
                 FormatType whichFormat,
                 void* buffPtr,
                 Size* buffLen);

OSErr WriteEdition(EditionRefNum whichEdition,
                  FormatType whichFormat,
                  const void* buffPtr,
                  Size buffLen);

OSErr GetEditionFormatMark(EditionRefNum whichEdition,
                          FormatType whichFormat,
                          uint32_t* currentMark);

OSErr SetEditionFormatMark(EditionRefNum whichEdition,
                          FormatType whichFormat,
                          uint32_t setMarkTo);

/* Edition information */
OSErr GetEditionInfo(const SectionHandle sectionH,
                    EditionInfoRecord* editionInfo);

OSErr GoToPublisherSection(const EditionContainerSpec* container);

OSErr GetLastEditionContainerUsed(EditionContainerSpec* container);

OSErr GetStandardFormats(const EditionContainerSpec* container,
                        FormatType* previewFormat,
                        Handle preview,
                        Handle publisherAlias,
                        Handle formats);

/* Edition opener management */
OSErr GetEditionOpenerProc(EditionOpenerProcPtr* opener);
OSErr SetEditionOpenerProc(EditionOpenerProcPtr opener);

OSErr CallEditionOpenerProc(EditionOpenerVerb selector,
                           EditionOpenerParamBlock* PB,
                           EditionOpenerProcPtr routine);

OSErr CallFormatIOProc(FormatIOVerb selector,
                      FormatIOParamBlock* PB,
                      FormatIOProcPtr routine);

/* User interface dialogs */
OSErr NewSubscriberDialog(NewSubscriberReply* reply);
OSErr NewPublisherDialog(NewPublisherReply* reply);
OSErr SectionOptionsDialog(SectionOptionsReply* reply);

/* Private/internal functions */
OSErr GetCurrentAppRefNum(AppRefNum* thisApp);
OSErr PostSectionEvent(SectionHandle sectionH, AppRefNum toApp, ResType classID);
OSErr EditionBackGroundTask(void);

#ifdef __cplusplus
}
#endif

#endif /* __EDITION_MANAGER_H__ */