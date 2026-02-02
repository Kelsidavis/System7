/*
 * LocaleManager.h - Locale Management and Localized String Access
 *
 * Provides runtime locale selection and localized string retrieval.
 * Strings are loaded from STR# resources in per-language resource files.
 *
 * Based on Inside Macintosh: Text internationalization model.
 */

#ifndef LOCALE_MANAGER_H
#define LOCALE_MANAGER_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Locale Identifiers ------------------------------------------------- */

typedef struct LocaleRef {
    ScriptCode script;
    LangCode   language;
    SInt16     regionCode;
} LocaleRef;

/* Predefined locale IDs (used with SetCurrentLocaleByID) */
#define kLocaleIDEnglish    0
#define kLocaleIDFrench     1
#define kLocaleIDGerman     2
#define kLocaleIDSpanish    3
#define kLocaleIDJapanese   4
#define kLocaleIDSimpChinese 5
#define kLocaleIDKorean     6
#define kLocaleIDRussian    7
#define kLocaleIDUkrainian  8
#define kLocaleIDPolish     9
#define kLocaleIDCzech      10
#define kLocaleIDAlbanian   11
#define kLocaleIDBulgarian  12
#define kLocaleIDCroatian   13
#define kLocaleIDDanish     14
#define kLocaleIDDutch      15
#define kLocaleIDEstonian   16
#define kLocaleIDFinnish    17
#define kLocaleIDGreek      18
#define kLocaleIDHungarian  19
#define kLocaleIDIcelandic  20
#define kLocaleIDItalian    21
#define kLocaleIDLatvian    22
#define kLocaleIDLithuanian 23
#define kLocaleIDMacedonian 24
#define kLocaleIDMontenegrin 25
#define kLocaleIDNorwegian  26
#define kLocaleIDPortuguese 27
#define kLocaleIDRomanian   28
#define kLocaleIDSlovak     29
#define kLocaleIDSlovenian  30
#define kLocaleIDSwedish    31
#define kLocaleIDTurkish    32
#define kLocaleIDHindi      33
#define kLocaleIDTradChinese 34
#define kLocaleIDArabic     35
#define kLocaleIDBengali    36
#define kLocaleIDUrdu       37

#define kLocaleCount        38

/* ---- Initialization ----------------------------------------------------- */

/* Initialize the Locale Manager. Call after InitResourceManager().
 * Defaults to English. Parses boot command line for lang= parameter. */
OSErr InitLocaleManager(void);

/* ---- Locale Selection --------------------------------------------------- */

/* Set the active locale by ID */
void SetCurrentLocaleByID(SInt16 localeID);

/* Get the active locale ID */
SInt16 GetCurrentLocaleID(void);

/* Get the active locale details */
LocaleRef GetCurrentLocale(void);

/* Get locale info by ID */
LocaleRef GetLocaleInfo(SInt16 localeID);

/* Get the two-letter language code string for a locale (e.g. "en", "fr") */
const char* GetLocaleCode(SInt16 localeID);

/* ---- Localized String Access -------------------------------------------- */

/* Get a localized Pascal string for the current locale.
 * Falls back to English if the string is not found in the current locale.
 *
 * Parameters:
 *   outString  - Buffer for Pascal string (Str255, at least 256 bytes)
 *   strListID  - STR# resource ID (from StringIDs.h)
 *   index      - 1-based string index within the list
 */
void GetLocalizedString(StringPtr outString, SInt16 strListID, SInt16 index);

/* Get a localized C string (null-terminated) for the current locale.
 * Returns pointer to internal static buffer. Not reentrant.
 *
 * Parameters:
 *   strListID  - STR# resource ID (from StringIDs.h)
 *   index      - 1-based string index within the list
 *
 * Returns:
 *   Pointer to null-terminated string, or "" if not found.
 */
const char* GetLocalizedCString(SInt16 strListID, SInt16 index);

/* ---- Resource Management ------------------------------------------------ */

/* Get the embedded resource data for a locale.
 * Used internally to load locale-specific STR# resources. */
const unsigned char* GetLocaleResourceData(SInt16 localeID);
UInt32 GetLocaleResourceSize(SInt16 localeID);

#ifdef __cplusplus
}
#endif

#endif /* LOCALE_MANAGER_H */
