/*
 * File: PronunciationEngine.h
 *
 * Contains: Phoneme processing and pronunciation engine for Speech Manager
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This header provides phoneme processing and pronunciation
 *              functionality including phonetic analysis and conversion.
 */

#ifndef _PRONUNCIATIONENGINE_H_
#define _PRONUNCIATIONENGINE_H_

#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Phoneme and Pronunciation Constants ===== */

/* Phoneme symbol types */
typedef enum {
    kPhonemeType_Unknown    = 0,
    kPhonemeType_IPA        = 1,  /* International Phonetic Alphabet */
    kPhonemeType_SAMPA      = 2,  /* Speech Assessment Methods Phonetic Alphabet */
    kPhonemeType_ARPABET    = 3,  /* ARPAbet phonetic notation */
    kPhonemeType_MacSpeech  = 4,  /* Classic Mac Speech phonemes */
    kPhonemeType_Custom     = 5   /* Custom phoneme notation */
} PhonemeSymbolType;

/* Phoneme categories */
typedef enum {
    kPhonemeCategory_Vowel      = 1,
    kPhonemeCategory_Consonant  = 2,
    kPhonemeCategory_Diphthong  = 3,
    kPhonemeCategory_Semivowel  = 4,
    kPhonemeCategory_Liquid     = 5,
    kPhonemeCategory_Nasal      = 6,
    kPhonemeCategory_Fricative  = 7,
    kPhonemeCategory_Plosive    = 8,
    kPhonemeCategory_Affricate  = 9,
    kPhonemeCategory_Silence    = 10
} PhonemeCategory;

/* Stress levels */
typedef enum {
    kStress_None        = 0,
    kStress_Primary     = 1,
    kStress_Secondary   = 2,
    kStress_Tertiary    = 3
} StressLevel;

/* Syllable boundaries */
typedef enum {
    kSyllableBoundary_None      = 0,
    kSyllableBoundary_Start     = 1,
    kSyllableBoundary_End       = 2,
    kSyllableBoundary_Peak      = 3
} SyllableBoundary;

/* ===== Phoneme Structures ===== */

/* Extended phoneme information */
typedef struct PhonemeInfoExtended {
    short opcode;                   /* Phoneme opcode */
    PhonemeCategory category;       /* Phoneme category */
    char ipaSymbol[8];             /* IPA representation */
    char sampaSymbol[8];           /* SAMPA representation */
    char arpabetSymbol[8];         /* ARPAbet representation */
    char macSpeechSymbol[8];       /* Mac Speech representation */
    char description[64];          /* Human-readable description */
    char examples[128];            /* Example words */
    long duration;                 /* Typical duration in milliseconds */
    short frequency;               /* Typical frequency in Hz */
    bool isVoiced;                 /* Whether phoneme is voiced */
    bool canBeStressed;            /* Whether phoneme can carry stress */
    void *acousticData;            /* Acoustic model data */
} PhonemeInfoExtended;

/* Pronunciation entry */
typedef struct PronunciationEntry {
    char word[64];                 /* Word in orthographic form */
    char phonemes[256];            /* Phonetic transcription */
    PhonemeSymbolType symbolType;  /* Type of phonetic symbols used */
    StressLevel *stressPattern;    /* Stress pattern for syllables */
    short syllableCount;           /* Number of syllables */
    SyllableBoundary *boundaries;  /* Syllable boundary markers */
    short frequency;               /* Word frequency (0-100) */
    short confidence;              /* Pronunciation confidence (0-100) */
    void *metadata;                /* Additional metadata */
} PronunciationEntry;

/* Phonetic analysis result */
typedef struct PhoneticAnalysis {
    char originalText[256];        /* Original text */
    char phonemeString[512];       /* Generated phoneme string */
    PhonemeInfoExtended *phonemes; /* Array of phoneme information */
    short phonemeCount;            /* Number of phonemes */
    StressLevel *stressLevels;     /* Stress levels for each phoneme */
    SyllableBoundary *syllables;   /* Syllable boundaries */
    short syllableCount;           /* Number of syllables */
    long totalDuration;            /* Estimated total duration */
    OSErr analysisError;           /* Error code if analysis failed */
    char *errorMessage;            /* Error message if any */
} PhoneticAnalysis;

/* Pronunciation dictionary */
typedef struct PronunciationDictionary PronunciationDictionary;

/* ===== Pronunciation Engine Management ===== */

/* Engine initialization */
OSErr InitializePronunciationEngine(void);
void CleanupPronunciationEngine(void);

/* Dictionary management */
OSErr CreatePronunciationDictionary(PronunciationDictionary **dictionary);
OSErr DisposePronunciationDictionary(PronunciationDictionary *dictionary);
OSErr LoadPronunciationDictionary(const char *dictionaryPath, PronunciationDictionary **dictionary);
OSErr SavePronunciationDictionary(PronunciationDictionary *dictionary, const char *dictionaryPath);

/* Dictionary operations */
OSErr AddPronunciation(PronunciationDictionary *dictionary, const char *word,
                       const char *pronunciation, PhonemeSymbolType symbolType);
OSErr RemovePronunciation(PronunciationDictionary *dictionary, const char *word);
OSErr LookupPronunciation(PronunciationDictionary *dictionary, const char *word,
                          PronunciationEntry **entry);
OSErr UpdatePronunciation(PronunciationDictionary *dictionary, const char *word,
                          const PronunciationEntry *newEntry);

/* ===== Text-to-Phoneme Conversion ===== */

/* Basic conversion */
OSErr ConvertTextToPhonemes(const char *text, long textLength, PhonemeSymbolType outputType,
                            char *phonemeBuffer, long bufferSize, long *phonemeLength);
OSErr ConvertTextToPhonemeString(const char *text, long textLength,
                                 char *phonemeString, long stringSize, long *stringLength);

/* Advanced conversion with analysis */
OSErr AnalyzeTextPhonetically(const char *text, long textLength, PhonemeSymbolType outputType,
                              PronunciationDictionary *dictionary, PhoneticAnalysis **analysis);
OSErr DisposePhoneticAnalysis(PhoneticAnalysis *analysis);

/* Word-by-word conversion */
OSErr ConvertWordToPhonemes(const char *word, PhonemeSymbolType outputType,
                            PronunciationDictionary *dictionary,
                            char *phonemeBuffer, long bufferSize);

/* ===== Phoneme-to-Text Conversion ===== */

/* Reverse conversion */
OSErr ConvertPhonemesToText(const char *phonemeString, PhonemeSymbolType inputType,
                            char *textBuffer, long bufferSize, long *textLength);
OSErr TransliteratePhonemesToOrthography(const char *phonemeString, PhonemeSymbolType inputType,
                                         char *orthographicText, long bufferSize);

/* ===== Phoneme Symbol Conversion ===== */

/* Symbol type conversion */
OSErr ConvertPhonemeSymbols(const char *inputPhonemes, PhonemeSymbolType inputType,
                            PhonemeSymbolType outputType, char *outputPhonemes, long bufferSize);

/* Symbol validation */
OSErr ValidatePhonemeString(const char *phonemeString, PhonemeSymbolType symbolType,
                            bool *isValid, char **errorMessage);

/* Symbol information */
OSErr GetPhonemeSymbolInfo(const char *symbol, PhonemeSymbolType symbolType,
                           PhonemeInfoExtended *info);

/* ===== Stress and Syllable Analysis ===== */

/* Stress pattern analysis */
OSErr AnalyzeStressPattern(const char *word, PronunciationDictionary *dictionary,
                           StressLevel **stressLevels, short *syllableCount);
OSErr ApplyStressPattern(char *phonemeString, const StressLevel *stressLevels,
                         short syllableCount, PhonemeSymbolType symbolType);

/* Syllable boundary detection */
OSErr DetectSyllableBoundaries(const char *phonemeString, PhonemeSymbolType symbolType,
                               SyllableBoundary **boundaries, short *boundaryCount);
OSErr InsertSyllableBoundaries(char *phonemeString, long bufferSize,
                               const SyllableBoundary *boundaries, short boundaryCount);

/* ===== Pronunciation Rules ===== */

/* Rule types */
typedef enum {
    kPronunciationRule_Grapheme     = 1,  /* Grapheme-to-phoneme rules */
    kPronunciationRule_Morpheme     = 2,  /* Morpheme-based rules */
    kPronunciationRule_Context      = 3,  /* Context-dependent rules */
    kPronunciationRule_Exception    = 4,  /* Exception rules */
    kPronunciationRule_Foreign      = 5   /* Foreign word rules */
} PronunciationRuleType;

/* Pronunciation rule */
typedef struct PronunciationRule {
    PronunciationRuleType type;
    char pattern[64];               /* Input pattern */
    char replacement[64];           /* Output replacement */
    char context[64];               /* Context constraints */
    short priority;                 /* Rule priority */
    bool isActive;                  /* Whether rule is active */
    void *ruleData;                 /* Rule-specific data */
} PronunciationRule;

/* Rule management */
OSErr AddPronunciationRule(PronunciationDictionary *dictionary, const PronunciationRule *rule);
OSErr RemovePronunciationRule(PronunciationDictionary *dictionary, const char *pattern);
OSErr GetPronunciationRules(PronunciationDictionary *dictionary, PronunciationRule **rules,
                            long *ruleCount);
OSErr ApplyPronunciationRules(const char *text, PronunciationDictionary *dictionary,
                              char *phonemeOutput, long outputSize);

/* ===== Language-Specific Support ===== */

/* Language models */
OSErr LoadLanguageModel(short languageCode, PronunciationDictionary **languageDict);
OSErr SetDefaultLanguage(short languageCode);
OSErr GetDefaultLanguage(short *languageCode);

/* Multi-language support */
OSErr DetectTextLanguage(const char *text, long textLength, short *languageCode,
                         short *confidence);
OSErr ConvertTextWithLanguage(const char *text, long textLength, short languageCode,
                              PhonemeSymbolType outputType, char *phonemeOutput, long outputSize);

/* ===== Acoustic Modeling ===== */

/* Acoustic features */
typedef struct AcousticFeatures {
    double *formantFrequencies;    /* Formant frequencies */
    short formantCount;            /* Number of formants */
    double fundamentalFreq;        /* Fundamental frequency */
    double duration;               /* Phoneme duration */
    double intensity;              /* Sound intensity */
    double *spectralEnvelope;      /* Spectral envelope */
    short spectrumSize;            /* Spectrum size */
    void *additionalFeatures;      /* Additional acoustic data */
} AcousticFeatures;

/* Acoustic modeling */
OSErr GetPhonemeAcousticFeatures(short phonemeOpcode, AcousticFeatures **features);
OSErr DisposeAcousticFeatures(AcousticFeatures *features);
OSErr SynthesizePhonemeAudio(short phonemeOpcode, long duration, void **audioData, long *audioSize);

/* ===== Pronunciation Training ===== */

/* Training data */
typedef struct PronunciationTrainingData {
    char word[64];                 /* Training word */
    char expectedPhonemes[256];    /* Expected pronunciation */
    char actualPhonemes[256];      /* Actual pronunciation */
    void *audioData;               /* Audio recording */
    long audioSize;                /* Audio data size */
    double similarity;             /* Pronunciation similarity (0.0-1.0) */
} PronunciationTrainingData;

/* Training operations */
OSErr TrainPronunciationModel(PronunciationTrainingData *trainingData, long dataCount);
OSErr EvaluatePronunciation(const char *word, const char *actualPhonemes,
                            PronunciationDictionary *dictionary, double *similarity);
OSErr AdaptPronunciationModel(const char *word, const char *correctPhonemes,
                              PronunciationDictionary *dictionary);

/* ===== Pronunciation Callbacks ===== */

/* Word analysis callback */
typedef void (*WordAnalysisProc)(const char *word, const PronunciationEntry *entry, void *userData);

/* Phoneme conversion callback */
typedef bool (*PhonemeConversionProc)(const char *inputText, char *outputPhonemes,
                                       long outputSize, void *userData);

/* Rule application callback */
typedef bool (*RuleApplicationProc)(const PronunciationRule *rule, const char *inputText,
                                    char *outputText, void *userData);

/* Callback registration */
OSErr SetWordAnalysisCallback(PronunciationDictionary *dictionary, WordAnalysisProc callback,
                              void *userData);
OSErr SetPhonemeConversionCallback(PhonemeConversionProc callback, void *userData);
OSErr SetRuleApplicationCallback(PronunciationDictionary *dictionary, RuleApplicationProc callback,
                                 void *userData);

/* ===== Utilities and Tools ===== */

/* Phoneme utilities */
bool IsValidPhonemeOpcode(short opcode);
OSErr GetPhonemeCount(short *phonemeCount);
OSErr GetIndPhoneme(short index, PhonemeInfoExtended *info);

/* Dictionary utilities */
OSErr GetDictionaryWordCount(PronunciationDictionary *dictionary, long *wordCount);
OSErr GetDictionaryWords(PronunciationDictionary *dictionary, char ***words, long *wordCount);
OSErr SearchDictionary(PronunciationDictionary *dictionary, const char *pattern,
                       PronunciationEntry **matches, long *matchCount);

/* Pronunciation comparison */
double ComparePronunciations(const char *pronunciation1, const char *pronunciation2,
                             PhonemeSymbolType symbolType);
OSErr AlignPronunciations(const char *pronunciation1, const char *pronunciation2,
                          PhonemeSymbolType symbolType, char **alignment1, char **alignment2);

/* Text preprocessing */
OSErr PreprocessTextForPronunciation(const char *inputText, long inputLength,
                                     char **preprocessedText, long *outputLength);
OSErr NormalizeOrthography(const char *inputText, long inputLength,
                           char **normalizedText, long *outputLength);

/* ===== Performance and Caching ===== */

/* Pronunciation caching */
OSErr EnablePronunciationCache(bool enable);
OSErr ClearPronunciationCache(void);
OSErr GetCacheStatistics(long *cacheSize, long *hitCount, long *missCount);

/* Performance optimization */
OSErr OptimizeDictionary(PronunciationDictionary *dictionary);
OSErr CompressDictionary(PronunciationDictionary *dictionary, bool compress);

#ifdef __cplusplus
}
#endif

#endif /* _PRONUNCIATIONENGINE_H_ */