/*
 * stdio.h - Minimal standard I/O for bare-metal ARM64 kernel
 *
 * Provides minimal declarations for I/O operations in bare-metal environment
 */

#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FILE type stub */
typedef struct {
    int fd;
} FILE;

/* Standard streams - minimal stubs */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Output functions */
int printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
int fprintf(FILE *stream, const char *format, ...) __attribute__((format(printf, 2, 3)));
int sprintf(char *str, const char *format, ...) __attribute__((format(printf, 2, 3)));
int snprintf(char *str, size_t size, const char *format, ...) __attribute__((format(printf, 3, 4)));
int vprintf(const char *format, va_list ap) __attribute__((format(printf, 1, 0)));
int vsprintf(char *str, const char *format, va_list ap) __attribute__((format(printf, 2, 0)));
int vsnprintf(char *str, size_t size, const char *format, va_list ap)
    __attribute__((format(printf, 3, 0)));


/* Character I/O */
int putchar(int c);
int puts(const char *s);
int getchar(void);

/* File operations */
FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);

/* Error reporting */
void perror(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_H_ */
