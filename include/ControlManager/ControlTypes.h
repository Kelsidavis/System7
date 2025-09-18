/**
 * @file ControlTypes.h
 * @brief Control Manager type definitions and constants
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#ifndef CONTROLTYPES_H
#define CONTROLTYPES_H

#include "../MacTypes.h"

/* Control Color Types */
typedef enum {
    kControlFrameColor = 0,
    kControlBodyColor = 1,
    kControlTextColor = 2,
    kControlHighlightColor = 3
} ControlColorType;

/* Forward declarations for headers */
typedef struct CNTLResource CNTLResource;
typedef struct ControlTemplate ControlTemplate;

#endif /* CONTROLTYPES_H */