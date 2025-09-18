/*
 * ScrapFormats.c - Scrap Data Format Handling and Validation
 * System 7.1 Portable - Scrap Manager Component
 *
 * Implements data format detection, validation, and basic conversion
 * functionality for various Mac OS clipboard data types.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ScrapManager/ScrapFormats.h"
#include "ScrapManager/ScrapTypes.h"
#include "ScrapManager/ScrapManager.h"
#include "MacTypes.h"
#include "ErrorCodes.h"

/* Format registry structure */
typedef struct {
    ResType         type;
    Str255          description;
    Str255          fileExtension;
    uint32_t        formatFlags;
    Boolean         isRegistered;
} FormatInfo;

/* Global format registry */
static FormatInfo gFormatRegistry[128];
static int16_t gFormatCount = 0;
static Boolean gFormatsInitialized = false;

/* Internal function prototypes */
static void InitializeFormatRegistry(void);
static OSErr RegisterBuiltinFormats(void);
static FormatInfo *FindFormatInfo(ResType type);
static Boolean IsTextData(const void *data, int32_t size);
static Boolean IsPICTData(const void *data, int32_t size);
static Boolean IsSoundData(const void *data, int32_t size);
static Boolean IsImageData(const void *data, int32_t size);
static OSErr ValidateTextData(const void *data, int32_t size);
static OSErr ValidatePICTData(const void *data, int32_t size);
static OSErr ValidateSoundData(const void *data, int32_t size);

/*
 * Format Detection and Validation
 */

ResType DetectScrapFormat(const void *data, int32_t size)
{
    if (data == NULL || size <= 0) {
        return 0;
    }

    if (!gFormatsInitialized) {
        InitializeFormatRegistry();
    }

    /* Check for text data */
    if (IsTextData(data, size)) {
        return SCRAP_TYPE_TEXT;
    }

    /* Check for PICT data */
    if (IsPICTData(data, size)) {
        return SCRAP_TYPE_PICT;
    }

    /* Check for sound data */
    if (IsSoundData(data, size)) {
        return SCRAP_TYPE_SOUND;
    }

    /* Check for modern image formats */
    if (IsImageData(data, size)) {
        const uint8_t *bytes = (const uint8_t *)data;

        /* PNG signature */
        if (size >= 8 && memcmp(bytes, "\x89PNG\r\n\x1a\n", 8) == 0) {
            return SCRAP_TYPE_PNG;
        }

        /* JPEG signature */
        if (size >= 2 && bytes[0] == 0xFF && bytes[1] == 0xD8) {
            return SCRAP_TYPE_JPEG;
        }

        /* TIFF signatures */
        if (size >= 4) {
            if ((bytes[0] == 'I' && bytes[1] == 'I' && bytes[2] == 42 && bytes[3] == 0) ||
                (bytes[0] == 'M' && bytes[1] == 'M' && bytes[2] == 0 && bytes[3] == 42)) {
                return SCRAP_TYPE_TIFF;
            }
        }
    }

    /* Default to generic data */
    return 0;
}

OSErr ValidateFormatData(ResType type, const void *data, int32_t size)
{
    if (data == NULL || size < 0) {
        return paramErr;
    }

    if (size == 0) {
        return noErr; /* Empty data is valid */
    }

    switch (type) {
        case SCRAP_TYPE_TEXT:
        case SCRAP_TYPE_UTF8:
        case SCRAP_TYPE_STRING:
            return ValidateTextData(data, size);

        case SCRAP_TYPE_PICT:
            return ValidatePICTData(data, size);

        case SCRAP_TYPE_SOUND:
            return ValidateSoundData(data, size);

        case SCRAP_TYPE_STYLE:
            /* Style data validation would go here */
            return noErr;

        case SCRAP_TYPE_PNG:
        case SCRAP_TYPE_JPEG:
        case SCRAP_TYPE_TIFF:
            /* Modern image format validation */
            return noErr;

        default:
            /* Unknown format - assume valid */
            return noErr;
    }
}

Boolean IsValidMacFormat(ResType type)
{
    if (!gFormatsInitialized) {
        InitializeFormatRegistry();
    }

    return (FindFormatInfo(type) != NULL);
}

Boolean FormatNeedsConversion(ResType sourceType, ResType destType)
{
    if (sourceType == destType) {
        return false;
    }

    /* Text formats that can be converted between each other */
    if ((sourceType == SCRAP_TYPE_TEXT && destType == SCRAP_TYPE_UTF8) ||
        (sourceType == SCRAP_TYPE_UTF8 && destType == SCRAP_TYPE_TEXT) ||
        (sourceType == SCRAP_TYPE_STRING && destType == SCRAP_TYPE_TEXT)) {
        return true;
    }

    /* Image formats that can be converted */
    if ((sourceType == SCRAP_TYPE_PICT &&
         (destType == SCRAP_TYPE_PNG || destType == SCRAP_TYPE_JPEG || destType == SCRAP_TYPE_TIFF)) ||
        ((sourceType == SCRAP_TYPE_PNG || sourceType == SCRAP_TYPE_JPEG || sourceType == SCRAP_TYPE_TIFF) &&
         destType == SCRAP_TYPE_PICT)) {
        return true;
    }

    return false;
}

void GetFormatDescription(ResType type, Str255 description)
{
    FormatInfo *info;

    if (!gFormatsInitialized) {
        InitializeFormatRegistry();
    }

    info = FindFormatInfo(type);
    if (info) {
        memcpy(description, info->description, info->description[0] + 1);
        return;
    }

    /* Generate description from type */
    char typeStr[5];
    typeStr[0] = (type >> 24) & 0xFF;
    typeStr[1] = (type >> 16) & 0xFF;
    typeStr[2] = (type >> 8) & 0xFF;
    typeStr[3] = type & 0xFF;
    typeStr[4] = '\0';

    snprintf((char *)&description[1], 254, "Unknown format '%s'", typeStr);
    description[0] = strlen((char *)&description[1]);
}

/*
 * Text Format Handling
 */

OSErr MacTextToUTF8(Handle macText, Handle *utf8Text)
{
    OSErr err = noErr;
    int32_t macSize, utf8Size;
    char *macPtr, *utf8Ptr;

    if (macText == NULL || utf8Text == NULL) {
        return paramErr;
    }

    macSize = GetHandleSize(macText);
    if (macSize <= 0) {
        return paramErr;
    }

    /* For simplicity, assume 1:1 mapping for now */
    /* Real implementation would use proper character encoding conversion */
    utf8Size = macSize;

    *utf8Text = NewHandle(utf8Size);
    if (*utf8Text == NULL) {
        return memFullErr;
    }

    HLock(macText);
    HLock(*utf8Text);

    macPtr = *macText;
    utf8Ptr = **utf8Text;

    /* Simple character mapping - real implementation would be more complex */
    memcpy(utf8Ptr, macPtr, macSize);

    HUnlock(*utf8Text);
    HUnlock(macText);

    return noErr;
}

OSErr UTF8ToMacText(Handle utf8Text, Handle *macText)
{
    OSErr err = noErr;
    int32_t utf8Size, macSize;
    char *utf8Ptr, *macPtr;

    if (utf8Text == NULL || macText == NULL) {
        return paramErr;
    }

    utf8Size = GetHandleSize(utf8Text);
    if (utf8Size <= 0) {
        return paramErr;
    }

    /* For simplicity, assume 1:1 mapping for now */
    macSize = utf8Size;

    *macText = NewHandle(macSize);
    if (*macText == NULL) {
        return memFullErr;
    }

    HLock(utf8Text);
    HLock(*macText);

    utf8Ptr = *utf8Text;
    macPtr = **macText;

    /* Simple character mapping */
    memcpy(macPtr, utf8Ptr, utf8Size);

    HUnlock(*macText);
    HUnlock(utf8Text);

    return noErr;
}

OSErr DetectTextEncoding(const void *data, int32_t size, uint32_t *encoding)
{
    const uint8_t *bytes = (const uint8_t *)data;
    int32_t i;
    Boolean hasHighBits = false;
    Boolean validUTF8 = true;

    if (data == NULL || size <= 0 || encoding == NULL) {
        return paramErr;
    }

    /* Check for UTF-8 BOM */
    if (size >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        *encoding = 'utf8';
        return noErr;
    }

    /* Scan for high-bit characters and UTF-8 patterns */
    for (i = 0; i < size; i++) {
        if (bytes[i] > 127) {
            hasHighBits = true;

            /* Check for valid UTF-8 sequences */
            if ((bytes[i] & 0xE0) == 0xC0) { /* 2-byte sequence */
                if (i + 1 >= size || (bytes[i + 1] & 0xC0) != 0x80) {
                    validUTF8 = false;
                    break;
                }
                i++; /* Skip next byte */
            } else if ((bytes[i] & 0xF0) == 0xE0) { /* 3-byte sequence */
                if (i + 2 >= size || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80) {
                    validUTF8 = false;
                    break;
                }
                i += 2; /* Skip next two bytes */
            } else if ((bytes[i] & 0xF8) == 0xF0) { /* 4-byte sequence */
                if (i + 3 >= size || (bytes[i + 1] & 0xC0) != 0x80 ||
                    (bytes[i + 2] & 0xC0) != 0x80 || (bytes[i + 3] & 0xC0) != 0x80) {
                    validUTF8 = false;
                    break;
                }
                i += 3; /* Skip next three bytes */
            } else {
                validUTF8 = false;
                break;
            }
        }
    }

    if (!hasHighBits) {
        *encoding = 'asci'; /* Pure ASCII */
    } else if (validUTF8) {
        *encoding = 'utf8'; /* Valid UTF-8 */
    } else {
        *encoding = 'mac '; /* Assume Mac Roman */
    }

    return noErr;
}

/*
 * Format Registry Functions
 */

OSErr RegisterScrapFormat(ResType type, ConstStr255Param description,
                         ConstStr255Param fileExtension, uint32_t formatFlags)
{
    FormatInfo *info;

    if (!gFormatsInitialized) {
        InitializeFormatRegistry();
    }

    if (gFormatCount >= 128) {
        return scrapTooManyFormats;
    }

    /* Check if already registered */
    info = FindFormatInfo(type);
    if (info) {
        /* Update existing registration */
        if (description) {
            memcpy(info->description, description, description[0] + 1);
        }
        if (fileExtension) {
            memcpy(info->fileExtension, fileExtension, fileExtension[0] + 1);
        }
        info->formatFlags = formatFlags;
        return noErr;
    }

    /* Add new registration */
    info = &gFormatRegistry[gFormatCount];
    info->type = type;

    if (description) {
        memcpy(info->description, description, description[0] + 1);
    } else {
        info->description[0] = 0;
    }

    if (fileExtension) {
        memcpy(info->fileExtension, fileExtension, fileExtension[0] + 1);
    } else {
        info->fileExtension[0] = 0;
    }

    info->formatFlags = formatFlags;
    info->isRegistered = true;

    gFormatCount++;
    return noErr;
}

OSErr UnregisterScrapFormat(ResType type)
{
    int16_t i, j;

    if (!gFormatsInitialized) {
        return scrapNoTypeError;
    }

    for (i = 0; i < gFormatCount; i++) {
        if (gFormatRegistry[i].type == type && gFormatRegistry[i].isRegistered) {
            /* Shift remaining entries down */
            for (j = i; j < gFormatCount - 1; j++) {
                gFormatRegistry[j] = gFormatRegistry[j + 1];
            }
            gFormatCount--;
            return noErr;
        }
    }

    return scrapNoTypeError;
}

OSErr GetFormatInfo(ResType type, Str255 description, Str255 fileExtension,
                   uint32_t *formatFlags)
{
    FormatInfo *info;

    if (!gFormatsInitialized) {
        InitializeFormatRegistry();
    }

    info = FindFormatInfo(type);
    if (!info) {
        return scrapNoTypeError;
    }

    if (description) {
        memcpy(description, info->description, info->description[0] + 1);
    }
    if (fileExtension) {
        memcpy(fileExtension, info->fileExtension, info->fileExtension[0] + 1);
    }
    if (formatFlags) {
        *formatFlags = info->formatFlags;
    }

    return noErr;
}

OSErr EnumerateFormats(ResType *types, int16_t *count, int16_t maxTypes)
{
    int16_t i, actualCount = 0;

    if (!gFormatsInitialized) {
        InitializeFormatRegistry();
    }

    if (types == NULL || count == NULL) {
        return paramErr;
    }

    for (i = 0; i < gFormatCount && actualCount < maxTypes; i++) {
        if (gFormatRegistry[i].isRegistered) {
            types[actualCount] = gFormatRegistry[i].type;
            actualCount++;
        }
    }

    *count = actualCount;
    return noErr;
}

/*
 * Internal Helper Functions
 */

static void InitializeFormatRegistry(void)
{
    if (gFormatsInitialized) {
        return;
    }

    memset(gFormatRegistry, 0, sizeof(gFormatRegistry));
    gFormatCount = 0;

    RegisterBuiltinFormats();
    gFormatsInitialized = true;
}

static OSErr RegisterBuiltinFormats(void)
{
    /* Register standard Mac OS formats */
    RegisterScrapFormat(SCRAP_TYPE_TEXT, "\pPlain Text", "\ptxt", TEXT_FORMAT_PLAIN);
    RegisterScrapFormat(SCRAP_TYPE_PICT, "\pQuickDraw Picture", "\ppict", IMAGE_FORMAT_VECTOR);
    RegisterScrapFormat(SCRAP_TYPE_SOUND, "\pSound Resource", "\psnd", SOUND_FORMAT_COMPRESSED);
    RegisterScrapFormat(SCRAP_TYPE_STYLE, "\pStyle Information", "\pstyl", TEXT_FORMAT_STYLED);
    RegisterScrapFormat(SCRAP_TYPE_STRING, "\pPascal String", "\pstr", TEXT_FORMAT_PLAIN);
    RegisterScrapFormat(SCRAP_TYPE_ICON, "\pIcon", "\picn", IMAGE_FORMAT_BITMAP);

    /* Register modern formats */
    RegisterScrapFormat(SCRAP_TYPE_UTF8, "\pUTF-8 Text", "\putf8", TEXT_FORMAT_UNICODE);
    RegisterScrapFormat(SCRAP_TYPE_RTF, "\pRich Text Format", "\prtf", TEXT_FORMAT_MARKUP);
    RegisterScrapFormat(SCRAP_TYPE_HTML, "\pHTML Markup", "\phtml", TEXT_FORMAT_MARKUP);
    RegisterScrapFormat(SCRAP_TYPE_PNG, "\pPNG Image", "\ppng", IMAGE_FORMAT_BITMAP | IMAGE_FORMAT_COMPRESSED);
    RegisterScrapFormat(SCRAP_TYPE_JPEG, "\pJPEG Image", "\pjpg", IMAGE_FORMAT_BITMAP | IMAGE_FORMAT_COMPRESSED | IMAGE_FORMAT_LOSSY);
    RegisterScrapFormat(SCRAP_TYPE_TIFF, "\pTIFF Image", "\ptiff", IMAGE_FORMAT_BITMAP);

    return noErr;
}

static FormatInfo *FindFormatInfo(ResType type)
{
    int16_t i;

    for (i = 0; i < gFormatCount; i++) {
        if (gFormatRegistry[i].type == type && gFormatRegistry[i].isRegistered) {
            return &gFormatRegistry[i];
        }
    }

    return NULL;
}

static Boolean IsTextData(const void *data, int32_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    int32_t i;
    int32_t printableCount = 0;

    if (size == 0) {
        return true; /* Empty data can be text */
    }

    /* Check for reasonable text characteristics */
    for (i = 0; i < size && i < 1024; i++) { /* Sample first 1K */
        uint8_t c = bytes[i];

        if (c == 0) {
            break; /* Null terminator suggests text */
        }

        if (isprint(c) || c == '\n' || c == '\r' || c == '\t') {
            printableCount++;
        }
    }

    /* Consider it text if most characters are printable */
    return (printableCount * 100 / (i ? i : 1)) > 80;
}

static Boolean IsPICTData(const void *data, int32_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;

    if (size < 10) {
        return false;
    }

    /* PICT files start with a size word, then a frame rectangle */
    /* This is a simplified check - real PICT validation is more complex */

    /* Check for reasonable frame rectangle values */
    int16_t top = (bytes[2] << 8) | bytes[3];
    int16_t left = (bytes[4] << 8) | bytes[5];
    int16_t bottom = (bytes[6] << 8) | bytes[7];
    int16_t right = (bytes[8] << 8) | bytes[9];

    return (top >= 0 && left >= 0 && bottom > top && right > left &&
            bottom - top < 32767 && right - left < 32767);
}

static Boolean IsSoundData(const void *data, int32_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;

    if (size < 20) {
        return false;
    }

    /* Check for sound resource header - simplified */
    /* Real sound resource validation would be more thorough */

    return false; /* For now, don't auto-detect sound */
}

static Boolean IsImageData(const void *data, int32_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;

    if (size < 8) {
        return false;
    }

    /* Check for common image signatures */

    /* PNG */
    if (memcmp(bytes, "\x89PNG\r\n\x1a\n", 8) == 0) {
        return true;
    }

    /* JPEG */
    if (bytes[0] == 0xFF && bytes[1] == 0xD8) {
        return true;
    }

    /* TIFF */
    if ((bytes[0] == 'I' && bytes[1] == 'I' && bytes[2] == 42 && bytes[3] == 0) ||
        (bytes[0] == 'M' && bytes[1] == 'M' && bytes[2] == 0 && bytes[3] == 42)) {
        return true;
    }

    return false;
}

static OSErr ValidateTextData(const void *data, int32_t size)
{
    /* Text data is generally valid if it doesn't contain control characters */
    /* except for common whitespace characters */

    return noErr; /* Assume valid for now */
}

static OSErr ValidatePICTData(const void *data, int32_t size)
{
    /* PICT validation would check for valid opcodes and structure */

    return noErr; /* Assume valid for now */
}

static OSErr ValidateSoundData(const void *data, int32_t size)
{
    /* Sound validation would check for valid sound resource structure */

    return noErr; /* Assume valid for now */
}