/*
 * StringManipulation.c - String Manipulation Utilities
 *
 * Implements string manipulation functions for Mac OS String Package.
 * These utilities provide common string operations like copying, concatenation,
 * searching, replacement, and trimming. They work with Pascal strings
 * (length-byte prefix format).
 *
 * Based on Inside Macintosh: Text
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include <string.h>

/* Forward declarations */
void CopyString(const char* source, char* dest, SInt16 maxLen);
void ConcatString(const char* source, char* dest, SInt16 maxLen);
SInt16 FindString(const char* searchIn, const char* searchFor, SInt16 startPos);
void ReplaceString(char* theString, const char* oldStr, const char* newStr);
void TrimString(char* theString);

/* Debug logging */
#define STR_MANIP_DEBUG 0

#if STR_MANIP_DEBUG
#define STRMANIP_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[StrManip] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define STRMANIP_LOG(...)
#endif

/*
 * CopyString - Copy a Pascal string with length limit
 *
 * Copies a Pascal string from source to destination, respecting the maximum
 * length constraint. If the source string is longer than maxLen, it will be
 * truncated. This is safer than a direct memory copy.
 *
 * Parameters:
 *   source - Source Pascal string (length byte + data)
 *   dest - Destination buffer for Pascal string
 *   maxLen - Maximum length of destination (not including length byte)
 *
 * Note: The destination buffer must be at least maxLen + 1 bytes.
 *
 * Example:
 *   source = "\x05Hello" (Pascal string "Hello")
 *   CopyString(source, dest, 255) -> dest = "\x05Hello"
 *   CopyString(source, dest, 3) -> dest = "\x03Hel" (truncated)
 *
 * Based on Inside Macintosh: Text
 */
void CopyString(const char* source, char* dest, SInt16 maxLen) {
    UInt8 srcLen;
    UInt8 copyLen;

    if (!source || !dest) {
        STRMANIP_LOG("CopyString: NULL pointer\n");
        return;
    }

    if (maxLen < 0) {
        maxLen = 0;
    }

    /* Get source string length */
    srcLen = (UInt8)source[0];

    /* Determine how much to copy */
    copyLen = (srcLen > maxLen) ? maxLen : srcLen;

    /* Set destination length */
    dest[0] = copyLen;

    /* Copy string data */
    if (copyLen > 0) {
        memcpy(&dest[1], &source[1], copyLen);
    }

    STRMANIP_LOG("CopyString: Copied %d bytes (src len=%d, maxLen=%d)\n",
                 copyLen, srcLen, maxLen);
}

/*
 * ConcatString - Concatenate two Pascal strings with length limit
 *
 * Appends the source string to the destination string, respecting the
 * maximum length constraint. If the combined length would exceed maxLen,
 * the source string is truncated.
 *
 * Parameters:
 *   source - Source Pascal string to append
 *   dest - Destination Pascal string (will be modified)
 *   maxLen - Maximum length of destination (not including length byte)
 *
 * Example:
 *   dest = "\x05Hello", source = "\x06 World"
 *   ConcatString(source, dest, 255) -> dest = "\x0BHello World"
 *   ConcatString(source, dest, 7) -> dest = "\x07Hello W" (truncated)
 *
 * Based on Inside Macintosh: Text
 */
void ConcatString(const char* source, char* dest, SInt16 maxLen) {
    UInt8 destLen;
    UInt8 srcLen;
    UInt8 copyLen;
    UInt8 newLen;

    if (!source || !dest) {
        STRMANIP_LOG("ConcatString: NULL pointer\n");
        return;
    }

    if (maxLen < 0) {
        maxLen = 0;
    }

    /* Get current lengths */
    destLen = (UInt8)dest[0];
    srcLen = (UInt8)source[0];

    /* Calculate how much we can copy */
    if (destLen >= maxLen) {
        /* Destination is already at max length */
        STRMANIP_LOG("ConcatString: Dest already at max length\n");
        return;
    }

    copyLen = maxLen - destLen;
    if (copyLen > srcLen) {
        copyLen = srcLen;
    }

    /* Append source data to destination */
    if (copyLen > 0) {
        memcpy(&dest[1 + destLen], &source[1], copyLen);
    }

    /* Update destination length */
    newLen = destLen + copyLen;
    dest[0] = newLen;

    STRMANIP_LOG("ConcatString: Appended %d bytes (dest %d -> %d, maxLen=%d)\n",
                 copyLen, destLen, newLen, maxLen);
}

/*
 * FindString - Find substring in a Pascal string
 *
 * Searches for the first occurrence of searchFor within searchIn, starting
 * at the specified position. Returns the position (1-based) of the match,
 * or 0 if not found.
 *
 * Parameters:
 *   searchIn - Pascal string to search within
 *   searchFor - Pascal string to search for
 *   startPos - Starting position (1-based, 0 or 1 = start from beginning)
 *
 * Returns:
 *   Position of first match (1-based), or 0 if not found
 *
 * Example:
 *   searchIn = "\x0BHello World", searchFor = "\x05World"
 *   FindString(searchIn, searchFor, 0) -> 7 (position of "World")
 *   FindString(searchIn, searchFor, 8) -> 0 (not found after position 8)
 *
 * Based on Inside Macintosh: Text
 */
SInt16 FindString(const char* searchIn, const char* searchFor, SInt16 startPos) {
    UInt8 inLen;
    UInt8 forLen;
    SInt16 i;
    SInt16 j;
    Boolean match;

    if (!searchIn || !searchFor) {
        STRMANIP_LOG("FindString: NULL pointer\n");
        return 0;
    }

    inLen = (UInt8)searchIn[0];
    forLen = (UInt8)searchFor[0];

    /* Empty search string always matches at position 1 */
    if (forLen == 0) {
        return (startPos <= 0) ? 1 : startPos;
    }

    /* Check if search string is longer than input - no match possible */
    if (forLen > inLen) {
        STRMANIP_LOG("FindString: Search string longer than input\n");
        return 0;
    }

    /* Adjust startPos to 1-based if needed */
    if (startPos <= 0) {
        startPos = 1;
    }

    /* Convert to 0-based for internal logic */
    startPos--;

    /* Search for substring - use < to avoid off-by-one read */
    for (i = startPos; i <= (SInt16)(inLen - forLen); i++) {
        match = true;

        /* Check if substring matches at position i */
        for (j = 0; j < forLen; j++) {
            if (searchIn[1 + i + j] != searchFor[1 + j]) {
                match = false;
                break;
            }
        }

        if (match) {
            /* Return 1-based position */
            SInt16 foundPos = i + 1;
            STRMANIP_LOG("FindString: Found at position %d\n", foundPos);
            return foundPos;
        }
    }

    STRMANIP_LOG("FindString: Not found\n");
    return 0;
}

/*
 * ReplaceString - Replace substring in a Pascal string
 *
 * Replaces the first occurrence of oldStr with newStr in theString.
 * The string is modified in place. If the replacement would make the
 * string longer than 255 characters, it is truncated.
 *
 * Parameters:
 *   theString - Pascal string to modify (must have space for 256 bytes)
 *   oldStr - Pascal string to search for
 *   newStr - Pascal string to replace with
 *
 * Note: Only replaces the first occurrence. To replace all occurrences,
 * call this function repeatedly until FindString returns 0.
 *
 * Example:
 *   theString = "\x0BHello World"
 *   ReplaceString(theString, "\x05World", "\x04Mars")
 *   Result: theString = "\x0AHello Mars"
 *
 * Based on Inside Macintosh: Text
 */
void ReplaceString(char* theString, const char* oldStr, const char* newStr) {
    UInt8 strLen;
    UInt8 oldLen;
    UInt8 newLen;
    SInt16 pos;
    SInt16 afterOldPos;
    UInt8 afterOldLen;
    SInt16 finalLen;
    char tempBuf[256];

    if (!theString || !oldStr || !newStr) {
        STRMANIP_LOG("ReplaceString: NULL pointer\n");
        return;
    }

    /* Find the old string */
    pos = FindString(theString, oldStr, 0);
    if (pos == 0) {
        /* Not found, nothing to replace */
        STRMANIP_LOG("ReplaceString: Old string not found\n");
        return;
    }

    strLen = (UInt8)theString[0];
    oldLen = (UInt8)oldStr[0];
    newLen = (UInt8)newStr[0];

    /* Calculate position after old string (1-based) */
    afterOldPos = pos + oldLen;
    afterOldLen = (afterOldPos <= strLen) ? (strLen - afterOldPos + 1) : 0;

    /* Build new string in temporary buffer */
    /* Copy part before old string */
    tempBuf[0] = 0;  /* Will update length at end */
    if (pos > 1) {
        memcpy(&tempBuf[1], &theString[1], pos - 1);
    }

    /* Copy new string */
    if (newLen > 0) {
        memcpy(&tempBuf[pos], &newStr[1], newLen);
    }

    /* Copy part after old string */
    if (afterOldLen > 0) {
        memcpy(&tempBuf[pos + newLen], &theString[afterOldPos], afterOldLen);
    }

    /* Calculate final length (may be truncated) */
    finalLen = (pos - 1) + newLen + afterOldLen;
    if (finalLen > 255) {
        finalLen = 255;
    }

    tempBuf[0] = finalLen;

    /* Copy result back to original string */
    memcpy(theString, tempBuf, finalLen + 1);

    STRMANIP_LOG("ReplaceString: Replaced at position %d (len %d -> %d)\n",
                 pos, strLen, finalLen);
}

/*
 * TrimString - Remove leading and trailing whitespace from Pascal string
 *
 * Removes spaces, tabs, and control characters from the beginning and end
 * of a Pascal string. The string is modified in place.
 *
 * Parameters:
 *   theString - Pascal string to trim (modified in place)
 *
 * Example:
 *   theString = "\x0D  Hello World  "
 *   TrimString(theString)
 *   Result: theString = "\x0BHello World"
 *
 * Based on Inside Macintosh: Text
 */
void TrimString(char* theString) {
    UInt8 len;
    SInt16 start;
    SInt16 end;
    UInt8 newLen;
    char ch;

    if (!theString) {
        STRMANIP_LOG("TrimString: NULL pointer\n");
        return;
    }

    len = (UInt8)theString[0];

    if (len == 0) {
        return;
    }

    /* Find first non-whitespace character (1-based) */
    start = 1;
    while (start <= len) {
        ch = theString[start];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' && ch > 0x20) {
            break;
        }
        start++;
    }

    /* If entire string is whitespace */
    if (start > len) {
        theString[0] = 0;
        STRMANIP_LOG("TrimString: Entire string was whitespace\n");
        return;
    }

    /* Find last non-whitespace character (1-based) */
    end = len;
    while (end >= start) {
        ch = theString[end];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' && ch > 0x20) {
            break;
        }
        end--;
    }

    /* Calculate new length */
    newLen = end - start + 1;

    /* Move trimmed string to beginning if necessary */
    if (start > 1 && newLen > 0) {
        memmove(&theString[1], &theString[start], newLen);
    }

    /* Update length */
    theString[0] = newLen;

    STRMANIP_LOG("TrimString: Trimmed %d -> %d (removed %d from start, %d from end)\n",
                 len, newLen, start - 1, len - end);
}
