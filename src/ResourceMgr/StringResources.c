/*
 * StringResources.c - String Resource Utilities
 *
 * Implements Resource Manager functions for retrieving string resources.
 * These are commonly used throughout the Toolbox and applications.
 *
 * Based on Inside Macintosh: More Macintosh Toolbox
 */

#include "SystemTypes.h"
#include "ResourceManager.h"
#include "System71StdLib.h"
#include <string.h>

/* Debug logging */
#define STR_RES_DEBUG 0

#if STR_RES_DEBUG
extern void serial_puts(const char* str);
#define STRRES_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[StrRes] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define STRRES_LOG(...)
#endif

/* Resource types */
#define kSTRResourceType  0x53545220  /* 'STR ' - String resource */
#define kSTR_ResourceType 0x53545223  /* 'STR#' - String list resource */

/*
 * GetString - Get a string resource
 *
 * Retrieves a single string from a 'STR ' resource. The string is returned
 * as a Pascal string (length byte followed by characters).
 *
 * Parameters:
 *   theString - Buffer to receive the string (must be at least 256 bytes)
 *   stringID - Resource ID of the 'STR ' resource
 *
 * If the resource is not found, theString[0] is set to 0 (empty string).
 *
 * Based on Inside Macintosh: More Macintosh Toolbox, Chapter 7
 */
void GetString(StringPtr theString, SInt16 stringID) {
    Handle strHandle;
    Ptr strData;
    UInt8 len;

    if (!theString) {
        STRRES_LOG("GetString: NULL string buffer\n");
        return;
    }

    /* Initialize to empty string */
    theString[0] = 0;

    /* Try to load the 'STR ' resource */
    strHandle = GetResource(kSTRResourceType, stringID);
    if (!strHandle) {
        STRRES_LOG("GetString: STR resource %d not found\n", stringID);
        return;
    }

    /* Load the resource if needed */
    LoadResource(strHandle);

    /* Get pointer to resource data */
    strData = *strHandle;
    if (!strData) {
        STRRES_LOG("GetString: STR resource %d has no data\n", stringID);
        return;
    }

    /* First byte is the length - UInt8 already limited to 255 max */
    len = (UInt8)strData[0];

    theString[0] = len;
    if (len > 0) {
        memcpy(&theString[1], &strData[1], len);
    }

    STRRES_LOG("GetString: Loaded STR resource %d (length=%d)\n", stringID, len);
}

/*
 * GetIndString - Get a string from a string list resource
 *
 * Retrieves a string from a 'STR#' (string list) resource. String lists
 * contain multiple strings indexed starting from 1.
 *
 * Format of 'STR#' resource:
 *   [2 bytes] - Number of strings
 *   [n bytes] - String 1 (Pascal string)
 *   [n bytes] - String 2 (Pascal string)
 *   ...
 *
 * Parameters:
 *   theString - Buffer to receive the string (must be at least 256 bytes)
 *   strListID - Resource ID of the 'STR#' resource
 *   index - Index of string to retrieve (1-based)
 *
 * If the resource is not found or index is out of range,
 * theString[0] is set to 0 (empty string).
 *
 * Based on Inside Macintosh: More Macintosh Toolbox, Chapter 7
 */
void GetIndString(StringPtr theString, SInt16 strListID, SInt16 index) {
    Handle strListHandle;
    Ptr strListData;
    UInt16 numStrings;
    UInt16 i;
    UInt8 len;
    Ptr currentPtr;

    if (!theString) {
        STRRES_LOG("GetIndString: NULL string buffer\n");
        return;
    }

    /* Initialize to empty string */
    theString[0] = 0;

    /* Validate index */
    if (index < 1) {
        STRRES_LOG("GetIndString: Invalid index %d (must be >= 1)\n", index);
        return;
    }

    /* Try to load the 'STR#' resource */
    strListHandle = GetResource(kSTR_ResourceType, strListID);
    if (!strListHandle) {
        STRRES_LOG("GetIndString: STR# resource %d not found\n", strListID);
        return;
    }

    /* Load the resource if needed */
    LoadResource(strListHandle);

    /* Get pointer to resource data */
    strListData = *strListHandle;
    if (!strListData) {
        STRRES_LOG("GetIndString: STR# resource %d has no data\n", strListID);
        return;
    }

    /* First 2 bytes are the number of strings (big-endian) */
    numStrings = ((UInt8)strListData[0] << 8) | (UInt8)strListData[1];

    if (index > numStrings) {
        STRRES_LOG("GetIndString: Index %d out of range (count=%d)\n", index, numStrings);
        return;
    }

    /* Skip past the count */
    currentPtr = strListData + 2;

    /* Iterate through strings to find the requested one */
    for (i = 1; i <= numStrings; i++) {
        len = (UInt8)*currentPtr;

        if (i == index) {
            /* Found the requested string - UInt8 len already limited to 255 max */
            theString[0] = len;
            if (len > 0) {
                memcpy(&theString[1], currentPtr + 1, len);
            }

            STRRES_LOG("GetIndString: Loaded string %d from STR# %d (length=%d)\n",
                       index, strListID, len);
            return;
        }

        /* Skip to next string (length byte + string data) */
        currentPtr += len + 1;
    }

    /* Should not reach here if index is valid */
    STRRES_LOG("GetIndString: Failed to find string %d in STR# %d\n", index, strListID);
}
