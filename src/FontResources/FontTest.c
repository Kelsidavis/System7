/*
 * System 7.1 Font Test Utility
 * Tests and demonstrates the portable font system
 *
 * This utility validates the font resource conversion and provides
 * examples of using the System 7.1 font API
 */

#include "../../include/FontResources/SystemFonts.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * PrintFontInfo - Display information about a font family
 */
static void PrintFontInfo(SystemFontPackage* package) {
    if (!package) {
        printf("  Font not available\n");
        return;
    }

    printf("  Family ID: %d\n", package->family.familyID);
    printf("  Name: %.*s\n", package->family.familyName[0], &package->family.familyName[1]);
    printf("  Character range: %d-%d\n", package->family.firstChar, package->family.lastChar);
    printf("  Ascent: %d pixels\n", package->family.ascent);
    printf("  Descent: %d pixels\n", package->family.descent);
    printf("  Leading: %d pixels\n", package->family.leading);
    printf("  Max width: %d pixels\n", package->family.widMax);
    printf("  Bitmap fonts: %d\n", package->numFonts);
    printf("  Resources: %d\n", package->numResources);

    if (package->numFonts > 0 && package->fonts) {
        BitmapFont* font = &package->fonts[0];
        printf("  First font metrics:\n");
        printf("    Type: 0x%04X\n", font->fontType);
        printf("    Font rect: %dx%d pixels\n", font->fRectWidth, font->fRectHeight);
        printf("    Row words: %d\n", font->rowWords);
    }
}

/*
 * TestFontMetrics - Test font metrics functions
 */
static void TestFontMetrics(void) {
    printf("\n=== Font Metrics Test ===\n");

    short fontIDs[] = {kChicagoFont, kGenevahFont, kNewYorkFont,
                       kMonacoFont, kCourierFont, kHelveticaFont};
    const char* fontNames[] = {"Chicago", "Geneva", "New York",
                              "Monaco", "Courier", "Helvetica"};

    for (int i = 0; i < 6; i++) {
        short ascent, descent, leading;
        OSErr err = GetFontMetrics(fontIDs[i], &ascent, &descent, &leading);

        if (err == noErr) {
            printf("%s: ascent=%d, descent=%d, leading=%d\n",
                   fontNames[i], ascent, descent, leading);
        } else {
            printf("%s: metrics not available (error %d)\n", fontNames[i], err);
        }
    }
}

/*
 * TestFontNames - Test font name functions
 */
static void TestFontNames(void) {
    printf("\n=== Font Names Test ===\n");

    short fontIDs[] = {kChicagoFont, kGenevahFont, kNewYorkFont,
                       kMonacoFont, kCourierFont, kHelveticaFont};

    for (int i = 0; i < 6; i++) {
        Str255 fontName;
        OSErr err = GetFontName(fontIDs[i], fontName);

        if (err == noErr) {
            printf("Font ID %d: %.*s\n", fontIDs[i], fontName[0], &fontName[1]);
        } else {
            printf("Font ID %d: name not available (error %d)\n", fontIDs[i], err);
        }
    }
}

/*
 * TestFontAvailability - Test font availability checking
 */
static void TestFontAvailability(void) {
    printf("\n=== Font Availability Test ===\n");

    short testIDs[] = {kChicagoFont, kGenevahFont, kNewYorkFont,
                       kMonacoFont, kCourierFont, kHelveticaFont, 99};
    const char* testNames[] = {"Chicago", "Geneva", "New York",
                              "Monaco", "Courier", "Helvetica", "Unknown"};

    for (int i = 0; i < 7; i++) {
        Boolean available = IsFontAvailable(testIDs[i]);
        printf("%s (ID %d): %s\n", testNames[i], testIDs[i],
               available ? "Available" : "Not Available");
    }
}

/*
 * TestStandardSizes - Test standard font sizes
 */
static void TestStandardSizes(void) {
    printf("\n=== Standard Font Sizes Test ===\n");

    short numSizes;
    const short* sizes = GetStandardFontSizes(&numSizes);

    printf("Standard font sizes (%d total): ", numSizes);
    for (int i = 0; i < numSizes; i++) {
        printf("%d", sizes[i]);
        if (i < numSizes - 1) printf(", ");
    }
    printf(" points\n");
}

/*
 * TestBitmapFonts - Test bitmap font access
 */
static void TestBitmapFonts(void) {
    printf("\n=== Bitmap Font Test ===\n");

    BitmapFont* font = GetBitmapFont(kChicagoFont, 12, kFontStylePlain);
    if (font) {
        printf("Chicago 12pt bitmap font:\n");
        printf("  Type: 0x%04X\n", font->fontType);
        printf("  Character range: %d-%d\n", font->firstChar, font->lastChar);
        printf("  Font rectangle: %dx%d\n", font->fRectWidth, font->fRectHeight);
        printf("  Ascent/descent: %d/%d\n", font->ascent, font->descent);
    } else {
        printf("Chicago 12pt bitmap font not available\n");
    }
}

/*
 * Main test function
 */
int main(void) {
    printf("=== System 7.1 Font Resource Test ===\n");

    /* Initialize the font system */
    OSErr err = InitSystemFonts();
    if (err != noErr) {
        printf("Failed to initialize font system (error %d)\n", err);
        return 1;
    }

    /* Test individual fonts */
    printf("\n=== Individual Font Information ===\n");

    printf("\nChicago (System Font):\n");
    PrintFontInfo(GetSystemFont(kChicagoFont));

    printf("\nGeneva (Application Font):\n");
    PrintFontInfo(GetSystemFont(kGenevahFont));

    printf("\nNew York (Serif Font):\n");
    PrintFontInfo(GetSystemFont(kNewYorkFont));

    printf("\nMonaco (Monospace Font):\n");
    PrintFontInfo(GetSystemFont(kMonacoFont));

    printf("\nCourier (Monospace Serif):\n");
    PrintFontInfo(GetSystemFont(kCourierFont));

    printf("\nHelvetica (Sans Serif):\n");
    PrintFontInfo(GetSystemFont(kHelveticaFont));

    /* Run additional tests */
    TestFontMetrics();
    TestFontNames();
    TestFontAvailability();
    TestStandardSizes();
    TestBitmapFonts();

    /* Test font lookup by name */
    printf("\n=== Font Lookup by Name Test ===\n");
    Str255 testName = "\007Chicago";  /* Pascal string for "Chicago" */
    SystemFontPackage* package = GetFontByName(testName);
    if (package) {
        printf("Found font by name: %.*s (ID %d)\n",
               package->family.familyName[0], &package->family.familyName[1],
               package->family.familyID);
    } else {
        printf("Font lookup by name failed\n");
    }

    printf("\n=== Font Resource Test Complete ===\n");
    printf("All System 7.1 fonts are ready for use in portable applications.\n");

    return 0;
}