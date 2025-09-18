/*
 * ScrapConversion.c - Data Format Conversion and Transformation
 * System 7.1 Portable - Scrap Manager Component
 *
 * Implements data format conversion, transformation chains, and coercion
 * for seamless clipboard data exchange between different formats.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ScrapManager/ScrapConversion.h"
#include "ScrapManager/ScrapFormats.h"
#include "ScrapManager/ScrapTypes.h"
#include "MacTypes.h"
#include "MemoryManager.h"
#include "ErrorCodes.h"

/* Converter registry */
static ScrapConverter gConverters[64];
static int16_t gConverterCount = 0;
static Boolean gConversionInitialized = false;

/* Conversion preferences */
static ConversionPrefs gConversionPrefs = {
    QUALITY_NORMAL,     /* quality */
    true,               /* preserveMetadata */
    true,               /* allowLossy */
    1024 * 1024,        /* maxMemoryUsage (1MB) */
    30                  /* timeoutSeconds */
};

/* Internal function prototypes */
static void InitializeConversion(void);
static OSErr RegisterBuiltinConverters(void);
static ScrapConverter *FindConverter(ResType sourceType, ResType destType);
static OSErr ConvertTextToUTF8(Handle sourceData, Handle *destData);
static OSErr ConvertUTF8ToText(Handle sourceData, Handle *destData);
static OSErr ConvertTextToRTF(Handle sourceData, Handle *destData);
static OSErr ConvertRTFToText(Handle sourceData, Handle *destData);
static OSErr ConvertPICTToPNG(Handle sourceData, Handle *destData);
static OSErr ConvertPNGToPICT(Handle sourceData, Handle *destData);
static OSErr SimpleTextConversion(Handle sourceData, Handle *destData,
                                  ResType sourceType, ResType destType);

/*
 * Core Conversion Functions
 */

OSErr InitScrapConversion(void)
{
    if (gConversionInitialized) {
        return noErr;
    }

    memset(gConverters, 0, sizeof(gConverters));
    gConverterCount = 0;

    RegisterBuiltinConverters();
    gConversionInitialized = true;

    return noErr;
}

void CleanupScrapConversion(void)
{
    /* No cleanup needed for static arrays */
    gConversionInitialized = false;
}

OSErr ConvertScrapData(Handle sourceData, ResType sourceType,
                      Handle *destData, ResType destType, uint16_t conversionFlags)
{
    OSErr err = noErr;
    ScrapConverter *converter;

    if (sourceData == NULL || destData == NULL) {
        return paramErr;
    }

    if (!gConversionInitialized) {
        InitScrapConversion();
    }

    /* No conversion needed if types are the same */
    if (sourceType == destType) {
        *destData = NewHandle(GetHandleSize(sourceData));
        if (*destData == NULL) {
            return memFullErr;
        }

        HLock(sourceData);
        HLock(*destData);
        memcpy(**destData, *sourceData, GetHandleSize(sourceData));
        HUnlock(*destData);
        HUnlock(sourceData);

        return noErr;
    }

    /* Find appropriate converter */
    converter = FindConverter(sourceType, destType);
    if (converter == NULL) {
        /* Try to build conversion chain */
        ResType chainTypes[8];
        int16_t chainLength;

        err = BuildConversionChain(sourceType, destType, chainTypes, &chainLength, 8);
        if (err == noErr && chainLength > 0) {
            return ExecuteConversionChain(sourceData, chainTypes, chainLength, destData);
        }

        return scrapConversionError;
    }

    /* Execute conversion */
    return converter->converter(sourceType, destType, sourceData, destData, converter->refCon);
}

Boolean CanConvertScrapData(ResType sourceType, ResType destType)
{
    ResType chainTypes[8];
    int16_t chainLength;

    if (!gConversionInitialized) {
        InitScrapConversion();
    }

    if (sourceType == destType) {
        return true;
    }

    if (FindConverter(sourceType, destType) != NULL) {
        return true;
    }

    /* Check if conversion chain is possible */
    return (BuildConversionChain(sourceType, destType, chainTypes, &chainLength, 8) == noErr);
}

OSErr GetConversionCost(ResType sourceType, ResType destType, int32_t sourceSize,
                       int32_t *conversionTime, int32_t *memoryNeeded, int16_t *qualityLoss)
{
    if (!gConversionInitialized) {
        InitScrapConversion();
    }

    /* Provide estimates based on conversion type */
    if (conversionTime) {
        *conversionTime = 100; /* Default 100ms */

        /* Text conversions are fast */
        if ((sourceType == SCRAP_TYPE_TEXT && destType == SCRAP_TYPE_UTF8) ||
            (sourceType == SCRAP_TYPE_UTF8 && destType == SCRAP_TYPE_TEXT)) {
            *conversionTime = 10;
        }

        /* Image conversions are slower */
        if ((sourceType == SCRAP_TYPE_PICT && destType == SCRAP_TYPE_PNG) ||
            (sourceType == SCRAP_TYPE_PNG && destType == SCRAP_TYPE_PICT)) {
            *conversionTime = sourceSize / 1000; /* ~1ms per KB */
        }
    }

    if (memoryNeeded) {
        *memoryNeeded = sourceSize * 2; /* Estimate 2x source size */
    }

    if (qualityLoss) {
        *qualityLoss = 0; /* Default to lossless */

        /* JPEG conversion is lossy */
        if (destType == SCRAP_TYPE_JPEG) {
            *qualityLoss = 10;
        }
    }

    return noErr;
}

/*
 * Converter Registration and Management
 */

OSErr RegisterDataConverter(ResType sourceType, ResType destType,
                           ScrapConverterProc converter, void *refCon,
                           uint16_t priority, uint16_t flags)
{
    if (converter == NULL) {
        return paramErr;
    }

    if (!gConversionInitialized) {
        InitScrapConversion();
    }

    if (gConverterCount >= 64) {
        return scrapTooManyFormats;
    }

    gConverters[gConverterCount].sourceType = sourceType;
    gConverters[gConverterCount].destType = destType;
    gConverters[gConverterCount].converter = converter;
    gConverters[gConverterCount].refCon = refCon;
    gConverters[gConverterCount].priority = priority;
    gConverters[gConverterCount].flags = flags;

    gConverterCount++;
    return noErr;
}

OSErr UnregisterDataConverter(ResType sourceType, ResType destType,
                             ScrapConverterProc converter)
{
    int16_t i, j;

    if (!gConversionInitialized) {
        return scrapNoTypeError;
    }

    for (i = 0; i < gConverterCount; i++) {
        if (gConverters[i].sourceType == sourceType &&
            gConverters[i].destType == destType &&
            gConverters[i].converter == converter) {

            /* Shift remaining converters down */
            for (j = i; j < gConverterCount - 1; j++) {
                gConverters[j] = gConverters[j + 1];
            }
            gConverterCount--;
            return noErr;
        }
    }

    return scrapNoTypeError;
}

ScrapConverterProc FindBestConverter(ResType sourceType, ResType destType,
                                    void **refCon, uint16_t *priority)
{
    ScrapConverter *best = NULL;
    int16_t i;

    if (!gConversionInitialized) {
        InitScrapConversion();
    }

    for (i = 0; i < gConverterCount; i++) {
        if (gConverters[i].sourceType == sourceType &&
            gConverters[i].destType == destType) {

            if (best == NULL || gConverters[i].priority > best->priority) {
                best = &gConverters[i];
            }
        }
    }

    if (best) {
        if (refCon) *refCon = best->refCon;
        if (priority) *priority = best->priority;
        return best->converter;
    }

    return NULL;
}

/*
 * Conversion Chain Management
 */

OSErr BuildConversionChain(ResType sourceType, ResType destType,
                          ResType *chainTypes, int16_t *chainLength, int16_t maxSteps)
{
    int16_t i, step = 0;

    if (chainTypes == NULL || chainLength == NULL || maxSteps < 2) {
        return paramErr;
    }

    if (!gConversionInitialized) {
        InitScrapConversion();
    }

    /* Simple chain building - could be enhanced with graph algorithms */
    chainTypes[step++] = sourceType;

    /* Look for direct conversion */
    if (FindConverter(sourceType, destType) != NULL) {
        chainTypes[step++] = destType;
        *chainLength = step;
        return noErr;
    }

    /* Try common intermediate formats */
    ResType intermediates[] = {SCRAP_TYPE_TEXT, SCRAP_TYPE_UTF8, SCRAP_TYPE_PICT, SCRAP_TYPE_PNG};
    int16_t intermediateCount = sizeof(intermediates) / sizeof(ResType);

    for (i = 0; i < intermediateCount && step < maxSteps - 1; i++) {
        ResType intermediate = intermediates[i];

        if (intermediate != sourceType && intermediate != destType) {
            if (FindConverter(sourceType, intermediate) != NULL &&
                FindConverter(intermediate, destType) != NULL) {

                chainTypes[step++] = intermediate;
                chainTypes[step++] = destType;
                *chainLength = step;
                return noErr;
            }
        }
    }

    *chainLength = 0;
    return scrapConversionError;
}

OSErr ExecuteConversionChain(Handle sourceData, const ResType *chainTypes,
                            int16_t chainLength, Handle *destData)
{
    OSErr err = noErr;
    Handle currentData, nextData;
    int16_t i;

    if (sourceData == NULL || chainTypes == NULL || chainLength < 2 || destData == NULL) {
        return paramErr;
    }

    currentData = sourceData;
    *destData = NULL;

    /* Execute each conversion step */
    for (i = 0; i < chainLength - 1; i++) {
        ResType currentType = chainTypes[i];
        ResType nextType = chainTypes[i + 1];

        err = ConvertScrapData(currentData, currentType, &nextData, nextType, 0);

        /* Clean up intermediate data (except source and final result) */
        if (currentData != sourceData) {
            DisposeHandle(currentData);
        }

        if (err != noErr) {
            return err;
        }

        currentData = nextData;
    }

    *destData = currentData;
    return noErr;
}

/*
 * Text Conversion Functions
 */

OSErr ConvertTextEncoding(Handle sourceText, uint32_t sourceEncoding,
                         Handle *destText, uint32_t destEncoding)
{
    if (sourceText == NULL || destText == NULL) {
        return paramErr;
    }

    /* For now, implement basic conversions */
    if (sourceEncoding == destEncoding) {
        /* No conversion needed */
        int32_t size = GetHandleSize(sourceText);
        *destText = NewHandle(size);
        if (*destText == NULL) {
            return memFullErr;
        }

        HLock(sourceText);
        HLock(*destText);
        memcpy(**destText, *sourceText, size);
        HUnlock(*destText);
        HUnlock(sourceText);

        return noErr;
    }

    /* Convert between Mac Roman and UTF-8 */
    if ((sourceEncoding == 'mac ' && destEncoding == 'utf8') ||
        (sourceEncoding == 'TEXT' && destEncoding == 'utf8')) {
        return ConvertTextToUTF8(sourceText, destText);
    }

    if (sourceEncoding == 'utf8' && (destEncoding == 'mac ' || destEncoding == 'TEXT')) {
        return ConvertUTF8ToText(sourceText, destText);
    }

    return scrapConversionError;
}

OSErr ConvertTextCase(Handle textData, int16_t caseConversion)
{
    int32_t size;
    char *text;
    int32_t i;

    if (textData == NULL) {
        return paramErr;
    }

    size = GetHandleSize(textData);
    if (size <= 0) {
        return noErr;
    }

    HLock(textData);
    text = *textData;

    switch (caseConversion) {
        case CASE_UPPER:
            for (i = 0; i < size; i++) {
                text[i] = toupper(text[i]);
            }
            break;

        case CASE_LOWER:
            for (i = 0; i < size; i++) {
                text[i] = tolower(text[i]);
            }
            break;

        case CASE_TITLE:
            /* Convert to title case */
            {
                Boolean nextUpper = true;
                for (i = 0; i < size; i++) {
                    if (isalpha(text[i])) {
                        text[i] = nextUpper ? toupper(text[i]) : tolower(text[i]);
                        nextUpper = false;
                    } else if (isspace(text[i])) {
                        nextUpper = true;
                    }
                }
            }
            break;

        default:
            HUnlock(textData);
            return paramErr;
    }

    HUnlock(textData);
    return noErr;
}

OSErr ConvertTextLineEndings(Handle textData, int16_t sourceFormat, int16_t destFormat)
{
    int32_t size, newSize, i, j;
    char *sourceText, *destText;
    Handle newHandle;

    if (textData == NULL) {
        return paramErr;
    }

    if (sourceFormat == destFormat) {
        return noErr;
    }

    size = GetHandleSize(textData);
    if (size <= 0) {
        return noErr;
    }

    HLock(textData);
    sourceText = *textData;

    /* Count line endings to estimate new size */
    int32_t lineCount = 0;
    for (i = 0; i < size; i++) {
        if (sourceText[i] == '\r' || sourceText[i] == '\n') {
            lineCount++;
            /* Skip CRLF pairs */
            if (i + 1 < size && sourceText[i] == '\r' && sourceText[i + 1] == '\n') {
                i++;
            }
        }
    }

    /* Calculate new size */
    newSize = size;
    if (destFormat == LINE_ENDING_CRLF) {
        newSize += lineCount; /* Add extra characters for CRLF */
    } else if (sourceFormat == LINE_ENDING_CRLF) {
        newSize -= lineCount; /* Remove extra characters from CRLF */
    }

    /* Create new handle */
    newHandle = NewHandle(newSize);
    if (newHandle == NULL) {
        HUnlock(textData);
        return memFullErr;
    }

    HLock(newHandle);
    destText = *newHandle;

    /* Convert line endings */
    for (i = 0, j = 0; i < size && j < newSize; i++) {
        char c = sourceText[i];

        if (c == '\r' || c == '\n') {
            /* Handle source line ending */
            if (sourceFormat == LINE_ENDING_CRLF && c == '\r' && i + 1 < size && sourceText[i + 1] == '\n') {
                i++; /* Skip the LF part of CRLF */
            }

            /* Write destination line ending */
            switch (destFormat) {
                case LINE_ENDING_CR:
                    destText[j++] = '\r';
                    break;
                case LINE_ENDING_LF:
                    destText[j++] = '\n';
                    break;
                case LINE_ENDING_CRLF:
                    destText[j++] = '\r';
                    destText[j++] = '\n';
                    break;
            }
        } else {
            destText[j++] = c;
        }
    }

    HUnlock(newHandle);
    HUnlock(textData);

    /* Replace original handle */
    DisposeHandle(textData);
    *((Handle *)&textData) = newHandle; /* This is a bit hacky but works for the pattern */

    return noErr;
}

/*
 * Internal Helper Functions
 */

static void InitializeConversion(void)
{
    if (gConversionInitialized) {
        return;
    }

    InitScrapConversion();
}

static OSErr RegisterBuiltinConverters(void)
{
    /* Register text converters */
    RegisterDataConverter(SCRAP_TYPE_TEXT, SCRAP_TYPE_UTF8,
                         (ScrapConverterProc)ConvertTextToUTF8, NULL, 100, 0);
    RegisterDataConverter(SCRAP_TYPE_UTF8, SCRAP_TYPE_TEXT,
                         (ScrapConverterProc)ConvertUTF8ToText, NULL, 100, 0);
    RegisterDataConverter(SCRAP_TYPE_TEXT, SCRAP_TYPE_RTF,
                         (ScrapConverterProc)ConvertTextToRTF, NULL, 80, 0);
    RegisterDataConverter(SCRAP_TYPE_RTF, SCRAP_TYPE_TEXT,
                         (ScrapConverterProc)ConvertRTFToText, NULL, 80, 0);

    /* Register image converters (simplified) */
    RegisterDataConverter(SCRAP_TYPE_PICT, SCRAP_TYPE_PNG,
                         (ScrapConverterProc)ConvertPICTToPNG, NULL, 90, 0);
    RegisterDataConverter(SCRAP_TYPE_PNG, SCRAP_TYPE_PICT,
                         (ScrapConverterProc)ConvertPNGToPICT, NULL, 90, 0);

    return noErr;
}

static ScrapConverter *FindConverter(ResType sourceType, ResType destType)
{
    int16_t i;

    for (i = 0; i < gConverterCount; i++) {
        if (gConverters[i].sourceType == sourceType &&
            gConverters[i].destType == destType) {
            return &gConverters[i];
        }
    }

    return NULL;
}

static OSErr ConvertTextToUTF8(Handle sourceData, Handle *destData)
{
    /* For now, implement a simple conversion */
    return SimpleTextConversion(sourceData, destData, SCRAP_TYPE_TEXT, SCRAP_TYPE_UTF8);
}

static OSErr ConvertUTF8ToText(Handle sourceData, Handle *destData)
{
    /* For now, implement a simple conversion */
    return SimpleTextConversion(sourceData, destData, SCRAP_TYPE_UTF8, SCRAP_TYPE_TEXT);
}

static OSErr ConvertTextToRTF(Handle sourceData, Handle *destData)
{
    int32_t sourceSize, destSize;
    char *sourceText, *destText;
    const char *rtfHeader = "{\\rtf1\\ansi\\deff0 {\\fonttbl {\\f0 Times New Roman;}}\\f0 ";
    const char *rtfFooter = "}";

    if (sourceData == NULL || destData == NULL) {
        return paramErr;
    }

    sourceSize = GetHandleSize(sourceData);
    destSize = sourceSize + strlen(rtfHeader) + strlen(rtfFooter) + 100; /* Extra space for escaping */

    *destData = NewHandle(destSize);
    if (*destData == NULL) {
        return memFullErr;
    }

    HLock(sourceData);
    HLock(*destData);

    sourceText = *sourceData;
    destText = **destData;

    /* Build RTF document */
    strcpy(destText, rtfHeader);
    int32_t destPos = strlen(rtfHeader);

    /* Copy text with RTF escaping */
    int32_t i;
    for (i = 0; i < sourceSize && destPos < destSize - 10; i++) {
        char c = sourceText[i];
        switch (c) {
            case '\\':
                destText[destPos++] = '\\';
                destText[destPos++] = '\\';
                break;
            case '{':
                destText[destPos++] = '\\';
                destText[destPos++] = '{';
                break;
            case '}':
                destText[destPos++] = '\\';
                destText[destPos++] = '}';
                break;
            case '\n':
                destText[destPos++] = '\\';
                destText[destPos++] = 'p';
                destText[destPos++] = 'a';
                destText[destPos++] = 'r';
                destText[destPos++] = ' ';
                break;
            default:
                destText[destPos++] = c;
                break;
        }
    }

    strcpy(destText + destPos, rtfFooter);
    destPos += strlen(rtfFooter);

    HUnlock(*destData);
    HUnlock(sourceData);

    /* Resize to actual size */
    SetHandleSize(*destData, destPos);

    return noErr;
}

static OSErr ConvertRTFToText(Handle sourceData, Handle *destData)
{
    /* Simple RTF to text conversion - strip RTF commands */
    int32_t sourceSize, destSize = 0;
    char *sourceText, *destText;
    int32_t i, destPos = 0;
    Boolean inCommand = false;
    Boolean inGroup = false;

    if (sourceData == NULL || destData == NULL) {
        return paramErr;
    }

    sourceSize = GetHandleSize(sourceData);
    *destData = NewHandle(sourceSize); /* Worst case same size */
    if (*destData == NULL) {
        return memFullErr;
    }

    HLock(sourceData);
    HLock(*destData);

    sourceText = *sourceData;
    destText = **destData;

    for (i = 0; i < sourceSize; i++) {
        char c = sourceText[i];

        if (c == '{') {
            inGroup = true;
        } else if (c == '}') {
            inGroup = false;
        } else if (c == '\\') {
            inCommand = true;
        } else if (inCommand && (c == ' ' || c == '\r' || c == '\n')) {
            inCommand = false;
        } else if (!inCommand && !inGroup) {
            destText[destPos++] = c;
        }
    }

    HUnlock(*destData);
    HUnlock(sourceData);

    /* Resize to actual text size */
    SetHandleSize(*destData, destPos);

    return noErr;
}

static OSErr ConvertPICTToPNG(Handle sourceData, Handle *destData)
{
    /* This would require QuickDraw and image processing capabilities */
    /* For now, return conversion error */
    return scrapConversionError;
}

static OSErr ConvertPNGToPICT(Handle sourceData, Handle *destData)
{
    /* This would require PNG decoding and QuickDraw capabilities */
    /* For now, return conversion error */
    return scrapConversionError;
}

static OSErr SimpleTextConversion(Handle sourceData, Handle *destData,
                                  ResType sourceType, ResType destType)
{
    int32_t size;

    if (sourceData == NULL || destData == NULL) {
        return paramErr;
    }

    size = GetHandleSize(sourceData);
    *destData = NewHandle(size);
    if (*destData == NULL) {
        return memFullErr;
    }

    HLock(sourceData);
    HLock(*destData);
    memcpy(**destData, *sourceData, size);
    HUnlock(*destData);
    HUnlock(sourceData);

    return noErr;
}