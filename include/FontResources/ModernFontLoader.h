/*
 * Modern Font Loader for System 7.1 Portable
 * Loads TrueType/OpenType versions of classic Mac OS fonts
 *
 * This header provides functionality to load modern vector font versions
 * of the classic System 7.1 fonts (Chicago, Geneva, Monaco, etc.) while
 * maintaining compatibility with the original bitmap font API.
 */

#ifndef MODERN_FONT_LOADER_H
#define MODERN_FONT_LOADER_H

#include "SystemFonts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Modern font format types */
typedef enum {
    kModernFontTrueType = 1,
    kModernFontOpenType = 2,
    kModernFontWOFF = 3,
    kModernFontWOFF2 = 4
} ModernFontFormat;

/* Modern font file structure */
typedef struct ModernFontFile {
    char            fileName[256];      /* Font file name */
    char            filePath[512];      /* Full path to font file */
    ModernFontFormat format;            /* Font format type */
    long            fileSize;           /* File size in bytes */
    short           familyID;           /* Corresponding System 7.1 font ID */
    Str255          familyName;         /* Font family name */
    Boolean         isAvailable;        /* Font file exists and is readable */
} ModernFontFile;

/* Modern font collection */
typedef struct ModernFontCollection {
    short           numFonts;           /* Number of modern fonts */
    ModernFontFile* fonts;              /* Array of modern font files */
    char            basePath[512];      /* Base directory for font files */
} ModernFontCollection;

/* Font preference modes */
typedef enum {
    kFontPreferBitmap = 0,      /* Prefer original bitmap fonts */
    kFontPreferModern = 1,      /* Prefer modern vector fonts */
    kFontPreferAuto = 2         /* Auto-select based on size/context */
} FontPreferenceMode;

/* Function prototypes */

/*
 * LoadModernFonts - Load modern TrueType/OpenType font files
 * Parameters: fontDir - Directory containing modern font files
 * Returns: noErr on success, error code on failure
 */
OSErr LoadModernFonts(const char* fontDir);

/*
 * GetModernFontCollection - Get the loaded modern font collection
 * Returns: Pointer to ModernFontCollection, NULL if not loaded
 */
ModernFontCollection* GetModernFontCollection(void);

/*
 * FindModernFont - Find modern font file for a System 7.1 font family
 * Parameters: familyID - System 7.1 font family ID
 * Returns: Pointer to ModernFontFile, NULL if not found
 */
ModernFontFile* FindModernFont(short familyID);

/*
 * FindModernFontByName - Find modern font by name
 * Parameters: fontName - Font family name (Pascal string)
 * Returns: Pointer to ModernFontFile, NULL if not found
 */
ModernFontFile* FindModernFontByName(ConstStr255Param fontName);

/*
 * SetFontPreference - Set font rendering preference
 * Parameters: mode - Font preference mode
 */
void SetFontPreference(FontPreferenceMode mode);

/*
 * GetFontPreference - Get current font preference mode
 * Returns: Current FontPreferenceMode
 */
FontPreferenceMode GetFontPreference(void);

/*
 * IsModernFontAvailable - Check if modern version of font is available
 * Parameters: familyID - System 7.1 font family ID
 * Returns: true if modern font is available, false otherwise
 */
Boolean IsModernFontAvailable(short familyID);

/*
 * GetOptimalFont - Get optimal font choice based on size and preference
 * Parameters: familyID - Font family ID
 *            size - Requested point size
 *            useModern - Output: true if modern font recommended
 * Returns: Font package (bitmap or modern info), NULL if not available
 */
SystemFontPackage* GetOptimalFont(short familyID, short size, Boolean* useModern);

/*
 * LoadFontFile - Load a specific modern font file
 * Parameters: filePath - Path to font file
 *            fontFile - Output structure to fill
 * Returns: noErr on success, error code on failure
 */
OSErr LoadFontFile(const char* filePath, ModernFontFile* fontFile);

/*
 * ScanFontDirectory - Scan directory for modern font files
 * Parameters: dirPath - Directory to scan
 *            collection - Collection to populate
 * Returns: noErr on success, error code on failure
 */
OSErr ScanFontDirectory(const char* dirPath, ModernFontCollection* collection);

/*
 * UnloadModernFonts - Release modern font resources
 */
void UnloadModernFonts(void);

/* Font mapping utilities */

/*
 * MapFontFileName - Map font file name to System 7.1 font family
 * Parameters: fileName - Font file name
 * Returns: System 7.1 font family ID, or -1 if not recognized
 */
short MapFontFileName(const char* fileName);

/*
 * GetExpectedFontFiles - Get list of expected modern font files
 * Parameters: fileNames - Array to fill with file names
 *            maxFiles - Maximum number of files
 * Returns: Number of expected files
 */
short GetExpectedFontFiles(char fileNames[][256], short maxFiles);

/* Font quality/size recommendations */

/*
 * GetRecommendedFontType - Get recommended font type for size
 * Parameters: size - Point size
 * Returns: true for modern fonts, false for bitmap fonts
 */
Boolean GetRecommendedFontType(short size);

/*
 * ConvertBitmapToVector - Information about vector conversion
 * Parameters: bitmapFont - Original bitmap font
 *            modernFile - Corresponding modern font file
 * Returns: Quality score (0-100)
 */
short GetFontCompatibilityScore(const BitmapFont* bitmapFont, const ModernFontFile* modernFile);

/* Global modern font collection */
extern ModernFontCollection gModernFonts;

/* Expected modern font file names for the six core System 7.1 fonts */
extern const char* kExpectedFontFiles[];
extern const short kNumExpectedFonts;

/* Font file name patterns for recognition */
extern const char* kChicagoFilePatterns[];
extern const char* kGenevaFilePatterns[];
extern const char* kNewYorkFilePatterns[];
extern const char* kMonacoFilePatterns[];
extern const char* kCourierFilePatterns[];
extern const char* kHelveticaFilePatterns[];

#ifdef __cplusplus
}
#endif

#endif /* MODERN_FONT_LOADER_H */

/*
 * Implementation Notes:
 *
 * This system provides a bridge between the original System 7.1 bitmap fonts
 * and modern TrueType/OpenType versions. The key design principles:
 *
 * 1. Compatibility: Maintains the original font ID and naming system
 * 2. Flexibility: Supports multiple font formats (TTF, OTF, WOFF, WOFF2)
 * 3. Quality: Automatic selection between bitmap and vector based on context
 * 4. Performance: Lazy loading and caching of font files
 *
 * Font Selection Logic:
 * - Small sizes (9-12pt): Prefer bitmap fonts for pixel-perfect rendering
 * - Large sizes (14pt+): Prefer vector fonts for smooth scaling
 * - User preference: Allow manual override of automatic selection
 * - Fallback: Always fall back to bitmap if modern font unavailable
 */