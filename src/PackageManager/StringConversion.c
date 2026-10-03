/*
 * StringConversion.c - String Package Number Conversion Utilities
 *
 * Implements System 7 string conversion functions for converting between
 * numbers and strings. These are commonly used utilities throughout the OS.
 *
 * Based on Inside Macintosh: Text
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include <string.h>

/* Forward declarations to avoid including full StringPackage.h */
void StringToNum(const char *theString, SInt32 *theNum);
void NumToString(SInt32 theNum, char *theString);

/* Debug logging */
#define STR_CONV_DEBUG 0

#if STR_CONV_DEBUG
#define STRCONV_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[StrConv] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define STRCONV_LOG(...)
#endif

/*
 * NumToString - Convert a signed integer to a decimal string
 *
 * Converts a 32-bit signed integer to its decimal string representation.
 * The resulting string is NOT null-terminated (Pascal-style string).
 * The first byte contains the length, followed by the digits.
 *
 * Parameters:
 *   theNum - The number to convert
 *   theString - Buffer to receive the string (must be at least 12 bytes)
 *               Format: [length byte][digits]
 *
 * Example:
 *   NumToString(1234, str) produces: str[0]=4, str[1]='1', str[2]='2',
 *                                     str[3]='3', str[4]='4'
 *
 * Based on Inside Macintosh: Text
 */
void NumToString(SInt32 theNum, char *theString) {
    char temp[32];
    int len;
    int i;
    Boolean negative = false;
    UInt32 absNum;

    if (!theString) {
        STRCONV_LOG("NumToString: NULL string buffer\n");
        return;
    }

    /* Handle negative numbers */
    if (theNum < 0) {
        negative = true;
        /* Handle INT_MIN specially to avoid overflow */
        if (theNum == (SInt32)0x80000000) {
            absNum = 0x80000000;
        } else {
            absNum = (UInt32)(-theNum);
        }
    } else {
        absNum = (UInt32)theNum;
    }

    /* Convert digits in reverse order */
    len = 0;
    do {
        temp[len++] = '0' + (absNum % 10);
        absNum /= 10;
    } while (absNum > 0);

    /* Add minus sign if negative */
    if (negative) {
        temp[len++] = '-';
    }

    /* Store length in first byte (Pascal string) */
    theString[0] = (char)len;

    /* Reverse digits into output string */
    for (i = 0; i < len; i++) {
        theString[i + 1] = temp[len - 1 - i];
    }

    STRCONV_LOG("NumToString: %ld -> '%.*s'\n", (long)theNum, len, &theString[1]);
}

/*
 * StringToNum - Convert a decimal string to a signed integer
 *
 * Parses a string containing a decimal number and converts it to a
 * 32-bit signed integer. Handles both Pascal strings (length byte first)
 * and C strings (null-terminated).
 *
 * Parameters:
 *   theString - The string to parse (Pascal or C string)
 *   theNum - Pointer to receive the converted number
 *
 * The function parses:
 * - Optional leading whitespace
 * - Optional + or - sign
 * - One or more decimal digits
 *
 * Parsing stops at the first non-digit character.
 * If no valid number is found, *theNum is set to 0.
 *
 * Based on Inside Macintosh: Text
 */
void StringToNum(const char *theString, SInt32 *theNum) {
    SInt32 result = 0;
    Boolean negative = false;
    Boolean foundDigits = false;
    int i = 0;
    int len;
    const char *str;

    if (!theString || !theNum) {
        STRCONV_LOG("StringToNum: NULL parameter\n");
        if (theNum) *theNum = 0;
        return;
    }

    /* Check if it's a Pascal string (first byte is length) */
    if ((unsigned char)theString[0] <= 127) {
        /* Assume Pascal string if first byte looks like a reasonable length */
        len = (unsigned char)theString[0];
        str = theString + 1;
    } else {
        /* C string - find length */
        len = strlen(theString);
        str = theString;
    }

    /* Skip leading whitespace */
    while (i < len && (str[i] == ' ' || str[i] == '\t')) {
        i++;
    }

    /* Check for sign */
    if (i < len) {
        if (str[i] == '-') {
            negative = true;
            i++;
        } else if (str[i] == '+') {
            i++;
        }
    }

    /* Parse digits */
    while (i < len && str[i] >= '0' && str[i] <= '9') {
        int digit = str[i] - '0';

        /* Check for overflow (simple check) */
        if (result > (0x7FFFFFFF / 10)) {
            /* Would overflow - clamp to max/min */
            result = negative ? (SInt32)0x80000000 : 0x7FFFFFFF;
            STRCONV_LOG("StringToNum: Overflow detected\n");
            *theNum = result;
            return;
        }

        result = result * 10 + digit;
        foundDigits = true;
        i++;
    }

    /* Apply sign */
    if (negative && result != 0) {
        result = -result;
    }

    /* If no digits found, result is 0 */
    if (!foundDigits) {
        result = 0;
        STRCONV_LOG("StringToNum: No digits found in string\n");
    }

    *theNum = result;

    STRCONV_LOG("StringToNum: '%.*s' -> %ld\n", len, str, (long)result);
}
