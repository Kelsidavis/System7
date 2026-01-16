/*
 * Simple printf implementation for ARM64 kernel
 * Minimal snprintf for integer and hex formatting
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

static void reverse_string(char *str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static int uint_to_str(uint64_t value, char *buf, int base) {
    if (value == 0) {
        buf[0] = '0';
        return 1;
    }

    int i = 0;
    while (value > 0) {
        int digit = value % base;
        buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    }

    reverse_string(buf, i);
    return i;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    if (!str || size == 0) return 0;

    va_list args;
    va_start(args, format);

    char *dst = str;
    const char *src = format;
    size_t remaining = size - 1;  /* Reserve space for null terminator */

    while (*src && remaining > 0) {
        if (*src == '%') {
            src++;

            /* Handle %% */
            if (*src == '%') {
                *dst++ = '%';
                remaining--;
                src++;
                continue;
            }

            /* Handle format specifiers */
            int is_long = 0;
            int is_long_long = 0;

            /* Check for length modifiers */
            while (*src == 'l') {
                if (is_long) {
                    is_long_long = 1;
                    is_long = 0;
                }
               else {
                    is_long = 1;
                }
                src++;
            }

            /* Get the value */
            uint64_t value = 0;
            if (is_long_long || is_long) {
                value = va_arg(args, uint64_t);
            } else {
                value = va_arg(args, unsigned int);
            }

            /* Format based on specifier */
            char temp_buf[32];
            int len = 0;

            if (*src == 'd' || *src == 'u') {
                len = uint_to_str(value, temp_buf, 10);
            } else if (*src == 'x' || *src == 'X') {
                len = uint_to_str(value, temp_buf, 16);
            } else if (*src == 's') {
                /* String */
                const char *s = (const char *)value;
                if (!s) s = "(null)";
                while (*s && remaining > 0) {
                    *dst++ = *s++;
                    remaining--;
                }
                src++;
                continue;
            } else {
                /* Unknown specifier, just skip */
                src++;
                continue;
            }

            /* Copy the formatted number */
            for (int i = 0; i < len && remaining > 0; i++) {
                *dst++ = temp_buf[i];
                remaining--;
            }

            src++;
        } else {
            *dst++ = *src++;
            remaining--;
        }
    }

    *dst = '\0';
    va_end(args);

    return dst - str;
}
