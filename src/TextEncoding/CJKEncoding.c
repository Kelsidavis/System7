/*
 * CJKEncoding.c - CJK Multi-byte Encoding Support
 *
 * Implements byte-level encoding detection for Shift-JIS, GBK, and EUC-KR.
 * These are the native Mac OS encodings for Japanese, Chinese, and Korean text.
 *
 * Encoding ranges:
 *
 * Shift-JIS (Japanese):
 *   Lead bytes: 0x81-0x9F, 0xE0-0xEF
 *   Trail bytes: 0x40-0x7E, 0x80-0xFC
 *   Single-byte katakana: 0xA1-0xDF
 *
 * GBK (Simplified Chinese):
 *   Lead bytes: 0x81-0xFE
 *   Trail bytes: 0x40-0x7E, 0x80-0xFE
 *
 * EUC-KR (Korean):
 *   Lead bytes: 0xA1-0xFE
 *   Trail bytes: 0xA1-0xFE
 */

#include "TextEncoding/CJKEncoding.h"

/* Forward declarations */
Boolean IsMultiByteScript(ScriptCode script);
Boolean IsLeadByte(ScriptCode script, UInt8 b);
SInt16 CharByteCount(ScriptCode script, UInt8 b);
OSErr DecodeCJKChar(ScriptCode script, UInt8 lead, UInt8 trail, UInt32 *outIndex);

Boolean IsMultiByteScript(ScriptCode script) {
    switch (script) {
        case kScriptJapanese:
        case kScriptTradChinese:
        case kScriptKorean:
        case kScriptSimpChinese:
            return true;
        default:
            return false;
    }
}

Boolean IsLeadByte(ScriptCode script, UInt8 b) {
    switch (script) {
        case kScriptJapanese:
            /* Shift-JIS lead bytes */
            return (b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xEF);

        case kScriptSimpChinese:
        case kScriptTradChinese:
            /* GBK / Big5 lead bytes */
            return (b >= 0x81 && b <= 0xFE);

        case kScriptKorean:
            /* EUC-KR lead bytes */
            return (b >= 0xA1 && b <= 0xFE);

        default:
            return false;
    }
}

SInt16 CharByteCount(ScriptCode script, UInt8 b) {
    if (IsLeadByte(script, b))
        return 2;
    return 1;
}

OSErr DecodeCJKChar(ScriptCode script, UInt8 lead, UInt8 trail, UInt32 *outIndex) {
    if (outIndex == NULL) return paramErr;

    /* Single-byte character */
    if (!IsLeadByte(script, lead)) {
        *outIndex = lead;
        return noErr;
    }

    switch (script) {
        case kScriptJapanese: {
            /*
             * Shift-JIS to JIS X 0208 row/cell conversion:
             * Adjust lead byte to get row (ku), trail byte to get cell (ten).
             * Linear index = (row * 94) + cell
             */
            UInt8 row, cell;
            UInt8 adj_lead = lead;
            UInt8 adj_trail = trail;

            if (adj_lead >= 0xE0)
                adj_lead -= 0x40; /* shift 0xE0..0xEF -> 0xA0..0xAF */

            adj_lead -= 0x81;

            if (adj_trail >= 0x80)
                adj_trail -= 1;
            adj_trail -= 0x40;

            row = adj_lead * 2;
            if (adj_trail >= 94) {
                row += 1;
                adj_trail -= 94;
            }
            cell = adj_trail;

            *outIndex = 0x10000 + (UInt32)row * 94 + cell;
            return noErr;
        }

        case kScriptSimpChinese:
        case kScriptTradChinese: {
            /*
             * GBK/Big5: linear index from lead/trail
             * lead: 0x81-0xFE (126 values)
             * trail: 0x40-0xFE excluding 0x7F (190 values)
             */
            UInt8 adj_lead = lead - 0x81;
            UInt8 adj_trail;

            if (trail >= 0x80)
                adj_trail = trail - 0x41; /* skip 0x7F gap */
            else
                adj_trail = trail - 0x40;

            *outIndex = 0x20000 + (UInt32)adj_lead * 190 + adj_trail;
            return noErr;
        }

        case kScriptKorean: {
            /*
             * EUC-KR: lead 0xA1-0xFE, trail 0xA1-0xFE
             * 94 * 94 = 8836 code points
             */
            UInt8 row = lead - 0xA1;
            UInt8 cell = trail - 0xA1;

            *outIndex = 0x30000 + (UInt32)row * 94 + cell;
            return noErr;
        }

        default:
            *outIndex = lead;
            return paramErr;
    }
}
