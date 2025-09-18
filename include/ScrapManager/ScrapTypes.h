/*
 * ScrapTypes.h - Scrap Manager Data Structures and Constants
 * System 7.1 Portable - Scrap Manager Component
 *
 * Defines core data types, structures, and constants for the Mac OS Scrap Manager.
 * The Scrap Manager provides clipboard functionality for inter-application data exchange.
 */

#ifndef SCRAP_TYPES_H
#define SCRAP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "MacTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Scrap Manager Constants */
#define SCRAP_STATE_LOADED       0x0001    /* Scrap is loaded in memory */
#define SCRAP_STATE_DISK         0x0002    /* Scrap is on disk */
#define SCRAP_STATE_PRIVATE      0x0004    /* Private scrap flag */
#define SCRAP_STATE_CONVERTED    0x0008    /* Scrap has been converted */
#define SCRAP_STATE_RESERVED     0x0010    /* Reserved state flag */

/* Maximum scrap sizes */
#define MAX_SCRAP_SIZE           0x7FFFFFF0L   /* Maximum scrap size */
#define MAX_MEMORY_SCRAP         32000         /* Maximum memory-based scrap */
#define SCRAP_FILE_THRESHOLD     16384         /* Size threshold for disk storage */

/* Scrap file constants */
#define SCRAP_FILE_NAME          "Clipboard File"
#define SCRAP_FILE_TYPE          'CLIP'
#define SCRAP_FILE_CREATOR       'MACS'
#define SCRAP_TEMP_PREFIX        "ScrapTemp"

/* Common scrap data types (ResType format) */
#define SCRAP_TYPE_TEXT          'TEXT'        /* Plain text */
#define SCRAP_TYPE_PICT          'PICT'        /* QuickDraw picture */
#define SCRAP_TYPE_SOUND         'snd '        /* Sound resource */
#define SCRAP_TYPE_STYLE         'styl'        /* TextEdit style info */
#define SCRAP_TYPE_STRING        'STR '        /* Pascal string */
#define SCRAP_TYPE_STRINGLIST    'STR#'        /* String list */
#define SCRAP_TYPE_ICON          'ICON'        /* Icon */
#define SCRAP_TYPE_CICN          'cicn'        /* Color icon */
#define SCRAP_TYPE_MOVIE         'moov'        /* QuickTime movie */
#define SCRAP_TYPE_FILE          'hfs '        /* File reference */
#define SCRAP_TYPE_FOLDER        'fdrp'        /* Folder reference */
#define SCRAP_TYPE_URL           'url '        /* URL string */

/* Modern clipboard format mappings */
#define SCRAP_TYPE_UTF8          'utf8'        /* UTF-8 text */
#define SCRAP_TYPE_RTF           'RTF '        /* Rich Text Format */
#define SCRAP_TYPE_HTML          'HTML'        /* HTML markup */
#define SCRAP_TYPE_PDF           'PDF '        /* PDF data */
#define SCRAP_TYPE_PNG           'PNG '        /* PNG image */
#define SCRAP_TYPE_JPEG          'JPEG'        /* JPEG image */
#define SCRAP_TYPE_TIFF          'TIFF'        /* TIFF image */

/* Error codes */
typedef enum {
    scrapNoError = 0,
    scrapNoScrap = -100,              /* No scrap data available */
    scrapNoTypeError = -102,          /* Requested type not available */
    scrapTooManyFormats = -103,       /* Too many formats in scrap */
    scrapSizeError = -104,            /* Scrap size error */
    scrapMemoryError = -108,          /* Memory allocation failed */
    scrapDiskError = -109,            /* Disk I/O error */
    scrapConversionError = -110,      /* Data conversion failed */
    scrapFormatError = -111,          /* Invalid format */
    scrapPermissionError = -112,      /* Permission denied */
    scrapCorruptError = -113          /* Scrap data corrupted */
} ScrapError;

/* Scrap data format entry */
typedef struct {
    ResType         type;             /* Data type (4-char code) */
    int32_t         size;             /* Size of data in bytes */
    int32_t         offset;           /* Offset in scrap data */
    uint16_t        flags;            /* Format-specific flags */
    uint16_t        reserved;         /* Reserved for future use */
} ScrapFormatEntry;

/* Scrap format table */
typedef struct {
    uint16_t        count;            /* Number of formats */
    uint16_t        maxCount;         /* Maximum capacity */
    ScrapFormatEntry formats[1];      /* Variable length array */
} ScrapFormatTable;

/* Main scrap data structure */
typedef struct {
    int32_t         scrapSize;        /* Total size of scrap data */
    Handle          scrapHandle;      /* Handle to scrap data */
    int16_t         scrapCount;       /* Change counter */
    int16_t         scrapState;       /* State flags */
    StringPtr       scrapName;        /* Name of scrap file */

    /* Extended fields for portable implementation */
    ScrapFormatTable *formatTable;   /* Format table */
    void            *privateData;     /* Private implementation data */
    uint32_t        lastModified;    /* Last modification time */
    uint16_t        version;          /* Scrap format version */
    uint16_t        flags;            /* Extended flags */
} ScrapStuff;

typedef ScrapStuff *PScrapStuff;

/* Scrap format conversion function type */
typedef OSErr (*ScrapConverterProc)(
    ResType         sourceType,
    ResType         destType,
    Handle          sourceData,
    Handle          *destData,
    void            *refCon
);

/* Scrap format converter entry */
typedef struct {
    ResType         sourceType;       /* Source format */
    ResType         destType;         /* Destination format */
    ScrapConverterProc converter;     /* Conversion function */
    void            *refCon;          /* Converter reference */
    uint16_t        priority;         /* Conversion priority */
    uint16_t        flags;            /* Converter flags */
} ScrapConverter;

/* Scrap conversion context */
typedef struct {
    ScrapConverter  *converters;      /* Available converters */
    uint16_t        converterCount;   /* Number of converters */
    uint16_t        maxConverters;    /* Maximum converters */
    void            *privateData;     /* Private converter data */
} ScrapConversionContext;

/* Platform-specific clipboard data */
typedef struct {
    void            *platformHandle;  /* Platform clipboard handle */
    uint32_t        platformFormat;   /* Platform format ID */
    size_t          dataSize;         /* Size of platform data */
    void            *userData;        /* User-defined data */
} PlatformClipboardData;

/* Modern clipboard integration structure */
typedef struct {
    bool            isNativeClipboard;    /* Using native clipboard */
    void            *nativeContext;       /* Native clipboard context */
    PlatformClipboardData *platformData; /* Platform-specific data */
    uint32_t        changeSequence;      /* Native change sequence */
    void            *callbackData;       /* Callback user data */
} ModernClipboardContext;

/* Scrap file header structure */
typedef struct {
    uint32_t        signature;        /* File signature 'SCRF' */
    uint16_t        version;          /* File format version */
    uint16_t        flags;            /* File flags */
    uint32_t        createTime;       /* Creation timestamp */
    uint32_t        modifyTime;       /* Modification timestamp */
    uint32_t        dataSize;         /* Total data size */
    uint16_t        formatCount;      /* Number of formats */
    uint16_t        reserved;         /* Reserved */
} ScrapFileHeader;

/* Scrap memory block header */
typedef struct {
    uint32_t        signature;        /* Block signature 'SCRP' */
    uint32_t        size;             /* Block size */
    ResType         type;             /* Data type */
    uint32_t        checksum;         /* Data checksum */
} ScrapBlockHeader;

/* Function pointer types for callbacks */
typedef void (*ScrapChangeCallback)(int16_t newCount, void *userData);
typedef OSErr (*ScrapIOCallback)(OSErr result, void *userData);

#ifdef __cplusplus
}
#endif

#endif /* SCRAP_TYPES_H */