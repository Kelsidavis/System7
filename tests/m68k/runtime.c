/*
 * runtime.c - the arithmetic GCC calls out for, for a 68000
 *
 * A 68000 multiplies 16 bits by 16 and divides 32 by 16; GCC calls these
 * for anything wider. Its own copies, in libgcc, are built for the 68020
 * and use instructions a 68000 does not have.
 */

unsigned long __udivsi3(unsigned long n, unsigned long d);
unsigned long __umodsi3(unsigned long n, unsigned long d);
long __divsi3(long n, long d);
long __modsi3(long n, long d);
long __mulsi3(long a, long b);

static unsigned long udivmod(unsigned long n, unsigned long d, unsigned long* rem) {
    unsigned long q = 0, r = 0;
    if (d == 0) {
        *rem = n;
        return 0;
    }
    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) {
            r -= d;
            q |= 1UL << i;
        }
    }
    *rem = r;
    return q;
}

unsigned long __udivsi3(unsigned long n, unsigned long d) {
    unsigned long r;
    return udivmod(n, d, &r);
}

unsigned long __umodsi3(unsigned long n, unsigned long d) {
    unsigned long r;
    udivmod(n, d, &r);
    return r;
}

long __divsi3(long n, long d) {
    unsigned long r;
    int neg = (n < 0) != (d < 0);
    unsigned long q = udivmod(n < 0 ? -(unsigned long)n : (unsigned long)n,
                              d < 0 ? -(unsigned long)d : (unsigned long)d, &r);
    return neg ? -(long)q : (long)q;
}

long __modsi3(long n, long d) {
    unsigned long r;
    udivmod(n < 0 ? -(unsigned long)n : (unsigned long)n,
            d < 0 ? -(unsigned long)d : (unsigned long)d, &r);
    return n < 0 ? -(long)r : (long)r;
}

long __mulsi3(long a, long b) {
    unsigned long x = (unsigned long)a, y = (unsigned long)b, p = 0;
    while (y) {
        if (y & 1) p += x;
        x <<= 1;
        y >>= 1;
    }
    return (long)p;
}
