#ifndef PACKAGE_MANAGER_STRING_COMPARE_INTERNAL_H
#define PACKAGE_MANAGER_STRING_COMPARE_INTERNAL_H

#include "SystemTypes.h"

static inline SInt16 PackageManager_CompareBytesIgnoreCase(UInt8 a, UInt8 b)
{
    if (a >= 'A' && a <= 'Z') a = (UInt8)(a + ('a' - 'A'));
    if (b >= 'A' && b <= 'Z') b = (UInt8)(b + ('a' - 'A'));
    return (a > b) - (a < b);
}

#endif /* PACKAGE_MANAGER_STRING_COMPARE_INTERNAL_H */
