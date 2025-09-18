/*
 * Modern Font Loader Implementation
 * Loads and manages TrueType/OpenType versions of classic System 7.1 fonts
 *
 * This implementation provides integration between original bitmap fonts
 * and modern vector font versions while maintaining API compatibility.
 */

#include "../../include/FontResources/ModernFontLoader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <strings.h>

/* Handle missing DT_REG on some systems */
#ifndef DT_REG
#define DT_REG 8
#endif

/* Global modern font collection */
ModernFontCollection gModernFonts = {0};

/* Current font preference */
static FontPreferenceMode gFontPreference = kFontPreferAuto;

/* Expected font file names for each System 7.1 font */
const char* kExpectedFontFiles[] = {
    "Chicago.ttf", "Chicago.otf",
    "Geneva.ttf", "Geneva.otf",
    "New York.ttf", "NewYork.ttf", "New York.otf", "NewYork.otf",
    "Monaco.ttf", "Monaco.otf",
    "Courier.ttf", "Courier.otf",
    "Helvetica.ttf", "Helvetica.otf"
};
const short kNumExpectedFonts = sizeof(kExpectedFontFiles) / sizeof(kExpectedFontFiles[0]);

/* Font file name patterns for recognition */
const char* kChicagoFilePatterns[] = {"chicago", "chikargo", "chikareGo", NULL};
const char* kGenevaFilePatterns[] = {"geneva", "finderskeepers", NULL};
const char* kNewYorkFilePatterns[] = {"new york", "newyork", "new_york", NULL};
const char* kMonacoFilePatterns[] = {"monaco", NULL};
const char* kCourierFilePatterns[] = {"courier", NULL};
const char* kHelveticaFilePatterns[] = {"helvetica", NULL};

/*
 * Utility function to convert string to lowercase
 */
static void ToLowerCase(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

/*
 * Check if file exists and is readable
 */
static Boolean FileExists(const char* filePath) {
    struct stat st;
    return (stat(filePath, &st) == 0 && S_ISREG(st.st_mode));
}

/*
 * Get file size in bytes
 */
static long GetFileSize(const char* filePath) {
    struct stat st;
    if (stat(filePath, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

/*
 * Determine font format from file extension
 */
static ModernFontFormat GetFontFormat(const char* fileName) {
    const char* ext = strrchr(fileName, '.');
    if (!ext) return kModernFontTrueType; /* Default */

    if (strcasecmp(ext, ".ttf") == 0) return kModernFontTrueType;
    if (strcasecmp(ext, ".otf") == 0) return kModernFontOpenType;
    if (strcasecmp(ext, ".woff") == 0) return kModernFontWOFF;
    if (strcasecmp(ext, ".woff2") == 0) return kModernFontWOFF2;

    return kModernFontTrueType; /* Default */
}

/*
 * MapFontFileName - Map font file name to System 7.1 font family
 */
short MapFontFileName(const char* fileName) {
    char lowerName[256];
    strncpy(lowerName, fileName, sizeof(lowerName) - 1);
    lowerName[sizeof(lowerName) - 1] = '\0';
    ToLowerCase(lowerName);

    /* Remove extension for matching */
    char* dot = strrchr(lowerName, '.');
    if (dot) *dot = '\0';

    /* Check each font pattern */
    for (int i = 0; kChicagoFilePatterns[i]; i++) {
        if (strstr(lowerName, kChicagoFilePatterns[i])) {
            return kChicagoFont;
        }
    }

    for (int i = 0; kGenevaFilePatterns[i]; i++) {
        if (strstr(lowerName, kGenevaFilePatterns[i])) {
            return kGenevahFont;
        }
    }

    for (int i = 0; kNewYorkFilePatterns[i]; i++) {
        if (strstr(lowerName, kNewYorkFilePatterns[i])) {
            return kNewYorkFont;
        }
    }

    for (int i = 0; kMonacoFilePatterns[i]; i++) {
        if (strstr(lowerName, kMonacoFilePatterns[i])) {
            return kMonacoFont;
        }
    }

    for (int i = 0; kCourierFilePatterns[i]; i++) {
        if (strstr(lowerName, kCourierFilePatterns[i])) {
            return kCourierFont;
        }
    }

    for (int i = 0; kHelveticaFilePatterns[i]; i++) {
        if (strstr(lowerName, kHelveticaFilePatterns[i])) {
            return kHelveticaFont;
        }
    }

    return -1; /* Not recognized */
}

/*
 * LoadFontFile - Load a specific modern font file
 */
OSErr LoadFontFile(const char* filePath, ModernFontFile* fontFile) {
    if (!filePath || !fontFile) return paramErr;

    /* Initialize structure */
    memset(fontFile, 0, sizeof(ModernFontFile));

    /* Check if file exists */
    if (!FileExists(filePath)) {
        fontFile->isAvailable = false;
        return fnfErr;
    }

    /* Extract file name from path */
    const char* fileName = strrchr(filePath, '/');
    if (fileName) {
        fileName++; /* Skip the '/' */
    } else {
        fileName = filePath;
    }

    /* Fill structure */
    strncpy(fontFile->fileName, fileName, sizeof(fontFile->fileName) - 1);
    strncpy(fontFile->filePath, filePath, sizeof(fontFile->filePath) - 1);
    fontFile->format = GetFontFormat(fileName);
    fontFile->fileSize = GetFileSize(filePath);
    fontFile->familyID = MapFontFileName(fileName);
    fontFile->isAvailable = true;

    /* Set family name based on font ID */
    if (fontFile->familyID >= 0) {
        SystemFontPackage* package = GetSystemFont(fontFile->familyID);
        if (package) {
            memcpy(fontFile->familyName, package->family.familyName,
                   package->family.familyName[0] + 1);
        }
    }

    return noErr;
}

/*
 * ScanFontDirectory - Scan directory for modern font files
 */
OSErr ScanFontDirectory(const char* dirPath, ModernFontCollection* collection) {
    if (!dirPath || !collection) return paramErr;

    DIR* dir = opendir(dirPath);
    if (!dir) return fnfErr;

    struct dirent* entry;
    int fontCount = 0;
    ModernFontFile* tempFonts = NULL;

    /* Count potential font files first */
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { /* Regular file */
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && (strcasecmp(ext, ".ttf") == 0 ||
                       strcasecmp(ext, ".otf") == 0 ||
                       strcasecmp(ext, ".woff") == 0 ||
                       strcasecmp(ext, ".woff2") == 0)) {
                fontCount++;
            }
        }
    }

    if (fontCount == 0) {
        closedir(dir);
        return noErr; /* No fonts found, but not an error */
    }

    /* Allocate memory for fonts */
    tempFonts = (ModernFontFile*)malloc(fontCount * sizeof(ModernFontFile));
    if (!tempFonts) {
        closedir(dir);
        return memFullErr;
    }

    /* Reset directory position and load fonts */
    rewinddir(dir);
    int loadedFonts = 0;

    while ((entry = readdir(dir)) != NULL && loadedFonts < fontCount) {
        if (entry->d_type == DT_REG) {
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && (strcasecmp(ext, ".ttf") == 0 ||
                       strcasecmp(ext, ".otf") == 0 ||
                       strcasecmp(ext, ".woff") == 0 ||
                       strcasecmp(ext, ".woff2") == 0)) {

                /* Build full path */
                char fullPath[1024];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);

                /* Load font file */
                if (LoadFontFile(fullPath, &tempFonts[loadedFonts]) == noErr) {
                    loadedFonts++;
                }
            }
        }
    }

    closedir(dir);

    /* Update collection */
    if (collection->fonts) {
        free(collection->fonts);
    }
    collection->fonts = tempFonts;
    collection->numFonts = loadedFonts;
    strncpy(collection->basePath, dirPath, sizeof(collection->basePath) - 1);

    printf("Loaded %d modern font files from %s\n", loadedFonts, dirPath);

    return noErr;
}

/*
 * LoadModernFonts - Load modern TrueType/OpenType font files
 */
OSErr LoadModernFonts(const char* fontDir) {
    if (!fontDir) return paramErr;

    return ScanFontDirectory(fontDir, &gModernFonts);
}

/*
 * GetModernFontCollection - Get the loaded modern font collection
 */
ModernFontCollection* GetModernFontCollection(void) {
    return &gModernFonts;
}

/*
 * FindModernFont - Find modern font file for a System 7.1 font family
 */
ModernFontFile* FindModernFont(short familyID) {
    for (int i = 0; i < gModernFonts.numFonts; i++) {
        if (gModernFonts.fonts[i].familyID == familyID &&
            gModernFonts.fonts[i].isAvailable) {
            return &gModernFonts.fonts[i];
        }
    }
    return NULL;
}

/*
 * FindModernFontByName - Find modern font by name
 */
ModernFontFile* FindModernFontByName(ConstStr255Param fontName) {
    if (!fontName) return NULL;

    /* Convert Pascal string to C string */
    char cName[256];
    memcpy(cName, &fontName[1], fontName[0]);
    cName[fontName[0]] = '\0';

    for (int i = 0; i < gModernFonts.numFonts; i++) {
        if (gModernFonts.fonts[i].isAvailable) {
            /* Compare with family name */
            char familyName[256];
            memcpy(familyName, &gModernFonts.fonts[i].familyName[1],
                   gModernFonts.fonts[i].familyName[0]);
            familyName[gModernFonts.fonts[i].familyName[0]] = '\0';

            if (strcasecmp(cName, familyName) == 0) {
                return &gModernFonts.fonts[i];
            }
        }
    }
    return NULL;
}

/*
 * SetFontPreference - Set font rendering preference
 */
void SetFontPreference(FontPreferenceMode mode) {
    gFontPreference = mode;
}

/*
 * GetFontPreference - Get current font preference mode
 */
FontPreferenceMode GetFontPreference(void) {
    return gFontPreference;
}

/*
 * IsModernFontAvailable - Check if modern version of font is available
 */
Boolean IsModernFontAvailable(short familyID) {
    return (FindModernFont(familyID) != NULL);
}

/*
 * GetRecommendedFontType - Get recommended font type for size
 */
Boolean GetRecommendedFontType(short size) {
    /* Recommend bitmap fonts for small sizes, modern for large sizes */
    return (size >= 14); /* 14pt and above use modern fonts */
}

/*
 * GetOptimalFont - Get optimal font choice based on size and preference
 */
SystemFontPackage* GetOptimalFont(short familyID, short size, Boolean* useModern) {
    SystemFontPackage* package = GetSystemFont(familyID);
    if (!package) return NULL;

    Boolean modernAvailable = IsModernFontAvailable(familyID);
    Boolean recommendModern = GetRecommendedFontType(size);

    if (useModern) {
        switch (gFontPreference) {
            case kFontPreferBitmap:
                *useModern = false;
                break;
            case kFontPreferModern:
                *useModern = modernAvailable;
                break;
            case kFontPreferAuto:
                *useModern = modernAvailable && recommendModern;
                break;
        }
    }

    return package;
}

/*
 * GetFontCompatibilityScore - Get compatibility score between bitmap and modern font
 */
short GetFontCompatibilityScore(const BitmapFont* bitmapFont, const ModernFontFile* modernFile) {
    if (!bitmapFont || !modernFile) return 0;

    short score = 50; /* Base score */

    /* Same family = +30 points */
    if (modernFile->familyID >= 0) {
        score += 30;
    }

    /* TrueType format = +10 points */
    if (modernFile->format == kModernFontTrueType) {
        score += 10;
    }

    /* Available file = +10 points */
    if (modernFile->isAvailable) {
        score += 10;
    }

    return (score > 100) ? 100 : score;
}

/*
 * GetExpectedFontFiles - Get list of expected modern font files
 */
short GetExpectedFontFiles(char fileNames[][256], short maxFiles) {
    short count = 0;
    for (int i = 0; i < kNumExpectedFonts && count < maxFiles; i++) {
        strncpy(fileNames[count], kExpectedFontFiles[i], 255);
        fileNames[count][255] = '\0';
        count++;
    }
    return count;
}

/*
 * UnloadModernFonts - Release modern font resources
 */
void UnloadModernFonts(void) {
    if (gModernFonts.fonts) {
        free(gModernFonts.fonts);
        gModernFonts.fonts = NULL;
    }
    gModernFonts.numFonts = 0;
    memset(gModernFonts.basePath, 0, sizeof(gModernFonts.basePath));

    printf("Modern fonts unloaded\n");
}