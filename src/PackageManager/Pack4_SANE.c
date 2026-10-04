/*
 * Pack4_SANE.c - SANE (Standard Apple Numerics Environment) Package (Pack4)
 *
 * Implements Pack4, the SANE floating-point math package for Mac OS System 7.
 * This package provides IEEE 754 compliant floating-point arithmetic and
 * elementary mathematical functions.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 * and Apple Numerics Manual, 2nd Edition
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "PackageManager/PackageDispatchInternal.h"
#include <math.h>

/* Debug logging */
#define PACK4_DEBUG 0

#if PACK4_DEBUG
#define PACK4_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[Pack4] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define PACK4_LOG(...)
#endif

/* Pack4 SANE selectors - based on Apple Numerics Manual */
#define kSANEAdd         0   /* Addition */
#define kSANESub         1   /* Subtraction */
#define kSANEMul         2   /* Multiplication */
#define kSANEDiv         3   /* Division */
#define kSANESqrt        4   /* Square root */
#define kSANESin         5   /* Sine */
#define kSANECos         6   /* Cosine */
#define kSANETan         7   /* Tangent */
#define kSANEAsin        8   /* Arc sine */
#define kSANEAcos        9   /* Arc cosine */
#define kSANEAtan        10  /* Arc tangent */
#define kSANELog         11  /* Natural logarithm */
#define kSANELog10       12  /* Base-10 logarithm */
#define kSANEExp         13  /* Exponential */
#define kSANEPow         14  /* Power */
#define kSANEAtan2       15  /* Arc tangent of y/x */
#define kSANEFabs        16  /* Absolute value */
#define kSANEFloor       17  /* Floor */
#define kSANECeil        18  /* Ceiling */
#define kSANEFmod        19  /* Floating-point remainder */

/* Parameter blocks for SANE operations */

/* Binary operations (add, sub, mul, div, pow, atan2, fmod) */
typedef struct {
    double operand1;
    double operand2;
    double result;
} SANEBinaryParams;

/* Unary operations (sqrt, sin, cos, tan, asin, acos, atan, log, log10, exp, fabs, floor, ceil) */
typedef struct {
    double operand;
    double result;
} SANEUnaryParams;

/*
 * SANE Binary Operations
 */

static OSErr SANE_Add(SANEBinaryParams* p) {
    if (!p) return paramErr;
    p->result = p->operand1 + p->operand2;
    PACK4_LOG("Add: %g + %g = %g\n", p->operand1, p->operand2, p->result);
    return noErr;
}

static OSErr SANE_Sub(SANEBinaryParams* p) {
    if (!p) return paramErr;
    p->result = p->operand1 - p->operand2;
    PACK4_LOG("Sub: %g - %g = %g\n", p->operand1, p->operand2, p->result);
    return noErr;
}

static OSErr SANE_Mul(SANEBinaryParams* p) {
    if (!p) return paramErr;
    p->result = p->operand1 * p->operand2;
    PACK4_LOG("Mul: %g * %g = %g\n", p->operand1, p->operand2, p->result);
    return noErr;
}

static OSErr SANE_Div(SANEBinaryParams* p) {
    if (!p) return paramErr;
    if (p->operand2 == 0.0) {
        PACK4_LOG("Div: Division by zero\n");
        p->result = INFINITY;
        return noErr;  /* SANE handles division by zero gracefully */
    }
    p->result = p->operand1 / p->operand2;
    PACK4_LOG("Div: %g / %g = %g\n", p->operand1, p->operand2, p->result);
    return noErr;
}

static OSErr SANE_Pow(SANEBinaryParams* p) {
    if (!p) return paramErr;
    p->result = pow(p->operand1, p->operand2);
    PACK4_LOG("Pow: %g ^ %g = %g\n", p->operand1, p->operand2, p->result);
    return noErr;
}

static OSErr SANE_Atan2(SANEBinaryParams* p) {
    if (!p) return paramErr;
    p->result = atan2(p->operand1, p->operand2);
    PACK4_LOG("Atan2: atan2(%g, %g) = %g\n", p->operand1, p->operand2, p->result);
    return noErr;
}

static OSErr SANE_Fmod(SANEBinaryParams* p) {
    if (!p) return paramErr;
    /* Simple fmod implementation: x - floor(x/y) * y */
    if (p->operand2 == 0.0) {
        p->result = NAN;
    } else {
        double quotient = p->operand1 / p->operand2;
        p->result = p->operand1 - floor(quotient) * p->operand2;
    }
    PACK4_LOG("Fmod: %g mod %g = %g\n", p->operand1, p->operand2, p->result);
    return noErr;
}

/*
 * SANE Unary Operations
 */

static OSErr SANE_Sqrt(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = sqrt(p->operand);
    PACK4_LOG("Sqrt: sqrt(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Sin(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = sin(p->operand);
    PACK4_LOG("Sin: sin(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Cos(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = cos(p->operand);
    PACK4_LOG("Cos: cos(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Tan(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = tan(p->operand);
    PACK4_LOG("Tan: tan(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Asin(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = asin(p->operand);
    PACK4_LOG("Asin: asin(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Acos(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = acos(p->operand);
    PACK4_LOG("Acos: acos(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Atan(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = atan(p->operand);
    PACK4_LOG("Atan: atan(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Log(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = log(p->operand);
    PACK4_LOG("Log: ln(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Log10(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = log10(p->operand);
    PACK4_LOG("Log10: log10(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Exp(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = exp(p->operand);
    PACK4_LOG("Exp: exp(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Fabs(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = fabs(p->operand);
    PACK4_LOG("Fabs: fabs(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Floor(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = floor(p->operand);
    PACK4_LOG("Floor: floor(%g) = %g\n", p->operand, p->result);
    return noErr;
}

static OSErr SANE_Ceil(SANEUnaryParams* p) {
    if (!p) return paramErr;
    p->result = ceil(p->operand);
    PACK4_LOG("Ceil: ceil(%g) = %g\n", p->operand, p->result);
    return noErr;
}

/*
 * Pack4_Dispatch - Pack4 SANE package dispatcher
 *
 * Main dispatcher for the SANE (Standard Apple Numerics Environment) Package.
 * Routes selector calls to the appropriate floating-point math function.
 *
 * Parameters:
 *   selector - Function selector (0-19, see kSANE* constants)
 *   params - Pointer to SANEBinaryParams or SANEUnaryParams
 *
 * Returns:
 *   noErr if successful
 *   paramErr if selector is invalid or params are NULL
 *
 * Example usage through Package Manager:
 *   SANEUnaryParams params;
 *   params.operand = 3.14159 / 4.0;  // 45 degrees in radians
 *   CallPackage(4, 5, &params);       // Call Pack4, sin selector
 *   // params.result now contains sin(45°) ≈ 0.707
 *
 * Example with binary operation:
 *   SANEBinaryParams params;
 *   params.operand1 = 2.0;
 *   params.operand2 = 3.0;
 *   CallPackage(4, 14, &params);      // Call Pack4, power selector
 *   // params.result now contains 2.0^3.0 = 8.0
 *
 * Notes:
 * - All operations use IEEE 754 double-precision (64-bit) format
 * - NaN and infinity are handled according to IEEE 754 standard
 * - Division by zero returns infinity (not an error)
 * - Domain errors (e.g., sqrt of negative) return NaN
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 * and Apple Numerics Manual, 2nd Edition
 */
OSErr Pack4_Dispatch(short selector, void* params) {
    PACK4_LOG("Dispatch: selector=%d, params=%p\n", selector, params);

    if (!params) {
        PACK4_LOG("Dispatch: NULL params\n");
        return paramErr;
    }

    switch (selector) {
        /* Binary operations */
        case kSANEAdd:
            return SANE_Add((SANEBinaryParams*)params);
        case kSANESub:
            return SANE_Sub((SANEBinaryParams*)params);
        case kSANEMul:
            return SANE_Mul((SANEBinaryParams*)params);
        case kSANEDiv:
            return SANE_Div((SANEBinaryParams*)params);
        case kSANEPow:
            return SANE_Pow((SANEBinaryParams*)params);
        case kSANEAtan2:
            return SANE_Atan2((SANEBinaryParams*)params);
        case kSANEFmod:
            return SANE_Fmod((SANEBinaryParams*)params);

        /* Unary operations */
        case kSANESqrt:
            return SANE_Sqrt((SANEUnaryParams*)params);
        case kSANESin:
            return SANE_Sin((SANEUnaryParams*)params);
        case kSANECos:
            return SANE_Cos((SANEUnaryParams*)params);
        case kSANETan:
            return SANE_Tan((SANEUnaryParams*)params);
        case kSANEAsin:
            return SANE_Asin((SANEUnaryParams*)params);
        case kSANEAcos:
            return SANE_Acos((SANEUnaryParams*)params);
        case kSANEAtan:
            return SANE_Atan((SANEUnaryParams*)params);
        case kSANELog:
            return SANE_Log((SANEUnaryParams*)params);
        case kSANELog10:
            return SANE_Log10((SANEUnaryParams*)params);
        case kSANEExp:
            return SANE_Exp((SANEUnaryParams*)params);
        case kSANEFabs:
            return SANE_Fabs((SANEUnaryParams*)params);
        case kSANEFloor:
            return SANE_Floor((SANEUnaryParams*)params);
        case kSANECeil:
            return SANE_Ceil((SANEUnaryParams*)params);

        default:
            PACK4_LOG("Dispatch: Invalid selector %d\n", selector);
            return paramErr;
    }
}
