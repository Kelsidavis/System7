/* #include "SystemTypes.h" */
#include <string.h>
/**
 * @file ControlResources.c
 * @brief CNTL resource loading and template processing
 *
 * This file implements functionality for loading and managing control
 * resources, including CNTL resources and control templates.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
/* ControlResources.h local */
#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlInternal.h"
#include "MemoryMgr/MemoryManager.h"
#include "ResourceMgr/resource_types.h"


/* Internal CNTL template representation */
typedef struct CNTLTemplate {
    Rect    boundsRect;
    SInt16  value;
    Boolean visible;
    SInt16  max;
    SInt16  min;
    SInt16  procID;
    SInt32  refCon;
    Str255  title;
} CNTLTemplate;

/* --- Local helpers ------------------------------------------------------ */

static OSErr ParseCNTLResource(Handle resource, CNTLTemplate *cntlData);

static inline SInt16 read_s16_be(const UInt8 **data) {
    SInt16 value = (SInt16)((((*data)[0]) << 8) | (*data)[1]);
    *data += 2;
    return value;
}

static inline SInt32 read_s32_be(const UInt8 **data) {
    SInt32 value =  ((SInt32)(*data)[0] << 24) |
                    ((SInt32)(*data)[1] << 16) |
                    ((SInt32)(*data)[2] << 8)  |
                    (SInt32)(*data)[3];
    *data += 4;
    return value;
}

/**
 * Load a control from a CNTL resource
 */
ControlHandle LoadControlFromResource(Handle cntlResource, WindowPtr owner) {
    CNTLTemplate cntlData;
    ControlHandle control;
    OSErr err;

    if (!cntlResource || !owner) {
        return NULL;
    }

    /* Parse CNTL resource */
    err = ParseCNTLResource(cntlResource, &cntlData);
    if (err != noErr) {
        return NULL;
    }

    /* Create control from resource data */
    control = NewControl(owner, &cntlData.boundsRect, cntlData.title,
                         cntlData.visible, cntlData.value,
                         cntlData.min, cntlData.max, cntlData.procID,
                         cntlData.refCon);

    return control;
}

/**
 * Parse a CNTL resource
 */
static OSErr ParseCNTLResource(Handle resource, CNTLTemplate *cntlData) {
    const UInt8 *cursor;
    SInt16 titleLen;

    if (!resource || !cntlData) {
        return paramErr;
    }

    HLock(resource);
    cursor = (const UInt8 *)*resource;

    /* Read rectangle bounds */
    cntlData->boundsRect.top = read_s16_be(&cursor);
    cntlData->boundsRect.left = read_s16_be(&cursor);
    cntlData->boundsRect.bottom = read_s16_be(&cursor);
    cntlData->boundsRect.right = read_s16_be(&cursor);

    /* Read control properties */
    cntlData->value = read_s16_be(&cursor);
    cntlData->visible = read_s16_be(&cursor) != 0;
    cntlData->max = read_s16_be(&cursor);
    cntlData->min = read_s16_be(&cursor);
    cntlData->procID = read_s16_be(&cursor);
    cntlData->refCon = read_s32_be(&cursor);

    /* Read title (Pascal string) */
    titleLen = *cursor;
    if (titleLen > 255) {
        titleLen = 255;
    }
    memcpy(cntlData->title, cursor, (size_t)titleLen + 1);

    HUnlock(resource);
    return noErr;
}
