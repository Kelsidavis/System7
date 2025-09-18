/*
 * ColorManagerCore.c - Core Color Manager Implementation
 *
 * Main Color Manager implementation providing professional color management
 * with ICC profiles, color space conversion, and device calibration.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager and ColorSync
 */

#include "../include/ColorManager/ColorManager.h"
#include "../include/ColorManager/ColorSpaces.h"
#include "../include/ColorManager/ICCProfiles.h"
#include "../include/ColorManager/ColorMatching.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

/* ================================================================
 * INTERNAL STRUCTURES
 * ================================================================ */

struct CMProfile {
    char                filename[256];
    void               *profileData;
    uint32_t           profileSize;
    CMProfileClass     profileClass;
    CMColorSpace       dataSpace;
    CMColorSpace       pcs;
    uint32_t           refCount;
    bool               isValid;
    bool               isEmbedded;
    uint32_t           AID;
    void              *header;
    void              *elementTable;
    uint32_t           elementCount;
};

struct CMWorldRef {
    CMProfileRef        srcProfile;
    CMProfileRef        dstProfile;
    CMProfileRef        proofProfile;
    CMRenderingIntent   intent;
    CMQuality          quality;
    uint32_t           flags;
    void              *transform;
    bool               isValid;
};

struct CMMatchRef {
    CMProfileRef        srcProfile;
    CMProfileRef        dstProfile;
    void              *matchData;
    bool               isActive;
};

/* ================================================================
 * GLOBAL STATE
 * ================================================================ */

static bool g_colorManagerInitialized = false;
static CMProfileRef g_systemProfile = NULL;
static CMProfileRef g_defaultProfiles[8] = {NULL}; /* One for each class */
static CMWorldRef g_currentWorld = NULL;
static uint32_t g_profileSeed = 1;
static uint32_t g_nextAID = 1000;

/* Standard illuminants */
static const CMXYZColor g_illuminantD65 = {95047, 100000, 108883};  /* D65 white point */
static const CMXYZColor g_illuminantD50 = {96422, 100000, 82521};   /* D50 white point */

/* Forward declarations */
static CMError LoadProfileFromFile(const char *filename, CMProfileRef prof);
static CMError ValidateProfileData(const void *data, uint32_t size);
static CMError CreateTransform(CMWorldRef cw);
static void DisposeTransform(CMWorldRef cw);
static uint32_t GenerateProfileAID(void);
static CMError SetProfileDefaults(CMProfileRef prof);

/* ================================================================
 * COLOR MANAGER INITIALIZATION
 * ================================================================ */

CMError CMInitColorManager(void) {
    if (g_colorManagerInitialized) {
        return cmNoError;
    }

    /* Initialize color spaces */
    CMError err = CMInitColorSpaces();
    if (err != cmNoError) return err;

    /* Initialize ICC profiles */
    err = CMInitICCProfiles();
    if (err != cmNoError) return err;

    /* Initialize color matching */
    err = CMInitColorMatching();
    if (err != cmNoError) return err;

    /* Create default sRGB profile */
    err = CMNewProfile(&g_systemProfile, cmDisplayClass, cmRGBSpace, cmXYZSpace);
    if (err != cmNoError) return err;

    /* Set up default profile as sRGB */
    err = SetProfileDefaults(g_systemProfile);
    if (err != cmNoError) return err;

    g_defaultProfiles[cmDisplayClass & 0x7] = g_systemProfile;

    g_colorManagerInitialized = true;
    return cmNoError;
}

uint32_t CMGetColorManagerVersion(void) {
    return kColorManagerVersion;
}

bool CMColorManagerAvailable(void) {
    return g_colorManagerInitialized;
}

/* ================================================================
 * PROFILE MANAGEMENT
 * ================================================================ */

CMError CMOpenProfile(CMProfileRef *prof, const char *filename) {
    if (!prof || !filename) return cmParameterError;
    if (!g_colorManagerInitialized) {
        CMError err = CMInitColorManager();
        if (err != cmNoError) return err;
    }

    /* Allocate profile structure */
    CMProfileRef newProf = (CMProfileRef)calloc(1, sizeof(struct CMProfile));
    if (!newProf) return cmProfileError;

    /* Load profile from file */
    CMError err = LoadProfileFromFile(filename, newProf);
    if (err != cmNoError) {
        free(newProf);
        return err;
    }

    newProf->refCount = 1;
    newProf->AID = GenerateProfileAID();
    *prof = newProf;

    return cmNoError;
}

CMError CMNewProfile(CMProfileRef *prof, CMProfileClass profileClass,
                     CMColorSpace dataSpace, CMColorSpace pcs) {
    if (!prof) return cmParameterError;
    if (!g_colorManagerInitialized) {
        CMError err = CMInitColorManager();
        if (err != cmNoError) return err;
    }

    /* Allocate profile structure */
    CMProfileRef newProf = (CMProfileRef)calloc(1, sizeof(struct CMProfile));
    if (!newProf) return cmProfileError;

    /* Initialize profile */
    newProf->profileClass = profileClass;
    newProf->dataSpace = dataSpace;
    newProf->pcs = pcs;
    newProf->refCount = 1;
    newProf->isValid = true;
    newProf->AID = GenerateProfileAID();

    /* Create basic ICC profile structure */
    CMError err = CMCreateICCProfile(newProf, profileClass, dataSpace, pcs);
    if (err != cmNoError) {
        free(newProf);
        return err;
    }

    *prof = newProf;
    return cmNoError;
}

CMError CMCloseProfile(CMProfileRef prof) {
    if (!prof) return cmParameterError;

    prof->refCount--;
    if (prof->refCount > 0) {
        return cmNoError;
    }

    /* Free profile data */
    if (prof->profileData) {
        free(prof->profileData);
    }
    if (prof->header) {
        free(prof->header);
    }
    if (prof->elementTable) {
        free(prof->elementTable);
    }

    /* Clear from default profiles */
    for (int i = 0; i < 8; i++) {
        if (g_defaultProfiles[i] == prof) {
            g_defaultProfiles[i] = NULL;
        }
    }

    if (g_systemProfile == prof) {
        g_systemProfile = NULL;
    }

    free(prof);
    return cmNoError;
}

CMError CMCopyProfile(CMProfileRef *dstProf, CMProfileRef srcProf) {
    if (!dstProf || !srcProf) return cmParameterError;

    /* Allocate new profile */
    CMProfileRef newProf = (CMProfileRef)calloc(1, sizeof(struct CMProfile));
    if (!newProf) return cmProfileError;

    /* Copy structure */
    *newProf = *srcProf;
    newProf->refCount = 1;
    newProf->AID = GenerateProfileAID();

    /* Deep copy profile data */
    if (srcProf->profileData && srcProf->profileSize > 0) {
        newProf->profileData = malloc(srcProf->profileSize);
        if (!newProf->profileData) {
            free(newProf);
            return cmProfileError;
        }
        memcpy(newProf->profileData, srcProf->profileData, srcProf->profileSize);
    }

    /* Copy header if present */
    if (srcProf->header) {
        newProf->header = malloc(128); /* ICC header size */
        if (newProf->header) {
            memcpy(newProf->header, srcProf->header, 128);
        }
    }

    *dstProf = newProf;
    return cmNoError;
}

CMError CMCloneProfileRef(CMProfileRef prof) {
    if (!prof) return cmParameterError;
    prof->refCount++;
    return cmNoError;
}

CMError CMGetProfileLocation(CMProfileRef prof, char *location, uint32_t *size) {
    if (!prof || !size) return cmParameterError;

    uint32_t len = strlen(prof->filename);
    if (location && *size > len) {
        strcpy(location, prof->filename);
    }
    *size = len + 1;
    return cmNoError;
}

/* ================================================================
 * COLOR SPACE MANAGEMENT
 * ================================================================ */

CMError CMGetProfileSpace(CMProfileRef prof, CMColorSpace *space) {
    if (!prof || !space) return cmParameterError;
    *space = prof->dataSpace;
    return cmNoError;
}

CMError CMGetProfilePCS(CMProfileRef prof, CMColorSpace *pcs) {
    if (!prof || !pcs) return cmParameterError;
    *pcs = prof->pcs;
    return cmNoError;
}

CMError CMGetProfileClass(CMProfileRef prof, CMProfileClass *profileClass) {
    if (!prof || !profileClass) return cmParameterError;
    *profileClass = prof->profileClass;
    return cmNoError;
}

CMError CMSetProfileClass(CMProfileRef prof, CMProfileClass profileClass) {
    if (!prof) return cmParameterError;
    prof->profileClass = profileClass;
    return cmNoError;
}

CMError CMGetColorSpaceName(CMColorSpace space, char *name, uint32_t maxLength) {
    if (!name || maxLength < 5) return cmParameterError;

    const char *spaceName;
    switch (space) {
        case cmGraySpace: spaceName = "Gray"; break;
        case cmRGBSpace:  spaceName = "RGB";  break;
        case cmCMYKSpace: spaceName = "CMYK"; break;
        case cmHSVSpace:  spaceName = "HSV";  break;
        case cmHLSSpace:  spaceName = "HLS";  break;
        case cmXYZSpace:  spaceName = "XYZ";  break;
        case cmLABSpace:  spaceName = "Lab";  break;
        case cmYCCSpace:  spaceName = "YCC";  break;
        default:          spaceName = "Unknown"; break;
    }

    strncpy(name, spaceName, maxLength - 1);
    name[maxLength - 1] = '\0';
    return cmNoError;
}

/* ================================================================
 * COLOR MATCHING WORLD
 * ================================================================ */

CMError CMBeginMatching(CMProfileRef src, CMProfileRef dst, CMMatchRef *myRef) {
    if (!src || !dst || !myRef) return cmParameterError;

    CMMatchRef match = (CMMatchRef)calloc(1, sizeof(struct CMMatchRef));
    if (!match) return cmMethodError;

    match->srcProfile = src;
    match->dstProfile = dst;
    match->isActive = true;

    /* Clone profile references */
    CMCloneProfileRef(src);
    CMCloneProfileRef(dst);

    *myRef = match;
    return cmNoError;
}

CMError CMEndMatching(CMMatchRef myRef) {
    if (!myRef) return cmParameterError;

    myRef->isActive = false;

    /* Release profile references */
    if (myRef->srcProfile) {
        CMCloseProfile(myRef->srcProfile);
    }
    if (myRef->dstProfile) {
        CMCloseProfile(myRef->dstProfile);
    }

    if (myRef->matchData) {
        free(myRef->matchData);
    }

    free(myRef);
    return cmNoError;
}

CMError CMEnableMatching(bool enableIt) {
    /* Global matching enable/disable */
    return cmNoError;
}

CMError CMCreateColorWorld(CMWorldRef *cw, CMProfileRef src, CMProfileRef dst,
                          CMProfileRef proof) {
    if (!cw || !src || !dst) return cmParameterError;

    CMWorldRef world = (CMWorldRef)calloc(1, sizeof(struct CMWorldRef));
    if (!world) return cmMethodError;

    world->srcProfile = src;
    world->dstProfile = dst;
    world->proofProfile = proof;
    world->intent = cmPerceptual;
    world->quality = cmNormalQuality;
    world->isValid = true;

    /* Clone profile references */
    CMCloneProfileRef(src);
    CMCloneProfileRef(dst);
    if (proof) CMCloneProfileRef(proof);

    /* Create color transform */
    CMError err = CreateTransform(world);
    if (err != cmNoError) {
        CMDisposeColorWorld(world);
        return err;
    }

    *cw = world;
    return cmNoError;
}

CMError CMDisposeColorWorld(CMWorldRef cw) {
    if (!cw) return cmParameterError;

    /* Dispose transform */
    DisposeTransform(cw);

    /* Release profile references */
    if (cw->srcProfile) CMCloseProfile(cw->srcProfile);
    if (cw->dstProfile) CMCloseProfile(cw->dstProfile);
    if (cw->proofProfile) CMCloseProfile(cw->proofProfile);

    if (g_currentWorld == cw) {
        g_currentWorld = NULL;
    }

    free(cw);
    return cmNoError;
}

CMError CMCloneColorWorld(CMWorldRef *clonedCW, CMWorldRef srcCW) {
    if (!clonedCW || !srcCW) return cmParameterError;

    return CMCreateColorWorld(clonedCW, srcCW->srcProfile, srcCW->dstProfile,
                             srcCW->proofProfile);
}

CMError CMUseColorWorld(CMWorldRef cw) {
    g_currentWorld = cw;
    return cmNoError;
}

/* ================================================================
 * COLOR CONVERSION
 * ================================================================ */

CMError CMMatchColors(CMWorldRef cw, CMColor *myColors, uint32_t count) {
    if (!cw || !myColors || count == 0) return cmParameterError;
    if (!cw->isValid) return cmInvalidProfileID;

    /* Use color matching engine */
    return CMPerformColorMatching(cw->srcProfile, cw->dstProfile,
                                 myColors, count, cw->intent);
}

CMError CMCheckColors(CMWorldRef cw, CMColor *myColors, uint32_t count,
                     bool *gamutResult) {
    if (!cw || !myColors || !gamutResult || count == 0) return cmParameterError;

    /* Check each color against destination gamut */
    for (uint32_t i = 0; i < count; i++) {
        CMError err = CMCheckColor(cw, &myColors[i], &gamutResult[i]);
        if (err != cmNoError) return err;
    }

    return cmNoError;
}

CMError CMMatchColor(CMWorldRef cw, CMColor *color) {
    if (!cw || !color) return cmParameterError;
    return CMMatchColors(cw, color, 1);
}

CMError CMCheckColor(CMWorldRef cw, const CMColor *color, bool *inGamut) {
    if (!cw || !color || !inGamut) return cmParameterError;

    /* Simplified gamut check - assume most colors are in gamut */
    *inGamut = true;

    /* Check if color is within reasonable bounds */
    switch (cw->dstProfile->dataSpace) {
        case cmRGBSpace:
            *inGamut = (color->rgb.red <= 65535 &&
                       color->rgb.green <= 65535 &&
                       color->rgb.blue <= 65535);
            break;
        case cmCMYKSpace:
            *inGamut = (color->cmyk.cyan <= 65535 &&
                       color->cmyk.magenta <= 65535 &&
                       color->cmyk.yellow <= 65535 &&
                       color->cmyk.black <= 65535);
            break;
        default:
            *inGamut = true;
            break;
    }

    return cmNoError;
}

/* ================================================================
 * XYZ/Lab CONVERSIONS
 * ================================================================ */

CMError CMConvertXYZToLab(const CMXYZColor *src, CMLABColor *dst,
                         const CMXYZColor *whitePoint) {
    if (!src || !dst) return cmParameterError;

    const CMXYZColor *wp = whitePoint ? whitePoint : &g_illuminantD65;

    /* Convert XYZ to Lab using standard formulas */
    double fx = (double)src->X / wp->X;
    double fy = (double)src->Y / wp->Y;
    double fz = (double)src->Z / wp->Z;

    /* Apply Lab transformation */
    if (fx > 0.008856) fx = pow(fx, 1.0/3.0);
    else fx = 7.787 * fx + 16.0/116.0;

    if (fy > 0.008856) fy = pow(fy, 1.0/3.0);
    else fy = 7.787 * fy + 16.0/116.0;

    if (fz > 0.008856) fz = pow(fz, 1.0/3.0);
    else fz = 7.787 * fz + 16.0/116.0;

    dst->L = (int32_t)(116.0 * fy - 16.0);
    dst->a = (int32_t)(500.0 * (fx - fy));
    dst->b = (int32_t)(200.0 * (fy - fz));

    return cmNoError;
}

CMError CMConvertLabToXYZ(const CMLABColor *src, CMXYZColor *dst,
                         const CMXYZColor *whitePoint) {
    if (!src || !dst) return cmParameterError;

    const CMXYZColor *wp = whitePoint ? whitePoint : &g_illuminantD65;

    /* Convert Lab to XYZ using standard formulas */
    double fy = ((double)src->L + 16.0) / 116.0;
    double fx = (double)src->a / 500.0 + fy;
    double fz = fy - (double)src->b / 200.0;

    /* Apply inverse transformation */
    double fx3 = fx * fx * fx;
    double fy3 = fy * fy * fy;
    double fz3 = fz * fz * fz;

    double xr = (fx3 > 0.008856) ? fx3 : (fx - 16.0/116.0) / 7.787;
    double yr = (fy3 > 0.008856) ? fy3 : (fy - 16.0/116.0) / 7.787;
    double zr = (fz3 > 0.008856) ? fz3 : (fz - 16.0/116.0) / 7.787;

    dst->X = (int32_t)(xr * wp->X);
    dst->Y = (int32_t)(yr * wp->Y);
    dst->Z = (int32_t)(zr * wp->Z);

    return cmNoError;
}

/* ================================================================
 * SYSTEM INTEGRATION
 * ================================================================ */

CMError CMGetSystemProfile(CMProfileRef *prof) {
    if (!prof) return cmParameterError;
    if (!g_systemProfile) return cmNoCurrentProfile;

    *prof = g_systemProfile;
    CMCloneProfileRef(g_systemProfile);
    return cmNoError;
}

CMError CMSetSystemProfile(const char *profileName) {
    if (!profileName) return cmParameterError;

    CMProfileRef newProfile;
    CMError err = CMOpenProfile(&newProfile, profileName);
    if (err != cmNoError) return err;

    if (g_systemProfile) {
        CMCloseProfile(g_systemProfile);
    }
    g_systemProfile = newProfile;

    return cmNoError;
}

CMError CMGetDefaultProfileByClass(CMProfileClass profileClass, CMProfileRef *prof) {
    if (!prof) return cmParameterError;

    uint32_t index = profileClass & 0x7;
    if (index >= 8 || !g_defaultProfiles[index]) {
        return cmNoCurrentProfile;
    }

    *prof = g_defaultProfiles[index];
    CMCloneProfileRef(g_defaultProfiles[index]);
    return cmNoError;
}

CMError CMSetDefaultProfileByClass(CMProfileClass profileClass, CMProfileRef prof) {
    if (!prof) return cmParameterError;

    uint32_t index = profileClass & 0x7;
    if (index >= 8) return cmParameterError;

    if (g_defaultProfiles[index]) {
        CMCloseProfile(g_defaultProfiles[index]);
    }

    g_defaultProfiles[index] = prof;
    CMCloneProfileRef(prof);
    return cmNoError;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static CMError LoadProfileFromFile(const char *filename, CMProfileRef prof) {
    FILE *file = fopen(filename, "rb");
    if (!file) return cmProfileError;

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0 || size > 10*1024*1024) { /* Max 10MB */
        fclose(file);
        return cmProfileError;
    }

    /* Allocate and read profile data */
    prof->profileData = malloc(size);
    if (!prof->profileData) {
        fclose(file);
        return cmProfileError;
    }

    if (fread(prof->profileData, 1, size, file) != size) {
        fclose(file);
        free(prof->profileData);
        prof->profileData = NULL;
        return cmProfileError;
    }

    fclose(file);
    prof->profileSize = size;

    /* Validate profile */
    CMError err = ValidateProfileData(prof->profileData, size);
    if (err != cmNoError) {
        free(prof->profileData);
        prof->profileData = NULL;
        return err;
    }

    /* Store filename */
    strncpy(prof->filename, filename, sizeof(prof->filename) - 1);
    prof->filename[sizeof(prof->filename) - 1] = '\0';

    prof->isValid = true;
    return cmNoError;
}

static CMError ValidateProfileData(const void *data, uint32_t size) {
    if (!data || size < 128) return cmFatalProfileErr;

    /* Basic ICC header validation */
    const uint8_t *header = (const uint8_t *)data;

    /* Check profile size field */
    uint32_t profileSize = (header[0] << 24) | (header[1] << 16) |
                          (header[2] << 8) | header[3];
    if (profileSize != size) return cmFatalProfileErr;

    /* Check CMM type signature */
    /* Check version number */
    /* Check device class */
    /* Check data color space */
    /* Check PCS */

    return cmNoError;
}

static CMError CreateTransform(CMWorldRef cw) {
    if (!cw) return cmParameterError;

    /* Create transform based on profiles */
    cw->transform = CMCreateColorTransform(cw->srcProfile, cw->dstProfile,
                                          cw->intent, cw->quality);
    return cw->transform ? cmNoError : cmMethodError;
}

static void DisposeTransform(CMWorldRef cw) {
    if (cw && cw->transform) {
        CMDisposeColorTransform(cw->transform);
        cw->transform = NULL;
    }
}

static uint32_t GenerateProfileAID(void) {
    return g_nextAID++;
}

static CMError SetProfileDefaults(CMProfileRef prof) {
    if (!prof) return cmParameterError;

    /* Set up a basic sRGB profile */
    prof->profileClass = cmDisplayClass;
    prof->dataSpace = cmRGBSpace;
    prof->pcs = cmXYZSpace;

    /* Create minimal ICC structure */
    return CMCreateDefaultICCProfile(prof);
}

/* ================================================================
 * RENDERING INTENTS AND QUALITY
 * ================================================================ */

CMError CMSetRenderingIntent(CMWorldRef cw, CMRenderingIntent intent) {
    if (!cw) return cmParameterError;
    cw->intent = intent;
    /* Recreate transform if needed */
    return cmNoError;
}

CMError CMGetRenderingIntent(CMWorldRef cw, CMRenderingIntent *intent) {
    if (!cw || !intent) return cmParameterError;
    *intent = cw->intent;
    return cmNoError;
}

CMError CMSetQuality(CMWorldRef cw, CMQuality quality) {
    if (!cw) return cmParameterError;
    cw->quality = quality;
    return cmNoError;
}

CMError CMGetQuality(CMWorldRef cw, CMQuality *quality) {
    if (!cw || !quality) return cmParameterError;
    *quality = cw->quality;
    return cmNoError;
}

/* ================================================================
 * VALIDATION
 * ================================================================ */

CMError CMValidateProfile(CMProfileRef prof, bool *valid, bool *preferred) {
    if (!prof || !valid) return cmParameterError;

    *valid = prof->isValid;
    if (preferred) {
        *preferred = (prof == g_systemProfile);
    }

    return cmNoError;
}

CMError CMGetProfileDescription(CMProfileRef prof, char *name, uint32_t *size) {
    if (!prof || !size) return cmParameterError;

    const char *desc = "Color Profile";
    uint32_t len = strlen(desc);

    if (name && *size > len) {
        strcpy(name, desc);
    }
    *size = len + 1;

    return cmNoError;
}