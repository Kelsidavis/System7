/*
 * System71Math.c - the C math functions the Toolbox uses
 *
 * QuickDraw's arcs, the Calculator and SANE (Pack4) call these. They were in
 * sys71_stubs.c as short Taylor series with no argument reduction worth the
 * name: cos was out by 0.026 near pi, atan by 0.03 near 1, and exp lost all
 * accuracy past |x| of about 10.
 *
 * These follow fdlibm (Sun Microsystems, freely distributable), which most C
 * libraries descend from: reduce the argument to a small interval, evaluate
 * a minimax polynomial there, and undo the reduction. The polynomial
 * coefficients and the atan tables are fdlibm's. Accuracy is within an ulp or
 * two over the ranges the Toolbox uses; sin and cos reduce exactly enough for
 * |x| up to about 1e9 and approximately beyond.
 *
 * IEEE-754 doubles are assumed, as on every platform this builds for.
 */

#include <stdint.h>
#include "math.h"

static uint64_t bits_of(double x) {
    union { double d; uint64_t u; } v = { x };
    return v.u;
}

static double double_of(uint64_t u) {
    union { uint64_t u; double d; } v = { u };
    return v.d;
}

static double make_nan(void) { return double_of(0x7FF8000000000000ULL); }
static double make_inf(void) { return double_of(0x7FF0000000000000ULL); }

int isnan(double x) {
    return x != x;
}

double fabs(double x) {
    return double_of(bits_of(x) & 0x7FFFFFFFFFFFFFFFULL);
}

double floor(double x) {
    if (x != x || fabs(x) >= 4503599627370496.0) return x;   /* NaN, inf, or already whole: 2^52 */
    double t = (double)(int64_t)x;
    return (t > x) ? t - 1.0 : t;
}

/* x * 2^n, stepping through the exponent range so no intermediate overflows. */
static double scale2(double x, int n) {
    while (n > 1023) { x *= 8.98846567431157953865e307; n -= 1023; }  /* 2^1023 */
    while (n < -1022) { x *= 2.22507385850720138309e-308; n += 1022; } /* 2^-1022 */
    return x * double_of((uint64_t)(n + 1023) << 52);
}

/* Newton's method from an estimate got by halving the exponent. */
double sqrt(double x) {
    if (x != x || x < 0.0) return make_nan();
    if (x == 0.0 || x == make_inf()) return x;
    double g = double_of((bits_of(x) >> 1) + 0x1FF8000000000000ULL);
    for (int i = 0; i < 6; i++) g = 0.5 * (g + x / g);
    return g;
}

/* ---- sin, cos, tan ----------------------------------------------------- */

/* Kernels on [-pi/4, pi/4]. */
static double ksin(double x) {
    const double S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03,
                 S3 = -1.98412698298579493134e-04, S4 = 2.75573137070700676789e-06,
                 S5 = -2.50507602534068634195e-08, S6 = 1.58969099521155010221e-10;
    double z = x * x;
    return x + x * z * (S1 + z * (S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)))));
}

static double kcos(double x) {
    const double C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03,
                 C3 = 2.48015872894767294178e-05, C4 = -2.75573143513906633035e-07,
                 C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11;
    double z = x * x;
    double r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
    double hz = 0.5 * z;
    double w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + z * r);
}

/* x - n*pi/2 for the nearest n, with pi/2 carried in three parts so the
 * subtraction keeps its precision; *quadrant gets n mod 4. */
static double reduce_pio2(double x, int* quadrant) {
    const double invpio2 = 6.36619772367581382433e-01;
    const double pio2_1 = 1.57079632673412561417e+00;
    const double pio2_2 = 6.07710050630396597660e-11;
    const double pio2_3 = 2.02226624871116645580e-21;
    double fn = floor(x * invpio2 + 0.5);
    double r = ((x - fn * pio2_1) - fn * pio2_2) - fn * pio2_3;
    int64_t n = (int64_t)fn;
    *quadrant = (int)(n & 3);
    return r;
}

double sin(double x) {
    if (x != x || fabs(x) == make_inf()) return make_nan();
    if (fabs(x) <= 7.85398163397448278999e-01) return ksin(x);
    int q;
    double r = reduce_pio2(x, &q);
    switch (q) {
        case 0:  return ksin(r);
        case 1:  return kcos(r);
        case 2:  return -ksin(r);
        default: return -kcos(r);
    }
}

double cos(double x) {
    if (x != x || fabs(x) == make_inf()) return make_nan();
    if (fabs(x) <= 7.85398163397448278999e-01) return kcos(x);
    int q;
    double r = reduce_pio2(x, &q);
    switch (q) {
        case 0:  return kcos(r);
        case 1:  return -ksin(r);
        case 2:  return -kcos(r);
        default: return ksin(r);
    }
}

double tan(double x) {
    return sin(x) / cos(x);
}

/* ---- atan, atan2, asin, acos -------------------------------------------- */

double atan(double x) {
    static const double atanhi[] = {
        4.63647609000806093515e-01, 7.85398163397448278999e-01,
        9.82793723247329054082e-01, 1.57079632679489655800e+00 };
    static const double atanlo[] = {
        2.26987774529616870924e-17, 3.06161699786838301793e-17,
        1.39033110312309984516e-17, 6.12323399573676603587e-17 };
    static const double aT[] = {
        3.33333333333329318027e-01, -1.99999999998764832476e-01,
        1.42857142725034663711e-01, -1.11111104054623557880e-01,
        9.09088713343650656196e-02, -7.69187620504482999495e-02,
        6.66107313738753120669e-02, -5.83357013379057348645e-02,
        4.97687799461593236017e-02, -3.65315727442169155270e-02,
        1.62858201153657823623e-02 };

    if (x != x) return x;
    int negative = x < 0.0;
    double ax = fabs(x);
    if (ax >= 7.37869762948382064640e+19) {           /* 2^66: atan is pi/2 to the last bit */
        double r = atanhi[3] + atanlo[3];
        return negative ? -r : r;
    }

    int id;
    if (ax < 0.4375) {
        id = -1;
    } else if (ax < 1.1875) {
        if (ax < 0.6875) { id = 0; ax = (2.0 * ax - 1.0) / (2.0 + ax); }
        else             { id = 1; ax = (ax - 1.0) / (ax + 1.0); }
    } else if (ax < 2.4375) {
        id = 2; ax = (ax - 1.5) / (1.0 + 1.5 * ax);
    } else {
        id = 3; ax = -1.0 / ax;
    }

    double z = ax * ax;
    double w = z * z;
    double s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    double s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    double r;
    if (id < 0) {
        r = ax - ax * (s1 + s2);
    } else {
        r = atanhi[id] - ((ax * (s1 + s2) - atanlo[id]) - ax);
    }
    return negative ? -r : r;
}

double atan2(double y, double x) {
    const double pi = 3.14159265358979311600e+00;
    const double pi_2 = 1.57079632679489655800e+00;
    if (x != x || y != y) return make_nan();
    if (y == 0.0) {
        if (x > 0.0 || (x == 0.0 && !(bits_of(x) >> 63))) return y;  /* +x or +0: keeps y's sign */
        return (bits_of(y) >> 63) ? -pi : pi;
    }
    if (x == 0.0) return (y > 0.0) ? pi_2 : -pi_2;

    double a = atan(fabs(y / x));
    if (x < 0.0) a = pi - a;
    return (y < 0.0) ? -a : a;
}

double asin(double x) {
    if (x != x || fabs(x) > 1.0) return make_nan();
    return atan2(x, sqrt((1.0 - x) * (1.0 + x)));
}

double acos(double x) {
    if (x != x || fabs(x) > 1.0) return make_nan();
    return atan2(sqrt((1.0 - x) * (1.0 + x)), x);
}

/* ---- exp, log, log10 ----------------------------------------------------- */

static const double ln2_hi = 6.93147180369123816490e-01;
static const double ln2_lo = 1.90821492927058770002e-10;

double exp(double x) {
    if (x != x) return x;
    if (x > 7.09782712893383973096e+02) return make_inf();
    if (x < -7.45133219101941108420e+02) return 0.0;

    /* x = k*ln2 + r with |r| <= ln2/2, so e^x = 2^k * e^r. */
    double k = floor(x * 1.44269504088896338700e+00 + 0.5);
    double r = (x - k * ln2_hi) - k * ln2_lo;

    /* Taylor on |r| <= 0.347: the 14th term is below an ulp. */
    double sum = 1.0, term = 1.0;
    for (int n = 1; n <= 14; n++) {
        term *= r / (double)n;
        sum += term;
    }
    return scale2(sum, (int)k);
}

double log(double x) {
    if (x != x || x < 0.0) return make_nan();
    if (x == 0.0) return -make_inf();
    if (x == make_inf()) return x;

    /* x = m * 2^e with m in [sqrt(1/2), sqrt(2)). */
    int e = 0;
    if (x < 2.2250738585072014e-308) {           /* subnormal: bring it into range */
        x *= 18014398509481984.0;                  /* 2^54 */
        e = -54;
    }
    uint64_t u = bits_of(x);
    e += (int)((u >> 52) & 0x7FF) - 1023;
    double m = double_of((u & 0x000FFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL);
    if (m > 1.41421356237309504880) { m *= 0.5; e++; }

    /* log(m) = 2*atanh(f) with f = (m-1)/(m+1), |f| <= 0.172. */
    double f = (m - 1.0) / (m + 1.0);
    double f2 = f * f;
    double sum = 0.0, power = f;
    for (int n = 0; n < 13; n++) {
        sum += power / (double)(2 * n + 1);
        power *= f2;
    }
    return (double)e * ln2_hi + (2.0 * sum + (double)e * ln2_lo);
}

double log10(double x) {
    return log(x) * 4.34294481903251816668e-01;   /* 1 / ln 10 */
}

/* ---- ceil, pow ------------------------------------------------------------ */

double ceil(double x) {
    return -floor(-x);
}

/*
 * Whole exponents up to 64 in size are multiplied out exactly, by squaring;
 * anything else is exp(y * log|x|), good to a few ulp for results of
 * ordinary size. A negative base takes only a whole exponent.
 */
double pow(double x, double y) {
    if (y == 0.0) return 1.0;
    if (x == 1.0) return 1.0;
    if (x != x || y != y) return make_nan();

    int yIsWhole = (floor(y) == y);
    int yIsOdd = yIsWhole && fabs(y) < 9007199254740992.0 &&
                 ((int64_t)y & 1);

    if (x == 0.0) {
        int negZero = (int)(bits_of(x) >> 63);
        if (y < 0.0) return (negZero && yIsOdd) ? -make_inf() : make_inf();
        return (negZero && yIsOdd) ? -0.0 : 0.0;
    }
    if (x < 0.0 && !yIsWhole) return make_nan();

    if (yIsWhole && fabs(y) <= 64.0) {
        double base = fabs(x), result = 1.0;
        int64_t n = (int64_t)fabs(y);
        while (n) {
            if (n & 1) result *= base;
            base *= base;
            n >>= 1;
        }
        if (y < 0.0) result = 1.0 / result;
        return (x < 0.0 && yIsOdd) ? -result : result;
    }

    double r = exp(y * log(fabs(x)));
    return (x < 0.0 && yIsOdd) ? -r : r;
}
