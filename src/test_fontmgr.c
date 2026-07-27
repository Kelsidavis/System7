/*
 * test_fontmgr.c - Font Manager Test Program
 * Tests the core Font Manager APIs with Chicago font
 */

#include "FontManager/FontManager.h"
#include "FontManager/FontTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontLogging.h"

void TestFontManager(void);

void TestFontManager(void) {
    Str255 fontName;
    short familyID;
    FMetricRec metrics;

    FONT_LOG_DEBUG("FM: === Font Manager Test Suite ===\n");

    /* Initialize Font Manager */
    InitFonts();
    FONT_LOG_DEBUG("FM: InitFonts() complete\n");

    /* Test 1: Get font names */
    GetFontName(chicagoFont, fontName);
    FONT_LOG_DEBUG("FM: Font ID 0 name: %.*s\n", fontName[0], &fontName[1]);

    GetFontName(genevaFont, fontName);
    FONT_LOG_DEBUG("FM: Font ID 3 name: %.*s\n", fontName[0], &fontName[1]);

    /* Test 2: Get font ID from name */
    static Str255 chicagoStr;
    c2pstrcpy(chicagoStr, "Chicago");
    GetFNum(chicagoStr, &familyID);
    FONT_LOG_DEBUG("FM: 'Chicago' -> ID %d\n", familyID);

    /* Test 3: Check real font */
    Boolean isReal = RealFont(chicagoFont, 12);
    FONT_LOG_DEBUG("FM: Chicago 12 is real: %s\n", isReal ? "YES" : "NO");

    isReal = RealFont(chicagoFont, 14);
    FONT_LOG_DEBUG("FM: Chicago 14 is real: %s\n", isReal ? "YES" : "NO");

    /* Test 4: Set font and get metrics */
    TextFont(chicagoFont);
    TextSize(12);
    TextFace(normal);

    GetFontMetrics(&metrics);
    FONT_LOG_DEBUG("FM: Chicago 12 metrics:\n");
    FONT_LOG_DEBUG("FM:   Ascent: %d\n", metrics.ascent);
    FONT_LOG_DEBUG("FM:   Descent: %d\n", metrics.descent);
    FONT_LOG_DEBUG("FM:   Leading: %d\n", metrics.leading);
    FONT_LOG_DEBUG("FM:   WidMax: %d\n", metrics.widMax);

    /* Test 5: Measure text widths */
    static Str255 testStr1;
    c2pstrcpy(testStr1, "System 7");
    short width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM: Width of 'System 7' = %d pixels\n", width);

    static Str255 testStr2;
    c2pstrcpy(testStr2, "About This Macintosh");
    width = StringWidth(testStr2);
    FONT_LOG_DEBUG("FM: Width of 'About This Macintosh' = %d pixels\n", width);

    /* Test 6: Character widths */
    FONT_LOG_DEBUG("FM: Character widths:\n");
    FONT_LOG_DEBUG("FM:   'A' = %d pixels\n", CharWidth('A'));
    FONT_LOG_DEBUG("FM:   'W' = %d pixels\n", CharWidth('W'));
    FONT_LOG_DEBUG("FM:   'i' = %d pixels\n", CharWidth('i'));
    FONT_LOG_DEBUG("FM:   ' ' = %d pixels\n", CharWidth(' '));

    /* Test 7: Bold face */
    TextFace(bold);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM: Width of 'System 7' (bold) = %d pixels\n", width);

    /* Test 7b: Italic face */
    TextFace(italic);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM: Width of 'System 7' (italic) = %d pixels\n", width);

    /* Test 7c: Bold+Italic */
    TextFace(bold | italic);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM: Width of 'System 7' (bold+italic) = %d pixels\n", width);

    /* Test 7d: Underline */
    TextFace(underline);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM: Width of 'System 7' (underline) = %d pixels\n", width);

    /* Test 7e: Shadow */
    TextFace(shadow);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM: Width of 'System 7' (shadow) = %d pixels\n", width);

    /* Test 7f: Outline */
    TextFace(outline);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM: Width of 'System 7' (outline) = %d pixels\n", width);

    /* Reset face for size tests */
    TextFace(normal);

    /* Test 8: Different font sizes */
    FONT_LOG_DEBUG("FM: Testing multiple font sizes:\n");

    TextSize(9);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM:   'System 7' at 9pt = %d pixels\n", width);

    TextSize(10);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM:   'System 7' at 10pt = %d pixels\n", width);

    TextSize(12);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM:   'System 7' at 12pt = %d pixels\n", width);

    TextSize(14);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM:   'System 7' at 14pt = %d pixels\n", width);

    TextSize(18);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM:   'System 7' at 18pt = %d pixels\n", width);

    TextSize(24);
    width = StringWidth(testStr1);
    FONT_LOG_DEBUG("FM:   'System 7' at 24pt = %d pixels\n", width);

    /* Reset to default */
    TextSize(12);

    /* Test 9: FMSwapFont */
    FMInput input = {chicagoFont, 12, normal, 0, 1, 1}; /* FALSE = 0 */
    FMOutPtr output = FMSwapFont(&input);
    if (output) {
        FONT_LOG_DEBUG("FM: FMSwapFont returned:\n");
        FONT_LOG_DEBUG("FM:   errNum: %d\n", output->errNum);
        FONT_LOG_DEBUG("FM:   ascent: %d\n", output->ascent);
        FONT_LOG_DEBUG("FM:   descent: %d\n", output->descent);
        FONT_LOG_DEBUG("FM:   widMax: %d\n", output->widMax);
    }

    FONT_LOG_DEBUG("FM: === Font Manager Tests Complete ===\n");
}