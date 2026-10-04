/*
 * IconUtilities.h - Icon Utilities Toolbox interface
 */

#ifndef TOOLBOX_ICON_UTILITIES_H
#define TOOLBOX_ICON_UTILITIES_H

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    kAlignNone = 0x00,
    kAlignVerticalCenter = 0x01,
    kAlignTop = 0x02,
    kAlignBottom = 0x03,
    kAlignHorizontalCenter = 0x04,
    kAlignAbsoluteCenter = kAlignVerticalCenter | kAlignHorizontalCenter,
    kAlignLeft = 0x08,
    kAlignRight = 0x0C
} IconAlignmentType;

typedef enum {
    kTransformNone = 0x00,
    kTransformDisabled = 0x01,
    kTransformOffline = 0x02,
    kTransformOpen = 0x03,
    kTransformLabel1 = 0x0100,
    kTransformLabel2 = 0x0200,
    kTransformLabel3 = 0x0300,
    kTransformLabel4 = 0x0400,
    kTransformLabel5 = 0x0500,
    kTransformLabel6 = 0x0600,
    kTransformLabel7 = 0x0700,
    kTransformSelected = 0x4000,
    kTransformSelectedDisabled = kTransformSelected | kTransformDisabled
} IconTransformType;

typedef unsigned short IconSelectorValue;

void PlotIcon(const Rect* theRect, Handle theIcon);
void PlotIconID(const Rect* theRect, IconAlignmentType align, IconTransformType transform, short theResID);
Handle GetIcon(short iconID);
void PlotIconHandle(const Rect* theRect, IconAlignmentType align, IconTransformType transform, Handle theIcon);
OSErr GetIconSuite(Handle* theIconSuite, short theResID, IconSelectorValue selector);
OSErr PlotIconSuite(const Rect* theRect, IconAlignmentType align, IconTransformType transform, Handle theIconSuite);
OSErr DisposeIconSuite(Handle theIconSuite, Boolean disposeData);

#ifdef __cplusplus
}
#endif

#endif
