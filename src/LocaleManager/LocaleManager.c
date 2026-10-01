/*
 * LocaleManager.c - Locale Management and Localized String Access
 *
 * Manages runtime locale selection and provides localized string retrieval
 * via STR# resources loaded from per-language embedded resource data.
 *
 * Based on Inside Macintosh: Text internationalization model.
 */

#include "LocaleManager/LocaleManager.h"
#include "LocaleManager/StringIDs.h"
#include "ResourceManager.h"
#include "System71StdLib.h"
#include <string.h>

/* Debug logging */
#define LOCALE_DEBUG 1

#if LOCALE_DEBUG
#define LOCALE_LOG(...) do { serial_printf("[Locale] " __VA_ARGS__); } while(0)
#else
#define LOCALE_LOG(...) ((void)0)
#endif

/* Script codes (matching TextEncodingUtils.c) */
#ifndef smRoman
#define smRoman         0
#define smJapanese      1
#define smTradChinese   2
#define smKorean        3
#define smSimpChinese   25
#endif

/* Language codes */
#ifndef langEnglish
#define langEnglish     0
#define langFrench      1
#define langGerman      2
#define langSpanish     6
#define langJapanese    11
#define langKorean      23
#define langChinese     33
#define langRussian     32
#define langUkrainian   45
#define langPolish      25
#define langCzech       38
#define langAlbanian    36
#define langBulgarian   44
#define langCroatian    18
#define langDanish      7
#define langDutch       4
#define langEstonian    27
#define langFinnish     13
#define langGreek       14
#define langHungarian   26
#define langIcelandic   15
#define langItalian     3
#define langLatvian     28
#define langLithuanian  24
#define langMacedonian  43
#define langMontenegrin 100  /* No official Mac OS code; custom */
#define langNorwegian   9
#define langPortuguese  10
#define langRomanian    37
#define langSlovak      39
#define langSlovenian   40
#define langSwedish     5
#define langTurkish     17
#define langHindi       21
#define langTradChinese 19
#define langArabic      12
#define langBengali     67
#define langUrdu        20
#endif

/* Script codes for Cyrillic */
#ifndef smCyrillic
#define smCyrillic      7
#endif

/* Script codes for Central European */
#ifndef smCentralEuroRoman
#define smCentralEuroRoman 29
#endif

/* Script codes for Greek */
#ifndef smGreek
#define smGreek         6
#endif

/* Script codes for Devanagari */
#ifndef smDevanagari
#define smDevanagari    9
#endif

/* Script codes for Arabic */
#ifndef smArabic
#define smArabic        4
#endif

/* Script codes for Bengali */
#ifndef smBengali
#define smBengali       13
#endif

/* ---- Locale Table ------------------------------------------------------- */

typedef struct {
    LocaleRef   locale;
    const char* code;       /* Two-letter ISO code */
    const char* name;       /* Display name (English) */
} LocaleEntry;

static const LocaleEntry gLocaleTable[kLocaleCount] = {
    /* kLocaleIDEnglish */
    { { smRoman, langEnglish, 0 },     "en", "English" },
    /* kLocaleIDFrench */
    { { smRoman, langFrench, 1 },      "fr", "French" },
    /* kLocaleIDGerman */
    { { smRoman, langGerman, 2 },      "de", "German" },
    /* kLocaleIDSpanish */
    { { smRoman, langSpanish, 6 },     "es", "Spanish" },
    /* kLocaleIDJapanese */
    { { smJapanese, langJapanese, 14 }, "ja", "Japanese" },
    /* kLocaleIDSimpChinese */
    { { smSimpChinese, langChinese, 52 }, "zh", "Chinese" },
    /* kLocaleIDKorean */
    { { smKorean, langKorean, 51 },    "ko", "Korean" },
    /* kLocaleIDRussian */
    { { smCyrillic, langRussian, 49 }, "ru", "Russian" },
    /* kLocaleIDUkrainian */
    { { smCyrillic, langUkrainian, 62 }, "uk", "Ukrainian" },
    /* kLocaleIDPolish */
    { { smCentralEuroRoman, langPolish, 30 }, "pl", "Polish" },
    /* kLocaleIDCzech */
    { { smCentralEuroRoman, langCzech, 56 }, "cs", "Czech" },
    /* kLocaleIDAlbanian */
    { { smRoman, langAlbanian, 0 }, "sq", "Albanian" },
    /* kLocaleIDBulgarian */
    { { smCyrillic, langBulgarian, 0 }, "bg", "Bulgarian" },
    /* kLocaleIDCroatian */
    { { smCentralEuroRoman, langCroatian, 0 }, "hr", "Croatian" },
    /* kLocaleIDDanish */
    { { smRoman, langDanish, 0 }, "da", "Danish" },
    /* kLocaleIDDutch */
    { { smRoman, langDutch, 0 }, "nl", "Dutch" },
    /* kLocaleIDEstonian */
    { { smCentralEuroRoman, langEstonian, 0 }, "et", "Estonian" },
    /* kLocaleIDFinnish */
    { { smRoman, langFinnish, 0 }, "fi", "Finnish" },
    /* kLocaleIDGreek */
    { { smGreek, langGreek, 0 }, "el", "Greek" },
    /* kLocaleIDHungarian */
    { { smCentralEuroRoman, langHungarian, 0 }, "hu", "Hungarian" },
    /* kLocaleIDIcelandic */
    { { smRoman, langIcelandic, 0 }, "is", "Icelandic" },
    /* kLocaleIDItalian */
    { { smRoman, langItalian, 0 }, "it", "Italian" },
    /* kLocaleIDLatvian */
    { { smCentralEuroRoman, langLatvian, 0 }, "lv", "Latvian" },
    /* kLocaleIDLithuanian */
    { { smCentralEuroRoman, langLithuanian, 0 }, "lt", "Lithuanian" },
    /* kLocaleIDMacedonian */
    { { smCyrillic, langMacedonian, 0 }, "mk", "Macedonian" },
    /* kLocaleIDMontenegrin */
    { { smCentralEuroRoman, langMontenegrin, 0 }, "me", "Montenegrin" },
    /* kLocaleIDNorwegian */
    { { smRoman, langNorwegian, 0 }, "no", "Norwegian" },
    /* kLocaleIDPortuguese */
    { { smRoman, langPortuguese, 0 }, "pt", "Portuguese" },
    /* kLocaleIDRomanian */
    { { smCentralEuroRoman, langRomanian, 0 }, "ro", "Romanian" },
    /* kLocaleIDSlovak */
    { { smCentralEuroRoman, langSlovak, 0 }, "sk", "Slovak" },
    /* kLocaleIDSlovenian */
    { { smCentralEuroRoman, langSlovenian, 0 }, "sl", "Slovenian" },
    /* kLocaleIDSwedish */
    { { smRoman, langSwedish, 0 }, "sv", "Swedish" },
    /* kLocaleIDTurkish */
    { { smRoman, langTurkish, 0 }, "tr", "Turkish" },
    /* kLocaleIDHindi */
    { { smDevanagari, langHindi, 0 }, "hi", "Hindi" },
    /* kLocaleIDTradChinese */
    { { smTradChinese, langTradChinese, 0 }, "tw", "Traditional Chinese" },
    /* kLocaleIDArabic */
    { { smArabic, langArabic, 0 }, "ar", "Arabic" },
    /* kLocaleIDBengali */
    { { smBengali, langBengali, 0 }, "bn", "Bengali" },
    /* kLocaleIDUrdu */
    { { smArabic, langUrdu, 0 }, "ur", "Urdu" },
};

/* ---- Embedded Resource Data (generated by build system) ----------------- */

/* English is always available as the fallback */
extern const unsigned char strings_en_rsrc_data[];
extern const unsigned int  strings_en_rsrc_size;






































/* ---- State -------------------------------------------------------------- */

static SInt16 gCurrentLocaleID = kLocaleIDEnglish;
static SInt16 gLocaleResFileRef = -1;   /* RefNum for current locale resource file */
static SInt16 gEnglishResFileRef = -1;  /* RefNum for English fallback */
static Boolean gLocaleInited = false;

/* ---- Internal: Get resource data for a locale ID ------------------------ */

static const unsigned char* GetLocaleData(SInt16 localeID, UInt32* outSize) {
    switch (localeID) {
        case kLocaleIDEnglish:
            *outSize = strings_en_rsrc_size;
            return strings_en_rsrc_data;
        default:
            *outSize = 0;
            return NULL;
    }
}

/* ---- Internal: Parse lang= from boot command line ----------------------- */

/* Global set by multiboot2 parsing in main.c */
const char* g_boot_cmdline = NULL;

static SInt16 ParseBootLocale(void) {
    if (!g_boot_cmdline) {
        return kLocaleIDEnglish;
    }

    /* Search for "lang=" parameter */
    const char* p = g_boot_cmdline;
    while (*p) {
        if (p[0] == 'l' && p[1] == 'a' && p[2] == 'n' && p[3] == 'g' && p[4] == '=') {
            const char* code = p + 5;
            /* Match against locale table */
            for (SInt16 i = 0; i < kLocaleCount; i++) {
                const char* lc = gLocaleTable[i].code;
                if (code[0] == lc[0] && code[1] == lc[1] &&
                    (code[2] == '\0' || code[2] == ' ')) {
                    return i;
                }
            }
            break;
        }
        p++;
    }

    return kLocaleIDEnglish;
}

/* ---- Public API --------------------------------------------------------- */

OSErr InitLocaleManager(void) {
    UInt32 enSize = 0;

    LOCALE_LOG("Initializing Locale Manager\n");

    /* Always register English as fallback */
    const unsigned char* enData = GetLocaleData(kLocaleIDEnglish, &enSize);
    if (!enData || enSize == 0) {
        LOCALE_LOG("ERROR: English locale data not available\n");
        return -1;
    }

    gEnglishResFileRef = OpenResMemory(enData, enSize);
    if (gEnglishResFileRef < 0) {
        LOCALE_LOG("ERROR: Failed to register English resources\n");
        return -1;
    }

    /* Detect locale from boot parameters */
    SInt16 bootLocale = ParseBootLocale();
    LOCALE_LOG("Boot locale: %s (%d)\n", gLocaleTable[bootLocale].name, bootLocale);

    /* Set the locale (registers the locale's resource file if not English) */
    gLocaleInited = true;
    SetCurrentLocaleByID(bootLocale);

    LOCALE_LOG("Locale Manager initialized (locale=%s)\n",
               gLocaleTable[gCurrentLocaleID].name);

    return noErr;
}

void SetCurrentLocaleByID(SInt16 localeID) {
    UInt32 dataSize = 0;

    if (localeID < 0 || localeID >= kLocaleCount) {
        LOCALE_LOG("SetCurrentLocaleByID: invalid ID %d\n", localeID);
        return;
    }

    /* Close previous non-English locale resource file */
    if (gLocaleResFileRef >= 0 && gLocaleResFileRef != gEnglishResFileRef) {
        CloseResMemory(gLocaleResFileRef);
        gLocaleResFileRef = -1;
    }

    gCurrentLocaleID = localeID;

    if (localeID == kLocaleIDEnglish) {
        gLocaleResFileRef = gEnglishResFileRef;
    } else {
        /* Try to load locale-specific resources */
        const unsigned char* locData = GetLocaleData(localeID, &dataSize);
        if (locData && dataSize > 0) {
            gLocaleResFileRef = OpenResMemory(locData, dataSize);
            if (gLocaleResFileRef < 0) {
                LOCALE_LOG("SetCurrentLocaleByID: failed to register locale %d, using English\n",
                           localeID);
                gLocaleResFileRef = gEnglishResFileRef;
            }
        } else {
            LOCALE_LOG("SetCurrentLocaleByID: no data for locale %d, using English\n", localeID);
            gLocaleResFileRef = gEnglishResFileRef;
        }
    }

    /* Update the text encoding system */
    extern void SetStringPackageScript(ScriptCode script);
    extern void SetStringPackageLanguage(LangCode language);
    SetStringPackageScript(gLocaleTable[localeID].locale.script);
    SetStringPackageLanguage(gLocaleTable[localeID].locale.language);
}

SInt16 GetCurrentLocaleID(void) {
    return gCurrentLocaleID;
}

LocaleRef GetCurrentLocale(void) {
    return gLocaleTable[gCurrentLocaleID].locale;
}

LocaleRef GetLocaleInfo(SInt16 localeID) {
    if (localeID < 0 || localeID >= kLocaleCount) {
        return gLocaleTable[kLocaleIDEnglish].locale;
    }
    return gLocaleTable[localeID].locale;
}

const char* GetLocaleCode(SInt16 localeID) {
    if (localeID < 0 || localeID >= kLocaleCount) {
        return "en";
    }
    return gLocaleTable[localeID].code;
}

void GetLocalizedString(StringPtr outString, SInt16 strListID, SInt16 index) {
    SInt16 savedResFile;

    if (!outString) {
        return;
    }

    /* Initialize to empty */
    outString[0] = 0;

    if (!gLocaleInited) {
        return;
    }

    savedResFile = CurResFile();

    /* First try the current locale's resource file */
    if (gLocaleResFileRef >= 0) {
        UseResFile(gLocaleResFileRef);
        GetIndString(outString, strListID, index);
    }

    /* If not found and current locale is not English, fall back to English */
    if (outString[0] == 0 && gCurrentLocaleID != kLocaleIDEnglish &&
        gEnglishResFileRef >= 0) {
        UseResFile(gEnglishResFileRef);
        GetIndString(outString, strListID, index);
    }

    /* Restore previous resource file context */
    UseResFile(savedResFile);
}

const char* GetLocalizedCString(SInt16 strListID, SInt16 index) {
    static Str255 sBuf;
    static char sCBuf[256];

    GetLocalizedString(sBuf, strListID, index);

    /* Convert Pascal string to C string */
    UInt8 len = sBuf[0];
    if (len > 0) {
        memcpy(sCBuf, &sBuf[1], len);
    }
    sCBuf[len] = '\0';

    return sCBuf;
}

const unsigned char* GetLocaleResourceData(SInt16 localeID) {
    UInt32 size = 0;
    return GetLocaleData(localeID, &size);
}

UInt32 GetLocaleResourceSize(SInt16 localeID) {
    UInt32 size = 0;
    GetLocaleData(localeID, &size);
    return size;
}
