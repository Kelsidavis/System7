/*
 * OSUtils.h - Operating System Utilities
 *
 * Provides various utility functions for memory manipulation, data searching,
 * and other common operations.
 *
 * Based on Inside Macintosh: Operating System Utilities
 */

#ifndef __OSUTILS_H__
#define __OSUTILS_H__

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Munger - Memory munging (search and replace) in a handle
 *
 * Searches for data in a handle and optionally replaces it or inserts data.
 * This is a powerful utility used throughout the Toolbox.
 *
 * Parameters:
 *   h - Handle to search/modify
 *   offset - Starting offset for search
 *   ptr1 - Pointer to data to search for (NULL = don't search)
 *   len1 - Length of search data (0 = don't search)
 *   ptr2 - Pointer to replacement data (NULL = delete)
 *   len2 - Length of replacement data
 *
 * Returns:
 *   Offset where match was found, or -1 if not found
 *   If ptr1 is NULL, inserts ptr2 at offset and returns offset
 *
 * Based on Inside Macintosh: Memory
 */
SInt32 Munger(Handle h, SInt32 offset, const void* ptr1, SInt32 len1,
              const void* ptr2, SInt32 len2);

/* Date/Time utilities */
typedef struct {
    SInt16 year;
    SInt16 month;
    SInt16 day;
    SInt16 hour;
    SInt16 minute;
    SInt16 second;
    SInt16 dayOfWeek; /* 1=Sunday through 7=Saturday */
} DateTimeRec;

void InitDateTime(void);
void GetDateTime(UInt32* secs);
void SetDateTime(UInt32 secs);
void ReadDateTime(UInt32* secs);
void Secs2Date(UInt32 secs, DateTimeRec* date);
void SecondsToDate(UInt32 secs, DateTimeRec* date);
void Date2Secs(const DateTimeRec* date, UInt32* secs);
void DateToSeconds(const DateTimeRec* date, UInt32* secs);

/* Delay utilities */
void Delay(UInt32 numTicks, UInt32* finalTicks);

/* Queue utilities */
void Enqueue(QElemPtr qElement, QHdr* qHeader);
OSErr Dequeue(QElemPtr qElement, QHdr* qHeader);

/* Bit manipulation utilities */
Boolean BitTst(const void* bytePtr, SInt32 bitNum);
void BitSet(void* bytePtr, SInt32 bitNum);
void BitClr(void* bytePtr, SInt32 bitNum);

/* Bitwise logical operations */
SInt32 BitAnd(SInt32 value1, SInt32 value2);
SInt32 BitOr(SInt32 value1, SInt32 value2);
SInt32 BitXor(SInt32 value1, SInt32 value2);
SInt32 BitNot(SInt32 value);
SInt32 BitShift(SInt32 value, SInt16 count);

/* Fixed-point mathematics utilities */
Fixed FixMul(Fixed a, Fixed b);
Fixed FixDiv(Fixed dividend, Fixed divisor);
Fixed FixRatio(SInt16 numer, SInt16 denom);
SInt32 FixRound(Fixed x);
SInt32 Fix2Long(Fixed x);
Fixed Long2Fix(SInt32 x);
Fract Fix2Frac(Fixed x);
Fixed Frac2Fix(Fract x);

#ifdef __cplusplus
}
#endif

#endif /* __OSUTILS_H__ */
