/* Shared Finder icon-label layout and bitmap rendering. */

#include "Finder/Icon/icon_label.h"
#include "Finder/Icon/icon_types.h"
#include <string.h>
#include <stdint.h>
#include "QuickDraw/QuickDraw.h"
#include "Finder/Icon/icon_port.h"
#include "icon_logging.h"

#include "chicago_font.h"
#include "chicago_font_extended.h"

/* Draw rectangle */
static void FillRectLocal(int left, int top, int right, int bottom, uint32_t color) {
    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            IconPort_WritePixel(x, y, color);
        }
    }
}

/*
 * Italic labels.
 *
 * The Finder draws an alias's name in italics, and this label renderer draws
 * Chicago from a bitmap strike rather than through QuickDraw, so TextFace has
 * nothing to act on. Italic is a shear: each row is pushed right in
 * proportion to its height above the baseline, which is what QuickDraw does
 * to synthesise an italic from an upright face.
 *
 * gItalicLabel is set for the duration of one label. Drawing and measuring
 * both read it, so the background and centring account for the lean instead
 * of being computed for an upright name and drawn slanted.
 */
static bool gItalicLabel = false;

#define kIconLabelItalicLean 3   /* pixels of lean over the glyph's height */
#define kIconLabelGlyphHeight 9

static int IconLabel_Shear(int row) {
    if (!gItalicLabel) return 0;
    return ((14 - row) * kIconLabelItalicLean) / 14;
}

void IconLabel_SetItalic(bool slanted) {
    gItalicLabel = slanted;
}

static void DrawReducedGlyph(const ChicagoCharInfo* info, const uint8_t* strike,
                             int rowBytes, int x, int y, uint32_t color) {
    if (!info || !strike) return;
    for (int row = 0; row < kIconLabelGlyphHeight; row++) {
        int firstSourceRow = row * 15 / kIconLabelGlyphHeight;
        int afterLastSourceRow = (row + 1) * 15 / kIconLabelGlyphHeight;

        for (int col = 0; col < info->bit_width; col++) {
            int bit_position = info->bit_start + col;
            int byte_index = bit_position >> 3;
            int bit_offset = 7 - (bit_position & 7);

            bool set = false;
            for (int sourceRow = firstSourceRow; sourceRow < afterLastSourceRow; sourceRow++) {
                const uint8_t* strike_row = strike + sourceRow * rowBytes;
                if (strike_row[byte_index] & (1 << bit_offset)) {
                    set = true;
                    break;
                }
            }
            if (set) {
                int lean = (IconLabel_Shear(firstSourceRow) * kIconLabelGlyphHeight + 7) / 15;
                IconPort_WritePixel(x + col + lean, y + row, color);
            }
        }
    }
}

static int IconLabel_CharWidth(unsigned char ch) {
    unsigned char symbol = Chicago_DrawnSymbol(ch);
    if (symbol != kNoAccent) {
        return chicago_accents[symbol].bit_width + 1;
    }

    ChicagoComposition composition = Chicago_Compose(ch);
    const ChicagoCharInfo* info;
    if (composition.base != 0) {
        info = &chicago_ascii[composition.base - 32];
    } else {
        info = Chicago_Glyph(ch, NULL, NULL);
    }
    if (!info) return 0;

    int width = info->bit_width + 1;
    if (ch == ' ') width += 3;
    return width;
}

static void DrawLabelChar(unsigned char ch, int x, int y, uint32_t color) {
    unsigned char symbol = Chicago_DrawnSymbol(ch);
    if (symbol != kNoAccent) {
        DrawReducedGlyph(&chicago_accents[symbol], chicago_accent_bitmap,
                         CHICAGO_ACCENT_ROW_BYTES, x, y, color);
        return;
    }

    ChicagoComposition composition = Chicago_Compose(ch);
    if (composition.base != 0) {
        const ChicagoCharInfo* base = &chicago_ascii[composition.base - 32];
        DrawReducedGlyph(base, chicago_bitmap, CHICAGO_ROW_BYTES, x, y, color);
        if (composition.accent != kNoAccent) {
            const ChicagoCharInfo* mark = &chicago_accents[composition.accent];
            int markX = x + (base->bit_width - mark->bit_width) / 2;
            DrawReducedGlyph(mark, chicago_accent_bitmap, CHICAGO_ACCENT_ROW_BYTES,
                             markX, y, color);
        }
        return;
    }

    const uint8_t* strike = NULL;
    int rowBytes = 0;
    const ChicagoCharInfo* info = Chicago_Glyph(ch, &strike, &rowBytes);
    if (info) {
        DrawReducedGlyph(info, strike, rowBytes, x, y, color);
    }
}

#define kIconLabelLineStep 10

/* Width of the first `len` characters, using the same metrics as
 * IconLabel_Measure. */
static int MeasureRun(const char* s, int len) {
    int width = 0;
    for (int i = 0; i < len; i++) {
        width += IconLabel_CharWidth((unsigned char)s[i]);
    }
    if (gItalicLabel) width += kIconLabelItalicLean;
    return width;
}

/* Draw one line of label text, centred on cx and clamped inside the port. */
static void DrawLabelLine(const char* s, int len, int cx, int topY, bool selected) {
    int textWidth = MeasureRun(s, len);
    int textHeight = kIconLabelGlyphHeight;
    int textX = cx - (textWidth / 2);
    int padding = 2;

    /* Keep the label inside the port.
     *
     * Centring on the icon alone lets a wide name start at a negative x. In a
     * folder window the first column's icon centre sits at about x=36 while a
     * name like "System Folder" is nearly 90px wide, so the label began off the
     * left edge and rendered as "ystem Folder" with the leading character cut
     * off by the window frame. The same applies at the right edge and to
     * desktop icons near a screen border.
     *
     * Nudging the label back inside is the lesser evil: slightly off-centre
     * beats truncated and unreadable. */
    {
        GrafPtr port = qd.thePort;
        if (port) {
            int minX = port->portRect.left + padding;
            int maxX = port->portRect.right - padding;
            if (textX + textWidth > maxX) {
                textX = maxX - textWidth;
            }
            if (textX < minX) {
                textX = minX;  /* label wider than the port: favour the start */
            }
        }
    }

    /* Draw background rectangle behind text */
    uint32_t bgColor = selected ? 0xFF000000 : 0xFFFFFFFF;  /* Black if selected, white otherwise */
    uint32_t fgColor = selected ? 0xFFFFFFFF : 0xFF000000;  /* White text if selected, black otherwise */

    /* Background for this line's glyphs. */
    FillRectLocal(textX - padding, topY - textHeight + 3,
                  textX + textWidth + 1, topY + 2, bgColor);  /* Reduced height by 2px */

    /* Draw text using direct bitmap rendering */
    int currentX = textX;

    for (int i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        DrawLabelChar(ch, currentX, topY - textHeight + 3, fgColor);
        currentX += IconLabel_CharWidth(ch);
    }
}

/*
 * Draw an icon label, wrapping onto a second line when it is too wide.
 *
 * System 7 breaks a long icon name at a space and centres both halves under the
 * icon. If there is no space to break at - or a half is still too wide - the
 * line is cut and given a trailing ellipsis, which is what the Finder does for
 * a single long word.
 */
typedef struct IconLabelLayout {
    char lines[2][256];
    int lengths[2];
    int count;
    int width;
} IconLabelLayout;

static void LayoutLabel(const char* name, int maxWidth, IconLabelLayout* layout) {
    memset(layout, 0, sizeof(*layout));
    if (!name || maxWidth <= 0) return;
    int len = (int)strlen(name);
    int split = -1;
    if (MeasureRun(name, len) > maxWidth) {
        for (int i = 1; i < len; i++) {
            if (MeasureRun(name, i) > maxWidth) break;
            if (name[i] == ' ') split = i;
        }
    }
    layout->count = split < 0 ? 1 : 2;
    for (int line = 0; line < layout->count; line++) {
        const char* text = line == 0 ? name : name + split + 1;
        int length = split >= 0 && line == 0 ? split : (int)strlen(text);
        bool truncated = MeasureRun(text, length) > maxWidth;
        int dots = truncated ? 3 : 0;
        while (dots && MeasureRun("...", dots) > maxWidth) dots--;
        int limit = maxWidth - (dots ? MeasureRun("...", dots) : 0);
        int n = 0;
        while (n < length && n < (int)sizeof(layout->lines[line]) - 4 &&
               MeasureRun(text, n + 1) <= limit) {
            layout->lines[line][n] = text[n];
            n++;
        }
        if (dots) {
            memcpy(layout->lines[line] + n, "...", dots);
            n += dots;
        }
        layout->lengths[line] = n;
        int width = MeasureRun(layout->lines[line], n);
        if (width > layout->width) layout->width = width;
    }
}

void IconLabel_Measure(const char* name, int* outWidth, int* outHeight) {
    IconLabel_MeasureWithWidth(name, kIconLabelMaxWidth, outWidth, outHeight);
}

void IconLabel_MeasureWithWidth(const char* name, int maxWidth, int* outWidth, int* outHeight) {
    if (!name || !outWidth || !outHeight) return;
    IconLabelLayout layout;
    LayoutLabel(name, maxWidth, &layout);
    *outWidth = layout.width;
    *outHeight = layout.count ? kIconLabelGlyphHeight + (layout.count - 1) * kIconLabelLineStep : 0;
}

static void IconLabel_Draw_Body(const char* name, int cx, int topY, bool selected, int maxWidth) {
    IconLabelLayout layout;
    LayoutLabel(name, maxWidth, &layout);
    for (int line = 0; line < layout.count; line++) {
        DrawLabelLine(layout.lines[line], layout.lengths[line], cx,
                      topY + line * kIconLabelLineStep, selected);
    }
}

void IconLabel_Draw(const char* name, int cx, int topY, bool selected) {
    QD_ClipBegin(g_currentPort);
    IconLabel_Draw_Body(name, cx, topY, selected, kIconLabelMaxWidth);
    QD_ClipEnd();
}

/* Draw icon with label - main entry point for icon+label rendering */
IconRect Icon_DrawWithLabel(const IconHandle* h, const char* name,
                            int centerX, int iconTopY, bool selected) {
    return Icon_DrawWithLabelWidth(h, name, centerX, iconTopY, selected, kIconLabelMaxWidth);
}

IconRect Icon_DrawWithLabelWidth(const IconHandle* h, const char* name,
                                 int centerX, int iconTopY, bool selected, int maxWidth) {
    FINDER_ICON_LOG_DEBUG("Icon_DrawWithLabel: centerX=%d iconTopY=%d name='%s'\n", centerX, iconTopY, name ? name : "NULL");

    /* Draw icon centered at centerX */
    int iconLeft = centerX - 16;  /* 32x32 icon */
    FINDER_ICON_LOG_DEBUG("Icon_DrawWithLabel: calling Icon_Draw32 at X=%d Y=%d selected=%d\n", iconLeft, iconTopY, selected);
    Icon_Draw32(h, iconLeft, iconTopY, selected);

    int labelTop = iconTopY + 34;
    IconLabel_SetItalic(h && h->italicLabel);
    QD_ClipBegin(g_currentPort);
    IconLabel_Draw_Body(name, centerX, labelTop, selected, maxWidth);
    QD_ClipEnd();

    /* Return combined bounds for hit testing */
    int textWidth, textHeight;
    IconLabel_MeasureWithWidth(name, maxWidth, &textWidth, &textHeight);
    IconLabel_SetItalic(false);

    IconRect bounds;
    bounds.left = iconLeft;
    bounds.top = iconTopY;
    bounds.right = iconLeft + 32;
    bounds.bottom = labelTop + 3 + textHeight - kIconLabelGlyphHeight;

    /* Expand to include label width */
    int labelLeft = centerX - (textWidth / 2) - 2;
    int labelRight = centerX + (textWidth / 2) + 2;
    if (labelLeft < bounds.left) bounds.left = labelLeft;
    if (labelRight > bounds.right) bounds.right = labelRight;

    return bounds;
}

/* Draw icon with label at custom offset - for special icons like Trash */
IconRect Icon_DrawWithLabelOffset(const IconHandle* h, const char* name,
                                  int centerX, int iconTopY, int labelOffset, bool selected) {
    /* Draw icon centered at centerX */
    int iconLeft = centerX - 16;  /* 32x32 icon */
    Icon_Draw32(h, iconLeft, iconTopY, selected);

    /* Draw label with custom offset */
    int labelTop = iconTopY + labelOffset;
    IconLabel_Draw(name, centerX, labelTop, selected);

    /* Return combined bounds for hit testing */
    int textWidth, textHeight;
    IconLabel_Measure(name, &textWidth, &textHeight);

    IconRect bounds;
    bounds.left = iconLeft;
    bounds.top = iconTopY;
    bounds.right = iconLeft + 32;
    bounds.bottom = labelTop + 3 + textHeight - kIconLabelGlyphHeight;

    /* Expand to include label width */
    int labelLeft = centerX - (textWidth / 2) - 2;
    int labelRight = centerX + (textWidth / 2) + 2;
    if (labelLeft < bounds.left) bounds.left = labelLeft;
    if (labelRight > bounds.right) bounds.right = labelRight;

    return bounds;
}
