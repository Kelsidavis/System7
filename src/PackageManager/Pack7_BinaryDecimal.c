/*
 * Pack7_BinaryDecimal.c - Binary/Decimal Conversion Package (Pack7)
 *
 * Implements Pack7, the Binary/Decimal Conversion Package for Mac OS System 7.
 * This package provides string-to-number and number-to-string conversion
 * utilities that are accessed through the Package Manager dispatcher.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "PackageManager/BinaryDecimal.h"
#include "PackageManager/PackageDispatchInternal.h"

/* Debug logging */
#define PACK7_DEBUG 0

#if PACK7_DEBUG
#define PACK7_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[Pack7] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define PACK7_LOG(...)
#endif

/* Pack7 selectors */
#define kPack7NumToString  0  /* Convert number to string */
#define kPack7StringToNum  1  /* Convert string to number */

/* Parameter block for NumToString */
typedef struct {
    SInt32  theNum;      /* Input: number to convert */
    char*   theString;   /* Output: Pascal string buffer (12 bytes minimum) */
} NumToStringParams;

/* Parameter block for StringToNum */
typedef struct {
    const char* theString;  /* Input: Pascal or C string */
    SInt32*     theNum;     /* Output: converted number */
} StringToNumParams;

/*
 * Pack7_NumToString - Convert number to string handler
 *
 * Handles the Pack7 NumToString selector. Converts a 32-bit signed integer
 * to a Pascal string representation.
 *
 * Parameters:
 *   params - Pointer to NumToStringParams structure
 *
 * Returns:
 *   noErr if successful
 *   paramErr if parameters are invalid
 */
static OSErr Pack7_NumToString(NumToStringParams* params) {
    if (!params) {
        PACK7_LOG("NumToString: NULL params\n");
        return paramErr;
    }

    if (!params->theString) {
        PACK7_LOG("NumToString: NULL string buffer\n");
        return paramErr;
    }

    PACK7_LOG("NumToString: Converting %ld\n", (long)params->theNum);

    /* Call the actual conversion function */
    NumToString(params->theNum, params->theString);

    return noErr;
}

/*
 * Pack7_StringToNum - Convert string to number handler
 *
 * Handles the Pack7 StringToNum selector. Parses a Pascal or C string
 * and converts it to a 32-bit signed integer.
 *
 * Parameters:
 *   params - Pointer to StringToNumParams structure
 *
 * Returns:
 *   noErr if successful
 *   paramErr if parameters are invalid
 */
static OSErr Pack7_StringToNum(StringToNumParams* params) {
    if (!params) {
        PACK7_LOG("StringToNum: NULL params\n");
        return paramErr;
    }

    if (!params->theString || !params->theNum) {
        PACK7_LOG("StringToNum: NULL string or number pointer\n");
        return paramErr;
    }

    PACK7_LOG("StringToNum: Converting string\n");

    /* Call the actual conversion function */
    StringToNum(params->theString, params->theNum);

    return noErr;
}

/*
 * Pack7_Dispatch - Pack7 package dispatcher
 *
 * Main dispatcher for the Binary/Decimal Conversion Package (Pack7).
 * Routes selector calls to the appropriate conversion function.
 *
 * Parameters:
 *   selector - Function selector:
 *              0 = NumToString (convert number to string)
 *              1 = StringToNum (convert string to number)
 *   params - Pointer to selector-specific parameter block
 *
 * Returns:
 *   noErr if successful
 *   paramErr if selector is invalid or params are NULL
 *
 * Example usage through Package Manager:
 *   NumToStringParams params;
 *   params.theNum = 1234;
 *   params.theString = buffer;
 *   CallPackage(7, 0, &params);  // Call Pack7, NumToString selector
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */
OSErr Pack7_Dispatch(short selector, void* params) {
    PACK7_LOG("Dispatch: selector=%d, params=%p\n", selector, params);

    if (!params) {
        PACK7_LOG("Dispatch: NULL params\n");
        return paramErr;
    }

    switch (selector) {
        case kPack7NumToString:
            PACK7_LOG("Dispatch: NumToString\n");
            return Pack7_NumToString((NumToStringParams*)params);

        case kPack7StringToNum:
            PACK7_LOG("Dispatch: StringToNum\n");
            return Pack7_StringToNum((StringToNumParams*)params);

        default:
            PACK7_LOG("Dispatch: Invalid selector %d\n", selector);
            return paramErr;
    }
}
