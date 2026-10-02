/*
 * SANENumbers.c - SANE's number formats and decimal conversions, in integers
 *
 * See SANENumbers.h. Everything here is exact: a decimal string becomes the
 * binary number nearest it (in the rounding direction asked for), and a
 * binary number becomes the decimal digits nearest it, however many the
 * conversion has to look at to know. That takes integers wider than any
 * machine word - an extended's exponent reaches 2^16383 - so there is a
 * small big-integer arithmetic below, used only for the conversions.
 */

#include "SANENumbers.h"
#include <string.h>

#define TOP64 0x8000000000000000ULL
#define QUIET 0x4000000000000000ULL

static uint64_t be64(const uint8_t* b) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | b[i];
    return v;
}
static void put_be64(uint8_t* b, uint64_t v) {
    for (int i = 7; i >= 0; i--) { b[i] = (uint8_t)v; v >>= 8; }
}
static uint32_t be32(const uint8_t* b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}
static void put_be32(uint8_t* b, uint32_t v) {
    b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16); b[2] = (uint8_t)(v >> 8); b[3] = (uint8_t)v;
}

static int msb64(uint64_t v) {          /* index of the highest set bit; v != 0 */
    int n = 0;
    while (v >>= 1) n++;
    return n;
}

int SANE_FormatSize(int format) {
    switch (format) {
        case kSANEExtended: return 10;
        case kSANEDouble:   return 8;
        case kSANESingle:   return 4;
        case kSANEInteger:  return 2;
        case kSANELongInt:  return 4;
        case kSANEComp:     return 8;
        default:            return 0;
    }
}

SANEExt SANE_NaN(int code) {
    SANEExt x = { 0, 0x7FFF, TOP64 | QUIET | ((uint64_t)(code & 0xFF) << 48) };
    return x;
}

int SANE_NaNCode(SANEExt x) {
    return (int)((x.mant >> 48) & 0xFF);
}

/* An integer of either sign as an extended */
static SANEExt from_int(int64_t v) {
    SANEExt x = { v < 0, 0, 0 };
    if (v == 0) return x;
    uint64_t m = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
    int p = msb64(m);
    x.exp = (uint16_t)(16383 + p);
    x.mant = m << (63 - p);
    return x;
}

/* A binary IEEE format (double or single) as an extended. fracBits is the
 * width of its fraction, bias its exponent bias. */
static SANEExt from_ieee(uint64_t u, int fracBits, int expBits, int bias) {
    SANEExt x;
    x.sign = (int)(u >> (fracBits + expBits));
    int e = (int)((u >> fracBits) & ((1u << expBits) - 1));
    uint64_t f = u & (((uint64_t)1 << fracBits) - 1);
    if (e == (1 << expBits) - 1) {                  /* infinity or NaN */
        x.exp = 0x7FFF;
        x.mant = TOP64 | (f << (63 - fracBits));
    } else if (e == 0) {
        if (f == 0) { x.exp = 0; x.mant = 0; }
        else {                                       /* denormal: f * 2^(1-bias-fracBits) */
            int p = msb64(f);
            x.exp = (uint16_t)(p + 1 - bias - fracBits + 16383);
            x.mant = f << (63 - p);
        }
    } else {
        x.exp = (uint16_t)(e - bias + 16383);
        x.mant = TOP64 | (f << (63 - fracBits));
    }
    return x;
}

SANEExt SANE_ToExtended(int format, const uint8_t* b) {
    SANEExt x = { 0, 0, 0 };
    switch (format) {
        case kSANEExtended:
            x.sign = b[0] >> 7;
            x.exp = (uint16_t)(((b[0] & 0x7F) << 8) | b[1]);
            x.mant = be64(b + 2);
            return x;
        case kSANEDouble:  return from_ieee(be64(b), 52, 11, 1023);
        case kSANESingle:  return from_ieee(be32(b), 23, 8, 127);
        case kSANEInteger: return from_int((int16_t)((b[0] << 8) | b[1]));
        case kSANELongInt: return from_int((int32_t)be32(b));
        case kSANEComp: {
            uint64_t v = be64(b);
            if (v == TOP64) return SANE_NaN(kNaNComp);   /* comp's one NaN */
            return from_int((int64_t)v);
        }
        default: return x;
    }
}

void SANE_ExtToBytes(SANEExt x, uint8_t b[10]) {
    b[0] = (uint8_t)((x.sign ? 0x80 : 0) | ((x.exp >> 8) & 0x7F));
    b[1] = (uint8_t)x.exp;
    put_be64(b + 2, x.mant);
}

int SANE_Classify(int format, const uint8_t* bytes) {
    SANEExt x = SANE_ToExtended(format, bytes);
    int c;
    if (x.exp == 0x7FFF) {
        if ((x.mant << 1) == 0) c = kSANEInfinite;
        else c = (x.mant & QUIET) ? kSANEQNaN : kSANESNaN;
    } else if (x.mant == 0) {
        c = kSANEZero;
    } else if (format == kSANEExtended && x.exp == 0) {
        c = kSANEDenormal;
    } else if (format == kSANEDouble && x.exp < 16383 - 1022) {
        c = kSANEDenormal;
    } else if (format == kSANESingle && x.exp < 16383 - 126) {
        c = kSANEDenormal;
    } else {
        c = kSANENormal;
    }
    /* Every format keeps its sign in the first bit */
    int negative = (format == kSANEExtended || format == kSANEDouble || format == kSANESingle)
                   ? (bytes[0] >> 7) : x.sign;
    return negative ? -c : c;
}

/* ------------------------------------------------------------------------
 * Rounding an extended to fewer bits
 * ------------------------------------------------------------------------ */

static int is_nan(SANEExt x) { return x.exp == 0x7FFF && (x.mant << 1) != 0; }
static int is_inf(SANEExt x) { return x.exp == 0x7FFF && (x.mant << 1) == 0; }

/* Should a magnitude round up, given the bit below it and whether anything
 * below that is set? */
static int round_up(int round, int sign, int lsb, int roundBit, int sticky) {
    switch (round) {
        case kSANEToNearest:  return roundBit && (sticky || lsb);
        case kSANEUpward:     return (roundBit || sticky) && !sign;
        case kSANEDownward:   return (roundBit || sticky) && sign;
        default:              return 0;
    }
}

/* An extended to an IEEE format of fracBits fraction bits: its bit pattern,
 * and the exceptions. */
static uint64_t to_ieee(SANEExt x, int fracBits, int expBits, int bias, int round, int* exc) {
    uint64_t signBit = (uint64_t)x.sign << (fracBits + expBits);
    uint64_t expMax = ((uint64_t)1 << expBits) - 1;
    if (is_nan(x)) {
        if (!(x.mant & QUIET)) *exc |= kSANEInvalid;          /* signalling: quieted */
        uint64_t f = (x.mant << 1) >> (64 - fracBits);         /* the leading fraction bits */
        f |= (uint64_t)1 << (fracBits - 1);
        return signBit | (expMax << fracBits) | f;
    }
    if (is_inf(x)) return signBit | (expMax << fracBits);
    if (x.mant == 0) return signBit;

    /* Normalise: unbiased exponent of the leading bit */
    int lead = msb64(x.mant);
    int e = (x.exp ? x.exp : 1) - 16383 - (63 - lead);
    uint64_t m = x.mant << (63 - lead);                       /* leading bit at 63 */

    int emin = 1 - bias;
    int keep = fracBits + 1;                                  /* bits kept, with the leading one */
    if (e < emin) keep -= emin - e;                           /* a denormal keeps fewer */
    uint64_t q;
    int roundBit, sticky;
    if (keep <= 0) {
        q = 0;
        roundBit = keep == 0;
        sticky = keep == 0 ? (m << 1) != 0 : 1;
    } else {
        int drop = 64 - keep;
        q = m >> drop;
        roundBit = (int)((m >> (drop - 1)) & 1);
        sticky = (m & ((((uint64_t)1) << (drop - 1)) - 1)) != 0;
    }
    int inexact = roundBit || sticky;
    if (round_up(round, x.sign, (int)(q & 1), roundBit, sticky)) q++;
    if (inexact) *exc |= kSANEInexact;

    if (e < emin) {
        /* A denormal: q is the fraction field as it stands, and a carry
         * into the leading bit's place makes it the smallest normal */
        if (inexact) *exc |= kSANEUnderflow;
        return signBit | q;
    }
    if (q >> (fracBits + 1)) { q >>= 1; e++; }                /* rounded up past a power of two */
    if (e + bias >= (int)expMax) {
        *exc |= kSANEOverflow | kSANEInexact;
        int toInf = round == kSANEToNearest || (round == kSANEUpward && !x.sign) ||
                    (round == kSANEDownward && x.sign);
        if (toInf) return signBit | (expMax << fracBits);
        return signBit | ((expMax - 1) << fracBits) | (((uint64_t)1 << fracBits) - 1);
    }
    return signBit | ((uint64_t)(e + bias) << fracBits) | (q & (((uint64_t)1 << fracBits) - 1));
}

/* An extended rounded to an integer, as a magnitude; *ok is false if the
 * magnitude does not fit in 64 bits. */
static uint64_t to_integer(SANEExt x, int round, int* exc, int* ok) {
    *ok = 1;
    if (x.mant == 0) return 0;
    int e = (x.exp ? x.exp : 1) - 16383;                      /* value = mant * 2^(e-63) */
    if (e >= 64) { *ok = 0; return 0; }
    uint64_t q;
    int roundBit, sticky;
    if (e >= 63) {
        q = x.mant;
        roundBit = 0;
        sticky = 0;
    } else if (e >= 0) {
        int drop = 63 - e;
        q = x.mant >> drop;
        roundBit = (int)((x.mant >> (drop - 1)) & 1);
        sticky = (x.mant & ((((uint64_t)1) << (drop - 1)) - 1)) != 0;
    } else {
        q = 0;
        roundBit = e == -1 && (x.mant & TOP64);
        sticky = e == -1 ? (x.mant << 1) != 0 : 1;
    }
    if (roundBit || sticky) *exc |= kSANEInexact;
    if (round_up(round, x.sign, (int)(q & 1), roundBit, sticky)) {
        q++;
        if (q == 0) *ok = 0;
    }
    return q;
}

int SANE_FromExtended(SANEExt x, int format, int round, uint8_t* bytes) {
    int exc = 0;
    switch (format) {
        case kSANEExtended:
            SANE_ExtToBytes(x, bytes);
            return 0;
        case kSANEDouble:
            put_be64(bytes, to_ieee(x, 52, 11, 1023, round, &exc));
            return exc;
        case kSANESingle:
            put_be32(bytes, (uint32_t)to_ieee(x, 23, 8, 127, round, &exc));
            return exc;
        default: break;
    }
    /* The integer formats: out of range or NaN is invalid, and gives the
     * most negative value - which for comp is its NaN */
    int ok = !is_nan(x) && !is_inf(x);
    uint64_t q = ok ? to_integer(x, round, &exc, &ok) : 0;
    uint64_t limit = format == kSANEInteger ? 0x8000u : format == kSANELongInt ? 0x80000000u : TOP64;
    if (ok && (q > limit || (q == limit && (!x.sign || format == kSANEComp)))) ok = 0;
    uint64_t v = !ok ? limit : (x.sign ? (uint64_t)0 - q : q);
    if (!ok) exc = kSANEInvalid;
    if (format == kSANEInteger) { bytes[0] = (uint8_t)(v >> 8); bytes[1] = (uint8_t)v; }
    else if (format == kSANELongInt) put_be32(bytes, (uint32_t)v);
    else put_be64(bytes, v);
    return exc;
}

/* ------------------------------------------------------------------------
 * Next-after
 * ------------------------------------------------------------------------ */

/* -1, 0, 1 for a < b, a == b, a > b; neither a NaN */
static int ext_compare(SANEExt a, SANEExt b) {
    int az = a.mant == 0, bz = b.mant == 0;
    if (az && bz) return 0;
    int as = az ? 0 : (a.sign ? -1 : 1), bs = bz ? 0 : (b.sign ? -1 : 1);
    if (as != bs) return as < bs ? -1 : 1;
    /* Same sign: compare magnitudes, normalised so that unnormals and
     * denormals line up with normals */
    int ae = (a.exp ? a.exp : 1) - (63 - msb64(a.mant)), be = (b.exp ? b.exp : 1) - (63 - msb64(b.mant));
    uint64_t am = a.mant << (63 - msb64(a.mant)), bm = b.mant << (63 - msb64(b.mant));
    int mag = ae != be ? (ae < be ? -1 : 1) : am != bm ? (am < bm ? -1 : 1) : 0;
    return as < 0 ? -mag : mag;
}

int SANE_Compare(SANEExt a, SANEExt b, int* signalling) {
    *signalling = (is_nan(a) && !(a.mant & QUIET)) || (is_nan(b) && !(b.mant & QUIET));
    if (is_nan(a) || is_nan(b)) return 2;
    return ext_compare(a, b);
}

int SANE_NextAfter(int format, uint8_t* xb, const uint8_t* yb) {
    SANEExt x = SANE_ToExtended(format, xb), y = SANE_ToExtended(format, yb);
    if (is_nan(x)) return x.mant & QUIET ? 0 : kSANEInvalid;
    if (is_nan(y)) {
        /* The NaN is the answer */
        int n = SANE_FormatSize(format);
        for (int i = 0; i < n; i++) xb[i] = yb[i];
        return y.mant & QUIET ? 0 : kSANEInvalid;
    }
    int c = ext_compare(x, y);
    if (c == 0) return 0;
    int away = (c < 0) == !x.sign || x.mant == 0;             /* magnitude grows */
    int exc = 0;

    if (format == kSANEExtended) {
        if (x.mant == 0) {                                     /* the smallest denormal, toward y */
            x.sign = y.sign;
            x.exp = 0;
            x.mant = 1;
        } else if (away) {
            if (x.exp == 0) {
                x.mant++;
                if (x.mant == TOP64) x.exp = 1;
            } else if (++x.mant == 0) {
                x.mant = TOP64;
                x.exp++;
            }
        } else {
            if (x.exp == 0x7FFF) { x.exp = 0x7FFE; x.mant = ~(uint64_t)0; }
            else if (x.exp > 1 && x.mant == TOP64) { x.exp--; x.mant = ~(uint64_t)0; }
            else { x.mant--; if (x.exp == 1 && !(x.mant & TOP64)) x.exp = 0; }
        }
        SANE_ExtToBytes(x, xb);
        if (is_inf(x)) exc = kSANEOverflow | kSANEInexact;
        else if (x.exp == 0) exc = kSANEUnderflow | kSANEInexact;
        return exc;
    }

    /* Double and single: the bits, as sign and magnitude, are in order */
    int bits = format == kSANEDouble ? 64 : 32;
    uint64_t u = bits == 64 ? be64(xb) : be32(xb);
    uint64_t signBit = (uint64_t)1 << (bits - 1);
    uint64_t mag = u & (signBit - 1);
    if (mag == 0) u = (y.sign ? signBit : 0) | 1;
    else u = (u & signBit) | (away ? mag + 1 : mag - 1);
    if (bits == 64) put_be64(xb, u); else put_be32(xb, (uint32_t)u);
    int fracBits = bits == 64 ? 52 : 23;
    uint64_t e = (u & (signBit - 1)) >> fracBits;
    uint64_t emax = bits == 64 ? 0x7FF : 0xFF;
    if (e == emax) exc = kSANEOverflow | kSANEInexact;
    else if (e == 0) exc = kSANEUnderflow | kSANEInexact;
    return exc;
}

/* ------------------------------------------------------------------------
 * Big integers, for the decimal conversions
 *
 * Little-endian 32-bit limbs. The largest needed is an extended's 64-bit
 * significand times 5^16508, for the smallest denormal: 38,400 bits.
 * ------------------------------------------------------------------------ */

enum { kBigLimbs = 1210 };
typedef struct {
    int n;                              /* limbs in use; 0 is zero */
    uint32_t d[kBigLimbs];
} Big;

static Big gR, gD, gT;                  /* the conversions run one at a time */

static void big_set64(Big* a, uint64_t v) {
    a->n = 0;
    while (v) { a->d[a->n++] = (uint32_t)v; v >>= 32; }
}

static void big_mul_small(Big* a, uint32_t m) {
    uint64_t carry = 0;
    for (int i = 0; i < a->n; i++) {
        uint64_t t = (uint64_t)a->d[i] * m + carry;
        a->d[i] = (uint32_t)t;
        carry = t >> 32;
    }
    if (carry && a->n < kBigLimbs) a->d[a->n++] = (uint32_t)carry;
}

static void big_mul_pow(Big* a, uint32_t base, uint32_t chunkPow, int chunkExp, int exp) {
    while (exp >= chunkExp) { big_mul_small(a, chunkPow); exp -= chunkExp; }
    while (exp-- > 0) big_mul_small(a, base);
}

static void big_shl(Big* a, int bits) {
    if (a->n == 0 || bits == 0) return;
    int limbs = bits / 32, r = bits % 32;
    int n = a->n + limbs + 1;
    if (n > kBigLimbs) n = kBigLimbs;
    for (int i = n - 1; i >= 0; i--) {
        int src = i - limbs;
        uint32_t hi = src >= 0 && src < a->n ? a->d[src] : 0;
        uint32_t lo = src - 1 >= 0 && src - 1 < a->n ? a->d[src - 1] : 0;
        a->d[i] = r ? (hi << r) | (lo >> (32 - r)) : hi;
    }
    a->n = n;
    while (a->n && !a->d[a->n - 1]) a->n--;
}

static int big_bitlen(const Big* a) {
    if (a->n == 0) return 0;
    return (a->n - 1) * 32 + msb64(a->d[a->n - 1]) + 1;
}

static int big_bit(const Big* a, int i) {
    if (i < 0 || i / 32 >= a->n) return 0;
    return (int)((a->d[i / 32] >> (i % 32)) & 1);
}

static int big_any_below(const Big* a, int i) {    /* any of bits 0..i-1 set */
    for (int k = 0; k < i / 32 && k < a->n; k++) if (a->d[k]) return 1;
    if (i % 32 && i / 32 < a->n && (a->d[i / 32] & ((1u << (i % 32)) - 1))) return 1;
    return 0;
}

static int big_cmp(const Big* a, const Big* b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

static void big_sub(Big* a, const Big* b) {        /* a -= b, a >= b */
    int64_t borrow = 0;
    for (int i = 0; i < a->n; i++) {
        int64_t t = (int64_t)a->d[i] - (i < b->n ? b->d[i] : 0) - borrow;
        borrow = t < 0;
        a->d[i] = (uint32_t)(t + (borrow ? ((int64_t)1 << 32) : 0));
    }
    while (a->n && !a->d[a->n - 1]) a->n--;
}

static uint32_t big_divmod_small(Big* a, uint32_t v) {
    uint64_t rem = 0;
    for (int i = a->n - 1; i >= 0; i--) {
        uint64_t t = (rem << 32) | a->d[i];
        a->d[i] = (uint32_t)(t / v);
        rem = t % v;
    }
    while (a->n && !a->d[a->n - 1]) a->n--;
    return (uint32_t)rem;
}

/* The decimal digits of a, most significant first; returns their count.
 * a is consumed. */
static char gDigits[12000];
static uint32_t gChunks[1400];

static int big_to_decimal(Big* a, char* out) {
    int nc = 0;
    while (a->n && nc < 1400) gChunks[nc++] = big_divmod_small(a, 1000000000u);
    int len = 0;
    for (int i = nc - 1; i >= 0; i--) {
        char buf[9];
        uint32_t c = gChunks[i];
        for (int k = 8; k >= 0; k--) { buf[k] = (char)('0' + c % 10); c /= 10; }
        int k = 0;
        if (i == nc - 1) while (k < 8 && buf[k] == '0') k++;
        for (; k < 9; k++) out[len++] = buf[k];
    }
    return len;
}

/* ------------------------------------------------------------------------
 * Binary to decimal
 * ------------------------------------------------------------------------ */

/* Round the digit string D (L digits, value D * 10^anything) to its first n
 * digits. out gets the kept digits; returns their count, which is n, or
 * n + 1 if rounding carried into a new leading digit, or 0 if nothing is
 * kept. *inexact says whether anything was dropped. */
static int round_digits(const char* D, int L, int n, int sign, int round, char* out, int* inexact) {
    *inexact = 0;
    if (n >= L) {
        memcpy(out, D, (size_t)L);
        for (int i = L; i < n; i++) out[i] = '0';
        return n;
    }
    int keep = n > 0 ? n : 0;
    for (int i = keep; i < L && !*inexact; i++) if (D[i] != '0') *inexact = 1;
    int up;
    if (round == kSANEToNearest) {
        if (n < 0) up = 0;                                  /* below a tenth of a unit */
        else if (D[n] != '5') up = D[n] > '5';
        else {
            int rest = 0;
            for (int i = n + 1; i < L && !rest; i++) if (D[i] != '0') rest = 1;
            up = rest || (n > 0 && ((D[n - 1] - '0') & 1));
        }
    } else {
        up = round_up(round, sign, 0, *inexact, 0);
    }
    memcpy(out, D, (size_t)keep);
    if (!up) return keep;
    for (int i = keep - 1; i >= 0; i--) {
        if (out[i] != '9') { out[i]++; return keep; }
        out[i] = '0';
    }
    /* Carried out of every digit: 10^keep */
    out[0] = '1';
    for (int i = 1; i <= keep; i++) out[i] = '0';
    return keep + 1;
}

static void set_sig(SANEDecimal* d, const char* s, int n) {
    if (n > kSANESigDigLen) n = kSANESigDigLen;
    d->sig[0] = (uint8_t)n;
    memcpy(d->sig + 1, s, (size_t)n);
}

static const char kHex[] = "0123456789ABCDEF";

/* A NaN's decimal record: N, then its fraction bits in hex, left-justified */
static void set_nan_sig(SANEDecimal* d, uint64_t mant) {
    char s[17];
    uint64_t f = mant << 1;
    s[0] = 'N';
    for (int i = 0; i < 16; i++) s[1 + i] = kHex[(f >> (60 - 4 * i)) & 0xF];
    set_sig(d, s, 17);
}

int SANE_X2Dec(const SANEDecForm* form, SANEExt x, int round, SANEDecimal* out) {
    out->sgn = x.sign;
    out->exp = 0;
    if (is_inf(x)) { set_sig(out, "I", 1); return 0; }
    if (is_nan(x)) { set_nan_sig(out, x.mant); return 0; }
    if (x.mant == 0) { set_sig(out, "0", 1); return 0; }

    /* x = m * 2^e2 exactly, then as an integer N times 10^E10 */
    uint64_t m = x.mant;
    int e2 = (x.exp ? x.exp : 1) - 16383 - 63;
    while (!(m & 1)) { m >>= 1; e2++; }
    big_set64(&gR, m);
    int E10 = 0;
    if (e2 >= 0) big_shl(&gR, e2);
    else { big_mul_pow(&gR, 5, 1220703125u, 13, -e2); E10 = e2; }   /* m/2^k = m*5^k/10^k */
    int L = big_to_decimal(&gR, gDigits);

    char kept[kSANESigDigLen + 2];
    int inexact;
    if (!form->fixed) {
        int n = form->digits < 1 ? 1 : form->digits > 19 ? 19 : form->digits;
        int k = round_digits(gDigits, L, n, x.sign, round, kept, &inexact);
        int exp = E10 + L - n;
        if (k > n) { k = n; exp++; }                        /* 10^n: one digit fewer, ten times */
        set_sig(out, kept, k);
        out->exp = (int16_t)exp;
    } else {
        int f = form->digits;
        long n = (long)L + E10 + f;                        /* digits down to 10^-f */
        if (n > 19) {
            set_sig(out, "?", 1);
            return 0;
        }
        int k = round_digits(gDigits, L, (int)n, x.sign, round, kept, &inexact);
        if (k == 0) { set_sig(out, "0", 1); out->exp = 0; }
        else if (k > 19) { set_sig(out, "?", 1); return 0; }
        else { set_sig(out, kept, k); out->exp = (int16_t)-f; }
    }
    return inexact ? kSANEInexact : 0;
}

/* ------------------------------------------------------------------------
 * Decimal to binary
 * ------------------------------------------------------------------------ */

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* An overflowed result, as the rounding direction makes it */
static SANEExt overflowed(int sign, int round) {
    int toInf = round == kSANEToNearest || (round == kSANEUpward && !sign) ||
                (round == kSANEDownward && sign);
    SANEExt x = { sign, (uint16_t)(toInf ? 0x7FFF : 0x7FFE), toInf ? TOP64 : ~(uint64_t)0 };
    return x;
}

int SANE_Dec2X(const SANEDecimal* d, int round, int toOdd, SANEExt* out) {
    int sign = d->sgn != 0;
    int len = d->sig[0];
    const uint8_t* s = d->sig + 1;
    out->sign = sign;
    out->exp = 0;
    out->mant = 0;
    if (len == 0) return 0;
    if (s[0] == 'I' || s[0] == 'i') { out->exp = 0x7FFF; out->mant = TOP64; return 0; }
    if (s[0] < '0' || s[0] > '9') {
        if (s[0] != 'N' && s[0] != 'n') { *out = SANE_NaN(kNaNAsciiBin); out->sign = sign; return 0; }
        uint64_t v = 0;
        int h = 0;
        for (int i = 1; i < len && h < 16 && hexval(s[i]) >= 0; i++, h++) v = (v << 4) | (uint64_t)hexval(s[i]);
        uint64_t f = h ? v << (64 - 4 * h) : 0;
        out->exp = 0x7FFF;
        out->mant = TOP64 | (f >> 1);
        if ((out->mant << 1) == 0) out->mant |= QUIET;      /* a NaN, not an infinity */
        return 0;
    }

    /* The significand as an integer N; the number is N * 10^E */
    int i = 0;
    while (i < len && s[i] == '0') i++;
    int digits = 0;
    big_set64(&gR, 0);
    for (; i < len && s[i] >= '0' && s[i] <= '9'; i++) {
        big_mul_small(&gR, 10);
        if (gR.n == 0 && s[i] != '0') big_set64(&gR, (uint64_t)(s[i] - '0'));
        else if (s[i] != '0') {
            /* add the digit */
            uint64_t carry = (uint64_t)(s[i] - '0');
            for (int k = 0; k < gR.n && carry; k++) {
                uint64_t t = (uint64_t)gR.d[k] + carry;
                gR.d[k] = (uint32_t)t;
                carry = t >> 32;
            }
            if (carry) gR.d[gR.n++] = (uint32_t)carry;
        }
        digits++;
    }
    if (gR.n == 0) return 0;                                /* zero, signed */
    int E = d->exp;
    long magnitude = (long)digits + E;                      /* N*10^E < 10^magnitude */

    int sticky = 0, s2 = 0;                                 /* value = gR * 2^-s2, plus sticky */
    if (magnitude > 4940) {
        *out = toOdd ? (SANEExt){ sign, 0x7FFE, ~(uint64_t)0 } : overflowed(sign, round);
        return toOdd ? 0 : kSANEOverflow | kSANEInexact;
    }
    if (magnitude < -4970) {
        /* Far below the smallest denormal: nothing, with a sticky bit */
        big_set64(&gR, 1);
        s2 = 16445 + 200;
        sticky = 0;
    } else if (E >= 0) {
        big_mul_pow(&gR, 10, 1000000000u, 9, E);
    } else {
        /* N / 10^k by long division, to at least 67 bits of quotient */
        big_set64(&gD, 1);
        big_mul_pow(&gD, 10, 1000000000u, 9, -E);
        int shift = 67 + big_bitlen(&gD) - big_bitlen(&gR);
        if (shift < 0) shift = 0;
        big_shl(&gR, shift);
        s2 = shift;
        int qbits = big_bitlen(&gR) - big_bitlen(&gD) + 1;
        static Big quotient;
        Big* q = &quotient;
        q->n = (qbits + 31) / 32;
        for (int k = 0; k < q->n; k++) q->d[k] = 0;
        for (int b = qbits - 1; b >= 0; b--) {
            gT = gD;
            big_shl(&gT, b);
            if (big_cmp(&gR, &gT) >= 0) {
                big_sub(&gR, &gT);
                q->d[b / 32] |= 1u << (b % 32);
            }
        }
        while (q->n && !q->d[q->n - 1]) q->n--;
        sticky = gR.n != 0;
        gR = *q;
    }

    /* Round gR * 2^-s2 (+ sticky) to an extended */
    int bl = big_bitlen(&gR);
    long biased = (long)bl - 1 - s2 + 16383;
    int keep = 64;
    if (biased <= 0) { keep = (int)(63 + biased); biased = 0; }
    int shift = bl - keep;
    uint64_t m = 0;
    int roundBit = 0;
    if (shift <= 0) {
        for (int b = bl - 1; b >= 0; b--) m = (m << 1) | (uint64_t)big_bit(&gR, b);
        m <<= -shift;
    } else {
        for (int b = bl - 1; b >= shift && b >= 0; b--) m = (m << 1) | (uint64_t)big_bit(&gR, b);
        roundBit = big_bit(&gR, shift - 1);
        sticky |= big_any_below(&gR, shift - 1);
    }
    int inexact = roundBit || sticky;
    int exc = 0;
    if (toOdd) {
        if (inexact) m |= 1;
    } else if (round_up(round, sign, (int)(m & 1), roundBit, sticky)) {
        m++;
        if (biased > 0 && m == 0) { m = TOP64; biased++; }
    }
    if (biased == 0 && (m & TOP64)) biased = 1;             /* rounded up out of the denormals */
    if (biased >= 0x7FFF) {
        *out = toOdd ? (SANEExt){ sign, 0x7FFE, ~(uint64_t)0 } : overflowed(sign, round);
        return toOdd ? 0 : kSANEOverflow | kSANEInexact;
    }
    out->exp = (uint16_t)biased;
    out->mant = m;
    if (!toOdd && inexact) {
        exc |= kSANEInexact;
        if (biased == 0) exc |= kSANEUnderflow;
    }
    return exc;
}

/* ------------------------------------------------------------------------
 * Scanning and formatting
 * ------------------------------------------------------------------------ */

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int is_digit(int c) { return c >= '0' && c <= '9'; }

void SANE_Str2Dec(const uint8_t* s, int len, int* index, SANEDecimal* d, int* validPrefix) {
    int i = *index;
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    int sign = 0;
    if (i < len && (s[i] == '+' || s[i] == '-')) sign = s[i++] == '-';
    d->sgn = sign;
    d->exp = 0;

    /* INF and NAN(n), in any case */
    static const char kInf[] = "inf", kNan[] = "nan";
    if (i < len && (lower(s[i]) == 'i' || lower(s[i]) == 'n')) {
        const char* word = lower(s[i]) == 'i' ? kInf : kNan;
        int k = 0;
        while (k < 3 && i + k < len && lower(s[i + k]) == word[k]) k++;
        if (k < 3) {
            *validPrefix = i + k == len;
            set_nan_sig(d, SANE_NaN(kNaNAsciiBin).mant);      /* not a number */
            return;
        }
        i += 3;
        if (word == kInf) {
            set_sig(d, "I", 1);
            *index = i;
            *validPrefix = i == len;
            return;
        }
        /* NAN(n): the code counts only with its closing parenthesis */
        int code = 0, end = i, j = i + 1;
        if (i < len && s[i] == '(') {
            int c = 0;
            while (j < len && is_digit(s[j])) c = c * 10 + (s[j++] - '0');
            if (j < len && s[j] == ')') { code = c; end = j + 1; }
        }
        *index = end;
        *validPrefix = end == len || (i < len && s[i] == '(' && j == len);
        set_nan_sig(d, SANE_NaN(code).mant);
        return;
    }

    /* Digits, a point and digits, an exponent */
    char sig[kSANESigDigLen];
    int n = 0, any = 0, dropped = 0, sticky = 0;
    long exp = 0;
    int seenPoint = 0;
    for (; i < len; i++) {
        if (s[i] == '.' && !seenPoint) { seenPoint = 1; continue; }
        if (!is_digit(s[i])) break;
        any = 1;
        if (n == 0 && s[i] == '0') { if (seenPoint) exp--; continue; }   /* leading zeros */
        if (n < kSANESigDigLen - 1) { sig[n++] = (char)s[i]; if (seenPoint) exp--; }
        else { dropped = 1; if (s[i] != '0') sticky = 1; if (!seenPoint) exp++; }
    }
    if (!any) {
        *validPrefix = i == len;
        set_nan_sig(d, SANE_NaN(kNaNAsciiBin).mant);
        return;
    }
    *index = i;
    if (i < len && lower(s[i]) == 'e') {
        int j = i + 1;
        int esign = 0;
        if (j < len && (s[j] == '+' || s[j] == '-')) esign = s[j++] == '-';
        if (j < len && is_digit(s[j])) {
            long e = 0;
            while (j < len && is_digit(s[j])) { if (e < 100000) e = e * 10 + (s[j] - '0'); j++; }
            exp += esign ? -e : e;
            i = j;
            *index = i;
        } else {
            i = j;                                          /* "1e" or "1e-": not taken */
        }
    }
    *validPrefix = i == len;
    /* Digits past the 19th become one more, nonzero if any of them was:
     * enough for the conversion to round correctly */
    if (dropped) { sig[n++] = sticky ? '1' : '0'; exp--; }
    while (n > 1 && sig[n - 1] == '0') { n--; exp++; }
    if (n == 0) { set_sig(d, "0", 1); return; }
    if (exp > 32767) exp = 32767;
    if (exp < -32767) exp = -32767;
    set_sig(d, sig, n);
    d->exp = (int16_t)exp;
}

void SANE_Dec2Str(const SANEDecForm* form, const SANEDecimal* d, uint8_t out[kSANEDecStrLen + 1]) {
    char buf[400];
    int n = 0;
    int len = d->sig[0];
    const uint8_t* sig = d->sig + 1;
    char signChar = d->sgn ? '-' : (form->fixed ? 0 : ' ');

    if (len > 0 && sig[0] == '?') {
        buf[n++] = '?';
    } else if (len > 0 && (sig[0] == 'I' || sig[0] == 'N')) {
        if (signChar) buf[n++] = signChar;
        if (sig[0] == 'I') { memcpy(buf + n, "INF", 3); n += 3; }
        else {
            uint64_t v = 0;
            int h = 0;
            for (int i = 1; i < len && h < 16 && hexval(sig[i]) >= 0; i++, h++) v = (v << 4) | (uint64_t)hexval(sig[i]);
            uint64_t f = h ? v << (64 - 4 * h) : 0;
            int code = (int)((f >> 49) & 0xFF);
            memcpy(buf + n, "NAN(", 4); n += 4;
            buf[n++] = (char)('0' + code / 100);
            buf[n++] = (char)('0' + code / 10 % 10);
            buf[n++] = (char)('0' + code % 10);
            buf[n++] = ')';
        }
    } else {
        int zero = len == 0 || sig[0] == '0';
        if (!form->fixed) {
            int digits = form->digits < 1 ? 1 : form->digits > 19 ? 19 : form->digits;
            int total = zero ? digits : (len > digits ? len : digits);
            if (signChar) buf[n++] = signChar;
            buf[n++] = zero ? '0' : (char)sig[0];
            if (total > 1) buf[n++] = '.';
            for (int i = 1; i < total; i++) buf[n++] = !zero && i < len ? (char)sig[i] : '0';
            long e = zero ? 0 : (long)d->exp + len - 1;
            buf[n++] = 'e';
            buf[n++] = e < 0 ? '-' : '+';
            if (e < 0) e = -e;
            char eb[8];
            int en = 0;
            do { eb[en++] = (char)('0' + e % 10); e /= 10; } while (e);
            while (en) buf[n++] = eb[--en];
        } else {
            int f = form->digits < 0 ? 0 : form->digits;
            int exp = zero ? 0 : d->exp;
            int sigLen = zero ? 0 : len;
            int intDigits = sigLen + exp;                   /* digits before the point */
            int fracDigits = exp < 0 ? -exp : 0;
            if (fracDigits < f) fracDigits = f;
            if (signChar) buf[n++] = signChar;
            if (intDigits > 300 || fracDigits > 300) { n = 0; buf[n++] = '?'; goto done; }
            if (intDigits <= 0) buf[n++] = '0';
            for (int i = 0; i < intDigits; i++) buf[n++] = i < sigLen ? (char)sig[i] : '0';
            if (fracDigits > 0) {
                buf[n++] = '.';
                for (int i = 0; i < fracDigits; i++) {
                    int pos = intDigits + i;                /* index into sig */
                    buf[n++] = pos >= 0 && pos < sigLen ? (char)sig[pos] : '0';
                }
            }
        }
    }
done:
    if (n > kSANEDecStrLen) { n = 1; buf[0] = '?'; }
    out[0] = (uint8_t)n;
    memcpy(out + 1, buf, (size_t)n);
}
