/*
 * M68KSANE.c - _FP68K (Pack 4) and _Elems68K (Pack 5): SANE for 68K programs
 *
 * A program compiled with floating point does its arithmetic by calling
 * these: every add, every conversion, every printed number. The opword is
 * on top of the stack with the operands' addresses under it - destination,
 * then source, then a third for binary-to-decimal - and the package takes
 * them all off. The numbers stay in the program's memory, big-endian, in
 * any of six formats (Apple Numerics Manual, chapters 21-24).
 *
 * The arithmetic is done in the x87's extended format, which is SANE's own
 * - the same 64-bit significand with its explicit integer bit, the same
 * 15-bit exponent - so results are SANE's to the bit: the rounding
 * direction and precision come from the program's environment word, and
 * the exceptions it raises come back from the x87's status word. The
 * conversions and the decimal work are SANENumbers.c's, in integers.
 *
 * Halts are not taken: a program can enable them and set a halt vector,
 * and they are kept and given back, but an exception does not call it.
 *
 * Off x86 the arithmetic is the host's long double, with round-to-nearest
 * and no exception flags.
 */

#include <string.h>
#include "M68KToolboxInternal.h"
#include "CPU/M68KOpcodes.h"
#include "SANENumbers.h"
#include "System71StdLib.h"

#if defined(__i386__) || defined(__x86_64__)
#define SANE_X87 1
#else
#define SANE_X87 0
#endif

typedef long double xf;

static UInt16 gEnv;                     /* the environment word */
static UInt32 gHaltVector;

void M68KSANE_Reset(void) { gEnv = 0; gHaltVector = 0; }

static int Rounding(void) { return (gEnv >> 13) & 3; }
static void Raise(int exceptions) { gEnv |= (UInt16)(exceptions & 0x1F00); }

/* ------------------------------------------------------------------------
 * Operands in the program's memory
 * ------------------------------------------------------------------------ */

static SANEExt LoadAs(UInt32 addr, int format) {
    uint8_t b[10];
    int n = SANE_FormatSize(format);
    ReadBytes(addr, b, (UInt32)n);
    return SANE_ToExtended(format, b);
}

static SANEExt LoadExt(UInt32 addr) { return LoadAs(addr, kSANEExtended); }

static void StoreExt(UInt32 addr, SANEExt x) {
    uint8_t b[10];
    SANE_ExtToBytes(x, b);
    WriteBytes(addr, b, 10);
}

static void StoreAs(UInt32 addr, int format, SANEExt x) {
    uint8_t b[10];
    Raise(SANE_FromExtended(x, format, Rounding(), b));
    WriteBytes(addr, b, (UInt32)SANE_FormatSize(format));
}

static Boolean IsNaN(SANEExt x) { return x.exp == 0x7FFF && (x.mant << 1) != 0; }
static Boolean IsSNaN(SANEExt x) { return IsNaN(x) && !(x.mant & 0x4000000000000000ULL); }

/* ------------------------------------------------------------------------
 * The FPU, set as the environment says for the length of one operation
 * ------------------------------------------------------------------------ */

#if SANE_X87
static xf ToX(SANEExt e) {
    unsigned char b[sizeof(xf)];
    memset(b, 0, sizeof(b));
    memcpy(b, &e.mant, 8);                          /* little-endian: significand, then */
    UInt16 se = (UInt16)((e.sign ? 0x8000 : 0) | e.exp);
    memcpy(b + 8, &se, 2);                          /* sign and exponent */
    xf v;
    memcpy(&v, b, sizeof(v));
    return v;
}

static SANEExt FromX(xf v) {
    unsigned char b[sizeof(xf)];
    memcpy(b, &v, sizeof(v));
    SANEExt e;
    UInt16 se;
    memcpy(&e.mant, b, 8);
    memcpy(&se, b + 8, 2);
    e.sign = se >> 15;
    e.exp = se & 0x7FFF;
    return e;
}

static UInt16 gSavedCW;

static void FPBegin(void) {
    static const UInt16 kRC[4] = { 0x0000, 0x0800, 0x0400, 0x0C00 };   /* SANE's order to the x87's */
    UInt16 cw = 0x007F;                             /* every exception masked */
    if (gEnv & 0x0040) cw |= 0x0000;                /* single precision */
    else if (gEnv & 0x0020) cw |= 0x0200;           /* double */
    else cw |= 0x0300;                              /* extended */
    cw |= kRC[Rounding()];
    __asm__ volatile("fnstcw %0" : "=m"(gSavedCW) : : "memory");
    __asm__ volatile("fnclex\n\tfldcw %0" : : "m"(cw) : "memory");
}

/* The exceptions raised since FPBegin, into the environment; the FPU as
 * the rest of the system had it. Returns whether invalid was among them. */
static Boolean FPEnd(void) {
    UInt16 sw;
    __asm__ volatile("fnstsw %0\n\tfnclex\n\tfldcw %1" : "=m"(sw) : "m"(gSavedCW) : "memory");
    int exc = 0;
    if (sw & 0x01) exc |= kSANEInvalid;
    if (sw & 0x04) exc |= kSANEDivByZero;
    if (sw & 0x08) exc |= kSANEOverflow;
    if (sw & 0x10) exc |= kSANEUnderflow;
    if (sw & 0x20) exc |= kSANEInexact;
    Raise(exc);
    return (sw & 0x01) != 0;
}

static xf X87Sqrt(xf x)  { xf r; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }
static xf X87Rint(xf x)  { xf r; __asm__("frndint" : "=t"(r) : "0"(x)); return r; }
static xf X87Scale(xf x, xf n) { xf r; __asm__("fscale" : "=t"(r) : "0"(x), "u"(n)); return r; }
static xf X87Logb(xf x)  { xf sig, e; __asm__("fxtract" : "=t"(sig), "=u"(e) : "0"(x)); (void)sig; return e; }
static xf X87Log2(xf x)  { xf r; __asm__("fld1\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x) : "st(1)"); return r; }
static xf X87Ln(xf x)    { xf r; __asm__("fldln2\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x) : "st(1)"); return r; }
static xf X87Log2p1(xf x){ xf r; __asm__("fld1\n\tfxch\n\tfyl2xp1" : "=t"(r) : "0"(x) : "st(1)"); return r; }
static xf X87Exp2m1(xf x){ xf r; __asm__("f2xm1" : "=t"(r) : "0"(x)); return r; }   /* |x| <= 1 */
static xf X87Atan(xf x)  { xf r; __asm__("fld1\n\tfpatan" : "=t"(r) : "0"(x) : "st(1)"); return r; }
static xf X87Pi(void)    { xf r; __asm__("fldpi" : "=t"(r)); return r; }
static xf X87Log2e(void) { xf r; __asm__("fldl2e" : "=t"(r)); return r; }

/* The IEEE remainder of x by y, by FPREM1 until it is done */
static xf X87Rem(xf x, xf y) {
    xf r;
    __asm__("1:\tfprem1\n\tfnstsw %%ax\n\ttestw $0x0400, %%ax\n\tjnz 1b"
            : "=t"(r) : "0"(x), "u"(y) : "ax", "cc");
    return r;
}

/* Rounded to an integer in a direction of its own, whatever the program's */
static xf X87RoundIn(xf x, UInt16 rc) {
    UInt16 cw, tmp;
    __asm__ volatile("fnstcw %0" : "=m"(cw) : : "memory");
    tmp = (UInt16)((cw & ~0x0C00) | rc);
    __asm__ volatile("fldcw %0" : : "m"(tmp) : "memory");
    xf r = X87Rint(x);
    __asm__ volatile("fldcw %0" : : "m"(cw) : "memory");
    return r;
}

/* sin, cos and tan answer only below 2^63; past it, the argument is first
 * brought back by 2*pi */
static xf X87Reduce(xf x) {
    xf limit = 9223372036854775808.0L;
    if (x < limit && x > -limit) return x;
    return X87Rem(x, X87Pi() * 2);
}
static xf X87Sin(xf x) { xf r; __asm__("fsin" : "=t"(r) : "0"(X87Reduce(x))); return r; }
static xf X87Cos(xf x) { xf r; __asm__("fcos" : "=t"(r) : "0"(X87Reduce(x))); return r; }
static xf X87Tan(xf x) { xf one, r; __asm__("fptan" : "=t"(one), "=u"(r) : "0"(X87Reduce(x))); (void)one; return r; }

#else  /* !SANE_X87 */

static xf ToX(SANEExt e) {
    uint8_t b[8];
    SANE_FromExtended(e, kSANEDouble, kSANEToNearest, b);
    UInt64 u = 0;
    for (int i = 0; i < 8; i++) u = (u << 8) | b[i];
    double d;
    memcpy(&d, &u, 8);
    return d;
}
static SANEExt FromX(xf v) {
    double d = (double)v;
    UInt64 u;
    memcpy(&u, &d, 8);
    uint8_t b[8];
    for (int i = 7; i >= 0; i--) { b[i] = (uint8_t)u; u >>= 8; }
    return SANE_ToExtended(kSANEDouble, b);
}
static void FPBegin(void) {}
static Boolean FPEnd(void) { return false; }
#include "math.h"
static xf X87Sqrt(xf x)  { return sqrt((double)x); }
static xf X87Rint(xf x)  { double f = floor((double)x); double d = (double)x - f;
                           return d > 0.5 || (d == 0.5 && fmod(f, 2) != 0) ? f + 1 : f; }
static xf X87Scale(xf x, xf n) { return ldexp((double)x, (int)n); }
static xf X87Logb(xf x)  { int e; frexp((double)x, &e); return e - 1; }
static xf X87Log2(xf x)  { return log((double)x) / log(2.0); }
static xf X87Ln(xf x)    { return log((double)x); }
static xf X87Log2p1(xf x){ return log(1 + (double)x) / log(2.0); }
static xf X87Exp2m1(xf x){ return exp((double)x * log(2.0)) - 1; }
static xf X87Atan(xf x)  { return atan((double)x); }
static xf X87Pi(void)    { return 3.14159265358979323846; }
static xf X87Log2e(void) { return 1.44269504088896340736; }
static xf X87Rem(xf x, xf y) { double q = (double)X87Rint(x / y); return x - q * y; }
static xf X87RoundIn(xf x, UInt16 rc) {
    double d = (double)x;
    if (rc == 0x0C00) return d < 0 ? -floor(-d) : floor(d);
    if (rc == 0x0400) return floor(d);
    if (rc == 0x0800) return ceil(d);
    return X87Rint(x);
}
static xf X87Sin(xf x) { return sin((double)x); }
static xf X87Cos(xf x) { return cos((double)x); }
static xf X87Tan(xf x) { return tan((double)x); }
#endif

/* An invalid operation's NaN carries the code of what made it, as SANE's
 * do; the x87 makes the same one for everything */
static SANEExt Result(xf r, Boolean invalid, int nanCode) {
    SANEExt e = FromX(r);
    if (invalid && IsNaN(e) && e.mant == 0xC000000000000000ULL) e = SANE_NaN(nanCode);
    return e;
}

/* ------------------------------------------------------------------------
 * _FP68K
 * ------------------------------------------------------------------------ */

enum {
    kFOAdd = 0x00, kFOSub = 0x02, kFOMul = 0x04, kFODiv = 0x06, kFOCmp = 0x08,
    kFOCpx = 0x0A, kFORem = 0x0C, kFOZ2X = 0x0E, kFOX2Z = 0x10, kFOSqrt = 0x12,
    kFORti = 0x14, kFOTti = 0x16, kFOScalb = 0x18, kFOLogb = 0x1A, kFOClass = 0x1C,
    kFOSetEnv = 0x01, kFOGetEnv = 0x03, kFOSetHV = 0x05, kFOGetHV = 0x07,
    kFOD2B = 0x09, kFOB2D = 0x0B, kFONeg = 0x0D, kFOAbs = 0x0F, kFOCpySgn = 0x11,
    kFONext = 0x13, kFOSetXcp = 0x15, kFOProcEntry = 0x17, kFOProcExit = 0x19,
    kFOTestXcp = 0x1B
};

/* Decimal records: sgn a byte (then a spare one), exp a word, sig a
 * string[20]. DecForm: style a byte (then a spare), digits a word. */
void M68KSANE_ReadDecimal(UInt32 a, SANEDecimal* d) {
    d->sgn = R8(a) != 0;
    d->exp = (int16_t)R16(a + 2);
    UInt8 n = R8(a + 4);
    if (n > kSANESigDigLen) n = kSANESigDigLen;
    d->sig[0] = n;
    for (int i = 1; i <= n; i++) d->sig[i] = R8(a + 4 + i);
}

void M68KSANE_WriteDecimal(UInt32 a, const SANEDecimal* d) {
    W8(a, d->sgn);
    W8(a + 1, 0);
    W16(a + 2, (UInt16)d->exp);
    for (int i = 0; i <= d->sig[0]; i++) W8(a + 4 + i, d->sig[i]);
}

static void ReadDecForm(UInt32 a, SANEDecForm* f) {
    f->fixed = R8(a) != 0;
    f->digits = (int16_t)R16(a + 2);
}

/* The condition codes a comparison leaves: greater all clear, less X N C,
 * equal Z, unordered V */
static void SetCompareCodes(int c) {
    UInt16 sr = gM68KApp->regs.sr & ~(CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C);
    if (c < 0) sr |= CCR_X | CCR_N | CCR_C;
    else if (c == 0) sr |= CCR_Z;
    else if (c == 2) sr |= CCR_V;
    gM68KApp->regs.sr = sr;
}

static OSErr UnknownOperation(UInt16 op) {
    static char why[48];
    snprintf(why, sizeof(why), "SANE operation $%04X not implemented", op);
    gM68KApp->halted = true;
    gM68KApp->lastException = M68K_VEC_LINE_A;
    gM68KApp->faultReason = why;
    gM68KApp->faultPC = gM68KApp->instrPC;
    return noErr;
}

TRAP(Trap_FP68K) {
    UNUSED;
    UInt16 op = Pop16();
    int format = (op >> 11) & 7;
    int code = op & 0x1F;
    if (SANE_FormatSize(format) == 0) return UnknownOperation(op);

    switch (code) {
    case kFOAdd: case kFOSub: case kFOMul: case kFODiv: {
        UInt32 dst = Pop32(), src = Pop32();
        static const int kNaN[4] = { kNaNAdd, kNaNAdd, kNaNMul, kNaNDiv };
        FPBegin();
        xf a = ToX(LoadExt(dst)), b = ToX(LoadAs(src, format));
        volatile xf r;
        if (code == kFOAdd) r = a + b;
        else if (code == kFOSub) r = a - b;
        else if (code == kFOMul) r = a * b;
        else r = a / b;
        Boolean invalid = FPEnd();
        StoreExt(dst, Result(r, invalid, kNaN[code / 2]));
        break;
    }
    case kFOCmp: case kFOCpx: {
        UInt32 dst = Pop32(), src = Pop32();
        int signalling;
        int c = SANE_Compare(LoadExt(dst), LoadAs(src, format), &signalling);
        if (signalling || (c == 2 && code == kFOCpx)) Raise(kSANEInvalid);
        SetCompareCodes(c);
        break;
    }
    case kFORem: {
        /* dst := dst REM src; D0.W the low seven bits of the quotient,
         * with its sign */
        UInt32 dst = Pop32(), src = Pop32();
        FPBegin();
        xf a = ToX(LoadExt(dst)), b = ToX(LoadAs(src, format));
        volatile xf r = X87Rem(a, b);
        volatile xf q = X87Rint((a - r) / b);
        Boolean invalid = FPEnd();
        SInt32 n = 0;
        xf qa = q < 0 ? -q : q;
        if (qa == qa && qa < 2147483648.0L) n = (SInt32)(UInt32)qa & 0x7F;
        if (q < 0) n = -n;
        D(0) = (D(0) & 0xFFFF0000) | (UInt16)(SInt16)n;
        StoreExt(dst, Result(r, invalid, kNaNRem));
        break;
    }
    case kFOZ2X: {
        UInt32 dst = Pop32(), src = Pop32();
        SANEExt x = LoadAs(src, format);
        if (IsSNaN(x)) { Raise(kSANEInvalid); x.mant |= 0x4000000000000000ULL; }
        StoreExt(dst, x);
        break;
    }
    case kFOX2Z: {
        UInt32 dst = Pop32(), src = Pop32();
        StoreAs(dst, format, LoadExt(src));
        break;
    }
    case kFOSqrt: case kFORti: case kFOTti: case kFOLogb: {
        UInt32 dst = Pop32();
        FPBegin();
        xf a = ToX(LoadExt(dst));
        volatile xf r;
        if (code == kFOSqrt) r = X87Sqrt(a);
        else if (code == kFORti) r = X87Rint(a);
        else if (code == kFOTti) r = X87RoundIn(a, 0x0C00);
        else r = X87Logb(a);
        Boolean invalid = FPEnd();
        StoreExt(dst, Result(r, invalid, kNaNSqrt));
        break;
    }
    case kFOScalb: {
        /* dst := dst * 2^src, src an integer */
        UInt32 dst = Pop32(), src = Pop32();
        SInt16 n = (SInt16)R16(src);
        FPBegin();
        volatile xf r = X87Scale(ToX(LoadExt(dst)), (xf)n);
        FPEnd();
        StoreExt(dst, FromX(r));
        break;
    }
    case kFOClass: {
        UInt32 dst = Pop32(), src = Pop32();
        uint8_t b[10];
        ReadBytes(src, b, (UInt32)SANE_FormatSize(format));
        W16(dst, (UInt16)(SInt16)SANE_Classify(format, b));
        break;
    }
    case kFONeg: case kFOAbs: {
        /* The sign is the first bit, whatever the format */
        UInt32 dst = Pop32();
        UInt8 b = R8(dst);
        W8(dst, code == kFONeg ? b ^ 0x80 : b & 0x7F);
        break;
    }
    case kFOCpySgn: {
        /* The source takes the destination's sign (Numerics Manual p. 150) */
        UInt32 dst = Pop32(), src = Pop32();
        W8(src, (R8(src) & 0x7F) | (R8(dst) & 0x80));
        break;
    }
    case kFONext: {
        /* The source moves toward the destination, and holds the answer */
        UInt32 dst = Pop32(), src = Pop32();
        int n = SANE_FormatSize(format);
        uint8_t x[10], y[10];
        ReadBytes(src, x, (UInt32)n);
        ReadBytes(dst, y, (UInt32)n);
        Raise(SANE_NextAfter(format, x, y));
        WriteBytes(src, x, (UInt32)n);
        break;
    }
    case kFOD2B: {
        UInt32 dst = Pop32(), src = Pop32();
        SANEDecimal d;
        M68KSANE_ReadDecimal(src, &d);
        SANEExt x;
        if (format == kSANEExtended) {
            Raise(SANE_Dec2X(&d, Rounding(), 0, &x));
            StoreExt(dst, x);
        } else {
            /* Rounded to odd first, so the rounding into the format is
             * the only one that counts */
            SANE_Dec2X(&d, Rounding(), 1, &x);
            StoreAs(dst, format, x);
        }
        break;
    }
    case kFOB2D: {
        UInt32 dst = Pop32(), src = Pop32(), form = Pop32();
        SANEDecForm f;
        ReadDecForm(form, &f);
        SANEDecimal d;
        Raise(SANE_X2Dec(&f, LoadAs(src, format), Rounding(), &d));
        M68KSANE_WriteDecimal(dst, &d);
        break;
    }
    case kFOSetEnv:    gEnv = R16(Pop32()); break;
    case kFOGetEnv:    W16(Pop32(), gEnv); break;
    case kFOSetHV:     gHaltVector = R32(Pop32()); break;
    case kFOGetHV:     W32(Pop32(), gHaltVector); break;
    case kFOProcEntry: { UInt32 dst = Pop32(); W16(dst, gEnv); gEnv = 0; break; }
    case kFOProcExit: {
        /* The saved environment back, with the exceptions raised since */
        UInt16 raised = gEnv & 0x1F00;
        gEnv = R16(Pop32()) | raised;
        break;
    }
    case kFOSetXcp: case kFOTestXcp: {
        /* The operand is an Exception mask (Invalid 1 ... Inexact 16);
         * glue puts it in either byte, so either is read. TESTXCP
         * answers in the high byte, a Pascal Boolean. */
        UInt32 dst = Pop32();
        UInt16 w = R16(dst);
        int mask = ((w >> 8) | w) & 0x1F;
        if (code == kFOSetXcp) Raise(mask << 8);
        else W16(dst, (UInt16)((((gEnv >> 8) & mask) ? 0x0100 : 0) | (w & 0x00FF)));
        break;
    }
    default:
        return UnknownOperation(op);
    }
    return noErr;
}

/* ------------------------------------------------------------------------
 * _Elems68K: the elementary functions, on extendeds
 * ------------------------------------------------------------------------ */

enum {
    kFOLnX = 0x00, kFOLog2X = 0x02, kFOLn1X = 0x04, kFOLog21X = 0x06,
    kFOExpX = 0x08, kFOExp2X = 0x0A, kFOExp1X = 0x0C, kFOExp21X = 0x0E,
    kFOXPwrI = 0x10, kFOXPwrY = 0x12, kFOCompound = 0x14, kFOAnnuity = 0x16,
    kFOSinX = 0x18, kFOCosX = 0x1A, kFOTanX = 0x1C, kFOAtanX = 0x1E, kFORandX = 0x20
};

/* 2^x, by the integer part and f2xm1 on the rest */
static xf Exp2(xf x) {
    if (x != x) return x;
    if (x > 20000) return X87Scale(1, 20000);           /* overflows, as it should */
    if (x < -20000) return X87Scale(1, -20000);         /* underflows */
    xf n = X87Rint(x);
    return X87Scale(X87Exp2m1(x - n) + 1, n);
}

/* log2(1 + x), accurately for small x */
static xf Log21(xf x) {
    if (x > -0.29L && x < 0.29L) return X87Log2p1(x);
    return X87Log2(1 + x);
}

/* x^n for an integer n, by squaring */
static xf PowInt(xf x, long n) {
    unsigned long e = (unsigned long)(n < 0 ? -n : n);
    xf r = 1, b = x;
    while (e) {
        if (e & 1) r *= b;
        b *= b;
        e >>= 1;
    }
    return n < 0 ? 1 / r : r;
}

static xf Pow(xf x, xf y) {
    if (y == 0) return 1;
    if (x != x || y != y) return x + y;
    Boolean integral = X87Rint(y) == y;
    if (integral && y > -65536 && y < 65536) return PowInt(x, (long)y);
    Boolean odd = integral && X87Rint(y / 2) != y / 2;
    if (x < 0) {
        if (!integral) { xf z = 0; return z / z; }      /* invalid */
        xf r = Pow(-x, y);
        return odd ? -r : r;
    }
    if (x == 0) {
        Boolean negZero = FromX(x).sign && odd;
        xf r = y > 0 ? 0 : 1 / x;                       /* 1/0 raises divide-by-zero */
        if (y < 0) r = r < 0 ? -r : r;
        return negZero ? -r : r;
    }
    return Exp2(y * X87Log2(x));
}

TRAP(Trap_Elems68K) {
    UNUSED;
    UInt16 op = Pop16();
    int code = op & 0xFF;
    UInt32 dst = Pop32();
    UInt32 src = (op & 0x8000) ? Pop32() : 0;           /* two operands */
    UInt32 src2 = (op & 0x4000) ? Pop32() : 0;          /* three */
    if (code > kFORandX || (code & 1)) return UnknownOperation(op);

    FPBegin();
    xf x = ToX(LoadExt(dst));
    volatile xf r;
    int nanCode = kNaNLog;
    switch (code) {
    case kFOLnX:    r = X87Ln(x); break;
    case kFOLog2X:  r = X87Log2(x); break;
    case kFOLn1X:   r = Log21(x) / X87Log2e(); break;
    case kFOLog21X: r = Log21(x); break;
    case kFOExpX:   r = Exp2(x * X87Log2e()); break;
    case kFOExp2X:  r = Exp2(x); break;
    case kFOExp1X: {
        xf t = x * X87Log2e();
        r = (t > -1 && t < 1) ? X87Exp2m1(t) : Exp2(t) - 1;
        break;
    }
    case kFOExp21X: r = (x > -1 && x < 1) ? X87Exp2m1(x) : Exp2(x) - 1; break;
    case kFOXPwrI:  r = PowInt(x, (SInt16)R16(src)); nanCode = kNaNPower; break;
    case kFOXPwrY:  r = Pow(x, ToX(LoadExt(src))); nanCode = kNaNPower; break;
    case kFOCompound: case kFOAnnuity: {
        /* compound(rate, n) = (1+rate)^n; annuity(rate, n) =
         * (1 - (1+rate)^-n) / rate. dst, then n, then rate. */
        xf n = ToX(LoadExt(src)), rate = ToX(LoadExt(src2));
        nanCode = kNaNFinan;
        if (rate < -1) { xf z = 0; r = z / z; break; }
        if (code == kFOCompound) r = n == 0 ? 1 : Exp2(n * Log21(rate));
        else r = rate == 0 ? n : (1 - Exp2(-n * Log21(rate))) / rate;
        break;
    }
    case kFOSinX:   r = X87Sin(x); nanCode = kNaNTrig; break;
    case kFOCosX:   r = X87Cos(x); nanCode = kNaNTrig; break;
    case kFOTanX:   r = X87Tan(x); nanCode = kNaNTrig; break;
    case kFOAtanX:  r = X87Atan(x); nanCode = kNaNInvTrig; break;
    case kFORandX: {
        /* The Lehmer generator SANE uses: x := 7^5 x mod (2^31 - 1) */
        SInt64 v = (SInt64)x;
        r = (xf)((v * 16807) % 2147483647);
        break;
    }
    }
    Boolean invalid = FPEnd();
    StoreExt(dst, Result(r, invalid, nanCode));
    return noErr;
}

const M68KTrapEntry kM68KSANETraps[] = {
    { 0xA9EB, Trap_FP68K },         { 0xA9EC, Trap_Elems68K },
};
const int kM68KSANETrapCount = (int)(sizeof(kM68KSANETraps) / sizeof(kM68KSANETraps[0]));
