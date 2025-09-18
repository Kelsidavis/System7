/*
 * ColorManager.h - Main Color Manager API
 *
 * Professional color management interface providing ICC profiles, color space
 * conversion, color matching, and device calibration for System 7.1 Portable.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager and ColorSync
 */

#ifndef COLORMANAGER_H
#define COLORMANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * COLOR MANAGER TYPES AND CONSTANTS
 * ================================================================ */

/* Color Manager version */
#define kColorManagerVersion    0x0200

/* Error codes */
typedef enum {
    cmNoError                   = 0,
    cmProfileError             = -170,  /* Profile not found or invalid */
    cmMethodError              = -171,  /* Color matching method not found */
    cmRangeError               = -172,  /* Color value out of range */
    cmNamedColorNotFound       = -173,  /* Named color not found */
    cmParameterError           = -174,  /* Invalid parameter */
    cmElementTagNotFound       = -175,  /* Element tag not found */
    cmElementTypeNotFound      = -176,  /* Element type not found */
    cmCantDeleteElement        = -177,  /* Can't delete required element */
    cmFatalProfileErr          = -178,  /* Fatal profile error */
    cmInvalidProfileID         = -179,  /* Invalid profile ID */
    cmInvalidLinkID            = -180,  /* Invalid link ID */
    cmCantConcatenateError     = -181,  /* Can't concatenate profiles */
    cmCantXYZ                  = -182,  /* Can't access XYZ */
    cmCantDeleteProfile        = -183,  /* Can't delete profile */
    cmUnsupportedDataType      = -184,  /* Unsupported data type */
    cmNoCurrentProfile         = -185,  /* No current profile */
    cmBufferTooSmall           = -186,  /* Buffer too small */
    cmXformStackUnderflow      = -187,  /* Transform stack underflow */
    cmXformStackOverflow       = -188,  /* Transform stack overflow */
    cmNamedColorNotDefined     = -189   /* Named color not defined */
} CMError;

/* Profile flags */
typedef enum {
    cmPerceptualRenderingFlag   = 0x0001,
    cmRelativeRenderingFlag     = 0x0002,
    cmSaturationRenderingFlag   = 0x0004,
    cmAbsoluteRenderingFlag     = 0x0008,
    cmNormalRenderingFlag       = 0x0010,
    cmUseDisplayCompensation    = 0x0020,
    cmUseProofingIntent         = 0x0040,
    cmBlackPointCompensation    = 0x0080
} CMProfileFlags;

/* Color space types */
typedef enum {
    cmGraySpace     = 'GRAY',
    cmRGBSpace      = 'RGB ',
    cmCMYKSpace     = 'CMYK',
    cmHSVSpace      = 'HSV ',
    cmHLSSpace      = 'HLS ',
    cmYxySpace      = 'Yxy ',
    cmXYZSpace      = 'XYZ ',
    cmLUVSpace      = 'Luv ',
    cmLABSpace      = 'Lab ',
    cmYCCSpace      = 'YCC ',
    cmYIQSpace      = 'YIQ ',
    cmHiFiSpace     = 'HiFi',
    cmNamedSpace    = 'name'
} CMColorSpace;

/* Rendering intents */
typedef enum {
    cmPerceptual    = 0,
    cmRelative      = 1,
    cmSaturation    = 2,
    cmAbsolute      = 3
} CMRenderingIntent;

/* Quality levels */
typedef enum {
    cmDraftQuality      = 0,
    cmNormalQuality     = 1,
    cmBestQuality       = 2
} CMQuality;

/* Profile types */
typedef enum {
    cmInputClass        = 'scnr',
    cmDisplayClass      = 'mntr',
    cmOutputClass       = 'prtr',
    cmLinkClass         = 'link',
    cmAbstractClass     = 'abst',
    cmColorSpaceClass   = 'spac',
    cmNamedColorClass   = 'nmcl'
} CMProfileClass;

/* Basic color structures */
typedef struct {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
} CMRGBColor;

typedef struct {
    uint16_t cyan;
    uint16_t magenta;
    uint16_t yellow;
    uint16_t black;
} CMCMYKColor;

typedef struct {
    uint16_t hue;
    uint16_t saturation;
    uint16_t value;
} CMHSVColor;

typedef struct {
    uint16_t hue;
    uint16_t saturation;
    uint16_t lightness;
} CMHLSColor;

typedef struct {
    int32_t X;
    int32_t Y;
    int32_t Z;
} CMXYZColor;

typedef struct {
    int32_t L;
    int32_t a;
    int32_t b;
} CMLABColor;

/* Generic color value */
typedef union {
    CMRGBColor      rgb;
    CMCMYKColor     cmyk;
    CMHSVColor      hsv;
    CMHLSColor      hls;
    CMXYZColor      xyz;
    CMLABColor      lab;
    uint8_t         gray;
    uint8_t         data[16];  /* Generic data for any color space */
} CMColor;

/* Profile and world handles */
typedef struct CMProfile*       CMProfileRef;
typedef struct CMWorldRef*      CMWorldRef;
typedef struct CMMatchRef*      CMMatchRef;

/* Iteration callback function */
typedef bool (*CMProfileIterateUPP)(CMProfileRef prof, void *refCon);

/* Progress callback function */
typedef bool (*CMFlattenUPP)(int32_t command, int32_t *size, void *data, void *refCon);

/* ================================================================
 * COLOR MANAGER INITIALIZATION
 * ================================================================ */

/* Initialize Color Manager */
CMError CMInitColorManager(void);

/* Get Color Manager version */
uint32_t CMGetColorManagerVersion(void);

/* Check if Color Manager is available */
bool CMColorManagerAvailable(void);

/* ================================================================
 * PROFILE MANAGEMENT
 * ================================================================ */

/* Open profile from file */
CMError CMOpenProfile(CMProfileRef *prof, const char *filename);

/* Create new profile */
CMError CMNewProfile(CMProfileRef *prof, CMProfileClass profileClass,
                     CMColorSpace dataSpace, CMColorSpace pcs);

/* Close profile */
CMError CMCloseProfile(CMProfileRef prof);

/* Copy profile */
CMError CMCopyProfile(CMProfileRef *dstProf, CMProfileRef srcProf);

/* Clone profile */
CMError CMCloneProfileRef(CMProfileRef prof);

/* Get profile location */
CMError CMGetProfileLocation(CMProfileRef prof, char *location, uint32_t *size);

/* Flatten profile to memory */
CMError CMFlattenProfile(CMProfileRef prof, uint32_t flags,
                        CMFlattenUPP proc, void *refCon);

/* Update profile */
CMError CMUpdateProfile(CMProfileRef prof);

/* Get profile header */
CMError CMGetProfileHeader(CMProfileRef prof, void *header);

/* Set profile header */
CMError CMSetProfileHeader(CMProfileRef prof, const void *header);

/* Get profile element */
CMError CMGetProfileElement(CMProfileRef prof, uint32_t tag,
                           uint32_t *elementSize, void *elementData);

/* Set profile element */
CMError CMSetProfileElement(CMProfileRef prof, uint32_t tag,
                           uint32_t elementSize, const void *elementData);

/* Count profile elements */
CMError CMCountProfileElements(CMProfileRef prof, uint32_t *elementCount);

/* Get profile element info */
CMError CMGetProfileElementInfo(CMProfileRef prof, uint32_t index,
                               uint32_t *tag, uint32_t *elementSize, bool *refs);

/* Remove profile element */
CMError CMRemoveProfileElement(CMProfileRef prof, uint32_t tag);

/* ================================================================
 * COLOR SPACE MANAGEMENT
 * ================================================================ */

/* Get profile color space */
CMError CMGetProfileSpace(CMProfileRef prof, CMColorSpace *space);

/* Get profile connection space */
CMError CMGetProfilePCS(CMProfileRef prof, CMColorSpace *pcs);

/* Get profile class */
CMError CMGetProfileClass(CMProfileRef prof, CMProfileClass *profileClass);

/* Set profile class */
CMError CMSetProfileClass(CMProfileRef prof, CMProfileClass profileClass);

/* Get color space name */
CMError CMGetColorSpaceName(CMColorSpace space, char *name, uint32_t maxLength);

/* ================================================================
 * COLOR MATCHING WORLD
 * ================================================================ */

/* Begin color matching session */
CMError CMBeginMatching(CMProfileRef src, CMProfileRef dst, CMMatchRef *myRef);

/* End color matching session */
CMError CMEndMatching(CMMatchRef myRef);

/* Enable matching */
CMError CMEnableMatching(bool enableIt);

/* Create color world */
CMError CMCreateColorWorld(CMWorldRef *cw, CMProfileRef src, CMProfileRef dst,
                          CMProfileRef proof);

/* Dispose color world */
CMError CMDisposeColorWorld(CMWorldRef cw);

/* Clone color world */
CMError CMCloneColorWorld(CMWorldRef *clonedCW, CMWorldRef srcCW);

/* Get color world info */
CMError CMGetColorWorldInfo(CMWorldRef cw, uint32_t *info);

/* Use color world */
CMError CMUseColorWorld(CMWorldRef cw);

/* ================================================================
 * COLOR CONVERSION
 * ================================================================ */

/* Match colors using color world */
CMError CMMatchColors(CMWorldRef cw, CMColor *myColors, uint32_t count);

/* Check colors */
CMError CMCheckColors(CMWorldRef cw, CMColor *myColors, uint32_t count,
                     bool *gamutResult);

/* Match single color */
CMError CMMatchColor(CMWorldRef cw, CMColor *color);

/* Check single color gamut */
CMError CMCheckColor(CMWorldRef cw, const CMColor *color, bool *inGamut);

/* Convert colors between color spaces */
CMError CMConvertXYZToLab(const CMXYZColor *src, CMLABColor *dst,
                         const CMXYZColor *whitePoint);

CMError CMConvertLabToXYZ(const CMLABColor *src, CMXYZColor *dst,
                         const CMXYZColor *whitePoint);

CMError CMConvertXYZToRGB(const CMXYZColor *src, CMRGBColor *dst,
                         CMProfileRef profile);

CMError CMConvertRGBToXYZ(const CMRGBColor *src, CMXYZColor *dst,
                         CMProfileRef profile);

/* ================================================================
 * DEVICE LINK PROFILES
 * ================================================================ */

/* Concatenate profiles */
CMError CMConcatenateProfiles(CMProfileRef thru, CMProfileRef dst,
                             CMProfileRef *newDst);

/* Create multi-profile link */
CMError CMCreateMultiProfileLink(CMProfileRef *profiles, uint32_t nProfiles,
                                CMProfileRef *multiProf);

/* ================================================================
 * NAMED COLORS
 * ================================================================ */

/* Get named color count */
CMError CMGetNamedColorCount(CMProfileRef prof, uint32_t *count);

/* Get named color info */
CMError CMGetNamedColorInfo(CMProfileRef prof, uint32_t index,
                           CMColor *color, char *name, uint32_t maxNameLength);

/* Get named color value */
CMError CMGetNamedColorValue(CMProfileRef prof, const char *name, CMColor *color);

/* ================================================================
 * COLOR PICKER INTEGRATION
 * ================================================================ */

/* Get color */
bool CMGetColor(int16_t where_h, int16_t where_v, const char *prompt,
               const CMRGBColor *inColor, CMRGBColor *outColor);

/* Color picker dialog */
CMError CMPickColor(CMRGBColor *color, const char *prompt);

/* ================================================================
 * SYSTEM INTEGRATION
 * ================================================================ */

/* Get system profile */
CMError CMGetSystemProfile(CMProfileRef *prof);

/* Set system profile */
CMError CMSetSystemProfile(const char *profileName);

/* Get default profile for device class */
CMError CMGetDefaultProfileByClass(CMProfileClass profileClass, CMProfileRef *prof);

/* Set default profile for device class */
CMError CMSetDefaultProfileByClass(CMProfileClass profileClass, CMProfileRef prof);

/* Iterate through profiles */
CMError CMIterateColorProfiles(CMProfileIterateUPP proc, uint32_t *seed,
                              uint32_t *count, void *refCon);

/* Search for profiles */
CMError CMSearchForProfiles(void *searchSpec, CMProfileIterateUPP proc,
                           uint32_t *seed, uint32_t *count, void *refCon);

/* Get profile by AID */
CMError CMGetProfileByAID(uint32_t AID, CMProfileRef *prof);

/* Get profile AID */
CMError CMGetProfileAID(CMProfileRef prof, uint32_t *AID);

/* ================================================================
 * RENDERING INTENTS AND QUALITY
 * ================================================================ */

/* Set rendering intent */
CMError CMSetRenderingIntent(CMWorldRef cw, CMRenderingIntent intent);

/* Get rendering intent */
CMError CMGetRenderingIntent(CMWorldRef cw, CMRenderingIntent *intent);

/* Set quality */
CMError CMSetQuality(CMWorldRef cw, CMQuality quality);

/* Get quality */
CMError CMGetQuality(CMWorldRef cw, CMQuality *quality);

/* ================================================================
 * GAMUT CHECKING
 * ================================================================ */

/* Create gamut */
CMError CMCreateGamut(CMProfileRef prof, void **gamut);

/* Check point in gamut */
CMError CMGamutCheckColor(void *gamut, const CMColor *color, bool *inGamut);

/* ================================================================
 * VALIDATION
 * ================================================================ */

/* Validate profile */
CMError CMValidateProfile(CMProfileRef prof, bool *valid, bool *preferred);

/* Get profile description */
CMError CMGetProfileDescription(CMProfileRef prof, char *name, uint32_t *size);

/* Get profile MD5 */
CMError CMGetProfileMD5(CMProfileRef prof, uint8_t *digest);

#ifdef __cplusplus
}
#endif

#endif /* COLORMANAGER_H */