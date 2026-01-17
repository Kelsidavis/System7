/*
 * ARM64 String Functions Interface
 * Minimal implementations for kernel
 */

#ifndef ARM64_STRING_H
#define ARM64_STRING_H

#include <stddef.h>

/* Memory operations */
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

/* String operations */
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
char *strncpy(char *dest, const char *src, size_t n);

#endif /* ARM64_STRING_H */
