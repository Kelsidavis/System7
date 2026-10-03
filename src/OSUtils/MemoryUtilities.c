/*
 * MemoryUtilities.c - Memory Manipulation Utilities
 *
 * Implements extended memory manipulation functions for Mac OS.
 * Basic functions like BlockMove, HiWord, LoWord are in System71StdLib.c.
 * This file contains extended utilities like LongMul.
 *
 * Based on Inside Macintosh: Operating System Utilities
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations */
void LongMul(SInt32 a, SInt32 b, wide* result);

/* Debug logging */
#define MEM_UTIL_DEBUG 0

#if MEM_UTIL_DEBUG
#define MEMUTIL_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[MemUtil] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define MEMUTIL_LOG(...)
#endif

/*
 * LongMul - Multiply two 32-bit values producing 64-bit result
 *
 * Multiplies two signed 32-bit integers and produces a 64-bit result.
 * This is useful for calculations that might overflow a 32-bit result,
 * such as fixed-point arithmetic or large numeric computations.
 *
 * Parameters:
 *   a - First 32-bit multiplicand
 *   b - Second 32-bit multiplicand
 *   result - Pointer to 64-bit wide result structure
 *
 * The result is stored in a wide structure:
 *   result->hi - High 32 bits of product
 *   result->lo - Low 32 bits of product
 *
 * Example:
 *   wide result;
 *   LongMul(1000000, 1000000, &result);
 *   // result = 1,000,000,000,000 (0xE8D4A51000)
 *   // result.hi = 0x000000E8 (232)
 *   // result.lo = 0xD4A51000 (3,567,587,328)
 *
 * Common uses:
 *   - Large integer arithmetic
 *   - Fixed-point calculations requiring extended precision
 *   - Financial calculations
 *   - Time calculations (ticks to milliseconds, etc.)
 *
 * Based on Inside Macintosh: Operating System Utilities
 */
void LongMul(SInt32 a, SInt32 b, wide* result) {
    SInt64 product;

    if (!result) {
        MEMUTIL_LOG("LongMul: NULL result pointer\n");
        return;
    }

    /* Perform 64-bit multiplication */
    product = (SInt64)a * (SInt64)b;

    /* Split result into high and low 32-bit parts */
    result->hi = (SInt32)((product >> 32) & 0xFFFFFFFF);
    result->lo = (UInt32)(product & 0xFFFFFFFF);

    MEMUTIL_LOG("LongMul: %ld * %ld = 0x%08X%08X\n",
                (long)a, (long)b,
                (unsigned int)result->hi,
                (unsigned int)result->lo);
}
