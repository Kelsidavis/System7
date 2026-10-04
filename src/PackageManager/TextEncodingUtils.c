/*
 * TextEncodingUtils.c - Text Encoding and Script Utilities
 *
 * Implements text encoding conversion and script/language management utilities
 * for the Mac OS String Package. These functions support international text
 * handling and character encoding conversion.
 *
 * Based on Inside Macintosh: Text
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "TextEncoding/TextEncodingUtils.h"

/* Debug logging */
#define TEXT_ENC_DEBUG 0

#if TEXT_ENC_DEBUG
#define TEXTENC_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[TextEnc] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define TEXTENC_LOG(...)
#endif

/* Language codes (from Inside Macintosh: Text) */
#ifndef langEnglish
#define langEnglish     0
#define langFrench      1
#define langGerman      2
#define langItalian     3
#define langDutch       4
#define langSwedish     5
#define langSpanish     6
#define langDanish      7
#define langPortuguese  8
#define langNorwegian   9
#define langHebrew      10
#define langJapanese    11
#define langArabic      12
#define langFinnish     13
#define langGreek       14
#define langIcelandic   15
#endif

/* Truncation modes */
#ifndef smTruncEnd
#define smTruncEnd      0
#define smTruncMiddle   2
#endif

/* Script and language configuration */
static ScriptCode g_currentScript = smRoman;  /* Default to Roman script */
static LangCode g_currentLanguage = langEnglish;  /* Default to English */

/*
 * TextEncodingToScript - Convert text encoding to script code
 *
 * Converts a text encoding identifier to its corresponding script code.
 * Text encodings are more specific than scripts; this function maps them
 * to the broader script category.
 *
 * Parameters:
 *   encoding - Text encoding identifier
 *
 * Returns:
 *   Script code corresponding to the encoding
 *
 * Example:
 *   ScriptCode script = TextEncodingToScript(kTextEncodingMacRoman);
 *   // Returns smRoman
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt32 TextEncodingToScript(SInt32 encoding) {
    TEXTENC_LOG("TextEncodingToScript: encoding=%ld\n", (long)encoding);

    /* Simple mapping of common text encodings to script codes
     * In a full implementation, this would use the Text Encoding Converter
     */
    switch (encoding) {
        case 0:  /* kTextEncodingMacRoman */
            return smRoman;

        case 1:  /* kTextEncodingMacJapanese */
            return smJapanese;

        case 2:  /* kTextEncodingMacChineseTrad */
            return smTradChinese;

        case 3:  /* kTextEncodingMacKorean */
            return smKorean;

        case 4:  /* kTextEncodingMacArabic */
            return smArabic;

        case 5:  /* kTextEncodingMacHebrew */
            return smHebrew;

        case 6:  /* kTextEncodingMacGreek */
            return smGreek;

        case 7:  /* kTextEncodingMacCyrillic */
            return smCyrillic;

        case 25: /* kTextEncodingMacChineseSimp */
            return smSimpChinese;

        default:
            /* Unknown encoding, default to Roman */
            TEXTENC_LOG("TextEncodingToScript: Unknown encoding %ld, defaulting to Roman\n",
                       (long)encoding);
            return smRoman;
    }
}

/*
 * ScriptToTextEncoding - Convert script code to text encoding
 *
 * Converts a script code and optional language code to a text encoding
 * identifier. This is the inverse of TextEncodingToScript.
 *
 * Parameters:
 *   script - Script code (e.g., smRoman, smJapanese)
 *   language - Language code (e.g., langEnglish, langJapanese)
 *
 * Returns:
 *   Text encoding identifier
 *
 * Example:
 *   SInt32 encoding = ScriptToTextEncoding(smRoman, langEnglish);
 *   // Returns kTextEncodingMacRoman (0)
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt32 ScriptToTextEncoding(ScriptCode script, LangCode language) {
    (void)language;
    TEXTENC_LOG("ScriptToTextEncoding: script=%d, language=%d\n", script, language);

    /* Simple mapping of script codes to text encodings
     * Language code provides additional refinement for scripts with
     * multiple regional variants
     */
    switch (script) {
        case smRoman:
            return 0;  /* kTextEncodingMacRoman */

        case smJapanese:
            return 1;  /* kTextEncodingMacJapanese */

        case smTradChinese:
            return 2;  /* kTextEncodingMacChineseTrad */

        case smKorean:
            return 3;  /* kTextEncodingMacKorean */

        case smArabic:
            return 4;  /* kTextEncodingMacArabic */

        case smHebrew:
            return 5;  /* kTextEncodingMacHebrew */

        case smGreek:
            return 6;  /* kTextEncodingMacGreek */

        case smCyrillic:
            return 7;  /* kTextEncodingMacCyrillic */

        case smSimpChinese:
            return 25; /* kTextEncodingMacChineseSimp */

        default:
            /* Unknown script, default to Roman */
            TEXTENC_LOG("ScriptToTextEncoding: Unknown script %d, defaulting to Roman\n",
                       script);
            return 0;  /* kTextEncodingMacRoman */
    }
}

/*
 * SetStringPackageScript - Set current script for String Package
 *
 * Sets the script code used by default for string operations. This affects
 * string comparison, sorting, and case conversion operations.
 *
 * Parameters:
 *   script - Script code to set as current
 *
 * Example:
 *   SetStringPackageScript(smJapanese);
 *   // String operations now use Japanese script rules
 *
 * Based on Inside Macintosh: Text
 */
void SetStringPackageScript(ScriptCode script) {
    TEXTENC_LOG("SetStringPackageScript: script=%d\n", script);
    g_currentScript = script;
}

/*
 * GetStringPackageScript - Get current script for String Package
 *
 * Returns the script code currently used by default for string operations.
 *
 * Returns:
 *   Current script code
 *
 * Example:
 *   ScriptCode script = GetStringPackageScript();
 *
 * Based on Inside Macintosh: Text
 */
ScriptCode GetStringPackageScript(void) {
    TEXTENC_LOG("GetStringPackageScript: returning %d\n", g_currentScript);
    return g_currentScript;
}

/*
 * SetStringPackageLanguage - Set current language for String Package
 *
 * Sets the language code used by default for string operations. This provides
 * more specific localization than script code alone (e.g., distinguishing
 * between US English and UK English within the Roman script).
 *
 * Parameters:
 *   language - Language code to set as current
 *
 * Example:
 *   SetStringPackageLanguage(langFrench);
 *   // String operations now use French language rules
 *
 * Based on Inside Macintosh: Text
 */
void SetStringPackageLanguage(LangCode language) {
    TEXTENC_LOG("SetStringPackageLanguage: language=%d\n", language);
    g_currentLanguage = language;
}

/*
 * GetStringPackageLanguage - Get current language for String Package
 *
 * Returns the language code currently used by default for string operations.
 *
 * Returns:
 *   Current language code
 *
 * Example:
 *   LangCode language = GetStringPackageLanguage();
 *
 * Based on Inside Macintosh: Text
 */
LangCode GetStringPackageLanguage(void) {
    TEXTENC_LOG("GetStringPackageLanguage: returning %d\n", g_currentLanguage);
    return g_currentLanguage;
}

/*
 * TruncString - Truncate string to fit within pixel width
 *
 * Truncates a Pascal string to fit within a specified pixel width, adding
 * an ellipsis (…) to indicate truncation. This is commonly used for
 * displaying file names, menu items, and other text that may be too long
 * for the available space.
 *
 * Parameters:
 *   width - Maximum width in pixels
 *   theString - Pascal string to truncate (modified in place)
 *   truncWhere - Where to truncate (truncEnd, truncMiddle, or smTruncEnd)
 *
 * Truncation modes:
 *   smTruncEnd (0) - Truncate at end: "Long file name…"
 *   smTruncMiddle (2) - Truncate in middle: "Long…name"
 *
 * Example:
 *   Str255 filename = "\pVery Long File Name.txt";
 *   TruncString(100, filename, smTruncEnd);
 *   // Result: "Very Long…" (if width only allows ~10 characters)
 *
 * Note: This implementation provides basic truncation. A full implementation
 * would calculate exact character widths using the current font metrics.
 *
 * Based on Inside Macintosh: Text
 */
void TruncString(SInt16 width, char* theString, SInt16 truncWhere) {
    UInt8 len;
    SInt16 estimatedWidth;
    UInt8 maxChars;

    if (!theString) {
        TEXTENC_LOG("TruncString: NULL string pointer\n");
        return;
    }

    len = (UInt8)theString[0];

    if (len == 0 || width <= 0) {
        return;
    }

    /* Rough estimation: assume average character width of 6 pixels
     * (this is approximate; real implementation would use StringWidth
     * from Font Manager)
     */
    estimatedWidth = len * 6;

    if (estimatedWidth <= width) {
        /* String already fits */
        TEXTENC_LOG("TruncString: String already fits (len=%d, width=%d)\n",
                   len, width);
        return;
    }

    /* Calculate approximately how many characters fit
     * Reserve 3 characters worth of space for ellipsis
     */
    maxChars = (UInt8)((width / 6) - 1);
    if (maxChars < 1) {
        maxChars = 1;
    }

    TEXTENC_LOG("TruncString: Truncating from %d to ~%d chars (width=%d, where=%d)\n",
               len, maxChars, width, truncWhere);

    /* Apply truncation based on mode */
    switch (truncWhere) {
        case 0:  /* smTruncEnd - truncate at end */
            if (maxChars >= len) {
                return;  /* No truncation needed */
            }
            /* Add ellipsis character (0xC9 in Mac Roman encoding) */
            theString[maxChars] = 0xC9;  /* Ellipsis: … */
            theString[0] = maxChars;
            break;

        case 2:  /* smTruncMiddle - truncate in middle */
            {
                UInt8 halfChars = maxChars / 2;
                UInt8 keepStart = halfChars;
                UInt8 keepEnd = maxChars - halfChars - 1;  /* -1 for ellipsis */

                if (keepStart + keepEnd + 1 >= len) {
                    return;  /* No truncation needed */
                }

                /* Move end portion after the ellipsis */
                if (keepEnd > 0) {
                    UInt8 i;
                    for (i = 0; i < keepEnd; i++) {
                        theString[keepStart + 2 + i] = theString[len - keepEnd + 1 + i];
                    }
                }

                /* Insert ellipsis */
                theString[keepStart + 1] = 0xC9;  /* Ellipsis: … */
                theString[0] = keepStart + 1 + keepEnd;
            }
            break;

        default:
            /* Unknown truncation mode, default to end truncation */
            TEXTENC_LOG("TruncString: Unknown truncation mode %d, using end\n",
                       truncWhere);
            if (maxChars < len) {
                theString[maxChars] = 0xC9;  /* Ellipsis: … */
                theString[0] = maxChars;
            }
            break;
    }
}
