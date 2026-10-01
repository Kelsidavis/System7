/*
 * ScrapTypes.h - Scrap Manager Data Structures and Constants
 * System 7.1 Portable - Scrap Manager Component
 *
 * Defines core data types, structures, and constants for the Mac OS Scrap Manager.
 * The Scrap Manager provides clipboard functionality for inter-application data exchange.
 */

#ifndef SCRAP_TYPES_H
#define SCRAP_TYPES_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

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

#include "SystemTypes.h"
#define SCRAP_TYPE_PDF           'PDF '        /* PDF data */
#define SCRAP_TYPE_PNG           'PNG '        /* PNG image */
#define SCRAP_TYPE_JPEG          'JPEG'        /* JPEG image */
#define SCRAP_TYPE_TIFF          'TIFF'        /* TIFF image */

/* Error codes */

/* Scrap data format entry */

/* Scrap format table */

/* Main scrap data structure */

/* Scrap format conversion function type */

/* Scrap format converter entry */

/* Scrap conversion context */

/* Platform-specific clipboard data */

/* Modern clipboard integration structure */

/* Scrap file header structure */

/* Scrap memory block header */

/* Function pointer types for callbacks */

/* Scrap item structure */
typedef struct ScrapItem {
    ResType type;
    Handle data;
} ScrapItem;

/* ScrapManager API - MVP functions (prefixed to avoid conflicts) */
void   Scrap_Zero(void);
Size   Scrap_Get(void* dest, ResType type);
OSErr  Scrap_Put(Size size, ResType type, const void* src);
void   Scrap_Info(short* count, short* state);
void   Scrap_Unload(void);

/* Process-aware extensions */
#include "ProcessMgr/ProcessTypes.h"  /* Get ProcessID type */
ProcessID Scrap_GetOwner(void);

/* Standard scrap types for MVP */
#define kScrapTypeTEXT 'TEXT'
#define kScrapTypePICT 'PICT'

/* Classic Mac OS Scrap Manager API */
void ZeroScrap(void);
void PutScrap(long byteCount, OSType theType, const void* sourcePtr);
long GetScrap(Handle hDest, OSType theType, long* offset);
void LoadScrap(void);
void UnloadScrap(void);
long InfoScrap(void);

/* Helper functions */
Boolean ScrapHasFlavor(OSType theType);
long ScrapGetFlavorSize(OSType theType);

#ifdef __cplusplus
}
#endif

#endif /* SCRAP_TYPES_H */
