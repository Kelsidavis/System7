#ifndef PACKAGE_MANAGER_BINARY_DECIMAL_H
#define PACKAGE_MANAGER_BINARY_DECIMAL_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void NumToString(SInt32 theNum, char* theString);
void StringToNum(const char* theString, SInt32* theNum);

#ifdef __cplusplus
}
#endif

#endif
