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
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);


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
