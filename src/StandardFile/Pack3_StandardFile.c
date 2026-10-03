/*
 * Pack3_StandardFile.c - Standard File Package (Pack3)
 *
 * Implements Pack3, the Standard File Package for Mac OS System 7.
 * This package provides standard file dialogs for opening and saving files,
 * including file filtering and type selection.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 * and Inside Macintosh: Files, Chapter 3
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations for Standard File functions */
extern void SFGetFile(Point where,
                      const unsigned char* prompt,
                      void* fileFilter,
                      SInt16 numTypes,
                      const OSType* typeList,
                      void* dlgHook,
                      void* reply);

extern void SFPutFile(Point where,
                      const unsigned char* prompt,
                      const unsigned char* origName,
                      void* dlgHook,
                      void* reply);

/* Forward declaration for Pack3 dispatcher */
OSErr Pack3_Dispatch(short selector, void* params);

/* Debug logging */
#define PACK3_DEBUG 0

#if PACK3_DEBUG
#define PACK3_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[Pack3] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define PACK3_LOG(...)
#endif

/* Pack3 selectors */
#define kPack3SFGetFile  1  /* Open file dialog */
#define kPack3SFPutFile  2  /* Save file dialog */

/* Parameter block for SFGetFile */
typedef struct {
    Point                where;       /* Input: Top-left corner of dialog */
    const unsigned char* prompt;      /* Input: Prompt string (Pascal) */
    void*               fileFilter;   /* Input: File filter proc (NULL = none) */
    SInt16              numTypes;     /* Input: Number of file types (or -1 for all) */
    const OSType*       typeList;     /* Input: Array of file types */
    void*               dlgHook;      /* Input: Dialog hook proc (NULL = none) */
    void*               reply;        /* Output: StandardFileReply structure */
} SFGetFileParams;

/* Parameter block for SFPutFile */
typedef struct {
    Point                where;       /* Input: Top-left corner of dialog */
    const unsigned char* prompt;      /* Input: Prompt string (Pascal) */
    const unsigned char* origName;    /* Input: Default filename (Pascal) */
    void*               dlgHook;      /* Input: Dialog hook proc (NULL = none) */
    void*               reply;        /* Output: StandardFileReply structure */
} SFPutFileParams;

/*
 * Pack3_SFGetFile - Open file dialog handler
 *
 * Handles the Pack3 SFGetFile selector. Displays a standard file open dialog
 * allowing the user to select a file. Supports file type filtering.
 *
 * Parameters:
 *   params - Pointer to SFGetFileParams structure
 *
 * Returns:
 *   noErr if successful
 *   paramErr if parameters are invalid
 *
 * The reply structure is filled with:
 *   - good: true if user clicked Open, false if Cancel
 *   - fType: File type of selected file
 *   - vRefNum: Volume reference number
 *   - fName: Filename (Pascal string)
 */
static OSErr Pack3_SFGetFile(SFGetFileParams* params) {
    if (!params) {
        PACK3_LOG("SFGetFile: NULL params\n");
        return paramErr;
    }

    if (!params->reply) {
        PACK3_LOG("SFGetFile: NULL reply structure\n");
        return paramErr;
    }

    PACK3_LOG("SFGetFile: where=(%d,%d), numTypes=%d\n",
              params->where.v, params->where.h, params->numTypes);

    /* Call the actual Standard File function */
    SFGetFile(params->where,
              params->prompt,
              params->fileFilter,
              params->numTypes,
              params->typeList,
              params->dlgHook,
              params->reply);

    return noErr;
}

/*
 * Pack3_SFPutFile - Save file dialog handler
 *
 * Handles the Pack3 SFPutFile selector. Displays a standard file save dialog
 * allowing the user to specify a filename and location for saving.
 *
 * Parameters:
 *   params - Pointer to SFPutFileParams structure
 *
 * Returns:
 *   noErr if successful
 *   paramErr if parameters are invalid
 *
 * The reply structure is filled with:
 *   - good: true if user clicked Save, false if Cancel
 *   - fType: File type (default 'TEXT')
 *   - vRefNum: Volume reference number
 *   - fName: Filename entered by user (Pascal string)
 */
static OSErr Pack3_SFPutFile(SFPutFileParams* params) {
    if (!params) {
        PACK3_LOG("SFPutFile: NULL params\n");
        return paramErr;
    }

    if (!params->reply) {
        PACK3_LOG("SFPutFile: NULL reply structure\n");
        return paramErr;
    }

    PACK3_LOG("SFPutFile: where=(%d,%d)\n",
              params->where.v, params->where.h);

    /* Call the actual Standard File function */
    SFPutFile(params->where,
              params->prompt,
              params->origName,
              params->dlgHook,
              params->reply);

    return noErr;
}

/*
 * Pack3_Dispatch - Pack3 package dispatcher
 *
 * Main dispatcher for the Standard File Package (Pack3).
 * Routes selector calls to the appropriate file dialog function.
 *
 * Parameters:
 *   selector - Function selector:
 *              1 = SFGetFile (open file dialog)
 *              2 = SFPutFile (save file dialog)
 *   params - Pointer to selector-specific parameter block
 *
 * Returns:
 *   noErr if successful
 *   paramErr if selector is invalid or params are NULL
 *
 * Example usage through Package Manager:
 *   SFGetFileParams params;
 *   StandardFileReply reply;
 *   params.where.h = 100;
 *   params.where.v = 100;
 *   params.prompt = "\pSelect a file:";
 *   params.fileFilter = NULL;
 *   params.numTypes = -1;  // All file types
 *   params.typeList = NULL;
 *   params.dlgHook = NULL;
 *   params.reply = &reply;
 *   CallPackage(3, 1, &params);  // Call Pack3, SFGetFile selector
 *   if (reply.good) {
 *       // User selected a file, use reply.fName, reply.vRefNum, etc.
 *   }
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 * and Inside Macintosh: Files, Chapter 3
 */
OSErr Pack3_Dispatch(short selector, void* params) {
    PACK3_LOG("Dispatch: selector=%d, params=%p\n", selector, params);

    if (!params) {
        PACK3_LOG("Dispatch: NULL params\n");
        return paramErr;
    }

    switch (selector) {
        case kPack3SFGetFile:
            PACK3_LOG("Dispatch: SFGetFile\n");
            return Pack3_SFGetFile((SFGetFileParams*)params);

        case kPack3SFPutFile:
            PACK3_LOG("Dispatch: SFPutFile\n");
            return Pack3_SFPutFile((SFPutFileParams*)params);

        default:
            PACK3_LOG("Dispatch: Invalid selector %d\n", selector);
            return paramErr;
    }
}
