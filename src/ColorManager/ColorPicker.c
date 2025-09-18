/*
 * ColorPicker.c - Color Selection Interfaces Implementation
 *
 * Implementation of color picker interfaces and selection dialogs providing
 * HSV, RGB, CMYK, and other color space selection modes.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Picker
 */

#include "../include/ColorManager/ColorPicker.h"
#include "../include/ColorManager/ColorSpaces.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

/* ================================================================
 * CONSTANTS AND DATA
 * ================================================================ */

/* Crayon colors (from original Mac OS) */
static const CMColorSwatch g_crayonColors[] = {
    {{65535,     0,     0}, "Red",          0, false},
    {{65535, 32768,     0}, "Orange",       1, false},
    {{65535, 65535,     0}, "Yellow",       2, false},
    {{    0, 65535,     0}, "Green",        3, false},
    {{    0,     0, 65535}, "Blue",         4, false},
    {{32768,     0, 65535}, "Purple",       5, false},
    {{65535,     0, 65535}, "Magenta",      6, false},
    {{    0, 65535, 65535}, "Cyan",         7, false},
    {{65535, 65535, 65535}, "White",        8, false},
    {{49152, 49152, 49152}, "Light Gray",  9, false},
    {{32768, 32768, 32768}, "Gray",        10, false},
    {{16384, 16384, 16384}, "Dark Gray",   11, false},
    {{    0,     0,     0}, "Black",       12, false},
    {{39321, 26214, 13107}, "Brown",       13, false},
    {{65535, 49152, 52428}, "Pink",        14, false},
    {{52428, 65535, 49152}, "Light Green", 15, false}
};

#define CRAYON_COLOR_COUNT (sizeof(g_crayonColors) / sizeof(g_crayonColors[0]))

/* Web-safe colors (216 colors) */
static CMColorSwatch *g_webSafeColors = NULL;
static bool g_webSafeColorsInitialized = false;

/* Custom colors storage */
static CMCustomColors g_customColors = {{{0}}};
static bool g_customColorsInitialized = false;

/* Registered picker plugins */
static CMColorPickerPlugin *g_pickerPlugins = NULL;
static uint32_t g_pickerPluginCount = 0;
static uint32_t g_pickerPluginCapacity = 0;

/* Forward declarations */
static CMError InitializeWebSafeColors(void);
static CMError InitializeCustomColors(void);
static CMError ShowBasicColorPicker(const CMPickerConfig *config, CMRGBColor *selectedColor);
static CMError ConvertHSVToPickerCoords(const CMHSVColor *hsv, int16_t *x, int16_t *y);
static CMError ConvertPickerCoordsToHSV(int16_t x, int16_t y, CMHSVColor *hsv);
static float CalculateColorDistance(const CMRGBColor *color1, const CMRGBColor *color2);

/* ================================================================
 * BASIC COLOR PICKER FUNCTIONS
 * ================================================================ */

bool CMGetColor(int16_t where_h, int16_t where_v, const char *prompt,
               const CMRGBColor *inColor, CMRGBColor *outColor) {
    if (!outColor) return false;

    /* Set up picker configuration */
    CMPickerConfig config = {0};
    config.mode = cmPickerHSVMode;
    config.flags = cmPickerShowPreview | cmPickerShowSwatches;
    config.windowX = where_h;
    config.windowY = where_v;

    if (inColor) {
        config.initialColor = *inColor;
    } else {
        config.initialColor.red = 65535;
        config.initialColor.green = 65535;
        config.initialColor.blue = 65535;
    }

    if (prompt) {
        strncpy(config.prompt, prompt, sizeof(config.prompt) - 1);
        config.prompt[sizeof(config.prompt) - 1] = '\0';
    } else {
        strcpy(config.prompt, "Choose a color:");
    }

    strcpy(config.title, "Color Picker");

    /* Show color picker */
    CMPickerResult result = CMShowColorPicker(&config, outColor);
    return (result == cmPickerOK);
}

CMPickerResult CMShowColorPicker(const CMPickerConfig *config, CMRGBColor *selectedColor) {
    if (!config || !selectedColor) return cmPickerError;

    /* Initialize color picker systems */
    if (!g_webSafeColorsInitialized) {
        InitializeWebSafeColors();
    }
    if (!g_customColorsInitialized) {
        InitializeCustomColors();
    }

    /* For now, implement a basic color picker */
    return ShowBasicColorPicker(config, selectedColor);
}

CMPickerResult CMShowColorPickerWithCallback(const CMPickerConfig *config,
                                            CMPickerUpdateCallback updateCallback,
                                            CMPickerValidateCallback validateCallback,
                                            CMRGBColor *selectedColor) {
    if (!config || !selectedColor) return cmPickerError;

    /* This would implement a picker with real-time callbacks */
    /* For now, use basic picker and call callbacks at end */
    CMPickerResult result = CMShowColorPicker(config, selectedColor);

    if (result == cmPickerOK) {
        if (validateCallback && !validateCallback(selectedColor, config->userData)) {
            return cmPickerError;
        }
        if (updateCallback) {
            updateCallback(selectedColor, config->userData);
        }
    }

    return result;
}

/* ================================================================
 * COLOR SPACE CONVERSIONS IN PICKER
 * ================================================================ */

CMError CMConvertToPickerRGB(const CMColor *input, CMRGBColor *output) {
    if (!input || !output) return cmParameterError;

    /* Assume input is RGB for simplicity */
    *output = input->rgb;
    return cmNoError;
}

CMError CMConvertFromPickerRGB(const CMRGBColor *input, CMColor *output, CMColorSpace space) {
    if (!input || !output) return cmParameterError;

    switch (space) {
        case cmRGBSpace:
            output->rgb = *input;
            break;
        case cmHSVSpace:
            return CMConvertRGBToHSV(input, &output->hsv);
        case cmHLSSpace:
            return CMConvertRGBToHSL(input, &output->hls);
        case cmCMYKSpace:
            return CMConvertRGBToCMYK(input, &output->cmyk);
        case cmGraySpace:
            {
                CMGrayColor gray;
                CMError err = CMConvertRGBToGray(input, &gray);
                if (err == cmNoError) {
                    output->gray = gray.gray;
                }
                return err;
            }
        default:
            return cmUnsupportedDataType;
    }

    return cmNoError;
}

uint16_t CMFix2SmallFract(int32_t fixed) {
    /* Convert 32-bit fixed point to 16-bit fraction */
    return (uint16_t)((fixed & 0xFFFF0000) >> 16);
}

int32_t CMSmallFract2Fix(uint16_t fract) {
    /* Convert 16-bit fraction to 32-bit fixed point */
    return ((int32_t)fract) << 16;
}

/* Legacy picker color space conversions */
CMError CMCMY2RGB(const CMCMYColor *cColor, CMRGBColor *rColor) {
    if (!cColor || !rColor) return cmParameterError;
    return CMConvertCMYKToRGB((const CMCMYKColor *)cColor, rColor);
}

CMError CMRGB2CMY(const CMRGBColor *rColor, CMCMYColor *cColor) {
    if (!rColor || !cColor) return cmParameterError;
    return CMConvertRGBToCMYK(rColor, (CMCMYKColor *)cColor);
}

CMError CMHSL2RGB(const CMHLSColor *hColor, CMRGBColor *rColor) {
    if (!hColor || !rColor) return cmParameterError;
    return CMConvertHSLToRGB(hColor, rColor);
}

CMError CMRGB2HSL(const CMRGBColor *rColor, CMHLSColor *hColor) {
    if (!rColor || !hColor) return cmParameterError;
    return CMConvertRGBToHSL(rColor, hColor);
}

CMError CMHSV2RGB(const CMHSVColor *hColor, CMRGBColor *rColor) {
    if (!hColor || !rColor) return cmParameterError;
    return CMConvertHSVToRGB(hColor, rColor);
}

CMError CMRGB2HSV(const CMRGBColor *rColor, CMHSVColor *hColor) {
    if (!rColor || !hColor) return cmParameterError;
    return CMConvertRGBToHSV(rColor, hColor);
}

/* ================================================================
 * COLOR SWATCHES AND PALETTES
 * ================================================================ */

CMError CMCreateColorPalette(const char *name, CMColorPalette **palette) {
    if (!name || !palette) return cmParameterError;

    CMColorPalette *newPalette = (CMColorPalette *)calloc(1, sizeof(CMColorPalette));
    if (!newPalette) return cmProfileError;

    strncpy(newPalette->name, name, sizeof(newPalette->name) - 1);
    newPalette->name[sizeof(newPalette->name) - 1] = '\0';

    newPalette->swatchCount = 0;
    newPalette->swatches = NULL;
    newPalette->isReadOnly = false;

    *palette = newPalette;
    return cmNoError;
}

CMError CMLoadColorPalette(const char *filename, CMColorPalette **palette) {
    if (!filename || !palette) return cmParameterError;

    FILE *file = fopen(filename, "rb");
    if (!file) return cmProfileError;

    /* Simple palette file format:
     * - 64-byte name
     * - 4-byte swatch count
     * - For each swatch: 6 bytes RGB + 32 bytes name + 4 bytes index + 1 byte custom flag
     */

    CMColorPalette *newPalette = (CMColorPalette *)calloc(1, sizeof(CMColorPalette));
    if (!newPalette) {
        fclose(file);
        return cmProfileError;
    }

    /* Read palette name */
    if (fread(newPalette->name, 1, 64, file) != 64) {
        free(newPalette);
        fclose(file);
        return cmProfileError;
    }

    /* Read swatch count */
    if (fread(&newPalette->swatchCount, 4, 1, file) != 1) {
        free(newPalette);
        fclose(file);
        return cmProfileError;
    }

    /* Allocate swatches */
    if (newPalette->swatchCount > 0) {
        newPalette->swatches = (CMColorSwatch *)calloc(newPalette->swatchCount, sizeof(CMColorSwatch));
        if (!newPalette->swatches) {
            free(newPalette);
            fclose(file);
            return cmProfileError;
        }

        /* Read swatches */
        for (uint32_t i = 0; i < newPalette->swatchCount; i++) {
            CMColorSwatch *swatch = &newPalette->swatches[i];

            /* Read RGB values */
            uint16_t rgb[3];
            if (fread(rgb, 2, 3, file) != 3) {
                CMDisposeColorPalette(newPalette);
                fclose(file);
                return cmProfileError;
            }
            swatch->color.red = rgb[0];
            swatch->color.green = rgb[1];
            swatch->color.blue = rgb[2];

            /* Read name */
            if (fread(swatch->name, 1, 32, file) != 32) {
                CMDisposeColorPalette(newPalette);
                fclose(file);
                return cmProfileError;
            }

            /* Read index and custom flag */
            if (fread(&swatch->index, 4, 1, file) != 1 ||
                fread(&swatch->isCustom, 1, 1, file) != 1) {
                CMDisposeColorPalette(newPalette);
                fclose(file);
                return cmProfileError;
            }
        }
    }

    fclose(file);
    newPalette->isReadOnly = true; /* Loaded palettes are read-only by default */
    *palette = newPalette;
    return cmNoError;
}

CMError CMSaveColorPalette(const CMColorPalette *palette, const char *filename) {
    if (!palette || !filename) return cmParameterError;

    FILE *file = fopen(filename, "wb");
    if (!file) return cmProfileError;

    /* Write palette name */
    char paddedName[64] = {0};
    strncpy(paddedName, palette->name, 63);
    if (fwrite(paddedName, 1, 64, file) != 64) {
        fclose(file);
        return cmProfileError;
    }

    /* Write swatch count */
    if (fwrite(&palette->swatchCount, 4, 1, file) != 1) {
        fclose(file);
        return cmProfileError;
    }

    /* Write swatches */
    for (uint32_t i = 0; i < palette->swatchCount; i++) {
        const CMColorSwatch *swatch = &palette->swatches[i];

        /* Write RGB values */
        uint16_t rgb[3] = {swatch->color.red, swatch->color.green, swatch->color.blue};
        if (fwrite(rgb, 2, 3, file) != 3) {
            fclose(file);
            return cmProfileError;
        }

        /* Write name */
        char paddedSwatchName[32] = {0};
        strncpy(paddedSwatchName, swatch->name, 31);
        if (fwrite(paddedSwatchName, 1, 32, file) != 32) {
            fclose(file);
            return cmProfileError;
        }

        /* Write index and custom flag */
        if (fwrite(&swatch->index, 4, 1, file) != 1 ||
            fwrite(&swatch->isCustom, 1, 1, file) != 1) {
            fclose(file);
            return cmProfileError;
        }
    }

    fclose(file);
    return cmNoError;
}

void CMDisposeColorPalette(CMColorPalette *palette) {
    if (!palette) return;

    if (palette->swatches) {
        free(palette->swatches);
    }
    free(palette);
}

CMError CMAddSwatchToPalette(CMColorPalette *palette, const CMRGBColor *color, const char *name) {
    if (!palette || !color || palette->isReadOnly) return cmParameterError;

    /* Resize swatch array */
    CMColorSwatch *newSwatches = (CMColorSwatch *)realloc(palette->swatches,
        (palette->swatchCount + 1) * sizeof(CMColorSwatch));
    if (!newSwatches) return cmProfileError;

    palette->swatches = newSwatches;

    /* Initialize new swatch */
    CMColorSwatch *newSwatch = &palette->swatches[palette->swatchCount];
    newSwatch->color = *color;
    newSwatch->index = palette->swatchCount;
    newSwatch->isCustom = true;

    if (name) {
        strncpy(newSwatch->name, name, sizeof(newSwatch->name) - 1);
        newSwatch->name[sizeof(newSwatch->name) - 1] = '\0';
    } else {
        snprintf(newSwatch->name, sizeof(newSwatch->name), "Swatch %u", palette->swatchCount);
    }

    palette->swatchCount++;
    return cmNoError;
}

CMError CMFindSwatchByColor(const CMColorPalette *palette, const CMRGBColor *color,
                           float tolerance, uint32_t *index) {
    if (!palette || !color || !index) return cmParameterError;

    float bestDistance = 1000000.0f;
    uint32_t bestIndex = 0;
    bool found = false;

    for (uint32_t i = 0; i < palette->swatchCount; i++) {
        float distance = CalculateColorDistance(&palette->swatches[i].color, color);
        if (distance <= tolerance && distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
            found = true;
        }
    }

    if (found) {
        *index = bestIndex;
        return cmNoError;
    }

    return cmNamedColorNotFound;
}

/* ================================================================
 * STANDARD COLOR PALETTES
 * ================================================================ */

CMError CMGetSystemColorPalette(CMColorPalette **palette) {
    if (!palette) return cmParameterError;

    CMError err = CMCreateColorPalette("System Colors", palette);
    if (err != cmNoError) return err;

    /* Add standard system colors */
    const CMRGBColor systemColors[] = {
        {65535, 65535, 65535}, /* White */
        {49152, 49152, 49152}, /* Light Gray */
        {32768, 32768, 32768}, /* Gray */
        {16384, 16384, 16384}, /* Dark Gray */
        {    0,     0,     0}, /* Black */
        {65535,     0,     0}, /* Red */
        {    0, 65535,     0}, /* Green */
        {    0,     0, 65535}  /* Blue */
    };

    const char *systemColorNames[] = {
        "White", "Light Gray", "Gray", "Dark Gray", "Black", "Red", "Green", "Blue"
    };

    for (int i = 0; i < 8; i++) {
        CMAddSwatchToPalette(*palette, &systemColors[i], systemColorNames[i]);
    }

    (*palette)->isReadOnly = true;
    return cmNoError;
}

CMError CMGetWebSafeColorPalette(CMColorPalette **palette) {
    if (!palette) return cmParameterError;

    if (!g_webSafeColorsInitialized) {
        CMError err = InitializeWebSafeColors();
        if (err != cmNoError) return err;
    }

    CMError err = CMCreateColorPalette("Web Safe Colors", palette);
    if (err != cmNoError) return err;

    /* Copy web-safe colors to palette */
    (*palette)->swatches = (CMColorSwatch *)malloc(216 * sizeof(CMColorSwatch));
    if (!(*palette)->swatches) {
        CMDisposeColorPalette(*palette);
        return cmProfileError;
    }

    memcpy((*palette)->swatches, g_webSafeColors, 216 * sizeof(CMColorSwatch));
    (*palette)->swatchCount = 216;
    (*palette)->isReadOnly = true;

    return cmNoError;
}

CMError CMGetCrayonColorPalette(CMColorPalette **palette) {
    if (!palette) return cmParameterError;

    CMError err = CMCreateColorPalette("Crayons", palette);
    if (err != cmNoError) return err;

    /* Copy crayon colors to palette */
    (*palette)->swatches = (CMColorSwatch *)malloc(CRAYON_COLOR_COUNT * sizeof(CMColorSwatch));
    if (!(*palette)->swatches) {
        CMDisposeColorPalette(*palette);
        return cmProfileError;
    }

    memcpy((*palette)->swatches, g_crayonColors, CRAYON_COLOR_COUNT * sizeof(CMColorSwatch));
    (*palette)->swatchCount = CRAYON_COLOR_COUNT;
    (*palette)->isReadOnly = true;

    return cmNoError;
}

/* ================================================================
 * COLOR HARMONY AND GENERATION
 * ================================================================ */

CMError CMGenerateColorHarmony(const CMRGBColor *baseColor, CMColorHarmony harmony,
                              CMRGBColor *harmonicColors, uint32_t *colorCount) {
    if (!baseColor || !harmonicColors || !colorCount) return cmParameterError;

    /* Convert base color to HSV */
    CMHSVColor baseHSV;
    CMError err = CMConvertRGBToHSV(baseColor, &baseHSV);
    if (err != cmNoError) return err;

    uint32_t numColors = 0;

    switch (harmony) {
        case cmHarmonyComplementary:
            numColors = 2;
            if (*colorCount < numColors) {
                *colorCount = numColors;
                return cmBufferTooSmall;
            }

            harmonicColors[0] = *baseColor;

            /* Complementary: 180 degrees opposite */
            baseHSV.hue = (baseHSV.hue + 32768) % 65536; /* Add 180 degrees */
            CMConvertHSVToRGB(&baseHSV, &harmonicColors[1]);
            break;

        case cmHarmonyTriadic:
            numColors = 3;
            if (*colorCount < numColors) {
                *colorCount = numColors;
                return cmBufferTooSmall;
            }

            harmonicColors[0] = *baseColor;

            /* Triadic: 120 degrees apart */
            baseHSV.hue = (baseHSV.hue + 21845) % 65536; /* Add 120 degrees */
            CMConvertHSVToRGB(&baseHSV, &harmonicColors[1]);

            baseHSV.hue = (baseHSV.hue + 21845) % 65536; /* Add another 120 degrees */
            CMConvertHSVToRGB(&baseHSV, &harmonicColors[2]);
            break;

        case cmHarmonyAnalogous:
            numColors = 3;
            if (*colorCount < numColors) {
                *colorCount = numColors;
                return cmBufferTooSmall;
            }

            /* Analogous: +/- 30 degrees */
            baseHSV.hue = (baseHSV.hue - 5461) % 65536; /* -30 degrees */
            CMConvertHSVToRGB(&baseHSV, &harmonicColors[0]);

            harmonicColors[1] = *baseColor;

            baseHSV.hue = (baseHSV.hue + 10922) % 65536; /* +60 degrees from -30 */
            CMConvertHSVToRGB(&baseHSV, &harmonicColors[2]);
            break;

        case cmHarmonyMonochromatic:
            numColors = 5;
            if (*colorCount < numColors) {
                *colorCount = numColors;
                return cmBufferTooSmall;
            }

            /* Monochromatic: same hue, different saturation/value */
            for (uint32_t i = 0; i < numColors; i++) {
                CMHSVColor monoHSV = baseHSV;
                monoHSV.saturation = (uint16_t)(baseHSV.saturation * (0.2f + 0.8f * i / (numColors - 1)));
                monoHSV.value = (uint16_t)(baseHSV.value * (0.3f + 0.7f * i / (numColors - 1)));
                CMConvertHSVToRGB(&monoHSV, &harmonicColors[i]);
            }
            break;

        default:
            return cmParameterError;
    }

    *colorCount = numColors;
    return cmNoError;
}

/* ================================================================
 * COLOR ACCESSIBILITY
 * ================================================================ */

CMError CMSimulateColorVision(const CMRGBColor *original, CMVisionType visionType,
                             CMRGBColor *simulated) {
    if (!original || !simulated) return cmParameterError;

    if (visionType == cmVisionNormal) {
        *simulated = *original;
        return cmNoError;
    }

    /* Convert to linear RGB */
    float r = original->red / 65535.0f;
    float g = original->green / 65535.0f;
    float b = original->blue / 65535.0f;

    /* Apply color vision simulation matrices */
    float newR, newG, newB;

    switch (visionType) {
        case cmVisionProtanopia: /* Red-blind */
            newR = 0.567f * r + 0.433f * g + 0.0f * b;
            newG = 0.558f * r + 0.442f * g + 0.0f * b;
            newB = 0.0f * r + 0.242f * g + 0.758f * b;
            break;

        case cmVisionDeuteranopia: /* Green-blind */
            newR = 0.625f * r + 0.375f * g + 0.0f * b;
            newG = 0.7f * r + 0.3f * g + 0.0f * b;
            newB = 0.0f * r + 0.3f * g + 0.7f * b;
            break;

        case cmVisionTritanopia: /* Blue-blind */
            newR = 0.95f * r + 0.05f * g + 0.0f * b;
            newG = 0.0f * r + 0.433f * g + 0.567f * b;
            newB = 0.0f * r + 0.475f * g + 0.525f * b;
            break;

        case cmVisionMonochromacy: /* Complete color blindness */
            {
                float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
                newR = newG = newB = luminance;
            }
            break;

        default:
            *simulated = *original;
            return cmNoError;
    }

    /* Clamp and convert back to 16-bit */
    simulated->red = (uint16_t)(fmaxf(0.0f, fminf(1.0f, newR)) * 65535.0f);
    simulated->green = (uint16_t)(fmaxf(0.0f, fminf(1.0f, newG)) * 65535.0f);
    simulated->blue = (uint16_t)(fmaxf(0.0f, fminf(1.0f, newB)) * 65535.0f);

    return cmNoError;
}

CMError CMCheckColorContrast(const CMRGBColor *foreground, const CMRGBColor *background,
                            float *contrastRatio, bool *isAccessible) {
    if (!foreground || !background || !contrastRatio) return cmParameterError;

    /* Calculate relative luminance */
    auto calcLuminance = [](const CMRGBColor *color) -> float {
        float r = color->red / 65535.0f;
        float g = color->green / 65535.0f;
        float b = color->blue / 65535.0f;

        /* Apply gamma correction */
        r = (r <= 0.03928f) ? r / 12.92f : powf((r + 0.055f) / 1.055f, 2.4f);
        g = (g <= 0.03928f) ? g / 12.92f : powf((g + 0.055f) / 1.055f, 2.4f);
        b = (b <= 0.03928f) ? b / 12.92f : powf((b + 0.055f) / 1.055f, 2.4f);

        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    };

    float fgLuminance = calcLuminance(foreground);
    float bgLuminance = calcLuminance(background);

    /* Calculate contrast ratio */
    float lighter = fmaxf(fgLuminance, bgLuminance);
    float darker = fminf(fgLuminance, bgLuminance);
    *contrastRatio = (lighter + 0.05f) / (darker + 0.05f);

    /* Check WCAG accessibility standards */
    if (isAccessible) {
        *isAccessible = (*contrastRatio >= 4.5f); /* WCAG AA standard */
    }

    return cmNoError;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static CMError InitializeWebSafeColors(void) {
    if (g_webSafeColorsInitialized) return cmNoError;

    g_webSafeColors = (CMColorSwatch *)malloc(216 * sizeof(CMColorSwatch));
    if (!g_webSafeColors) return cmProfileError;

    /* Generate 216 web-safe colors (6x6x6 cube) */
    uint32_t index = 0;
    const uint16_t values[6] = {0, 13107, 26214, 39321, 52428, 65535}; /* 0, 51, 102, 153, 204, 255 in 16-bit */

    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 6; g++) {
            for (int b = 0; b < 6; b++) {
                CMColorSwatch *swatch = &g_webSafeColors[index];
                swatch->color.red = values[r];
                swatch->color.green = values[g];
                swatch->color.blue = values[b];
                swatch->index = index;
                swatch->isCustom = false;
                snprintf(swatch->name, sizeof(swatch->name), "#%02X%02X%02X",
                        values[r] >> 8, values[g] >> 8, values[b] >> 8);
                index++;
            }
        }
    }

    g_webSafeColorsInitialized = true;
    return cmNoError;
}

static CMError InitializeCustomColors(void) {
    if (g_customColorsInitialized) return cmNoError;

    /* Initialize with default colors */
    memset(&g_customColors, 0, sizeof(g_customColors));

    /* Set some default custom colors */
    const CMRGBColor defaultCustoms[] = {
        {65535, 32768, 32768}, /* Light Red */
        {32768, 65535, 32768}, /* Light Green */
        {32768, 32768, 65535}, /* Light Blue */
        {65535, 65535, 32768}, /* Light Yellow */
        {65535, 32768, 65535}, /* Light Magenta */
        {32768, 65535, 65535}  /* Light Cyan */
    };

    for (int i = 0; i < 6 && i < 16; i++) {
        g_customColors.colors[i] = defaultCustoms[i];
        snprintf(g_customColors.names[i], sizeof(g_customColors.names[i]), "Custom %d", i + 1);
        g_customColors.isUsed[i] = true;
    }

    g_customColorsInitialized = true;
    return cmNoError;
}

static CMError ShowBasicColorPicker(const CMPickerConfig *config, CMRGBColor *selectedColor) {
    /* This is a simplified implementation that would normally show a GUI dialog.
     * For demonstration, we'll implement basic console-based interaction or
     * return a default color based on the configuration. */

    *selectedColor = config->initialColor;

    /* In a real implementation, this would:
     * 1. Create a color picker window with the specified mode
     * 2. Handle user interaction (mouse clicks, slider changes, etc.)
     * 3. Update the color preview in real time
     * 4. Return the selected color when user clicks OK
     */

    printf("Color Picker: %s\n", config->prompt);
    printf("Initial color: R=%d G=%d B=%d\n",
           config->initialColor.red, config->initialColor.green, config->initialColor.blue);

    /* For demonstration, modify the color slightly */
    if (selectedColor->red < 32768) selectedColor->red += 1000;
    if (selectedColor->green < 32768) selectedColor->green += 1000;
    if (selectedColor->blue < 32768) selectedColor->blue += 1000;

    return cmPickerOK;
}

static float CalculateColorDistance(const CMRGBColor *color1, const CMRGBColor *color2) {
    float dr = (float)(color1->red - color2->red) / 65535.0f;
    float dg = (float)(color1->green - color2->green) / 65535.0f;
    float db = (float)(color1->blue - color2->blue) / 65535.0f;

    return sqrtf(dr * dr + dg * dg + db * db);
}