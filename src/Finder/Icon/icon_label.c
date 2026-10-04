/* Shared Label Renderer
 * Reuses perfected text rendering from HD icon
 */

#include "Finder/Icon/icon_label.h"
#include "Finder/Icon/icon_types.h"
#include <string.h>
#include <stdint.h>
#include "QuickDraw/QuickDraw.h"
#include "Finder/Icon/icon_port.h"
#include "icon_logging.h"

#include "chicago_font.h"

/* Draw rectangle */
static void FillRectLocal(int left, int top, int right, int bottom, uint32_t color) {
    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            IconPort_WritePixel(x, y, color);
        }
    }
}

/* Draw character using direct bitmap rendering (perfected from HD icon) */
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

static void DrawCharBitmap(char ch, int x, int y, uint32_t color) {
    if (ch < 32 || ch > 126) return;

    ChicagoCharInfo info = chicago_ascii[ch - 32];

    for (int row = 0; row < kIconLabelGlyphHeight; row++) {
        int firstSourceRow = row * 15 / kIconLabelGlyphHeight;
        int afterLastSourceRow = (row + 1) * 15 / kIconLabelGlyphHeight;

        for (int col = 0; col < info.bit_width; col++) {
            int bit_position = info.bit_start + col;
            int byte_index = bit_position >> 3;
            int bit_offset = 7 - (bit_position & 7);

            bool set = false;
            for (int sourceRow = firstSourceRow; sourceRow < afterLastSourceRow; sourceRow++) {
                const uint8_t* strike_row = chicago_bitmap + sourceRow * 140;
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

/* Measure text using exact character bit widths */
void IconLabel_Measure(const char* name, int* outWidth, int* outHeight) {
    int width = 0;
    int len = strlen(name);

    for (int i = 0; i < len; i++) {
        char ch = name[i];
        if (ch >= 32 && ch <= 126) {
            ChicagoCharInfo info = chicago_ascii[ch - 32];
            width += info.bit_width + 1;  /* bit width + 1 for spacing */
            if (ch == ' ') width += 3;  /* Extra space width (perfected value) */
        }
    }

    if (gItalicLabel) width += kIconLabelItalicLean;

    *outWidth = width;
    *outHeight = kIconLabelGlyphHeight;
}

/*
 * The widest a label may be before it wraps.
 *
 * The folder icon grid uses an 80px cell with a 10px gutter (IW/SH in
 * folder_window.c), so a label wider than the cell runs into its neighbour -
 * "Apple Menu Items" and "PrintMonitor Documents" in the System Folder ran
 * straight through the names either side of them. System 7 wraps an icon name
 * onto a second line rather than letting it collide.
 */
#define kIconLabelMaxWidth 80
#define kIconLabelLineStep 10

/* Width of the first `len` characters, using the same metrics as
 * IconLabel_Measure. */
static int MeasureRun(const char* s, int len) {
    int width = 0;
    for (int i = 0; i < len; i++) {
        char ch = s[i];
        if (ch >= 32 && ch <= 126) {
            width += chicago_ascii[ch - 32].bit_width + 1;
            if (ch == ' ') width += 3;
        }
    }
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

    /* Adjusted background rectangle (perfected from HD icon) */
    FillRectLocal(textX - padding, topY - textHeight + 3,
                  textX + textWidth + 1, topY + 2, bgColor);  /* Reduced height by 2px */

    /* Draw text using direct bitmap rendering */
    int currentX = textX;

    for (int i = 0; i < len; i++) {
        char ch = s[i];
        if (ch >= 32 && ch <= 126) {
            const ChicagoCharInfo* infoPtr = &chicago_ascii[ch - 32];
            DrawCharBitmap(ch, currentX, topY - textHeight + 3, fgColor);
            currentX += infoPtr->bit_width + 1;
            if (ch == ' ') currentX += 3;  /* Extra space between words */
        }
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
static void IconLabel_Draw_Body(const char* name, int cx, int topY, bool selected) {
    if (!name) {
        return;
    }

    int len = strlen(name);
    if (MeasureRun(name, len) <= kIconLabelMaxWidth) {
        DrawLabelLine(name, len, cx, topY, selected);
        return;
    }

    /* Break at the last space that still fits on the first line - ordinary
     * greedy wrapping, so "Apple Menu Items" becomes "Apple Menu" / "Items"
     * rather than being balanced across the two lines. */
    int best = -1;
    for (int i = 1; i < len; i++) {
        if (name[i] != ' ') continue;
        if (MeasureRun(name, i) > kIconLabelMaxWidth) break;
        best = i;
    }

    if (best < 0) {
        /* One long word: cut it and mark the cut with an ellipsis. */
        char cut[64];
        int n = 0;
        while (n < len && n < (int)sizeof(cut) - 4 &&
               MeasureRun(name, n + 1) <= kIconLabelMaxWidth - 12) {
            cut[n] = name[n];
            n++;
        }
        cut[n++] = '.'; cut[n++] = '.'; cut[n++] = '.';
        DrawLabelLine(cut, n, cx, topY, selected);
        return;
    }

    /* Second line may still be too long for one line; cut it the same way. */
    const char* second = name + best + 1;
    int secondLen = len - best - 1;
    if (MeasureRun(second, secondLen) > kIconLabelMaxWidth) {
        char cut[64];
        int n = 0;
        while (n < secondLen && n < (int)sizeof(cut) - 4 &&
               MeasureRun(second, n + 1) <= kIconLabelMaxWidth - 12) {
            cut[n] = second[n];
            n++;
        }
        cut[n++] = '.'; cut[n++] = '.'; cut[n++] = '.';
        DrawLabelLine(name, best, cx, topY, selected);
        DrawLabelLine(cut, n, cx, topY + kIconLabelLineStep, selected);
        return;
    }

    DrawLabelLine(name, best, cx, topY, selected);
    DrawLabelLine(second, secondLen, cx, topY + kIconLabelLineStep, selected);
}

void IconLabel_Draw(const char* name, int cx, int topY, bool selected) {
    QD_ClipBegin(g_currentPort);
    IconLabel_Draw_Body(name, cx, topY, selected);
    QD_ClipEnd();
}

/* Draw icon with label - main entry point for icon+label rendering */
IconRect Icon_DrawWithLabel(const IconHandle* h, const char* name,
                            int centerX, int iconTopY, bool selected) {
    FINDER_ICON_LOG_DEBUG("Icon_DrawWithLabel: centerX=%d iconTopY=%d name='%s'\n", centerX, iconTopY, name ? name : "NULL");

    /* Draw icon centered at centerX */
    int iconLeft = centerX - 16;  /* 32x32 icon */
    FINDER_ICON_LOG_DEBUG("Icon_DrawWithLabel: calling Icon_Draw32 at X=%d Y=%d selected=%d\n", iconLeft, iconTopY, selected);
    Icon_Draw32(h, iconLeft, iconTopY, selected);

    int labelTop = iconTopY + 34;
    IconLabel_SetItalic(h && h->italicLabel);
    IconLabel_Draw(name, centerX, labelTop, selected);
    IconLabel_SetItalic(false);

    /* Return combined bounds for hit testing */
    int textWidth, textHeight;
    IconLabel_Measure(name, &textWidth, &textHeight);

    IconRect bounds;
    bounds.left = iconLeft;
    bounds.top = iconTopY;
    bounds.right = iconLeft + 32;
    bounds.bottom = labelTop + 5;  /* Include label area with adjusted position */

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
    bounds.bottom = labelTop + 5;  /* Include label area with adjusted position */

    /* Expand to include label width */
    int labelLeft = centerX - (textWidth / 2) - 2;
    int labelRight = centerX + (textWidth / 2) + 2;
    if (labelLeft < bounds.left) bounds.left = labelLeft;
    if (labelRight > bounds.right) bounds.right = labelRight;

    return bounds;
}
