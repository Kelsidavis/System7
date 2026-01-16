/*
 * StringUtilities.c - Pascal/C String Conversion Utilities
 *
 * Implements standard Mac OS string conversion functions for converting
 * between Pascal strings (length byte prefix) and C strings (null-terminated).
 * These are commonly used throughout the Toolbox.
 *
 * Based on Inside Macintosh: Text
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include <string.h>

/* Forward declarations */
void C2PStr(char* cString);
void P2CStr(unsigned char* pString);
void CopyC2PStr(const char* cString, char* pString);
void CopyP2CStr(const char* pString, char* cString);

/* Debug logging */
#define STR_UTIL_DEBUG 0

#if STR_UTIL_DEBUG
extern void serial_puts(const char* str);
#define STRUTIL_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[StrUtil] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define STRUTIL_LOG(...)
#endif

/*
 * C2PStr - Convert C string to Pascal string in place
 *
 * Converts a null-terminated C string to a Pascal string (length byte
 * followed by characters) in place. The conversion modifies the string
 * by moving all characters forward one byte and placing the length in
 * the first byte.
 *
 * Parameters:
 *   cString - Pointer to null-terminated C string to convert
 *             Must have space for one extra byte at the beginning
 *
 * Note: The string buffer must be large enough to accommodate the length
 * byte. Maximum string length is 255 characters.
 *
 * Based on Inside Macintosh: Text
 */
void C2PStr(char* cString) {
    size_t len;

    if (!cString) {
        STRUTIL_LOG("C2PStr: NULL pointer\n");
        return;
    }

    /* Get C string length */
    len = strlen(cString);

    /* Clamp to Pascal string maximum */
    if (len > 255) {
        len = 255;
    }

    /* Move string data forward by 1 byte to make room for length */
    if (len > 0) {
        memmove(cString + 1, cString, len);
    }

    /* Store length in first byte */
    cString[0] = (char)len;

    STRUTIL_LOG("C2PStr: converted %zu chars\n", len);
}

/*
 * P2CStr - Convert Pascal string to C string in place
 *
 * Converts a Pascal string (length byte followed by characters) to a
 * null-terminated C string in place. The conversion modifies the string
 * by moving all characters back one byte and adding a null terminator.
 *
 * Parameters:
 *   pString - Pointer to Pascal string to convert
 *
 * Based on Inside Macintosh: Text
 */
void P2CStr(unsigned char* pString) {
    unsigned char len;

    if (!pString) {
        STRUTIL_LOG("P2CStr: NULL pointer\n");
        return;
    }

    /* Get Pascal string length from first byte - unsigned char already limited to 255 */
    len = pString[0];

    /* Move string data back by 1 byte */
    if (len > 0) {
        memmove(pString, pString + 1, len);
    }

    /* Add null terminator */
    pString[len] = '\0';

    STRUTIL_LOG("P2CStr: converted %u chars\n", (unsigned int)len);
}

/*
 * CopyC2PStr - Copy C string to Pascal string buffer
 *
 * Copies a null-terminated C string to a Pascal string buffer without
 * modifying the source string. The destination buffer receives a Pascal
 * string with the length byte followed by the string data.
 *
 * Parameters:
 *   cString - Source null-terminated C string
 *   pString - Destination buffer for Pascal string (must be at least 256 bytes)
 *
 * Based on Inside Macintosh: Text
 */
void CopyC2PStr(const char* cString, char* pString) {
    size_t len;

    if (!cString || !pString) {
        STRUTIL_LOG("CopyC2PStr: NULL pointer\n");
        return;
    }

    /* Get C string length */
    len = strlen(cString);

    /* Clamp to Pascal string maximum */
    if (len > 255) {
        len = 255;
    }

    /* Store length in first byte */
    pString[0] = (char)len;

    /* Copy string data */
    if (len > 0) {
        memcpy(pString + 1, cString, len);
    }

    STRUTIL_LOG("CopyC2PStr: copied %zu chars\n", len);
}

/*
 * CopyP2CStr - Copy Pascal string to C string buffer
 *
 * Copies a Pascal string to a null-terminated C string buffer without
 * modifying the source string. The destination buffer receives the
 * string data followed by a null terminator.
 *
 * Parameters:
 *   pString - Source Pascal string (length byte + data)
 *   cString - Destination buffer for C string (must be at least 256 bytes)
 *
 * Based on Inside Macintosh: Text
 */
void CopyP2CStr(const char* pString, char* cString) {
    unsigned char len;

    if (!pString || !cString) {
        STRUTIL_LOG("CopyP2CStr: NULL pointer\n");
        return;
    }

    /* Get Pascal string length from first byte - unsigned char already limited to 255 */
    len = (unsigned char)pString[0];

    /* Copy string data */
    if (len > 0) {
        memcpy(cString, pString + 1, len);
    }

    /* Add null terminator */
    cString[len] = '\0';

    STRUTIL_LOG("CopyP2CStr: copied %u chars\n", (unsigned int)len);
}
