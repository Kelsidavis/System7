/*
 * ICCProfiles.h - ICC Profile Management
 *
 * ICC profile loading, creation, validation, and management for professional
 * color workflows compatible with ICC v2 and v4 specifications.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager and ColorSync
 */

#ifndef ICCPROFILES_H
#define ICCPROFILES_H

#include "ColorManager.h"
#include "ColorSpaces.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * ICC PROFILE CONSTANTS
 * ================================================================ */

/* ICC signature constants */
#define kICCProfileSignature    'acsp'
#define kICCVersionMajor        2
#define kICCVersionMinor        0

/* ICC tag signatures */
#define kICCRedColorantTag      'rXYZ'
#define kICCGreenColorantTag    'gXYZ'
#define kICCBlueColorantTag     'bXYZ'
#define kICCWhitePointTag       'wtpt'
#define kICCRedTRCTag           'rTRC'
#define kICCGreenTRCTag         'gTRC'
#define kICCBlueTRCTag          'bTRC'
#define kICCGrayTRCTag          'kTRC'
#define kICCDescriptionTag      'desc'
#define kICCCopyrightTag        'cprt'
#define kICCMediaWhitePointTag  'wtpt'
#define kICCChromaticityTag     'chrm'
#define kICCLuminanceTag        'lumi'
#define kICCMeasurementTag      'meas'
#define kICCTechnologyTag       'tech'
#define kICCViewingConditionsTag 'view'
#define kICCAToB0Tag            'A2B0'
#define kICCAToB1Tag            'A2B1'
#define kICCAToB2Tag            'A2B2'
#define kICCBToA0Tag            'B2A0'
#define kICCBToA1Tag            'B2A1'
#define kICCBToA2Tag            'B2A2'
#define kICCGamutTag            'gamt'
#define kICCPreview0Tag         'pre0'
#define kICCPreview1Tag         'pre1'
#define kICCPreview2Tag         'pre2'
#define kICCNamedColorTag       'ncol'
#define kICCNamedColor2Tag      'ncl2'

/* Type signatures */
#define kICCCurveType           'curv'
#define kICCXYZType             'XYZ '
#define kICCTextType            'text'
#define kICCDescriptionType     'desc'
#define kICCChromaticityType    'chrm'
#define kICCLut8Type            'mft1'
#define kICCLut16Type           'mft2'
#define kICCLutAToBType         'mAB '
#define kICCLutBToAType         'mBA '
#define kICCMeasurementType     'meas'
#define kICCNamedColorType      'ncol'
#define kICCNamedColor2Type     'ncl2'
#define kICCParametricCurveType 'para'
#define kICCSignatureType       'sig '
#define kICCViewingConditionsType 'view'

/* Profile flags */
#define kICCEmbeddedProfile     0x00000001
#define kICCIndependentProfile  0x00000002

/* Device attributes */
#define kICCReflectiveDevice    0x00000000
#define kICCTransparencyDevice  0x00000001
#define kICCGlossyDevice        0x00000000
#define kICCMatteDevice         0x00000002

/* ================================================================
 * ICC PROFILE STRUCTURES
 * ================================================================ */

/* ICC profile header (128 bytes) */
typedef struct {
    uint32_t    profileSize;        /* Profile size in bytes */
    uint32_t    preferredCMM;       /* Preferred CMM type */
    uint32_t    version;            /* Profile version */
    uint32_t    deviceClass;        /* Device class */
    uint32_t    dataColorSpace;     /* Data color space */
    uint32_t    pcs;                /* Profile connection space */
    uint8_t     dateTime[12];       /* Creation date and time */
    uint32_t    signature;          /* 'acsp' signature */
    uint32_t    platform;           /* Primary platform */
    uint32_t    flags;              /* Profile flags */
    uint32_t    deviceManufacturer; /* Device manufacturer */
    uint32_t    deviceModel;        /* Device model */
    uint64_t    deviceAttributes;   /* Device attributes */
    uint32_t    renderingIntent;    /* Rendering intent */
    int32_t     pcsIlluminant[3];   /* PCS illuminant XYZ */
    uint32_t    creator;            /* Profile creator */
    uint8_t     profileID[16];      /* Profile ID (MD5) */
    uint8_t     reserved[28];       /* Reserved for future use */
} CMICCHeader;

/* Tag table entry */
typedef struct {
    uint32_t    signature;          /* Tag signature */
    uint32_t    offset;             /* Offset to tag data */
    uint32_t    size;               /* Size of tag data */
} CMICCTagEntry;

/* Tag table */
typedef struct {
    uint32_t        tagCount;       /* Number of tags */
    CMICCTagEntry   tags[1];        /* Variable length array */
} CMICCTagTable;

/* XYZ data */
typedef struct {
    uint32_t    type;               /* 'XYZ ' */
    uint32_t    reserved;           /* Must be 0 */
    int32_t     X;                  /* X component */
    int32_t     Y;                  /* Y component */
    int32_t     Z;                  /* Z component */
} CMICCXYZData;

/* Curve data */
typedef struct {
    uint32_t    type;               /* 'curv' */
    uint32_t    reserved;           /* Must be 0 */
    uint32_t    pointCount;         /* Number of curve points */
    uint16_t    data[1];            /* Variable length curve data */
} CMICCCurveData;

/* Text data */
typedef struct {
    uint32_t    type;               /* 'text' */
    uint32_t    reserved;           /* Must be 0 */
    char        text[1];            /* Variable length text */
} CMICCTextData;

/* Description data */
typedef struct {
    uint32_t    type;               /* 'desc' */
    uint32_t    reserved;           /* Must be 0 */
    uint32_t    asciiLength;        /* ASCII description length */
    char        asciiData[1];       /* Variable length description */
    /* Additional Unicode and ScriptCode descriptions follow */
} CMICCDescriptionData;

/* Named color data */
typedef struct {
    uint32_t    type;               /* 'ncol' or 'ncl2' */
    uint32_t    reserved;           /* Must be 0 */
    uint32_t    vendorFlags;        /* Vendor specific flags */
    uint32_t    colorCount;         /* Number of named colors */
    uint32_t    coordinateCount;    /* Number of device coordinates */
    char        prefix[32];         /* Color name prefix */
    char        suffix[32];         /* Color name suffix */
    /* Named color entries follow */
} CMICCNamedColorData;

/* ================================================================
 * ICC PROFILE MANAGEMENT
 * ================================================================ */

/* Initialize ICC profile system */
CMError CMInitICCProfiles(void);

/* Create ICC profile structure */
CMError CMCreateICCProfile(CMProfileRef prof, CMProfileClass profileClass,
                          CMColorSpace dataSpace, CMColorSpace pcs);

/* Create default ICC profile */
CMError CMCreateDefaultICCProfile(CMProfileRef prof);

/* Load ICC profile from data */
CMError CMLoadICCProfileFromData(CMProfileRef prof, const void *data, uint32_t size);

/* Save ICC profile to data */
CMError CMSaveICCProfileToData(CMProfileRef prof, void **data, uint32_t *size);

/* Validate ICC profile */
CMError CMValidateICCProfile(const void *data, uint32_t size, bool *isValid);

/* ================================================================
 * ICC HEADER MANAGEMENT
 * ================================================================ */

/* Get ICC header */
CMError CMGetICCHeader(CMProfileRef prof, CMICCHeader *header);

/* Set ICC header */
CMError CMSetICCHeader(CMProfileRef prof, const CMICCHeader *header);

/* Update ICC header fields */
CMError CMUpdateICCHeader(CMProfileRef prof);

/* Get profile creation date */
CMError CMGetProfileCreationDate(CMProfileRef prof, void *dateTime);

/* Set profile creation date */
CMError CMSetProfileCreationDate(CMProfileRef prof, const void *dateTime);

/* ================================================================
 * ICC TAG MANAGEMENT
 * ================================================================ */

/* Get tag count */
CMError CMGetICCTagCount(CMProfileRef prof, uint32_t *count);

/* Get tag info by index */
CMError CMGetICCTagInfo(CMProfileRef prof, uint32_t index,
                       uint32_t *signature, uint32_t *size);

/* Get tag data */
CMError CMGetICCTagData(CMProfileRef prof, uint32_t signature,
                       void *data, uint32_t *size);

/* Set tag data */
CMError CMSetICCTagData(CMProfileRef prof, uint32_t signature,
                       const void *data, uint32_t size);

/* Remove tag */
CMError CMRemoveICCTag(CMProfileRef prof, uint32_t signature);

/* Check if tag exists */
bool CMICCTagExists(CMProfileRef prof, uint32_t signature);

/* ================================================================
 * STANDARD ICC TAGS
 * ================================================================ */

/* RGB colorant tags */
CMError CMGetRedColorant(CMProfileRef prof, CMXYZColor *xyz);
CMError CMSetRedColorant(CMProfileRef prof, const CMXYZColor *xyz);
CMError CMGetGreenColorant(CMProfileRef prof, CMXYZColor *xyz);
CMError CMSetGreenColorant(CMProfileRef prof, const CMXYZColor *xyz);
CMError CMGetBlueColorant(CMProfileRef prof, CMXYZColor *xyz);
CMError CMSetBlueColorant(CMProfileRef prof, const CMXYZColor *xyz);

/* White point */
CMError CMGetWhitePoint(CMProfileRef prof, CMXYZColor *whitePoint);
CMError CMSetWhitePoint(CMProfileRef prof, const CMXYZColor *whitePoint);

/* Tone reproduction curves */
CMError CMGetRedTRC(CMProfileRef prof, uint16_t *curve, uint32_t *count);
CMError CMSetRedTRC(CMProfileRef prof, const uint16_t *curve, uint32_t count);
CMError CMGetGreenTRC(CMProfileRef prof, uint16_t *curve, uint32_t *count);
CMError CMSetGreenTRC(CMProfileRef prof, const uint16_t *curve, uint32_t count);
CMError CMGetBlueTRC(CMProfileRef prof, uint16_t *curve, uint32_t *count);
CMError CMSetBlueTRC(CMProfileRef prof, const uint16_t *curve, uint32_t count);
CMError CMGetGrayTRC(CMProfileRef prof, uint16_t *curve, uint32_t *count);
CMError CMSetGrayTRC(CMProfileRef prof, const uint16_t *curve, uint32_t count);

/* Profile description */
CMError CMGetProfileDescription(CMProfileRef prof, char *description, uint32_t *size);
CMError CMSetProfileDescription(CMProfileRef prof, const char *description);

/* Copyright */
CMError CMGetProfileCopyright(CMProfileRef prof, char *copyright, uint32_t *size);
CMError CMSetProfileCopyright(CMProfileRef prof, const char *copyright);

/* ================================================================
 * ICC CURVE UTILITIES
 * ================================================================ */

/* Create gamma curve */
CMError CMCreateGammaCurve(float gamma, uint16_t **curve, uint32_t *count);

/* Create linear curve */
CMError CMCreateLinearCurve(uint16_t **curve, uint32_t *count);

/* Create sRGB curve */
CMError CMCreateSRGBCurve(uint16_t **curve, uint32_t *count);

/* Apply curve to value */
uint16_t CMApplyCurve(const uint16_t *curve, uint32_t count, uint16_t input);

/* Invert curve */
CMError CMInvertCurve(const uint16_t *inputCurve, uint32_t inputCount,
                     uint16_t **outputCurve, uint32_t *outputCount);

/* ================================================================
 * ICC PROFILE UTILITIES
 * ================================================================ */

/* Calculate profile MD5 */
CMError CMCalculateProfileMD5(CMProfileRef prof, uint8_t *digest);

/* Get profile size */
CMError CMGetICCProfileSize(CMProfileRef prof, uint32_t *size);

/* Clone ICC profile data */
CMError CMCloneICCProfileData(CMProfileRef srcProf, CMProfileRef dstProf);

/* Compare ICC profiles */
bool CMCompareICCProfiles(CMProfileRef prof1, CMProfileRef prof2);

/* ================================================================
 * STANDARD PROFILE CREATION
 * ================================================================ */

/* Create sRGB profile */
CMError CMCreateSRGBProfile(CMProfileRef *prof);

/* Create Adobe RGB profile */
CMError CMCreateAdobeRGBProfile(CMProfileRef *prof);

/* Create ProPhoto RGB profile */
CMError CMCreateProPhotoRGBProfile(CMProfileRef *prof);

/* Create Gray gamma profile */
CMError CMCreateGrayProfile(CMProfileRef *prof, float gamma);

/* Create D50 gray profile */
CMError CMCreateD50GrayProfile(CMProfileRef *prof);

/* Create generic CMYK profile */
CMError CMCreateGenericCMYKProfile(CMProfileRef *prof);

/* ================================================================
 * ICC VALIDATION AND REPAIR
 * ================================================================ */

/* Validate profile structure */
CMError CMValidateProfileStructure(CMProfileRef prof, bool *isValid);

/* Check profile completeness */
CMError CMCheckProfileCompleteness(CMProfileRef prof, bool *isComplete);

/* Repair profile */
CMError CMRepairProfile(CMProfileRef prof);

/* ================================================================
 * ICC VERSION COMPATIBILITY
 * ================================================================ */

/* Get ICC version */
CMError CMGetICCVersion(CMProfileRef prof, uint32_t *version);

/* Convert to ICC v2 */
CMError CMConvertToICCv2(CMProfileRef prof);

/* Convert to ICC v4 */
CMError CMConvertToICCv4(CMProfileRef prof);

/* Check ICC compatibility */
bool CMCheckICCCompatibility(CMProfileRef prof, uint32_t version);

/* ================================================================
 * ICC PLATFORM INTEGRATION
 * ================================================================ */

/* Get platform signature */
CMError CMGetPlatformSignature(uint32_t *platform);

/* Set platform signature */
CMError CMSetPlatformSignature(CMProfileRef prof, uint32_t platform);

/* Get CMM preferences */
CMError CMGetCMMPreferences(uint32_t *cmm);

/* Set CMM preferences */
CMError CMSetCMMPreferences(CMProfileRef prof, uint32_t cmm);

#ifdef __cplusplus
}
#endif

#endif /* ICCPROFILES_H */