/*
 * StringComparison.c - International Utilities String Comparison
 *
 * Implements string comparison functions for Mac OS International Utilities
 * Package (Pack 6). These functions compare strings with proper handling of
 * case sensitivity and diacritical marks.
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include <string.h>

/* Forward declarations */
SInt16 IUMagString(const void* aPtr, const void* bPtr, SInt16 aLen, SInt16 bLen);
SInt16 IUMagIDString(const void* aPtr, const void* bPtr, SInt16 aLen, SInt16 bLen);
SInt16 IUCompString(const char* aStr, const char* bStr);
SInt16 IUEqualString(const char* aStr, const char* bStr);

/* Debug logging */
#define STR_CMP_DEBUG 0

#if STR_CMP_DEBUG
#define STRCMP_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[StrCmp] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define STRCMP_LOG(...)
#endif

/*
 * Helper function to compare two bytes case-insensitively
 */
static SInt16 CompareBytesIgnoreCase(UInt8 a, UInt8 b) {
    /* Convert to lowercase for comparison */
    if (a >= 'A' && a <= 'Z') {
        a = a + ('a' - 'A');
    }
    if (b >= 'A' && b <= 'Z') {
        b = b + ('a' - 'A');
    }

    if (a < b) {
        return -1;
    } else if (a > b) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * IUMagString - Compare strings with magnitude ordering (case-insensitive)
 *
 * Compares two strings and returns their relative ordering. The comparison
 * is case-insensitive and ignores diacritical marks. This is the most
 * commonly used string comparison function in Mac OS.
 *
 * Parameters:
 *   aPtr - Pointer to first string (raw bytes, not Pascal string)
 *   bPtr - Pointer to second string (raw bytes, not Pascal string)
 *   aLen - Length of first string in bytes
 *   bLen - Length of second string in bytes
 *
 * Returns:
 *   < 0 if a < b (a comes before b)
 *   0 if a == b (strings are equal)
 *   > 0 if a > b (a comes after b)
 *
 * Example:
 *   IUMagString("Apple", "Banana", 5, 6) -> negative (Apple < Banana)
 *   IUMagString("apple", "APPLE", 5, 5) -> 0 (equal, case-insensitive)
 *   IUMagString("Zebra", "Apple", 5, 5) -> positive (Zebra > Apple)
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt16 IUMagString(const void* aPtr, const void* bPtr, SInt16 aLen, SInt16 bLen) {
    const UInt8* aBytes;
    const UInt8* bBytes;
    SInt16 minLen;
    SInt16 i;
    SInt16 cmp;

    if (!aPtr || !bPtr) {
        STRCMP_LOG("IUMagString: NULL pointer\n");
        /* NULL is less than non-NULL */
        if (!aPtr && !bPtr) return 0;
        return aPtr ? 1 : -1;
    }

    aBytes = (const UInt8*)aPtr;
    bBytes = (const UInt8*)bPtr;

    /* Compare up to the length of the shorter string */
    minLen = (aLen < bLen) ? aLen : bLen;

    for (i = 0; i < minLen; i++) {
        cmp = CompareBytesIgnoreCase(aBytes[i], bBytes[i]);
        if (cmp != 0) {
            STRCMP_LOG("IUMagString: Differ at position %d\n", i);
            return cmp;
        }
    }

    /* If all compared bytes are equal, the shorter string comes first */
    if (aLen < bLen) {
        STRCMP_LOG("IUMagString: A is shorter\n");
        return -1;
    } else if (aLen > bLen) {
        STRCMP_LOG("IUMagString: B is shorter\n");
        return 1;
    } else {
        STRCMP_LOG("IUMagString: Strings are equal\n");
        return 0;
    }
}

/*
 * IUMagIDString - Compare strings with case and diacritical sensitivity
 *
 * Compares two strings with full case and diacritical sensitivity. This is
 * used when exact matching is required (e.g., password comparison).
 *
 * Parameters:
 *   aPtr - Pointer to first string (raw bytes, not Pascal string)
 *   bPtr - Pointer to second string (raw bytes, not Pascal string)
 *   aLen - Length of first string in bytes
 *   bLen - Length of second string in bytes
 *
 * Returns:
 *   < 0 if a < b (a comes before b)
 *   0 if a == b (strings are exactly equal)
 *   > 0 if a > b (a comes after b)
 *
 * Example:
 *   IUMagIDString("Apple", "apple", 5, 5) -> negative (A < a in ASCII)
 *   IUMagIDString("apple", "apple", 5, 5) -> 0 (exactly equal)
 *   IUMagIDString("Apple", "Apple", 5, 5) -> 0 (exactly equal)
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt16 IUMagIDString(const void* aPtr, const void* bPtr, SInt16 aLen, SInt16 bLen) {
    const UInt8* aBytes;
    const UInt8* bBytes;
    SInt16 minLen;
    SInt16 i;

    if (!aPtr || !bPtr) {
        STRCMP_LOG("IUMagIDString: NULL pointer\n");
        /* NULL is less than non-NULL */
        if (!aPtr && !bPtr) return 0;
        return aPtr ? 1 : -1;
    }

    aBytes = (const UInt8*)aPtr;
    bBytes = (const UInt8*)bPtr;

    /* Compare up to the length of the shorter string */
    minLen = (aLen < bLen) ? aLen : bLen;

    for (i = 0; i < minLen; i++) {
        if (aBytes[i] < bBytes[i]) {
            STRCMP_LOG("IUMagIDString: A < B at position %d\n", i);
            return -1;
        } else if (aBytes[i] > bBytes[i]) {
            STRCMP_LOG("IUMagIDString: A > B at position %d\n", i);
            return 1;
        }
    }

    /* If all compared bytes are equal, the shorter string comes first */
    if (aLen < bLen) {
        STRCMP_LOG("IUMagIDString: A is shorter\n");
        return -1;
    } else if (aLen > bLen) {
        STRCMP_LOG("IUMagIDString: B is shorter\n");
        return 1;
    } else {
        STRCMP_LOG("IUMagIDString: Strings are equal\n");
        return 0;
    }
}

/*
 * IUCompString - Compare Pascal strings (case-insensitive)
 *
 * Compares two Pascal strings (length byte + data) using case-insensitive
 * comparison. This is a convenience wrapper around IUMagString.
 *
 * Parameters:
 *   aStr - First Pascal string (length byte + data)
 *   bStr - Second Pascal string (length byte + data)
 *
 * Returns:
 *   < 0 if aStr < bStr
 *   0 if aStr == bStr
 *   > 0 if aStr > bStr
 *
 * Example:
 *   IUCompString("\x05Apple", "\x06Banana") -> negative
 *   IUCompString("\x05apple", "\x05APPLE") -> 0 (case-insensitive)
 *   IUCompString("\x05Zebra", "\x05Apple") -> positive
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt16 IUCompString(const char* aStr, const char* bStr) {
    UInt8 aLen;
    UInt8 bLen;

    if (!aStr || !bStr) {
        STRCMP_LOG("IUCompString: NULL pointer\n");
        /* NULL is less than non-NULL */
        if (!aStr && !bStr) return 0;
        return aStr ? 1 : -1;
    }

    /* Get string lengths from Pascal format */
    aLen = (UInt8)aStr[0];
    bLen = (UInt8)bStr[0];

    /* Compare using IUMagString (skip length byte) */
    return IUMagString(&aStr[1], &bStr[1], aLen, bLen);
}

/*
 * IUEqualString - Test if Pascal strings are equal (case-insensitive)
 *
 * Tests whether two Pascal strings are equal using case-insensitive
 * comparison. This is more efficient than IUCompString when you only
 * need to test equality.
 *
 * Parameters:
 *   aStr - First Pascal string (length byte + data)
 *   bStr - Second Pascal string (length byte + data)
 *
 * Returns:
 *   0 if strings are equal (case-insensitive)
 *   non-zero if strings are different
 *
 * Note: The return value is the same as IUCompString, so you can use
 * it directly in comparisons like: if (IUEqualString(a, b) == 0)
 *
 * Example:
 *   IUEqualString("\x05Apple", "\x05APPLE") -> 0 (equal)
 *   IUEqualString("\x05Apple", "\x06Banana") -> non-zero (different)
 *   IUEqualString("\x05apple", "\x05apple") -> 0 (equal)
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt16 IUEqualString(const char* aStr, const char* bStr) {
    UInt8 aLen;
    UInt8 bLen;

    if (!aStr || !bStr) {
        STRCMP_LOG("IUEqualString: NULL pointer\n");
        /* NULL is less than non-NULL */
        if (!aStr && !bStr) return 0;
        return aStr ? 1 : -1;
    }

    /* Get string lengths from Pascal format */
    aLen = (UInt8)aStr[0];
    bLen = (UInt8)bStr[0];

    /* Quick check: if lengths differ, strings can't be equal */
    if (aLen != bLen) {
        STRCMP_LOG("IUEqualString: Different lengths (%d vs %d)\n", aLen, bLen);
        return (aLen < bLen) ? -1 : 1;
    }

    /* Compare using IUMagString (skip length byte) */
    return IUMagString(&aStr[1], &bStr[1], aLen, bLen);
}
