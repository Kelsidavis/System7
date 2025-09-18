/*
 * StringPackage.c
 * System 7.1 Portable String Package (International Utilities) Implementation
 *
 * Implements Mac OS International Utilities Package (Pack 6) for string processing,
 * comparison, formatting, and international text handling.
 * Essential for proper text display and user interface functionality.
 */

#include "PackageManager/StringPackage.h"
#include "PackageManager/PackageTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <locale.h>
#include <time.h>

/* String package state */
static struct {
    ScriptCode      currentScript;
    LangCode        currentLanguage;
    Boolean         metricSystem;
    Boolean         unicodeSupport;
    Intl0Hndl       intlResource;
    void           *platformCallbacks;
    Handle          stringCache[8];
    Boolean         initialized;
} g_stringState = {
    .currentScript = smRoman,
    .currentLanguage = langEnglish,
    .metricSystem = false,
    .unicodeSupport = false,
    .intlResource = NULL,
    .platformCallbacks = NULL,
    .stringCache = {NULL},
    .initialized = false
};

/* Default international resource */
static Intl0Rec g_defaultIntl0 = {
    .decimalPt = '.',
    .thousSep = ',',
    .listSep = ',',
    .currSym1 = '$',
    .currSym2 = 0,
    .currSym3 = 0,
    .currFmt = 0,
    .dateOrder = 0, /* month/day/year */
    .shrtDateFmt = 0,
    .dateSep = '/',
    .timeCycle = 0, /* 12 hour */
    .timeFmt = 0,
    .mornStr = {'A','M',0,0},
    .eveStr = {'P','M',0,0},
    .timeSep = ':',
    .time1Suff = 0,
    .time2Suff = 0,
    .time3Suff = 0,
    .time4Suff = 0,
    .time5Suff = 0,
    .time6Suff = 0,
    .time7Suff = 0,
    .time8Suff = 0,
    .metricSys = 0,
    .intl0Vers = 0
};

/* Forward declarations */
static void InitializeDefaultResources(void);
static int16_t CompareStringData(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen,
                                Boolean caseSensitive, Boolean diacriticalSensitive);
static void FormatDateTime(uint32_t dateTime, DateForm dateFormat, Boolean wantSeconds,
                          Boolean isTime, char *result, Handle intlParam);
static Boolean IsLeapYear(int16_t year);
static void SecondsToDateTime(uint32_t seconds, int16_t *year, int16_t *month, int16_t *day,
                             int16_t *hour, int16_t *minute, int16_t *second);

/**
 * Initialize String Package
 */
int32_t InitStringPackage(void)
{
    if (g_stringState.initialized) {
        return PACKAGE_NO_ERROR;
    }

    /* Initialize default values */
    g_stringState.currentScript = smRoman;
    g_stringState.currentLanguage = langEnglish;
    g_stringState.metricSystem = false;
    g_stringState.unicodeSupport = false;

    /* Initialize default international resources */
    InitializeDefaultResources();

    /* Set system locale if available */
    setlocale(LC_ALL, "");

    g_stringState.initialized = true;
    return PACKAGE_NO_ERROR;
}

/**
 * String package dispatch function
 */
int32_t StringDispatch(int16_t selector, void *params)
{
    if (!g_stringState.initialized) {
        InitStringPackage();
    }

    if (!params) {
        return PACKAGE_INVALID_PARAMS;
    }

    switch (selector) {
        case iuSelMagString: {
            void **args = (void**)params;
            const void *aPtr = args[0];
            const void *bPtr = args[1];
            int16_t aLen = *(int16_t*)args[2];
            int16_t bLen = *(int16_t*)args[3];
            int16_t result = IUMagString(aPtr, bPtr, aLen, bLen);
            *(int16_t*)args[4] = result;
            return PACKAGE_NO_ERROR;
        }

        case iuSelMagIDString: {
            void **args = (void**)params;
            const void *aPtr = args[0];
            const void *bPtr = args[1];
            int16_t aLen = *(int16_t*)args[2];
            int16_t bLen = *(int16_t*)args[3];
            int16_t result = IUMagIDString(aPtr, bPtr, aLen, bLen);
            *(int16_t*)args[4] = result;
            return PACKAGE_NO_ERROR;
        }

        case iuSelDateString: {
            void **args = (void**)params;
            uint32_t dateTime = *(uint32_t*)args[0];
            DateForm longFlag = *(DateForm*)args[1];
            char *result = (char*)args[2];
            IUDateString(dateTime, longFlag, result);
            return PACKAGE_NO_ERROR;
        }

        case iuSelTimeString: {
            void **args = (void**)params;
            uint32_t dateTime = *(uint32_t*)args[0];
            Boolean wantSeconds = *(Boolean*)args[1];
            char *result = (char*)args[2];
            IUTimeString(dateTime, wantSeconds, result);
            return PACKAGE_NO_ERROR;
        }

        case iuSelMetric: {
            Boolean *result = (Boolean*)params;
            *result = IUMetric();
            return PACKAGE_NO_ERROR;
        }

        case iuSelGetIntl: {
            void **args = (void**)params;
            int16_t theID = *(int16_t*)args[0];
            Handle *result = (Handle*)args[1];
            *result = IUGetIntl(theID);
            return PACKAGE_NO_ERROR;
        }

        default:
            return PACKAGE_INVALID_SELECTOR;
    }
}

/**
 * String comparison functions
 */
int16_t IUMagString(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen)
{
    return CompareStringData(aPtr, bPtr, aLen, bLen, false, false);
}

int16_t IUMagIDString(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen)
{
    return CompareStringData(aPtr, bPtr, aLen, bLen, false, true);
}

int16_t IUCompString(const char *aStr, const char *bStr)
{
    if (!aStr || !bStr) {
        return (aStr == bStr) ? 0 : (aStr ? 1 : -1);
    }

    int16_t aLen = strlen(aStr);
    int16_t bLen = strlen(bStr);
    return CompareStringData(aStr, bStr, aLen, bLen, false, false);
}

int16_t IUEqualString(const char *aStr, const char *bStr)
{
    return (IUCompString(aStr, bStr) == 0) ? 1 : 0;
}

/**
 * International string comparison with parameters
 */
int16_t IUMagPString(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen, Handle intlParam)
{
    /* Use international parameters if provided */
    return CompareStringData(aPtr, bPtr, aLen, bLen, false, false);
}

int16_t IUMagIDPString(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen, Handle intlParam)
{
    /* Use international parameters if provided */
    return CompareStringData(aPtr, bPtr, aLen, bLen, false, true);
}

int16_t IUCompPString(const char *aStr, const char *bStr, Handle intlParam)
{
    if (!aStr || !bStr) {
        return (aStr == bStr) ? 0 : (aStr ? 1 : -1);
    }

    int16_t aLen = strlen(aStr);
    int16_t bLen = strlen(bStr);
    return IUMagPString(aStr, bStr, aLen, bLen, intlParam);
}

int16_t IUEqualPString(const char *aStr, const char *bStr, Handle intlParam)
{
    return (IUCompPString(aStr, bStr, intlParam) == 0) ? 1 : 0;
}

/**
 * Script and language ordering
 */
int16_t IUScriptOrder(ScriptCode aScript, ScriptCode bScript)
{
    if (aScript < bScript) return -1;
    if (aScript > bScript) return 1;
    return 0;
}

int16_t IULangOrder(LangCode aLang, LangCode bLang)
{
    if (aLang < bLang) return -1;
    if (aLang > bLang) return 1;
    return 0;
}

int16_t IUTextOrder(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen,
                    ScriptCode aScript, ScriptCode bScript, LangCode aLang, LangCode bLang)
{
    /* First compare by script */
    int16_t scriptOrder = IUScriptOrder(aScript, bScript);
    if (scriptOrder != 0) {
        return scriptOrder;
    }

    /* Then by language */
    int16_t langOrder = IULangOrder(aLang, bLang);
    if (langOrder != 0) {
        return langOrder;
    }

    /* Finally by string content */
    return CompareStringData(aPtr, bPtr, aLen, bLen, false, false);
}

int16_t IUStringOrder(const char *aStr, const char *bStr, ScriptCode aScript, ScriptCode bScript,
                      LangCode aLang, LangCode bLang)
{
    if (!aStr || !bStr) {
        return (aStr == bStr) ? 0 : (aStr ? 1 : -1);
    }

    int16_t aLen = strlen(aStr);
    int16_t bLen = strlen(bStr);
    return IUTextOrder(aStr, bStr, aLen, bLen, aScript, bScript, aLang, bLang);
}

/**
 * Date and time formatting
 */
void IUDateString(uint32_t dateTime, DateForm longFlag, char *result)
{
    FormatDateTime(dateTime, longFlag, false, false, result, NULL);
}

void IUTimeString(uint32_t dateTime, Boolean wantSeconds, char *result)
{
    FormatDateTime(dateTime, shortDate, wantSeconds, true, result, NULL);
}

void IUDatePString(uint32_t dateTime, DateForm longFlag, char *result, Handle intlParam)
{
    FormatDateTime(dateTime, longFlag, false, false, result, intlParam);
}

void IUTimePString(uint32_t dateTime, Boolean wantSeconds, char *result, Handle intlParam)
{
    FormatDateTime(dateTime, shortDate, wantSeconds, true, result, intlParam);
}

void IULDateString(const LongDateTime *dateTime, DateForm longFlag, char *result, Handle intlParam)
{
    if (!dateTime || !result) return;

    /* Convert LongDateTime to seconds since epoch */
    uint32_t seconds = dateTime->year * 365 * 24 * 3600; /* Simplified conversion */
    FormatDateTime(seconds, longFlag, false, false, result, intlParam);
}

void IULTimeString(const LongDateTime *dateTime, Boolean wantSeconds, char *result, Handle intlParam)
{
    if (!dateTime || !result) return;

    /* Convert LongDateTime to seconds since epoch */
    uint32_t seconds = dateTime->hour * 3600 + dateTime->minute * 60 + dateTime->second;
    FormatDateTime(seconds, shortDate, wantSeconds, true, result, intlParam);
}

/**
 * International resource management
 */
Handle IUGetIntl(int16_t theID)
{
    if (theID == 0) {
        /* Return international resource 0 (date/time/number formats) */
        if (!g_stringState.intlResource) {
            g_stringState.intlResource = (Intl0Hndl)malloc(sizeof(Intl0Rec));
            if (g_stringState.intlResource) {
                *g_stringState.intlResource = g_defaultIntl0;
            }
        }
        return (Handle)g_stringState.intlResource;
    }

    /* Return cached resource if available */
    if (theID >= 0 && theID < 8 && g_stringState.stringCache[theID]) {
        return g_stringState.stringCache[theID];
    }

    return NULL;
}

void IUSetIntl(int16_t refNum, int16_t theID, const void *intlParam)
{
    if (theID == 0 && intlParam) {
        /* Update international resource 0 */
        if (g_stringState.intlResource) {
            memcpy(g_stringState.intlResource, intlParam, sizeof(Intl0Rec));
        }
    }
}

Boolean IUMetric(void)
{
    if (g_stringState.intlResource) {
        return (*g_stringState.intlResource)->metricSys != 0;
    }
    return g_stringState.metricSystem;
}

void IUClearCache(void)
{
    /* Clear all cached international resources */
    for (int i = 0; i < 8; i++) {
        if (g_stringState.stringCache[i]) {
            free(g_stringState.stringCache[i]);
            g_stringState.stringCache[i] = NULL;
        }
    }
}

Handle IUGetItlTable(ScriptCode script, int16_t tableCode, Handle *itlHandle, int32_t *offset, int32_t *length)
{
    /* Return international table for specified script */
    if (itlHandle) *itlHandle = NULL;
    if (offset) *offset = 0;
    if (length) *length = 0;

    /* For now, return NULL - would need full international resource implementation */
    return NULL;
}

/**
 * String utility functions
 */
void StringToNum(const char *theString, int32_t *theNum)
{
    if (!theString || !theNum) return;

    *theNum = 0;
    char *endptr;
    long result = strtol(theString, &endptr, 10);

    if (endptr != theString) {
        *theNum = (int32_t)result;
    }
}

void NumToString(int32_t theNum, char *theString)
{
    if (!theString) return;
    sprintf(theString, "%d", (int)theNum);
}

int32_t StringWidth(const char *theString)
{
    if (!theString) return 0;
    return strlen(theString) * 8; /* Assume 8-pixel average character width */
}

void TruncString(int16_t width, char *theString, int16_t truncWhere)
{
    if (!theString) return;

    int16_t charWidth = 8; /* Average character width */
    int16_t maxChars = width / charWidth;
    int16_t stringLen = strlen(theString);

    if (stringLen <= maxChars) return;

    if (truncWhere == 0) { /* Truncate at end */
        theString[maxChars] = '\0';
    } else if (truncWhere == 1) { /* Truncate in middle */
        int16_t halfChars = (maxChars - 3) / 2;
        memmove(theString + halfChars, "...", 3);
        memmove(theString + halfChars + 3, theString + stringLen - halfChars, halfChars + 1);
    } else { /* Truncate at beginning */
        int16_t startPos = stringLen - maxChars;
        memmove(theString, theString + startPos, maxChars + 1);
    }
}

/**
 * Character classification and conversion
 */
Boolean IsLower(char ch)
{
    return islower((unsigned char)ch) != 0;
}

Boolean IsUpper(char ch)
{
    return isupper((unsigned char)ch) != 0;
}

char ToLower(char ch)
{
    return tolower((unsigned char)ch);
}

char ToUpper(char ch)
{
    return toupper((unsigned char)ch);
}

Boolean IsAlpha(char ch)
{
    return isalpha((unsigned char)ch) != 0;
}

Boolean IsDigit(char ch)
{
    return isdigit((unsigned char)ch) != 0;
}

Boolean IsAlphaNum(char ch)
{
    return isalnum((unsigned char)ch) != 0;
}

Boolean IsSpace(char ch)
{
    return isspace((unsigned char)ch) != 0;
}

Boolean IsPunct(char ch)
{
    return ispunct((unsigned char)ch) != 0;
}

/**
 * String manipulation
 */
void CopyString(const char *source, char *dest, int16_t maxLen)
{
    if (!source || !dest || maxLen <= 0) return;

    strncpy(dest, source, maxLen - 1);
    dest[maxLen - 1] = '\0';
}

void ConcatString(const char *source, char *dest, int16_t maxLen)
{
    if (!source || !dest || maxLen <= 0) return;

    int16_t destLen = strlen(dest);
    if (destLen >= maxLen - 1) return;

    strncat(dest, source, maxLen - destLen - 1);
}

int16_t FindString(const char *searchIn, const char *searchFor, int16_t startPos)
{
    if (!searchIn || !searchFor || startPos < 0) return -1;

    const char *found = strstr(searchIn + startPos, searchFor);
    return found ? (int16_t)(found - searchIn) : -1;
}

void ReplaceString(char *theString, const char *oldStr, const char *newStr)
{
    if (!theString || !oldStr || !newStr) return;

    char *pos = strstr(theString, oldStr);
    if (!pos) return;

    int16_t oldLen = strlen(oldStr);
    int16_t newLen = strlen(newStr);
    int16_t tailLen = strlen(pos + oldLen);

    if (newLen != oldLen) {
        memmove(pos + newLen, pos + oldLen, tailLen + 1);
    }
    memcpy(pos, newStr, newLen);
}

void TrimString(char *theString)
{
    if (!theString) return;

    /* Trim leading spaces */
    char *start = theString;
    while (*start && isspace(*start)) start++;

    if (start != theString) {
        memmove(theString, start, strlen(start) + 1);
    }

    /* Trim trailing spaces */
    int len = strlen(theString);
    while (len > 0 && isspace(theString[len - 1])) {
        theString[--len] = '\0';
    }
}

/**
 * Pascal/C string conversion
 */
void C2PStr(char *cString)
{
    if (!cString) return;

    int len = strlen(cString);
    if (len > 255) len = 255;

    memmove(cString + 1, cString, len);
    cString[0] = (char)len;
}

void P2CStr(char *pString)
{
    if (!pString) return;

    int len = (unsigned char)pString[0];
    if (len > 255) len = 255;

    memmove(pString, pString + 1, len);
    pString[len] = '\0';
}

void CopyC2PStr(const char *cString, char *pString)
{
    if (!cString || !pString) return;

    int len = strlen(cString);
    if (len > 255) len = 255;

    pString[0] = (char)len;
    memcpy(pString + 1, cString, len);
}

void CopyP2CStr(const char *pString, char *cString)
{
    if (!pString || !cString) return;

    int len = (unsigned char)pString[0];
    if (len > 255) len = 255;

    memcpy(cString, pString + 1, len);
    cString[len] = '\0';
}

/**
 * Configuration functions
 */
void SetStringPackageScript(ScriptCode script)
{
    g_stringState.currentScript = script;
}

ScriptCode GetStringPackageScript(void)
{
    return g_stringState.currentScript;
}

void SetStringPackageLanguage(LangCode language)
{
    g_stringState.currentLanguage = language;
}

LangCode GetStringPackageLanguage(void)
{
    return g_stringState.currentLanguage;
}

/**
 * Internal helper functions
 */
static void InitializeDefaultResources(void)
{
    /* Set up default international resource */
    g_stringState.intlResource = (Intl0Hndl)malloc(sizeof(Intl0Rec));
    if (g_stringState.intlResource) {
        *g_stringState.intlResource = g_defaultIntl0;
    }
}

static int16_t CompareStringData(const void *aPtr, const void *bPtr, int16_t aLen, int16_t bLen,
                                Boolean caseSensitive, Boolean diacriticalSensitive)
{
    if (!aPtr || !bPtr) {
        return (aPtr == bPtr) ? 0 : (aPtr ? 1 : -1);
    }

    const char *a = (const char*)aPtr;
    const char *b = (const char*)bPtr;
    int16_t minLen = (aLen < bLen) ? aLen : bLen;

    for (int16_t i = 0; i < minLen; i++) {
        char chA = a[i];
        char chB = b[i];

        if (!caseSensitive) {
            chA = tolower(chA);
            chB = tolower(chB);
        }

        if (chA < chB) return -1;
        if (chA > chB) return 1;
    }

    /* Strings are equal up to minimum length */
    if (aLen < bLen) return -1;
    if (aLen > bLen) return 1;
    return 0;
}

static void FormatDateTime(uint32_t dateTime, DateForm dateFormat, Boolean wantSeconds,
                          Boolean isTime, char *result, Handle intlParam)
{
    if (!result) return;

    int16_t year, month, day, hour, minute, second;
    SecondsToDateTime(dateTime, &year, &month, &day, &hour, &minute, &second);

    Intl0Rec *intl = (intlParam && *intlParam) ? (Intl0Rec*)*intlParam : &g_defaultIntl0;

    if (isTime) {
        /* Format time */
        if (intl->timeCycle == 0) { /* 12-hour format */
            int displayHour = hour;
            const char *ampm = (hour < 12) ? (char*)intl->mornStr : (char*)intl->eveStr;

            if (displayHour == 0) displayHour = 12;
            else if (displayHour > 12) displayHour -= 12;

            if (wantSeconds) {
                sprintf(result, "%d%c%02d%c%02d %s", displayHour, intl->timeSep,
                       minute, intl->timeSep, second, ampm);
            } else {
                sprintf(result, "%d%c%02d %s", displayHour, intl->timeSep, minute, ampm);
            }
        } else { /* 24-hour format */
            if (wantSeconds) {
                sprintf(result, "%02d%c%02d%c%02d", hour, intl->timeSep, minute, intl->timeSep, second);
            } else {
                sprintf(result, "%02d%c%02d", hour, intl->timeSep, minute);
            }
        }
    } else {
        /* Format date */
        const char *monthNames[] = {
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        };
        const char *shortMonthNames[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };

        switch (dateFormat) {
            case shortDate:
                if (intl->dateOrder == 0) { /* M/D/Y */
                    sprintf(result, "%d%c%d%c%d", month, intl->dateSep, day, intl->dateSep, year);
                } else if (intl->dateOrder == 1) { /* D/M/Y */
                    sprintf(result, "%d%c%d%c%d", day, intl->dateSep, month, intl->dateSep, year);
                } else { /* Y/M/D */
                    sprintf(result, "%d%c%d%c%d", year, intl->dateSep, month, intl->dateSep, day);
                }
                break;

            case longDate:
                sprintf(result, "%s %d, %d", monthNames[month - 1], day, year);
                break;

            case abbrevDate:
                sprintf(result, "%s %d, %d", shortMonthNames[month - 1], day, year);
                break;
        }
    }
}

static Boolean IsLeapYear(int16_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static void SecondsToDateTime(uint32_t seconds, int16_t *year, int16_t *month, int16_t *day,
                             int16_t *hour, int16_t *minute, int16_t *second)
{
    /* Mac OS epoch starts January 1, 1904 */
    const uint32_t baseYear = 1904;
    const uint32_t secondsPerDay = 24 * 60 * 60;
    const uint32_t secondsPerYear = 365 * secondsPerDay;

    /* Extract time components */
    uint32_t daySeconds = seconds % secondsPerDay;
    *hour = daySeconds / 3600;
    *minute = (daySeconds % 3600) / 60;
    *second = daySeconds % 60;

    /* Extract date components (simplified) */
    uint32_t days = seconds / secondsPerDay;
    *year = baseYear + days / 365; /* Approximate */

    /* Adjust for leap years (simplified) */
    uint32_t leapDays = (*year - baseYear) / 4;
    days -= leapDays;
    *year = baseYear + days / 365;

    uint32_t yearDays = days % 365;

    /* Convert day of year to month and day (simplified) */
    static int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (IsLeapYear(*year)) {
        daysInMonth[1] = 29;
    }

    *month = 1;
    while (yearDays >= daysInMonth[*month - 1] && *month <= 12) {
        yearDays -= daysInMonth[*month - 1];
        (*month)++;
    }

    *day = yearDays + 1;

    /* Reset February days */
    daysInMonth[1] = 28;
}

/**
 * Cleanup function
 */
void CleanupStringPackage(void)
{
    IUClearCache();

    if (g_stringState.intlResource) {
        free(g_stringState.intlResource);
        g_stringState.intlResource = NULL;
    }

    g_stringState.initialized = false;
}