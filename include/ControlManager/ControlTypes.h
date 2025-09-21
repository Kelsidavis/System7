/**
 * @file ControlTypes.h
 * @brief Control Manager type definitions and constants
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#ifndef CONTROLTYPES_H
#define CONTROLTYPES_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

/* Control Messages */
enum {
    drawCntl    = 0,
    testCntl    = 1,
    calcCRgns   = 2,
    initCntl    = 3,
    dispCntl    = 4,
    posCntl     = 5,
    thumbCntl   = 6,
    dragCntl    = 7,
    autoTrack   = 8,
    calcCntlRgn = 10,
    calcThumbRgn = 11,
    drawThumbOutline = 12
};

/* Control Change Notification Codes */
enum {
    kControlPositionChanged = 1,
    kControlSizeChanged = 2,
    kControlValueChanged = 3,
    kControlRangeChanged = 4,
    kControlTitleChanged = 5,
    kControlVisibilityChanged = 6,
    kControlHighlightChanged = 7
};

/* Control Highlight States */
enum {
    noHilite = 0,
    inactiveHilite = 255
};

/* Control Part Codes */
enum {
    kControlNoPart = 0,
    inButton = 10,
    inCheckBox = 11,
    inUpButton = 20,
    inDownButton = 21,
    inPageUp = 22,
    inPageDown = 23,
    inThumb = 129,
    kControlButtonPart = 10,
    kControlCheckBoxPart = 11,
    kControlRadioButtonPart = 12,
    kControlUpButtonPart = 20,
    kControlDownButtonPart = 21,
    kControlPageUpPart = 22,
    kControlPageDownPart = 23,
    kControlIndicatorPart = 129
};

/* Drag constraints */
enum {
    noConstraint = 0,
    hAxisOnly = 1,
    vAxisOnly = 2
};

/* Control Color Types */

/* Forward declarations for headers */

#endif /* CONTROLTYPES_H *//* Control color types */

