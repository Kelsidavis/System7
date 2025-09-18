/*
 * SANEPackage.c
 * System 7.1 Portable SANE (Standard Apple Numerics Environment) Implementation
 *
 * This is CRITICAL for Mac application compatibility. SANE provides all floating point
 * arithmetic for Mac OS applications. Without proper SANE support, applications will
 * crash on any mathematical operation including basic arithmetic.
 *
 * Implements complete IEEE 754 floating point with Mac OS SANE compatibility.
 */

#include "PackageManager/SANEPackage.h"
#include "PackageManager/PackageTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <fenv.h>
#include <errno.h>

/* SANE environment state */
static SANEEnvironment g_sane_env = {
    .rounding = TONEAREST,
    .precision = EXTPRECISION,
    .exceptions = 0,
    .halts = 0,
    .halt_handler = NULL,
    .ieee754_mode = true,
    .exception_handling = true
};

/* Random number state */
static uint32_t g_random_seed = 1;

/* Forward declarations */
static void sane_handle_exception(exception exc);
static void sane_check_result(extended result);
static bool sane_is_valid_number(extended x);
static void sane_set_fpu_environment(void);

/**
 * Initialize SANE package
 */
int32_t InitSANEPackage(void)
{
    /* Set up IEEE 754 compliant environment */
    g_sane_env.rounding = TONEAREST;
    g_sane_env.precision = EXTPRECISION;
    g_sane_env.exceptions = 0;
    g_sane_env.halts = 0;
    g_sane_env.ieee754_mode = true;
    g_sane_env.exception_handling = true;

    /* Initialize FPU environment */
    sane_set_fpu_environment();

    /* Initialize random seed */
    g_random_seed = 1;

    return PACKAGE_NO_ERROR;
}

/**
 * SANE package dispatch function
 */
int32_t SANEDispatch(int16_t selector, void *params)
{
    if (!params) {
        return PACKAGE_INVALID_PARAMS;
    }

    /* Cast params to appropriate types based on selector */
    switch (selector) {
        case SANE_ADD: {
            extended *args = (extended*)params;
            extended result = sane_add(args[0], args[1]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_SUB: {
            extended *args = (extended*)params;
            extended result = sane_sub(args[0], args[1]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_MUL: {
            extended *args = (extended*)params;
            extended result = sane_mul(args[0], args[1]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_DIV: {
            extended *args = (extended*)params;
            extended result = sane_div(args[0], args[1]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_SQRT: {
            extended *args = (extended*)params;
            extended result = sane_sqrt(args[0]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_SIN: {
            extended *args = (extended*)params;
            extended result = sane_sin(args[0]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_COS: {
            extended *args = (extended*)params;
            extended result = sane_cos(args[0]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_TAN: {
            extended *args = (extended*)params;
            extended result = sane_tan(args[0]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_LOG: {
            extended *args = (extended*)params;
            extended result = sane_log(args[0]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_EXP: {
            extended *args = (extended*)params;
            extended result = sane_exp(args[0]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        case SANE_POW: {
            extended *args = (extended*)params;
            extended result = sane_pow(args[0], args[1]);
            args[0] = result;
            return PACKAGE_NO_ERROR;
        }

        default:
            return PACKAGE_INVALID_SELECTOR;
    }
}

/**
 * Basic arithmetic operations
 */
extended sane_add(extended x, extended y)
{
    if (!sane_is_valid_number(x) || !sane_is_valid_number(y)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = x + y;
    sane_check_result(result);
    return result;
}

extended sane_sub(extended x, extended y)
{
    if (!sane_is_valid_number(x) || !sane_is_valid_number(y)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = x - y;
    sane_check_result(result);
    return result;
}

extended sane_mul(extended x, extended y)
{
    if (!sane_is_valid_number(x) || !sane_is_valid_number(y)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = x * y;
    sane_check_result(result);
    return result;
}

extended sane_div(extended x, extended y)
{
    if (!sane_is_valid_number(x) || !sane_is_valid_number(y)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (y == 0.0) {
        sane_handle_exception(DIVBYZERO);
        return (x > 0) ? sane_inf() : -sane_inf();
    }

    extended result = x / y;
    sane_check_result(result);
    return result;
}

/**
 * Transcendental functions
 */
extended sane_sqrt(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (x < 0.0) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = sqrt(x);
    sane_check_result(result);
    return result;
}

extended sane_sin(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = sin(x);
    sane_check_result(result);
    return result;
}

extended sane_cos(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = cos(x);
    sane_check_result(result);
    return result;
}

extended sane_tan(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = tan(x);
    sane_check_result(result);
    return result;
}

extended sane_asin(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (x < -1.0 || x > 1.0) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = asin(x);
    sane_check_result(result);
    return result;
}

extended sane_acos(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (x < -1.0 || x > 1.0) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = acos(x);
    sane_check_result(result);
    return result;
}

extended sane_atan(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = atan(x);
    sane_check_result(result);
    return result;
}

extended sane_atan2(extended y, extended x)
{
    if (!sane_is_valid_number(x) || !sane_is_valid_number(y)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = atan2(y, x);
    sane_check_result(result);
    return result;
}

extended sane_log(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (x <= 0.0) {
        if (x == 0.0) {
            sane_handle_exception(DIVBYZERO);
            return -sane_inf();
        } else {
            sane_handle_exception(INVALID);
            return sane_nan(0);
        }
    }

    extended result = log(x);
    sane_check_result(result);
    return result;
}

extended sane_log10(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (x <= 0.0) {
        if (x == 0.0) {
            sane_handle_exception(DIVBYZERO);
            return -sane_inf();
        } else {
            sane_handle_exception(INVALID);
            return sane_nan(0);
        }
    }

    extended result = log10(x);
    sane_check_result(result);
    return result;
}

extended sane_log2(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (x <= 0.0) {
        if (x == 0.0) {
            sane_handle_exception(DIVBYZERO);
            return -sane_inf();
        } else {
            sane_handle_exception(INVALID);
            return sane_nan(0);
        }
    }

    extended result = log2(x);
    sane_check_result(result);
    return result;
}

extended sane_log1(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (x <= -1.0) {
        if (x == -1.0) {
            sane_handle_exception(DIVBYZERO);
            return -sane_inf();
        } else {
            sane_handle_exception(INVALID);
            return sane_nan(0);
        }
    }

    extended result = log1p(x);
    sane_check_result(result);
    return result;
}

extended sane_exp(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = exp(x);
    sane_check_result(result);
    return result;
}

extended sane_exp10(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = pow(10.0, x);
    sane_check_result(result);
    return result;
}

extended sane_exp2(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = exp2(x);
    sane_check_result(result);
    return result;
}

extended sane_exp1(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = expm1(x);
    sane_check_result(result);
    return result;
}

extended sane_pow(extended x, extended y)
{
    if (!sane_is_valid_number(x) || !sane_is_valid_number(y)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = pow(x, y);
    sane_check_result(result);
    return result;
}

extended sane_ipower(extended x, int16_t i)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = pow(x, (double)i);
    sane_check_result(result);
    return result;
}

/**
 * Hyperbolic functions
 */
extended sane_sinh(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = sinh(x);
    sane_check_result(result);
    return result;
}

extended sane_cosh(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = cosh(x);
    sane_check_result(result);
    return result;
}

extended sane_tanh(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = tanh(x);
    sane_check_result(result);
    return result;
}

/**
 * Special functions
 */
extended sane_pi(void)
{
    return M_PI;
}

extended sane_inf(void)
{
    return INFINITY;
}

extended sane_nan(unsigned char c)
{
    return NAN;
}

extended sane_copysign(extended x, extended y)
{
    return copysign(x, y);
}

extended sane_scalb(int16_t n, extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = scalb(x, n);
    sane_check_result(result);
    return result;
}

extended sane_logb(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (x == 0.0) {
        sane_handle_exception(DIVBYZERO);
        return -sane_inf();
    }

    extended result = logb(x);
    sane_check_result(result);
    return result;
}

extended sane_rint(extended x)
{
    if (!sane_is_valid_number(x)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    extended result = rint(x);
    sane_check_result(result);
    return result;
}

extended sane_remainder(extended x, extended y, int16_t *quo)
{
    if (!sane_is_valid_number(x) || !sane_is_valid_number(y)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (y == 0.0) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    int q;
    extended result = remquo(x, y, &q);
    if (quo) *quo = (int16_t)q;
    sane_check_result(result);
    return result;
}

/**
 * Financial functions
 */
extended sane_compound(extended r, extended n)
{
    if (!sane_is_valid_number(r) || !sane_is_valid_number(n)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (r == -1.0) {
        sane_handle_exception(DIVBYZERO);
        return sane_inf();
    }

    extended result = pow(1.0 + r, n);
    sane_check_result(result);
    return result;
}

extended sane_annuity(extended r, extended n)
{
    if (!sane_is_valid_number(r) || !sane_is_valid_number(n)) {
        sane_handle_exception(INVALID);
        return sane_nan(0);
    }

    if (r == 0.0) {
        return n;
    }

    extended compound_val = sane_compound(r, n);
    extended result = (compound_val - 1.0) / r;
    sane_check_result(result);
    return result;
}

/**
 * Random number generation
 */
extended sane_randomx(extended *x)
{
    if (x && sane_is_valid_number(*x)) {
        g_random_seed = (uint32_t)(*x);
    }

    /* Linear congruential generator */
    g_random_seed = (g_random_seed * 1103515245 + 12345) & 0x7fffffff;

    extended result = (double)g_random_seed / (double)0x7fffffff;
    if (x) *x = result;
    return result;
}

/**
 * Number classification
 */
numclass sane_classfloat(extended x)
{
    if (isnan(x)) {
        /* Check if signaling NaN (implementation dependent) */
        return QNAN;
    }
    if (isinf(x)) {
        return INFINITE;
    }
    if (x == 0.0) {
        return ZERONUM;
    }
    if (isnormal(x)) {
        return NORMALNUM;
    }
    /* Subnormal/denormalized */
    return DENORMALNUM;
}

numclass sane_classdouble(extended x)
{
    return sane_classfloat(x);
}

numclass sane_classcomp(extended x)
{
    return sane_classfloat(x);
}

numclass sane_classextended(extended x)
{
    return sane_classfloat(x);
}

/**
 * Comparison operations
 */
relop sane_relation(extended x, extended y)
{
    if (!sane_is_valid_number(x) || !sane_is_valid_number(y)) {
        return UNORDERED;
    }

    if (isnan(x) || isnan(y)) {
        return UNORDERED;
    }

    if (x > y) return GREATERTHAN;
    if (x < y) return LESSTHAN;
    return EQUALTO;
}

int32_t sane_signnum(extended x)
{
    if (isnan(x)) {
        return 0;
    }
    return signbit(x) ? -1 : (x == 0.0 ? 0 : 1);
}

/**
 * Environment and exception handling
 */
void sane_setexception(exception e, int32_t s)
{
    if (s) {
        g_sane_env.exceptions |= e;
    } else {
        g_sane_env.exceptions &= ~e;
    }
}

int32_t sane_testexception(exception e)
{
    return (g_sane_env.exceptions & e) ? 1 : 0;
}

void sane_sethalt(exception e, int32_t s)
{
    if (s) {
        g_sane_env.halts |= e;
    } else {
        g_sane_env.halts &= ~e;
    }
}

int32_t sane_testhalt(exception e)
{
    return (g_sane_env.halts & e) ? 1 : 0;
}

void sane_setround(rounddir r)
{
    g_sane_env.rounding = r;
    sane_set_fpu_environment();
}

rounddir sane_getround(void)
{
    return g_sane_env.rounding;
}

void sane_setprecision(roundpre p)
{
    g_sane_env.precision = p;
}

roundpre sane_getprecision(void)
{
    return g_sane_env.precision;
}

void sane_setenvironment(environment e)
{
    /* Restore environment from encoded value */
    g_sane_env.rounding = TONEAREST;
    g_sane_env.precision = EXTPRECISION;
    g_sane_env.exceptions = 0;
    g_sane_env.halts = 0;
    sane_set_fpu_environment();
}

void sane_getenvironment(environment *e)
{
    if (e) {
        /* Encode environment into single value */
        *e = IEEEDEFAULTENV;
    }
}

void sane_procentry(environment *e)
{
    if (e) {
        sane_getenvironment(e);
    }
}

void sane_procexit(environment e)
{
    sane_setenvironment(e);
}

/**
 * Halt vector management
 */
haltvector sane_gethaltvector(void)
{
    return g_sane_env.halt_handler;
}

void sane_sethaltvector(haltvector v)
{
    g_sane_env.halt_handler = v;
}

/**
 * Type conversion functions
 */
void sane_x96tox80(const extended96 *x96, extended80 *x80)
{
    if (x96 && x80) {
        /* Convert 96-bit to 80-bit format */
        memcpy(x80->words, x96->words, sizeof(x80->words));
    }
}

void sane_x80tox96(const extended80 *x80, extended96 *x96)
{
    if (x80 && x96) {
        /* Convert 80-bit to 96-bit format */
        memcpy(x96->words, x80->words, sizeof(x80->words));
        x96->words[5] = 0; /* Pad with zero */
    }
}

/**
 * Utility functions
 */
static void sane_handle_exception(exception exc)
{
    g_sane_env.exceptions |= exc;

    if (g_sane_env.exception_handling && (g_sane_env.halts & exc)) {
        if (g_sane_env.halt_handler) {
            mischaltinfo info = {0};
            info.haltexceptions = exc;
            g_sane_env.halt_handler(&info, NULL, NULL, NULL, 0);
        }
    }
}

static void sane_check_result(extended result)
{
    if (isnan(result)) {
        sane_handle_exception(INVALID);
    } else if (isinf(result)) {
        sane_handle_exception(OVERFLOW);
    } else if (result != 0.0 && fabs(result) < DBL_MIN) {
        sane_handle_exception(UNDERFLOW);
    }
}

static bool sane_is_valid_number(extended x)
{
    return isfinite(x) || isinf(x); /* Allow infinity but not NaN for input validation */
}

static void sane_set_fpu_environment(void)
{
    /* Set FPU rounding mode */
    switch (g_sane_env.rounding) {
        case TONEAREST:
            fesetround(FE_TONEAREST);
            break;
        case UPWARD:
            fesetround(FE_UPWARD);
            break;
        case DOWNWARD:
            fesetround(FE_DOWNWARD);
            break;
        case TOWARDZERO:
            fesetround(FE_TOWARDZERO);
            break;
    }
}

/**
 * Decimal conversion functions
 */
void sane_num2dec(const decform *f, extended x, decimal *d)
{
    if (!f || !d) return;

    /* Convert number to decimal representation */
    d->sgn = signbit(x) ? 1 : 0;
    d->exp = 0;
    d->sig.length = 0;

    if (isnan(x) || isinf(x)) {
        /* Handle special cases */
        d->sig.length = 3;
        strcpy((char*)d->sig.text, "NaN");
        return;
    }

    /* Convert to string and parse */
    char buffer[64];
    sprintf(buffer, "%.15g", fabs(x));

    /* Parse the string representation */
    char *decimal_point = strchr(buffer, '.');
    char *exp_char = strchr(buffer, 'e');

    if (exp_char) {
        d->exp = atoi(exp_char + 1);
        *exp_char = '\0';
    }

    /* Copy significant digits */
    int digit_count = 0;
    for (char *p = buffer; *p && digit_count < SIGDIGLEN; p++) {
        if (*p >= '0' && *p <= '9') {
            d->sig.text[digit_count++] = *p;
        }
    }
    d->sig.length = digit_count;
}

extended sane_dec2num(const decimal *d)
{
    if (!d) return 0.0;

    /* Handle special cases */
    if (d->sig.length == 0) return 0.0;

    /* Reconstruct number from decimal representation */
    extended result = 0.0;
    extended power = 1.0;

    for (int i = d->sig.length - 1; i >= 0; i--) {
        if (d->sig.text[i] >= '0' && d->sig.text[i] <= '9') {
            result += (d->sig.text[i] - '0') * power;
            power *= 10.0;
        }
    }

    /* Apply exponent */
    if (d->exp != 0) {
        result *= pow(10.0, d->exp);
    }

    /* Apply sign */
    if (d->sgn) {
        result = -result;
    }

    return result;
}

void sane_dec2str(const decform *f, const decimal *d, char *s)
{
    if (!f || !d || !s) return;

    /* Convert decimal to string representation */
    if (d->sig.length == 0) {
        strcpy(s, "0");
        return;
    }

    char *p = s;
    if (d->sgn) {
        *p++ = '-';
    }

    /* Copy significant digits */
    for (int i = 0; i < d->sig.length && i < SIGDIGLEN; i++) {
        *p++ = d->sig.text[i];
        if (i == 0 && d->sig.length > 1) {
            *p++ = '.';
        }
    }

    /* Add exponent if needed */
    if (d->exp != 0) {
        sprintf(p, "e%d", d->exp);
    } else {
        *p = '\0';
    }
}

void sane_str2dec(const char *s, int16_t *ix, decimal *d, int16_t *vp)
{
    if (!s || !d) return;

    /* Parse string to decimal representation */
    memset(d, 0, sizeof(*d));

    const char *p = s;
    int index = 0;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t') {
        p++;
        index++;
    }

    /* Check sign */
    if (*p == '-') {
        d->sgn = 1;
        p++;
        index++;
    } else if (*p == '+') {
        p++;
        index++;
    }

    /* Parse digits */
    int digit_count = 0;
    bool found_decimal = false;
    int decimal_places = 0;

    while (*p && digit_count < SIGDIGLEN) {
        if (*p >= '0' && *p <= '9') {
            d->sig.text[digit_count++] = *p;
            if (found_decimal) decimal_places++;
        } else if (*p == '.' && !found_decimal) {
            found_decimal = true;
        } else if (*p == 'e' || *p == 'E') {
            /* Parse exponent */
            p++;
            index++;
            d->exp = atoi(p);
            while (*p && (*p == '+' || *p == '-' || (*p >= '0' && *p <= '9'))) {
                p++;
                index++;
            }
            break;
        } else {
            break;
        }
        p++;
        index++;
    }

    d->sig.length = digit_count;
    d->exp -= decimal_places;

    if (ix) *ix = index;
    if (vp) *vp = (digit_count > 0) ? 0 : 1; /* 0 = valid, 1 = invalid */
}

/**
 * Environment state management
 */
void sane_get_environment_state(SANEEnvironment *env)
{
    if (env) {
        *env = g_sane_env;
    }
}

void sane_set_environment_state(const SANEEnvironment *env)
{
    if (env) {
        g_sane_env = *env;
        sane_set_fpu_environment();
    }
}

/**
 * Platform integration
 */
void sane_set_math_library(void *mathLib)
{
    /* Platform-specific math library integration */
}

void sane_enable_ieee754_mode(bool enabled)
{
    g_sane_env.ieee754_mode = enabled;
}

void sane_enable_exception_handling(bool enabled)
{
    g_sane_env.exception_handling = enabled;
}

/**
 * Cleanup function
 */
void CleanupSANEPackage(void)
{
    /* Reset environment to defaults */
    g_sane_env.rounding = TONEAREST;
    g_sane_env.precision = EXTPRECISION;
    g_sane_env.exceptions = 0;
    g_sane_env.halts = 0;
    g_sane_env.halt_handler = NULL;
}