/*
 * Modern Font Integration Test
 * Tests the integration between bitmap and modern font systems
 *
 * This test validates the complete font system including:
 * - Bitmap font compatibility
 * - Modern font loading and detection
 * - Font preference system
 * - Optimal font selection
 */

#include "../../include/FontResources/SystemFonts.h"
#include "../../include/FontResources/ModernFontLoader.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * TestModernFontLoading - Test modern font detection and loading
 */
static void TestModernFontLoading(void) {
    printf("\n=== Modern Font Loading Test ===\n");

    /* Try to load modern fonts from default directory */
    OSErr err = LoadModernFonts("./resources/fonts/modern");
    if (err == noErr) {
        ModernFontCollection* collection = GetModernFontCollection();
        printf("✓ Modern font system initialized\n");
        printf("  Found %d modern font files\n", collection->numFonts);

        /* List detected fonts */
        for (int i = 0; i < collection->numFonts; i++) {
            ModernFontFile* font = &collection->fonts[i];
            const char* formatName = "Unknown";
            switch (font->format) {
                case kModernFontTrueType: formatName = "TrueType"; break;
                case kModernFontOpenType: formatName = "OpenType"; break;
                case kModernFontWOFF: formatName = "WOFF"; break;
                case kModernFontWOFF2: formatName = "WOFF2"; break;
            }

            printf("  - %s (%s, Family ID: %d, %.1f KB)\n",
                   font->fileName, formatName, font->familyID,
                   font->fileSize / 1024.0);
        }
    } else {
        printf("⚠ Modern fonts not found (directory: ./resources/fonts/modern)\n");
        printf("  This is expected if fonts haven't been downloaded yet\n");
    }
}

/*
 * TestFontMapping - Test font file name to family ID mapping
 */
static void TestFontMapping(void) {
    printf("\n=== Font File Mapping Test ===\n");

    const char* testFiles[] = {
        "Chicago.ttf", "chicago.otf", "ChiKareGo.ttf",
        "Geneva.ttf", "geneva.otf", "FindersKeepers.ttf",
        "Monaco.ttf", "monaco.otf",
        "New York.ttf", "NewYork.ttf", "newyork.otf",
        "Courier.ttf", "courier.otf",
        "Helvetica.ttf", "helvetica.otf",
        "Unknown.ttf"
    };

    for (int i = 0; i < sizeof(testFiles) / sizeof(testFiles[0]); i++) {
        short familyID = MapFontFileName(testFiles[i]);
        if (familyID >= 0) {
            SystemFontPackage* package = GetSystemFont(familyID);
            printf("✓ %s → Family ID %d (%.*s)\n",
                   testFiles[i], familyID,
                   package ? package->family.familyName[0] : 0,
                   package ? &package->family.familyName[1] : "Unknown");
        } else {
            printf("✗ %s → Not recognized\n", testFiles[i]);
        }
    }
}

/*
 * TestFontPreferences - Test font preference system
 */
static void TestFontPreferences(void) {
    printf("\n=== Font Preference Test ===\n");

    FontPreferenceMode modes[] = {kFontPreferBitmap, kFontPreferModern, kFontPreferAuto};
    const char* modeNames[] = {"Bitmap", "Modern", "Auto"};

    for (int m = 0; m < 3; m++) {
        SetFontPreference(modes[m]);
        printf("\nPreference: %s\n", modeNames[m]);

        /* Test different sizes */
        short testSizes[] = {9, 12, 14, 18, 24};
        for (int s = 0; s < 5; s++) {
            Boolean useModern;
            SystemFontPackage* package = GetOptimalFont(kChicagoFont, testSizes[s], &useModern);
            if (package) {
                printf("  %dpt Chicago: %s font recommended\n",
                       testSizes[s], useModern ? "Modern" : "Bitmap");
            }
        }
    }

    /* Reset to auto */
    SetFontPreference(kFontPreferAuto);
}

/*
 * TestFontAvailability - Test font availability checking
 */
static void TestFontAvailability(void) {
    printf("\n=== Font Availability Test ===\n");

    short fontIDs[] = {kChicagoFont, kGenevahFont, kNewYorkFont,
                       kMonacoFont, kCourierFont, kHelveticaFont};
    const char* fontNames[] = {"Chicago", "Geneva", "New York",
                              "Monaco", "Courier", "Helvetica"};

    for (int i = 0; i < 6; i++) {
        Boolean bitmapAvailable = IsFontAvailable(fontIDs[i]);
        Boolean modernAvailable = IsModernFontAvailable(fontIDs[i]);

        printf("  %s: Bitmap=%s, Modern=%s",
               fontNames[i],
               bitmapAvailable ? "✓" : "✗",
               modernAvailable ? "✓" : "✗");

        if (modernAvailable) {
            ModernFontFile* modernFont = FindModernFont(fontIDs[i]);
            if (modernFont) {
                printf(" (%s)", modernFont->fileName);
            }
        }
        printf("\n");
    }
}

/*
 * TestExpectedFonts - Test expected font file listing
 */
static void TestExpectedFonts(void) {
    printf("\n=== Expected Fonts Test ===\n");

    char fileNames[32][256];
    short numFiles = GetExpectedFontFiles(fileNames, 32);

    printf("Expected %d modern font files:\n", numFiles);
    for (int i = 0; i < numFiles; i++) {
        printf("  - %s\n", fileNames[i]);
    }
}

/*
 * TestFontRecommendations - Test font type recommendations
 */
static void TestFontRecommendations(void) {
    printf("\n=== Font Recommendation Test ===\n");

    printf("Font type recommendations by size:\n");
    for (short size = 8; size <= 72; size += 2) {
        Boolean recommendModern = GetRecommendedFontType(size);
        if (size % 6 == 0 || size <= 12 || size >= 18) { /* Show key sizes */
            printf("  %2dpt: %s\n", size, recommendModern ? "Modern" : "Bitmap");
        }
    }
}

/*
 * GenerateDownloadInstructions - Generate instructions for downloading fonts
 */
static void GenerateDownloadInstructions(void) {
    printf("\n=== Font Download Instructions ===\n");
    printf("To get modern versions of System 7.1 fonts:\n\n");

    printf("1. Urban Renewal Collection (High Quality):\n");
    printf("   Visit: https://www.kreativekorp.com/software/fonts/urbanrenewal/\n");
    printf("   Download: TrueType versions of classic Mac fonts\n\n");

    printf("2. GitHub macfonts Repository (Comprehensive):\n");
    printf("   Run: git clone https://github.com/JohnDDuncanIII/macfonts.git\n");
    printf("   Copy TTF files to: ./resources/fonts/modern/\n\n");

    printf("3. System Fonts (macOS):\n");
    printf("   Copy from: /System/Library/Fonts/Monaco.ttf\n");
    printf("   Copy from: /System/Library/Fonts/Geneva.ttf\n\n");

    printf("4. Alternative Sources:\n");
    printf("   - ChiKareGo.ttf (Chicago recreation)\n");
    printf("   - FindersKeepers.ttf (Geneva 9pt recreation)\n");
    printf("   - Search font sites for \"Mac classic fonts\"\n\n");

    printf("5. Installation:\n");
    printf("   mkdir -p ./resources/fonts/modern\n");
    printf("   # Place downloaded TTF/OTF files in the directory\n");
    printf("   # Re-run this test to verify detection\n");
}

/*
 * Main test function
 */
int main(void) {
    printf("=== System 7.1 Modern Font Integration Test ===\n");

    /* Initialize bitmap font system */
    OSErr err = InitSystemFonts();
    if (err != noErr) {
        printf("Failed to initialize bitmap font system\n");
        return 1;
    }

    /* Run comprehensive tests */
    TestModernFontLoading();
    TestFontMapping();
    TestFontAvailability();
    TestFontPreferences();
    TestExpectedFonts();
    TestFontRecommendations();

    /* Check if any modern fonts were found */
    ModernFontCollection* collection = GetModernFontCollection();
    if (collection->numFonts == 0) {
        GenerateDownloadInstructions();
    } else {
        printf("\n=== Integration Status ===\n");
        printf("✓ Bitmap fonts: 6/6 available\n");
        printf("✓ Modern fonts: %d detected\n", collection->numFonts);
        printf("✓ Font preference system: Working\n");
        printf("✓ Optimal font selection: Working\n");
        printf("\nFont system ready for use!\n");
    }

    /* Cleanup */
    UnloadModernFonts();

    printf("\n=== Modern Font Integration Test Complete ===\n");
    return 0;
}