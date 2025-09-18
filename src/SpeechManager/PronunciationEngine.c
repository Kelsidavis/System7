/*
 * File: PronunciationEngine.c
 *
 * Contains: Phoneme processing and pronunciation engine implementation
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This file implements phoneme processing and pronunciation
 *              functionality including phonetic analysis and conversion.
 */

#include "../include/SpeechManager/PronunciationEngine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ===== Pronunciation Engine Implementation ===== */

/* Maximum limits */
#define MAX_PHONEMES 512
#define MAX_DICTIONARY_ENTRIES 32768
#define MAX_PRONUNCIATION_RULES 1024
#define MAX_PHONEME_STRING_LENGTH 1024

/* Pronunciation dictionary structure */
struct PronunciationDictionary {
    bool initialized;
    PronunciationEntry *entries;
    long entryCount;
    long maxEntries;
    PronunciationRule *rules;
    long ruleCount;
    long maxRules;
    short defaultLanguage;

    /* Callbacks */
    WordAnalysisProc analysisCallback;
    void *analysisUserData;
    RuleApplicationProc ruleCallback;
    void *ruleUserData;

    /* Performance data */
    long lookupCount;
    long cacheHits;
    long cacheMisses;

    pthread_mutex_t dictionaryMutex;
};

/* Built-in phoneme information */
static const PhonemeInfoExtended kBuiltinPhonemes[] = {
    {
        .opcode = 1,
        .category = kPhonemeCategory_Vowel,
        .ipaSymbol = "i",
        .sampaSymbol = "i",
        .arpabetSymbol = "IY",
        .macSpeechSymbol = "EE",
        .description = "High front unrounded vowel",
        .examples = "beat, feet, see",
        .duration = 150,
        .frequency = 270,
        .isVoiced = true,
        .canBeStressed = true,
        .acousticData = NULL
    },
    {
        .opcode = 2,
        .category = kPhonemeCategory_Vowel,
        .ipaSymbol = "ɪ",
        .sampaSymbol = "I",
        .arpabetSymbol = "IH",
        .macSpeechSymbol = "IH",
        .description = "Near-high near-front unrounded vowel",
        .examples = "bit, fit, sit",
        .duration = 120,
        .frequency = 300,
        .isVoiced = true,
        .canBeStressed = true,
        .acousticData = NULL
    },
    {
        .opcode = 3,
        .category = kPhonemeCategory_Vowel,
        .ipaSymbol = "e",
        .sampaSymbol = "e",
        .arpabetSymbol = "EY",
        .macSpeechSymbol = "AY",
        .description = "Mid front unrounded vowel",
        .examples = "bay, say, day",
        .duration = 180,
        .frequency = 400,
        .isVoiced = true,
        .canBeStressed = true,
        .acousticData = NULL
    },
    {
        .opcode = 4,
        .category = kPhonemeCategory_Consonant,
        .ipaSymbol = "p",
        .sampaSymbol = "p",
        .arpabetSymbol = "P",
        .macSpeechSymbol = "P",
        .description = "Voiceless bilabial plosive",
        .examples = "pat, tap, sip",
        .duration = 80,
        .frequency = 1000,
        .isVoiced = false,
        .canBeStressed = false,
        .acousticData = NULL
    },
    {
        .opcode = 5,
        .category = kPhonemeCategory_Consonant,
        .ipaSymbol = "b",
        .sampaSymbol = "b",
        .arpabetSymbol = "B",
        .macSpeechSymbol = "B",
        .description = "Voiced bilabial plosive",
        .examples = "bat, cab, robe",
        .duration = 90,
        .frequency = 150,
        .isVoiced = true,
        .canBeStressed = false,
        .acousticData = NULL
    }
};

#define BUILTIN_PHONEME_COUNT (sizeof(kBuiltinPhonemes) / sizeof(PhonemeInfoExtended))

/* Global pronunciation engine state */
typedef struct PronunciationEngineState {
    bool initialized;
    PronunciationDictionary *defaultDictionary;
    short defaultLanguage;
    bool cacheEnabled;
    long cacheSize;
    long cacheHits;
    long cacheMisses;
    PhonemeConversionProc conversionCallback;
    void *conversionUserData;
} PronunciationEngineState;

static PronunciationEngineState gPronunciationEngine = {
    .initialized = false,
    .defaultDictionary = NULL,
    .defaultLanguage = 0,
    .cacheEnabled = true,
    .cacheSize = 0,
    .cacheHits = 0,
    .cacheMisses = 0,
    .conversionCallback = NULL,
    .conversionUserData = NULL
};

/* Built-in pronunciation rules */
static const PronunciationRule kBuiltinRules[] = {
    {
        .type = kPronunciationRule_Grapheme,
        .pattern = "ph",
        .replacement = "f",
        .context = "",
        .priority = 10,
        .isActive = true,
        .ruleData = NULL
    },
    {
        .type = kPronunciationRule_Grapheme,
        .pattern = "ck",
        .replacement = "k",
        .context = "",
        .priority = 10,
        .isActive = true,
        .ruleData = NULL
    },
    {
        .type = kPronunciationRule_Grapheme,
        .pattern = "qu",
        .replacement = "kw",
        .context = "",
        .priority = 10,
        .isActive = true,
        .ruleData = NULL
    }
};

#define BUILTIN_RULE_COUNT (sizeof(kBuiltinRules) / sizeof(PronunciationRule))

/* ===== Internal Functions ===== */

/*
 * FindPhonemeBySymbol
 * Finds phoneme information by symbol
 */
static const PhonemeInfoExtended *FindPhonemeBySymbol(const char *symbol, PhonemeSymbolType symbolType) {
    if (!symbol) return NULL;

    for (int i = 0; i < BUILTIN_PHONEME_COUNT; i++) {
        const PhonemeInfoExtended *phoneme = &kBuiltinPhonemes[i];

        switch (symbolType) {
            case kPhonemeType_IPA:
                if (strcmp(phoneme->ipaSymbol, symbol) == 0) return phoneme;
                break;
            case kPhonemeType_SAMPA:
                if (strcmp(phoneme->sampaSymbol, symbol) == 0) return phoneme;
                break;
            case kPhonemeType_ARPABET:
                if (strcmp(phoneme->arpabetSymbol, symbol) == 0) return phoneme;
                break;
            case kPhonemeType_MacSpeech:
                if (strcmp(phoneme->macSpeechSymbol, symbol) == 0) return phoneme;
                break;
        }
    }

    return NULL;
}

/*
 * ApplyGraphemeToPhonemeRules
 * Applies basic grapheme-to-phoneme conversion rules
 */
static OSErr ApplyGraphemeToPhonemeRules(const char *text, char *phonemes, long phonemeSize) {
    if (!text || !phonemes || phonemeSize <= 0) {
        return paramErr;
    }

    const char *input = text;
    char *output = phonemes;
    long remainingSize = phonemeSize - 1;

    while (*input && remainingSize > 0) {
        bool ruleApplied = false;

        /* Apply built-in rules */
        for (int i = 0; i < BUILTIN_RULE_COUNT; i++) {
            const PronunciationRule *rule = &kBuiltinRules[i];
            if (!rule->isActive) continue;

            size_t patternLen = strlen(rule->pattern);
            if (strncmp(input, rule->pattern, patternLen) == 0) {
                /* Apply rule */
                size_t replacementLen = strlen(rule->replacement);
                if (replacementLen <= remainingSize) {
                    strcpy(output, rule->replacement);
                    output += replacementLen;
                    remainingSize -= replacementLen;
                    input += patternLen;
                    ruleApplied = true;
                    break;
                }
            }
        }

        if (!ruleApplied) {
            /* Default: copy character as-is */
            *output++ = tolower(*input++);
            remainingSize--;
        }
    }

    *output = '\0';
    return noErr;
}

/*
 * ConvertToPhonemeSymbolType
 * Converts phonemes from one symbol type to another
 */
static OSErr ConvertToPhonemeSymbolType(const char *inputPhonemes, PhonemeSymbolType inputType,
                                        PhonemeSymbolType outputType, char *outputPhonemes, long outputSize) {
    if (!inputPhonemes || !outputPhonemes || outputSize <= 0) {
        return paramErr;
    }

    if (inputType == outputType) {
        /* Same type, just copy */
        strncpy(outputPhonemes, inputPhonemes, outputSize - 1);
        outputPhonemes[outputSize - 1] = '\0';
        return noErr;
    }

    /* For now, implement basic conversion */
    /* In a real implementation, this would parse phoneme symbols and convert them */
    strncpy(outputPhonemes, inputPhonemes, outputSize - 1);
    outputPhonemes[outputSize - 1] = '\0';

    return noErr;
}

/*
 * FindDictionaryEntry
 * Finds an entry in the pronunciation dictionary
 */
static PronunciationEntry *FindDictionaryEntry(PronunciationDictionary *dictionary, const char *word) {
    if (!dictionary || !word) {
        return NULL;
    }

    for (long i = 0; i < dictionary->entryCount; i++) {
        if (strcmp(dictionary->entries[i].word, word) == 0) {
            dictionary->lookupCount++;
            dictionary->cacheHits++;
            return &dictionary->entries[i];
        }
    }

    dictionary->cacheMisses++;
    return NULL;
}

/* ===== Public API Implementation ===== */

/*
 * InitializePronunciationEngine
 * Initializes the pronunciation engine
 */
OSErr InitializePronunciationEngine(void) {
    if (gPronunciationEngine.initialized) {
        return noErr;
    }

    /* Create default dictionary */
    OSErr err = CreatePronunciationDictionary(&gPronunciationEngine.defaultDictionary);
    if (err != noErr) {
        return err;
    }

    /* Add some basic pronunciation entries */
    AddPronunciation(gPronunciationEngine.defaultDictionary, "hello", "həˈloʊ", kPhonemeType_IPA);
    AddPronunciation(gPronunciationEngine.defaultDictionary, "world", "wɜrld", kPhonemeType_IPA);
    AddPronunciation(gPronunciationEngine.defaultDictionary, "speech", "spiːtʃ", kPhonemeType_IPA);

    gPronunciationEngine.initialized = true;
    return noErr;
}

/*
 * CleanupPronunciationEngine
 * Cleans up pronunciation engine resources
 */
void CleanupPronunciationEngine(void) {
    if (!gPronunciationEngine.initialized) {
        return;
    }

    if (gPronunciationEngine.defaultDictionary) {
        DisposePronunciationDictionary(gPronunciationEngine.defaultDictionary);
        gPronunciationEngine.defaultDictionary = NULL;
    }

    gPronunciationEngine.initialized = false;
}

/*
 * CreatePronunciationDictionary
 * Creates a new pronunciation dictionary
 */
OSErr CreatePronunciationDictionary(PronunciationDictionary **dictionary) {
    if (!dictionary) {
        return paramErr;
    }

    *dictionary = malloc(sizeof(PronunciationDictionary));
    if (!*dictionary) {
        return memFullErr;
    }

    memset(*dictionary, 0, sizeof(PronunciationDictionary));

    /* Allocate entries array */
    (*dictionary)->maxEntries = 1024; /* Initial size */
    (*dictionary)->entries = malloc((*dictionary)->maxEntries * sizeof(PronunciationEntry));
    if (!(*dictionary)->entries) {
        free(*dictionary);
        *dictionary = NULL;
        return memFullErr;
    }

    /* Allocate rules array */
    (*dictionary)->maxRules = 256; /* Initial size */
    (*dictionary)->rules = malloc((*dictionary)->maxRules * sizeof(PronunciationRule));
    if (!(*dictionary)->rules) {
        free((*dictionary)->entries);
        free(*dictionary);
        *dictionary = NULL;
        return memFullErr;
    }

    /* Initialize built-in rules */
    for (int i = 0; i < BUILTIN_RULE_COUNT; i++) {
        (*dictionary)->rules[i] = kBuiltinRules[i];
        (*dictionary)->ruleCount++;
    }

    /* Initialize mutex */
    int result = pthread_mutex_init(&(*dictionary)->dictionaryMutex, NULL);
    if (result != 0) {
        free((*dictionary)->rules);
        free((*dictionary)->entries);
        free(*dictionary);
        *dictionary = NULL;
        return memFullErr;
    }

    (*dictionary)->initialized = true;
    return noErr;
}

/*
 * DisposePronunciationDictionary
 * Disposes of a pronunciation dictionary
 */
OSErr DisposePronunciationDictionary(PronunciationDictionary *dictionary) {
    if (!dictionary) {
        return paramErr;
    }

    if (dictionary->entries) {
        free(dictionary->entries);
    }
    if (dictionary->rules) {
        free(dictionary->rules);
    }

    pthread_mutex_destroy(&dictionary->dictionaryMutex);
    free(dictionary);

    return noErr;
}

/*
 * AddPronunciation
 * Adds a pronunciation entry to the dictionary
 */
OSErr AddPronunciation(PronunciationDictionary *dictionary, const char *word,
                       const char *pronunciation, PhonemeSymbolType symbolType) {
    if (!dictionary || !word || !pronunciation) {
        return paramErr;
    }

    pthread_mutex_lock(&dictionary->dictionaryMutex);

    /* Check if we need to expand the array */
    if (dictionary->entryCount >= dictionary->maxEntries) {
        long newSize = dictionary->maxEntries * 2;
        PronunciationEntry *newEntries = realloc(dictionary->entries,
                                                  newSize * sizeof(PronunciationEntry));
        if (!newEntries) {
            pthread_mutex_unlock(&dictionary->dictionaryMutex);
            return memFullErr;
        }
        dictionary->entries = newEntries;
        dictionary->maxEntries = newSize;
    }

    /* Add new entry */
    PronunciationEntry *entry = &dictionary->entries[dictionary->entryCount];
    memset(entry, 0, sizeof(PronunciationEntry));

    strncpy(entry->word, word, sizeof(entry->word) - 1);
    entry->word[sizeof(entry->word) - 1] = '\0';

    strncpy(entry->phonemes, pronunciation, sizeof(entry->phonemes) - 1);
    entry->phonemes[sizeof(entry->phonemes) - 1] = '\0';

    entry->symbolType = symbolType;
    entry->frequency = 50; /* Default frequency */
    entry->confidence = 90; /* Default confidence */

    dictionary->entryCount++;

    pthread_mutex_unlock(&dictionary->dictionaryMutex);
    return noErr;
}

/*
 * LookupPronunciation
 * Looks up a pronunciation in the dictionary
 */
OSErr LookupPronunciation(PronunciationDictionary *dictionary, const char *word,
                          PronunciationEntry **entry) {
    if (!dictionary || !word || !entry) {
        return paramErr;
    }

    pthread_mutex_lock(&dictionary->dictionaryMutex);

    PronunciationEntry *found = FindDictionaryEntry(dictionary, word);
    if (found) {
        *entry = found;

        /* Notify analysis callback */
        if (dictionary->analysisCallback) {
            dictionary->analysisCallback(word, found, dictionary->analysisUserData);
        }

        pthread_mutex_unlock(&dictionary->dictionaryMutex);
        return noErr;
    }

    pthread_mutex_unlock(&dictionary->dictionaryMutex);
    return voiceNotFound;
}

/*
 * ConvertTextToPhonemes
 * Converts text to phonemes using basic rules
 */
OSErr ConvertTextToPhonemes(const char *text, long textLength, PhonemeSymbolType outputType,
                            char *phonemeBuffer, long bufferSize, long *phonemeLength) {
    if (!text || textLength <= 0 || !phonemeBuffer || bufferSize <= 0) {
        return paramErr;
    }

    if (!gPronunciationEngine.initialized) {
        OSErr err = InitializePronunciationEngine();
        if (err != noErr) {
            return err;
        }
    }

    /* Create null-terminated text */
    char *nullTermText = malloc(textLength + 1);
    if (!nullTermText) {
        return memFullErr;
    }
    memcpy(nullTermText, text, textLength);
    nullTermText[textLength] = '\0';

    /* Apply basic conversion rules */
    OSErr err = ApplyGraphemeToPhonemeRules(nullTermText, phonemeBuffer, bufferSize);

    if (err == noErr && phonemeLength) {
        *phonemeLength = strlen(phonemeBuffer);
    }

    free(nullTermText);
    return err;
}

/*
 * ConvertTextToPhonemeString
 * Converts text to phoneme string with dictionary lookup
 */
OSErr ConvertTextToPhonemeString(const char *text, long textLength,
                                 char *phonemeString, long stringSize, long *stringLength) {
    if (!text || textLength <= 0 || !phonemeString || stringSize <= 0) {
        return paramErr;
    }

    if (!gPronunciationEngine.initialized) {
        OSErr err = InitializePronunciationEngine();
        if (err != noErr) {
            return err;
        }
    }

    /* Use custom callback if available */
    if (gPronunciationEngine.conversionCallback) {
        bool result = gPronunciationEngine.conversionCallback(text, phonemeString, stringSize,
                                                               gPronunciationEngine.conversionUserData);
        if (result) {
            if (stringLength) {
                *stringLength = strlen(phonemeString);
            }
            return noErr;
        }
    }

    /* Default conversion */
    char tempText[1024];
    if (textLength >= sizeof(tempText)) {
        textLength = sizeof(tempText) - 1;
    }

    memcpy(tempText, text, textLength);
    tempText[textLength] = '\0';

    /* Simple word-by-word conversion */
    char *word = strtok(tempText, " \t\n\r");
    phonemeString[0] = '\0';

    while (word && strlen(phonemeString) < stringSize - 64) {
        /* Try dictionary lookup first */
        PronunciationEntry *entry = NULL;
        OSErr err = LookupPronunciation(gPronunciationEngine.defaultDictionary, word, &entry);

        if (err == noErr && entry) {
            /* Use dictionary pronunciation */
            strcat(phonemeString, entry->phonemes);
        } else {
            /* Use rule-based conversion */
            char wordPhonemes[256];
            ApplyGraphemeToPhonemeRules(word, wordPhonemes, sizeof(wordPhonemes));
            strcat(phonemeString, wordPhonemes);
        }

        /* Add word separator */
        strcat(phonemeString, " ");

        word = strtok(NULL, " \t\n\r");
    }

    /* Remove trailing space */
    long len = strlen(phonemeString);
    if (len > 0 && phonemeString[len - 1] == ' ') {
        phonemeString[len - 1] = '\0';
        len--;
    }

    if (stringLength) {
        *stringLength = len;
    }

    return noErr;
}

/*
 * ConvertPhonemeSymbols
 * Converts phonemes from one symbol type to another
 */
OSErr ConvertPhonemeSymbols(const char *inputPhonemes, PhonemeSymbolType inputType,
                            PhonemeSymbolType outputType, char *outputPhonemes, long bufferSize) {
    if (!inputPhonemes || !outputPhonemes || bufferSize <= 0) {
        return paramErr;
    }

    return ConvertToPhonemeSymbolType(inputPhonemes, inputType, outputType,
                                      outputPhonemes, bufferSize);
}

/*
 * GetPhonemeSymbolInfo
 * Gets information about a phoneme symbol
 */
OSErr GetPhonemeSymbolInfo(const char *symbol, PhonemeSymbolType symbolType,
                           PhonemeInfoExtended *info) {
    if (!symbol || !info) {
        return paramErr;
    }

    const PhonemeInfoExtended *phoneme = FindPhonemeBySymbol(symbol, symbolType);
    if (phoneme) {
        *info = *phoneme;
        return noErr;
    }

    return voiceNotFound;
}

/*
 * IsValidPhonemeOpcode
 * Checks if a phoneme opcode is valid
 */
bool IsValidPhonemeOpcode(short opcode) {
    for (int i = 0; i < BUILTIN_PHONEME_COUNT; i++) {
        if (kBuiltinPhonemes[i].opcode == opcode) {
            return true;
        }
    }
    return false;
}

/*
 * GetPhonemeCount
 * Returns the number of available phonemes
 */
OSErr GetPhonemeCount(short *phonemeCount) {
    if (!phonemeCount) {
        return paramErr;
    }

    *phonemeCount = BUILTIN_PHONEME_COUNT;
    return noErr;
}

/*
 * GetIndPhoneme
 * Gets phoneme information by index
 */
OSErr GetIndPhoneme(short index, PhonemeInfoExtended *info) {
    if (index < 1 || !info) {
        return paramErr;
    }

    if (index > BUILTIN_PHONEME_COUNT) {
        return paramErr;
    }

    *info = kBuiltinPhonemes[index - 1];
    return noErr;
}

/*
 * SetPhonemeConversionCallback
 * Sets custom phoneme conversion callback
 */
OSErr SetPhonemeConversionCallback(PhonemeConversionProc callback, void *userData) {
    gPronunciationEngine.conversionCallback = callback;
    gPronunciationEngine.conversionUserData = userData;
    return noErr;
}

/*
 * EnablePronunciationCache
 * Enables or disables pronunciation caching
 */
OSErr EnablePronunciationCache(bool enable) {
    gPronunciationEngine.cacheEnabled = enable;
    return noErr;
}

/*
 * GetCacheStatistics
 * Gets pronunciation cache statistics
 */
OSErr GetCacheStatistics(long *cacheSize, long *hitCount, long *missCount) {
    if (cacheSize) *cacheSize = gPronunciationEngine.cacheSize;
    if (hitCount) *hitCount = gPronunciationEngine.cacheHits;
    if (missCount) *missCount = gPronunciationEngine.cacheMisses;
    return noErr;
}

/* Module cleanup */
static void __attribute__((destructor)) PronunciationEngineCleanup(void) {
    CleanupPronunciationEngine();
}