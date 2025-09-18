/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * ICCProfiles.c - ICC Profile Management Implementation
 *
 * Implementation of ICC profile loading, creation, validation, and management
 * compatible with ICC v2 and v4 specifications.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager and ColorSync
 */

#include "../include/ColorManager/ICCProfiles.h"
#include "../include/ColorManager/ColorSpaces.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <assert.h>

/* ================================================================
 * INTERNAL STRUCTURES
 * ================================================================ */

/* Internal ICC profile data */
typedef struct {
    CMICCHeader     header;
    CMICCTagTable  *tagTable;
    void           *profileData;
    uint32_t       profileSize;
    bool           isDirty;
    bool           isValid;
} CMICCProfileData;

/* ================================================================
 * GLOBAL STATE
 * ================================================================ */

static bool g_iccInitialized = false;
static uint32_t g_platformSignature = 'APPL';  /* Apple platform */
static uint32_t g_cmmSignature = 'ADBE';       /* Adobe CMM */

/* Standard primaries and white points */
static const CMXYZColor g_sRGBRed   = {41246, 21267, 1933};
static const CMXYZColor g_sRGBGreen = {35758, 71515, 11919};
static const CMXYZColor g_sRGBBlue  = {18045, 7217, 95030};
static const CMXYZColor g_d65White  = {95047, 100000, 108883};

/* Forward declarations */
static CMError CreateICCHeader(CMICCProfileData *iccData, CMProfileClass profileClass,
                              CMColorSpace dataSpace, CMColorSpace pcs);
static CMError AddStandardTags(CMICCProfileData *iccData, CMProfileClass profileClass);
static CMError WriteTagToProfile(CMICCProfileData *iccData, uint32_t signature,
                                const void *data, uint32_t size);
static CMError ReadTagFromProfile(CMICCProfileData *iccData, uint32_t signature,
                                 void *data, uint32_t *size);
static void UpdateDateTime(uint8_t *dateTime);
static uint32_t SwapBytes32(uint32_t value);
static uint16_t SwapBytes16(uint16_t value);
static bool IsLittleEndian(void);

/* ================================================================
 * ICC PROFILE SYSTEM INITIALIZATION
 * ================================================================ */

CMError CMInitICCProfiles(void) {
    if (g_iccInitialized) {
        return cmNoError;
    }

    /* Set platform-specific defaults */
#ifdef __APPLE__
    g_platformSignature = 'APPL';
#elif defined(_WIN32)
    g_platformSignature = 'MSFT';
#elif defined(__linux__)
    g_platformSignature = 'SGI ';  /* Generic Unix */
#else
    g_platformSignature = 'UNKN';
#endif

    g_iccInitialized = true;
    return cmNoError;
}

/* ================================================================
 * ICC PROFILE CREATION
 * ================================================================ */

CMError CMCreateICCProfile(CMProfileRef prof, CMProfileClass profileClass,
                          CMColorSpace dataSpace, CMColorSpace pcs) {
    if (!prof) return cmParameterError;

    /* Allocate ICC data structure */
    CMICCProfileData *iccData = (CMICCProfileData *)calloc(1, sizeof(CMICCProfileData));
    if (!iccData) return cmProfileError;

    /* Create ICC header */
    CMError err = CreateICCHeader(iccData, profileClass, dataSpace, pcs);
    if (err != cmNoError) {
        free(iccData);
        return err;
    }

    /* Add standard tags */
    err = AddStandardTags(iccData, profileClass);
    if (err != cmNoError) {
        if (iccData->tagTable) free(iccData->tagTable);
        free(iccData);
        return err;
    }

    /* Store ICC data in profile */
    prof->profileData = iccData;
    prof->isValid = true;

    return cmNoError;
}

CMError CMCreateDefaultICCProfile(CMProfileRef prof) {
    return CMCreateSRGBProfile(&prof);
}

CMError CMLoadICCProfileFromData(CMProfileRef prof, const void *data, uint32_t size) {
    if (!prof || !data || size < 128) return cmParameterError;

    /* Validate ICC header */
    bool isValid;
    CMError err = CMValidateICCProfile(data, size, &isValid);
    if (err != cmNoError || !isValid) {
        return cmFatalProfileErr;
    }

    /* Allocate ICC data structure */
    CMICCProfileData *iccData = (CMICCProfileData *)calloc(1, sizeof(CMICCProfileData));
    if (!iccData) return cmProfileError;

    /* Copy profile data */
    iccData->profileData = malloc(size);
    if (!iccData->profileData) {
        free(iccData);
        return cmProfileError;
    }

    memcpy(iccData->profileData, data, size);
    iccData->profileSize = size;

    /* Parse header */
    const CMICCHeader *srcHeader = (const CMICCHeader *)data;
    iccData->header = *srcHeader;

    /* Fix byte order if needed */
    if (IsLittleEndian()) {
        iccData->header.profileSize = SwapBytes32(iccData->header.profileSize);
        iccData->header.version = SwapBytes32(iccData->header.version);
        iccData->header.deviceClass = SwapBytes32(iccData->header.deviceClass);
        iccData->header.dataColorSpace = SwapBytes32(iccData->header.dataColorSpace);
        iccData->header.pcs = SwapBytes32(iccData->header.pcs);
        iccData->header.signature = SwapBytes32(iccData->header.signature);
        iccData->header.platform = SwapBytes32(iccData->header.platform);
        iccData->header.flags = SwapBytes32(iccData->header.flags);
    }

    /* Parse tag table */
    const uint8_t *profileBytes = (const uint8_t *)data;
    const uint32_t *tagCountPtr = (const uint32_t *)(profileBytes + 128);
    uint32_t tagCount = IsLittleEndian() ? SwapBytes32(*tagCountPtr) : *tagCountPtr;

    uint32_t tagTableSize = sizeof(CMICCTagTable) + (tagCount - 1) * sizeof(CMICCTagEntry);
    iccData->tagTable = (CMICCTagTable *)malloc(tagTableSize);
    if (!iccData->tagTable) {
        free(iccData->profileData);
        free(iccData);
        return cmProfileError;
    }

    iccData->tagTable->tagCount = tagCount;

    /* Copy tag entries */
    const CMICCTagEntry *srcTags = (const CMICCTagEntry *)(profileBytes + 132);
    for (uint32_t i = 0; i < tagCount; i++) {
        iccData->tagTable->tags[i] = srcTags[i];
        if (IsLittleEndian()) {
            iccData->tagTable->tags[i].signature = SwapBytes32(iccData->tagTable->tags[i].signature);
            iccData->tagTable->tags[i].offset = SwapBytes32(iccData->tagTable->tags[i].offset);
            iccData->tagTable->tags[i].size = SwapBytes32(iccData->tagTable->tags[i].size);
        }
    }

    iccData->isValid = true;
    prof->profileData = iccData;

    return cmNoError;
}

CMError CMSaveICCProfileToData(CMProfileRef prof, void **data, uint32_t *size) {
    if (!prof || !data || !size) return cmParameterError;

    CMICCProfileData *iccData = (CMICCProfileData *)prof->profileData;
    if (!iccData) return cmInvalidProfileID;

    /* Calculate total size needed */
    uint32_t headerSize = 128;
    uint32_t tagTableSize = 4 + iccData->tagTable->tagCount * 12;
    uint32_t totalSize = headerSize + tagTableSize;

    /* Add tag data sizes */
    for (uint32_t i = 0; i < iccData->tagTable->tagCount; i++) {
        totalSize += iccData->tagTable->tags[i].size;
        /* Align to 4-byte boundary */
        totalSize = (totalSize + 3) & ~3;
    }

    /* Allocate output buffer */
    uint8_t *output = (uint8_t *)malloc(totalSize);
    if (!output) return cmProfileError;

    /* Write header */
    memcpy(output, &iccData->header, headerSize);

    /* Update profile size in header */
    uint32_t *profileSizePtr = (uint32_t *)output;
    *profileSizePtr = IsLittleEndian() ? SwapBytes32(totalSize) : totalSize;

    /* Write tag table */
    uint32_t *tagCountPtr = (uint32_t *)(output + headerSize);
    *tagCountPtr = IsLittleEndian() ? SwapBytes32(iccData->tagTable->tagCount) : iccData->tagTable->tagCount;

    CMICCTagEntry *outputTags = (CMICCTagEntry *)(output + headerSize + 4);
    uint32_t currentOffset = headerSize + tagTableSize;

    for (uint32_t i = 0; i < iccData->tagTable->tagCount; i++) {
        outputTags[i].signature = iccData->tagTable->tags[i].signature;
        outputTags[i].offset = IsLittleEndian() ? SwapBytes32(currentOffset) : currentOffset;
        outputTags[i].size = iccData->tagTable->tags[i].size;

        if (IsLittleEndian()) {
            outputTags[i].signature = SwapBytes32(outputTags[i].signature);
            outputTags[i].size = SwapBytes32(outputTags[i].size);
        }

        /* Copy tag data (implementation would fetch actual tag data) */
        currentOffset += iccData->tagTable->tags[i].size;
        currentOffset = (currentOffset + 3) & ~3;  /* Align */
    }

    *data = output;
    *size = totalSize;

    return cmNoError;
}

CMError CMValidateICCProfile(const void *data, uint32_t size, bool *isValid) {
    if (!data || !isValid || size < 128) {
        if (isValid) *isValid = false;
        return cmParameterError;
    }

    *isValid = false;

    const CMICCHeader *header = (const CMICCHeader *)data;

    /* Check profile signature */
    uint32_t signature = IsLittleEndian() ? SwapBytes32(header->signature) : header->signature;
    if (signature != kICCProfileSignature) {
        return cmNoError;
    }

    /* Check profile size */
    uint32_t profileSize = IsLittleEndian() ? SwapBytes32(header->profileSize) : header->profileSize;
    if (profileSize != size || profileSize < 128) {
        return cmNoError;
    }

    /* Check version */
    uint32_t version = IsLittleEndian() ? SwapBytes32(header->version) : header->version;
    uint8_t majorVersion = (version >> 24) & 0xFF;
    if (majorVersion < 2 || majorVersion > 4) {
        return cmNoError;
    }

    /* Check device class */
    uint32_t deviceClass = IsLittleEndian() ? SwapBytes32(header->deviceClass) : header->deviceClass;
    if (deviceClass != cmInputClass && deviceClass != cmDisplayClass &&
        deviceClass != cmOutputClass && deviceClass != cmLinkClass &&
        deviceClass != cmAbstractClass && deviceClass != cmColorSpaceClass &&
        deviceClass != cmNamedColorClass) {
        return cmNoError;
    }

    /* Validate tag table */
    if (size > 132) {
        const uint32_t *tagCountPtr = (const uint32_t *)((const uint8_t *)data + 128);
        uint32_t tagCount = IsLittleEndian() ? SwapBytes32(*tagCountPtr) : *tagCountPtr;

        if (tagCount > 0 && (132 + tagCount * 12) <= size) {
            *isValid = true;
        }
    }

    return cmNoError;
}

/* ================================================================
 * ICC HEADER MANAGEMENT
 * ================================================================ */

CMError CMGetICCHeader(CMProfileRef prof, CMICCHeader *header) {
    if (!prof || !header) return cmParameterError;

    CMICCProfileData *iccData = (CMICCProfileData *)prof->profileData;
    if (!iccData) return cmInvalidProfileID;

    *header = iccData->header;
    return cmNoError;
}

CMError CMSetICCHeader(CMProfileRef prof, const CMICCHeader *header) {
    if (!prof || !header) return cmParameterError;

    CMICCProfileData *iccData = (CMICCProfileData *)prof->profileData;
    if (!iccData) return cmInvalidProfileID;

    iccData->header = *header;
    iccData->isDirty = true;
    return cmNoError;
}

CMError CMUpdateICCHeader(CMProfileRef prof) {
    if (!prof) return cmParameterError;

    CMICCProfileData *iccData = (CMICCProfileData *)prof->profileData;
    if (!iccData) return cmInvalidProfileID;

    /* Update modification date */
    UpdateDateTime(iccData->header.dateTime);

    /* Update platform signature */
    iccData->header.platform = g_platformSignature;

    /* Update CMM signature */
    iccData->header.preferredCMM = g_cmmSignature;

    iccData->isDirty = true;
    return cmNoError;
}

/* ================================================================
 * ICC TAG MANAGEMENT
 * ================================================================ */

CMError CMGetICCTagCount(CMProfileRef prof, uint32_t *count) {
    if (!prof || !count) return cmParameterError;

    CMICCProfileData *iccData = (CMICCProfileData *)prof->profileData;
    if (!iccData || !iccData->tagTable) return cmInvalidProfileID;

    *count = iccData->tagTable->tagCount;
    return cmNoError;
}

CMError CMGetICCTagInfo(CMProfileRef prof, uint32_t index,
                       uint32_t *signature, uint32_t *size) {
    if (!prof) return cmParameterError;

    CMICCProfileData *iccData = (CMICCProfileData *)prof->profileData;
    if (!iccData || !iccData->tagTable) return cmInvalidProfileID;

    if (index >= iccData->tagTable->tagCount) return cmElementTagNotFound;

    if (signature) *signature = iccData->tagTable->tags[index].signature;
    if (size) *size = iccData->tagTable->tags[index].size;

    return cmNoError;
}

CMError CMGetICCTagData(CMProfileRef prof, uint32_t signature,
                       void *data, uint32_t *size) {
    if (!prof || !size) return cmParameterError;

    CMICCProfileData *iccData = (CMICCProfileData *)prof->profileData;
    if (!iccData) return cmInvalidProfileID;

    return ReadTagFromProfile(iccData, signature, data, size);
}

CMError CMSetICCTagData(CMProfileRef prof, uint32_t signature,
                       const void *data, uint32_t size) {
    if (!prof || !data || size == 0) return cmParameterError;

    CMICCProfileData *iccData = (CMICCProfileData *)prof->profileData;
    if (!iccData) return cmInvalidProfileID;

    return WriteTagToProfile(iccData, signature, data, size);
}

bool CMICCTagExists(CMProfileRef prof, uint32_t signature) {
    if (!prof) return false;

    CMICCProfileData *iccData = (CMICCProfileData *)prof->profileData;
    if (!iccData || !iccData->tagTable) return false;

    for (uint32_t i = 0; i < iccData->tagTable->tagCount; i++) {
        if (iccData->tagTable->tags[i].signature == signature) {
            return true;
        }
    }
    return false;
}

/* ================================================================
 * STANDARD ICC TAGS
 * ================================================================ */

CMError CMGetRedColorant(CMProfileRef prof, CMXYZColor *xyz) {
    if (!prof || !xyz) return cmParameterError;

    CMICCXYZData xyzData;
    uint32_t size = sizeof(xyzData);
    CMError err = CMGetICCTagData(prof, kICCRedColorantTag, &xyzData, &size);
    if (err != cmNoError) return err;

    xyz->X = IsLittleEndian() ? SwapBytes32(xyzData.X) : xyzData.X;
    xyz->Y = IsLittleEndian() ? SwapBytes32(xyzData.Y) : xyzData.Y;
    xyz->Z = IsLittleEndian() ? SwapBytes32(xyzData.Z) : xyzData.Z;

    return cmNoError;
}

CMError CMSetRedColorant(CMProfileRef prof, const CMXYZColor *xyz) {
    if (!prof || !xyz) return cmParameterError;

    CMICCXYZData xyzData;
    xyzData.type = IsLittleEndian() ? SwapBytes32(kICCXYZType) : kICCXYZType;
    xyzData.reserved = 0;
    xyzData.X = IsLittleEndian() ? SwapBytes32(xyz->X) : xyz->X;
    xyzData.Y = IsLittleEndian() ? SwapBytes32(xyz->Y) : xyz->Y;
    xyzData.Z = IsLittleEndian() ? SwapBytes32(xyz->Z) : xyz->Z;

    return CMSetICCTagData(prof, kICCRedColorantTag, &xyzData, sizeof(xyzData));
}

CMError CMGetWhitePoint(CMProfileRef prof, CMXYZColor *whitePoint) {
    if (!prof || !whitePoint) return cmParameterError;

    CMICCXYZData xyzData;
    uint32_t size = sizeof(xyzData);
    CMError err = CMGetICCTagData(prof, kICCWhitePointTag, &xyzData, &size);
    if (err != cmNoError) return err;

    whitePoint->X = IsLittleEndian() ? SwapBytes32(xyzData.X) : xyzData.X;
    whitePoint->Y = IsLittleEndian() ? SwapBytes32(xyzData.Y) : xyzData.Y;
    whitePoint->Z = IsLittleEndian() ? SwapBytes32(xyzData.Z) : xyzData.Z;

    return cmNoError;
}

CMError CMSetWhitePoint(CMProfileRef prof, const CMXYZColor *whitePoint) {
    if (!prof || !whitePoint) return cmParameterError;

    CMICCXYZData xyzData;
    xyzData.type = IsLittleEndian() ? SwapBytes32(kICCXYZType) : kICCXYZType;
    xyzData.reserved = 0;
    xyzData.X = IsLittleEndian() ? SwapBytes32(whitePoint->X) : whitePoint->X;
    xyzData.Y = IsLittleEndian() ? SwapBytes32(whitePoint->Y) : whitePoint->Y;
    xyzData.Z = IsLittleEndian() ? SwapBytes32(whitePoint->Z) : whitePoint->Z;

    return CMSetICCTagData(prof, kICCWhitePointTag, &xyzData, sizeof(xyzData));
}

/* ================================================================
 * STANDARD PROFILE CREATION
 * ================================================================ */

CMError CMCreateSRGBProfile(CMProfileRef *prof) {
    if (!prof) return cmParameterError;

    CMError err = CMNewProfile(prof, cmDisplayClass, cmRGBSpace, cmXYZSpace);
    if (err != cmNoError) return err;

    /* Set colorants */
    CMSetRedColorant(*prof, &g_sRGBRed);
    CMSetGreenColorant(*prof, &g_sRGBGreen);
    CMSetBlueColorant(*prof, &g_sRGBBlue);
    CMSetWhitePoint(*prof, &g_d65White);

    /* Set gamma curves (sRGB) */
    uint16_t *srgbCurve;
    uint32_t curveCount;
    err = CMCreateSRGBCurve(&srgbCurve, &curveCount);
    if (err == cmNoError) {
        CMSetRedTRC(*prof, srgbCurve, curveCount);
        CMSetGreenTRC(*prof, srgbCurve, curveCount);
        CMSetBlueTRC(*prof, srgbCurve, curveCount);
        free(srgbCurve);
    }

    /* Set description */
    CMSetProfileDescription(*prof, "sRGB IEC61966-2.1");

    return cmNoError;
}

CMError CMCreateGrayProfile(CMProfileRef *prof, float gamma) {
    if (!prof) return cmParameterError;

    CMError err = CMNewProfile(prof, cmDisplayClass, cmGraySpace, cmXYZSpace);
    if (err != cmNoError) return err;

    /* Set white point */
    CMSetWhitePoint(*prof, &g_d65White);

    /* Set gamma curve */
    uint16_t *gammaCurve;
    uint32_t curveCount;
    err = CMCreateGammaCurve(gamma, &gammaCurve, &curveCount);
    if (err == cmNoError) {
        CMSetGrayTRC(*prof, gammaCurve, curveCount);
        free(gammaCurve);
    }

    /* Set description */
    char description[64];
    snprintf(description, sizeof(description), "Gray Gamma %.1f", gamma);
    CMSetProfileDescription(*prof, description);

    return cmNoError;
}

/* ================================================================
 * CURVE UTILITIES
 * ================================================================ */

CMError CMCreateGammaCurve(float gamma, uint16_t **curve, uint32_t *count) {
    if (!curve || !count || gamma <= 0.0f) return cmParameterError;

    const uint32_t numPoints = 256;
    uint16_t *newCurve = (uint16_t *)malloc(numPoints * sizeof(uint16_t));
    if (!newCurve) return cmProfileError;

    for (uint32_t i = 0; i < numPoints; i++) {
        float input = i / 255.0f;
        float output = powf(input, gamma);
        newCurve[i] = (uint16_t)(output * 65535.0f);
    }

    *curve = newCurve;
    *count = numPoints;
    return cmNoError;
}

CMError CMCreateSRGBCurve(uint16_t **curve, uint32_t *count) {
    if (!curve || !count) return cmParameterError;

    const uint32_t numPoints = 256;
    uint16_t *newCurve = (uint16_t *)malloc(numPoints * sizeof(uint16_t));
    if (!newCurve) return cmProfileError;

    for (uint32_t i = 0; i < numPoints; i++) {
        float input = i / 255.0f;
        float output;

        /* sRGB transfer function */
        if (input <= 0.04045f) {
            output = input / 12.92f;
        } else {
            output = powf((input + 0.055f) / 1.055f, 2.4f);
        }

        newCurve[i] = (uint16_t)(output * 65535.0f);
    }

    *curve = newCurve;
    *count = numPoints;
    return cmNoError;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static CMError CreateICCHeader(CMICCProfileData *iccData, CMProfileClass profileClass,
                              CMColorSpace dataSpace, CMColorSpace pcs) {
    memset(&iccData->header, 0, sizeof(CMICCHeader));

    iccData->header.profileSize = 0;  /* Will be set when saving */
    iccData->header.preferredCMM = g_cmmSignature;
    iccData->header.version = 0x02200000;  /* Version 2.2 */
    iccData->header.deviceClass = profileClass;
    iccData->header.dataColorSpace = dataSpace;
    iccData->header.pcs = pcs;
    iccData->header.signature = kICCProfileSignature;
    iccData->header.platform = g_platformSignature;
    iccData->header.flags = 0;
    iccData->header.deviceManufacturer = 0;
    iccData->header.deviceModel = 0;
    iccData->header.deviceAttributes = 0;
    iccData->header.renderingIntent = cmPerceptual;
    iccData->header.creator = 'CM71';  /* Color Manager 7.1 */

    /* Set PCS illuminant (D50) */
    iccData->header.pcsIlluminant[0] = 96422;  /* X */
    iccData->header.pcsIlluminant[1] = 100000; /* Y */
    iccData->header.pcsIlluminant[2] = 82521;  /* Z */

    /* Set creation date */
    UpdateDateTime(iccData->header.dateTime);

    return cmNoError;
}

static CMError AddStandardTags(CMICCProfileData *iccData, CMProfileClass profileClass) {
    /* Allocate tag table */
    uint32_t maxTags = 20;  /* Reasonable number of standard tags */
    uint32_t tagTableSize = sizeof(CMICCTagTable) + (maxTags - 1) * sizeof(CMICCTagEntry);
    iccData->tagTable = (CMICCTagTable *)calloc(1, tagTableSize);
    if (!iccData->tagTable) return cmProfileError;

    iccData->tagTable->tagCount = 0;

    /* Add required tags based on profile class */
    switch (profileClass) {
        case cmDisplayClass:
            /* RGB display profiles need colorant and TRC tags */
            if (iccData->header.dataColorSpace == cmRGBSpace) {
                /* These would be added when setting actual values */
            }
            break;

        case cmInputClass:
        case cmOutputClass:
            /* Input/output profiles need appropriate LUT tags */
            break;

        default:
            break;
    }

    return cmNoError;
}

static CMError WriteTagToProfile(CMICCProfileData *iccData, uint32_t signature,
                                const void *data, uint32_t size) {
    if (!iccData || !data || size == 0) return cmParameterError;

    /* Find existing tag or add new one */
    int32_t tagIndex = -1;
    for (uint32_t i = 0; i < iccData->tagTable->tagCount; i++) {
        if (iccData->tagTable->tags[i].signature == signature) {
            tagIndex = i;
            break;
        }
    }

    if (tagIndex == -1) {
        /* Add new tag */
        tagIndex = iccData->tagTable->tagCount;
        iccData->tagTable->tagCount++;
    }

    /* Update tag entry */
    iccData->tagTable->tags[tagIndex].signature = signature;
    iccData->tagTable->tags[tagIndex].size = size;
    iccData->tagTable->tags[tagIndex].offset = 0;  /* Will be calculated when saving */

    iccData->isDirty = true;
    return cmNoError;
}

static CMError ReadTagFromProfile(CMICCProfileData *iccData, uint32_t signature,
                                 void *data, uint32_t *size) {
    if (!iccData || !size) return cmParameterError;

    /* Find tag */
    for (uint32_t i = 0; i < iccData->tagTable->tagCount; i++) {
        if (iccData->tagTable->tags[i].signature == signature) {
            uint32_t tagSize = iccData->tagTable->tags[i].size;
            if (data && *size >= tagSize) {
                /* Would copy actual tag data from profile */
                /* For now, return size */
            }
            *size = tagSize;
            return cmNoError;
        }
    }

    return cmElementTagNotFound;
}

static void UpdateDateTime(uint8_t *dateTime) {
    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);

    /* ICC uses a 12-byte date/time format */
    uint16_t year = 1900 + utc->tm_year;
    uint16_t month = utc->tm_mon + 1;
    uint16_t day = utc->tm_mday;
    uint16_t hour = utc->tm_hour;
    uint16_t minute = utc->tm_min;
    uint16_t second = utc->tm_sec;

    /* Store in big-endian format */
    dateTime[0] = (year >> 8) & 0xFF;
    dateTime[1] = year & 0xFF;
    dateTime[2] = (month >> 8) & 0xFF;
    dateTime[3] = month & 0xFF;
    dateTime[4] = (day >> 8) & 0xFF;
    dateTime[5] = day & 0xFF;
    dateTime[6] = (hour >> 8) & 0xFF;
    dateTime[7] = hour & 0xFF;
    dateTime[8] = (minute >> 8) & 0xFF;
    dateTime[9] = minute & 0xFF;
    dateTime[10] = (second >> 8) & 0xFF;
    dateTime[11] = second & 0xFF;
}

static uint32_t SwapBytes32(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

static uint16_t SwapBytes16(uint16_t value) {
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

static bool IsLittleEndian(void) {
    uint16_t test = 1;
    return *((uint8_t *)&test) == 1;
}