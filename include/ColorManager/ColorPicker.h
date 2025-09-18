/*
 * ColorPicker.h - Color Selection Interfaces
 *
 * Color picker interfaces and selection dialogs providing HSV, RGB, CMYK,
 * and other color space selection modes compatible with Mac OS color picker.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Picker
 */

#ifndef COLORPICKER_H
#define COLORPICKER_H

#include "ColorManager.h"
#include "ColorSpaces.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * COLOR PICKER CONSTANTS
 * ================================================================ */

/* Color picker modes */
typedef enum {
    cmPickerRGBMode     = 0,    /* RGB sliders */
    cmPickerHSVMode     = 1,    /* HSV wheel/sliders */
    cmPickerHSLMode     = 2,    /* HSL sliders */
    cmPickerCMYKMode    = 3,    /* CMYK sliders */
    cmPickerGrayMode    = 4,    /* Grayscale slider */
    cmPickerNamedMode   = 5,    /* Named colors */
    cmPickerSwatchMode  = 6,    /* Color swatches */
    cmPickerCrayonMode  = 7,    /* Crayon colors */
    cmPickerWebMode     = 8     /* Web-safe colors */
} CMPickerMode;

/* Color picker flags */
typedef enum {
    cmPickerShowAlpha       = 0x0001,   /* Show alpha channel */
    cmPickerShowPreview     = 0x0002,   /* Show color preview */
    cmPickerAllowCustom     = 0x0004,   /* Allow custom colors */
    cmPickerShowEyedropper  = 0x0008,   /* Show eyedropper tool */
    cmPickerShowSwatches    = 0x0010,   /* Show color swatches */
    cmPickerShowWheel       = 0x0020,   /* Show color wheel */
    cmPickerContinuous      = 0x0040,   /* Continuous updates */
    cmPickerModal           = 0x0080    /* Modal dialog */
} CMPickerFlags;

/* Color picker result codes */
typedef enum {
    cmPickerOK          = 1,    /* User clicked OK */
    cmPickerCancel      = 0,    /* User cancelled */
    cmPickerError       = -1    /* Error occurred */
} CMPickerResult;

/* ================================================================
 * COLOR PICKER STRUCTURES
 * ================================================================ */

/* Color picker dialog configuration */
typedef struct {
    CMPickerMode        mode;           /* Initial picker mode */
    CMPickerFlags       flags;          /* Picker flags */
    CMRGBColor          initialColor;   /* Initial color */
    char                prompt[256];    /* Dialog prompt text */
    char                title[64];      /* Dialog title */
    int16_t             windowX;        /* Window position X */
    int16_t             windowY;        /* Window position Y */
    void               *parentWindow;   /* Parent window handle */
    void               *userData;       /* User data pointer */
} CMPickerConfig;

/* Color picker callback functions */
typedef void (*CMPickerUpdateCallback)(const CMRGBColor *color, void *userData);
typedef bool (*CMPickerValidateCallback)(const CMRGBColor *color, void *userData);

/* Color swatch definition */
typedef struct {
    CMRGBColor          color;          /* Swatch color */
    char                name[32];       /* Swatch name */
    uint32_t            index;          /* Swatch index */
    bool                isCustom;       /* Custom vs. predefined */
} CMColorSwatch;

/* Color palette definition */
typedef struct {
    char                name[64];       /* Palette name */
    CMColorSwatch      *swatches;       /* Array of swatches */
    uint32_t           swatchCount;     /* Number of swatches */
    bool               isReadOnly;      /* Read-only palette */
} CMColorPalette;

/* ================================================================
 * BASIC COLOR PICKER FUNCTIONS
 * ================================================================ */

/* Simple color picker dialog */
bool CMGetColor(int16_t where_h, int16_t where_v, const char *prompt,
               const CMRGBColor *inColor, CMRGBColor *outColor);

/* Extended color picker dialog */
CMPickerResult CMShowColorPicker(const CMPickerConfig *config, CMRGBColor *selectedColor);

/* Color picker with callback */
CMPickerResult CMShowColorPickerWithCallback(const CMPickerConfig *config,
                                            CMPickerUpdateCallback updateCallback,
                                            CMPickerValidateCallback validateCallback,
                                            CMRGBColor *selectedColor);

/* ================================================================
 * COLOR SPACE CONVERSIONS IN PICKER
 * ================================================================ */

/* Convert between picker formats */
CMError CMConvertToPickerRGB(const CMColor *input, CMRGBColor *output);
CMError CMConvertFromPickerRGB(const CMRGBColor *input, CMColor *output, CMColorSpace space);

/* Legacy picker compatibility */
uint16_t CMFix2SmallFract(int32_t fixed);
int32_t CMSmallFract2Fix(uint16_t fract);

/* Color space conversions for picker */
CMError CMCMY2RGB(const CMCMYColor *cColor, CMRGBColor *rColor);
CMError CMRGB2CMY(const CMRGBColor *rColor, CMCMYColor *cColor);
CMError CMHSL2RGB(const CMHLSColor *hColor, CMRGBColor *rColor);
CMError CMRGB2HSL(const CMRGBColor *rColor, CMHLSColor *hColor);
CMError CMHSV2RGB(const CMHSVColor *hColor, CMRGBColor *rColor);
CMError CMRGB2HSV(const CMRGBColor *rColor, CMHSVColor *hColor);

/* ================================================================
 * COLOR SWATCHES AND PALETTES
 * ================================================================ */

/* Create color palette */
CMError CMCreateColorPalette(const char *name, CMColorPalette **palette);

/* Load palette from file */
CMError CMLoadColorPalette(const char *filename, CMColorPalette **palette);

/* Save palette to file */
CMError CMSaveColorPalette(const CMColorPalette *palette, const char *filename);

/* Dispose color palette */
void CMDisposeColorPalette(CMColorPalette *palette);

/* Add swatch to palette */
CMError CMAddSwatchToPalette(CMColorPalette *palette, const CMRGBColor *color, const char *name);

/* Remove swatch from palette */
CMError CMRemoveSwatchFromPalette(CMColorPalette *palette, uint32_t index);

/* Get swatch from palette */
CMError CMGetSwatchFromPalette(const CMColorPalette *palette, uint32_t index, CMColorSwatch *swatch);

/* Find swatch by color */
CMError CMFindSwatchByColor(const CMColorPalette *palette, const CMRGBColor *color,
                           float tolerance, uint32_t *index);

/* Find swatch by name */
CMError CMFindSwatchByName(const CMColorPalette *palette, const char *name, uint32_t *index);

/* ================================================================
 * STANDARD COLOR PALETTES
 * ================================================================ */

/* Get system color palette */
CMError CMGetSystemColorPalette(CMColorPalette **palette);

/* Get web-safe color palette */
CMError CMGetWebSafeColorPalette(CMColorPalette **palette);

/* Get crayon color palette */
CMError CMGetCrayonColorPalette(CMColorPalette **palette);

/* Get grayscale palette */
CMError CMGetGrayscalePalette(uint32_t steps, CMColorPalette **palette);

/* Create rainbow palette */
CMError CMCreateRainbowPalette(uint32_t steps, CMColorPalette **palette);

/* ================================================================
 * CUSTOM COLOR MANAGEMENT
 * ================================================================ */

/* Custom color storage */
typedef struct {
    CMRGBColor          colors[16];     /* 16 custom colors */
    char                names[16][32];  /* Custom color names */
    bool                isUsed[16];     /* Slot usage flags */
} CMCustomColors;

/* Get custom colors */
CMError CMGetCustomColors(CMCustomColors *customColors);

/* Set custom colors */
CMError CMSetCustomColors(const CMCustomColors *customColors);

/* Add custom color */
CMError CMAddCustomColor(const CMRGBColor *color, const char *name, uint32_t *slot);

/* Remove custom color */
CMError CMRemoveCustomColor(uint32_t slot);

/* ================================================================
 * COLOR HARMONY AND GENERATION
 * ================================================================ */

/* Color harmony types */
typedef enum {
    cmHarmonyComplementary  = 0,    /* Complementary colors */
    cmHarmonyTriadic       = 1,    /* Triadic colors */
    cmHarmonyAnalogous     = 2,    /* Analogous colors */
    cmHarmonyMonochromatic = 3,    /* Monochromatic colors */
    cmHarmonyTetradic      = 4,    /* Tetradic colors */
    cmHarmonySplitComp     = 5     /* Split complementary */
} CMColorHarmony;

/* Generate color harmony */
CMError CMGenerateColorHarmony(const CMRGBColor *baseColor, CMColorHarmony harmony,
                              CMRGBColor *harmonicColors, uint32_t *colorCount);

/* Generate color tints */
CMError CMGenerateColorTints(const CMRGBColor *baseColor, uint32_t steps,
                            CMRGBColor *tints);

/* Generate color shades */
CMError CMGenerateColorShades(const CMRGBColor *baseColor, uint32_t steps,
                             CMRGBColor *shades);

/* Generate color tones */
CMError CMGenerateColorTones(const CMRGBColor *baseColor, uint32_t steps,
                            CMRGBColor *tones);

/* ================================================================
 * COLOR ACCESSIBILITY
 * ================================================================ */

/* Color vision deficiency types */
typedef enum {
    cmVisionNormal      = 0,    /* Normal color vision */
    cmVisionProtanopia  = 1,    /* Red-blind */
    cmVisionDeuteranopia = 2,   /* Green-blind */
    cmVisionTritanopia  = 3,    /* Blue-blind */
    cmVisionProtanomaly = 4,    /* Red-weak */
    cmVisionDeuteranomaly = 5,  /* Green-weak */
    cmVisionTritanomaly = 6,    /* Blue-weak */
    cmVisionMonochromacy = 7    /* Complete color blindness */
} CMVisionType;

/* Simulate color vision deficiency */
CMError CMSimulateColorVision(const CMRGBColor *original, CMVisionType visionType,
                             CMRGBColor *simulated);

/* Check color contrast */
CMError CMCheckColorContrast(const CMRGBColor *foreground, const CMRGBColor *background,
                            float *contrastRatio, bool *isAccessible);

/* Suggest accessible colors */
CMError CMSuggestAccessibleColors(const CMRGBColor *baseColor, bool isDarkBackground,
                                 CMRGBColor *suggestions, uint32_t *suggestionCount);

/* ================================================================
 * EYEDROPPER TOOL
 * ================================================================ */

/* Eyedropper configuration */
typedef struct {
    int16_t             hotspotX;       /* Hotspot X offset */
    int16_t             hotspotY;       /* Hotspot Y offset */
    int16_t             sampleSize;     /* Sample area size */
    bool                showPreview;    /* Show preview window */
    bool                showCoords;     /* Show coordinates */
    bool                averagePixels;  /* Average sample area */
} CMEyedropperConfig;

/* Eyedropper callback */
typedef void (*CMEyedropperCallback)(int16_t x, int16_t y, const CMRGBColor *color, void *userData);

/* Start eyedropper tool */
CMError CMStartEyedropper(const CMEyedropperConfig *config, CMEyedropperCallback callback, void *userData);

/* Stop eyedropper tool */
void CMStopEyedropper(void);

/* Get pixel color at coordinates */
CMError CMGetPixelColor(int16_t x, int16_t y, CMRGBColor *color);

/* Get average color in area */
CMError CMGetAverageColor(int16_t x, int16_t y, int16_t width, int16_t height, CMRGBColor *color);

/* ================================================================
 * COLOR PICKER EXTENSIONS
 * ================================================================ */

/* Color picker plugin interface */
typedef struct {
    char                name[64];       /* Picker name */
    CMPickerMode        mode;           /* Picker mode */
    void               *pluginData;     /* Plugin-specific data */

    /* Plugin callbacks */
    CMError (*initialize)(void *pluginData);
    CMError (*showPicker)(const CMPickerConfig *config, CMRGBColor *color);
    void (*cleanup)(void *pluginData);
} CMColorPickerPlugin;

/* Register color picker plugin */
CMError CMRegisterColorPickerPlugin(const CMColorPickerPlugin *plugin);

/* Unregister color picker plugin */
CMError CMUnregisterColorPickerPlugin(const char *name);

/* Get available picker modes */
CMError CMGetAvailablePickerModes(CMPickerMode *modes, uint32_t *count);

/* ================================================================
 * PLATFORM INTEGRATION
 * ================================================================ */

/* Platform-specific color picker */
CMPickerResult CMShowNativeColorPicker(const CMPickerConfig *config, CMRGBColor *selectedColor);

/* Set platform color picker preferences */
CMError CMSetPlatformPickerPreferences(const CMPickerConfig *config);

/* Get platform color picker capabilities */
CMError CMGetPlatformPickerCapabilities(uint32_t *capabilities);

#ifdef __cplusplus
}
#endif

#endif /* COLORPICKER_H */