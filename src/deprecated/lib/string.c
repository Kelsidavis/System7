/*
 * string.c - Minimal string function implementations for bare-metal kernel
 */

#include <string.h>
#include <stddef.h>
#include <stdlib.h>

/* Memory functions */

void *memcpy(void *dest, const void *src, size_t n)
{
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;

	for (size_t i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;

	if (d < s) {
		/* Copy forward */
		for (size_t i = 0; i < n; i++) {
			d[i] = s[i];
		}
	} else if (d > s) {
		/* Copy backward to avoid overlap */
		for (size_t i = n; i > 0; i--) {
			d[i - 1] = s[i - 1];
		}
	}
	return dest;
}

void *memset(void *s, int c, size_t n)
{
	unsigned char *p = (unsigned char *)s;
	unsigned char val = (unsigned char)c;

	for (size_t i = 0; i < n; i++) {
		p[i] = val;
	}
	return s;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
	const unsigned char *p1 = (const unsigned char *)s1;
	const unsigned char *p2 = (const unsigned char *)s2;

	for (size_t i = 0; i < n; i++) {
		if (p1[i] != p2[i]) {
			return p1[i] - p2[i];
		}
	}
	return 0;
}

/* String functions */

char *strcpy(char *dest, const char *src)
{
	char *d = dest;
	const char *s = src;

	while (*s) {
		*d++ = *s++;
	}
	*d = '\0';
	return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	size_t i;

	for (i = 0; i < n && src[i]; i++) {
		dest[i] = src[i];
	}
	for (; i < n; i++) {
		dest[i] = '\0';
	}
	return dest;
}

char *strcat(char *dest, const char *src)
{
	char *d = dest;

	/* Find end of dest */
	while (*d) {
		d++;
	}
	/* Append src */
	while (*src) {
		*d++ = *src++;
	}
	*d = '\0';
	return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
	char *d = dest;

	/* Find end of dest */
	while (*d) {
		d++;
	}
	/* Append up to n chars from src */
	for (size_t i = 0; i < n && src[i]; i++) {
		*d++ = src[i];
	}
	*d = '\0';
	return dest;
}

size_t strlen(const char *s)
{
	size_t len = 0;
	while (*s++) {
		len++;
	}
	return len;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (s1[i] != s2[i]) {
			return (unsigned char)s1[i] - (unsigned char)s2[i];
		}
		if (!s1[i]) {
			break;
		}
	}
	return 0;
}

char *strchr(const char *s, int c)
{
	unsigned char ch = (unsigned char)c;

	while (*s) {
		if ((unsigned char)*s == ch) {
			return (char *)s;
		}
		s++;
	}
	if (ch == '\0') {
		return (char *)s;
	}
	return NULL;
}

char *strrchr(const char *s, int c)
{
	const char *p = NULL;
	unsigned char ch = (unsigned char)c;

	while (*s) {
		if ((unsigned char)*s == ch) {
			p = s;
		}
		s++;
	}
	if (ch == '\0') {
		return (char *)s;
	}
	return (char *)p;
}

char *strstr(const char *haystack, const char *needle)
{
	size_t needle_len = strlen(needle);

	if (needle_len == 0) {
		return (char *)haystack;
	}

	while (*haystack) {
		if (strncmp(haystack, needle, needle_len) == 0) {
			return (char *)haystack;
		}
		haystack++;
	}
	return NULL;
}

char *strdup(const char *s)
{
	if (!s) {
		return NULL;
	}

	size_t len = strlen(s) + 1;
	char *dup = (char *)malloc(len);
	if (dup) {
		memcpy(dup, s, len);
	}
	return dup;
}

char *strerror(int errnum)
{
	/* Minimal error string handling for bare-metal environment */
	static char error_buffer[32];

	switch (errnum) {
		case 0:
			return "No error";
		case 1:
			return "Operation not permitted";
		case 2:
			return "No such file or directory";
		case 3:
			return "No such process";
		case 4:
			return "Interrupted system call";
		case 5:
			return "Input/output error";
		case 12:
			return "Out of memory";
		default:
			/* For unknown errors, format a number string */
			for (int i = 0; i < 32; i++) {
				error_buffer[i] = 0;
			}

			/* Simple integer to string conversion */
			int num = errnum;
			int pos = 0;

			if (num < 0) {
				error_buffer[pos++] = '-';
				num = -num;
			}

			/* Count digits */
			int temp = num;
			int len = 0;
			if (temp == 0) len = 1;
			else {
				while (temp > 0) {
					len++;
					temp /= 10;
				}
			}

			/* Fill in digits from right to left */
			for (int i = pos + len - 1; i >= pos; i--) {
				error_buffer[i] = '0' + (num % 10);
				num /= 10;
			}
			error_buffer[pos + len] = '\0';

			return error_buffer;
	}
}
