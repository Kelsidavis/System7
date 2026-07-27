/*
 * chicago_font_extended.h - Extended Chicago font for Mac Roman 0x80-0xFF
 *
 * Provides glyph metrics and bitmap data for Mac Roman high characters,
 * enabling rendering of accented European characters (French, German, Spanish, etc.)
 *
 * Mac Roman 0x80-0xFF character map:
 *   0x80: A-dieresis     0x81: A-ring        0x82: C-cedilla    0x83: E-acute
 *   0x84: N-tilde        0x85: O-dieresis     0x86: U-dieresis   0x87: a-acute
 *   0x88: a-grave         0x89: a-circumflex   0x8A: a-dieresis   0x8B: a-tilde
 *   0x8C: a-ring          0x8D: c-cedilla      0x8E: e-acute      0x8F: e-grave
 *   0x90: e-circumflex    0x91: e-dieresis      0x92: i-acute      0x93: i-grave
 *   0x94: i-circumflex    0x95: i-dieresis      0x96: n-tilde      0x97: o-acute
 *   0x98: o-grave         0x99: o-circumflex    0x9A: o-dieresis   0x9B: o-tilde
 *   0x9C: u-acute         0x9D: u-grave         0x9E: u-circumflex 0x9F: u-dieresis
 *   ...
 *   0xC0: inverted ?      0xC1: inverted !      0xC7: guillemet << 0xC8: guillemet >>
 *   0xC9: ellipsis        0xCA: non-breaking sp
 *   ...
 *   0xD2: open double quote  0xD3: close double quote
 *   0xD4: open single quote  0xD5: close single quote
 */

#ifndef CHICAGO_FONT_EXTENDED_H
#define CHICAGO_FONT_EXTENDED_H

#include <stdint.h>
#include <stddef.h>
#include "chicago_font.h"

/* Extended character info for Mac Roman 0x80-0xFF (128 entries) */
extern const ChicagoCharInfo chicago_extended[128];

/* Extended strike bitmap data */
#define CHICAGO_EXT_ROW_BYTES 96  /* 96 bytes per row (768 bits) */
extern const uint8_t chicago_ext_bitmap[CHICAGO_HEIGHT * CHICAGO_EXT_ROW_BYTES];

/*
 * Chicago_Glyph - the glyph for a Mac Roman byte, whichever strike holds it
 *
 * Chicago's glyphs live in two tables with two strike bitmaps, and every
 * caller that wanted one used to pick a table itself. Only the ASCII half was
 * ever wired into the drawing path, so text outside 32..126 - accented
 * letters, the curly quotes System 7 writes its dialogs with, the ellipsis in
 * a menu item - drew as nothing at all, while the data to draw it sat in the
 * other table unused. The command symbol beside a menu shortcut hit this and
 * got its own hand-drawn workaround; the apostrophe in "Don't Save" hit it
 * next. Asking one question in one place is what stops the next character
 * finding it again.
 *
 * Returns NULL when the byte has no glyph, which callers must treat as
 * "draw nothing" rather than "draw whatever is at index zero".
 */
/*
 * Chicago_AsciiFallback - the ASCII character to draw when Mac Roman's is missing
 *
 * The extended strike in this tree has bitmaps for the accented letters but
 * not for the typographic punctuation, and System 7's own interface strings
 * are full of the latter - the apostrophe in "Don't Save", the curly quotes
 * around a document's name in every save prompt. Drawing nothing leaves a
 * hole in the middle of a word, which reads as a bug in the text rather than
 * a gap in the font.
 *
 * These substitutions are the plain forms of the same punctuation, so the
 * words stay readable and the shapes stay honest - nothing here invents a
 * glyph that Chicago has. As soon as a real strike supplies the character,
 * the substitution stops being reached, because the lookup below tries the
 * genuine glyph first.
 */
static inline unsigned char Chicago_AsciiFallback(unsigned char ch)
{
    switch (ch) {
        case 0xD0: case 0xD1: return '-';   /* en dash, em dash */
        case 0xD2: case 0xD3: return '"';   /* curly double quotes */
        case 0xC7: case 0xC8: return '"';   /* guillemets */
        case 0xD4: case 0xD5: return '\'';  /* curly single quotes */
        case 0xDF: return '.';              /* middle dot */
        case 0xD6: return '/';              /* division sign */
        case 0xF3: return 'i';              /* dotless i */

        /*
         * Nothing else gets one. A dagger is not a plus sign and an OE
         * ligature is not the letter O; substituting them would put a
         * character on screen that the string never asked for, which is a
         * worse lie than the gap it replaces. Those still draw as nothing
         * until Chicago's own glyphs are available.
         */
        default:   return 0;
    }
}

static inline const ChicagoCharInfo* Chicago_Glyph(unsigned char ch,
                                                   const uint8_t** outStrike,
                                                   int* outRowBytes)
{
    const ChicagoCharInfo* info = NULL;
    const uint8_t* strike = NULL;
    int rowBytes = 0;

    if (ch >= 32 && ch <= 126) {
        info = &chicago_ascii[ch - 32];
        strike = chicago_bitmap;
        rowBytes = CHICAGO_ROW_BYTES;
    } else if (ch >= 0x80) {
        info = &chicago_extended[ch - 0x80];
        strike = chicago_ext_bitmap;
        rowBytes = CHICAGO_EXT_ROW_BYTES;
    }

    /* The extended table has an entry for all 128 codes but a bitmap for only
     * some; a zero width means there is nothing to draw. Where a plain ASCII
     * equivalent exists, draw that instead of leaving a hole. */
    if (!info || info->bit_width == 0) {
        unsigned char sub = Chicago_AsciiFallback(ch);
        if (sub == 0) {
            return NULL;
        }
        info = &chicago_ascii[sub - 32];
        strike = chicago_bitmap;
        rowBytes = CHICAGO_ROW_BYTES;
    }

    if (outStrike) *outStrike = strike;
    if (outRowBytes) *outRowBytes = rowBytes;
    return info;
}

#endif /* CHICAGO_FONT_EXTENDED_H */
