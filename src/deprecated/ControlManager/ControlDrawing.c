/* #include "SystemTypes.h" */
/**
 * @file ControlDrawing.c
 * @brief Control rendering and visual management
 *
 * This file implements common drawing utilities and visual management
 * for controls, providing consistent appearance and theming support.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "ControlManager/ControlManager.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontManager.h"


/**
 * Draw text in rectangle with alignment
 */
void DrawTextInRect(ConstStr255Param text, const Rect *rect, SInt16 alignment) {
    FontInfo fontInfo;
    SInt16 textWidth, textHeight;
    SInt16 h, v;

    if (!text || text[0] == 0 || !rect) {
        return;
    }

    /* Get font metrics */
    GetFontInfo(&fontInfo);
    textWidth = StringWidth(text);
    textHeight = fontInfo.ascent + fontInfo.descent;

    /* Calculate horizontal position */
    switch (alignment) {
    case teCenter:
        h = rect->left + (rect->right - rect->left - textWidth) / 2;
        break;
    case teFlushRight:
        h = rect->right - textWidth;
        break;
    default: /* teFlushLeft */
        h = rect->left;
        break;
    }

    /* Calculate vertical position (centered) */
    v = rect->top + (rect->bottom - rect->top + fontInfo.ascent) / 2;

    /* Draw text */
    MoveTo(h, v);
    DrawString(text);
}

/**
 * Draw 3D button frame
 */
void DrawButtonFrame(ControlHandle button, Boolean pushed) {
    Rect frameRect;

    if (!button) {
        return;
    }

    frameRect = (*button)->contrlRect;

    if (pushed) {
        /* Pushed state - inset frame */
        PenPat(&black);
        MoveTo(frameRect.left, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.top);
        LineTo(frameRect.right - 1, frameRect.top);

        PenPat(&dkGray);
        LineTo(frameRect.right - 1, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.bottom - 1);
    } else {
        /* Normal state - raised frame */
        PenPat(&white);
        MoveTo(frameRect.left, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.top);
        LineTo(frameRect.right - 1, frameRect.top);

        PenPat(&black);
        LineTo(frameRect.right - 1, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.bottom - 1);
    }

    /* Fill interior */
    InsetRect(&frameRect, 1, 1);
    PenPat(&ltGray);
    PaintRect(&frameRect);
    PenPat(&black);
}

/**
 * Draw control focus ring
 */
void DrawControlFocusRing(ControlHandle control) {
    Rect focusRect;

    if (!control) {
        return;
    }

    focusRect = (*control)->contrlRect;
    InsetRect(&focusRect, -3, -3);

    /* Draw dotted focus ring */
    PenPat(&gray);
    PenMode(patXor);
    FrameRect(&focusRect);
    PenMode(patCopy);
    PenPat(&black);
}

/**
 * Get standard control colors
 */
void GetControlColors(ControlColorType colorType, RGBColor *color) {
    if (!color) {
        return;
    }

    switch (colorType) {
    case kControlFrameColor:
        color->red = 0x0000;
        color->green = 0x0000;
        color->blue = 0x0000;
        break;
    case kControlBodyColor:
        color->red = 0xC000;
        color->green = 0xC000;
        color->blue = 0xC000;
        break;
    case kControlTextColor:
        color->red = 0x0000;
        color->green = 0x0000;
        color->blue = 0x0000;
        break;
    case kControlHighlightColor:
        color->red = 0x0000;
        color->green = 0x0000;
        color->blue = 0x8000;
        break;
    default:
        color->red = 0x8000;
        color->green = 0x8000;
        color->blue = 0x8000;
        break;
    }
}
