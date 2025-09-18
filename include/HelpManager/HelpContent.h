/*
 * HelpContent.h - Help Content Loading and Formatting
 *
 * This file defines structures and functions for loading, formatting, and
 * managing help content in various formats including text, pictures, and
 * styled text.
 */

#ifndef HELPCONTENT_H
#define HELPCONTENT_H

#include "MacTypes.h"
#include "ResourceManager.h"
#include "TextEdit.h"
#include "HelpManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Content format types */
typedef enum {
    kHMContentPlainText = 1,         /* Plain text content */
    kHMContentRichText = 2,          /* Rich text with formatting */
    kHMContentHTML = 3,              /* HTML content */
    kHMContentMarkdown = 4,          /* Markdown content */
    kHMContentPicture = 5,           /* Picture content */
    kHMContentMixed = 6              /* Mixed content types */
} HMContentFormat;

/* Content source types */
typedef enum {
    kHMContentSourceResource = 1,    /* Content from Mac resource */
    kHMContentSourceFile = 2,        /* Content from external file */
    kHMContentSourceURL = 3,         /* Content from URL */
    kHMContentSourceMemory = 4,      /* Content from memory buffer */
    kHMContentSourceCallback = 5     /* Content from callback function */
} HMContentSource;

/* Content loading options */
typedef struct HMContentOptions {
    HMContentFormat format;          /* Content format */
    HMContentSource source;          /* Content source */
    Boolean cacheContent;            /* Cache loaded content */
    Boolean preloadImages;           /* Preload embedded images */
    Boolean processLinks;            /* Process hyperlinks */
    short maxCacheSize;              /* Maximum cache size in KB */
    char encoding[16];               /* Text encoding (UTF-8, etc.) */
    char language[8];                /* Content language code */
} HMContentOptions;

/* Content metadata */
typedef struct HMContentMetadata {
    char title[256];                 /* Content title */
    char author[128];                /* Content author */
    char description[512];           /* Content description */
    char keywords[256];              /* Search keywords */
    char version[32];                /* Content version */
    long creationDate;               /* Creation date */
    long modificationDate;           /* Last modification date */
    HMContentFormat format;          /* Content format */
    long contentSize;                /* Content size in bytes */
} HMContentMetadata;

/* Styled text information */
typedef struct HMStyledTextInfo {
    Handle textHandle;               /* Text data handle */
    Handle styleHandle;              /* Style data handle */
    short fontID;                    /* Default font */
    short fontSize;                  /* Default font size */
    short fontStyle;                 /* Default font style */
    RGBColor textColor;              /* Default text color */
    RGBColor backgroundColor;        /* Background color */
    Boolean hasHyperlinks;           /* Contains hyperlinks */
    short hyperlinkCount;            /* Number of hyperlinks */
} HMStyledTextInfo;

/* Picture content information */
typedef struct HMPictureInfo {
    PicHandle pictureHandle;         /* Picture handle */
    Rect pictureBounds;              /* Picture bounds */
    short pictureType;               /* Picture type (PICT, etc.) */
    long pictureSize;                /* Picture data size */
    Boolean isColor;                 /* Is color picture */
    short bitDepth;                  /* Color bit depth */
    char altText[256];               /* Alternative text description */
} HMPictureInfo;

/* Mixed content element */
typedef struct HMContentElement {
    HMContentFormat elementType;     /* Type of this element */
    union {
        struct {
            char *text;              /* Text content */
            short fontID;            /* Font ID */
            short fontSize;          /* Font size */
            short fontStyle;         /* Font style */
            RGBColor color;          /* Text color */
        } text;
        struct {
            PicHandle picture;       /* Picture handle */
            Rect bounds;             /* Picture bounds */
            char altText[256];       /* Alternative text */
        } picture;
        struct {
            char *htmlContent;       /* HTML content */
            char baseURL[256];       /* Base URL for relative links */
        } html;
    } content;
    struct HMContentElement *next;   /* Next element in list */
} HMContentElement;

/* Content container */
typedef struct HMContentContainer {
    HMContentFormat primaryFormat;   /* Primary content format */
    HMContentMetadata metadata;      /* Content metadata */
    HMContentOptions options;        /* Loading options */
    union {
        char *plainText;             /* Plain text content */
        HMStyledTextInfo styledText; /* Styled text content */
        HMPictureInfo picture;       /* Picture content */
        HMContentElement *elements;  /* Mixed content elements */
        Handle rawData;              /* Raw data handle */
    } data;
    Boolean isLoaded;                /* Content loaded flag */
    long loadTime;                   /* Time content was loaded */
    OSErr lastError;                 /* Last loading error */
} HMContentContainer;

/* Content cache entry */
typedef struct HMContentCacheEntry {
    ResType resourceType;            /* Resource type */
    short resourceID;                /* Resource ID */
    short resourceIndex;             /* String resource index */
    char cacheKey[256];              /* Cache key string */
    HMContentContainer *content;     /* Cached content */
    long accessCount;                /* Access count */
    long lastAccessTime;             /* Last access time */
    struct HMContentCacheEntry *next; /* Next cache entry */
} HMContentCacheEntry;

/* Content loading callback */
typedef OSErr (*HMContentLoadCallback)(const char *contentID, HMContentContainer *container);

/* Content processing callback */
typedef OSErr (*HMContentProcessCallback)(HMContentContainer *container, void *userData);

/* Content loading functions */
OSErr HMContentLoad(const HMContentOptions *options, const char *contentSpec,
                   HMContentContainer **container);

OSErr HMContentLoadFromResource(ResType resourceType, short resourceID,
                              short index, HMContentContainer **container);

OSErr HMContentLoadFromFile(const char *filePath, HMContentFormat format,
                          HMContentContainer **container);

OSErr HMContentLoadFromURL(const char *url, HMContentContainer **container);

OSErr HMContentLoadFromMemory(const void *data, long dataSize,
                            HMContentFormat format, HMContentContainer **container);

/* Content formatting functions */
OSErr HMContentFormat(HMContentContainer *container, HMContentFormat targetFormat);

OSErr HMContentFormatText(const char *plainText, const HMContentOptions *options,
                        HMStyledTextInfo *styledText);

OSErr HMContentFormatHTML(const char *htmlContent, HMStyledTextInfo *styledText);

OSErr HMContentFormatMarkdown(const char *markdownContent, HMStyledTextInfo *styledText);

/* Content measurement functions */
OSErr HMContentMeasure(const HMContentContainer *container, Size *contentSize);

OSErr HMContentMeasureText(const HMStyledTextInfo *styledText, Size *textSize);

OSErr HMContentMeasurePicture(const HMPictureInfo *picture, Size *pictureSize);

/* Content rendering functions */
OSErr HMContentRender(const HMContentContainer *container, const Rect *renderRect,
                     short renderMode);

OSErr HMContentRenderText(const HMStyledTextInfo *styledText, const Rect *textRect);

OSErr HMContentRenderPicture(const HMPictureInfo *picture, const Rect *pictureRect);

OSErr HMContentRenderMixed(const HMContentElement *elements, const Rect *renderRect);

/* Content cache management */
OSErr HMContentCacheInit(short maxEntries, long maxMemory);
void HMContentCacheShutdown(void);

OSErr HMContentCacheStore(const char *cacheKey, HMContentContainer *container);
OSErr HMContentCacheRetrieve(const char *cacheKey, HMContentContainer **container);
OSErr HMContentCacheRemove(const char *cacheKey);
void HMContentCacheClear(void);

OSErr HMContentCacheGetStatistics(short *entryCount, long *memoryUsed,
                                 long *hitCount, long *missCount);

/* Content validation functions */
Boolean HMContentValidate(const HMContentContainer *container);
OSErr HMContentCheckFormat(const void *data, long dataSize, HMContentFormat *format);
Boolean HMContentIsAccessible(const HMContentContainer *container);

/* Content conversion functions */
OSErr HMContentConvertToPlainText(const HMContentContainer *source, char **plainText);
OSErr HMContentConvertToHTML(const HMContentContainer *source, char **htmlContent);
OSErr HMContentConvertToAccessibleText(const HMContentContainer *source, char **accessibleText);

/* String resource utilities */
OSErr HMContentLoadStringResource(short resourceID, short index, char **string);
OSErr HMContentLoadStringList(short resourceID, Handle *stringListHandle);
OSErr HMContentGetStringFromList(Handle stringListHandle, short index, char **string);

/* Picture resource utilities */
OSErr HMContentLoadPictureResource(short resourceID, PicHandle *picture);
OSErr HMContentScalePicture(PicHandle sourcePicture, const Size *targetSize,
                          PicHandle *scaledPicture);
OSErr HMContentConvertPictureFormat(PicHandle sourcePicture, short targetFormat,
                                  Handle *convertedPicture);

/* Styled text utilities */
OSErr HMContentCreateStyledText(const char *plainText, const HMContentOptions *options,
                              TEHandle *styledTextHandle);
OSErr HMContentApplyTextStyle(TEHandle textHandle, short startOffset, short length,
                            short fontID, short fontSize, short fontStyle,
                            const RGBColor *color);
OSErr HMContentInsertHyperlink(TEHandle textHandle, short startOffset, short length,
                             const char *linkTarget);

/* Modern content support */
OSErr HMContentSupportUTF8(Boolean enable);
OSErr HMContentSetDefaultEncoding(const char *encoding);
OSErr HMContentRegisterLoader(HMContentFormat format, HMContentLoadCallback callback);
OSErr HMContentRegisterProcessor(HMContentFormat format, HMContentProcessCallback callback);

/* Accessibility content functions */
OSErr HMContentGenerateAccessibleDescription(const HMContentContainer *container,
                                           char **description);
OSErr HMContentGenerateScreenReaderText(const HMContentContainer *container,
                                       char **screenReaderText);
Boolean HMContentHasAccessibleAlternative(const HMContentContainer *container);

/* Content disposal functions */
void HMContentDispose(HMContentContainer *container);
void HMContentDisposeElement(HMContentElement *element);
void HMContentDisposeStyledText(HMStyledTextInfo *styledText);
void HMContentDisposePicture(HMPictureInfo *picture);

#ifdef __cplusplus
}
#endif

#endif /* HELPCONTENT_H */