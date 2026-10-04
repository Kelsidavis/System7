#ifndef INPUT_TEST_CHECK_H
#define INPUT_TEST_CHECK_H

#include <stdio.h>

#define CHECK(condition, code) do { \
    if (!(condition)) { \
        printf("%s:%d: input regression failed: %s\n", __FILE__, __LINE__, #condition); \
        return (code); \
    } \
} while (0)

#endif
