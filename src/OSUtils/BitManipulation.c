/*
 * BitManipulation.c - Bit Manipulation Utilities
 *
 * Implements low-level bit manipulation functions for testing, setting,
 * and clearing individual bits in memory. These are fundamental utilities
 * used throughout the Mac OS Toolbox.
 *
 * Based on Inside Macintosh: Operating System Utilities
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations */
Boolean BitTst(const void* bytePtr, SInt32 bitNum);
void BitSet(void* bytePtr, SInt32 bitNum);
void BitClr(void* bytePtr, SInt32 bitNum);

/* Debug logging */
#define BIT_DEBUG 0

#if BIT_DEBUG
#define BIT_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[BitManip] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define BIT_LOG(...)
#endif

/*
 * BitTst - Test a bit
 *
 * Tests the value of a specified bit in memory. Bits are numbered starting
 * from bit 0 (the high-order bit) of the first byte. This follows the
 * Mac OS convention where bit 0 is the most significant bit.
 *
 * Parameters:
 *   bytePtr - Pointer to the byte containing the bit
 *   bitNum - Bit number to test (0 = high-order bit of first byte,
 *            8 = high-order bit of second byte, etc.)
 *
 * Returns:
 *   true if the bit is set (1), false if clear (0)
 *
 * Example:
 *   For byte 0x80 (10000000 binary):
 *   BitTst(&byte, 0) returns true  (high bit set)
 *   BitTst(&byte, 1) returns false (bit 1 clear)
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
Boolean BitTst(const void* bytePtr, SInt32 bitNum) {
    const UInt8* bytes;
    SInt32 byteOffset;
    SInt32 bitOffset;
    UInt8 mask;
    Boolean result;

    if (!bytePtr) {
        BIT_LOG("BitTst: NULL pointer\n");
        return false;
    }

    bytes = (const UInt8*)bytePtr;

    /* Calculate which byte contains the bit */
    byteOffset = bitNum / 8;

    /* Calculate bit position within the byte (0-7) */
    bitOffset = bitNum % 8;

    /* Create mask: bit 0 is high-order bit (0x80), bit 7 is low-order (0x01) */
    mask = 0x80 >> bitOffset;

    /* Test the bit */
    result = (bytes[byteOffset] & mask) != 0;

    BIT_LOG("BitTst: ptr=%p bit=%ld -> %d\n", bytePtr, (long)bitNum, result);

    return result;
}

/*
 * BitSet - Set a bit to 1
 *
 * Sets the specified bit in memory to 1. Bits are numbered starting from
 * bit 0 (the high-order bit) of the first byte. This operation modifies
 * the memory at the specified location.
 *
 * Parameters:
 *   bytePtr - Pointer to the byte containing the bit
 *   bitNum - Bit number to set (0 = high-order bit of first byte,
 *            8 = high-order bit of second byte, etc.)
 *
 * Example:
 *   byte = 0x00;  // 00000000 binary
 *   BitSet(&byte, 0);  // Sets high bit: byte becomes 0x80 (10000000)
 *   BitSet(&byte, 7);  // Sets low bit: byte becomes 0x81 (10000001)
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
void BitSet(void* bytePtr, SInt32 bitNum) {
    UInt8* bytes;
    SInt32 byteOffset;
    SInt32 bitOffset;
    UInt8 mask;

    if (!bytePtr) {
        BIT_LOG("BitSet: NULL pointer\n");
        return;
    }

    bytes = (UInt8*)bytePtr;

    /* Calculate which byte contains the bit */
    byteOffset = bitNum / 8;

    /* Calculate bit position within the byte (0-7) */
    bitOffset = bitNum % 8;

    /* Create mask: bit 0 is high-order bit (0x80), bit 7 is low-order (0x01) */
    mask = 0x80 >> bitOffset;

    /* Set the bit using OR */
    bytes[byteOffset] |= mask;

    BIT_LOG("BitSet: ptr=%p bit=%ld (byte[%ld] |= 0x%02X)\n",
            bytePtr, (long)bitNum, (long)byteOffset, mask);
}

/*
 * BitClr - Clear a bit to 0
 *
 * Clears the specified bit in memory to 0. Bits are numbered starting from
 * bit 0 (the high-order bit) of the first byte. This operation modifies
 * the memory at the specified location.
 *
 * Parameters:
 *   bytePtr - Pointer to the byte containing the bit
 *   bitNum - Bit number to clear (0 = high-order bit of first byte,
 *            8 = high-order bit of second byte, etc.)
 *
 * Example:
 *   byte = 0xFF;  // 11111111 binary
 *   BitClr(&byte, 0);  // Clears high bit: byte becomes 0x7F (01111111)
 *   BitClr(&byte, 7);  // Clears low bit: byte becomes 0x7E (01111110)
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */
void BitClr(void* bytePtr, SInt32 bitNum) {
    UInt8* bytes;
    SInt32 byteOffset;
    SInt32 bitOffset;
    UInt8 mask;

    if (!bytePtr) {
        BIT_LOG("BitClr: NULL pointer\n");
        return;
    }

    bytes = (UInt8*)bytePtr;

    /* Calculate which byte contains the bit */
    byteOffset = bitNum / 8;

    /* Calculate bit position within the byte (0-7) */
    bitOffset = bitNum % 8;

    /* Create mask: bit 0 is high-order bit (0x80), bit 7 is low-order (0x01) */
    mask = 0x80 >> bitOffset;

    /* Clear the bit using AND with inverted mask */
    bytes[byteOffset] &= ~mask;

    BIT_LOG("BitClr: ptr=%p bit=%ld (byte[%ld] &= ~0x%02X)\n",
            bytePtr, (long)bitNum, (long)byteOffset, mask);
}
