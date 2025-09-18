/*
 * File: TextToSpeech.c
 *
 * Contains: Text-to-speech conversion and processing implementation
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This file implements text processing, analysis, and
 *              conversion for speech synthesis.
 */

#include "../include/SpeechManager/TextToSpeech.h"
#include "../include/SpeechManager/SpeechChannels.h"
#include "../include/SpeechManager/PronunciationEngine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

/* ===== Text Processing Globals ===== */

/* Maximum text processing limits */
#define MAX_TEXT_LENGTH 65536
#define MAX_SEGMENTS 1024
#define MAX_PHONEME_LENGTH 8192
#define MAX_WORD_LENGTH 256

/* Default command delimiters */
#define DEFAULT_START_DELIMITER "[["
#define DEFAULT_END_DELIMITER "]]"

/* Text processing state */
typedef struct TextProcessorState {
    bool initialized;
    regex_t numberRegex;
    regex_t abbreviationRegex;
    regex_t punctuationRegex;
    regex_t markupRegex;
    void *defaultDictionary;
    void *abbreviationDictionary;
} TextProcessorState;

static TextProcessorState gTextProcessor = {
    .initialized = false,
    .defaultDictionary = NULL,
    .abbreviationDictionary = NULL
};

/* Built-in abbreviation expansions */
typedef struct AbbreviationEntry {
    const char *abbrev;
    const char *expansion;
} AbbreviationEntry;

static const AbbreviationEntry kBuiltinAbbreviations[] = {
    {"Dr.", "Doctor"},
    {"Mr.", "Mister"},
    {"Mrs.", "Misses"},
    {"Ms.", "Miss"},
    {"Inc.", "Incorporated"},
    {"Corp.", "Corporation"},
    {"Ltd.", "Limited"},
    {"Ave.", "Avenue"},
    {"St.", "Street"},
    {"Rd.", "Road"},
    {"Blvd.", "Boulevard"},
    {"etc.", "et cetera"},
    {"vs.", "versus"},
    {"i.e.", "that is"},
    {"e.g.", "for example"},
    {NULL, NULL}
};

/* Number words for conversion */
static const char *kOnesWords[] = {
    "", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
    "seventeen", "eighteen", "nineteen"
};

static const char *kTensWords[] = {
    "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"
};

/* ===== Internal Functions ===== */

/*
 * InitializeTextProcessor
 * Initializes text processing subsystem
 */
static OSErr InitializeTextProcessor(void) {
    if (gTextProcessor.initialized) {
        return noErr;
    }

    /* Compile regular expressions */
    int result = regcomp(&gTextProcessor.numberRegex, "[0-9]+", REG_EXTENDED);
    if (result != 0) {
        return memFullErr;
    }

    result = regcomp(&gTextProcessor.abbreviationRegex, "[A-Za-z]+\\.", REG_EXTENDED);
    if (result != 0) {
        regfree(&gTextProcessor.numberRegex);
        return memFullErr;
    }

    result = regcomp(&gTextProcessor.punctuationRegex, "[.,;:!?()\"'-]", REG_EXTENDED);
    if (result != 0) {
        regfree(&gTextProcessor.numberRegex);
        regfree(&gTextProcessor.abbreviationRegex);
        return memFullErr;
    }

    result = regcomp(&gTextProcessor.markupRegex, "<[^>]*>", REG_EXTENDED);
    if (result != 0) {
        regfree(&gTextProcessor.numberRegex);
        regfree(&gTextProcessor.abbreviationRegex);
        regfree(&gTextProcessor.punctuationRegex);
        return memFullErr;
    }

    gTextProcessor.initialized = true;
    return noErr;
}

/*
 * CleanupTextProcessor
 * Cleans up text processing resources
 */
static void CleanupTextProcessor(void) {
    if (!gTextProcessor.initialized) {
        return;
    }

    regfree(&gTextProcessor.numberRegex);
    regfree(&gTextProcessor.abbreviationRegex);
    regfree(&gTextProcessor.punctuationRegex);
    regfree(&gTextProcessor.markupRegex);

    if (gTextProcessor.defaultDictionary) {
        UnloadTextDictionary(gTextProcessor.defaultDictionary);
        gTextProcessor.defaultDictionary = NULL;
    }

    if (gTextProcessor.abbreviationDictionary) {
        UnloadTextDictionary(gTextProcessor.abbreviationDictionary);
        gTextProcessor.abbreviationDictionary = NULL;
    }

    gTextProcessor.initialized = false;
}

/*
 * IsWhitespace
 * Checks if a character is whitespace
 */
static bool IsWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/*
 * SkipWhitespace
 * Skips whitespace characters in text
 */
static const char *SkipWhitespace(const char *text) {
    while (*text && IsWhitespace(*text)) {
        text++;
    }
    return text;
}

/*
 * FindWordEnd
 * Finds the end of a word in text
 */
static const char *FindWordEnd(const char *text) {
    while (*text && !IsWhitespace(*text) && !ispunct(*text)) {
        text++;
    }
    return text;
}

/*
 * ConvertNumberToWords
 * Converts a numeric string to words
 */
static OSErr ConvertNumberToWords(const char *number, char *words, long wordsSize) {
    if (!number || !words || wordsSize < 1) {
        return paramErr;
    }

    long num = atol(number);
    words[0] = '\0';

    if (num == 0) {
        strncpy(words, "zero", wordsSize - 1);
        words[wordsSize - 1] = '\0';
        return noErr;
    }

    if (num < 0) {
        strncat(words, "negative ", wordsSize - strlen(words) - 1);
        num = -num;
    }

    /* Handle thousands */
    if (num >= 1000) {
        long thousands = num / 1000;
        if (thousands < 20) {
            strncat(words, kOnesWords[thousands], wordsSize - strlen(words) - 1);
        } else {
            strncat(words, kTensWords[thousands / 10], wordsSize - strlen(words) - 1);
            if (thousands % 10 > 0) {
                strncat(words, " ", wordsSize - strlen(words) - 1);
                strncat(words, kOnesWords[thousands % 10], wordsSize - strlen(words) - 1);
            }
        }
        strncat(words, " thousand", wordsSize - strlen(words) - 1);
        num %= 1000;
        if (num > 0) {
            strncat(words, " ", wordsSize - strlen(words) - 1);
        }
    }

    /* Handle hundreds */
    if (num >= 100) {
        long hundreds = num / 100;
        strncat(words, kOnesWords[hundreds], wordsSize - strlen(words) - 1);
        strncat(words, " hundred", wordsSize - strlen(words) - 1);
        num %= 100;
        if (num > 0) {
            strncat(words, " ", wordsSize - strlen(words) - 1);
        }
    }

    /* Handle tens and ones */
    if (num >= 20) {
        strncat(words, kTensWords[num / 10], wordsSize - strlen(words) - 1);
        if (num % 10 > 0) {
            strncat(words, " ", wordsSize - strlen(words) - 1);
            strncat(words, kOnesWords[num % 10], wordsSize - strlen(words) - 1);
        }
    } else if (num > 0) {
        strncat(words, kOnesWords[num], wordsSize - strlen(words) - 1);
    }

    return noErr;
}

/*
 * ExpandAbbreviation
 * Expands a known abbreviation
 */
static OSErr ExpandAbbreviation(const char *abbrev, char *expansion, long expansionSize) {
    if (!abbrev || !expansion || expansionSize < 1) {
        return paramErr;
    }

    /* Search built-in abbreviations */
    for (int i = 0; kBuiltinAbbreviations[i].abbrev != NULL; i++) {
        if (strcmp(abbrev, kBuiltinAbbreviations[i].abbrev) == 0) {
            strncpy(expansion, kBuiltinAbbreviations[i].expansion, expansionSize - 1);
            expansion[expansionSize - 1] = '\0';
            return noErr;
        }
    }

    /* If not found, return original */
    strncpy(expansion, abbrev, expansionSize - 1);
    expansion[expansionSize - 1] = '\0';
    return noErr;
}

/*
 * AnalyzeTextSegment
 * Analyzes a text segment to determine its type
 */
static TextAnalysisType AnalyzeTextSegment(const char *text, long length) {
    if (!text || length <= 0) {
        return kTextAnalysis_Unknown;
    }

    /* Check for numbers */
    bool isNumber = true;
    for (long i = 0; i < length; i++) {
        if (!isdigit(text[i]) && text[i] != '.' && text[i] != ',') {
            isNumber = false;
            break;
        }
    }
    if (isNumber) {
        return kTextAnalysis_Number;
    }

    /* Check for punctuation */
    bool isPunctuation = true;
    for (long i = 0; i < length; i++) {
        if (!ispunct(text[i])) {
            isPunctuation = false;
            break;
        }
    }
    if (isPunctuation) {
        return kTextAnalysis_Punctuation;
    }

    /* Check for abbreviation */
    if (length > 1 && text[length - 1] == '.') {
        bool hasLetters = false;
        for (long i = 0; i < length - 1; i++) {
            if (isalpha(text[i])) {
                hasLetters = true;
                break;
            }
        }
        if (hasLetters) {
            return kTextAnalysis_Abbreviation;
        }
    }

    /* Check for markup */
    if (length > 2 && text[0] == '<' && text[length - 1] == '>') {
        return kTextAnalysis_Markup;
    }

    /* Default to word */
    return kTextAnalysis_Word;
}

/* ===== Public API Implementation ===== */

/*
 * CreateTextProcessingContext
 * Creates a new text processing context
 */
OSErr CreateTextProcessingContext(TextProcessingContext **context) {
    if (!context) {
        return paramErr;
    }

    OSErr err = InitializeTextProcessor();
    if (err != noErr) {
        return err;
    }

    *context = malloc(sizeof(TextProcessingContext));
    if (!*context) {
        return memFullErr;
    }

    /* Initialize with defaults */
    memset(*context, 0, sizeof(TextProcessingContext));
    (*context)->inputMode = kTextMode_Normal;
    (*context)->flags = kTextFlag_ProcessNumbers | kTextFlag_ProcessAbbrev | kTextFlag_ProcessPunctuation;
    (*context)->language = 0; /* Default language */
    (*context)->region = 0;   /* Default region */
    strcpy((*context)->commandDelimiters, DEFAULT_START_DELIMITER DEFAULT_END_DELIMITER);

    return noErr;
}

/*
 * DisposeTextProcessingContext
 * Disposes of a text processing context
 */
OSErr DisposeTextProcessingContext(TextProcessingContext *context) {
    if (!context) {
        return paramErr;
    }

    /* Free any allocated resources */
    if (context->dictionary) {
        UnloadTextDictionary(context->dictionary);
    }
    if (context->abbreviationDict) {
        UnloadTextDictionary(context->abbreviationDict);
    }

    free(context);
    return noErr;
}

/*
 * AnalyzeText
 * Analyzes text and returns segmentation information
 */
OSErr AnalyzeText(const char *text, long textLength, TextProcessingContext *context,
                  TextAnalysisResult **result) {
    if (!text || textLength <= 0 || !context || !result) {
        return paramErr;
    }

    *result = malloc(sizeof(TextAnalysisResult));
    if (!*result) {
        return memFullErr;
    }

    memset(*result, 0, sizeof(TextAnalysisResult));

    /* Allocate segments array */
    (*result)->segments = malloc(MAX_SEGMENTS * sizeof(TextSegment));
    if (!(*result)->segments) {
        free(*result);
        *result = NULL;
        return memFullErr;
    }

    /* Analyze text into segments */
    const char *current = text;
    const char *end = text + textLength;
    long segmentCount = 0;

    while (current < end && segmentCount < MAX_SEGMENTS) {
        /* Skip whitespace */
        current = SkipWhitespace(current);
        if (current >= end) break;

        /* Find end of current segment */
        const char *segmentStart = current;
        const char *segmentEnd = FindWordEnd(current);

        if (segmentEnd == segmentStart) {
            /* Single character (probably punctuation) */
            segmentEnd = segmentStart + 1;
        }

        /* Create segment */
        TextSegment *segment = &(*result)->segments[segmentCount];
        segment->text = segmentStart;
        segment->length = segmentEnd - segmentStart;
        segment->position = segmentStart - text;
        segment->type = AnalyzeTextSegment(segmentStart, segment->length);
        segment->shouldSpeak = true;
        segment->priority = 1;
        segment->metadata = NULL;

        segmentCount++;
        current = segmentEnd;
    }

    (*result)->segmentCount = segmentCount;
    (*result)->totalLength = textLength;
    (*result)->errorCode = noErr;

    return noErr;
}

/*
 * DisposeTextAnalysisResult
 * Disposes of text analysis results
 */
OSErr DisposeTextAnalysisResult(TextAnalysisResult *result) {
    if (!result) {
        return paramErr;
    }

    if (result->segments) {
        free(result->segments);
    }
    if (result->errorMessage) {
        free(result->errorMessage);
    }

    free(result);
    return noErr;
}

/*
 * NormalizeText
 * Normalizes text for speech processing
 */
OSErr NormalizeText(const char *inputText, long inputLength, TextProcessingContext *context,
                    char **outputText, long *outputLength) {
    if (!inputText || inputLength <= 0 || !context || !outputText || !outputLength) {
        return paramErr;
    }

    /* Allocate output buffer */
    *outputText = malloc(MAX_TEXT_LENGTH);
    if (!*outputText) {
        return memFullErr;
    }

    char *output = *outputText;
    long outputPos = 0;
    const char *input = inputText;
    const char *inputEnd = inputText + inputLength;

    while (input < inputEnd && outputPos < MAX_TEXT_LENGTH - 1) {
        /* Skip command delimiters if not processing them */
        if (strncmp(input, context->commandDelimiters, 2) == 0) {
            /* Find end delimiter */
            const char *endDelim = strstr(input + 2, &context->commandDelimiters[2]);
            if (endDelim) {
                input = endDelim + 2;
                continue;
            }
        }

        /* Copy character, normalizing case if requested */
        char c = *input;
        if (context->flags & kTextFlag_NormalizeCasing) {
            c = tolower(c);
        }

        output[outputPos++] = c;
        input++;
    }

    output[outputPos] = '\0';
    *outputLength = outputPos;

    return noErr;
}

/*
 * ProcessNumbers
 * Processes numbers in text, converting them to words
 */
OSErr ProcessNumbers(const char *inputText, long inputLength, TextProcessingContext *context,
                     char **outputText, long *outputLength) {
    if (!inputText || inputLength <= 0 || !context || !outputText || !outputLength) {
        return paramErr;
    }

    if (!(context->flags & kTextFlag_ProcessNumbers)) {
        /* Just copy input to output */
        *outputText = malloc(inputLength + 1);
        if (!*outputText) {
            return memFullErr;
        }
        memcpy(*outputText, inputText, inputLength);
        (*outputText)[inputLength] = '\0';
        *outputLength = inputLength;
        return noErr;
    }

    /* Allocate output buffer */
    *outputText = malloc(MAX_TEXT_LENGTH);
    if (!*outputText) {
        return memFullErr;
    }

    char *output = *outputText;
    long outputPos = 0;
    const char *input = inputText;
    const char *inputEnd = inputText + inputLength;

    while (input < inputEnd && outputPos < MAX_TEXT_LENGTH - 256) {
        if (isdigit(*input)) {
            /* Extract number */
            const char *numberStart = input;
            while (input < inputEnd && isdigit(*input)) {
                input++;
            }

            /* Convert number to words */
            char numberStr[32];
            long numberLen = input - numberStart;
            if (numberLen < sizeof(numberStr)) {
                memcpy(numberStr, numberStart, numberLen);
                numberStr[numberLen] = '\0';

                char numberWords[256];
                OSErr err = ConvertNumberToWords(numberStr, numberWords, sizeof(numberWords));
                if (err == noErr) {
                    long wordsLen = strlen(numberWords);
                    if (outputPos + wordsLen < MAX_TEXT_LENGTH - 1) {
                        strcpy(output + outputPos, numberWords);
                        outputPos += wordsLen;
                    }
                }
            }
        } else {
            /* Copy non-digit character */
            output[outputPos++] = *input++;
        }
    }

    output[outputPos] = '\0';
    *outputLength = outputPos;

    return noErr;
}

/*
 * ExpandAbbreviations
 * Expands abbreviations in text
 */
OSErr ExpandAbbreviations(const char *inputText, long inputLength, TextProcessingContext *context,
                          char **outputText, long *outputLength) {
    if (!inputText || inputLength <= 0 || !context || !outputText || !outputLength) {
        return paramErr;
    }

    if (!(context->flags & kTextFlag_ProcessAbbrev)) {
        /* Just copy input to output */
        *outputText = malloc(inputLength + 1);
        if (!*outputText) {
            return memFullErr;
        }
        memcpy(*outputText, inputText, inputLength);
        (*outputText)[inputLength] = '\0';
        *outputLength = inputLength;
        return noErr;
    }

    /* Allocate output buffer */
    *outputText = malloc(MAX_TEXT_LENGTH);
    if (!*outputText) {
        return memFullErr;
    }

    char *output = *outputText;
    long outputPos = 0;
    const char *input = inputText;
    const char *inputEnd = inputText + inputLength;

    while (input < inputEnd && outputPos < MAX_TEXT_LENGTH - 256) {
        /* Skip whitespace */
        while (input < inputEnd && IsWhitespace(*input)) {
            output[outputPos++] = *input++;
        }

        if (input >= inputEnd) break;

        /* Check for abbreviation */
        const char *wordStart = input;
        const char *wordEnd = FindWordEnd(input);

        if (wordEnd > wordStart && wordEnd < inputEnd && *(wordEnd - 1) == '.') {
            /* Potential abbreviation */
            char abbrev[MAX_WORD_LENGTH];
            long abbrevLen = wordEnd - wordStart;
            if (abbrevLen < sizeof(abbrev)) {
                memcpy(abbrev, wordStart, abbrevLen);
                abbrev[abbrevLen] = '\0';

                char expansion[MAX_WORD_LENGTH];
                OSErr err = ExpandAbbreviation(abbrev, expansion, sizeof(expansion));
                if (err == noErr) {
                    long expansionLen = strlen(expansion);
                    if (outputPos + expansionLen < MAX_TEXT_LENGTH - 1) {
                        strcpy(output + outputPos, expansion);
                        outputPos += expansionLen;
                    }
                    input = wordEnd;
                    continue;
                }
            }
        }

        /* Copy word as-is */
        while (input < wordEnd && outputPos < MAX_TEXT_LENGTH - 1) {
            output[outputPos++] = *input++;
        }
    }

    output[outputPos] = '\0';
    *outputLength = outputPos;

    return noErr;
}

/*
 * TextToPhonemes
 * Converts text to phonemes using speech channel
 */
OSErr TextToPhonemes(SpeechChannel chan, void *textBuf, long textBytes,
                     void **phonemeBuf, long *phonemeBytes) {
    if (!chan || !textBuf || textBytes <= 0 || !phonemeBuf || !phonemeBytes) {
        return paramErr;
    }

    /* Allocate phoneme buffer */
    *phonemeBuf = malloc(MAX_PHONEME_LENGTH);
    if (!*phonemeBuf) {
        return memFullErr;
    }

    /* Convert text to phonemes using pronunciation engine */
    OSErr err = ConvertTextToPhonemeString((const char *)textBuf, textBytes,
                                           (char *)*phonemeBuf, MAX_PHONEME_LENGTH,
                                           phonemeBytes);

    if (err != noErr) {
        free(*phonemeBuf);
        *phonemeBuf = NULL;
        *phonemeBytes = 0;
    }

    return err;
}

/*
 * LoadTextDictionary
 * Loads a pronunciation dictionary from file
 */
OSErr LoadTextDictionary(const char *dictionaryPath, void **dictionary) {
    if (!dictionaryPath || !dictionary) {
        return paramErr;
    }

    /* For now, just create an empty dictionary placeholder */
    *dictionary = malloc(sizeof(int)); /* Placeholder */
    if (!*dictionary) {
        return memFullErr;
    }

    return noErr;
}

/*
 * UnloadTextDictionary
 * Unloads a pronunciation dictionary
 */
OSErr UnloadTextDictionary(void *dictionary) {
    if (dictionary) {
        free(dictionary);
    }
    return noErr;
}

/* Module cleanup */
static void __attribute__((destructor)) TextToSpeechCleanup(void) {
    CleanupTextProcessor();
}