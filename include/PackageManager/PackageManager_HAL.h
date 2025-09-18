/*
 * PackageManager_HAL.h - Hardware Abstraction Layer for Package Manager
 *
 * Provides platform-specific implementations for package loading,
 * resource management, and package-specific operations.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#ifndef PACKAGEMANAGER_HAL_H
#define PACKAGEMANAGER_HAL_H

#include "PackageManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HAL Initialization */
OSErr PackageManager_HAL_Init(void);
void PackageManager_HAL_Cleanup(void);

/* Package Loading */
OSErr PackageManager_HAL_LoadPackage(short packID, Handle* packHandle);
OSErr PackageManager_HAL_LoadFromResource(short packID, Handle* packHandle);
OSErr PackageManager_HAL_LoadFromFile(short packID, const char* path,
                                     Handle* packHandle);

/* Package Execution */
OSErr PackageManager_HAL_CallPackage(short packID, short selector,
                                     Ptr entryPoint, void* params);

/* Standard File Package HAL */
OSErr PackageManager_HAL_StandardGetFile(void* params);
OSErr PackageManager_HAL_StandardPutFile(void* params);
OSErr PackageManager_HAL_CustomGetFile(void* params);
OSErr PackageManager_HAL_CustomPutFile(void* params);

/* SANE (Floating Point) HAL */
OSErr PackageManager_HAL_SANEOperation(short selector, void* params);
double PackageManager_HAL_FloatAdd(double a, double b);
double PackageManager_HAL_FloatSub(double a, double b);
double PackageManager_HAL_FloatMul(double a, double b);
double PackageManager_HAL_FloatDiv(double a, double b);
double PackageManager_HAL_FloatSqrt(double x);
double PackageManager_HAL_FloatSin(double x);
double PackageManager_HAL_FloatCos(double x);
double PackageManager_HAL_FloatTan(double x);
double PackageManager_HAL_FloatLog(double x);
double PackageManager_HAL_FloatExp(double x);

/* International Utilities HAL */
OSErr PackageManager_HAL_IntlOperation(short selector, void* params);
OSErr PackageManager_HAL_GetIntlResource(short id, Handle* resource);
OSErr PackageManager_HAL_CompareString(const char* s1, const char* s2,
                                       short* result);
OSErr PackageManager_HAL_UppercaseText(char* text, short length);
OSErr PackageManager_HAL_LowercaseText(char* text, short length);

/* Binary-Decimal Conversion HAL */
OSErr PackageManager_HAL_BinaryToDecimal(long binary, char* decimal);
OSErr PackageManager_HAL_DecimalToBinary(const char* decimal, long* binary);

/* Color Picker HAL */
OSErr PackageManager_HAL_ColorPicker(short selector, void* params);
OSErr PackageManager_HAL_ShowColorDialog(RGBColor* color, Boolean* ok);

/* Platform-specific resource loading */
Handle PackageManager_HAL_GetResource(ResType type, short id);
void PackageManager_HAL_ReleaseResource(Handle resource);

/* Package paths */
const char* PackageManager_HAL_GetPackagePath(void);
void PackageManager_HAL_SetPackagePath(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* PACKAGEMANAGER_HAL_H */