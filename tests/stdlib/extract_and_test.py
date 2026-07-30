#!/usr/bin/env python3
"""
Differential test for the in-tree C string/memory routines.

src/System71StdLib.c cannot be compiled on the host - it pulls in MacTypes.h and
the rest of the kernel headers. But the string and memory routines in it are
pure: no kernel state, no I/O. This extracts those functions by brace matching,
renames them with an s7_ prefix, compiles them natively, and compares every one
against the host libc over a spread of inputs.

Each destination buffer is wrapped in guard bytes so a routine that writes even
one byte outside its buffer is caught. That is what the real bug was: strncpy
padded n bytes AFTER the terminator it had already stored, writing n+1 bytes
whenever src was shorter than n, and silently clearing the first byte of
whatever followed.

Usage:
    python3 tests/stdlib/extract_and_test.py
Exit status is non-zero if any case differs from libc or touches a guard byte.
"""

import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src', 'System71StdLib.c')

# Pure routines worth testing. Excluded: strdup/strndup (need the kernel
# allocator), strtok/strsep (carry static state), strupr/strlwr/strrev
# (non-standard, nothing to compare against).
WANTED = [
    'memcpy', 'memset', 'memmove', 'memcmp', 'memchr',
    'strlen', 'strcpy', 'strncpy', 'strcmp', 'strncmp',
    'strcasecmp', 'strncasecmp', 'strcat', 'strncat',
    'strlcpy', 'strlcat', 'strchr', 'strrchr', 'strstr',
    'strspn', 'strcspn', 'strpbrk',
    # support routines the above call
    'toupper', 'tolower', 'isupper', 'islower',
    # formatted output, plus the helpers it dispatches to
    'vsnprintf', 'fmt_emit', 'fmt_pad', 'fmt_number', 'fmt_double',
    # 64-bit division - the freestanding build has no libgcc __udivdi3
    'udiv64',
]

# Types the extracted formatter needs that live in the kernel headers.
PREAMBLE_EXTRA = '''
#include <stdarg.h>
typedef struct {
    char*  buf;
    size_t size;
    size_t count;
} FmtSink;
'''


def extract(text, name):
    """Pull one top-level function definition out by brace matching."""
    pat = re.compile(
        r'^[A-Za-z_][A-Za-z0-9_\s\*]*?\b' + re.escape(name) + r'\s*\([^;{]*\)\s*\{',
        re.M)
    m = pat.search(text)
    if not m:
        return None
    i = text.index('{', m.start())
    depth = 0
    for j in range(i, len(text)):
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                return text[m.start():j + 1]
    return None


def build_source():
    text = open(SRC, errors='ignore').read()
    bodies, missing = [], []
    for name in WANTED:
        body = extract(text, name)
        if body is None:
            missing.append(name)
        else:
            bodies.append(body)
    if missing:
        print('could not extract: %s' % ', '.join(missing), file=sys.stderr)
    src = '\n\n'.join(bodies)

    # Rename every extracted symbol, definitions and internal calls alike, so
    # the copies under test never resolve to libc.
    for name in WANTED:
        src = re.sub(r'\b' + re.escape(name) + r'\b', 's7_' + name, src)

    # The extracted bodies keep source order, so a routine may call one defined
    # further down. Emit prototypes first rather than trying to topologically
    # sort them.
    protos = []
    for body in bodies:
        head = body[:body.index('{')].strip()
        for name in WANTED:
            if re.search(r'\bs7_?' + re.escape(name) + r'\b', head) or \
               re.search(r'\b' + re.escape(name) + r'\b', head):
                protos.append(head + ';')
                break
    protos = [re.sub(r'\b(' + '|'.join(map(re.escape, WANTED)) + r')\b',
                     lambda m: 's7_' + m.group(1), p) for p in protos]
    protos = [re.sub(r'^static inline\b', 'static', p) for p in protos]

    return ('#include <stddef.h>\n#include <stdint.h>\n'
            '#include <stdbool.h>\n' + PREAMBLE_EXTRA + '\ntypedef int OSErr;\n\n'
            + '\n'.join(protos) + '\n\n' + src)


HARNESS = r'''
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GUARD 0xA5
#define BUF   64
#define PAD   16

static int failures = 0;
static int checks   = 0;

/* A destination buffer bracketed by guard bytes. Anything a routine writes
   outside [PAD, PAD+BUF) is a buffer overrun. */
typedef struct { unsigned char raw[PAD + BUF + PAD]; } Guarded;

static char *gbuf(Guarded *g) {
    memset(g->raw, GUARD, sizeof g->raw);
    return (char *)(g->raw + PAD);
}

static int guards_intact(Guarded *g, const char *what, const char *detail) {
    for (size_t i = 0; i < PAD; i++) {
        if (g->raw[i] != GUARD) {
            printf("FAIL %-12s underrun at -%zu  (%s)\n", what, PAD - i, detail);
            failures++;
            return 0;
        }
    }
    for (size_t i = 0; i < PAD; i++) {
        if (g->raw[PAD + BUF + i] != GUARD) {
            printf("FAIL %-12s OVERRUN +%zu bytes past end  (%s)\n",
                   what, i + 1, detail);
            failures++;
            return 0;
        }
    }
    return 1;
}

static void cmp_bytes(const char *what, const char *detail,
                      const void *a, const void *b, size_t n) {
    checks++;
    if (memcmp(a, b, n) != 0) {
        printf("FAIL %-12s content differs from libc  (%s)\n", what, detail);
        failures++;
    }
}

static void cmp_int(const char *what, const char *detail, long a, long b) {
    checks++;
    /* Only the sign is specified for the compare functions. */
    long sa = (a > 0) - (a < 0), sb = (b > 0) - (b < 0);
    if (sa != sb) {
        printf("FAIL %-12s returned %ld, libc %ld  (%s)\n", what, a, b, detail);
        failures++;
    }
}

/* Variadic wrapper so the tests can call the extracted vsnprintf directly. */
static int s7_vsnprintf_wrap(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = s7_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

static const char *SAMPLES[] = {
    "", "a", "ab", "abc", "hello", "Hello, World",
    "Control Panels", "Note Pad", "About This Macintosh",
    "0123456789012345678901234567890123456789",
};
#define NSAMPLES (sizeof SAMPLES / sizeof SAMPLES[0])

int main(void) {
    Guarded g1, g2;
    char detail[160];

    for (size_t s = 0; s < NSAMPLES; s++) {
        const char *src = SAMPLES[s];
        size_t slen = strlen(src);

        checks++;
        if (s7_strlen(src) != slen) {
            printf("FAIL strlen       got %zu want %zu\n", s7_strlen(src), slen);
            failures++;
        }

        /* strcpy */
        {
            char *a = gbuf(&g1), *b = gbuf(&g2);
            s7_strcpy(a, src); strcpy(b, src);
            snprintf(detail, sizeof detail, "src=\"%s\"", src);
            if (guards_intact(&g1, "strcpy", detail))
                cmp_bytes("strcpy", detail, a, b, BUF);
        }

        /* strncpy across every n up to the full buffer - this is where the
           off-by-one lived, and only n == BUF puts the extra byte on the
           guard where it is unambiguously an overrun rather than a content
           difference. */
        for (size_t n = 0; n <= BUF; n++) {
            char *a = gbuf(&g1), *b = gbuf(&g2);
            s7_strncpy(a, src, n); strncpy(b, src, n);
            snprintf(detail, sizeof detail, "src=\"%s\" n=%zu", src, n);
            if (guards_intact(&g1, "strncpy", detail))
                cmp_bytes("strncpy", detail, a, b, BUF);
        }

        /* strcat / strncat onto a non-empty destination */
        for (size_t n = 0; n <= 24; n++) {
            char *a = gbuf(&g1), *b = gbuf(&g2);
            strcpy(a, "seed:"); strcpy(b, "seed:");
            s7_strncat(a, src, n); strncat(b, src, n);
            snprintf(detail, sizeof detail, "src=\"%s\" n=%zu", src, n);
            if (guards_intact(&g1, "strncat", detail))
                cmp_bytes("strncat", detail, a, b, BUF);
        }
        {
            char *a = gbuf(&g1), *b = gbuf(&g2);
            strcpy(a, "seed:"); strcpy(b, "seed:");
            s7_strcat(a, src); strcat(b, src);
            snprintf(detail, sizeof detail, "src=\"%s\"", src);
            if (guards_intact(&g1, "strcat", detail))
                cmp_bytes("strcat", detail, a, b, BUF);
        }

        /* strlcpy / strlcat - OpenBSD semantics, return total source length */
        for (size_t n = 0; n <= 24; n++) {
            char *a = gbuf(&g1);
            size_t r = s7_strlcpy(a, src, n);
            snprintf(detail, sizeof detail, "src=\"%s\" size=%zu", src, n);
            if (guards_intact(&g1, "strlcpy", detail)) {
                checks++;
                if (r != slen) {
                    printf("FAIL strlcpy      returned %zu want %zu (%s)\n",
                           r, slen, detail);
                    failures++;
                }
                checks++;
                if (n > 0 && a[(slen < n - 1) ? slen : n - 1] != '\0') {
                    printf("FAIL strlcpy      not NUL terminated (%s)\n", detail);
                    failures++;
                }
            }
        }
        for (size_t n = 1; n <= 24; n++) {
            char *a = gbuf(&g1);
            strcpy(a, "seed:");
            s7_strlcat(a, src, n);
            snprintf(detail, sizeof detail, "src=\"%s\" size=%zu", src, n);
            if (guards_intact(&g1, "strlcat", detail)) {
                checks++;
                /* OpenBSD strlcat leaves dst alone when size <= strlen(dst) -
                   there is no room to append and it must not truncate what is
                   already there. Only check termination when it did append. */
                if (n > strlen("seed:") && strlen(a) >= n) {
                    printf("FAIL strlcat      overflowed size (%s)\n", detail);
                    failures++;
                }
            }
        }

        /* memset / memcpy / memmove */
        for (size_t n = 0; n <= 32; n++) {
            char *a = gbuf(&g1), *b = gbuf(&g2);
            s7_memset(a, 'x', n); memset(b, 'x', n);
            snprintf(detail, sizeof detail, "n=%zu", n);
            if (guards_intact(&g1, "memset", detail))
                cmp_bytes("memset", detail, a, b, BUF);

            a = gbuf(&g1); b = gbuf(&g2);
            s7_memcpy(a, SAMPLES[NSAMPLES - 1], n);
            memcpy(b, SAMPLES[NSAMPLES - 1], n);
            if (guards_intact(&g1, "memcpy", detail))
                cmp_bytes("memcpy", detail, a, b, BUF);
        }

        /* memmove with overlap, both directions */
        for (int off = -8; off <= 8; off++) {
            char *a = gbuf(&g1), *b = gbuf(&g2);
            memcpy(a + 16, SAMPLES[NSAMPLES - 1], 32);
            memcpy(b + 16, SAMPLES[NSAMPLES - 1], 32);
            s7_memmove(a + 16 + off, a + 16, 24);
            memmove(b + 16 + off, b + 16, 24);
            snprintf(detail, sizeof detail, "overlap off=%d", off);
            if (guards_intact(&g1, "memmove", detail))
                cmp_bytes("memmove", detail, a, b, BUF);
        }

        /* comparisons and searches */
        for (size_t t = 0; t < NSAMPLES; t++) {
            const char *o = SAMPLES[t];
            snprintf(detail, sizeof detail, "\"%s\" vs \"%s\"", src, o);
            cmp_int("strcmp", detail, s7_strcmp(src, o), strcmp(src, o));
            cmp_int("strcasecmp", detail, s7_strcasecmp(src, o), strcasecmp(src, o));
            for (size_t n = 0; n <= 8; n++) {
                cmp_int("strncmp", detail, s7_strncmp(src, o, n), strncmp(src, o, n));
                cmp_int("strncasecmp", detail,
                        s7_strncasecmp(src, o, n), strncasecmp(src, o, n));
            }
            checks++;
            if ((s7_strstr(src, o) == NULL) != (strstr(src, o) == NULL)) {
                printf("FAIL strstr       differs (%s)\n", detail);
                failures++;
            }
            checks++;
            if (s7_strspn(src, o) != strspn(src, o)) {
                printf("FAIL strspn       got %zu want %zu (%s)\n",
                       s7_strspn(src, o), strspn(src, o), detail);
                failures++;
            }
            checks++;
            if (s7_strcspn(src, o) != strcspn(src, o)) {
                printf("FAIL strcspn      got %zu want %zu (%s)\n",
                       s7_strcspn(src, o), strcspn(src, o), detail);
                failures++;
            }
            checks++;
            if ((s7_strpbrk(src, o) == NULL) != (strpbrk(src, o) == NULL)) {
                printf("FAIL strpbrk      differs (%s)\n", detail);
                failures++;
            }
        }

        for (int c = 0; c < 128; c++) {
            snprintf(detail, sizeof detail, "\"%s\" c=%d", src, c);
            checks++;
            if ((s7_strchr(src, c) == NULL) != (strchr(src, c) == NULL)) {
                printf("FAIL strchr       differs (%s)\n", detail);
                failures++;
            }
            checks++;
            if ((s7_strrchr(src, c) == NULL) != (strrchr(src, c) == NULL)) {
                printf("FAIL strrchr      differs (%s)\n", detail);
                failures++;
            }
            checks++;
            if ((s7_memchr(src, c, slen) == NULL) != (memchr(src, c, slen) == NULL)) {
                printf("FAIL memchr       differs (%s)\n", detail);
                failures++;
            }
        }

        for (size_t n = 0; n <= slen; n++) {
            snprintf(detail, sizeof detail, "\"%s\" n=%zu", src, n);
            cmp_int("memcmp", detail,
                    s7_memcmp(src, "abc", n), memcmp(src, "abc", n));
        }
    }

    /* ---- formatted output ---------------------------------------------
       Every case is compared against the host libc: the produced text, the
       return value (C99: what WOULD have been written), and the guard bytes.
       A %lx used to print literally and consume no argument, shifting every
       following one. */
    {
        struct { const char *fmt; int kind; } cases[] = {
            {"%d",      0}, {"%5d",   0}, {"%-5d|", 0}, {"%05d",  0},
            {"%+d",     0}, {"% d",   0}, {"%.3d",  0}, {"%8.3d", 0},
            {"%u",      1}, {"%x",    1}, {"%X",    1}, {"%o",    1},
            {"%#x",     1}, {"%08x",  1}, {"%-8x|", 1}, {"%.5x",  1},
            {"%ld",     2}, {"%lu",   3}, {"%lx",   3}, {"%08lx", 3},
            {"%lld",    4}, {"%llu",  5}, {"%llx",  5},
            {"%zu",     6}, {"%zx",   6},
            {"%hd",     0}, {"%hhd",  0}, {"%hu",   1}, {"%hhx",  1},
            {"%s",      7}, {"%10s|", 7}, {"%-10s|",7}, {"%.3s",  7},
            {"%c",      8}, {"%5c|",  8},
            {"%%",      9}, {"a%%b",  9}, {"plain text", 9},
            {"[%d,%s,%x]", 10},
            {"%lx then %d", 11},
        };
        int ints[]              = {0, 1, -1, 42, -42, 255, 2147483647, -2147483647 - 1};
        unsigned uints[]        = {0u, 1u, 42u, 255u, 4294967295u};
        long longs[]            = {0L, -1L, 123456789L};
        unsigned long ulongs[]  = {0uL, 1uL, 0xDEADBEEFuL};
        long long llongs[]      = {0LL, -1LL, -9223372036854775807LL - 1};
        unsigned long long ulls[]= {0uLL, 0xFFFFFFFFFFFFuLL};
        size_t zs[]             = {0u, 4096u, 605348u};
        const char *strs[]      = {"", "a", "Note Pad", "About This Macintosh"};

        for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++) {
            const char *f = cases[k].fmt;
            int kind = cases[k].kind;
            #define TRY(...) do {                                        \
                Guarded ga, gb;                                               \
                char *A = gbuf(&ga), *B = gbuf(&gb);                          \
                int ra = s7_vsnprintf_wrap(A, BUF, __VA_ARGS__);                 \
                int rb = snprintf(B, BUF, __VA_ARGS__);                          \
                snprintf(detail, sizeof detail, "fmt=\"%s\"", f);             \
                if (guards_intact(&ga, "vsnprintf", detail)) {                \
                    checks++;                                                 \
                    if (strcmp(A, B) != 0) {                                  \
                        printf("FAIL vsnprintf   \"%s\" -> \"%s\" want \"%s\"\n", f, A, B); \
                        failures++;                                           \
                    }                                                         \
                    checks++;                                                 \
                    if (ra != rb) {                                           \
                        printf("FAIL vsnprintf   \"%s\" returned %d want %d\n", f, ra, rb); \
                        failures++;                                           \
                    }                                                         \
                }                                                             \
            } while (0)

            switch (kind) {
            case 0: for (size_t i=0;i<sizeof ints/sizeof*ints;i++)     TRY(f, ints[i]);   break;
            case 1: for (size_t i=0;i<sizeof uints/sizeof*uints;i++)   TRY(f, uints[i]);  break;
            case 2: for (size_t i=0;i<sizeof longs/sizeof*longs;i++)   TRY(f, longs[i]);  break;
            case 3: for (size_t i=0;i<sizeof ulongs/sizeof*ulongs;i++) TRY(f, ulongs[i]); break;
            case 4: for (size_t i=0;i<sizeof llongs/sizeof*llongs;i++) TRY(f, llongs[i]); break;
            case 5: for (size_t i=0;i<sizeof ulls/sizeof*ulls;i++)     TRY(f, ulls[i]);   break;
            case 6: for (size_t i=0;i<sizeof zs/sizeof*zs;i++)         TRY(f, zs[i]);     break;
            case 7: for (size_t i=0;i<sizeof strs/sizeof*strs;i++)     TRY(f, strs[i]);   break;
            case 8: TRY(f, 'M'); TRY(f, 'z'); break;
            case 9: TRY(f); break;
            case 10: TRY(f, 42, "Chooser", 0xBEEF); break;
            case 11: TRY(f, 0xCAFEuL, 7); break;
            }
            #undef TRY
        }

        /* Truncation: the return value must still be the full length, which is
           what vasprintf relies on to decide whether to grow its buffer. */
        for (size_t cap = 1; cap <= 24; cap++) {
            Guarded ga;
            char *A = gbuf(&ga);
            char B[64];
            int ra = s7_vsnprintf_wrap(A, cap, "%s-%d", "truncate", 12345);
            int rb = snprintf(B, cap, "%s-%d", "truncate", 12345);
            snprintf(detail, sizeof detail, "cap=%zu", cap);
            if (guards_intact(&ga, "vsnprintf", detail)) {
                checks++;
                if (ra != rb) {
                    printf("FAIL vsnprintf   truncated return %d want %d (%s)\n",
                           ra, rb, detail);
                    failures++;
                }
                checks++;
                if (strcmp(A, B) != 0) {
                    printf("FAIL vsnprintf   truncated \"%s\" want \"%s\" (%s)\n",
                           A, B, detail);
                    failures++;
                }
            }
        }
    }

    /* ---- udiv64 --------------------------------------------------------
       Compared against the host's native 64-bit division. The original
       omitted the left-shift that aligns the divisor and saturated instead of
       dividing: 100/3 gave 3 and 1000000/16667 gave 32767, which stopped
       TickCount advancing since it is udiv64(microseconds, 16667). */
    {
        static uint64_t rng = 88172645463325252ULL;
        #define XS() (rng ^= rng << 13, rng ^= rng >> 7, rng ^= rng << 17, rng)

        struct { unsigned long long a, b; } known[] = {
            {100, 3}, {1000000, 16667}, {0, 1}, {1, 1},
            {~0ULL, 1}, {~0ULL, ~0ULL}, {~0ULL, 2}, {1ULL << 63, 3},
            {16667, 16667}, {16666, 16667},
        };
        for (size_t i = 0; i < sizeof known / sizeof known[0]; i++) {
            checks++;
            unsigned long long got = s7_udiv64(known[i].a, known[i].b);
            unsigned long long want = known[i].a / known[i].b;
            if (got != want) {
                printf("FAIL udiv64      %llu/%llu = %llu want %llu\n",
                       known[i].a, known[i].b, got, want);
                failures++;
            }
        }

        for (unsigned long long a = 0; a < 200; a++) {
            for (unsigned long long b = 1; b < 200; b++) {
                checks++;
                if (s7_udiv64(a, b) != a / b) {
                    printf("FAIL udiv64      %llu/%llu = %llu want %llu\n",
                           a, b, (unsigned long long)s7_udiv64(a, b), a / b);
                    failures++;
                }
            }
        }

        for (long i = 0; i < 200000; i++) {
            unsigned long long a = XS() >> (XS() % 64);
            unsigned long long b = XS() >> (XS() % 64);
            if (b == 0) b = 1;
            checks++;
            if (s7_udiv64(a, b) != a / b) {
                if (failures < 8) {
                    printf("FAIL udiv64      %llu/%llu = %llu want %llu\n",
                           a, b, (unsigned long long)s7_udiv64(a, b), a / b);
                }
                failures++;
            }
        }

        /* Division by zero must not hang or trap; the kernel copies return 0. */
        checks++;
        if (s7_udiv64(12345, 0) != 0) {
            printf("FAIL udiv64      x/0 should return 0\n");
            failures++;
        }
        #undef XS
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
'''


def main():
    generated = build_source()
    keep = os.environ.get('KEEP_GENERATED')
    if keep:
        with open(keep, 'w') as fh:
            fh.write(generated)
            fh.write(HARNESS)
        print('wrote %s' % keep)
    with tempfile.TemporaryDirectory() as tmp:
        cfile = os.path.join(tmp, 'stdlib_test.c')
        binf = os.path.join(tmp, 'stdlib_test')
        with open(cfile, 'w') as fh:
            fh.write(generated)
            fh.write(HARNESS)

        cc = subprocess.run(
            # -Wno-format-security: the harness deliberately passes non-literal
            # format strings, which is the whole point of the comparison.
            ['gcc', '-O1', '-fno-builtin', '-Wall', '-Wno-format-security',
             '-o', binf, cfile],
            capture_output=True, text=True)
        if cc.returncode != 0:
            print(cc.stderr, file=sys.stderr)
            return 2

        run = subprocess.run([binf], capture_output=True, text=True)
        sys.stdout.write(run.stdout)
        if run.stderr:
            sys.stderr.write(run.stderr)
        return run.returncode


if __name__ == '__main__':
    sys.exit(main())
