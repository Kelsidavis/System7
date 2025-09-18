/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * ColorManager_HAL.h - Hardware Abstraction Layer for Color Manager
 * Platform-specific color management interfaces
 */

#ifndef COLORMANAGER_HAL_H
#define COLORMANAGER_HAL_H

#include "ColorManager/ColorTypes.h"

/* HAL Initialization */
OSErr ColorManager_HAL_Init(void);
void ColorManager_HAL_Cleanup(void);

/* Color Conversion */
uint32_t ColorManager_HAL_RGBToNative(const RGBColor *rgb);
void ColorManager_HAL_NativeToRGB(uint32_t native, RGBColor *rgb);

/* Display Information */
uint32_t ColorManager_HAL_GetDisplayDepth(void);
Boolean ColorManager_HAL_HasHardwareAccel(void);
Boolean ColorManager_HAL_SupportsWideGamut(void);

/* Color Space Conversion */
void ColorManager_HAL_RGBToHSV(const RGBColor *rgb, float *h, float *s, float *v);
void ColorManager_HAL_HSVToRGB(float h, float s, float v, RGBColor *rgb);
uint16_t ColorManager_HAL_RGBToGrayscale(const RGBColor *rgb);

/* Color Processing */
void ColorManager_HAL_GammaCorrect(RGBColor *rgb, float gamma);
void ColorManager_HAL_BlendColors(const RGBColor *src, const RGBColor *dst,
                                 RGBColor *result, float alpha);
void ColorManager_HAL_ApplyColorMatrix(RGBColor *rgb, const float matrix[9]);
void ColorManager_HAL_DitherColor(const RGBColor *src, RGBColor *dst, uint32_t targetDepth);

/* System Integration */
OSErr ColorManager_HAL_GetSystemPalette(CTabHandle *palette);
OSErr ColorManager_HAL_SetDisplayGamma(float gamma);

#endif /* COLORMANAGER_HAL_H */