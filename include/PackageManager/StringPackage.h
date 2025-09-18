/*
 * StringPackage.h
 * System 7.1 Portable String Package (International Utilities)
 *
 * Implements Mac OS International Utilities Package (Pack 6) for string processing,
 * comparison, formatting, and international text handling.
 * Critical for proper text display and user interface functionality.
 */

#ifndef __STRING_PACKAGE_H__
#define __STRING_PACKAGE_H__

#include "PackageTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* International Utilities selector constants */
#define iuSelDateString     0
#define iuSelTimeString     2
#define iuSelMetric         4
#define iuSelGetIntl        6
#define iuSelSetIntl        8
#define iuSelMagString      10
#define iuSelMagIDString    12
#define iuSelDatePString    14
#define iuSelTimePString    16
#define iuSelLDateString    20
#define iuSelLTimeString    22
#define iuSelClearCache     24
#define iuSelMagPString     26
#define iuSelMagIDPString   28
#define iuSelScriptOrder    30
#define iuSelLangOrder      32
#define iuSelTextOrder      34
#define iuSelGetItlTable    36

/* Date formatting constants */
typedef enum {
    shortDate = 0,
    longDate = 1,
    abbrevDate = 2
} DateForm;

/* International resource types */
typedef enum {
    iuDateCache = 0,
    iuStringCache = 1,
    iuNumberCache = 2,
    iuStringTable = 3,
    iuTokenTable = 4,
    iuUnTokenTable = 5,
    iuWhiteSpaceList = 6
} IntlResourceType;

/* String comparison result */
typedef enum {
    strLess = -1,
    strEqual = 0,
    strGreater = 1
} StringCompareResult;

/* Text encoding types */
typedef enum {
    smRoman = 0,
    smJapanese = 1,
    smTradChinese = 2,
    smKorean = 3,
    smArabic = 4,
    smHebrew = 5,
    smGreek = 6,
    smCyrillic = 7,
    smRSymbol = 8,
    smDevanagari = 9,
    smGurmukhi = 10,
    smGujarati = 11,
    smOriya = 12,
    smBengali = 13,
    smTamil = 14,
    smTelugu = 15,
    smKannada = 16,
    smMalayalam = 17,
    smSinhalese = 18,
    smBurmese = 19,
    smKhmer = 20,
    smThai = 21,
    smLaotian = 22,
    smGeorgian = 23,
    smArmenian = 24,
    smSimpChinese = 25,
    smTibetan = 26,
    smMongolian = 27,
    smEthiopic = 28,
    smCentralEuroRoman = 29,
    smVietnamese = 30,
    smExtArabic = 31
} ScriptCode;

/* Language codes */
typedef enum {
    langEnglish = 0,
    langFrench = 1,
    langGerman = 2,
    langItalian = 3,
    langDutch = 4,
    langSwedish = 5,
    langSpanish = 6,
    langDanish = 7,
    langPortuguese = 8,
    langNorwegian = 9,
    langHebrew = 10,
    langJapanese = 11,
    langArabic = 12,
    langFinnish = 13,
    langGreek = 14,
    langIcelandic = 15,
    langMaltese = 16,
    langTurkish = 17,
    langCroatian = 18,
    langTradChinese = 19,
    langUrdu = 20,
    langHindi = 21,
    langThai = 22,
    langKorean = 23,
    langLithuanian = 24,
    langPolish = 25,
    langHungarian = 26,
    langEstonian = 27,
    langLatvian = 28,
    langSami = 29,
    langFaroese = 30,
    langFarsi = 31,
    langRussian = 32,
    langSimpChinese = 33,
    langFlemish = 34,
    langIrishGaelic = 35,
    langAlbanian = 36,
    langRomanian = 37,
    langCzech = 38,
    langSlovak = 39,
    langSlovenian = 40,
    langYiddish = 41,
    langSerbian = 42,
    langMacedonian = 43,
    langBulgarian = 44,
    langUkrainian = 45,
    langByelorussian = 46,
    langUzbek = 47,
    langKazakh = 48,
    langAzerbaijani = 49,
    langAzerbaijanAr = 50,
    langArmenian = 51,
    langGeorgian = 52,
    langMoldavian = 53,
    langKirghiz = 54,
    langTajiki = 55,
    langTurkmen = 56,
    langMongolian = 57,
    langMongolianCyr = 58,
    langPashto = 59,
    langKurdish = 60,
    langKashmiri = 61,
    langSindhi = 62,
    langTibetan = 63,
    langNepali = 64,
    langSanskrit = 65,
    langMarathi = 66,
    langBengali = 67,
    langAssamese = 68,
    langGujarati = 69,
    langPunjabi = 70
} LangCode;

/* International resource structure */
typedef struct {
    int16_t     decimalPt;          /* Decimal point character */
    int16_t     thousSep;           /* Thousands separator */
    int16_t     listSep;            /* List separator */
    int16_t     currSym1;           /* Currency symbol 1 */
    int16_t     currSym2;           /* Currency symbol 2 */
    int16_t     currSym3;           /* Currency symbol 3 */
    uint8_t     currFmt;            /* Currency format */
    uint8_t     dateOrder;          /* Date order */
    uint8_t     shrtDateFmt;        /* Short date format */
    int8_t      dateSep;            /* Date separator */
    uint8_t     timeCycle;          /* Time cycle (12/24 hour) */
    uint8_t     timeFmt;            /* Time format */
    int8_t      mornStr[4];         /* Morning string */
    int8_t      eveStr[4];          /* Evening string */
    int8_t      timeSep;            /* Time separator */
    int8_t      time1Suff;          /* Time suffix 1 */
    int8_t      time2Suff;          /* Time suffix 2 */
    int8_t      time3Suff;          /* Time suffix 3 */
    int8_t      time4Suff;          /* Time suffix 4 */
    int8_t      time5Suff;          /* Time suffix 5 */
    int8_t      time6Suff;          /* Time suffix 6 */
    int8_t      time7Suff;          /* Time suffix 7 */
    int8_t      time8Suff;          /* Time suffix 8 */
    uint8_t     metricSys;          /* Metric system flag */
    int16_t     intl0Vers;          /* International resource version */
} Intl0Rec, *Intl0Ptr, **Intl0Hndl;

/* String Package API Functions */

/* Package initialization and management */
int32_t InitStringPackage(void);
void CleanupStringPackage(void);
int32_t StringDispatch(int16_t selector, void *params);

/* String comparison functions */
int16_t IUMagString(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen);
int16_t IUMagIDString(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen);
int16_t IUCompString(const char *aStr, const char *bStr);
int16_t IUEqualString(const char *aStr, const char *bStr);

/* International string comparison */
int16_t IUMagPString(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen, Handle intlParam);
int16_t IUMagIDPString(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen, Handle intlParam);
int16_t IUCompPString(const char *aStr, const char *bStr, Handle intlParam);
int16_t IUEqualPString(const char *aStr, const char *bStr, Handle intlParam);

/* Script and language ordering */
int16_t IUScriptOrder(ScriptCode aScript, ScriptCode bScript);
int16_t IULangOrder(LangCode aLang, LangCode bLang);
int16_t IUTextOrder(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen,
                    ScriptCode aScript, ScriptCode bScript, LangCode aLang, LangCode bLang);
int16_t IUStringOrder(const char *aStr, const char *bStr, ScriptCode aScript, ScriptCode bScript,
                      LangCode aLang, LangCode bLang);

/* Date and time formatting */
void IUDateString(uint32_t dateTime, DateForm longFlag, char *result);
void IUTimeString(uint32_t dateTime, Boolean wantSeconds, char *result);
void IUDatePString(uint32_t dateTime, DateForm longFlag, char *result, Handle intlParam);
void IUTimePString(uint32_t dateTime, Boolean wantSeconds, char *result, Handle intlParam);
void IULDateString(const LongDateTime *dateTime, DateForm longFlag, char *result, Handle intlParam);
void IULTimeString(const LongDateTime *dateTime, Boolean wantSeconds, char *result, Handle intlParam);

/* International resource management */
Handle IUGetIntl(int16_t theID);
void IUSetIntl(int16_t refNum, int16_t theID, const void *intlParam);
Boolean IUMetric(void);
void IUClearCache(void);
Handle IUGetItlTable(ScriptCode script, int16_t tableCode, Handle *itlHandle, int32_t *offset, int32_t *length);

/* String utility functions */
void StringToNum(const char *theString, int32_t *theNum);
void NumToString(int32_t theNum, char *theString);
int32_t StringWidth(const char *theString);
void TruncString(int16_t width, char *theString, int16_t truncWhere);

/* Character classification and conversion */
Boolean IsLower(char ch);
Boolean IsUpper(char ch);
char ToLower(char ch);
char ToUpper(char ch);
Boolean IsAlpha(char ch);
Boolean IsDigit(char ch);
Boolean IsAlphaNum(char ch);
Boolean IsSpace(char ch);
Boolean IsPunct(char ch);

/* String manipulation */
void CopyString(const char *source, char *dest, int16_t maxLen);
void ConcatString(const char *source, char *dest, int16_t maxLen);
int16_t FindString(const char *searchIn, const char *searchFor, int16_t startPos);
void ReplaceString(char *theString, const char *oldStr, const char *newStr);
void TrimString(char *theString);

/* Pascal/C string conversion */
void C2PStr(char *cString);
void P2CStr(char *pString);
void CopyC2PStr(const char *cString, char *pString);
void CopyP2CStr(const char *pString, char *cString);

/* Text encoding and conversion */
int32_t TextEncodingToScript(int32_t encoding);
int32_t ScriptToTextEncoding(ScriptCode script, LangCode language);
OSErr ConvertFromTextEncoding(int32_t srcEncoding, int32_t dstEncoding,
                             const void *srcText, int32_t srcLen,
                             void *dstText, int32_t dstMaxLen, int32_t *dstLen);

/* International utilities configuration */
void SetStringPackageScript(ScriptCode script);
ScriptCode GetStringPackageScript(void);
void SetStringPackageLanguage(LangCode language);
LangCode GetStringPackageLanguage(void);

/* String sorting and collation */
typedef struct {
    const char *str;
    int16_t     length;
    void       *userData;
} StringSortEntry;

void SortStringArray(StringSortEntry *entries, int16_t count,
                     ScriptCode script, LangCode language);
int16_t CompareStringEntries(const StringSortEntry *a, const StringSortEntry *b,
                            ScriptCode script, LangCode language);

/* Token parsing */
typedef struct {
    char       separator;
    Boolean    skipEmpty;
    Boolean    trimSpaces;
    int16_t    maxTokens;
} TokenizeOptions;

int16_t TokenizeString(const char *input, char **tokens, int16_t maxTokens,
                      const TokenizeOptions *options);
void FreeTokenArray(char **tokens, int16_t count);

/* String formatting */
int32_t FormatString(char *output, int32_t maxLen, const char *format, ...);
int32_t FormatStringArgs(char *output, int32_t maxLen, const char *format, va_list args);

/* Localized string resources */
Handle GetLocalizedString(int16_t stringID, ScriptCode script, LangCode language);
void GetLocalizedCString(int16_t stringID, char *buffer, int16_t maxLen,
                        ScriptCode script, LangCode language);

/* String validation and sanitization */
Boolean ValidateStringEncoding(const char *str, int16_t len, ScriptCode script);
void SanitizeString(char *str, int16_t maxLen, ScriptCode script);
Boolean IsValidFilename(const char *filename, ScriptCode script);

/* Platform integration */
void SetPlatformStringCallbacks(void *callbacks);
void EnableUnicodeSupport(Boolean enabled);
void SetDefaultSystemScript(ScriptCode script);
void SetDefaultSystemLanguage(LangCode language);

#ifdef __cplusplus
}
#endif

#endif /* __STRING_PACKAGE_H__ */