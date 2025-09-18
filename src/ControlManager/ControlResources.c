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

#include "ControlResources.h"
#include "ControlManager.h"
#include "../ResourceManager/ResourceManager.h"
#include "../MacTypes.h"
#include <string.h>
#include <stdlib.h>

/* CNTL resource structure */
typedef struct CNTLResource {
    Rect    boundsRect;
    int16_t value;
    int16_t visible;
    int16_t max;
    int16_t min;
    int16_t procID;
    int32_t refCon;
    Str255  title;
} CNTLResource;

/**
 * Load a control from a CNTL resource
 */
ControlHandle LoadControlFromResource(Handle cntlResource, WindowPtr owner) {
    CNTLResource cntlData;
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
                         cntlData.visible != 0, cntlData.value,
                         cntlData.min, cntlData.max, cntlData.procID,
                         cntlData.refCon);

    return control;
}

/**
 * Parse a CNTL resource
 */
OSErr ParseCNTLResource(Handle resource, CNTLResource *cntlData) {
    uint8_t *data;
    int16_t titleLen;

    if (!resource || !cntlData) {
        return paramErr;
    }

    HLock(resource);
    data = (uint8_t *)*resource;

    /* Read rectangle bounds */
    cntlData->boundsRect.top = *(int16_t *)data; data += 2;
    cntlData->boundsRect.left = *(int16_t *)data; data += 2;
    cntlData->boundsRect.bottom = *(int16_t *)data; data += 2;
    cntlData->boundsRect.right = *(int16_t *)data; data += 2;

    /* Read control properties */
    cntlData->value = *(int16_t *)data; data += 2;
    cntlData->visible = *(int16_t *)data; data += 2;
    cntlData->max = *(int16_t *)data; data += 2;
    cntlData->min = *(int16_t *)data; data += 2;
    cntlData->procID = *(int16_t *)data; data += 2;
    cntlData->refCon = *(int32_t *)data; data += 4;

    /* Read title (Pascal string) */
    titleLen = *data;
    if (titleLen > 255) titleLen = 255;
    memcpy(cntlData->title, data, titleLen + 1);

    HUnlock(resource);
    return noErr;
}

/**
 * Create a CNTL resource from control data
 */
Handle CreateCNTLResource(const CNTLResource *cntlData) {
    Handle resource;
    uint8_t *data;
    int32_t dataSize;
    int16_t titleLen;

    if (!cntlData) {
        return NULL;
    }

    /* Calculate resource size */
    titleLen = cntlData->title[0];
    dataSize = 8 + 10 + 4 + titleLen + 1; /* rect + 5 shorts + long + string */

    /* Allocate resource */
    resource = NewHandle(dataSize);
    if (!resource) {
        return NULL;
    }

    HLock(resource);
    data = (uint8_t *)*resource;

    /* Write bounds rectangle */
    *(int16_t *)data = cntlData->boundsRect.top; data += 2;
    *(int16_t *)data = cntlData->boundsRect.left; data += 2;
    *(int16_t *)data = cntlData->boundsRect.bottom; data += 2;
    *(int16_t *)data = cntlData->boundsRect.right; data += 2;

    /* Write control properties */
    *(int16_t *)data = cntlData->value; data += 2;
    *(int16_t *)data = cntlData->visible; data += 2;
    *(int16_t *)data = cntlData->max; data += 2;
    *(int16_t *)data = cntlData->min; data += 2;
    *(int16_t *)data = cntlData->procID; data += 2;
    *(int32_t *)data = cntlData->refCon; data += 4;

    /* Write title */
    memcpy(data, cntlData->title, titleLen + 1);

    HUnlock(resource);
    return resource;
}