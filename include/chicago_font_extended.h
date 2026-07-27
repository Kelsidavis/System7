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

/* ---- Composed accented letters ----------------------------------------- */

#define CHICAGO_ACCENT_ROW_BYTES 10
#define kChicagoAccentSlots      16

/* Slot numbers into chicago_accents[]; "upper" variants sit higher, over a
 * capital's cap height rather than a lowercase letter's x-height. */
enum {
    kAcuteLower = 0, kAcuteUpper,
    kGraveLower,     kGraveUpper,
    kCircumLower,    kCircumUpper,
    kTildeLower,     kTildeUpper,
    kDieresisLower,  kDieresisUpper,
    kRingLower,      kRingUpper,
    kCedilla,
    kNoAccent = 0xFF
};

extern const ChicagoCharInfo chicago_accents[kChicagoAccentSlots];
extern const uint8_t chicago_accent_bitmap[CHICAGO_HEIGHT * CHICAGO_ACCENT_ROW_BYTES];

/*
 * Chicago_Compose - the letter and mark a Mac Roman accented character is made of
 *
 * Returns the ASCII letter to draw and the accent slot to draw over it, or
 * base 0 when the character is not a composed letter.
 *
 * Composing is what the real font does and what the strike in this tree
 * failed at: it stored whole accented glyphs, drawn by hand and wrong - the
 * a-grave had no "a" in it, only the mark; the E-acute put its accent three
 * rows below the baseline. Taking the letter from Chicago and adding a mark
 * cannot go wrong that way, and it covers every accented character at once
 * rather than one glyph at a time.
 */
typedef struct {
    unsigned char base;    /* ASCII letter, or 0 if this is not composed */
    unsigned char accent;  /* slot in chicago_accents[] */
} ChicagoComposition;

static inline ChicagoComposition Chicago_Compose(unsigned char ch)
{
    ChicagoComposition c;
    c.base = 0;
    c.accent = kNoAccent;

    switch (ch) {
        /* Lowercase */
        case 0x87: c.base='a'; c.accent=kAcuteLower;    break;  /* a-acute */
        case 0x88: c.base='a'; c.accent=kGraveLower;    break;  /* a-grave */
        case 0x89: c.base='a'; c.accent=kCircumLower;   break;  /* a-circumflex */
        case 0x8A: c.base='a'; c.accent=kDieresisLower; break;  /* a-dieresis */
        case 0x8B: c.base='a'; c.accent=kTildeLower;    break;  /* a-tilde */
        case 0x8C: c.base='a'; c.accent=kRingLower;     break;  /* a-ring */
        case 0x8D: c.base='c'; c.accent=kCedilla;       break;  /* c-cedilla */
        case 0x8E: c.base='e'; c.accent=kAcuteLower;    break;  /* e-acute */
        case 0x8F: c.base='e'; c.accent=kGraveLower;    break;  /* e-grave */
        case 0x90: c.base='e'; c.accent=kCircumLower;   break;  /* e-circumflex */
        case 0x91: c.base='e'; c.accent=kDieresisLower; break;  /* e-dieresis */
        /* The dotted i would collide with its own mark, so accented forms use
         * the upper placement to clear the dot. */
        case 0x92: c.base='i'; c.accent=kAcuteUpper;    break;  /* i-acute */
        case 0x93: c.base='i'; c.accent=kGraveUpper;    break;  /* i-grave */
        case 0x94: c.base='i'; c.accent=kCircumUpper;   break;  /* i-circumflex */
        case 0x95: c.base='i'; c.accent=kDieresisUpper; break;  /* i-dieresis */
        case 0x96: c.base='n'; c.accent=kTildeLower;    break;  /* n-tilde */
        case 0x97: c.base='o'; c.accent=kAcuteLower;    break;  /* o-acute */
        case 0x98: c.base='o'; c.accent=kGraveLower;    break;  /* o-grave */
        case 0x99: c.base='o'; c.accent=kCircumLower;   break;  /* o-circumflex */
        case 0x9A: c.base='o'; c.accent=kDieresisLower; break;  /* o-dieresis */
        case 0x9B: c.base='o'; c.accent=kTildeLower;    break;  /* o-tilde */
        case 0x9C: c.base='u'; c.accent=kAcuteLower;    break;  /* u-acute */
        case 0x9D: c.base='u'; c.accent=kGraveLower;    break;  /* u-grave */
        case 0x9E: c.base='u'; c.accent=kCircumLower;   break;  /* u-circumflex */
        case 0x9F: c.base='u'; c.accent=kDieresisLower; break;  /* u-dieresis */
        case 0xD8: c.base='y'; c.accent=kDieresisLower; break;  /* y-dieresis */
        case 0xF5: c.base='i'; c.accent=kNoAccent;      break;  /* dotless i */

        /* Uppercase */
        case 0x80: c.base='A'; c.accent=kDieresisUpper; break;  /* A-dieresis */
        case 0x81: c.base='A'; c.accent=kRingUpper;     break;  /* A-ring */
        case 0x82: c.base='C'; c.accent=kCedilla;       break;  /* C-cedilla */
        case 0x83: c.base='E'; c.accent=kAcuteUpper;    break;  /* E-acute */
        case 0x84: c.base='N'; c.accent=kTildeUpper;    break;  /* N-tilde */
        case 0x85: c.base='O'; c.accent=kDieresisUpper; break;  /* O-dieresis */
        case 0x86: c.base='U'; c.accent=kDieresisUpper; break;  /* U-dieresis */
        case 0xCB: c.base='A'; c.accent=kGraveUpper;    break;  /* A-grave */
        case 0xCC: c.base='A'; c.accent=kTildeUpper;    break;  /* A-tilde */
        case 0xCD: c.base='O'; c.accent=kTildeUpper;    break;  /* O-tilde */
        case 0xD9: c.base='Y'; c.accent=kDieresisUpper; break;  /* Y-dieresis */
        case 0xE5: c.base='A'; c.accent=kCircumUpper;   break;  /* A-circumflex */
        case 0xE6: c.base='E'; c.accent=kCircumUpper;   break;  /* E-circumflex */
        case 0xE7: c.base='A'; c.accent=kAcuteUpper;    break;  /* A-acute */
        case 0xE8: c.base='E'; c.accent=kDieresisUpper; break;  /* E-dieresis */
        case 0xE9: c.base='E'; c.accent=kGraveUpper;    break;  /* E-grave */
        case 0xEA: c.base='I'; c.accent=kAcuteUpper;    break;  /* I-acute */
        case 0xEB: c.base='I'; c.accent=kCircumUpper;   break;  /* I-circumflex */
        case 0xEC: c.base='I'; c.accent=kDieresisUpper; break;  /* I-dieresis */
        case 0xED: c.base='I'; c.accent=kGraveUpper;    break;  /* I-grave */
        case 0xEE: c.base='O'; c.accent=kAcuteUpper;    break;  /* O-acute */
        case 0xEF: c.base='O'; c.accent=kCircumUpper;   break;  /* O-circumflex */
        case 0xF1: c.base='O'; c.accent=kGraveUpper;    break;  /* O-grave */
        case 0xF2: c.base='U'; c.accent=kAcuteUpper;    break;  /* U-acute */
        case 0xF4: c.base='U'; c.accent=kGraveUpper;    break;  /* U-grave */
        default: break;
    }
    return c;
}

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
    }

    /*
     * The extended strike is deliberately not consulted.
     *
     * Its entries do not draw the characters they name. Every one still
     * reachable was checked: "degree" and "cent" are both a bare vertical
     * bar, "bullet" is a zigzag rather than a dot, and neighbouring glyphs
     * bleed into each other's columns - the bitmap was generated against one
     * column layout and the table describes another. Composing accented
     * letters from Chicago's own glyphs covers what the strike was mainly
     * wanted for, and for the rest a blank is closer to the truth than a
     * shape that is confidently the wrong character.
     *
     * The data is left in the tree, and the moment a strike is available that
     * genuinely holds these glyphs it belongs here.
     */

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
