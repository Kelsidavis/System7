/*
 * SANEPackage.h
 * System 7.1 Portable SANE (Standard Apple Numerics Environment) Package
 *
 * Complete implementation of Mac OS SANE floating point operations.
 * This is CRITICAL - many Mac applications depend on SANE for all floating point math.
 * Without proper SANE support, applications will crash on mathematical operations.
 *
 * Implements IEEE 754 floating point standard with Mac OS compatibility layer.
 */

#ifndef __SANE_PACKAGE_H__
#define __SANE_PACKAGE_H__

#include "PackageTypes.h"
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SANE floating point types */
typedef double          extended;      /* Extended precision (80-bit on Mac, 64-bit here) */
typedef float           single;        /* Single precision 32-bit */
typedef double          comp;          /* Computational type */

/* Extended precision structure for 96-bit compatibility */
typedef struct {
    uint16_t words[6];                 /* 96-bit extended format */
} extended96;

/* Extended precision structure for 80-bit compatibility */
typedef struct {
    uint16_t words[5];                 /* 80-bit extended format */
} extended80;

/* Exception types */
typedef uint16_t exception;

/* Environment type for floating point state */
typedef uint16_t environment;

/* Relational operators */
typedef enum {
    GREATERTHAN = 0,
    LESSTHAN = 1,
    EQUALTO = 2,
    UNORDERED = 3
} relop;

/* Number classes for inquiry functions */
typedef enum {
    SNAN = 0,           /* Signaling NaN */
    QNAN = 1,           /* Quiet NaN */
    INFINITE = 2,       /* Infinite */
    ZERONUM = 3,        /* Zero */
    NORMALNUM = 4,      /* Normal number */
    DENORMALNUM = 5     /* Denormalized number */
} numclass;

/* Rounding directions */
typedef enum {
    TONEAREST = 0,      /* Round to nearest */
    UPWARD = 1,         /* Round upward */
    DOWNWARD = 2,       /* Round downward */
    TOWARDZERO = 3      /* Round toward zero */
} rounddir;

/* Rounding precisions */
typedef enum {
    EXTPRECISION = 0,   /* Extended precision */
    DBLPRECISION = 1,   /* Double precision */
    FLOATPRECISION = 2  /* Single precision */
} roundpre;

/* Exception flags */
#define INVALID         0x0001
#define UNDERFLOW       0x0002
#define OVERFLOW        0x0004
#define DIVBYZERO       0x0008
#define INEXACT         0x0010

/* Default environment */
#define IEEEDEFAULTENV  ((environment)0)

/* Decimal representation constants */
#define SIGDIGLEN       20
#define DECSTROUTLEN    80

/* Decimal formatting styles */
#define FLOATDECIMAL    0
#define FIXEDDECIMAL    1

/* Decimal number structure */
typedef struct {
    char sgn;                          /* Sign: 0 for +, 1 for - */
    char unused;
    int16_t exp;                       /* Decimal exponent */
    struct {
        unsigned char length;
        unsigned char text[SIGDIGLEN]; /* Significant digits */
        unsigned char unused;
    } sig;
} decimal;

/* Decimal format structure */
typedef struct {
    char style;                        /* FLOATDECIMAL or FIXEDDECIMAL */
    char unused;
    int16_t digits;                    /* Number of digits */
} decform;

/* Halt vector for exception handling */
typedef struct {
    uint16_t haltexceptions;
    uint16_t pendingCCR;
    int32_t pendingD0;
} mischaltinfo;

typedef void (*haltvector)(mischaltinfo *misc, void *src2, void *src, void *dst, int16_t opcode);

/* SANE Package API Functions */

/* Core SANE initialization and management */
int32_t InitSANEPackage(void);
void CleanupSANEPackage(void);
int32_t SANEDispatch(int16_t selector, void *params);

/* Basic arithmetic operations */
extended sane_add(extended x, extended y);
extended sane_sub(extended x, extended y);
extended sane_mul(extended x, extended y);
extended sane_div(extended x, extended y);
extended sane_rem(extended x, extended y);

/* Comparison operations */
relop sane_relation(extended x, extended y);
int32_t sane_signnum(extended x);

/* Transcendental functions */
extended sane_sqrt(extended x);
extended sane_sin(extended x);
extended sane_cos(extended x);
extended sane_tan(extended x);
extended sane_asin(extended x);
extended sane_acos(extended x);
extended sane_atan(extended x);
extended sane_atan2(extended y, extended x);
extended sane_log(extended x);
extended sane_log10(extended x);
extended sane_log2(extended x);
extended sane_log1(extended x);        /* ln(1+x) */
extended sane_exp(extended x);
extended sane_exp10(extended x);
extended sane_exp2(extended x);
extended sane_exp1(extended x);        /* exp(x)-1 */
extended sane_pow(extended x, extended y);
extended sane_ipower(extended x, int16_t i);

/* Hyperbolic functions */
extended sane_sinh(extended x);
extended sane_cosh(extended x);
extended sane_tanh(extended x);

/* Special functions */
extended sane_pi(void);
extended sane_inf(void);
extended sane_nan(unsigned char c);
extended sane_copysign(extended x, extended y);
extended sane_scalb(int16_t n, extended x);
extended sane_logb(extended x);
extended sane_rint(extended x);
extended sane_remainder(extended x, extended y, int16_t *quo);

/* Financial functions */
extended sane_compound(extended r, extended n);
extended sane_annuity(extended r, extended n);

/* Random number generation */
extended sane_randomx(extended *x);

/* Type conversion functions */
void sane_x96tox80(const extended96 *x96, extended80 *x80);
void sane_x80tox96(const extended80 *x80, extended96 *x96);
void sane_x96toextended(const extended96 *x96, extended *x);
void sane_extendedtox96(const extended *x, extended96 *x96);

/* Number classification */
numclass sane_classfloat(extended x);
numclass sane_classdouble(extended x);
numclass sane_classcomp(extended x);
numclass sane_classextended(extended x);

/* Decimal conversion */
void sane_num2dec(const decform *f, extended x, decimal *d);
extended sane_dec2num(const decimal *d);
void sane_dec2str(const decform *f, const decimal *d, char *s);
void sane_str2dec(const char *s, int16_t *ix, decimal *d, int16_t *vp);

/* Next representable number */
extended sane_nextfloat(extended x, extended y);
extended sane_nextdouble(extended x, extended y);
extended sane_nextextended(extended x, extended y);

/* Environment and exception handling */
void sane_setexception(exception e, int32_t s);
int32_t sane_testexception(exception e);
void sane_sethalt(exception e, int32_t s);
int32_t sane_testhalt(exception e);
void sane_setround(rounddir r);
rounddir sane_getround(void);
void sane_setprecision(roundpre p);
roundpre sane_getprecision(void);
void sane_setenvironment(environment e);
void sane_getenvironment(environment *e);
void sane_procentry(environment *e);
void sane_procexit(environment e);

/* Halt vector management */
haltvector sane_gethaltvector(void);
void sane_sethaltvector(haltvector v);

/* SANE selector constants for dispatch */
#define SANE_ADD            0x0000
#define SANE_SUB            0x0002
#define SANE_MUL            0x0004
#define SANE_DIV            0x0006
#define SANE_CMP            0x0008
#define SANE_CPSX           0x000A
#define SANE_REM            0x000C
#define SANE_X2Z            0x000E
#define SANE_SQRT           0x0010
#define SANE_RINT           0x0012
#define SANE_SCALB          0x0014
#define SANE_LOGB           0x0016
#define SANE_CLASS          0x0018
#define SANE_SIN            0x001A
#define SANE_COS            0x001C
#define SANE_TAN            0x001E
#define SANE_ATAN           0x0020
#define SANE_LOG            0x0022
#define SANE_EXP            0x0024
#define SANE_POW            0x0026
#define SANE_IPOWER         0x0028
#define SANE_LOG2           0x002A
#define SANE_EXP2           0x002C
#define SANE_LOG1           0x002E
#define SANE_EXP1           0x0030
#define SANE_COMPOUND       0x0032
#define SANE_ANNUITY        0x0034
#define SANE_RANDOMX        0x0036
#define SANE_SETENV         0x0038
#define SANE_GETENV         0x003A
#define SANE_SETHALT        0x003C
#define SANE_GETHALT        0x003E
#define SANE_SETROUND       0x0040
#define SANE_GETROUND       0x0042
#define SANE_SETPREC        0x0044
#define SANE_GETPREC        0x0046
#define SANE_SETEXC         0x0048
#define SANE_TESTEXC        0x004A
#define SANE_NUM2DEC        0x004C
#define SANE_DEC2NUM        0x004E
#define SANE_DEC2STR        0x0050
#define SANE_STR2DEC        0x0052

/* Utility macros for SANE compatibility */
#define isnan(x)            (sane_classfloat(x) == QNAN || sane_classfloat(x) == SNAN)
#define isinf(x)            (sane_classfloat(x) == INFINITE)
#define isfinite(x)         (sane_classfloat(x) == NORMALNUM || sane_classfloat(x) == DENORMALNUM || sane_classfloat(x) == ZERONUM)
#define isnormal(x)         (sane_classfloat(x) == NORMALNUM)

/* Platform math library integration */
void sane_set_math_library(void *mathLib);
void sane_enable_ieee754_mode(bool enabled);
void sane_enable_exception_handling(bool enabled);

/* SANE environment state */
typedef struct {
    rounddir        rounding;
    roundpre        precision;
    exception       exceptions;
    exception       halts;
    haltvector      halt_handler;
    bool            ieee754_mode;
    bool            exception_handling;
} SANEEnvironment;

/* Get current SANE environment */
void sane_get_environment_state(SANEEnvironment *env);
void sane_set_environment_state(const SANEEnvironment *env);

#ifdef __cplusplus
}
#endif

#endif /* __SANE_PACKAGE_H__ */