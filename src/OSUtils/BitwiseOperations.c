/*
 * BitwiseOperations.c - Bitwise Logical Operations
 *
 * Implements bitwise logical operations for Mac OS. These functions perform
 * AND, OR, XOR, NOT, and shift operations on 32-bit values. While modern
 * C provides these as operators, Mac OS defined them as functions for
 * consistency and to support assembly language implementations.
 *
 * These functions are commonly used for flag manipulation, bit masking,
 * and low-level data processing throughout the Toolbox.
 *
 * Based on Inside Macintosh: Operating System Utilities
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations */
SInt32 BitAnd(SInt32 value1, SInt32 value2);
SInt32 BitOr(SInt32 value1, SInt32 value2);
SInt32 BitXor(SInt32 value1, SInt32 value2);
SInt32 BitNot(SInt32 value);
SInt32 BitShift(SInt32 value, SInt16 count);

/* Debug logging */
#define BITWISE_DEBUG 0

#if BITWISE_DEBUG
#define BITWISE_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[Bitwise] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define BITWISE_LOG(...)
#endif

/*
 * BitAnd - Bitwise AND operation
 *
 * Performs a bitwise AND operation on two 32-bit values. Each bit in the
 * result is set to 1 only if the corresponding bits in both operands are 1.
 *
 * Parameters:
 *   value1 - First operand
 *   value2 - Second operand
 *
 * Returns:
 *   Bitwise AND of value1 and value2
 *
 * Truth table for each bit:
 *   0 AND 0 = 0
 *   0 AND 1 = 0
 *   1 AND 0 = 0
 *   1 AND 1 = 1
 *
 * Example:
 *   BitAnd(0x0F0F0F0F, 0xFFFF0000) = 0x0F0F0000
 *   BitAnd(0b1100, 0b1010) = 0b1000
 *
 * Common uses:
 *   - Extracting specific bits (masking)
 *   - Testing if flags are set
 *   - Clearing unwanted bits
 *
 * Based on Inside Macintosh: Operating System Utilities
 */
SInt32 BitAnd(SInt32 value1, SInt32 value2) {
    SInt32 result;

    result = value1 & value2;

    BITWISE_LOG("BitAnd: 0x%08X & 0x%08X = 0x%08X\n",
                (unsigned int)value1, (unsigned int)value2, (unsigned int)result);

    return result;
}

/*
 * BitOr - Bitwise OR operation
 *
 * Performs a bitwise OR operation on two 32-bit values. Each bit in the
 * result is set to 1 if the corresponding bit in either operand is 1.
 *
 * Parameters:
 *   value1 - First operand
 *   value2 - Second operand
 *
 * Returns:
 *   Bitwise OR of value1 and value2
 *
 * Truth table for each bit:
 *   0 OR 0 = 0
 *   0 OR 1 = 1
 *   1 OR 0 = 1
 *   1 OR 1 = 1
 *
 * Example:
 *   BitOr(0x0F0F0F0F, 0xFFFF0000) = 0xFFFF0F0F
 *   BitOr(0b1100, 0b1010) = 0b1110
 *
 * Common uses:
 *   - Setting specific bits (flags)
 *   - Combining bit masks
 *   - Merging fields
 *
 * Based on Inside Macintosh: Operating System Utilities
 */
SInt32 BitOr(SInt32 value1, SInt32 value2) {
    SInt32 result;

    result = value1 | value2;

    BITWISE_LOG("BitOr: 0x%08X | 0x%08X = 0x%08X\n",
                (unsigned int)value1, (unsigned int)value2, (unsigned int)result);

    return result;
}

/*
 * BitXor - Bitwise XOR (exclusive OR) operation
 *
 * Performs a bitwise XOR operation on two 32-bit values. Each bit in the
 * result is set to 1 if the corresponding bits in the operands are different.
 *
 * Parameters:
 *   value1 - First operand
 *   value2 - Second operand
 *
 * Returns:
 *   Bitwise XOR of value1 and value2
 *
 * Truth table for each bit:
 *   0 XOR 0 = 0
 *   0 XOR 1 = 1
 *   1 XOR 0 = 1
 *   1 XOR 1 = 0
 *
 * Example:
 *   BitXor(0x0F0F0F0F, 0xFFFF0000) = 0xF0F00F0F
 *   BitXor(0b1100, 0b1010) = 0b0110
 *
 * Common uses:
 *   - Toggling specific bits
 *   - Simple encryption/obfuscation
 *   - Detecting differences
 *   - Swapping values (x = x XOR y; y = x XOR y; x = x XOR y)
 *
 * Based on Inside Macintosh: Operating System Utilities
 */
SInt32 BitXor(SInt32 value1, SInt32 value2) {
    SInt32 result;

    result = value1 ^ value2;

    BITWISE_LOG("BitXor: 0x%08X ^ 0x%08X = 0x%08X\n",
                (unsigned int)value1, (unsigned int)value2, (unsigned int)result);

    return result;
}

/*
 * BitNot - Bitwise NOT operation (one's complement)
 *
 * Performs a bitwise NOT operation on a 32-bit value. Each bit in the
 * result is the inverse of the corresponding bit in the operand.
 *
 * Parameters:
 *   value - Value to invert
 *
 * Returns:
 *   Bitwise NOT of value (one's complement)
 *
 * Truth table for each bit:
 *   NOT 0 = 1
 *   NOT 1 = 0
 *
 * Example:
 *   BitNot(0x0F0F0F0F) = 0xF0F0F0F0
 *   BitNot(0b00001111) = 0b11110000 (in 8-bit representation)
 *   BitNot(0x00000000) = 0xFFFFFFFF
 *   BitNot(0xFFFFFFFF) = 0x00000000
 *
 * Common uses:
 *   - Creating inverse masks
 *   - Bitwise complement operations
 *   - Converting between positive and negative patterns
 *
 * Based on Inside Macintosh: Operating System Utilities
 */
SInt32 BitNot(SInt32 value) {
    SInt32 result;

    result = ~value;

    BITWISE_LOG("BitNot: ~0x%08X = 0x%08X\n",
                (unsigned int)value, (unsigned int)result);

    return result;
}

/*
 * BitShift - Bitwise shift operation
 *
 * Shifts the bits in a 32-bit value left or right by the specified count.
 * Positive count shifts left (multiply by powers of 2), negative count
 * shifts right (divide by powers of 2). This is an arithmetic shift that
 * preserves the sign bit for right shifts.
 *
 * Parameters:
 *   value - Value to shift
 *   count - Number of bit positions to shift
 *           Positive: shift left (<<)
 *           Negative: shift right (>>)
 *           Zero: no shift
 *
 * Returns:
 *   Shifted value
 *
 * Examples:
 *   BitShift(0x00000001, 4) = 0x00000010   // Shift left 4 bits (multiply by 16)
 *   BitShift(0x00000080, -4) = 0x00000008  // Shift right 4 bits (divide by 16)
 *   BitShift(0xFFFFFFFF, -1) = 0xFFFFFFFF  // Arithmetic right shift preserves sign
 *   BitShift(0x7FFFFFFF, -1) = 0x3FFFFFFF  // Positive values shift normally
 *
 * Common uses:
 *   - Fast multiplication/division by powers of 2
 *   - Extracting bit fields
 *   - Packing/unpacking data
 *   - Fixed-point arithmetic
 *
 * Note: For shifts >= 32 or <= -32, behavior is implementation-defined.
 * This implementation clamps to reasonable bounds.
 *
 * Based on Inside Macintosh: Operating System Utilities
 */
SInt32 BitShift(SInt32 value, SInt16 count) {
    SInt32 result;

    /* Clamp shift count to prevent undefined behavior */
    if (count >= 32) {
        result = 0;  /* Left shift beyond width gives 0 */
    } else if (count <= -32) {
        /* Right arithmetic shift beyond width gives sign extension */
        result = (value < 0) ? -1 : 0;
    } else if (count > 0) {
        /* Left shift */
        result = value << count;
    } else if (count < 0) {
        /* Right arithmetic shift (preserves sign) */
        result = value >> (-count);
    } else {
        /* No shift */
        result = value;
    }

    BITWISE_LOG("BitShift: 0x%08X %s %d = 0x%08X\n",
                (unsigned int)value,
                (count >= 0) ? "<<" : ">>",
                (count >= 0) ? count : -count,
                (unsigned int)result);

    return result;
}
