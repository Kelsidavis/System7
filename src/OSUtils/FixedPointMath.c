/*
 * FixedPointMath.c - Fixed-Point Mathematics Utilities
 *
 * Implements fixed-point arithmetic operations for Mac OS. Fixed-point numbers
 * use a 16.16 format (16 bits integer, 16 bits fractional part) to perform
 * fast fractional math without floating-point hardware.
 *
 * This was essential on early Macs that lacked FPUs, and is still used
 * throughout QuickDraw and other graphics operations for performance.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations */
Fixed FixMul(Fixed a, Fixed b);
Fixed FixDiv(Fixed dividend, Fixed divisor);
Fixed FixRatio(SInt16 numer, SInt16 denom);
Fixed FixRound(Fixed x);
SInt32 Fix2Long(Fixed x);
Fixed Long2Fix(SInt32 x);
Fract Fix2Frac(Fixed x);
Fixed Frac2Fix(Fract x);

/* Debug logging */
#define FIXED_DEBUG 0

#if FIXED_DEBUG
#define FIXED_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[FixedMath] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define FIXED_LOG(...)
#endif

/*
 * Fixed-Point Format Notes:
 *
 * Fixed numbers use a 16.16 format:
 * - Bits 31-16: Integer part (signed)
 * - Bits 15-0:  Fractional part
 *
 * Examples:
 *   1.0 = 0x00010000 (65536 decimal)
 *   0.5 = 0x00008000 (32768 decimal)
 *   2.5 = 0x00028000 (163840 decimal)
 *  -1.0 = 0xFFFF0000 (-65536 decimal)
 *
 * Fract numbers use a 2.30 format:
 * - Bits 31-30: Integer part (signed, typically -1, 0, or nearly 1)
 * - Bits 29-0:  Fractional part
 *
 * This allows representing values from -2 to nearly +2 with high precision.
 */

/*
 * FixMul - Multiply two Fixed-point numbers
 *
 * Multiplies two Fixed-point numbers and returns the result as a Fixed-point
 * number. The multiplication is performed using 64-bit intermediate arithmetic
 * to prevent overflow.
 *
 * Parameters:
 *   a - First Fixed-point number (16.16 format)
 *   b - Second Fixed-point number (16.16 format)
 *
 * Returns:
 *   Product as a Fixed-point number
 *
 * Formula:
 *   result = (a * b) / 65536
 *   Using 64-bit intermediate: result = (SInt64)a * b >> 16
 *
 * Example:
 *   FixMul(0x00010000, 0x00020000) = 0x00020000  // 1.0 * 2.0 = 2.0
 *   FixMul(0x00008000, 0x00010000) = 0x00008000  // 0.5 * 1.0 = 0.5
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
Fixed FixMul(Fixed a, Fixed b) {
    SInt64 product;
    Fixed result;

    /* Multiply using 64-bit intermediate to prevent overflow */
    product = (SInt64)a * (SInt64)b;

    /* Shift right by 16 bits to account for fixed-point format */
    result = (Fixed)(product >> 16);

    FIXED_LOG("FixMul: 0x%08X * 0x%08X = 0x%08X\n",
              (unsigned int)a, (unsigned int)b, (unsigned int)result);

    return result;
}

/*
 * FixDiv - Divide two Fixed-point numbers
 *
 * Divides one Fixed-point number by another and returns the result as a
 * Fixed-point number. Uses 64-bit intermediate arithmetic for precision.
 *
 * Parameters:
 *   dividend - Numerator (Fixed-point 16.16 format)
 *   divisor - Denominator (Fixed-point 16.16 format)
 *
 * Returns:
 *   Quotient as a Fixed-point number
 *   Returns 0x7FFFFFFF (max positive) if divisor is 0 and dividend >= 0
 *   Returns 0x80000000 (max negative) if divisor is 0 and dividend < 0
 *
 * Formula:
 *   result = (dividend * 65536) / divisor
 *   Using 64-bit: result = ((SInt64)dividend << 16) / divisor
 *
 * Example:
 *   FixDiv(0x00020000, 0x00010000) = 0x00020000  // 2.0 / 1.0 = 2.0
 *   FixDiv(0x00010000, 0x00020000) = 0x00008000  // 1.0 / 2.0 = 0.5
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
Fixed FixDiv(Fixed dividend, Fixed divisor) {
    SInt64 numerator;
    Fixed result;

    /* Check for division by zero */
    if (divisor == 0) {
        FIXED_LOG("FixDiv: Division by zero!\n");
        /* Return max positive or negative value based on dividend sign */
        return (dividend >= 0) ? 0x7FFFFFFF : 0x80000000;
    }

    /* Shift left by 16 bits before division to maintain fixed-point format */
    numerator = (SInt64)dividend << 16;

    /* Perform division */
    result = (Fixed)(numerator / divisor);

    FIXED_LOG("FixDiv: 0x%08X / 0x%08X = 0x%08X\n",
              (unsigned int)dividend, (unsigned int)divisor, (unsigned int)result);

    return result;
}

/*
 * FixRatio - Create Fixed-point number from integer ratio
 *
 * Converts a ratio of two integers into a Fixed-point number. This is more
 * efficient than calling Long2Fix followed by FixDiv.
 *
 * Parameters:
 *   numer - Numerator (16-bit integer)
 *   denom - Denominator (16-bit integer)
 *
 * Returns:
 *   Ratio as a Fixed-point number
 *
 * Example:
 *   FixRatio(1, 2) = 0x00008000  // 1/2 = 0.5
 *   FixRatio(3, 4) = 0x0000C000  // 3/4 = 0.75
 *   FixRatio(5, 2) = 0x00028000  // 5/2 = 2.5
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
Fixed FixRatio(SInt16 numer, SInt16 denom) {
    SInt64 numerator;
    Fixed result;

    /* Check for division by zero */
    if (denom == 0) {
        FIXED_LOG("FixRatio: Division by zero!\n");
        /* Return max positive or negative value based on numerator sign */
        return (numer >= 0) ? 0x7FFFFFFF : 0x80000000;
    }

    /* Convert numerator to fixed-point (shift left 16) and divide */
    numerator = (SInt64)numer << 16;
    result = (Fixed)(numerator / denom);

    FIXED_LOG("FixRatio: %d / %d = 0x%08X\n",
              numer, denom, (unsigned int)result);

    return result;
}

/*
 * FixRound - Round Fixed-point number to nearest integer
 *
 * Rounds a Fixed-point number to the nearest integer value, returning the
 * result as a signed 32-bit integer. Uses round-half-up (0.5 rounds up).
 *
 * Parameters:
 *   x - Fixed-point number to round
 *
 * Returns:
 *   Rounded integer value
 *
 * Example:
 *   FixRound(0x00018000) = 2   // 1.5 rounds up to 2
 *   FixRound(0x00014000) = 1   // 1.25 rounds down to 1
 *   FixRound(0x0001C000) = 2   // 1.75 rounds up to 2
 *   FixRound(0xFFFE8000) = -1  // -1.5 rounds up to -1 (toward zero)
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
SInt32 FixRound(Fixed x) {
    SInt32 result;

    /* Add 0.5 (0x00008000) and truncate for round-half-up */
    result = (x + 0x00008000) >> 16;

    FIXED_LOG("FixRound: 0x%08X = %ld\n", (unsigned int)x, (long)result);

    return result;
}

/*
 * Fix2Long - Convert Fixed-point to integer (truncate)
 *
 * Extracts the integer part of a Fixed-point number, truncating the
 * fractional part (rounding toward zero).
 *
 * Parameters:
 *   x - Fixed-point number
 *
 * Returns:
 *   Integer part of the Fixed-point number
 *
 * Example:
 *   Fix2Long(0x00018000) = 1   // 1.5 truncates to 1
 *   Fix2Long(0x00028000) = 2   // 2.5 truncates to 2
 *   Fix2Long(0xFFFE8000) = -1  // -1.5 truncates to -1
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
SInt32 Fix2Long(Fixed x) {
    SInt32 result;

    /* Simply shift right 16 bits to extract integer part */
    result = x >> 16;

    FIXED_LOG("Fix2Long: 0x%08X = %ld\n", (unsigned int)x, (long)result);

    return result;
}

/*
 * Long2Fix - Convert integer to Fixed-point
 *
 * Converts a signed 32-bit integer to a Fixed-point number by setting the
 * fractional part to zero.
 *
 * Parameters:
 *   x - Integer value to convert
 *
 * Returns:
 *   Fixed-point representation of the integer
 *
 * Example:
 *   Long2Fix(1) = 0x00010000   // 1 becomes 1.0
 *   Long2Fix(2) = 0x00020000   // 2 becomes 2.0
 *   Long2Fix(-1) = 0xFFFF0000  // -1 becomes -1.0
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
Fixed Long2Fix(SInt32 x) {
    Fixed result;

    /* Shift left 16 bits to create fixed-point representation */
    result = x << 16;

    FIXED_LOG("Long2Fix: %ld = 0x%08X\n", (long)x, (unsigned int)result);

    return result;
}

/*
 * Fix2Frac - Convert Fixed (16.16) to Fract (2.30)
 *
 * Converts a Fixed-point number (16.16 format) to a Fract number (2.30 format).
 * Fract numbers have higher precision for fractional values between -2 and +2.
 *
 * Parameters:
 *   x - Fixed-point number (16.16 format)
 *
 * Returns:
 *   Fract number (2.30 format)
 *
 * Note: Values outside the range -2 to +2 will overflow in Fract format.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
Fract Fix2Frac(Fixed x) {
    Fract result;

    /* Shift left 14 bits to convert from 16.16 to 2.30 format */
    result = (Fract)((SInt64)x << 14);

    FIXED_LOG("Fix2Frac: 0x%08X = 0x%08X\n",
              (unsigned int)x, (unsigned int)result);

    return result;
}

/*
 * Frac2Fix - Convert Fract (2.30) to Fixed (16.16)
 *
 * Converts a Fract number (2.30 format) to a Fixed-point number (16.16 format).
 * Some precision is lost in the conversion due to the reduction in fractional bits.
 *
 * Parameters:
 *   x - Fract number (2.30 format)
 *
 * Returns:
 *   Fixed-point number (16.16 format)
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
Fixed Frac2Fix(Fract x) {
    Fixed result;

    /* Shift right 14 bits to convert from 2.30 to 16.16 format */
    result = (Fixed)(x >> 14);

    FIXED_LOG("Frac2Fix: 0x%08X = 0x%08X\n",
              (unsigned int)x, (unsigned int)result);

    return result;
}
