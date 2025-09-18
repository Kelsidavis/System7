/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * FormatNegotiation.c
 *
 * Format negotiation and conversion for Edition Manager
 * Handles data format conversion, negotiation, and compatibility
 *
 * This module enables different applications to share data even when
 * they support different formats, by providing automatic conversion
 */

#include "EditionManager/EditionManager.h"
#include "EditionManager/EditionManagerPrivate.h"
#include "EditionManager/FormatNegotiation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Format converter registry */
typedef struct FormatConverterReg {
    FormatType fromFormat;          /* Source format */
    FormatType toFormat;            /* Target format */
    FormatConverterProc converter;  /* Conversion function */
    int32_t priority;               /* Converter priority */
    uint32_t flags;                 /* Converter flags */
    void* userData;                 /* User data for converter */
    struct FormatConverterReg* next; /* Linked list */
} FormatConverterReg;

/* Format compatibility matrix */
typedef struct {
    FormatType format1;
    FormatType format2;
    float compatibility;            /* 0.0 to 1.0 */
    uint32_t conversionCost;        /* Relative cost */
} FormatCompatibility;

/* Global format registry */
static FormatConverterReg* gConverterRegistry = NULL;
static FormatCompatibility* gCompatibilityMatrix = NULL;
static int32_t gCompatibilityCount = 0;
static int32_t gMaxCompatibilities = 256;

/* Standard format converters */
static OSErr ConvertTextToPICT(const void* inputData, Size inputSize,
                              void** outputData, Size* outputSize, void* userData);
static OSErr ConvertPICTToText(const void* inputData, Size inputSize,
                              void** outputData, Size* outputSize, void* userData);
static OSErr ConvertSoundToAIFF(const void* inputData, Size inputSize,
                               void** outputData, Size* outputSize, void* userData);
static OSErr ConvertRTFToText(const void* inputData, Size inputSize,
                             void** outputData, Size* outputSize, void* userData);

/* Internal helper functions */
static FormatConverterReg* FindConverter(FormatType fromFormat, FormatType toFormat);
static float GetFormatCompatibility(FormatType format1, FormatType format2);
static OSErr InitializeStandardConverters(void);
static OSErr FindConversionPath(FormatType fromFormat, FormatType toFormat,
                               FormatType* path, int32_t* pathLength);

/*
 * InitializeFormatNegotiation
 *
 * Initialize the format negotiation system.
 */
OSErr InitializeFormatNegotiation(void)
{
    /* Allocate compatibility matrix */
    gCompatibilityMatrix = (FormatCompatibility*)malloc(sizeof(FormatCompatibility) * gMaxCompatibilities);
    if (!gCompatibilityMatrix) {
        return editionMgrInitErr;
    }

    memset(gCompatibilityMatrix, 0, sizeof(FormatCompatibility) * gMaxCompatibilities);
    gCompatibilityCount = 0;

    /* Initialize standard format converters */
    OSErr err = InitializeStandardConverters();
    if (err != noErr) {
        free(gCompatibilityMatrix);
        gCompatibilityMatrix = NULL;
        return err;
    }

    return noErr;
}

/*
 * CleanupFormatNegotiation
 *
 * Clean up the format negotiation system.
 */
void CleanupFormatNegotiation(void)
{
    /* Free converter registry */
    FormatConverterReg* current = gConverterRegistry;
    while (current) {
        FormatConverterReg* next = current->next;
        free(current);
        current = next;
    }
    gConverterRegistry = NULL;

    /* Free compatibility matrix */
    if (gCompatibilityMatrix) {
        free(gCompatibilityMatrix);
        gCompatibilityMatrix = NULL;
    }
    gCompatibilityCount = 0;
}

/*
 * RegisterFormatConverter
 *
 * Register a format converter.
 */
OSErr RegisterFormatConverter(FormatType fromFormat, FormatType toFormat,
                             FormatConverterProc converter, int32_t priority)
{
    if (!converter) {
        return badSubPartErr;
    }

    /* Check if converter already exists */
    FormatConverterReg* existing = FindConverter(fromFormat, toFormat);
    if (existing) {
        /* Update existing converter if new one has higher priority */
        if (priority > existing->priority) {
            existing->converter = converter;
            existing->priority = priority;
        }
        return noErr;
    }

    /* Create new converter registration */
    FormatConverterReg* reg = (FormatConverterReg*)malloc(sizeof(FormatConverterReg));
    if (!reg) {
        return editionMgrInitErr;
    }

    reg->fromFormat = fromFormat;
    reg->toFormat = toFormat;
    reg->converter = converter;
    reg->priority = priority;
    reg->flags = 0;
    reg->userData = NULL;
    reg->next = gConverterRegistry;
    gConverterRegistry = reg;

    /* Add compatibility entry */
    if (gCompatibilityCount < gMaxCompatibilities) {
        FormatCompatibility* compat = &gCompatibilityMatrix[gCompatibilityCount++];
        compat->format1 = fromFormat;
        compat->format2 = toFormat;
        compat->compatibility = 0.8f;  /* Default compatibility */
        compat->conversionCost = 100;   /* Default cost */
    }

    return noErr;
}

/*
 * UnregisterFormatConverter
 *
 * Unregister a format converter.
 */
OSErr UnregisterFormatConverter(FormatType fromFormat, FormatType toFormat)
{
    FormatConverterReg* prev = NULL;
    FormatConverterReg* current = gConverterRegistry;

    while (current) {
        if (current->fromFormat == fromFormat && current->toFormat == toFormat) {
            if (prev) {
                prev->next = current->next;
            } else {
                gConverterRegistry = current->next;
            }
            free(current);
            return noErr;
        }
        prev = current;
        current = current->next;
    }

    return badSubPartErr;  /* Converter not found */
}

/*
 * ConvertFormat
 *
 * Convert data from one format to another.
 */
OSErr ConvertFormat(FormatType fromFormat, FormatType toFormat,
                   const void* inputData, Size inputSize,
                   void** outputData, Size* outputSize)
{
    if (!inputData || !outputData || !outputSize || inputSize <= 0) {
        return badSubPartErr;
    }

    /* Check for identity conversion */
    if (fromFormat == toFormat) {
        *outputData = malloc(inputSize);
        if (!*outputData) {
            return editionMgrInitErr;
        }
        memcpy(*outputData, inputData, inputSize);
        *outputSize = inputSize;
        return noErr;
    }

    /* Find direct converter */
    FormatConverterReg* converter = FindConverter(fromFormat, toFormat);
    if (converter) {
        return converter->converter(inputData, inputSize, outputData, outputSize, converter->userData);
    }

    /* Try to find conversion path */
    FormatType conversionPath[8];  /* Maximum path length */
    int32_t pathLength = 0;

    OSErr err = FindConversionPath(fromFormat, toFormat, conversionPath, &pathLength);
    if (err != noErr || pathLength == 0) {
        return badSubPartErr;  /* No conversion path found */
    }

    /* Perform multi-step conversion */
    void* currentData = (void*)inputData;
    Size currentSize = inputSize;
    bool needsFree = false;

    for (int32_t i = 0; i < pathLength - 1; i++) {
        FormatType stepFrom = (i == 0) ? fromFormat : conversionPath[i-1];
        FormatType stepTo = conversionPath[i];

        converter = FindConverter(stepFrom, stepTo);
        if (!converter) {
            if (needsFree) free(currentData);
            return badSubPartErr;
        }

        void* stepOutput;
        Size stepOutputSize;

        err = converter->converter(currentData, currentSize, &stepOutput, &stepOutputSize, converter->userData);

        if (needsFree) {
            free(currentData);
        }

        if (err != noErr) {
            return err;
        }

        currentData = stepOutput;
        currentSize = stepOutputSize;
        needsFree = true;
    }

    *outputData = currentData;
    *outputSize = currentSize;
    return noErr;
}

/*
 * NegotiateBestFormat
 *
 * Negotiate the best format between publisher and subscriber.
 */
OSErr NegotiateBestFormat(const FormatType* publisherFormats, int32_t publisherCount,
                         const FormatType* subscriberFormats, int32_t subscriberCount,
                         FormatType* bestFormat, float* compatibility)
{
    if (!publisherFormats || !subscriberFormats || !bestFormat || !compatibility) {
        return badSubPartErr;
    }

    *bestFormat = 0;
    *compatibility = 0.0f;

    float bestCompatibility = 0.0f;
    FormatType bestMatch = 0;

    /* Try to find exact matches first */
    for (int32_t p = 0; p < publisherCount; p++) {
        for (int32_t s = 0; s < subscriberCount; s++) {
            if (publisherFormats[p] == subscriberFormats[s]) {
                *bestFormat = publisherFormats[p];
                *compatibility = 1.0f;
                return noErr;
            }
        }
    }

    /* Find best compatible formats */
    for (int32_t p = 0; p < publisherCount; p++) {
        for (int32_t s = 0; s < subscriberCount; s++) {
            float compat = GetFormatCompatibility(publisherFormats[p], subscriberFormats[s]);
            if (compat > bestCompatibility) {
                bestCompatibility = compat;
                bestMatch = publisherFormats[p];  /* Use publisher format as primary */
            }
        }
    }

    if (bestCompatibility > 0.0f) {
        *bestFormat = bestMatch;
        *compatibility = bestCompatibility;
        return noErr;
    }

    return badSubPartErr;  /* No compatible formats found */
}

/*
 * GetSupportedFormats
 *
 * Get list of formats that can be converted to a target format.
 */
OSErr GetSupportedFormats(FormatType targetFormat,
                         FormatType** supportedFormats,
                         int32_t* formatCount)
{
    if (!supportedFormats || !formatCount) {
        return badSubPartErr;
    }

    /* Count converters that target this format */
    int32_t count = 0;
    FormatConverterReg* current = gConverterRegistry;
    while (current) {
        if (current->toFormat == targetFormat) {
            count++;
        }
        current = current->next;
    }

    if (count == 0) {
        *supportedFormats = NULL;
        *formatCount = 0;
        return noErr;
    }

    /* Allocate format array */
    *supportedFormats = (FormatType*)malloc(sizeof(FormatType) * count);
    if (!*supportedFormats) {
        return editionMgrInitErr;
    }

    /* Fill format array */
    int32_t index = 0;
    current = gConverterRegistry;
    while (current && index < count) {
        if (current->toFormat == targetFormat) {
            (*supportedFormats)[index++] = current->fromFormat;
        }
        current = current->next;
    }

    *formatCount = count;
    return noErr;
}

/*
 * GetConvertibleFormats
 *
 * Get list of formats that a source format can be converted to.
 */
OSErr GetConvertibleFormats(FormatType sourceFormat,
                           FormatType** convertibleFormats,
                           int32_t* formatCount)
{
    if (!convertibleFormats || !formatCount) {
        return badSubPartErr;
    }

    /* Count converters from this format */
    int32_t count = 0;
    FormatConverterReg* current = gConverterRegistry;
    while (current) {
        if (current->fromFormat == sourceFormat) {
            count++;
        }
        current = current->next;
    }

    if (count == 0) {
        *convertibleFormats = NULL;
        *formatCount = 0;
        return noErr;
    }

    /* Allocate format array */
    *convertibleFormats = (FormatType*)malloc(sizeof(FormatType) * count);
    if (!*convertibleFormats) {
        return editionMgrInitErr;
    }

    /* Fill format array */
    int32_t index = 0;
    current = gConverterRegistry;
    while (current && index < count) {
        if (current->fromFormat == sourceFormat) {
            (*convertibleFormats)[index++] = current->toFormat;
        }
        current = current->next;
    }

    *formatCount = count;
    return noErr;
}

/*
 * SetFormatCompatibility
 *
 * Set compatibility rating between two formats.
 */
OSErr SetFormatCompatibility(FormatType format1, FormatType format2,
                            float compatibility, uint32_t conversionCost)
{
    if (compatibility < 0.0f || compatibility > 1.0f) {
        return badSubPartErr;
    }

    /* Find existing compatibility entry */
    for (int32_t i = 0; i < gCompatibilityCount; i++) {
        FormatCompatibility* compat = &gCompatibilityMatrix[i];
        if ((compat->format1 == format1 && compat->format2 == format2) ||
            (compat->format1 == format2 && compat->format2 == format1)) {
            compat->compatibility = compatibility;
            compat->conversionCost = conversionCost;
            return noErr;
        }
    }

    /* Add new compatibility entry */
    if (gCompatibilityCount < gMaxCompatibilities) {
        FormatCompatibility* compat = &gCompatibilityMatrix[gCompatibilityCount++];
        compat->format1 = format1;
        compat->format2 = format2;
        compat->compatibility = compatibility;
        compat->conversionCost = conversionCost;
        return noErr;
    }

    return editionMgrInitErr;  /* Matrix full */
}

/* Internal helper functions */

static FormatConverterReg* FindConverter(FormatType fromFormat, FormatType toFormat)
{
    FormatConverterReg* current = gConverterRegistry;
    while (current) {
        if (current->fromFormat == fromFormat && current->toFormat == toFormat) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static float GetFormatCompatibility(FormatType format1, FormatType format2)
{
    /* Check for exact match */
    if (format1 == format2) {
        return 1.0f;
    }

    /* Look up in compatibility matrix */
    for (int32_t i = 0; i < gCompatibilityCount; i++) {
        FormatCompatibility* compat = &gCompatibilityMatrix[i];
        if ((compat->format1 == format1 && compat->format2 == format2) ||
            (compat->format1 == format2 && compat->format2 == format1)) {
            return compat->compatibility;
        }
    }

    /* Check if converter exists */
    if (FindConverter(format1, format2) || FindConverter(format2, format1)) {
        return 0.5f;  /* Default compatibility for convertible formats */
    }

    return 0.0f;  /* No compatibility */
}

static OSErr InitializeStandardConverters(void)
{
    OSErr err;

    /* Register TEXT to PICT converter */
    err = RegisterFormatConverter('TEXT', 'PICT', ConvertTextToPICT, 10);
    if (err != noErr) return err;

    /* Register PICT to TEXT converter */
    err = RegisterFormatConverter('PICT', 'TEXT', ConvertPICTToText, 5);
    if (err != noErr) return err;

    /* Register sound converters */
    err = RegisterFormatConverter('snd ', 'AIFF', ConvertSoundToAIFF, 10);
    if (err != noErr) return err;

    /* Register RTF to TEXT converter */
    err = RegisterFormatConverter('RTF ', 'TEXT', ConvertRTFToText, 15);
    if (err != noErr) return err;

    /* Set up standard compatibility matrix */
    SetFormatCompatibility('TEXT', 'RTF ', 0.9f, 50);
    SetFormatCompatibility('TEXT', 'PICT', 0.3f, 200);
    SetFormatCompatibility('PICT', 'TEXT', 0.2f, 300);
    SetFormatCompatibility('snd ', 'AIFF', 0.95f, 100);

    return noErr;
}

static OSErr FindConversionPath(FormatType fromFormat, FormatType toFormat,
                               FormatType* path, int32_t* pathLength)
{
    /* Simple pathfinding - could be improved with A* algorithm */
    *pathLength = 0;

    /* For now, only support single-step conversions */
    if (FindConverter(fromFormat, toFormat)) {
        path[0] = toFormat;
        *pathLength = 1;
        return noErr;
    }

    /* Try two-step conversion through common formats */
    FormatType commonFormats[] = {'TEXT', 'PICT', 'RTF ', 'snd '};
    int32_t commonCount = sizeof(commonFormats) / sizeof(FormatType);

    for (int32_t i = 0; i < commonCount; i++) {
        FormatType intermediate = commonFormats[i];
        if (intermediate != fromFormat && intermediate != toFormat) {
            if (FindConverter(fromFormat, intermediate) && FindConverter(intermediate, toFormat)) {
                path[0] = intermediate;
                path[1] = toFormat;
                *pathLength = 2;
                return noErr;
            }
        }
    }

    return badSubPartErr;  /* No path found */
}

/* Standard format converters */

static OSErr ConvertTextToPICT(const void* inputData, Size inputSize,
                              void** outputData, Size* outputSize, void* userData)
{
    /* Simple text-to-PICT conversion - creates a PICT with the text rendered */
    /* This is a placeholder implementation */

    Size pictSize = 512 + inputSize;  /* PICT header + text data */
    *outputData = malloc(pictSize);
    if (!*outputData) {
        return editionMgrInitErr;
    }

    /* Create minimal PICT header */
    uint8_t* pict = (uint8_t*)*outputData;
    memset(pict, 0, 512);  /* PICT header */

    /* Copy text data after header */
    memcpy(pict + 512, inputData, inputSize);

    *outputSize = pictSize;
    return noErr;
}

static OSErr ConvertPICTToText(const void* inputData, Size inputSize,
                              void** outputData, Size* outputSize, void* userData)
{
    /* Simple PICT-to-text conversion - extracts any text from PICT */
    /* This is a placeholder implementation */

    const char* defaultText = "[Image]";
    Size textSize = strlen(defaultText);

    *outputData = malloc(textSize + 1);
    if (!*outputData) {
        return editionMgrInitErr;
    }

    strcpy((char*)*outputData, defaultText);
    *outputSize = textSize;
    return noErr;
}

static OSErr ConvertSoundToAIFF(const void* inputData, Size inputSize,
                               void** outputData, Size* outputSize, void* userData)
{
    /* Simple sound-to-AIFF conversion */
    /* This is a placeholder implementation */

    Size aiffSize = 54 + inputSize;  /* AIFF header + sound data */
    *outputData = malloc(aiffSize);
    if (!*outputData) {
        return editionMgrInitErr;
    }

    /* Create minimal AIFF header */
    uint8_t* aiff = (uint8_t*)*outputData;
    memset(aiff, 0, 54);  /* AIFF header */
    memcpy(aiff, "FORM", 4);
    memcpy(aiff + 8, "AIFF", 4);

    /* Copy sound data after header */
    memcpy(aiff + 54, inputData, inputSize);

    *outputSize = aiffSize;
    return noErr;
}

static OSErr ConvertRTFToText(const void* inputData, Size inputSize,
                             void** outputData, Size* outputSize, void* userData)
{
    /* Simple RTF-to-text conversion - strips RTF codes */
    /* This is a placeholder implementation */

    const char* rtf = (const char*)inputData;
    Size maxTextSize = inputSize;  /* Text will be smaller than RTF */

    *outputData = malloc(maxTextSize);
    if (!*outputData) {
        return editionMgrInitErr;
    }

    char* text = (char*)*outputData;
    Size textPos = 0;
    bool inGroup = false;

    /* Simple RTF parsing - skip control codes */
    for (Size i = 0; i < inputSize && textPos < maxTextSize - 1; i++) {
        char c = rtf[i];

        if (c == '{') {
            inGroup = true;
        } else if (c == '}') {
            inGroup = false;
        } else if (c == '\\') {
            /* Skip control sequence */
            while (i < inputSize && rtf[i] != ' ' && rtf[i] != '}' && rtf[i] != '{') {
                i++;
            }
        } else if (!inGroup && c >= 32 && c <= 126) {
            text[textPos++] = c;
        }
    }

    text[textPos] = '\0';
    *outputSize = textPos;
    return noErr;
}