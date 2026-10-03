/*
 * CharacterClassification.c - Character Classification and Conversion
 *
 * Implements character classification and case conversion functions for
 * Mac OS String Package. These utilities test character types and convert
 * between uppercase and lowercase.
 *
 * Based on Inside Macintosh: Text
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations */
Boolean IsLower(char ch);
Boolean IsUpper(char ch);
char ToLower(char ch);
char ToUpper(char ch);
Boolean IsAlpha(char ch);
Boolean IsDigit(char ch);
Boolean IsAlphaNum(char ch);
Boolean IsSpace(char ch);
Boolean IsPunct(char ch);

/* Debug logging */
#define CHAR_CLASS_DEBUG 0

#if CHAR_CLASS_DEBUG
#define CHARCLASS_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[CharClass] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define CHARCLASS_LOG(...)
#endif

/*
 * IsLower - Test if character is lowercase letter
 *
 * Tests whether a character is a lowercase letter (a-z). This uses the
 * standard ASCII definition.
 *
 * Parameters:
 *   ch - Character to test
 *
 * Returns:
 *   true if ch is lowercase letter (a-z), false otherwise
 *
 * Example:
 *   IsLower('a') -> true
 *   IsLower('A') -> false
 *   IsLower('5') -> false
 *
 * Based on Inside Macintosh: Text
 */
Boolean IsLower(char ch) {
    return (ch >= 'a' && ch <= 'z');
}

/*
 * IsUpper - Test if character is uppercase letter
 *
 * Tests whether a character is an uppercase letter (A-Z). This uses the
 * standard ASCII definition.
 *
 * Parameters:
 *   ch - Character to test
 *
 * Returns:
 *   true if ch is uppercase letter (A-Z), false otherwise
 *
 * Example:
 *   IsUpper('A') -> true
 *   IsUpper('a') -> false
 *   IsUpper('5') -> false
 *
 * Based on Inside Macintosh: Text
 */
Boolean IsUpper(char ch) {
    return (ch >= 'A' && ch <= 'Z');
}

/*
 * ToLower - Convert character to lowercase
 *
 * Converts an uppercase letter to its lowercase equivalent. If the
 * character is not an uppercase letter, it is returned unchanged.
 *
 * Parameters:
 *   ch - Character to convert
 *
 * Returns:
 *   Lowercase equivalent if ch is uppercase, otherwise ch unchanged
 *
 * Example:
 *   ToLower('A') -> 'a'
 *   ToLower('a') -> 'a'
 *   ToLower('5') -> '5'
 *
 * Based on Inside Macintosh: Text
 */
char ToLower(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch + ('a' - 'A');
    }
    return ch;
}

/*
 * ToUpper - Convert character to uppercase
 *
 * Converts a lowercase letter to its uppercase equivalent. If the
 * character is not a lowercase letter, it is returned unchanged.
 *
 * Parameters:
 *   ch - Character to convert
 *
 * Returns:
 *   Uppercase equivalent if ch is lowercase, otherwise ch unchanged
 *
 * Example:
 *   ToUpper('a') -> 'A'
 *   ToUpper('A') -> 'A'
 *   ToUpper('5') -> '5'
 *
 * Based on Inside Macintosh: Text
 */
char ToUpper(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return ch - ('a' - 'A');
    }
    return ch;
}

/*
 * IsAlpha - Test if character is alphabetic letter
 *
 * Tests whether a character is an alphabetic letter (A-Z or a-z).
 *
 * Parameters:
 *   ch - Character to test
 *
 * Returns:
 *   true if ch is alphabetic letter, false otherwise
 *
 * Example:
 *   IsAlpha('a') -> true
 *   IsAlpha('Z') -> true
 *   IsAlpha('5') -> false
 *   IsAlpha(' ') -> false
 *
 * Based on Inside Macintosh: Text
 */
Boolean IsAlpha(char ch) {
    return ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'));
}

/*
 * IsDigit - Test if character is decimal digit
 *
 * Tests whether a character is a decimal digit (0-9).
 *
 * Parameters:
 *   ch - Character to test
 *
 * Returns:
 *   true if ch is digit (0-9), false otherwise
 *
 * Example:
 *   IsDigit('0') -> true
 *   IsDigit('9') -> true
 *   IsDigit('a') -> false
 *   IsDigit(' ') -> false
 *
 * Based on Inside Macintosh: Text
 */
Boolean IsDigit(char ch) {
    return (ch >= '0' && ch <= '9');
}

/*
 * IsAlphaNum - Test if character is alphanumeric
 *
 * Tests whether a character is alphanumeric (A-Z, a-z, or 0-9).
 *
 * Parameters:
 *   ch - Character to test
 *
 * Returns:
 *   true if ch is alphanumeric, false otherwise
 *
 * Example:
 *   IsAlphaNum('a') -> true
 *   IsAlphaNum('Z') -> true
 *   IsAlphaNum('5') -> true
 *   IsAlphaNum(' ') -> false
 *   IsAlphaNum('!') -> false
 *
 * Based on Inside Macintosh: Text
 */
Boolean IsAlphaNum(char ch) {
    return IsAlpha(ch) || IsDigit(ch);
}

/*
 * IsSpace - Test if character is whitespace
 *
 * Tests whether a character is whitespace. This includes:
 * - Space (0x20)
 * - Tab (0x09)
 * - Newline/Line Feed (0x0A)
 * - Carriage Return (0x0D)
 * - Form Feed (0x0C)
 * - Vertical Tab (0x0B)
 *
 * Parameters:
 *   ch - Character to test
 *
 * Returns:
 *   true if ch is whitespace, false otherwise
 *
 * Example:
 *   IsSpace(' ') -> true
 *   IsSpace('\t') -> true
 *   IsSpace('\n') -> true
 *   IsSpace('a') -> false
 *
 * Based on Inside Macintosh: Text
 */
Boolean IsSpace(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' ||
            ch == '\r' || ch == '\f' || ch == '\v');
}

/*
 * IsPunct - Test if character is punctuation
 *
 * Tests whether a character is punctuation. This includes all printable
 * ASCII characters that are not alphanumeric or whitespace.
 *
 * Punctuation characters include:
 * ! " # $ % & ' ( ) * + , - . / : ; < = > ? @ [ \ ] ^ _ ` { | } ~
 *
 * Parameters:
 *   ch - Character to test
 *
 * Returns:
 *   true if ch is punctuation, false otherwise
 *
 * Example:
 *   IsPunct('!') -> true
 *   IsPunct('.') -> true
 *   IsPunct(',') -> true
 *   IsPunct('a') -> false
 *   IsPunct('5') -> false
 *   IsPunct(' ') -> false
 *
 * Based on Inside Macintosh: Text
 */
Boolean IsPunct(char ch) {
    /* Punctuation is printable ASCII that's not alphanumeric or space */
    if (ch < 0x21 || ch > 0x7E) {
        return false;  /* Not printable ASCII range */
    }

    /* Check if it's alphanumeric */
    if (IsAlphaNum(ch)) {
        return false;
    }

    /* It's printable and not alphanumeric, so it's punctuation */
    return true;
}
