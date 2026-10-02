/*
 * SANENumbers.h - SANE's number formats and decimal conversions, in integers
 *
 * What the FP68K and Elems68K packages need that does not depend on the
 * floating-point hardware: the six operand formats as they lie in a 68K
 * program's memory, classifying, next-after, and conversion between binary
 * and SANE's decimal record - exact, by big-integer arithmetic, and rounded
 * in whatever direction the environment says. Pure C over integers, so it
 * behaves the same on every host and can be tested on any of them.
 *
 * Source: Apple Numerics Manual, second edition (1988).
 */

#ifndef SANE_NUMBERS_H
#define SANE_NUMBERS_H

#include <stdint.h>

/* Operand formats, as bits 11-13 of an FP68K opword give them */
enum {
    kSANEExtended = 0, kSANEDouble = 1, kSANESingle = 2,
    kSANEInteger = 4, kSANELongInt = 5, kSANEComp = 6
};

/* Rounding directions, as bits 13-14 of the environment word give them */
enum { kSANEToNearest = 0, kSANEUpward = 1, kSANEDownward = 2, kSANETowardZero = 3 };

/* Exception flags, as bits 8-12 of the environment word hold them; the
 * same bits shifted right by 8 are SANE's Exception masks. */
enum {
    kSANEInvalid   = 0x0100,
    kSANEUnderflow = 0x0200,
    kSANEOverflow  = 0x0400,
    kSANEDivByZero = 0x0800,
    kSANEInexact   = 0x1000
};

/* Classes, as FCLASS answers them (negated for a negative operand) */
enum {
    kSANESNaN = 1, kSANEQNaN = 2, kSANEInfinite = 3,
    kSANEZero = 4, kSANENormal = 5, kSANEDenormal = 6
};

/* NaN codes (Apple Numerics Manual, Table 5-1): in the byte of the
 * significand below the quiet bit */
enum {
    kNaNSqrt = 1, kNaNAdd = 2, kNaNDiv = 4, kNaNMul = 8, kNaNRem = 9,
    kNaNAsciiBin = 17, kNaNComp = 20, kNaNZero = 21, kNaNTrig = 33,
    kNaNInvTrig = 34, kNaNLog = 36, kNaNPower = 37, kNaNFinan = 38
};

/* An extended number by its fields: sign, 15-bit biased exponent, and the
 * 64-bit significand with its explicit integer bit. */
typedef struct {
    int      sign;
    uint16_t exp;
    uint64_t mant;
} SANEExt;

/* A decimal record (sig at most 20 digits; see SANE_Str2Dec) */
enum { kSANESigDigLen = 20, kSANEDecStrLen = 80 };
typedef struct SANEDecimalTag {
    int     sgn;
    int16_t exp;
    uint8_t sig[kSANESigDigLen + 1];   /* Pascal string */
} SANEDecimal;

typedef struct {
    int     fixed;                      /* FIXEDDECIMAL rather than FLOATDECIMAL */
    int16_t digits;
} SANEDecForm;

int SANE_FormatSize(int format);       /* bytes; 0 for no such format */

/* An operand's bytes (big-endian, as in the program's memory) as an
 * extended, exactly - every format fits. A comp NaN gives NaN(20). */
SANEExt SANE_ToExtended(int format, const uint8_t* bytes);
void SANE_ExtToBytes(SANEExt x, uint8_t bytes[10]);
SANEExt SANE_NaN(int code);            /* a quiet NaN carrying the code */
int SANE_NaNCode(SANEExt x);

/* An extended to a format, rounded in direction 'round': the bytes, and
 * the exceptions. Out of range for an integer format is invalid, and gives
 * that format's most negative value (comp's NaN). */
int SANE_FromExtended(SANEExt x, int format, int round, uint8_t* bytes);

int SANE_Classify(int format, const uint8_t* bytes);

/* -1, 0 or 1 as a is less than, equal to or greater than b; 2 if they are
 * unordered (either is a NaN). *signalling is set if either is a
 * signalling NaN. */
int SANE_Compare(SANEExt a, SANEExt b, int* signalling);

/* FNEXT: x moved one step toward y, in x's own format (extended, double or
 * single). Returns the exceptions it raises. */
int SANE_NextAfter(int format, uint8_t* x, const uint8_t* y);

/* FX2DEC: x to a decimal record, by form, rounded in direction 'round'.
 * Returns the exceptions (inexact). */
int SANE_X2Dec(const SANEDecForm* form, SANEExt x, int round, SANEDecimal* out);

/* FDEC2X: a decimal record to extended, rounded in direction 'round' - or,
 * with toOdd, rounded to odd: a result that a second rounding to fewer
 * bits rounds exactly as rounding the decimal directly would have.
 * Returns the exceptions. */
int SANE_Dec2X(const SANEDecimal* d, int round, int toOdd, SANEExt* out);

/* Str2Dec: scan s[*index..len-1] (0-based) for a number. *index moves past
 * the longest numeric prefix; validPrefix says whether the whole rest of
 * the string is the start of one. */
void SANE_Str2Dec(const uint8_t* s, int len, int* index, SANEDecimal* d, int* validPrefix);

/* Dec2Str: a decimal record as text, by form. out is a Pascal string of at
 * most 80 characters. */
void SANE_Dec2Str(const SANEDecForm* form, const SANEDecimal* d, uint8_t out[kSANEDecStrLen + 1]);

#endif /* SANE_NUMBERS_H */
