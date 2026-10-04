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
#define SCRAP_FILE_TYPE          FOURCC('C', 'L', 'I', 'P')
#define SCRAP_FILE_CREATOR       FOURCC('M', 'A', 'C', 'S')
#define SCRAP_TEMP_PREFIX        "ScrapTemp"

/* Common scrap data types (ResType format) */
#define SCRAP_TYPE_TEXT          FOURCC('T', 'E', 'X', 'T') /* Plain text */
#define SCRAP_TYPE_PICT          FOURCC('P', 'I', 'C', 'T') /* QuickDraw picture */
#define SCRAP_TYPE_SOUND         FOURCC('s', 'n', 'd', ' ') /* Sound resource */
#define SCRAP_TYPE_STYLE         FOURCC('s', 't', 'y', 'l') /* TextEdit style info */
#define SCRAP_TYPE_STRING        FOURCC('S', 'T', 'R', ' ') /* Pascal string */
#define SCRAP_TYPE_STRINGLIST    FOURCC('S', 'T', 'R', '#') /* String list */
#define SCRAP_TYPE_ICON          FOURCC('I', 'C', 'O', 'N') /* Icon */
#define SCRAP_TYPE_CICN          FOURCC('c', 'i', 'c', 'n') /* Color icon */
#define SCRAP_TYPE_MOVIE         FOURCC('m', 'o', 'o', 'v') /* QuickTime movie */
#define SCRAP_TYPE_FILE          FOURCC('h', 'f', 's', ' ') /* File reference */
#define SCRAP_TYPE_FOLDER        FOURCC('f', 'd', 'r', 'p') /* Folder reference */
#define SCRAP_TYPE_URL           FOURCC('u', 'r', 'l', ' ') /* URL string */

/* Modern clipboard format mappings */
#define SCRAP_TYPE_UTF8          FOURCC('u', 't', 'f', '8') /* UTF-8 text */
#define SCRAP_TYPE_RTF           FOURCC('R', 'T', 'F', ' ') /* Rich Text Format */
#define SCRAP_TYPE_HTML          FOURCC('H', 'T', 'M', 'L') /* HTML markup */

#define SCRAP_TYPE_PDF           FOURCC('P', 'D', 'F', ' ') /* PDF data */
#define SCRAP_TYPE_PNG           FOURCC('P', 'N', 'G', ' ') /* PNG image */
#define SCRAP_TYPE_JPEG          FOURCC('J', 'P', 'E', 'G') /* JPEG image */
#define SCRAP_TYPE_TIFF          FOURCC('T', 'I', 'F', 'F') /* TIFF image */

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

/* Prefixed Scrap Manager API, kept separate from the classic API below. */
void   Scrap_Zero(void);
Size   Scrap_Get(void* dest, ResType type);
OSErr  Scrap_Put(Size size, ResType type, const void* src);
void   Scrap_Info(short* count, short* state);
void   Scrap_Unload(void);

/* Process-aware extensions */
#include "ProcessMgr/ProcessTypes.h"  /* Get ProcessID type */
ProcessID Scrap_GetOwner(void);

/* Compatibility names for the standard text and picture scrap types. */
#define kScrapTypeTEXT FOURCC('T', 'E', 'X', 'T')
#define kScrapTypePICT FOURCC('P', 'I', 'C', 'T')

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
