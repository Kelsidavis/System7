/* System 7.1 Standard Library Implementation */

#include "System71StdLib.h"
#include "MacTypes.h"

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
    return *(unsigned char*)s1 - *(unsigned char*)s2;
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
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) {
        d++;
    }
    while ((*d++ = *src++));
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == c) {
            return (char*)s;
        }
        s++;
    }
    return NULL;
}

char* strstr(const char* haystack, const char* needle) {
    if (!needle || !*needle) return (char*)haystack;

    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;

        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }

        if (!*n) {
            return (char*)haystack;
        }

        haystack++;
    }
    return NULL;
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

/* Math functions */
int abs(int n) {
    return n < 0 ? -n : n;
}

long labs(long n) {
    return n < 0 ? -n : n;
}

/* Serial I/O functions */
#include <stdint.h>
#include <stdarg.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00);    //                  (hi byte)
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void serial_putchar(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0);
    outb(COM1, c);
}

void serial_puts(const char* str) {
    /* Direct serial output only - no framebuffer interaction */

    /* Direct serial output only - no framebuffer interaction */
    if (!str) return;
    while (*str) {
        if (*str == '\n') {
            while ((inb(COM1 + 5) & 0x20) == 0);
            outb(COM1, '\r');
        }
        while ((inb(COM1 + 5) & 0x20) == 0);
        outb(COM1, *str);
        str++;
    }
}

int serial_data_ready(void) {
    return (inb(COM1 + 5) & 0x01) != 0;
}

char serial_getchar(void) {
    while (!serial_data_ready());
    return inb(COM1);
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

void serial_printf(const char* fmt, ...) {
    /* Only output critical debug messages to avoid lag */
    if (!fmt) return;

    /* Only output HandleMouseDown, MenuSelect, PostEvent, HandleDesktopClick, and IconAtPoint messages */
    if (strstr(fmt, "HandleMouseDown") == NULL &&
        strstr(fmt, "MenuSelect") == NULL &&
        strstr(fmt, "PostEvent") == NULL &&
        strstr(fmt, "HandleDesktopClick") == NULL &&
        strstr(fmt, "Desktop icon") == NULL &&
        strstr(fmt, "IconAtPoint") == NULL &&
        strstr(fmt, "Hit icon") == NULL &&
        strstr(fmt, "Single-click") == NULL &&
        strstr(fmt, "Selection") == NULL &&
        strstr(fmt, "Volume") == NULL &&
        strstr(fmt, "Trash") == NULL &&
        strstr(fmt, "DrawVolumeIcon") == NULL &&
        strstr(fmt, "TrackIconDragSync") == NULL &&
        strstr(fmt, "[PRE-IF]") == NULL &&
        strstr(fmt, "[DBLCLK") == NULL &&
        strstr(fmt, "[WIN_OPEN]") == NULL) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    const char* p = fmt;

    char buffer[256];
    int buf_idx = 0;

    while (*p && buf_idx < 255) {
        if (*p == '%') {
            p++;

            /* Handle %02x format for hex bytes */
            if (*p == '0' && *(p+1) == '2' && *(p+2) == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                const char* hex_digits = "0123456789abcdef";
                buffer[buf_idx++] = hex_digits[(val >> 4) & 0xF];
                if (buf_idx < 255) buffer[buf_idx++] = hex_digits[val & 0xF];
                p += 3;
            }
            /* Handle %d for signed integers */
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
                p++;
            }
            /* Handle %x for hex without leading zeros */
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
                p++;
            }
            /* Handle %08x for hex with leading zeros */
            else if (*p == '0' && *(p+1) == '8' && *(p+2) == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                const char* hex_digits = "0123456789abcdef";
                for (int i = 7; i >= 0 && buf_idx < 255; i--) {
                    buffer[buf_idx++] = hex_digits[(val >> (i * 4)) & 0xF];
                }
                p += 3;
            }
            /* Handle %s for strings */
            else if (*p == 's') {
                const char* s = va_arg(args, const char*);
                while (*s && buf_idx < 255) {
                    buffer[buf_idx++] = *s++;
                }
                p++;
            }
            /* Handle %c for characters */
            else if (*p == 'c') {
                int ch = va_arg(args, int);
                if (buf_idx < 255) buffer[buf_idx++] = (char)ch;
                p++;
            }
            /* Handle %% for literal % */
            else if (*p == '%') {
                if (buf_idx < 255) buffer[buf_idx++] = '%';
                p++;
            }
            /* Unknown format, just print % and continue */
            else {
                if (buf_idx < 255) buffer[buf_idx++] = '%';
            }
        } else {
            if (buf_idx < 255) buffer[buf_idx++] = *p;
            p++;
        }
    }

    buffer[buf_idx] = '\0';
    va_end(args);

    serial_puts(buffer);
}

/* Standard I/O functions */
int sprintf(char* str, const char* format, ...) {
    /* TODO: Implement properly */
    if (str) str[0] = 0;
    return 0;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    /* Minimal implementation - just copy format string */
    if (size == 0) return 0;
    size_t i = 0;
    while (format[i] && i < size - 1) {
        str[i] = format[i];
        i++;
    }
    str[i] = '\0';
    return i;
}

/* Assert implementation */
void __assert_fail(const char* expr, const char* file, int line, const char* func) {
    serial_printf("Assertion failed: %s at %s:%d in %s\n", expr, file, line, func);
    /* In production, could halt or reset system */
}