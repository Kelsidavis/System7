/*
 * chicago_accents_data.c - marks Chicago needs that are shapes, not letters
 *
 * Chicago has A, E, a, e and the rest already; what it lacked were the marks
 * that go over them, a bullet, and an ellipsis. The extended strike that used
 * to supply whole accented glyphs had them badly wrong - an a-grave with no
 * "a" in it at all, an E-acute whose accent sat below the baseline, a
 * "bullet" that was a zigzag - so the letters are taken from Chicago itself
 * now and only these marks are drawn, the same choice already made for the
 * command symbol in menus. Each is a geometric shape with one obvious form,
 * which is why drawing them cannot produce the wrong character the way a
 * hand-drawn letterform can.
 *
 * The marks share a strike laid out exactly like Chicago's own, so their rows
 * line up with the letter they sit over and the ordinary glyph blitter draws
 * them with no special case. Upper and lower variants differ only in which
 * rows they use: capitals start at row 3 and lowercase at row 5, so a mark
 * for each sits just above. The bullet is centred on the x-height and the
 * ellipsis sits on the baseline, since both stand on their own.
 */

#include "chicago_font_extended.h"

const ChicagoCharInfo chicago_accents[kChicagoAccentSlots] = {
    /* acute lower        */ {   0, 5, 0, 0 },
    /* acute upper        */ {   5, 5, 0, 0 },
    /* grave lower        */ {  10, 5, 0, 0 },
    /* grave upper        */ {  15, 5, 0, 0 },
    /* circumflex lower   */ {  20, 5, 0, 0 },
    /* circumflex upper   */ {  25, 5, 0, 0 },
    /* tilde lower        */ {  30, 5, 0, 0 },
    /* tilde upper        */ {  35, 5, 0, 0 },
    /* dieresis lower     */ {  40, 5, 0, 0 },
    /* dieresis upper     */ {  45, 5, 0, 0 },
    /* ring lower         */ {  50, 5, 0, 0 },
    /* ring upper         */ {  55, 5, 0, 0 },
    /* cedilla below      */ {  60, 5, 0, 0 },
    /* bullet xmid        */ {  65, 4, 0, 0 },
    /* ellipsis baseline  */ {  69, 8, 0, 0 },
    /* spare              */ {   0, 0, 0, 0 },
};

const uint8_t chicago_accent_bitmap[CHICAGO_HEIGHT * CHICAGO_ACCENT_ROW_BYTES] = {
    /* row  0 */ 0x00, 0xC1, 0x80, 0x10, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00,
    /* row  1 */ 0x01, 0x80, 0xC0, 0x38, 0x0B, 0x06, 0xC0, 0x90, 0x00, 0x00,
    /* row  2 */ 0x1B, 0x30, 0x62, 0x6C, 0x1A, 0x06, 0xD8, 0xC0, 0x00, 0x00,
    /* row  3 */ 0x30, 0x18, 0x07, 0x01, 0x60, 0xD8, 0x12, 0x00, 0x00, 0x00,
    /* row  4 */ 0x60, 0x0C, 0x0D, 0x83, 0x40, 0xD8, 0x18, 0x00, 0x00, 0x00,
    /* row  5 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* row  6 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00,
    /* row  7 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00,
    /* row  8 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00,
    /* row  9 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00,
    /* row 10 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xD8,
    /* row 11 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xD8,
    /* row 12 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
    /* row 13 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00,
    /* row 14 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
};
