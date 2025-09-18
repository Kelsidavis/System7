/*
 * FontSubstitution.c - Font Fallback and Substitution System
 *
 * Handles font substitution when requested fonts are not available,
 * provides fallback mechanisms, and manages font mapping tables.
 */

#include "../include/FontManager/FontManager.h"
#include "../include/FontManager/BitmapFonts.h"
#include "../include/FontManager/TrueTypeFonts.h"
#include <Memory.h>
#include <Errors.h>
#include <string.h>
#include <stdlib.h>

/* Maximum number of substitution entries */
#define MAX_SUBSTITUTIONS 128
#define MAX_FALLBACK_CHAIN 8

/* Font substitution table */
static FontSubstitution gSubstitutionTable[MAX_SUBSTITUTIONS];
static short gSubstitutionCount = 0;

/* Default fallback chain for different font categories */
static struct {
    short originalID;
    short fallbacks[MAX_FALLBACK_CHAIN];
    short count;
} gDefaultFallbacks[] = {
    /* Serif fonts */
    { times, { times, newYork, geneva, systemFont, 0, 0, 0, 0 }, 4 },
    { newYork, { newYork, times, geneva, systemFont, 0, 0, 0, 0 }, 4 },

    /* Sans-serif fonts */
    { helvetica, { helvetica, geneva, systemFont, applFont, 0, 0, 0, 0 }, 4 },
    { geneva, { geneva, helvetica, systemFont, applFont, 0, 0, 0, 0 }, 4 },

    /* Monospace fonts */
    { courier, { courier, monaco, geneva, systemFont, 0, 0, 0, 0 }, 4 },
    { monaco, { monaco, courier, geneva, systemFont, 0, 0, 0, 0 }, 4 },

    /* System fonts */
    { systemFont, { systemFont, geneva, applFont, helvetica, 0, 0, 0, 0 }, 4 },
    { applFont, { applFont, geneva, systemFont, helvetica, 0, 0, 0, 0 }, 4 },

    /* Decorative fonts */
    { venice, { venice, geneva, systemFont, 0, 0, 0, 0, 0 }, 3 },
    { london, { london, geneva, systemFont, 0, 0, 0, 0, 0 }, 3 },
    { athens, { athens, geneva, systemFont, 0, 0, 0, 0, 0 }, 3 },
    { sanFran, { sanFran, geneva, systemFont, 0, 0, 0, 0, 0 }, 3 },
    { toronto, { toronto, geneva, systemFont, 0, 0, 0, 0, 0 }, 3 },
    { cairo, { cairo, geneva, systemFont, 0, 0, 0, 0, 0 }, 3 },
    { losAngeles, { losAngeles, geneva, systemFont, 0, 0, 0, 0, 0 }, 3 },
    { symbol, { symbol, geneva, systemFont, 0, 0, 0, 0, 0 }, 3 },
    { mobile, { mobile, geneva, systemFont, 0, 0, 0, 0, 0 }, 3 }
};

#define DEFAULT_FALLBACK_COUNT (sizeof(gDefaultFallbacks) / sizeof(gDefaultFallbacks[0]))

/* Font classification for better substitution */
typedef enum {
    kFontClassSerifProportional,
    kFontClassSansSerifProportional,
    kFontClassMonospace,
    kFontClassDecorative,
    kFontClassSymbol,
    kFontClassSystem,
    kFontClassUnknown
} FontClass;

/* Internal helper functions */
static OSErr FindSubstitutionEntry(short originalID, short *index);
static OSErr RemoveSubstitutionEntry(short index);
static FontClass ClassifyFont(short familyID);
static OSErr GetFallbackChain(short originalID, short *fallbacks, short *count);
static OSErr FindBestSubstitute(short originalID, short size, short style, short *substituteID);
static Boolean IsFontAvailable(short familyID, short size, short style);
static OSErr AnalyzeFontSimilarity(short font1ID, short font2ID, short *similarity);
static OSErr GetDefaultSubstitute(FontClass fontClass, short *substituteID);

/*
 * SetFontSubstitution - Set up font substitution mapping
 */
OSErr SetFontSubstitution(short originalID, short substituteID)
{
    short index;
    OSErr error;

    /* Check if substitution already exists */
    error = FindSubstitutionEntry(originalID, &index);
    if (error == noErr) {
        /* Update existing entry */
        gSubstitutionTable[index].substituteID = substituteID;
        GetFontName(substituteID, gSubstitutionTable[index].substituteName);
        return noErr;
    }

    /* Check if table is full */
    if (gSubstitutionCount >= MAX_SUBSTITUTIONS) {
        return fontCacheFullErr;
    }

    /* Add new substitution */
    index = gSubstitutionCount;
    gSubstitutionTable[index].originalID = originalID;
    gSubstitutionTable[index].substituteID = substituteID;
    GetFontName(originalID, gSubstitutionTable[index].originalName);
    GetFontName(substituteID, gSubstitutionTable[index].substituteName);

    gSubstitutionCount++;
    return noErr;
}

/*
 * GetFontSubstitution - Get substitution for a font
 */
OSErr GetFontSubstitution(short originalID, short *substituteID)
{
    short index;
    OSErr error;

    if (substituteID == NULL) {
        return paramErr;
    }

    *substituteID = originalID; /* Default to no substitution */

    /* Check explicit substitution table */
    error = FindSubstitutionEntry(originalID, &index);
    if (error == noErr) {
        *substituteID = gSubstitutionTable[index].substituteID;
        return noErr;
    }

    return fontNotFoundErr;
}

/*
 * RemoveFontSubstitution - Remove a font substitution
 */
OSErr RemoveFontSubstitution(short originalID)
{
    short index;
    OSErr error;

    error = FindSubstitutionEntry(originalID, &index);
    if (error != noErr) {
        return fontNotFoundErr;
    }

    return RemoveSubstitutionEntry(index);
}

/*
 * ClearFontSubstitutions - Remove all font substitutions
 */
OSErr ClearFontSubstitutions(void)
{
    gSubstitutionCount = 0;
    memset(gSubstitutionTable, 0, sizeof(gSubstitutionTable));
    return noErr;
}

/*
 * FindFontSubstitute - Find best substitute for unavailable font
 */
OSErr FindFontSubstitute(short originalID, short size, short style, short *substituteID)
{
    OSErr error;
    short fallbacks[MAX_FALLBACK_CHAIN];
    short fallbackCount;
    short i;

    if (substituteID == NULL) {
        return paramErr;
    }

    *substituteID = originalID;

    /* First check if original font is available */
    if (IsFontAvailable(originalID, size, style)) {
        return noErr;
    }

    /* Check explicit substitution table */
    error = GetFontSubstitution(originalID, substituteID);
    if (error == noErr && IsFontAvailable(*substituteID, size, style)) {
        return noErr;
    }

    /* Try fallback chain */
    error = GetFallbackChain(originalID, fallbacks, &fallbackCount);
    if (error == noErr) {
        for (i = 0; i < fallbackCount; i++) {
            if (IsFontAvailable(fallbacks[i], size, style)) {
                *substituteID = fallbacks[i];
                return noErr;
            }
        }
    }

    /* Try best substitute based on font classification */
    error = FindBestSubstitute(originalID, size, style, substituteID);
    if (error == noErr) {
        return noErr;
    }

    /* Last resort - use system font */
    *substituteID = systemFont;
    return noErr;
}

/*
 * GetFontFallbackChain - Get complete fallback chain for a font
 */
OSErr GetFontFallbackChain(short originalID, short **fallbackChain, short *count)
{
    OSErr error;
    short *chain;
    short chainCount;

    if (fallbackChain == NULL || count == NULL) {
        return paramErr;
    }

    *fallbackChain = NULL;
    *count = 0;

    /* Allocate chain array */
    chain = (short *)NewPtr(MAX_FALLBACK_CHAIN * sizeof(short));
    if (chain == NULL) {
        return fontOutOfMemoryErr;
    }

    error = GetFallbackChain(originalID, chain, &chainCount);
    if (error != noErr) {
        DisposePtr((Ptr)chain);
        return error;
    }

    *fallbackChain = chain;
    *count = chainCount;
    return noErr;
}

/*
 * SetupDefaultSubstitutions - Set up default font substitutions
 */
OSErr SetupDefaultSubstitutions(void)
{
    /* Clear existing substitutions */
    ClearFontSubstitutions();

    /* Set up common substitutions for missing fonts */
    SetFontSubstitution(times, newYork);
    SetFontSubstitution(helvetica, geneva);
    SetFontSubstitution(courier, monaco);

    /* International font substitutions */
    SetFontSubstitution(24, geneva);     /* Palatino -> Geneva */
    SetFontSubstitution(25, times);      /* Bookman -> Times */
    SetFontSubstitution(26, helvetica);  /* Avant Garde -> Helvetica */

    return noErr;
}

/*
 * ValidateSubstitutionChain - Validate that substitution won't create loops
 */
OSErr ValidateSubstitutionChain(short originalID, short substituteID)
{
    short currentID = substituteID;
    short visited[MAX_FALLBACK_CHAIN];
    short visitCount = 0;
    short i;

    /* Check for direct loop */
    if (originalID == substituteID) {
        return fontCorruptErr;
    }

    /* Follow substitution chain to check for loops */
    while (visitCount < MAX_FALLBACK_CHAIN) {
        /* Check if we've seen this ID before */
        for (i = 0; i < visitCount; i++) {
            if (visited[i] == currentID) {
                return fontCorruptErr; /* Loop detected */
            }
        }

        visited[visitCount++] = currentID;

        /* Get next substitute */
        OSErr error = GetFontSubstitution(currentID, &currentID);
        if (error != noErr || currentID == originalID) {
            /* End of chain or back to original */
            if (currentID == originalID) {
                return fontCorruptErr; /* Loop detected */
            }
            break;
        }
    }

    return noErr;
}

/*
 * GetSubstitutionStatistics - Get substitution usage statistics
 */
OSErr GetSubstitutionStatistics(short *totalSubstitutions, short *activeSubstitutions,
                               short *mostUsedOriginal, short *mostUsedSubstitute)
{
    short i;
    short activeCount = 0;

    if (totalSubstitutions == NULL || activeSubstitutions == NULL ||
        mostUsedOriginal == NULL || mostUsedSubstitute == NULL) {
        return paramErr;
    }

    *totalSubstitutions = gSubstitutionCount;
    *mostUsedOriginal = 0;
    *mostUsedSubstitute = 0;

    /* Count active substitutions */
    for (i = 0; i < gSubstitutionCount; i++) {
        if (gSubstitutionTable[i].originalID != 0) {
            activeCount++;
        }
    }

    *activeSubstitutions = activeCount;

    /* Find most common substitutions - simplified implementation */
    if (gSubstitutionCount > 0) {
        *mostUsedOriginal = gSubstitutionTable[0].originalID;
        *mostUsedSubstitute = gSubstitutionTable[0].substituteID;
    }

    return noErr;
}

/* Internal helper function implementations */

static OSErr FindSubstitutionEntry(short originalID, short *index)
{
    short i;

    if (index == NULL) {
        return paramErr;
    }

    for (i = 0; i < gSubstitutionCount; i++) {
        if (gSubstitutionTable[i].originalID == originalID) {
            *index = i;
            return noErr;
        }
    }

    return fontNotFoundErr;
}

static OSErr RemoveSubstitutionEntry(short index)
{
    short i;

    if (index < 0 || index >= gSubstitutionCount) {
        return paramErr;
    }

    /* Shift remaining entries down */
    for (i = index; i < gSubstitutionCount - 1; i++) {
        gSubstitutionTable[i] = gSubstitutionTable[i + 1];
    }

    /* Clear last entry */
    memset(&gSubstitutionTable[gSubstitutionCount - 1], 0, sizeof(FontSubstitution));
    gSubstitutionCount--;

    return noErr;
}

static FontClass ClassifyFont(short familyID)
{
    switch (familyID) {
        case times:
        case newYork:
            return kFontClassSerifProportional;

        case helvetica:
        case geneva:
        case systemFont:
        case applFont:
            return kFontClassSansSerifProportional;

        case courier:
        case monaco:
            return kFontClassMonospace;

        case symbol:
            return kFontClassSymbol;

        case venice:
        case london:
        case athens:
        case sanFran:
        case toronto:
        case cairo:
        case losAngeles:
        case mobile:
            return kFontClassDecorative;

        default:
            return kFontClassUnknown;
    }
}

static OSErr GetFallbackChain(short originalID, short *fallbacks, short *count)
{
    short i, j;
    FontClass fontClass;

    if (fallbacks == NULL || count == NULL) {
        return paramErr;
    }

    *count = 0;

    /* Look for explicit fallback chain */
    for (i = 0; i < DEFAULT_FALLBACK_COUNT; i++) {
        if (gDefaultFallbacks[i].originalID == originalID) {
            for (j = 0; j < gDefaultFallbacks[i].count && j < MAX_FALLBACK_CHAIN; j++) {
                fallbacks[j] = gDefaultFallbacks[i].fallbacks[j];
            }
            *count = gDefaultFallbacks[i].count;
            return noErr;
        }
    }

    /* Create fallback chain based on font class */
    fontClass = ClassifyFont(originalID);
    fallbacks[(*count)++] = originalID; /* Include original */

    switch (fontClass) {
        case kFontClassSerifProportional:
            fallbacks[(*count)++] = times;
            fallbacks[(*count)++] = newYork;
            fallbacks[(*count)++] = geneva;
            fallbacks[(*count)++] = systemFont;
            break;

        case kFontClassSansSerifProportional:
            fallbacks[(*count)++] = geneva;
            fallbacks[(*count)++] = helvetica;
            fallbacks[(*count)++] = systemFont;
            fallbacks[(*count)++] = applFont;
            break;

        case kFontClassMonospace:
            fallbacks[(*count)++] = monaco;
            fallbacks[(*count)++] = courier;
            fallbacks[(*count)++] = geneva;
            fallbacks[(*count)++] = systemFont;
            break;

        case kFontClassDecorative:
        case kFontClassSymbol:
        case kFontClassUnknown:
        default:
            fallbacks[(*count)++] = geneva;
            fallbacks[(*count)++] = systemFont;
            break;
    }

    /* Remove duplicates */
    for (i = 0; i < *count - 1; i++) {
        for (j = i + 1; j < *count; j++) {
            if (fallbacks[i] == fallbacks[j]) {
                /* Remove duplicate */
                for (short k = j; k < *count - 1; k++) {
                    fallbacks[k] = fallbacks[k + 1];
                }
                (*count)--;
                j--; /* Check this position again */
            }
        }
    }

    return noErr;
}

static OSErr FindBestSubstitute(short originalID, short size, short style, short *substituteID)
{
    FontClass originalClass;
    short bestID = systemFont;
    short bestSimilarity = 0;
    short i, similarity;

    if (substituteID == NULL) {
        return paramErr;
    }

    originalClass = ClassifyFont(originalID);

    /* Try fonts of the same class first */
    for (i = 0; i < DEFAULT_FALLBACK_COUNT; i++) {
        short candidateID = gDefaultFallbacks[i].originalID;

        if (candidateID != originalID && ClassifyFont(candidateID) == originalClass) {
            if (IsFontAvailable(candidateID, size, style)) {
                if (AnalyzeFontSimilarity(originalID, candidateID, &similarity) == noErr) {
                    if (similarity > bestSimilarity) {
                        bestSimilarity = similarity;
                        bestID = candidateID;
                    }
                }
            }
        }
    }

    /* If no good match found, use class-based default */
    if (bestSimilarity == 0) {
        GetDefaultSubstitute(originalClass, &bestID);
    }

    *substituteID = bestID;
    return noErr;
}

static Boolean IsFontAvailable(short familyID, short size, short style)
{
    Handle fontResource;
    short resourceID;
    BitmapFontData *bitmapFont;
    TTFont *ttFont;
    OSErr error;

    /* Try bitmap font */
    resourceID = (familyID * 128) + size; /* Simplified calculation */
    fontResource = GetResource(kNFNTResourceType, resourceID);
    if (fontResource == NULL) {
        fontResource = GetResource(kFONTResourceType, resourceID);
    }

    if (fontResource != NULL) {
        return TRUE;
    }

    /* Try TrueType font */
    fontResource = GetResource(kSFNTResourceType, familyID);
    if (fontResource != NULL) {
        return TRUE;
    }

    /* Try loading through Font Manager */
    error = LoadBitmapFont(familyID, &bitmapFont);
    if (error == noErr) {
        UnloadBitmapFont(bitmapFont);
        return TRUE;
    }

    error = LoadTrueTypeFont(familyID, &ttFont);
    if (error == noErr) {
        UnloadTrueTypeFont(ttFont);
        return TRUE;
    }

    return FALSE;
}

static OSErr AnalyzeFontSimilarity(short font1ID, short font2ID, short *similarity)
{
    FontClass class1, class2;
    short score = 0;

    if (similarity == NULL) {
        return paramErr;
    }

    class1 = ClassifyFont(font1ID);
    class2 = ClassifyFont(font2ID);

    /* Same class gets high score */
    if (class1 == class2) {
        score += 50;
    }

    /* Specific font pair bonuses */
    if ((font1ID == times && font2ID == newYork) ||
        (font1ID == newYork && font2ID == times)) {
        score += 30;
    }

    if ((font1ID == helvetica && font2ID == geneva) ||
        (font1ID == geneva && font2ID == helvetica)) {
        score += 30;
    }

    if ((font1ID == courier && font2ID == monaco) ||
        (font1ID == monaco && font2ID == courier)) {
        score += 30;
    }

    *similarity = score;
    return noErr;
}

static OSErr GetDefaultSubstitute(FontClass fontClass, short *substituteID)
{
    if (substituteID == NULL) {
        return paramErr;
    }

    switch (fontClass) {
        case kFontClassSerifProportional:
            *substituteID = times;
            break;

        case kFontClassSansSerifProportional:
            *substituteID = geneva;
            break;

        case kFontClassMonospace:
            *substituteID = monaco;
            break;

        case kFontClassDecorative:
        case kFontClassSymbol:
        case kFontClassUnknown:
        default:
            *substituteID = systemFont;
            break;
    }

    return noErr;
}