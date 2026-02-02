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
#include "chicago_font.h"

/* Extended character info for Mac Roman 0x80-0xFF (128 entries) */
extern const ChicagoCharInfo chicago_extended[128];

/* Extended strike bitmap data */
#define CHICAGO_EXT_ROW_BYTES 96  /* 96 bytes per row (768 bits) */
extern const uint8_t chicago_ext_bitmap[CHICAGO_HEIGHT * CHICAGO_EXT_ROW_BYTES];

#endif /* CHICAGO_FONT_EXTENDED_H */
