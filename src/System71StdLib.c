/* System 7.1 Standard Library Implementation */

#include "System71StdLib.h"
#include "MacTypes.h"
#include "SystemInternal.h"

#include <stdbool.h>

/* Forward declarations for character classification functions */
int tolower(int c);
int toupper(int c);

#if defined(__powerpc__) || defined(__powerpc64__)
#include "Platform/PowerPC/OpenFirmware.h"
#include "Platform/PowerPC/escc_uart.h"
#endif

/* Global memory error variable */
static OSErr gMemError = noErr;

/* Memory Manager Error Function */
OSErr MemError(void) {
    return gMemError;
}

/* Memory functions */
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    while (n--) {
        *p++ = (uint8_t)c;
    }
    return s;
}

void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;

    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

void* memchr(const void* s, int c, size_t n) {
    const unsigned char* p = (const unsigned char*)s;
    unsigned char ch = (unsigned char)c;

    while (n--) {
        if (*p == ch) {
            /* Per C standard, memchr returns non-const pointer to allow
             * modification if original buffer was non-const */
            return (void*)(uintptr_t)p;
        }
        p++;
    }
    return NULL;
}

/* Mac Toolbox utility functions */
SInt16 HiWord(SInt32 x) {
    return (x >> 16) & 0xFFFF;
}

SInt16 LoWord(SInt32 x) {
    return x & 0xFFFF;
}

void BlockMoveData(const void* srcPtr, void* destPtr, Size byteCount) {
    /* Use memmove to handle overlapping memory regions correctly */
    memmove(destPtr, srcPtr, byteCount);
}

/* BSD compatibility functions */
void bzero(void* s, size_t n) {
    memset(s, 0, n);
}

void explicit_bzero(void* s, size_t n) {
    /* Secure memory clearing that won't be optimized away */
    volatile unsigned char* p = (volatile unsigned char*)s;
    while (n--) {
        *p++ = 0;
    }
}

int memset_s(void* s, size_t smax, int c, size_t n) {
    /* C11 Annex K secure memset with bounds checking */
    if (s == NULL) return 1;
    if (smax > SIZE_MAX) return 1;
    if (n > smax) return 1;

    /* Secure fill using volatile to prevent optimization */
    volatile unsigned char* p = (volatile unsigned char*)s;
    unsigned char value = (unsigned char)c;

    for (size_t i = 0; i < n; i++) {
        p[i] = value;
    }

    return 0;
}

void bcopy(const void* src, void* dst, size_t n) {
    memmove(dst, src, n);
}

int bcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

/* Program termination */
#define MAX_ATEXIT_HANDLERS 32
static void (*atexit_handlers[MAX_ATEXIT_HANDLERS])(void);
static int atexit_count = 0;

int atexit(void (*func)(void)) {
    if (atexit_count >= MAX_ATEXIT_HANDLERS) {
        return -1;  /* Too many handlers */
    }
    atexit_handlers[atexit_count++] = func;
    return 0;
}

void exit(int status) {
    /* Call all registered exit handlers in reverse order */
    for (int i = atexit_count - 1; i >= 0; i--) {
        if (atexit_handlers[i]) {
            atexit_handlers[i]();
        }
    }

    /* Halt the system */
    while (1) {}
}

void abort(void) {
    extern void serial_puts(const char* s);
    serial_puts("ABORT: Program terminated abnormally\n");
    while (1) {}
}

/* Environment variables (bare-metal stub implementation) */
#define MAX_ENV_VARS 32
static struct {
    char* name;
    char* value;
} env_vars[MAX_ENV_VARS];
static int env_count = 0;

char* getenv(const char* name) {
    if (!name) return NULL;

    for (int i = 0; i < env_count; i++) {
        if (env_vars[i].name && strcmp(env_vars[i].name, name) == 0) {
            return env_vars[i].value;
        }
    }
    return NULL;
}

int setenv(const char* name, const char* value, int overwrite) {
    if (!name || !value) return -1;

    /* Check if variable already exists */
    for (int i = 0; i < env_count; i++) {
        if (env_vars[i].name && strcmp(env_vars[i].name, name) == 0) {
            if (!overwrite) return 0;

            /* Free old value and set new one */
            extern void free(void* ptr);
            if (env_vars[i].value) free(env_vars[i].value);
            env_vars[i].value = strdup(value);
            return env_vars[i].value ? 0 : -1;
        }
    }

    /* Add new variable */
    if (env_count >= MAX_ENV_VARS) return -1;

    env_vars[env_count].name = strdup(name);
    env_vars[env_count].value = strdup(value);
    if (!env_vars[env_count].name || !env_vars[env_count].value) {
        return -1;
    }
    env_count++;
    return 0;
}

int unsetenv(const char* name) {
    if (!name) return -1;

    extern void free(void* ptr);
    for (int i = 0; i < env_count; i++) {
        if (env_vars[i].name && strcmp(env_vars[i].name, name) == 0) {
            /* Free name and value */
            if (env_vars[i].name) free(env_vars[i].name);
            if (env_vars[i].value) free(env_vars[i].value);

            /* Shift remaining entries down */
            for (int j = i; j < env_count - 1; j++) {
                env_vars[j] = env_vars[j + 1];
            }
            env_count--;
            return 0;
        }
    }
    return 0;  /* Not found is not an error */
}

/* Command-line option parsing */
char* optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

int getopt(int argc, char* const argv[], const char* optstring) {
    static int sp = 1;
    int c;
    const char* cp;

    if (sp == 1) {
        /* Check for end of options */
        if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return -1;
        }
        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }
    }

    optopt = c = argv[optind][sp];

    /* Check for invalid option */
    if (c == ':' || (cp = strchr(optstring, c)) == NULL) {
        if (opterr && optstring[0] != ':') {
            extern void serial_puts(const char* s);
            extern void serial_putchar(char c);
            serial_puts("Unknown option: -");
            serial_putchar(c);
            serial_puts("\n");
        }
        if (argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return '?';
    }

    /* Check if option requires an argument */
    if (cp[1] == ':') {
        if (argv[optind][sp + 1] != '\0') {
            /* Argument immediately follows option */
            optarg = &argv[optind++][sp + 1];
        } else if (++optind >= argc) {
            /* Missing argument */
            if (opterr && optstring[0] != ':') {
                extern void serial_puts(const char* s);
                extern void serial_putchar(char c);
                serial_puts("Option requires an argument: -");
                serial_putchar(c);
                serial_puts("\n");
            }
            sp = 1;
            return (optstring[0] == ':') ? ':' : '?';
        } else {
            /* Argument is next argv element */
            optarg = argv[optind++];
        }
        sp = 1;
    } else {
        /* No argument */
        if (argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = NULL;
    }

    return c;
}

/* Error reporting */
/* Provide our own strerror to avoid linking to system libraries */
const char* sys71_strerror(int errnum) {
    /* Simple error message lookup */
    switch (errnum) {
        case 0: return "Success";
        case 1: return "Operation not permitted";
        case 2: return "No such file or directory";
        case 3: return "No such process";
        case 4: return "Interrupted system call";
        case 5: return "Input/output error";
        case 6: return "No such device or address";
        case 7: return "Argument list too long";
        case 8: return "Exec format error";
        case 9: return "Bad file descriptor";
        case 10: return "No child processes";
        case 11: return "Resource temporarily unavailable";
        case 12: return "Cannot allocate memory";
        case 13: return "Permission denied";
        case 14: return "Bad address";
        case 15: return "Block device required";
        case 16: return "Device or resource busy";
        case 17: return "File exists";
        case 18: return "Invalid cross-device link";
        case 19: return "No such device";
        case 20: return "Not a directory";
        case 21: return "Is a directory";
        case 22: return "Invalid argument";
        case 23: return "Too many open files in system";
        case 24: return "Too many open files";
        case 25: return "Inappropriate ioctl for device";
        case 26: return "Text file busy";
        case 27: return "File too large";
        case 28: return "No space left on device";
        case 29: return "Illegal seek";
        case 30: return "Read-only file system";
        case 31: return "Too many links";
        case 32: return "Broken pipe";
        case 33: return "Numerical argument out of domain";
        case 34: return "Numerical result out of range";
        default: return "Unknown error";
    }
}

void perror(const char* s) {
    extern void serial_puts(const char* str);
    extern int errno;

    if (s && *s) {
        serial_puts(s);
        serial_puts(": ");
    }
    serial_puts(sys71_strerror(errno));
    serial_puts("\n");
}

/* POSIX file I/O stubs (bare-metal environment) */
int open(const char* pathname, int flags, ...) {
    /* Stub: no file system in bare-metal */
    (void)pathname;
    (void)flags;
    extern int errno;
    errno = 2;  /* ENOENT */
    return -1;
}

int close(int fd) {
    /* Stub: no file descriptors */
    (void)fd;
    extern int errno;
    errno = 9;  /* EBADF */
    return -1;
}

ssize_t read(int fd, void* buf, size_t count) {
    /* Stub: no file I/O */
    (void)fd;
    (void)buf;
    (void)count;
    extern int errno;
    errno = 9;  /* EBADF */
    return -1;
}

ssize_t write(int fd, const void* buf, size_t count) {
    /* Stub: only support stdout/stderr to serial */
    if (fd == 1 || fd == 2) {
        /* stdout or stderr - write to serial console */
        const char* str = (const char*)buf;
        for (size_t i = 0; i < count; i++) {
            serial_putchar(str[i]);
        }
        return count;
    }

    /* Other file descriptors not supported */
    (void)buf;
    extern int errno;
    errno = 9;  /* EBADF */
    return -1;
}

/* Time and delay functions */
unsigned int sleep(unsigned int seconds) {
    /* Simple busy-wait sleep (not accurate, cooperative) */
    extern void Delay(UInt32 numTicks, UInt32* finalTicks);

    /* Mac ticks are ~60Hz, so multiply by 60 to get approximate seconds */
    UInt32 ticks = seconds * 60;
    UInt32 finalTicks;
    Delay(ticks, &finalTicks);

    return 0;  /* Full sleep completed */
}

int usleep(unsigned int usec) {
    /* Microsecond sleep - convert to ticks (1 tick ≈ 16667 μs at 60Hz) */
    extern void Delay(UInt32 numTicks, UInt32* finalTicks);

    UInt32 ticks = (usec + 16666) / 16667;  /* Round up */
    if (ticks == 0) ticks = 1;  /* At least 1 tick */

    UInt32 finalTicks;
    Delay(ticks, &finalTicks);

    return 0;  /* Success */
}

/* String functions */
size_t strlen(const char* s) {
    size_t len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n && (*d++ = *src++)) {
        n--;
    }
    while (n--) {
        *d++ = '\0';
    }
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) {
        d++;
    }
    while ((*d++ = *src++));
    return dest;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (*d) {
        d++;
    }
    while (n && (*d = *src)) {
        d++;
        src++;
        n--;
    }
    *d = '\0';
    return dest;
}

size_t strlcpy(char* dst, const char* src, size_t size) {
    /* Safe copy with guaranteed NUL termination (OpenBSD) */
    size_t src_len = strlen(src);

    if (size > 0) {
        size_t copy_len = (src_len >= size) ? size - 1 : src_len;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }

    return src_len;  /* Return total length of source */
}

size_t strlcat(char* dst, const char* src, size_t size) {
    /* Safe concatenation with guaranteed NUL termination (OpenBSD) */
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

    if (dst_len >= size) {
        /* Buffer already full or size too small */
        return size + src_len;
    }

    size_t available = size - dst_len - 1;
    size_t copy_len = (src_len > available) ? available : src_len;

    memcpy(dst + dst_len, src, copy_len);
    dst[dst_len + copy_len] = '\0';

    return dst_len + src_len;  /* Return total length attempted */
}

char* strchr(const char* s, int c) {
    char ch = (char)c;
    while (*s) {
        if (*s == ch) {
            return (char*)(uintptr_t)s;
        }
        s++;
    }
    /* Per C standard, strchr(s, '\0') returns pointer to terminating NUL */
    return (ch == '\0') ? (char*)(uintptr_t)s : NULL;
}

char* strrchr(const char* s, int c) {
    char ch = (char)c;
    const char* last = NULL;
    while (*s) {
        if (*s == ch) {
            last = s;
        }
        s++;
    }
    /* Per C standard, strrchr(s, '\0') returns pointer to terminating NUL */
    if (ch == '\0') {
        return (char*)(uintptr_t)s;
    }
    return (char*)(uintptr_t)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)(uintptr_t)haystack;

    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;

        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }

        if (!*n) {
            return (char*)(uintptr_t)haystack;
        }

        haystack++;
    }
    return NULL;
}

char* strcasestr(const char* haystack, const char* needle) {
    /* Case-insensitive substring search */
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char*)(uintptr_t)haystack;

    size_t needle_len = strlen(needle);

    while (*haystack) {
        if (strncasecmp(haystack, needle, needle_len) == 0) {
            return (char*)(uintptr_t)haystack;
        }
        haystack++;
    }
    return NULL;
}

char* index(const char* s, int c) {
    /* BSD alias for strchr */
    return strchr(s, c);
}

char* rindex(const char* s, int c) {
    /* BSD alias for strrchr */
    return strrchr(s, c);
}

char* strdup(const char* s) {
    /* Duplicate string (allocates memory) */
    if (!s) return NULL;

    extern void* malloc(size_t size);
    size_t len = strlen(s) + 1;
    char* dup = (char*)malloc(len);
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

char* strndup(const char* s, size_t n) {
    /* Duplicate at most n characters of string (allocates memory) */
    if (!s) return NULL;

    extern void* malloc(size_t size);
    size_t len = strlen(s);
    if (len > n) len = n;

    char* dup = (char*)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}

char* strupr(char* s) {
    /* Convert string to uppercase (in-place) */
    if (!s) return NULL;

    char* p = s;
    while (*p) {
        *p = toupper(*p);
        p++;
    }
    return s;
}

char* strlwr(char* s) {
    /* Convert string to lowercase (in-place) */
    if (!s) return NULL;

    char* p = s;
    while (*p) {
        *p = tolower(*p);
        p++;
    }
    return s;
}

char* strrev(char* s) {
    /* Reverse string (in-place) */
    if (!s) return NULL;

    size_t len = strlen(s);
    if (len <= 1) return s;

    char* start = s;
    char* end = s + len - 1;

    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
    return s;
}

size_t strspn(const char* s, const char* accept) {
    /* Count initial characters in s that are in accept set */
    const char* p = s;
    while (*p) {
        const char* a = accept;
        int found = 0;
        while (*a) {
            if (*p == *a) {
                found = 1;
                break;
            }
            a++;
        }
        if (!found) break;
        p++;
    }
    return p - s;
}

size_t strcspn(const char* s, const char* reject) {
    /* Count initial characters in s NOT in reject set */
    const char* p = s;
    while (*p) {
        const char* r = reject;
        while (*r) {
            if (*p == *r) {
                return p - s;
            }
            r++;
        }
        p++;
    }
    return p - s;
}

char* strpbrk(const char* s, const char* accept) {
    /* Find first character in s that matches any character in accept */
    while (*s) {
        const char* a = accept;
        while (*a) {
            if (*s == *a) {
                return (char*)(uintptr_t)s;
            }
            a++;
        }
        s++;
    }
    return NULL;
}

char* strtok(char* str, const char* delim) {
    /* Tokenize string using delimiters
     * Uses static variable to maintain state between calls
     * NOT thread-safe */
    static char* saved = NULL;

    /* Use saved pointer if str is NULL */
    if (str == NULL) {
        str = saved;
    }

    if (str == NULL) {
        return NULL;
    }

    /* Skip leading delimiters */
    str += strspn(str, delim);
    if (*str == '\0') {
        saved = NULL;
        return NULL;
    }

    /* Find end of token */
    char* token_start = str;
    str = strpbrk(str, delim);

    if (str == NULL) {
        /* Last token */
        saved = NULL;
    } else {
        /* Null-terminate token and save position */
        *str = '\0';
        saved = str + 1;
    }

    return token_start;
}

char* strtok_r(char* str, const char* delim, char** saveptr) {
    /* Reentrant version of strtok - thread-safe */
    if (str == NULL) {
        str = *saveptr;
    }

    if (str == NULL) {
        return NULL;
    }

    /* Skip leading delimiters */
    str += strspn(str, delim);
    if (*str == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    /* Find end of token */
    char* token_start = str;
    str = strpbrk(str, delim);
    if (str == NULL) {
        *saveptr = NULL;
    } else {
        *str = '\0';
        *saveptr = str + 1;
    }

    return token_start;
}

char* strsep(char** stringp, const char* delim) {
    /* Extract token and update pointer */
    if (!stringp || !*stringp) {
        return NULL;
    }

    char* start = *stringp;
    char* end = strpbrk(start, delim);

    if (end) {
        *end = '\0';
        *stringp = end + 1;
    } else {
        *stringp = NULL;
    }

    return start;
}

char* basename(const char* path) {
    /* Extract filename from path (simplified version) */
    static char buf[256];

    if (!path || !*path) {
        strcpy(buf, ".");
        return buf;
    }

    /* Find last slash */
    const char* p = strrchr(path, '/');
    if (!p) {
        /* No slash, return whole path */
        strncpy(buf, path, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }

    /* Skip trailing slashes */
    const char* end = path + strlen(path) - 1;
    while (end > path && *end == '/') {
        end--;
    }

    if (end < p) {
        /* Path is all slashes */
        strcpy(buf, "/");
        return buf;
    }

    /* Copy from last slash to end */
    p++;
    size_t len = end - p + 1;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';

    return buf;
}

char* dirname(const char* path) {
    /* Extract directory from path (simplified version) */
    static char buf[256];

    if (!path || !*path) {
        strcpy(buf, ".");
        return buf;
    }

    /* Copy path to buffer */
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Remove trailing slashes */
    char* end = buf + strlen(buf) - 1;
    while (end > buf && *end == '/') {
        *end = '\0';
        end--;
    }

    /* Find last slash */
    char* p = strrchr(buf, '/');
    if (!p) {
        /* No slash, return current directory */
        strcpy(buf, ".");
        return buf;
    }

    /* If slash is at beginning, return root */
    if (p == buf) {
        buf[1] = '\0';
        return buf;
    }

    /* Terminate at last slash */
    *p = '\0';
    return buf;
}

/* Pascal string utilities */
void c2pstrcpy(unsigned char* dst, const char* src) {
    /* Copy C string to Pascal string (safe version that doesn't modify src) */
    if (!dst || !src) return;

    size_t len = strlen(src);
    if (len > 255) len = 255;  /* Pascal strings limited to 255 characters */

    dst[0] = (unsigned char)len;  /* Set length byte */
    memcpy(dst + 1, src, len);    /* Copy string data */
}

void p2cstrcpy(char* dst, const unsigned char* src) {
    /* Copy Pascal string to C string (safe version that doesn't modify src) */
    if (!dst || !src) return;

    unsigned char len = src[0];   /* Get length from first byte */
    memcpy(dst, src + 1, len);    /* Copy string data */
    dst[len] = '\0';              /* Null terminate */
}

unsigned char* CopyCStringToPascal(const char* src, unsigned char* dst) {
    /* Mac Toolbox alias for c2pstrcpy */
    c2pstrcpy(dst, src);
    return dst;
}

char* CopyPascalStringToC(const unsigned char* src, char* dst) {
    /* Mac Toolbox alias for p2cstrcpy */
    p2cstrcpy(dst, src);
    return dst;
}

unsigned char PLstrlen(const unsigned char* str) {
    /* Get Pascal string length */
    return str ? str[0] : 0;
}

int PLstrcmp(const unsigned char* str1, const unsigned char* str2) {
    /* Compare two Pascal strings */
    if (!str1 || !str2) return 0;

    unsigned char len1 = str1[0];
    unsigned char len2 = str2[0];
    unsigned char minLen = (len1 < len2) ? len1 : len2;

    /* Compare the strings byte by byte */
    for (unsigned char i = 1; i <= minLen; i++) {
        if (str1[i] != str2[i]) {
            return str1[i] - str2[i];
        }
    }

    /* If all compared bytes are equal, shorter string is less */
    return len1 - len2;
}

void PLstrcpy(unsigned char* dst, const unsigned char* src) {
    /* Copy Pascal string */
    if (!dst || !src) return;

    unsigned char len = src[0];
    dst[0] = len;
    memcpy(dst + 1, src + 1, len);
}

void PLstrcat(unsigned char* dst, const unsigned char* src) {
    /* Concatenate Pascal strings */
    if (!dst || !src) return;

    unsigned char dstLen = dst[0];
    unsigned char srcLen = src[0];
    unsigned int totalLen = (unsigned int)dstLen + (unsigned int)srcLen;

    /* Limit to 255 characters */
    if (totalLen > 255) {
        totalLen = 255;
        srcLen = (unsigned char)(totalLen - dstLen);
    }
    unsigned char newLen = (unsigned char)totalLen;

    /* Copy source string data after destination */
    memcpy(dst + 1 + dstLen, src + 1, srcLen);
    dst[0] = newLen;
}

/* Byte swapping and endianness conversion */
uint16_t SwapInt16(uint16_t value) {
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

uint32_t SwapInt32(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

/* Network byte order conversion (big-endian)
 * Mac is already big-endian, so these are identity functions */

/* Undefine macOS system macros that conflict with our implementations */
#ifdef htons
#undef htons
#endif
#ifdef htonl
#undef htonl
#endif
#ifdef ntohs
#undef ntohs
#endif
#ifdef ntohl
#undef ntohl
#endif

uint16_t htons(uint16_t hostshort) {
    /* Host to network short - Mac is big-endian, network is big-endian */
    return hostshort;
}

uint32_t htonl(uint32_t hostlong) {
    /* Host to network long - Mac is big-endian, network is big-endian */
    return hostlong;
}

uint16_t ntohs(uint16_t netshort) {
    /* Network to host short - Mac is big-endian, network is big-endian */
    return netshort;
}

uint32_t ntohl(uint32_t netlong) {
    /* Network to host long - Mac is big-endian, network is big-endian */
    return netlong;
}

/* Character classification functions */
int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

static inline int isalpha(int c) {
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

static inline int isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}

static inline int islower(int c) {
    return (c >= 'a' && c <= 'z');
}

int toupper(int c) {
    if (islower(c)) {
        return c - ('a' - 'A');
    }
    return c;
}

int tolower(int c) {
    if (isupper(c)) {
        return c + ('a' - 'A');
    }
    return c;
}

int isxdigit(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isprint(int c) {
    return c >= 0x20 && c <= 0x7E;
}

int isgraph(int c) {
    return c > 0x20 && c <= 0x7E;
}

int iscntrl(int c) {
    return (c >= 0 && c < 0x20) || c == 0x7F;
}

int ispunct(int c) {
    return isgraph(c) && !isalnum(c);
}

int isblank(int c) {
    return c == ' ' || c == '\t';
}

int isascii(int c) {
    return c >= 0 && c <= 0x7F;
}

int toascii(int c) {
    return c & 0x7F;
}

/* Conversion functions */
int atoi(const char* str) {
    int result = 0;
    int sign = 1;

    while (*str == ' ' || *str == '\t') {
        str++;
    }

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

long atol(const char* str) {
    long result = 0;
    int sign = 1;

    while (*str == ' ' || *str == '\t') {
        str++;
    }

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

double atof(const char* str) {
    double result = 0.0;
    double fraction = 0.0;
    int sign = 1;
    int exponent = 0;
    int exp_sign = 1;
    double divisor = 1.0;

    /* Skip whitespace */
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    /* Handle sign */
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* Parse integer part */
    while (*str >= '0' && *str <= '9') {
        result = result * 10.0 + (*str - '0');
        str++;
    }

    /* Parse fractional part */
    if (*str == '.') {
        str++;
        while (*str >= '0' && *str <= '9') {
            fraction = fraction * 10.0 + (*str - '0');
            divisor *= 10.0;
            str++;
        }
        result += fraction / divisor;
    }

    /* Parse exponent */
    if (*str == 'e' || *str == 'E') {
        str++;
        if (*str == '-') {
            exp_sign = -1;
            str++;
        } else if (*str == '+') {
            str++;
        }

        while (*str >= '0' && *str <= '9') {
            exponent = exponent * 10 + (*str - '0');
            str++;
        }

        /* Apply exponent using repeated multiplication/division */
        if (exp_sign > 0) {
            for (int i = 0; i < exponent; i++) {
                result *= 10.0;
            }
        } else {
            for (int i = 0; i < exponent; i++) {
                result /= 10.0;
            }
        }
    }

    return result * sign;
}

long strtol(const char* str, char** endptr, int base) {
    long result = 0;
    int sign = 1;
    const char* start = str;

    /* Skip whitespace */
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    /* Handle sign */
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* Auto-detect base if base is 0 */
    if (base == 0) {
        if (*str == '0') {
            if (str[1] == 'x' || str[1] == 'X') {
                base = 16;
                str += 2;
            } else {
                base = 8;
                str++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && *str == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }

    /* Parse digits */
    while (*str) {
        int digit;
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'a' && *str <= 'z') {
            digit = *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'Z') {
            digit = *str - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) {
            break;
        }

        result = result * base + digit;
        str++;
    }

    if (endptr) {
        /* Per C standard, endptr returns non-const pointer into const input */
        *endptr = (char*)(uintptr_t)((str == start) ? start : str);
    }

    return result * sign;
}

unsigned long strtoul(const char* str, char** endptr, int base) {
    unsigned long result = 0;
    const char* start = str;

    /* Skip whitespace */
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    /* Skip optional + sign (no negative for unsigned) */
    if (*str == '+') {
        str++;
    }

    /* Auto-detect base if base is 0 */
    if (base == 0) {
        if (*str == '0') {
            if (str[1] == 'x' || str[1] == 'X') {
                base = 16;
                str += 2;
            } else {
                base = 8;
                str++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && *str == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }

    /* Parse digits */
    while (*str) {
        int digit;
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'a' && *str <= 'z') {
            digit = *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'Z') {
            digit = *str - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) {
            break;
        }

        result = result * base + digit;
        str++;
    }

    if (endptr) {
        /* Per C standard, endptr returns non-const pointer into const input */
        *endptr = (char*)(uintptr_t)((str == start) ? start : str);
    }

    return result;
}

/* Math functions */
int abs(int n) {
    return n < 0 ? -n : n;
}

long labs(long n) {
    return n < 0 ? -n : n;
}

int min(int a, int b) {
    return a < b ? a : b;
}

int max(int a, int b) {
    return a > b ? a : b;
}

long lmin(long a, long b) {
    return a < b ? a : b;
}

long lmax(long a, long b) {
    return a > b ? a : b;
}

double fmin(double a, double b) {
    return a < b ? a : b;
}

double fmax(double a, double b) {
    return a > b ? a : b;
}

int clamp(int value, int min_val, int max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/* Define div_t and ldiv_t only if not already defined by system headers
 * On ARM64 Darwin with stdlib.h included, these are already defined */
#if !defined(__div_t_defined) && !defined(_DIV_T) && !defined(__DARWIN_UNIX03)
typedef struct {
    int quot;
    int rem;
} div_t;
#endif

#if !defined(__ldiv_t_defined) && !defined(_LDIV_T) && !defined(__DARWIN_UNIX03)
typedef struct {
    long quot;
    long rem;
} ldiv_t;
#endif

div_t div(int numer, int denom) {
    div_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

ldiv_t ldiv(long numer, long denom) {
    ldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

/* Sorting and searching */
static void qsort_swap(void* a, void* b, size_t size) {
    unsigned char* p1 = (unsigned char*)a;
    unsigned char* p2 = (unsigned char*)b;
    unsigned char temp;

    while (size--) {
        temp = *p1;
        *p1++ = *p2;
        *p2++ = temp;
    }
}

static void qsort_impl(void* base, size_t left, size_t right, size_t size,
                      int (*compar)(const void*, const void*)) {
    if (left >= right) return;

    /* Choose pivot (median of three for better performance) */
    size_t mid = left + (right - left) / 2;
    unsigned char* arr = (unsigned char*)base;

    /* Sort left, mid, right */
    if (compar(arr + left * size, arr + mid * size) > 0)
        qsort_swap(arr + left * size, arr + mid * size, size);
    if (compar(arr + left * size, arr + right * size) > 0)
        qsort_swap(arr + left * size, arr + right * size, size);
    if (compar(arr + mid * size, arr + right * size) > 0)
        qsort_swap(arr + mid * size, arr + right * size, size);

    /* Use middle element as pivot */
    qsort_swap(arr + mid * size, arr + right * size, size);

    size_t i = left;
    size_t j = right - 1;
    void* pivot = arr + right * size;

    while (1) {
        while (i < right && compar(arr + i * size, pivot) < 0) i++;
        while (j > left && compar(arr + j * size, pivot) > 0) j--;

        if (i >= j) break;

        qsort_swap(arr + i * size, arr + j * size, size);
        i++;
        j--;
    }

    qsort_swap(arr + i * size, arr + right * size, size);

    if (i > left) qsort_impl(base, left, i - 1, size, compar);
    if (i < right) qsort_impl(base, i + 1, right, size, compar);
}

void qsort(void* base, size_t nmemb, size_t size,
          int (*compar)(const void*, const void*)) {
    if (nmemb <= 1 || !base || !compar) return;
    qsort_impl(base, 0, nmemb - 1, size, compar);
}

void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
             int (*compar)(const void*, const void*)) {
    const unsigned char* arr = (const unsigned char*)base;
    size_t left = 0;
    size_t right = nmemb;

    while (left < right) {
        size_t mid = left + (right - left) / 2;
        const void* elem = arr + mid * size;
        int cmp = compar(key, elem);

        if (cmp < 0) {
            right = mid;
        } else if (cmp > 0) {
            left = mid + 1;
        } else {
            return (void*)(uintptr_t)elem;
        }
    }

    return NULL;
}

/* Random number generation */
static unsigned long g_rand_seed = 1;

void srand(unsigned int seed) {
    g_rand_seed = seed;
}

int rand(void) {
    /* Linear congruential generator (LCG)
     * Using parameters from BSD/POSIX.1-2001:
     * - Multiplier: 1103515245
     * - Increment: 12345
     * - Modulus: 2^31 (implicit via masking)
     *
     * Returns value in range [0, RAND_MAX] where RAND_MAX = 32767
     */
    g_rand_seed = g_rand_seed * 1103515245 + 12345;
    return (int)((g_rand_seed >> 16) & 0x7FFF);  /* Return 15-bit value */
}

/* Serial I/O functions */
#include <stdint.h>
#include <stdarg.h>

#define COM1 0x3F8

#include "Platform/include/io.h"

#define inb(port) hal_inb(port)
#define outb(port, value) hal_outb(port, value)

#if defined(__arm__) || defined(__aarch64__)
#include <stdint.h>
static uintptr_t g_pl011_uart_base = 0x09000000u;
static inline volatile uint32_t* pl011_reg(uint32_t offset) {
    return (volatile uint32_t*)(g_pl011_uart_base + offset);
}
#define PL011_DR   0x00
#define PL011_FR   0x18
#define PL011_IBRD 0x24
#define PL011_FBRD 0x28
#define PL011_LCRH 0x2C
#define PL011_CR   0x30
#define PL011_IMSC 0x38
#define PL011_ICR  0x44
#define PL011_RXFE (1u << 4)
#define PL011_TXFF (1u << 5)
#define PL011_UARTEN (1u << 0)
#define PL011_TXE (1u << 8)
#define PL011_RXE (1u << 9)

void serial_set_pl011_base(uintptr_t base) {
    if (base != 0) {
        g_pl011_uart_base = base;
    }
}
#endif

void serial_init(void) {
#if defined(__arm__) || defined(__aarch64__)
    /* Initialize PL011 UART (115200 baud, 8N1) */
    volatile uint32_t* cr = pl011_reg(PL011_CR);
    volatile uint32_t* icr = pl011_reg(PL011_ICR);
    volatile uint32_t* ibrd = pl011_reg(PL011_IBRD);
    volatile uint32_t* fbrd = pl011_reg(PL011_FBRD);
    volatile uint32_t* lcrh = pl011_reg(PL011_LCRH);
    volatile uint32_t* imsc = pl011_reg(PL011_IMSC);

    *cr = 0;             /* disable UART */
    *icr = 0x7FF;        /* clear interrupts */
    *ibrd = 13;          /* assuming 24 MHz clock */
    *fbrd = 2;           /* fractional divider */
    *lcrh = (1u << 4) | (1u << 5) | (1u << 6); /* FIFO enable, 8-bit */
    *imsc = 0;           /* disable interrupts */
    *cr = PL011_UARTEN | PL011_TXE | PL011_RXE;
    return;
#elif defined(__powerpc__) || defined(__powerpc64__)
    /* Initialize ESCC (Zilog Z8530) for QEMU mac99 */
    escc_init();
    return;
#else
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00);    //                  (hi byte)
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
#endif
}

void serial_putchar(char c) {
#if defined(__arm__) || defined(__aarch64__)
    /* Basic PL011 transmit */
    volatile uint32_t* fr = pl011_reg(PL011_FR);
    volatile uint32_t* dr = pl011_reg(PL011_DR);
    if (c == '\n') {
        while ((*fr) & PL011_TXFF) { }
        *dr = '\r';
    }
    while ((*fr) & PL011_TXFF) {
        /* wait for space */
    }
    *dr = (uint32_t)c;
    return;
#elif defined(__powerpc__) || defined(__powerpc64__)
    /* Try OF console first, fall back to ESCC UART */
    if (ofw_console_available()) {
        if (c == '\n') {
            const char cr = '\r';
            ofw_console_write(&cr, 1);
        }
        ofw_console_write(&c, 1);
        return;
    }

    escc_putchar(c);
    return;
#else
    while ((inb(COM1 + 5) & 0x20) == 0);
    outb(COM1, c);
#endif
}

void serial_puts(const char* str) {
    /* Direct serial output only - no framebuffer interaction */

    /* Direct serial output only - no framebuffer interaction */
    if (!str) return;
#if defined(__arm__) || defined(__aarch64__)
    /* ARM/ARM64: use UART driver */
    extern void uart_puts(const char *s);
    uart_puts(str);
#elif defined(__powerpc__) || defined(__powerpc64__)
    /* Try OF console first, then fall back to MMIO or just return */
    if (ofw_console_available()) {
        while (*str) {
            char ch = *str++;
            if (ch == '\n') {
                const char cr = '\r';
                ofw_console_write(&cr, 1);
            }
            ofw_console_write(&ch, 1);
        }
        return;
    }

    escc_puts(str);
    return;
#else
    while (*str) {
        if (*str == '\n') {
            while ((inb(COM1 + 5) & 0x20) == 0);
            outb(COM1, '\r');
        }
        while ((inb(COM1 + 5) & 0x20) == 0);
        outb(COM1, *str);
        str++;
    }
#endif
}

int serial_data_ready(void) {
#if defined(__arm__) || defined(__aarch64__)
    volatile uint32_t* fr = pl011_reg(PL011_FR);
    return ((*fr) & PL011_RXFE) == 0;
#elif defined(__powerpc__) || defined(__powerpc64__)
    if (!ofw_console_input_available()) {
        return 0;
    }
    char ch = 0;
    int rc = ofw_console_poll_char(&ch);
    if (rc == 1) {
        return 1;
    }
    return 0;
#else
    return (inb(COM1 + 5) & 0x01) != 0;
#endif
}

char serial_getchar(void) {
#if defined(__arm__) || defined(__aarch64__)
    volatile uint32_t* fr = pl011_reg(PL011_FR);
    volatile uint32_t* dr = pl011_reg(PL011_DR);
    while ((*fr) & PL011_RXFE) {
        /* wait for data */
    }
    return (char)(*dr & 0xFF);
#elif defined(__powerpc__) || defined(__powerpc64__)
    char ch = 0;
    if (ofw_console_read_char(&ch) == 1) {
        return ch;
    }
    return 0;
#else
    while (!serial_data_ready());
    return inb(COM1);
#endif
}

void serial_print_hex(uint32_t value) {
    /* DISABLED - Serial output was somehow appearing in GUI menu bar */
    return;

    const char* hex = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 7; i >= 0; i--) {
        serial_putchar(hex[(value >> (i * 4)) & 0xF]);
    }
}

/* Standard I/O wrappers for serial console */
int putchar(int c) {
    serial_putchar((char)c);
    return c;
}

int puts(const char* str) {
    if (!str) return -1;
    serial_puts(str);
    serial_putchar('\n');  /* puts adds newline */
    return 0;
}

int getchar(void) {
    return (int)(unsigned char)serial_getchar();
}

char* gets(char* str) {
    /* gets() is deprecated and unsafe - but provided for compatibility
     * Reads until newline or EOF, discards newline, null-terminates
     * NOTE: No buffer overflow protection - caller must ensure adequate buffer */
    if (!str) return NULL;

    char* p = str;
    while (1) {
        int c = getchar();
        if (c == '\n' || c == '\r' || c == 0) {
            break;
        }
        *p++ = (char)c;
    }
    *p = '\0';
    return str;
}

/* -------------------------------------------------------------------------- */
/* Logging infrastructure */


typedef struct {
    const char* tag;
    SystemLogModule module;
    SystemLogLevel level;
} SysLogTag;

static SystemLogLevel g_globalLogLevel = kLogLevelWarn;
static SystemLogLevel g_moduleLevels[kLogModuleCount] = {
    [kLogModuleGeneral] = kLogLevelWarn,
    [kLogModuleDesktop] = kLogLevelWarn,
    [kLogModuleEvent] = kLogLevelWarn,
    [kLogModuleFinder] = kLogLevelWarn,
    [kLogModuleFileSystem] = kLogLevelWarn,
    [kLogModuleWindow] = kLogLevelWarn,
    [kLogModuleMenu] = kLogLevelWarn,
    [kLogModuleDialog] = kLogLevelWarn,
    [kLogModuleControl] = kLogLevelWarn,
    [kLogModuleFont] = kLogLevelWarn,
    [kLogModuleSound] = kLogLevelWarn,
    [kLogModuleResource] = kLogLevelWarn,
    [kLogModuleStandardFile] = kLogLevelWarn,
    [kLogModuleListManager] = kLogLevelWarn,
    [kLogModuleSystem] = kLogLevelWarn,
    [kLogModuleTextEdit] = kLogLevelWarn,
    [kLogModulePlatform] = kLogLevelWarn,
    [kLogModuleScrap] = kLogLevelWarn,
    [kLogModuleMemory] = kLogLevelWarn,
    [kLogModuleProcess] = kLogLevelWarn,
    [kLogModuleSegmentLoader] = kLogLevelWarn,
    [kLogModuleCPU] = kLogLevelWarn
};

static const SysLogTag kLogTagTable[] = {
    { "CTRL", kLogModuleControl, kLogLevelDebug },
    { "CTRL SMOKE", kLogModuleControl, kLogLevelTrace },
    { "DM", kLogModuleDialog, kLogLevelDebug },
    { "WM", kLogModuleWindow, kLogLevelDebug },
    { "SF", kLogModuleStandardFile, kLogLevelDebug },
    { "LIST", kLogModuleListManager, kLogLevelDebug },
    { "LIST SMOKE", kLogModuleListManager, kLogLevelTrace },
    { "MI", kLogModuleMenu, kLogLevelTrace },
    { "PRE-IF", kLogModuleEvent, kLogLevelTrace },
    { "DBLCLK", kLogModuleEvent, kLogLevelTrace },
    { "WIN_OPEN", kLogModuleWindow, kLogLevelDebug },
    { "NEWWIN", kLogModuleWindow, kLogLevelDebug },
    { "HILITE", kLogModuleWindow, kLogLevelTrace },
    { "PaintBehind", kLogModuleWindow, kLogLevelTrace },
    { "MEM", kLogModuleMemory, kLogLevelInfo },
    { "FM", kLogModuleFont, kLogLevelDebug },
    { "M68K", kLogModuleCPU, kLogLevelDebug },
    { "XHCI", kLogModulePlatform, kLogLevelWarn }
};


static const size_t kLogTagCount = sizeof(kLogTagTable) / sizeof(kLogTagTable[0]);

static SystemLogLevel SysLogLevelFromString(const char* levelStr) {
    if (!levelStr) {
        return kLogLevelDebug;
    }

    if (strcmp(levelStr, "ERROR") == 0) return kLogLevelError;
    if (strcmp(levelStr, "WARN") == 0 || strcmp(levelStr, "WARNING") == 0) return kLogLevelWarn;
    if (strcmp(levelStr, "INFO") == 0) return kLogLevelInfo;
    if (strcmp(levelStr, "DEBUG") == 0) return kLogLevelDebug;
    if (strcmp(levelStr, "TRACE") == 0) return kLogLevelTrace;
    return kLogLevelDebug;
}

static void SysLogNormalizeString(char* str) {
    if (!str) return;
    for (char* p = str; *p; ++p) {
        if (*p >= 'a' && *p <= 'z') {
            *p = (char)(*p - 'a' + 'A');
        }
    }
}

static bool SysLogParseBracketTag(const char* fmt, SystemLogModule* module, SystemLogLevel* level) {
    if (!fmt) return false;

    while (*fmt == ' ' || *fmt == '\t') {
        ++fmt;
    }

    if (*fmt != '[') {
        return false;
    }

    const char* closing = strchr(fmt, ']');
    if (!closing || closing == fmt + 1) {
        return false;
    }

    size_t tagLen = (size_t)(closing - fmt - 1);
    if (tagLen >= 32) {
        tagLen = 31;
    }

    char tagBuffer[32];
    memcpy(tagBuffer, fmt + 1, tagLen);
    tagBuffer[tagLen] = '\0';

    char* levelSep = strchr(tagBuffer, ':');
    if (levelSep) {
        *levelSep = '\0';
        ++levelSep;
        SysLogNormalizeString(levelSep);
        *level = SysLogLevelFromString(levelSep);
    }

    SysLogNormalizeString(tagBuffer);

    for (size_t i = 0; i < kLogTagCount; ++i) {
        if (strcmp(tagBuffer, kLogTagTable[i].tag) == 0) {
            *module = kLogTagTable[i].module;
            if (!levelSep) {
                *level = kLogTagTable[i].level;
            }
            return true;
        }
    }

    return false;
}

/* Deprecated: serial_printf() classifier - only parses bracket tags, defaults to General */
static void SysLogClassifyMessage(const char* fmt, SystemLogModule* outModule, SystemLogLevel* outLevel) {
    SystemLogModule module = kLogModuleGeneral;
    SystemLogLevel level = kLogLevelDebug;

    if (!fmt) {
        *outModule = module;
        *outLevel = level;
        return;
    }

    /* Try to parse bracket tag like [WM], [DM], etc. */
    SysLogParseBracketTag(fmt, &module, &level);

    /* If no bracket tag found, use defaults (General/Debug) */
    *outModule = module;
    *outLevel = level;
}

static bool SysLogShouldEmit(SystemLogModule module, SystemLogLevel level) {
    if (module < 0 || module >= kLogModuleCount) {
        module = kLogModuleGeneral;
    }

    if (level > g_globalLogLevel) {
        return false;
    }

    if (level > g_moduleLevels[module]) {
        return false;
    }

    return true;
}

static void SysLogFormatAndSend(const char* fmt, va_list args) {
    const char* p = fmt;
    char buffer[256];
    int buf_idx = 0;

    while (p && *p && buf_idx < 255) {
        if (*p == '%') {
            ++p;

            if (*p == '0' && *(p + 1) == '2' && *(p + 2) == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                const char* hex_digits = "0123456789abcdef";
                buffer[buf_idx++] = hex_digits[(val >> 4) & 0xF];
                if (buf_idx < 255) buffer[buf_idx++] = hex_digits[val & 0xF];
                p += 3;
            }
            else if (*p == '0' && *(p + 1) == '4' && *(p + 2) == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                const char* hex_digits = "0123456789abcdef";
                for (int i = 3; i >= 0 && buf_idx < 255; --i) {
                    buffer[buf_idx++] = hex_digits[(val >> (i * 4)) & 0xF];
                }
                p += 3;
            }
            else if (*p == '0' && *(p + 1) == '8' && *(p + 2) == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                const char* hex_digits = "0123456789abcdef";
                for (int i = 7; i >= 0 && buf_idx < 255; --i) {
                    buffer[buf_idx++] = hex_digits[(val >> (i * 4)) & 0xF];
                }
                p += 3;
            }
            else if (*p == 'd') {
                int val = va_arg(args, int);
                char num_buf[12];
                int i = 0;

                if (val < 0) {
                    if (buf_idx < 255) buffer[buf_idx++] = '-';
                    val = -val;
                }

                if (val == 0) {
                    if (buf_idx < 255) buffer[buf_idx++] = '0';
                } else {
                    while (val > 0 && i < 11) {
                        num_buf[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                    while (i > 0 && buf_idx < 255) {
                        buffer[buf_idx++] = num_buf[--i];
                    }
                }
                ++p;
            }
            else if (*p == 'u') {
                unsigned int val = va_arg(args, unsigned int);
                char num_buf[12];
                int i = 0;

                if (val == 0) {
                    if (buf_idx < 255) buffer[buf_idx++] = '0';
                } else {
                    while (val > 0 && i < 11) {
                        num_buf[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                    while (i > 0 && buf_idx < 255) {
                        buffer[buf_idx++] = num_buf[--i];
                    }
                }
                ++p;
            }
            else if (*p == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                char hex_buf[9];
                int i = 0;
                const char* hex_digits = "0123456789abcdef";

                if (val == 0) {
                    if (buf_idx < 255) buffer[buf_idx++] = '0';
                } else {
                    while (val > 0 && i < 8) {
                        hex_buf[i++] = hex_digits[val & 0xF];
                        val >>= 4;
                    }
                    while (i > 0 && buf_idx < 255) {
                        buffer[buf_idx++] = hex_buf[--i];
                    }
                }
                ++p;
            }
            else if (*p == 's') {
                const char* s = va_arg(args, const char*);
                while (s && *s && buf_idx < 255) {
                    buffer[buf_idx++] = *s++;
                }
                ++p;
            }
            else if (*p == 'c') {
                int ch = va_arg(args, int);
                if (buf_idx < 255) buffer[buf_idx++] = (char)ch;
                ++p;
            }
            else if (*p == '%') {
                if (buf_idx < 255) buffer[buf_idx++] = '%';
                ++p;
            }
            else {
                if (buf_idx < 255) buffer[buf_idx++] = '%';
            }
        } else {
            if (buf_idx < 255) buffer[buf_idx++] = *p;
            ++p;
        }
    }

    buffer[buf_idx] = '\0';
    serial_puts(buffer);
}

void SysLogSetGlobalLevel(SystemLogLevel level) {
    g_globalLogLevel = level;
}

SystemLogLevel SysLogGetGlobalLevel(void) {
    return g_globalLogLevel;
}

void SysLogSetModuleLevel(SystemLogModule module, SystemLogLevel level) {
    if (module < 0 || module >= kLogModuleCount) {
        return;
    }
    g_moduleLevels[module] = level;
}

SystemLogLevel SysLogGetModuleLevel(SystemLogModule module) {
    if (module < 0 || module >= kLogModuleCount) {
        return g_moduleLevels[kLogModuleGeneral];
    }
    return g_moduleLevels[module];
}

const char* SysLogModuleName(SystemLogModule module) {
    switch (module) {
        case kLogModuleGeneral: return "general";
        case kLogModuleDesktop: return "desktop";
        case kLogModuleEvent: return "event";
        case kLogModuleFinder: return "finder";
        case kLogModuleFileSystem: return "filesystem";
        case kLogModuleWindow: return "window";
        case kLogModuleMenu: return "menu";
        case kLogModuleDialog: return "dialog";
        case kLogModuleControl: return "control";
        case kLogModuleFont: return "font";
        case kLogModuleSound: return "sound";
        case kLogModuleResource: return "resource";
        case kLogModuleStandardFile: return "standardfile";
        case kLogModuleListManager: return "list";
        case kLogModuleSystem: return "system";
        case kLogModuleTextEdit: return "textedit";
        case kLogModulePlatform: return "platform";
        case kLogModuleScrap: return "scrap";
        case kLogModuleMemory: return "memory";
        case kLogModuleProcess: return "process";
        default: return "general";
    }
}

static void SysLogEmit(SystemLogModule module, SystemLogLevel level, const char* fmt, va_list args) {
    if (!SysLogShouldEmit(module, level)) {
        return;
    }

    va_list argsCopy;
    va_copy(argsCopy, args);
    SysLogFormatAndSend(fmt, argsCopy);
    va_end(argsCopy);
}

void serial_logf(SystemLogModule module, SystemLogLevel level, const char* fmt, ...) {
    if (!fmt) return;

    va_list args;
    va_start(args, fmt);
    SysLogEmit(module, level, fmt, args);
    va_end(args);
}

void serial_printf(const char* fmt, ...) {
    if (!fmt) return;

    SystemLogModule module;
    SystemLogLevel level;
    SysLogClassifyMessage(fmt, &module, &level);

    va_list args;
    va_start(args, fmt);
    SysLogEmit(module, level, fmt, args);
    va_end(args);
}

int vprintf(const char* format, va_list args) {
    /* vprintf - print formatted output to serial console with va_list
     * This is the va_list version of serial_printf for serial output */
    if (!format) return 0;

    SystemLogModule module;
    SystemLogLevel level;
    SysLogClassifyMessage(format, &module, &level);

    SysLogEmit(module, level, format, args);
    return 0;  /* Return value not meaningful for serial output */
}

int printf(const char* format, ...) {
    /* printf - print formatted output to serial console */
    if (!format) return 0;

    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}

/* Internal vsnprintf implementation */
static int vsnprintf(char* str, size_t size, const char* format, va_list args) {
    if (!str || size == 0 || !format) return 0;

    size_t written = 0;
    const char* p = format;

    while (*p && written < size - 1) {
        if (*p == '%') {
            p++;

            /* Handle format flags and width (simplified) */
            int width = 0;
            int leftAlign = 0;
            int zeroPad = 0;

            /* Check for flags */
            if (*p == '-') {
                leftAlign = 1;
                p++;
            }
            if (*p == '0') {
                zeroPad = 1;
                p++;
            }

            /* Suppress unused variable warnings - these are placeholders for future implementation */
            (void)leftAlign;
            (void)zeroPad;

            /* Parse width */
            while (*p >= '0' && *p <= '9') {
                width = width * 10 + (*p - '0');
                p++;
            }

            /* Handle format specifiers */
            if (*p == 's') {
                /* Handle %s - string argument */
                const char* s = va_arg(args, const char*);
                if (s) {
                    while (*s && written < size - 1) {
                        str[written++] = *s++;
                    }
                }
                p++;
            } else if (*p == 'd' || *p == 'i') {
                /* Handle %d/%i - signed integer */
                int val = va_arg(args, int);
                char numBuf[32];
                int numLen = 0;
                int isNeg = 0;

                if (val < 0) {
                    isNeg = 1;
                    val = -val;
                }

                if (val == 0) {
                    numBuf[numLen++] = '0';
                } else {
                    while (val > 0 && numLen < 31) {
                        numBuf[numLen++] = '0' + (val % 10);
                        val /= 10;
                    }
                }

                if (isNeg && written < size - 1) {
                    str[written++] = '-';
                }

                /* Reverse the digits */
                for (int i = numLen - 1; i >= 0 && written < size - 1; i--) {
                    str[written++] = numBuf[i];
                }
                p++;
            } else if (*p == 'u') {
                /* Handle %u - unsigned integer */
                unsigned int val = va_arg(args, unsigned int);
                char numBuf[32];
                int numLen = 0;

                if (val == 0) {
                    numBuf[numLen++] = '0';
                } else {
                    while (val > 0 && numLen < 31) {
                        numBuf[numLen++] = '0' + (val % 10);
                        val /= 10;
                    }
                }

                for (int i = numLen - 1; i >= 0 && written < size - 1; i--) {
                    str[written++] = numBuf[i];
                }
                p++;
            } else if (*p == 'x' || *p == 'X') {
                /* Handle %x/%X - hexadecimal */
                unsigned int val = va_arg(args, unsigned int);
                char numBuf[32];
                int numLen = 0;
                char hexBase = (*p == 'X') ? 'A' : 'a';

                if (val == 0) {
                    numBuf[numLen++] = '0';
                } else {
                    while (val > 0 && numLen < 31) {
                        int digit = val % 16;
                        numBuf[numLen++] = (digit < 10) ? ('0' + digit) : (hexBase + digit - 10);
                        val /= 16;
                    }
                }

                for (int i = numLen - 1; i >= 0 && written < size - 1; i--) {
                    str[written++] = numBuf[i];
                }
                p++;
            } else if (*p == 'c') {
                /* Handle %c - character */
                char c = (char)va_arg(args, int);
                if (written < size - 1) {
                    str[written++] = c;
                }
                p++;
            } else if (*p == 'p') {
                /* Handle %p - pointer */
                void* ptr = va_arg(args, void*);
                unsigned long val = (unsigned long)ptr;

                /* Add 0x prefix */
                if (written < size - 1) str[written++] = '0';
                if (written < size - 1) str[written++] = 'x';

                /* Convert to hex */
                char numBuf[32];
                int numLen = 0;
                if (val == 0) {
                    numBuf[numLen++] = '0';
                } else {
                    while (val > 0 && numLen < 31) {
                        int digit = val % 16;
                        numBuf[numLen++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                        val /= 16;
                    }
                }

                for (int i = numLen - 1; i >= 0 && written < size - 1; i--) {
                    str[written++] = numBuf[i];
                }
                p++;
            } else if (*p == '%') {
                /* Handle %% - literal % */
                if (written < size - 1) {
                    str[written++] = '%';
                }
                p++;
            } else {
                /* Unknown format - just copy it */
                if (written < size - 1) str[written++] = '%';
                if (*p && written < size - 1) str[written++] = *p++;
            }
        } else {
            str[written++] = *p++;
        }
    }

    str[written] = '\0';
    return written;
}

/* Standard I/O functions */
int vsprintf(char* str, const char* format, va_list args) {
    if (!str || !format) return 0;

    /* WARNING: vsprintf is inherently unsafe - no buffer size limit.
     * Caller MUST ensure buffer is large enough for formatted output.
     * Use SIZE_MAX to avoid artificial truncation. */
    return vsnprintf(str, SIZE_MAX, format, args);
}

int sprintf(char* str, const char* format, ...) {
    if (!str || !format) return 0;

    va_list args;
    va_start(args, format);

    /* Delegate to vsprintf for actual formatting */
    int result = vsprintf(str, format, args);

    va_end(args);
    return result;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    if (size == 0) return 0;

    va_list args;
    va_start(args, format);

    int result = vsnprintf(str, size, format, args);

    va_end(args);
    return result;
}

int vasprintf(char** strp, const char* format, va_list args) {
    /* Allocate and format string dynamically */
    if (!strp) return -1;

    /* Try with initial buffer size */
    size_t size = 256;
    char* buf = (char*)malloc(size);
    if (!buf) {
        *strp = NULL;
        return -1;
    }

    /* Format with va_list */
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(buf, size, format, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        free(buf);
        *strp = NULL;
        return -1;
    }

    /* Check if buffer was large enough */
    if ((size_t)needed >= size) {
        /* Reallocate with exact size needed */
        char* newbuf = (char*)realloc(buf, needed + 1);
        if (!newbuf) {
            free(buf);
            *strp = NULL;
            return -1;
        }
        buf = newbuf;

        /* Format again with correct size */
        vsnprintf(buf, needed + 1, format, args);
    }

    *strp = buf;
    return needed;
}

int asprintf(char** strp, const char* format, ...) {
    /* Allocate and format string with variadic arguments */
    va_list args;
    va_start(args, format);
    int result = vasprintf(strp, format, args);
    va_end(args);
    return result;
}

/* Assert implementation */
void __assert_fail(const char* expr, const char* file, int line, const char* func) {
    serial_printf("Assertion failed: %s at %s:%d in %s\n", expr, file, line, func);
    /* In production, could halt or reset system */
}
/* Math library functions for bare metal */
/* Newton-Raphson square root for QuickDraw distance calculations */
double sqrt(double x) {
    if (x < 0.0) return 0.0;
    if (x == 0.0) return 0.0;

    /* Newton-Raphson method for square root */
    double guess = x / 2.0;
    double epsilon = 0.00001;

    for (int i = 0; i < 20; i++) {
        double next = (guess + x / guess) / 2.0;
        if (next - guess < epsilon && guess - next < epsilon) {
            return next;
        }
        guess = next;
    }

    return guess;
}

double frexp(double value, int* exp) {
    /* Split floating point into mantissa and exponent
     * value = mantissa * 2^exp, where 0.5 <= |mantissa| < 1.0 */

    if (value == 0.0) {
        *exp = 0;
        return 0.0;
    }

    int sign = 1;
    if (value < 0.0) {
        sign = -1;
        value = -value;
    }

    int e = 0;

    /* Normalize to range [0.5, 1.0) */
    while (value >= 1.0) {
        value /= 2.0;
        e++;
    }
    while (value < 0.5) {
        value *= 2.0;
        e--;
    }

    *exp = e;
    return value * sign;
}

double ldexp(double value, int exp) {
    /* Multiply value by 2^exp */
    if (exp > 0) {
        while (exp--) {
            value *= 2.0;
        }
    } else if (exp < 0) {
        while (exp++) {
            value /= 2.0;
        }
    }
    return value;
}

double modf(double value, double* iptr) {
    /* Split value into integer and fractional parts */
    double int_part;

    if (value >= 0.0) {
        int_part = (double)(long)value;
    } else {
        int_part = -(double)(long)(-value);
    }

    *iptr = int_part;
    return value - int_part;
}

double hypot(double x, double y) {
    /* Compute sqrt(x^2 + y^2) with overflow protection */
    if (x < 0.0) x = -x;
    if (y < 0.0) y = -y;

    if (x == 0.0) return y;
    if (y == 0.0) return x;

    /* Use larger value to scale and avoid overflow */
    if (x > y) {
        double ratio = y / x;
        return x * sqrt(1.0 + ratio * ratio);
    } else {
        double ratio = x / y;
        return y * sqrt(1.0 + ratio * ratio);
    }
}
