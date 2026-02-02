/*
 * CJKFont.h - CJK Bitmap Font Support
 *
 * Provides on-demand loading and rendering of CJK bitmap fonts.
 * Each CJK script (Japanese, Chinese, Korean) has its own font
 * resource containing thousands of 12x12 pixel glyphs.
 *
 * Font resources are loaded on demand when a CJK script becomes
 * active, keeping memory usage low when only Latin text is displayed.
 */

#ifndef CJK_FONT_H
#define CJK_FONT_H

#include "SystemTypes.h"

/* CJK glyph dimensions (fixed 12x12 pixel grid) */
#define CJK_GLYPH_WIDTH    12
#define CJK_GLYPH_HEIGHT   12
#define CJK_GLYPH_BYTES    ((CJK_GLYPH_WIDTH + 7) / 8 * CJK_GLYPH_HEIGHT)  /* 24 bytes per glyph */
#define CJK_GLYPH_ROW_BYTES 2  /* 12 bits -> 2 bytes per row */

/*
 * CJKFontData - Runtime state for a loaded CJK font
 */
typedef struct {
    ScriptCode   script;      /* Which script this font serves */
    const UInt8 *glyphData;   /* Pointer to glyph bitmap array */
    UInt32       glyphCount;  /* Number of glyphs available */
    UInt16       cellWidth;   /* Glyph cell width in pixels */
    UInt16       cellHeight;  /* Glyph cell height in pixels */
    SInt16       ascent;      /* Font ascent */
    SInt16       descent;     /* Font descent */
    SInt16       leading;     /* Inter-line leading */
    Boolean      loaded;      /* true if font data is available */
} CJKFontData;

/*
 * InitCJKFonts - Initialize the CJK font subsystem
 *
 * Called during system init. Sets up internal state but does not
 * load any font data until a CJK script is requested.
 */
void InitCJKFonts(void);

/*
 * LoadCJKFont - Load font data for a CJK script
 *
 * Loads the bitmap font resource for the specified script.
 * Returns noErr if the font is loaded (or was already loaded),
 * fontNotFoundErr if no font resource exists for this script.
 */
OSErr LoadCJKFont(ScriptCode script);

/*
 * GetCJKFont - Get the loaded font for a script
 *
 * Returns a pointer to the CJKFontData for the given script,
 * or NULL if the font is not loaded.
 */
const CJKFontData* GetCJKFont(ScriptCode script);

/*
 * GetCJKGlyph - Get glyph bitmap for a decoded character index
 *
 * Retrieves the bitmap data for a CJK character identified by
 * its decoded index (from DecodeCJKChar).
 *
 * Parameters:
 *   font      - Loaded CJK font
 *   glyphIndex - Character index from DecodeCJKChar()
 *   outBitmap  - Buffer to receive glyph bitmap (CJK_GLYPH_BYTES bytes)
 *   outWidth   - Receives glyph width
 *   outHeight  - Receives glyph height
 *
 * Returns:
 *   noErr if glyph found, paramErr if index out of range
 */
OSErr GetCJKGlyph(const CJKFontData *font, UInt32 glyphIndex,
                  UInt8 *outBitmap, SInt16 *outWidth, SInt16 *outHeight);

/*
 * CJKCharWidth - Get advance width for a CJK character
 *
 * CJK characters are typically full-width (equal to cell width).
 * ASCII characters within CJK text are half-width.
 */
SInt16 CJKCharWidth(ScriptCode script, UInt8 lead, UInt8 trail);

#endif /* CJK_FONT_H */
