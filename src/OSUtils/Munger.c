/*
 * Munger.c - Memory Munging Utility
 *
 * Implements the Munger function, a powerful utility for searching and
 * replacing data in handles. Used throughout the Toolbox for text editing,
 * resource manipulation, and data processing.
 *
 * Based on Inside Macintosh: Memory, Chapter 2
 */

#include "OSUtils/OSUtils.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include <string.h>

/* Debug logging */
#define MUNGER_DEBUG 0

#if MUNGER_DEBUG
#define MUNGER_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[Munger] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define MUNGER_LOG(...)
#endif

/*
 * Munger - Memory munging (search and replace) in a handle
 *
 * This is one of the most versatile Toolbox utilities. It can:
 * - Search for a pattern in a handle
 * - Replace found pattern with different data
 * - Insert data at a position (when ptr1 is NULL)
 * - Delete data (when ptr2 is NULL)
 * - Resize the handle automatically
 *
 * Parameters:
 *   h - Handle to search/modify
 *   offset - Starting offset for search (must be >= 0)
 *   ptr1 - Pointer to search pattern (NULL = skip search, just insert)
 *   len1 - Length of search pattern (0 = skip search)
 *   ptr2 - Pointer to replacement data (NULL = delete found pattern)
 *   len2 - Length of replacement data (0 = delete)
 *
 * Returns:
 *   If searching (ptr1 != NULL, len1 > 0):
 *     - Offset where match was found (>= 0)
 *     - -1 if no match found
 *   If not searching (ptr1 == NULL or len1 == 0):
 *     - Returns offset where data was inserted
 *
 * Examples:
 *   Munger(h, 0, "old", 3, "new", 3) - Replace "old" with "new"
 *   Munger(h, 0, "bad", 3, NULL, 0) - Delete "bad"
 *   Munger(h, 10, NULL, 0, "data", 4) - Insert "data" at offset 10
 *
 * Based on Inside Macintosh: Memory
 */
SInt32 Munger(Handle h, SInt32 offset, const void* ptr1, SInt32 len1,
              const void* ptr2, SInt32 len2) {
    Ptr handleData;
    Size handleSize;
    SInt32 foundOffset = -1;
    SInt32 i;
    SInt32 searchLimit;
    Boolean searching;

    /* Validate handle */
    if (!h) {
        MUNGER_LOG("Munger: NULL handle\n");
        return -1;
    }

    handleSize = GetHandleSize(h);
    handleData = *h;

    if (!handleData) {
        MUNGER_LOG("Munger: Handle has no data\n");
        return -1;
    }

    /* Validate offset */
    if (offset < 0) {
        MUNGER_LOG("Munger: Negative offset %ld\n", (long)offset);
        return -1;
    }

    if (offset > (SInt32)handleSize) {
        offset = handleSize;
    }

    /* Determine if we're searching */
    searching = (ptr1 != NULL && len1 > 0);

    if (searching) {
        /* Search for pattern */
        /* searchLimit = last valid start position for pattern of length len1 */
        searchLimit = handleSize - len1;

        for (i = offset; i <= searchLimit; i++) {
            if (memcmp(handleData + i, ptr1, len1) == 0) {
                foundOffset = i;
                MUNGER_LOG("Munger: Found pattern at offset %ld\n", (long)foundOffset);
                break;
            }
        }

        if (foundOffset < 0) {
            /* Pattern not found */
            MUNGER_LOG("Munger: Pattern not found\n");
            return -1;
        }

        offset = foundOffset;
    }

    /* Calculate size change */
    SInt32 oldLen = searching ? len1 : 0;
    SInt32 newLen = (ptr2 != NULL) ? len2 : 0;
    SInt32 deltaSize = newLen - oldLen;

    if (deltaSize == 0 && newLen > 0) {
        /* Simple replacement - same size */
        memcpy(handleData + offset, ptr2, len2);
        MUNGER_LOG("Munger: Replaced %ld bytes at offset %ld\n",
                   (long)len2, (long)offset);
        return offset;
    }

    if (deltaSize != 0) {
        /* Need to resize handle */
        Size newSize = handleSize + deltaSize;

        if (newSize < 0) {
            /* Would make handle negative size */
            MUNGER_LOG("Munger: Invalid size change\n");
            return -1;
        }

        /* Resize the handle */
        if (!SetHandleSize(h, newSize)) {
            MUNGER_LOG("Munger: Failed to resize handle (old=%lu, new=%lu)\n",
                       (unsigned long)handleSize, (unsigned long)newSize);
            return -1;
        }

        /* Get new data pointer (may have moved during resize) */
        handleData = *h;
        if (!handleData) {
            MUNGER_LOG("Munger: Handle data is NULL after resize\n");
            return -1;
        }

        if (deltaSize > 0) {
            /* Growing - move tail data forward */
            SInt32 tailOffset = offset + oldLen;
            SInt32 tailSize = handleSize - tailOffset;

            if (tailSize > 0) {
                memmove(handleData + tailOffset + deltaSize,
                       handleData + tailOffset,
                       tailSize);
            }
        } else {
            /* Shrinking - move tail data backward */
            SInt32 tailOffset = offset + oldLen;
            SInt32 tailSize = handleSize - tailOffset;

            if (tailSize > 0) {
                memmove(handleData + offset + newLen,
                       handleData + tailOffset,
                       tailSize);
            }
        }

        /* Copy in new data if provided */
        if (ptr2 != NULL && len2 > 0) {
            memcpy(handleData + offset, ptr2, len2);
        }

        MUNGER_LOG("Munger: Modified handle (offset=%ld, oldLen=%ld, newLen=%ld, delta=%ld)\n",
                   (long)offset, (long)oldLen, (long)newLen, (long)deltaSize);
    }

    return offset;
}
