/* System 7.1 Standard Library Functions */

#ifndef SYSTEM71_STDLIB_H
#define SYSTEM71_STDLIB_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>

/* Define POSIX types if not available */
#ifndef __useconds_t_defined
#define __useconds_t_defined
typedef uint32_t useconds_t;
#endif

#ifndef __ssize_t_defined
#define __ssize_t_defined
typedef int32_t ssize_t;
#endif

#ifndef __off_t_defined
#define __off_t_defined
typedef int32_t off_t;
#endif

/* Logging ------------------------------------------------------------------ */

typedef enum {
    kLogLevelError = 0,
    kLogLevelWarn,
    kLogLevelInfo,
    kLogLevelDebug,
    kLogLevelTrace
} SystemLogLevel;

typedef enum {
    kLogModuleGeneral = 0,
    kLogModuleDesktop,
    kLogModuleEvent,
    kLogModuleFinder,
    kLogModuleFileSystem,
    kLogModuleWindow,
    kLogModuleMenu,
    kLogModuleDialog,
    kLogModuleControl,
    kLogModuleFont,
    kLogModuleSound,
    kLogModuleResource,
    kLogModuleStandardFile,
    kLogModuleListManager,
    kLogModuleSystem,
    kLogModuleTextEdit,
    kLogModulePlatform,
    kLogModuleScrap,
    kLogModuleMemory,
    kLogModuleProcess,
    kLogModuleSegmentLoader,
    kLogModuleCPU,
    kLogModuleCount
} SystemLogModule;

void SysLogSetGlobalLevel(SystemLogLevel level);
SystemLogLevel SysLogGetGlobalLevel(void);
void SysLogSetModuleLevel(SystemLogModule module, SystemLogLevel level);
SystemLogLevel SysLogGetModuleLevel(SystemLogModule module);
const char* SysLogModuleName(SystemLogModule module);
void serial_logf(SystemLogModule module, SystemLogLevel level, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Memory functions */
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
void* memchr(const void* s, int c, size_t n);
void bzero(void* s, size_t n);
void explicit_bzero(void* s, size_t n);
int memset_s(void* s, size_t smax, int c, size_t n);
void bcopy(const void* src, void* dest, size_t n);
int bcmp(const void* s1, const void* s2, size_t n);

/* String functions */
size_t strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcat(char* dest, const char* src);
char* strchr(const char* s, int c);
const char* sys71_strerror(int errnum);
void perror(const char* s);
size_t strspn(const char* s, const char* accept);
size_t strcspn(const char* s, const char* reject);
char* strpbrk(const char* s, const char* accept);
char* strtok(char* s, const char* delim);
char* strtok_r(char* s, const char* delim, char** saveptr);
char* strsep(char** stringp, const char* delim);
size_t strlcpy(char* dst, const char* src, size_t siz);
size_t strlcat(char* dst, const char* src, size_t siz);
char* strcasestr(const char* haystack, const char* needle);
char* index(const char* s, int c);
char* rindex(const char* s, int c);
char* strndup(const char* s, size_t n);
char* strupr(char* s);
char* strlwr(char* s);
char* strrev(char* s);
char* basename(const char* path);
char* dirname(const char* path);
void c2pstrcpy(unsigned char* pstr, const char* cstr);
void p2cstrcpy(char* cstr, const unsigned char* pstr);
unsigned char* CopyCStringToPascal(const char* src, unsigned char* dst);

/* Conversion functions */
int atoi(const char* str);
long atol(const char* str);
double atof(const char* str);
unsigned long strtoul(const char* str, char** endptr, int base);

/* Environment and utility functions */
int atexit(void (*func)(void));
char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
int getopt(int argc, char* const argv[], const char* optstring);

/* Sleep/delay functions */
unsigned int sleep(unsigned int seconds);
int usleep(useconds_t usec);

/* Math functions */
int abs(int n);
long labs(long n);
int min(int a, int b);
int max(int a, int b);
long lmin(long a, long b);
long lmax(long a, long b);
double fmin(double a, double b);
double fmax(double a, double b);

/* Serial output functions */
void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char* str);
int serial_data_ready(void);
char serial_getchar(void);
void serial_print_hex(uint32_t value);
void serial_printf(const char* fmt, ...)
    __attribute__((format(printf, 1, 2)));
#if defined(__arm__) || defined(__aarch64__)
void serial_set_pl011_base(uintptr_t base);
#endif

/* Compatibility aliases used by some platform ports */
#define Serial_WriteString(...) serial_printf(__VA_ARGS__)
#define Serial_Printf(...)      serial_printf(__VA_ARGS__)
int sprintf(char* str, const char* format, ...)
    __attribute__((format(printf, 2, 3)));
int snprintf(char* str, size_t size, const char* format, ...)
    __attribute__((format(printf, 3, 4)));
int printf(const char* format, ...)
    __attribute__((format(printf, 1, 2)));
int vprintf(const char* format, va_list ap)
    __attribute__((format(printf, 1, 0)));
int vsprintf(char* str, const char* format, va_list ap)
    __attribute__((format(printf, 2, 0)));
int asprintf(char** strp, const char* format, ...)
    __attribute__((format(printf, 2, 3)));
int vasprintf(char** strp, const char* format, va_list ap)
    __attribute__((format(printf, 2, 0)));

/* Character classification functions */
/* Note: isalpha, isupper, islower are defined as static inline in KeyboardEvents.c */
int isalnum(int c);
int isdigit(int c);
int isspace(int c);
int isxdigit(int c);
int isprint(int c);
int isgraph(int c);
int iscntrl(int c);
int ispunct(int c);
int isblank(int c);
int isascii(int c);
int toascii(int c);

/* Standard I/O functions */
int putchar(int c);
int puts(const char* s);
int getchar(void);
char* gets(char* s);

/* Utility functions */
int clamp(int value, int min_val, int max_val);
void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*));
void srand(unsigned int seed);
int rand(void);

/* Math functions (extended) */
double frexp(double x, int* exponent);
double ldexp(double x, int exponent);
double modf(double x, double* intpart);
double hypot(double x, double y);

/* POSIX file I/O functions */
int open(const char* pathname, int flags, ...);
int close(int fd);
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
/* Note: lseek is declared in unistd.h (system header) with possibly different off_t type */

/* Division functions */
#ifndef _DIV_T
#define _DIV_T
typedef struct {
    int quot;   /* Quotient */
    int rem;    /* Remainder */
} div_t;
#endif

#ifndef _LDIV_T
#define _LDIV_T
typedef struct {
    long quot;  /* Quotient */
    long rem;   /* Remainder */
} ldiv_t;
#endif

div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);

/* Networking byte order functions */
uint16_t htons(uint16_t hostshort);
uint32_t htonl(uint32_t hostlong);
uint16_t ntohs(uint16_t netshort);
uint32_t ntohl(uint32_t netlong);
uint16_t SwapInt16(uint16_t value);
uint32_t SwapInt32(uint32_t value);

/* Mac Toolbox Pascal string functions */
char* CopyPascalStringToC(const unsigned char* src, char* dst);
unsigned char PLstrlen(const unsigned char* str);
int PLstrcmp(const unsigned char* str1, const unsigned char* str2);
void PLstrcpy(unsigned char* dst, const unsigned char* src);
void PLstrcat(unsigned char* dst, const unsigned char* src);

/* Pointer-to-unsigned-long helper to avoid %p format pitfalls */
#define P2UL(p) ((unsigned long)(uintptr_t)(p))

/*
 * udiv64 - 64-bit unsigned division.
 *
 * The 32-bit freestanding build has no libgcc __udivdi3, so 64-bit division is
 * done by hand. This used to be copy-pasted into four files, each carrying a
 * comment asking the reader to keep it in sync with the others; the original
 * version forgot to shift the divisor left to align it and so saturated instead
 * of dividing (100/3 gave 3, 1000000/16667 gave 32767), which meant TickCount
 * stopped counting. Keep one copy, and keep it covered by make test-stdlib.
 */
uint64_t udiv64(uint64_t num, uint64_t den);

/* System utility functions */
#include "SystemTypes.h"
#ifndef HiWord
SInt16 HiWord(SInt32 x);
#endif
#ifndef LoWord
SInt16 LoWord(SInt32 x);
#endif
void BlockMoveData(const void* srcPtr, void* destPtr, Size byteCount);
void LongMul(SInt32 a, SInt32 b, wide* result);

/* GCC stack checking functions */
void __stack_chk_fail(void);
void __stack_chk_fail_local(void);

#endif /* SYSTEM71_STDLIB_H */
