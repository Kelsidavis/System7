/*
 * File: TextToSpeech.h
 *
 * Contains: Text-to-speech conversion and processing for Speech Manager
 *
 *
 * Description: This header provides text-to-speech conversion functionality
 *              including text processing, phoneme conversion, and speech synthesis.
 */

#ifndef _TEXTTOSPEECH_H_
#define _TEXTTOSPEECH_H_

#include "SystemTypes.h"

#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Text Processing Constants ===== */

/* Text input modes */
#define kTextInputModePlainText   0
#define kTextInputModePhonetic    1
#define kTextInputModeSSML        2

/* Text processing flags */
#define kTextProcessFlagNone      0x0000
#define kTextProcessFlagExpand    0x0001
#define kTextProcessFlagAnalyze   0x0002
#define kTextProcessFlagNormalize 0x0004

/* Text analysis results */
#define kTextAnalysisSuccess      0
#define kTextAnalysisWarning      1
#define kTextAnalysisError        2

/* ===== Text Processing Type Definitions ===== */

/* Text input mode enumeration */
typedef short TextInputMode;

/* Text processing flags */
typedef unsigned short TextProcessingFlags;

/* Forward declarations for opaque types */
typedef struct TextProcessingContextStruct *TextProcessingContext;
typedef struct TextSegmentStruct *TextSegment;
typedef struct TextAnalysisResultStruct *TextAnalysisResult;
typedef struct PhonemeConversionResultStruct *PhonemeConversionResult;
typedef struct TextStreamContextStruct *TextStreamContext;

/* Callback types */
typedef void (*SpeechTextDoneProcPtr)(SpeechChannel chan, long refCon);
typedef void (*TextProcessingProgressProc)(long progress, long total, void *userData);
typedef void (*TextAnalysisProc)(const char *text, long textLength, void *userData);
typedef void (*PronunciationProc)(const char *word, const char *pronunciation, void *userData);

/* ===== Text Structures ===== */

/* Text processing context - opaque */

/* Text segment for analysis */

/* Text analysis result */

/* Phoneme conversion result */

/* ===== Text Processing Functions ===== */


/* Text analysis */
OSErr AnalyzeText(const char *text, long textLength, TextProcessingContext *context,
                  TextAnalysisResult **result);

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
OSErr ProcessTextBuffer(SpeechChannel chan, const char *textBuffer, long bufferLength,
                        Boolean isLastBuffer);

/* Text streaming */

OSErr CreateTextStream(SpeechChannel chan, TextProcessingContext *context,
                       TextStreamContext **stream);

/* ===== Text Processing Utilities ===== */


/* Text validation */
OSErr ConvertTextEncoding(const char *inputText, long inputLength, long inputEncoding,
                          long outputEncoding, char **outputText, long *outputLength);

/* Text statistics */
OSErr GetTextStatistics(const char *text, long textLength, long *wordCount, long *sentenceCount,
                        long *characterCount, long *estimatedSpeechTime);

/* Pronunciation hints */
OSErr SetPronunciationHint(TextProcessingContext *context, const char *word,
                           const char *pronunciation);
OSErr GetPronunciationHint(TextProcessingContext *context, const char *word,
                           char **pronunciation);

/* Text emphasis and prosody */
OSErr SetTextEmphasis(TextProcessingContext *context, long startPos, long endPos,
                      short emphasisLevel);
OSErr SetTextProsody(TextProcessingContext *context, long startPos, long endPos,
                     Fixed rate, Fixed pitch, Fixed volume);

/* Text caching */
OSErr CacheProcessedText(const char *originalText, long textLength,
                         const char *processedText, long processedLength);
OSErr LookupCachedText(const char *originalText, long textLength,
                       char **processedText, long *processedLength);

/* ===== Text Processing Callbacks ===== */

/* Text processing progress callback */

/* Text analysis callback */

/* Pronunciation callback */

/* Callback registration */
OSErr SetTextProcessingProgressCallback(TextProcessingContext *context,
                                        TextProcessingProgressProc callback, void *userData);
OSErr SetTextAnalysisCallback(TextProcessingContext *context,
                              TextAnalysisProc callback, void *userData);
OSErr SetPronunciationCallback(TextProcessingContext *context,
                               PronunciationProc callback, void *userData);

/* ===== Advanced Text Features ===== */


/* Text variables */
OSErr ExpandTextVariables(const char *inputText, long inputLength,
                          TextProcessingContext *context, char **outputText, long *outputLength);

/* Conditional text */
OSErr ProcessConditionalText(const char *inputText, long inputLength,
                             TextProcessingContext *context, char **outputText, long *outputLength);

#ifdef __cplusplus
}
#endif

#endif /* _TEXTTOSPEECH_H_ */
