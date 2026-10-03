/*
 * ResourceData.h - System 7 Resource Data Management
 *
 * Provides access to embedded System 7 resources including icons, cursors,
 * patterns, and sounds extracted from original System 7 resource files.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#ifndef RESOURCE_DATA_H
#define RESOURCE_DATA_H

#include "SystemTypes.h"

/* Forward declarations */

#include "QuickDraw/QuickDraw.h"

/* Include the extracted System 7 resources */
#include "system7_resources.h"

/* Resource types */

/* Resource IDs for standard System 7 resources */

/* Resource data structure */

/* Public API */

/* Initialize resource data system */
OSErr InitResourceData(void);

/* Check if resource data system has been initialized */
Boolean GetResourceDataInitialized(void);


/* Draw icon at location */
void DrawResourceIcon(UInt16 iconID, short x, short y);


#endif /* RESOURCE_DATA_H */
