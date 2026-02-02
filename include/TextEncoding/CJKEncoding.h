/*
 * CJKEncoding.h - CJK Multi-byte Encoding Support
 *
 * Provides byte-level encoding detection for CJK text encodings:
 *   - Shift-JIS (Japanese, Mac encoding 1)
 *   - GBK/GB2312 (Simplified Chinese, Mac encoding 25)
 *   - EUC-KR/KS X 1001 (Korean, Mac encoding 3)
 *
 * These functions determine whether a byte is a lead byte of a
 * multi-byte sequence, enabling correct character boundary detection
 * for text measurement and rendering.
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */

#ifndef CJK_ENCODING_H
#define CJK_ENCODING_H

#include "SystemTypes.h"

/* Script codes (matching TextEncodingUtils.c) */
#define kScriptRoman        0
#define kScriptJapanese     1
#define kScriptTradChinese  2
#define kScriptKorean       3
#define kScriptSimpChinese  25

/*
 * IsMultiByteScript - Check if a script code uses multi-byte encoding
 *
 * Returns true for Japanese, Korean, Traditional Chinese, and
 * Simplified Chinese scripts.
 */
Boolean IsMultiByteScript(ScriptCode script);

/*
 * IsLeadByte - Check if a byte is a lead byte in a multi-byte sequence
 *
 * For the given script, determines whether the byte starts a 2-byte
 * character sequence. Single-byte characters return false.
 *
 * Parameters:
 *   script - Active script code
 *   b      - The byte to test
 *
 * Returns:
 *   true if b is the first byte of a 2-byte character
 */
Boolean IsLeadByte(ScriptCode script, UInt8 b);

/*
 * CharByteCount - Get the byte count for a character
 *
 * Returns 1 for single-byte characters, 2 for double-byte characters.
 *
 * Parameters:
 *   script - Active script code
 *   b      - The first byte of the character
 *
 * Returns:
 *   1 or 2
 */
SInt16 CharByteCount(ScriptCode script, UInt8 b);

/*
 * DecodeCJKChar - Decode a multi-byte character to a code point index
 *
 * Converts a 1 or 2 byte sequence into a linear index suitable for
 * font glyph lookup. For single-byte characters the index equals the
 * byte value. For double-byte characters the index encodes both bytes.
 *
 * Parameters:
 *   script   - Active script code
 *   lead     - First (or only) byte
 *   trail    - Second byte (ignored if single-byte)
 *   outIndex - Receives the decoded glyph index
 *
 * Returns:
 *   noErr on success, paramErr if encoding is invalid
 */
OSErr DecodeCJKChar(ScriptCode script, UInt8 lead, UInt8 trail, UInt32 *outIndex);

#endif /* CJK_ENCODING_H */
