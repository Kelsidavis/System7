/*
 * WebFontSupport.c - Web Font Support Implementation
 *
 * Implements web font loading, CSS parsing, and font downloading
 * capabilities for the modern Font Manager.
 */

#include "../include/FontManager/ModernFonts.h"
#include "../include/FontManager/FontManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <curl/curl.h>
#endif

/* Web font download structure */
typedef struct WebFontDownload {
    void *data;
    size_t size;
    size_t capacity;
} WebFontDownload;

/* Internal helper functions */
static OSErr DownloadFile(ConstStr255Param url, void **data, unsigned long *size);
static OSErr ParseCSSFontFace(const char *css, WebFontMetadata *metadata);
static OSErr ExtractFontURL(const char *srcProperty, char **url, char **format);
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, WebFontDownload *download);

/*
 * DownloadWebFont - Download a web font from URL
 */
OSErr DownloadWebFont(ConstStr255Param url, ConstStr255Param cachePath,
                     WebFontMetadata *metadata)
{
    void *fontData = NULL;
    unsigned long fontSize = 0;
    FILE *cacheFile;
    char cUrl[512];
    char cCachePath[512];
    OSErr error;

    if (url == NULL || cachePath == NULL) {
        return paramErr;
    }

    /* Convert Pascal strings to C strings */
    BlockMoveData(&url[1], cUrl, url[0]);
    cUrl[url[0]] = '\0';

    BlockMoveData(&cachePath[1], cCachePath, cachePath[0]);
    cCachePath[cachePath[0]] = '\0';

    /* Download the font file */
    error = DownloadFile(url, &fontData, &fontSize);
    if (error != noErr) {
        return error;
    }

    /* Save to cache */
    cacheFile = fopen(cCachePath, "wb");
    if (cacheFile != NULL) {
        fwrite(fontData, 1, fontSize, cacheFile);
        fclose(cacheFile);
    }

    /* Detect font format and fill metadata */
    if (metadata != NULL) {
        unsigned short format = DetectFontFormat(fontData, fontSize);

        metadata->fileSize = fontSize;
        metadata->isValid = (format != 0);

        switch (format) {
            case kFontFormatWOFF:
                metadata->format = strdup("woff");
                break;
            case kFontFormatWOFF2:
                metadata->format = strdup("woff2");
                break;
            case kFontFormatOpenType:
                metadata->format = strdup("opentype");
                break;
            case kFontFormatTrueType:
                metadata->format = strdup("truetype");
                break;
            default:
                metadata->format = strdup("unknown");
                metadata->isValid = FALSE;
                break;
        }

        metadata->src = strdup(cUrl);
    }

    free(fontData);
    return noErr;
}

/*
 * LoadWebFont - Load a web font from cached file
 */
OSErr LoadWebFont(ConstStr255Param filePath, WebFontMetadata *metadata, ModernFont **font)
{
    FILE *file;
    unsigned long fileSize;
    void *fontData;
    char cFilePath[512];
    unsigned short format;
    OSErr error;
    ModernFont *webFont;

    if (filePath == NULL || font == NULL) {
        return paramErr;
    }

    /* Convert Pascal string to C string */
    BlockMoveData(&filePath[1], cFilePath, filePath[0]);
    cFilePath[filePath[0]] = '\0';

    /* Read font file */
    file = fopen(cFilePath, "rb");
    if (file == NULL) {
        return fontNotFoundErr;
    }

    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    fontData = malloc(fileSize);
    if (fontData == NULL) {
        fclose(file);
        return fontOutOfMemoryErr;
    }

    if (fread(fontData, 1, fileSize, file) != fileSize) {
        free(fontData);
        fclose(file);
        return fontCorruptErr;
    }
    fclose(file);

    /* Detect format */
    format = DetectFontFormat(fontData, fileSize);
    if (format == 0) {
        free(fontData);
        return kModernFontNotSupportedErr;
    }

    /* Create ModernFont structure */
    webFont = (ModernFont *)calloc(1, sizeof(ModernFont));
    if (webFont == NULL) {
        free(fontData);
        return fontOutOfMemoryErr;
    }

    webFont->format = format;
    webFont->dataSize = fileSize;
    webFont->isLoaded = TRUE;
    webFont->isValid = TRUE;

    /* Load font based on format */
    switch (format) {
        case kFontFormatWOFF:
        case kFontFormatWOFF2:
            error = ParseOpenTypeFont(fontData, fileSize, &webFont->data.openType);
            if (error == noErr) {
                webFont->familyName = strdup(webFont->data.openType->familyName);
                webFont->styleName = strdup(webFont->data.openType->styleName);
            }
            break;

        case kFontFormatOpenType:
        case kFontFormatTrueType:
            error = ParseOpenTypeFont(fontData, fileSize, &webFont->data.openType);
            if (error == noErr) {
                webFont->familyName = strdup(webFont->data.openType->familyName);
                webFont->styleName = strdup(webFont->data.openType->styleName);
            }
            break;

        default:
            error = kModernFontNotSupportedErr;
            break;
    }

    if (error != noErr) {
        free(webFont);
        free(fontData);
        return error;
    }

    /* Update metadata if provided */
    if (metadata != NULL) {
        metadata->fileSize = fileSize;
        metadata->isValid = TRUE;
        if (webFont->familyName != NULL) {
            metadata->fontFamily = strdup(webFont->familyName);
        }
    }

    free(fontData);
    *font = webFont;
    return noErr;
}

/*
 * ParseWebFontCSS - Parse CSS file for @font-face declarations
 */
OSErr ParseWebFontCSS(ConstStr255Param cssPath, WebFontMetadata **metadataArray,
                     unsigned long *count)
{
    FILE *file;
    char *cssContent;
    unsigned long fileSize;
    char cCssPath[512];
    char *current;
    char *fontFaceStart;
    WebFontMetadata *metadata;
    unsigned long metadataCount = 0;
    unsigned long metadataCapacity = 16;

    if (cssPath == NULL || metadataArray == NULL || count == NULL) {
        return paramErr;
    }

    /* Convert Pascal string to C string */
    BlockMoveData(&cssPath[1], cCssPath, cssPath[0]);
    cCssPath[cssPath[0]] = '\0';

    /* Read CSS file */
    file = fopen(cCssPath, "r");
    if (file == NULL) {
        return fontNotFoundErr;
    }

    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    cssContent = (char *)malloc(fileSize + 1);
    if (cssContent == NULL) {
        fclose(file);
        return fontOutOfMemoryErr;
    }

    if (fread(cssContent, 1, fileSize, file) != fileSize) {
        free(cssContent);
        fclose(file);
        return fontCorruptErr;
    }
    cssContent[fileSize] = '\0';
    fclose(file);

    /* Allocate metadata array */
    metadata = (WebFontMetadata *)calloc(metadataCapacity, sizeof(WebFontMetadata));
    if (metadata == NULL) {
        free(cssContent);
        return fontOutOfMemoryErr;
    }

    /* Parse @font-face declarations */
    current = cssContent;
    while ((fontFaceStart = strstr(current, "@font-face")) != NULL) {
        char *braceStart = strchr(fontFaceStart, '{');
        char *braceEnd = strchr(braceStart, '}');

        if (braceStart != NULL && braceEnd != NULL) {
            /* Extract @font-face block */
            size_t blockLen = braceEnd - braceStart + 1;
            char *fontFaceBlock = (char *)malloc(blockLen + 1);
            if (fontFaceBlock != NULL) {
                memcpy(fontFaceBlock, braceStart, blockLen);
                fontFaceBlock[blockLen] = '\0';

                /* Parse the font-face block */
                if (metadataCount >= metadataCapacity) {
                    metadataCapacity *= 2;
                    metadata = (WebFontMetadata *)realloc(metadata,
                                                         metadataCapacity * sizeof(WebFontMetadata));
                    if (metadata == NULL) {
                        free(fontFaceBlock);
                        free(cssContent);
                        return fontOutOfMemoryErr;
                    }
                }

                ParseCSSFontFace(fontFaceBlock, &metadata[metadataCount]);
                metadataCount++;

                free(fontFaceBlock);
            }
        }

        current = (braceEnd != NULL) ? braceEnd + 1 : fontFaceStart + 10;
    }

    free(cssContent);

    *metadataArray = metadata;
    *count = metadataCount;
    return noErr;
}

/*
 * ValidateWebFont - Validate a web font file
 */
OSErr ValidateWebFont(ConstStr255Param filePath, WebFontMetadata *metadata)
{
    OSErr error;
    unsigned short format;
    char *familyName = NULL;
    char *styleName = NULL;

    if (filePath == NULL) {
        return paramErr;
    }

    /* Get font file information */
    error = GetFontFileInfo(filePath, &format, &familyName, &styleName);
    if (error != noErr) {
        return error;
    }

    /* Validate format is web-compatible */
    if (format != kFontFormatWOFF &&
        format != kFontFormatWOFF2 &&
        format != kFontFormatOpenType &&
        format != kFontFormatTrueType) {
        if (familyName) free(familyName);
        if (styleName) free(styleName);
        return kModernFontNotSupportedErr;
    }

    /* Update metadata if provided */
    if (metadata != NULL) {
        metadata->isValid = TRUE;
        if (familyName != NULL) {
            metadata->fontFamily = strdup(familyName);
        }
        if (styleName != NULL) {
            metadata->fontStyle = strdup(styleName);
        }

        switch (format) {
            case kFontFormatWOFF:
                metadata->format = strdup("woff");
                break;
            case kFontFormatWOFF2:
                metadata->format = strdup("woff2");
                break;
            case kFontFormatOpenType:
                metadata->format = strdup("opentype");
                break;
            case kFontFormatTrueType:
                metadata->format = strdup("truetype");
                break;
        }
    }

    if (familyName) free(familyName);
    if (styleName) free(styleName);
    return noErr;
}

/* Internal helper functions */

#ifdef _WIN32
static OSErr DownloadFile(ConstStr255Param url, void **data, unsigned long *size)
{
    HINTERNET hInternet, hUrl;
    DWORD bytesRead;
    WebFontDownload download = {0};
    char cUrl[512];

    /* Convert Pascal string to C string */
    BlockMoveData(&url[1], cUrl, url[0]);
    cUrl[url[0]] = '\0';

    /* Initialize WinINet */
    hInternet = InternetOpen("FontManager/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (hInternet == NULL) {
        return kModernFontNetworkErr;
    }

    /* Open URL */
    hUrl = InternetOpenUrl(hInternet, cUrl, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hUrl == NULL) {
        InternetCloseHandle(hInternet);
        return kModernFontNetworkErr;
    }

    /* Download data */
    download.capacity = 1024 * 1024; /* 1MB initial */
    download.data = malloc(download.capacity);
    if (download.data == NULL) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return fontOutOfMemoryErr;
    }

    char buffer[8192];
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        if (download.size + bytesRead > download.capacity) {
            download.capacity *= 2;
            download.data = realloc(download.data, download.capacity);
            if (download.data == NULL) {
                InternetCloseHandle(hUrl);
                InternetCloseHandle(hInternet);
                return fontOutOfMemoryErr;
            }
        }
        memcpy((char*)download.data + download.size, buffer, bytesRead);
        download.size += bytesRead;
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    *data = download.data;
    *size = download.size;
    return noErr;
}

#elif defined(__APPLE__) || defined(__linux__)

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, WebFontDownload *download)
{
    size_t realsize = size * nmemb;

    if (download->size + realsize > download->capacity) {
        download->capacity = (download->size + realsize) * 2;
        download->data = realloc(download->data, download->capacity);
        if (download->data == NULL) {
            return 0; /* Out of memory */
        }
    }

    memcpy((char*)download->data + download->size, contents, realsize);
    download->size += realsize;

    return realsize;
}

static OSErr DownloadFile(ConstStr255Param url, void **data, unsigned long *size)
{
    CURL *curl;
    CURLcode res;
    WebFontDownload download = {0};
    char cUrl[512];

    /* Convert Pascal string to C string */
    BlockMoveData(&url[1], cUrl, url[0]);
    cUrl[url[0]] = '\0';

    /* Initialize libcurl */
    curl = curl_easy_init();
    if (curl == NULL) {
        return kModernFontNetworkErr;
    }

    /* Initialize download buffer */
    download.capacity = 1024 * 1024; /* 1MB initial */
    download.data = malloc(download.capacity);
    if (download.data == NULL) {
        curl_easy_cleanup(curl);
        return fontOutOfMemoryErr;
    }

    /* Configure curl */
    curl_easy_setopt(curl, CURLOPT_URL, cUrl);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    /* Perform the download */
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(download.data);
        return kModernFontNetworkErr;
    }

    *data = download.data;
    *size = download.size;
    return noErr;
}

#else

static OSErr DownloadFile(ConstStr255Param url, void **data, unsigned long *size)
{
    /* Fallback implementation without network support */
    return kModernFontNotSupportedErr;
}

#endif

static OSErr ParseCSSFontFace(const char *css, WebFontMetadata *metadata)
{
    char *property;
    char *value;
    char *cssBlock;

    if (css == NULL || metadata == NULL) {
        return paramErr;
    }

    /* Initialize metadata */
    memset(metadata, 0, sizeof(WebFontMetadata));

    /* Make a working copy of the CSS block */
    cssBlock = strdup(css);
    if (cssBlock == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Parse font-family */
    property = strstr(cssBlock, "font-family:");
    if (property != NULL) {
        value = strchr(property, ':') + 1;
        char *semicolon = strchr(value, ';');
        if (semicolon != NULL) {
            *semicolon = '\0';
        }
        /* Trim whitespace and quotes */
        while (*value == ' ' || *value == '"' || *value == '\'') value++;
        char *end = value + strlen(value) - 1;
        while (end > value && (*end == ' ' || *end == '"' || *end == '\'' || *end == ';')) {
            *end = '\0';
            end--;
        }
        metadata->fontFamily = strdup(value);
    }

    /* Parse font-style */
    property = strstr(cssBlock, "font-style:");
    if (property != NULL) {
        value = strchr(property, ':') + 1;
        char *semicolon = strchr(value, ';');
        if (semicolon != NULL) {
            *semicolon = '\0';
        }
        while (*value == ' ') value++;
        char *end = value + strlen(value) - 1;
        while (end > value && (*end == ' ' || *end == ';')) {
            *end = '\0';
            end--;
        }
        metadata->fontStyle = strdup(value);
    }

    /* Parse font-weight */
    property = strstr(cssBlock, "font-weight:");
    if (property != NULL) {
        value = strchr(property, ':') + 1;
        char *semicolon = strchr(value, ';');
        if (semicolon != NULL) {
            *semicolon = '\0';
        }
        while (*value == ' ') value++;
        char *end = value + strlen(value) - 1;
        while (end > value && (*end == ' ' || *end == ';')) {
            *end = '\0';
            end--;
        }
        metadata->fontWeight = strdup(value);
    }

    /* Parse src */
    property = strstr(cssBlock, "src:");
    if (property != NULL) {
        value = strchr(property, ':') + 1;
        char *semicolon = strchr(value, ';');
        if (semicolon != NULL) {
            *semicolon = '\0';
        }
        metadata->src = strdup(value);

        /* Extract URL and format from src */
        char *url = NULL;
        char *format = NULL;
        ExtractFontURL(value, &url, &format);
        if (format != NULL) {
            metadata->format = format;
        }
    }

    metadata->isValid = (metadata->fontFamily != NULL && metadata->src != NULL);

    free(cssBlock);
    return noErr;
}

static OSErr ExtractFontURL(const char *srcProperty, char **url, char **format)
{
    const char *urlStart = strstr(srcProperty, "url(");
    if (urlStart == NULL) {
        return paramErr;
    }

    urlStart += 4; /* Skip "url(" */
    while (*urlStart == ' ' || *urlStart == '"' || *urlStart == '\'') {
        urlStart++;
    }

    const char *urlEnd = strstr(urlStart, ")");
    if (urlEnd == NULL) {
        return paramErr;
    }

    /* Back up over quotes and spaces */
    urlEnd--;
    while (urlEnd > urlStart && (*urlEnd == ' ' || *urlEnd == '"' || *urlEnd == '\'')) {
        urlEnd--;
    }

    /* Extract URL */
    if (url != NULL) {
        size_t urlLen = urlEnd - urlStart + 1;
        *url = (char *)malloc(urlLen + 1);
        if (*url != NULL) {
            memcpy(*url, urlStart, urlLen);
            (*url)[urlLen] = '\0';
        }
    }

    /* Look for format hint */
    const char *formatStart = strstr(srcProperty, "format(");
    if (formatStart != NULL && format != NULL) {
        formatStart += 8; /* Skip "format(" */
        while (*formatStart == ' ' || *formatStart == '"' || *formatStart == '\'') {
            formatStart++;
        }

        const char *formatEnd = strchr(formatStart, ')');
        if (formatEnd != NULL) {
            formatEnd--;
            while (formatEnd > formatStart && (*formatEnd == ' ' || *formatEnd == '"' || *formatEnd == '\'')) {
                formatEnd--;
            }

            size_t formatLen = formatEnd - formatStart + 1;
            *format = (char *)malloc(formatLen + 1);
            if (*format != NULL) {
                memcpy(*format, formatStart, formatLen);
                (*format)[formatLen] = '\0';
            }
        }
    }

    return noErr;
}