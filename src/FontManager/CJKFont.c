/*
 * CJKFont.c - CJK Bitmap Font Support
 *
 * Manages on-demand loading and glyph lookup for CJK bitmap fonts.
 * Currently provides the infrastructure and a tofu (missing glyph)
 * fallback. Actual CJK font resources can be added as NFNT resources
 * or as embedded bitmap arrays.
 */

#include "FontManager/CJKFont.h"
#include "TextEncoding/CJKEncoding.h"

/* Forward declarations */
void InitCJKFonts(void);
OSErr LoadCJKFont(ScriptCode script);
const CJKFontData* GetCJKFont(ScriptCode script);
OSErr GetCJKGlyph(const CJKFontData *font, UInt32 glyphIndex,
                  UInt8 *outBitmap, SInt16 *outWidth, SInt16 *outHeight);
SInt16 CJKCharWidth(ScriptCode script, UInt8 lead, UInt8 trail);

/* Font slots for each CJK script */
#define kCJKSlotJapanese    0
#define kCJKSlotChinese     1
#define kCJKSlotKorean       2
#define kCJKSlotCount        3

static CJKFontData g_cjkFonts[kCJKSlotCount];

/*
 * Tofu glyph: a 12x12 hollow rectangle used as the missing-glyph
 * indicator. This is the standard CJK fallback rendering.
 */
static const UInt8 g_tofuGlyph[CJK_GLYPH_BYTES] = {
    0xFF, 0xF0,  /* row 0:  ############ */
    0x80, 0x10,  /* row 1:  #..........# */
    0x80, 0x10,  /* row 2:  #..........# */
    0x80, 0x10,  /* row 3:  #..........# */
    0x80, 0x10,  /* row 4:  #..........# */
    0x80, 0x10,  /* row 5:  #..........# */
    0x80, 0x10,  /* row 6:  #..........# */
    0x80, 0x10,  /* row 7:  #..........# */
    0x80, 0x10,  /* row 8:  #..........# */
    0x80, 0x10,  /* row 9:  #..........# */
    0x80, 0x10,  /* row 10: #..........# */
    0xFF, 0xF0,  /* row 11: ############ */
};

static SInt16 ScriptToSlot(ScriptCode script) {
    switch (script) {
        case kScriptJapanese:    return kCJKSlotJapanese;
        case kScriptSimpChinese:
        case kScriptTradChinese: return kCJKSlotChinese;
        case kScriptKorean:      return kCJKSlotKorean;
        default:                 return -1;
    }
}

void InitCJKFonts(void) {
    int i;
    for (i = 0; i < kCJKSlotCount; i++) {
        g_cjkFonts[i].script = 0;
        g_cjkFonts[i].glyphData = NULL;
        g_cjkFonts[i].glyphCount = 0;
        g_cjkFonts[i].cellWidth = CJK_GLYPH_WIDTH;
        g_cjkFonts[i].cellHeight = CJK_GLYPH_HEIGHT;
        g_cjkFonts[i].ascent = 10;
        g_cjkFonts[i].descent = 2;
        g_cjkFonts[i].leading = 2;
        g_cjkFonts[i].loaded = false;
    }
}

OSErr LoadCJKFont(ScriptCode script) {
    SInt16 slot = ScriptToSlot(script);
    if (slot < 0) return paramErr;

    if (g_cjkFonts[slot].loaded) return noErr;

    /*
     * TODO: Load actual CJK font resource data here.
     *
     * When CJK font resources are available, this function should:
     * 1. Open the font resource file for this script
     * 2. Load the bitmap strike data
     * 3. Set glyphData and glyphCount
     *
     * For now, mark as loaded with no glyph data. The tofu fallback
     * will be used for all characters.
     */
    g_cjkFonts[slot].script = script;
    g_cjkFonts[slot].loaded = true;

    return noErr;
}

const CJKFontData* GetCJKFont(ScriptCode script) {
    SInt16 slot = ScriptToSlot(script);
    if (slot < 0) return NULL;

    if (!g_cjkFonts[slot].loaded) {
        /* Attempt to load on demand */
        if (LoadCJKFont(script) != noErr)
            return NULL;
    }

    return &g_cjkFonts[slot];
}

OSErr GetCJKGlyph(const CJKFontData *font, UInt32 glyphIndex,
                  UInt8 *outBitmap, SInt16 *outWidth, SInt16 *outHeight) {
    int i;

    if (font == NULL || outBitmap == NULL) return paramErr;

    if (outWidth)  *outWidth = CJK_GLYPH_WIDTH;
    if (outHeight) *outHeight = CJK_GLYPH_HEIGHT;

    /* If we have actual font data and the index is in range, use it */
    if (font->glyphData != NULL && glyphIndex < font->glyphCount) {
        const UInt8 *src = font->glyphData + (glyphIndex * CJK_GLYPH_BYTES);
        for (i = 0; i < CJK_GLYPH_BYTES; i++) {
            outBitmap[i] = src[i];
        }
        return noErr;
    }

    /* Fallback: render tofu (missing glyph box) */
    for (i = 0; i < CJK_GLYPH_BYTES; i++) {
        outBitmap[i] = g_tofuGlyph[i];
    }
    return noErr;
}

SInt16 CJKCharWidth(ScriptCode script, UInt8 lead, UInt8 trail) {
    (void)trail;

    /* Double-byte characters are full-width */
    if (IsLeadByte(script, lead))
        return CJK_GLYPH_WIDTH;

    /* Single-byte characters within CJK text are half-width */
    return CJK_GLYPH_WIDTH / 2;
}
