/*
 * InternationalOrdering.c - International Ordering Utilities
 *
 * Implements script and language ordering functions for the Mac OS
 * International Utilities Package. These functions compare and order
 * scripts, languages, and text according to international sorting rules.
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations */
SInt16 IUScriptOrder(ScriptCode aScript, ScriptCode bScript);
SInt16 IULangOrder(LangCode aLang, LangCode bLang);
SInt16 IUTextOrder(const void* aPtr, const void* bPtr, SInt16 aLen, SInt16 bLen,
                   ScriptCode aScript, ScriptCode bScript, LangCode aLang, LangCode bLang);
SInt16 IUStringOrder(const char* aStr, const char* bStr, ScriptCode aScript, ScriptCode bScript,
                     LangCode aLang, LangCode bLang);

/* Debug logging */
#define IU_ORDER_DEBUG 0

#if IU_ORDER_DEBUG
#define ORDER_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[IUOrder] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define ORDER_LOG(...)
#endif

/* Helper function to compare bytes case-insensitively (from StringComparison.c) */
static SInt16 CompareBytesIgnoreCase(UInt8 a, UInt8 b) {
    /* Convert to lowercase for comparison */
    if (a >= 'A' && a <= 'Z') {
        a = a + ('a' - 'A');
    }
    if (b >= 'A' && b <= 'Z') {
        b = b + ('a' - 'A');
    }

    if (a < b) {
        return -1;
    } else if (a > b) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * IUScriptOrder - Compare two script codes for ordering
 *
 * Compares two script codes and returns their relative ordering. This is
 * used for sorting items that may be in different scripts (e.g., sorting
 * a list containing both Roman and Japanese text).
 *
 * Parameters:
 *   aScript - First script code
 *   bScript - Second script code
 *
 * Returns:
 *   < 0 if aScript comes before bScript
 *   0 if scripts are equal
 *   > 0 if aScript comes after bScript
 *
 * Example:
 *   SInt16 order = IUScriptOrder(smRoman, smJapanese);
 *   // Returns negative (Roman sorts before Japanese)
 *
 * Note: Script ordering follows the natural numeric order of script codes.
 * smRoman (0) comes first, followed by smJapanese (1), etc.
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt16 IUScriptOrder(ScriptCode aScript, ScriptCode bScript) {
    ORDER_LOG("IUScriptOrder: aScript=%d, bScript=%d\n", aScript, bScript);

    /* Simple numeric comparison of script codes
     * In a full implementation, this might use script priority tables
     * or locale-specific ordering rules.
     */
    if (aScript < bScript) {
        return -1;
    } else if (aScript > bScript) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * IULangOrder - Compare two language codes for ordering
 *
 * Compares two language codes and returns their relative ordering. This is
 * used for sorting items by language when the script is the same (e.g.,
 * distinguishing between US English and UK English within Roman script).
 *
 * Parameters:
 *   aLang - First language code
 *   bLang - Second language code
 *
 * Returns:
 *   < 0 if aLang comes before bLang
 *   0 if languages are equal
 *   > 0 if aLang comes after bLang
 *
 * Example:
 *   SInt16 order = IULangOrder(langEnglish, langFrench);
 *   // Returns negative (English sorts before French)
 *
 * Note: Language ordering follows the natural numeric order of language codes.
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt16 IULangOrder(LangCode aLang, LangCode bLang) {
    ORDER_LOG("IULangOrder: aLang=%d, bLang=%d\n", aLang, bLang);

    /* Simple numeric comparison of language codes
     * In a full implementation, this might use language priority tables
     * or locale-specific ordering rules.
     */
    if (aLang < bLang) {
        return -1;
    } else if (aLang > bLang) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * IUTextOrder - Compare two text strings with script and language context
 *
 * Compares two text strings considering their script and language codes.
 * This provides culturally-appropriate ordering for international text.
 * For example, Japanese text uses different collation rules than Roman text.
 *
 * Parameters:
 *   aPtr - Pointer to first string (raw bytes)
 *   bPtr - Pointer to second string (raw bytes)
 *   aLen - Length of first string in bytes
 *   bLen - Length of second string in bytes
 *   aScript - Script code for first string
 *   bScript - Script code for second string
 *   aLang - Language code for first string
 *   bLang - Language code for second string
 *
 * Returns:
 *   < 0 if a < b
 *   0 if a == b
 *   > 0 if a > b
 *
 * Example:
 *   SInt16 order = IUTextOrder("Apple", "Banana", 5, 6,
 *                              smRoman, smRoman, langEnglish, langEnglish);
 *   // Returns negative (Apple < Banana)
 *
 * Note: This implementation provides basic case-insensitive comparison.
 * A full implementation would use script-specific collation tables and
 * language-specific sorting rules.
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt16 IUTextOrder(const void* aPtr, const void* bPtr, SInt16 aLen, SInt16 bLen,
                   ScriptCode aScript, ScriptCode bScript, LangCode aLang, LangCode bLang) {
    const UInt8* aBytes;
    const UInt8* bBytes;
    SInt16 minLen;
    SInt16 i;
    SInt16 cmp;

    ORDER_LOG("IUTextOrder: aLen=%d, bLen=%d, aScript=%d, bScript=%d\n",
              aLen, bLen, aScript, bScript);

    if (!aPtr || !bPtr) {
        /* NULL is less than non-NULL */
        if (!aPtr && !bPtr) return 0;
        return aPtr ? 1 : -1;
    }

    /* If scripts differ, order by script */
    if (aScript != bScript) {
        return IUScriptOrder(aScript, bScript);
    }

    /* If languages differ within same script, order by language */
    if (aLang != bLang) {
        return IULangOrder(aLang, bLang);
    }

    /* Same script and language - compare the actual text */
    aBytes = (const UInt8*)aPtr;
    bBytes = (const UInt8*)bPtr;

    /* Compare up to the length of the shorter string */
    minLen = (aLen < bLen) ? aLen : bLen;

    for (i = 0; i < minLen; i++) {
        /* Use case-insensitive comparison for Roman scripts
         * Other scripts would use script-specific collation
         */
        if (aScript == 0) {  /* smRoman */
            cmp = CompareBytesIgnoreCase(aBytes[i], bBytes[i]);
        } else {
            /* For non-Roman scripts, use byte comparison
             * Full implementation would use script-specific collation
             */
            if (aBytes[i] < bBytes[i]) {
                cmp = -1;
            } else if (aBytes[i] > bBytes[i]) {
                cmp = 1;
            } else {
                cmp = 0;
            }
        }

        if (cmp != 0) {
            return cmp;
        }
    }

    /* All compared bytes are equal - shorter string comes first */
    if (aLen < bLen) {
        return -1;
    } else if (aLen > bLen) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * IUStringOrder - Compare two Pascal strings with script and language context
 *
 * Compares two Pascal strings (length byte + data) considering their script
 * and language codes. This is a convenience wrapper around IUTextOrder that
 * handles Pascal string format.
 *
 * Parameters:
 *   aStr - First Pascal string (length byte + data)
 *   bStr - Second Pascal string (length byte + data)
 *   aScript - Script code for first string
 *   bScript - Script code for second string
 *   aLang - Language code for first string
 *   bLang - Language code for second string
 *
 * Returns:
 *   < 0 if aStr < bStr
 *   0 if aStr == bStr
 *   > 0 if aStr > bStr
 *
 * Example:
 *   SInt16 order = IUStringOrder("\x05Apple", "\x06Banana",
 *                                smRoman, smRoman, langEnglish, langEnglish);
 *   // Returns negative (Apple < Banana)
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
SInt16 IUStringOrder(const char* aStr, const char* bStr, ScriptCode aScript, ScriptCode bScript,
                     LangCode aLang, LangCode bLang) {
    UInt8 aLen;
    UInt8 bLen;

    if (!aStr || !bStr) {
        /* NULL is less than non-NULL */
        if (!aStr && !bStr) return 0;
        return aStr ? 1 : -1;
    }

    /* Get string lengths from Pascal format */
    aLen = (UInt8)aStr[0];
    bLen = (UInt8)bStr[0];

    ORDER_LOG("IUStringOrder: aLen=%d, bLen=%d, aScript=%d, bScript=%d\n",
              aLen, bLen, aScript, bScript);

    /* Compare using IUTextOrder (skip length byte) */
    return IUTextOrder(&aStr[1], &bStr[1], aLen, bLen, aScript, bScript, aLang, bLang);
}
