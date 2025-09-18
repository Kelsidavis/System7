/*
 * File: TextToSpeech.h
 *
 * Contains: Text-to-speech conversion and processing for Speech Manager
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This header provides text-to-speech conversion functionality
 *              including text processing, phoneme conversion, and speech synthesis.
 */

#ifndef _TEXTTOSPEECH_H_
#define _TEXTTOSPEECH_H_

#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Text Processing Constants ===== */

/* Text input modes */
typedef enum {
    kTextMode_Normal    = 0,  /* Regular text processing */
    kTextMode_Literal   = 1,  /* Literal character-by-character */
    kTextMode_Phonetic  = 2,  /* Phonetic input */
    kTextMode_SSML      = 3   /* Speech Synthesis Markup Language */
} TextInputMode;

/* Text processing flags */
typedef enum {
    kTextFlag_ProcessNumbers     = (1 << 0),  /* Convert numbers to words */
    kTextFlag_ProcessAbbrev      = (1 << 1),  /* Expand abbreviations */
    kTextFlag_ProcessPunctuation = (1 << 2),  /* Handle punctuation */
    kTextFlag_ProcessMarkup      = (1 << 3),  /* Process markup tags */
    kTextFlag_NormalizeCasing    = (1 << 4),  /* Normalize text casing */
    kTextFlag_FilterNonSpeech    = (1 << 5),  /* Remove non-speech content */
    kTextFlag_PreserveTiming     = (1 << 6),  /* Preserve timing marks */
    kTextFlag_HandleEmphasis     = (1 << 7)   /* Process emphasis marks */
} TextProcessingFlags;

/* Text analysis results */
typedef enum {
    kTextAnalysis_Unknown       = 0,
    kTextAnalysis_Word          = 1,
    kTextAnalysis_Number        = 2,
    kTextAnalysis_Punctuation   = 3,
    kTextAnalysis_Abbreviation  = 4,
    kTextAnalysis_Markup        = 5,
    kTextAnalysis_Phonetic      = 6,
    kTextAnalysis_Silence       = 7
} TextAnalysisType;

/* ===== Text Structures ===== */

/* Text processing context */
typedef struct TextProcessingContext {
    TextInputMode inputMode;
    TextProcessingFlags flags;
    short language;
    short region;
    void *dictionary;
    void *abbreviationDict;
    void *numberRules;
    void *markupParser;
    char commandDelimiters[4];  /* Start and end delimiters */
    void *userData;
} TextProcessingContext;

/* Text segment for analysis */
typedef struct TextSegment {
    const char *text;
    long length;
    long position;
    TextAnalysisType type;
    long priority;
    bool shouldSpeak;
    void *metadata;
} TextSegment;

/* Text analysis result */
typedef struct TextAnalysisResult {
    TextSegment *segments;
    long segmentCount;
    long totalLength;
    long processingTime;
    OSErr errorCode;
    char *errorMessage;
} TextAnalysisResult;

/* Phoneme conversion result */
typedef struct PhonemeConversionResult {
    char *phonemeString;
    long phonemeLength;
    short *stressMarks;
    long stressCount;
    short *syllableBounds;
    long syllableCount;
    OSErr conversionError;
} PhonemeConversionResult;

/* ===== Text Processing Functions ===== */

/* Context management */
OSErr CreateTextProcessingContext(TextProcessingContext **context);
OSErr DisposeTextProcessingContext(TextProcessingContext *context);
OSErr SetTextProcessingMode(TextProcessingContext *context, TextInputMode mode);
OSErr SetTextProcessingFlags(TextProcessingContext *context, TextProcessingFlags flags);

/* Text analysis */
OSErr AnalyzeText(const char *text, long textLength, TextProcessingContext *context,
                  TextAnalysisResult **result);
OSErr DisposeTextAnalysisResult(TextAnalysisResult *result);

/* Text normalization */
OSErr NormalizeText(const char *inputText, long inputLength, TextProcessingContext *context,
                    char **outputText, long *outputLength);
OSErr ExpandAbbreviations(const char *inputText, long inputLength, TextProcessingContext *context,
                          char **outputText, long *outputLength);
OSErr ProcessNumbers(const char *inputText, long inputLength, TextProcessingContext *context,
                     char **outputText, long *outputLength);

/* Phoneme conversion */
OSErr ConvertTextToPhonemes(const char *text, long textLength, TextProcessingContext *context,
                            PhonemeConversionResult **result);
OSErr DisposePhonemeConversionResult(PhonemeConversionResult *result);

/* Dictionary support */
OSErr LoadTextDictionary(const char *dictionaryPath, void **dictionary);
OSErr UnloadTextDictionary(void *dictionary);
OSErr LookupWord(void *dictionary, const char *word, char **pronunciation);
OSErr AddWordToDictionary(void *dictionary, const char *word, const char *pronunciation);

/* Markup processing */
OSErr ProcessSSMLMarkup(const char *ssmlText, long textLength, TextProcessingContext *context,
                        char **processedText, long *processedLength);
OSErr ExtractPlainText(const char *markupText, long textLength,
                       char **plainText, long *plainLength);

/* Text segmentation */
OSErr SegmentTextIntoSentences(const char *text, long textLength,
                               TextSegment **sentences, long *sentenceCount);
OSErr SegmentTextIntoWords(const char *text, long textLength,
                           TextSegment **words, long *wordCount);
OSErr SegmentTextIntoPhrases(const char *text, long textLength, TextProcessingContext *context,
                             TextSegment **phrases, long *phraseCount);

/* ===== Speech Synthesis Text Interface ===== */

/* Text-to-speech conversion */
OSErr SpeakProcessedText(SpeechChannel chan, const char *text, long textLength,
                         TextProcessingContext *context);
OSErr SpeakTextWithCallback(SpeechChannel chan, const char *text, long textLength,
                            TextProcessingContext *context, SpeechTextDoneProcPtr callback,
                            void *userData);

/* Buffered text processing */
OSErr BeginTextProcessing(SpeechChannel chan, TextProcessingContext *context);
OSErr ProcessTextBuffer(SpeechChannel chan, const char *textBuffer, long bufferLength,
                        bool isLastBuffer);
OSErr EndTextProcessing(SpeechChannel chan);

/* Text streaming */
typedef struct TextStreamContext TextStreamContext;

OSErr CreateTextStream(SpeechChannel chan, TextProcessingContext *context,
                       TextStreamContext **stream);
OSErr WriteToTextStream(TextStreamContext *stream, const char *text, long textLength);
OSErr FlushTextStream(TextStreamContext *stream);
OSErr CloseTextStream(TextStreamContext *stream);

/* ===== Text Processing Utilities ===== */

/* Language detection */
OSErr DetectTextLanguage(const char *text, long textLength, short *language, short *confidence);
OSErr IsTextInLanguage(const char *text, long textLength, short language, bool *isMatch);

/* Text validation */
bool IsValidTextForSpeech(const char *text, long textLength);
OSErr ValidateTextEncoding(const char *text, long textLength, long *encoding);
OSErr ConvertTextEncoding(const char *inputText, long inputLength, long inputEncoding,
                          long outputEncoding, char **outputText, long *outputLength);

/* Text statistics */
OSErr GetTextStatistics(const char *text, long textLength, long *wordCount, long *sentenceCount,
                        long *characterCount, long *estimatedSpeechTime);

/* Pronunciation hints */
OSErr SetPronunciationHint(TextProcessingContext *context, const char *word,
                           const char *pronunciation);
OSErr RemovePronunciationHint(TextProcessingContext *context, const char *word);
OSErr GetPronunciationHint(TextProcessingContext *context, const char *word,
                           char **pronunciation);

/* Text emphasis and prosody */
OSErr SetTextEmphasis(TextProcessingContext *context, long startPos, long endPos,
                      short emphasisLevel);
OSErr SetTextProsody(TextProcessingContext *context, long startPos, long endPos,
                     Fixed rate, Fixed pitch, Fixed volume);
OSErr ClearTextAttributes(TextProcessingContext *context, long startPos, long endPos);

/* Text caching */
OSErr CacheProcessedText(const char *originalText, long textLength,
                         const char *processedText, long processedLength);
OSErr LookupCachedText(const char *originalText, long textLength,
                       char **processedText, long *processedLength);
OSErr ClearTextCache(void);

/* ===== Text Processing Callbacks ===== */

/* Text processing progress callback */
typedef void (*TextProcessingProgressProc)(long bytesProcessed, long totalBytes, void *userData);

/* Text analysis callback */
typedef bool (*TextAnalysisProc)(const TextSegment *segment, void *userData);

/* Pronunciation callback */
typedef bool (*PronunciationProc)(const char *word, char **pronunciation, void *userData);

/* Callback registration */
OSErr SetTextProcessingProgressCallback(TextProcessingContext *context,
                                        TextProcessingProgressProc callback, void *userData);
OSErr SetTextAnalysisCallback(TextProcessingContext *context,
                              TextAnalysisProc callback, void *userData);
OSErr SetPronunciationCallback(TextProcessingContext *context,
                               PronunciationProc callback, void *userData);

/* ===== Advanced Text Features ===== */

/* Text bookmarks */
OSErr SetTextBookmark(TextProcessingContext *context, long position, const char *name);
OSErr GetTextBookmark(TextProcessingContext *context, const char *name, long *position);
OSErr RemoveTextBookmark(TextProcessingContext *context, const char *name);

/* Text variables */
OSErr SetTextVariable(TextProcessingContext *context, const char *name, const char *value);
OSErr GetTextVariable(TextProcessingContext *context, const char *name, char **value);
OSErr ExpandTextVariables(const char *inputText, long inputLength,
                          TextProcessingContext *context, char **outputText, long *outputLength);

/* Conditional text */
OSErr ProcessConditionalText(const char *inputText, long inputLength,
                             TextProcessingContext *context, char **outputText, long *outputLength);

#ifdef __cplusplus
}
#endif

#endif /* _TEXTTOSPEECH_H_ */