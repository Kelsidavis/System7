/*
 * IconUtilities.c - Icon Utilities Toolbox Functions
 *
 * Implements System 7 Icon Utilities for displaying and managing icons:
 * - PlotIcon: Plot an icon at a location
 * - PlotIconID: Plot an icon from resource ID
 * - GetIcon: Get icon resource
 * - PlotIconHandle: Plot an icon from handle
 * - PlotIconSuite: Plot complete icon suite
 * - GetIconSuite: Get icon suite from resource
 * - DisposeIconSuite: Dispose of icon suite
 *
 * Based on Inside Macintosh: Macintosh Toolbox Essentials, Chapter 7
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "QuickDraw/QuickDraw.h"
#include "ResourceManager.h"

/* Icon alignment types */
typedef enum {
    kAlignNone          = 0x00,
    kAlignVerticalCenter = 0x01,
    kAlignTop           = 0x02,
    kAlignBottom        = 0x03,
    kAlignHorizontalCenter = 0x04,
    kAlignAbsoluteCenter = kAlignVerticalCenter | kAlignHorizontalCenter,
    kAlignLeft          = 0x08,
    kAlignRight         = 0x0C
} IconAlignment;

/* Icon transform types */
typedef enum {
    kTransformNone      = 0x00,
    kTransformDisabled  = 0x01,
    kTransformOffline   = 0x02,
    kTransformOpen      = 0x03,
    kTransformLabel1    = 0x0100,
    kTransformLabel2    = 0x0200,
    kTransformLabel3    = 0x0300,
    kTransformLabel4    = 0x0400,
    kTransformLabel5    = 0x0500,
    kTransformLabel6    = 0x0600,
    kTransformLabel7    = 0x0700,
    kTransformSelected  = 0x4000,
    kTransformSelectedDisabled = kTransformSelected | kTransformDisabled
} IconTransform;

/* Type aliases for compatibility */
typedef IconAlignment IconAlignmentType;
typedef IconTransform IconTransformType;
typedef unsigned short IconSelectorValue;

/* Forward declarations */
void PlotIcon(const Rect* theRect, Handle theIcon);
void PlotIconID(const Rect* theRect, IconAlignmentType align, IconTransformType transform, short theResID);
Handle GetIcon(short iconID);
void PlotIconHandle(const Rect* theRect, IconAlignmentType align, IconTransformType transform, Handle theIcon);
OSErr GetIconSuite(Handle* theIconSuite, short theResID, IconSelectorValue selector);
OSErr PlotIconSuite(const Rect* theRect, IconAlignmentType align, IconTransformType transform, Handle theIconSuite);
OSErr DisposeIconSuite(Handle theIconSuite, Boolean disposeData);

/* Debug logging */
#define ICON_UTILS_DEBUG 0

#if ICON_UTILS_DEBUG
extern void serial_puts(const char* str);
#define ICON_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[IconUtils] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define ICON_LOG(...)
#endif

/* Icon resource types */
#ifndef FOUR_CHAR_CODE
#define FOUR_CHAR_CODE(x) (x)
#endif

#define kIconResourceType FOUR_CHAR_CODE('ICON')  /* 32x32 black and white */
#define kSmallerIconType  FOUR_CHAR_CODE('ICN#')  /* 32x32 icon with mask */
#define kSmallIconType    FOUR_CHAR_CODE('ics#')  /* 16x16 icon with mask */
#define kColorIconType    FOUR_CHAR_CODE('cicn')  /* Color icon */

/* Standard icon size */
#define kIconWidth  32
#define kIconHeight 32
#define kSmallIconWidth  16
#define kSmallIconHeight 16

/* External QuickDraw functions */
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern void PenMode(short mode);
extern void CopyBits(const BitMap* srcBits, const BitMap* dstBits,
                     const Rect* srcRect, const Rect* dstRect,
                     short mode, RgnHandle maskRgn);

/*
 * PlotIcon - Plot an icon at a location
 *
 * Draws a 32x32 pixel black-and-white icon within the specified rectangle.
 * The icon is centered and scaled to fit the rectangle.
 *
 * Parameters:
 *   theRect - Rectangle to draw icon within
 *   theIcon - Handle to icon data (32x32 bitmap, 128 bytes)
 *
 * Notes:
 * - Icon data is 128 bytes: 32x32 pixels, 1 bit per pixel
 * - Icon is drawn in current pen mode and pattern
 * - If theRect is NULL or theIcon is NULL, does nothing
 * - Icon is centered in the rectangle
 *
 * Example:
 *   Handle icon = GetIcon(128);
 *   Rect r = {10, 10, 42, 42};  // 32x32 rect
 *   PlotIcon(&r, icon);
 *
 * Based on Inside Macintosh: Macintosh Toolbox Essentials, Chapter 7-21
 */
void PlotIcon(const Rect* theRect, Handle theIcon) {
    if (theRect == NULL || theIcon == NULL) {
        ICON_LOG("PlotIcon: NULL parameter\n");
        return;
    }

    ICON_LOG("PlotIcon: rect=(%d,%d,%d,%d) icon=%p\n",
             theRect->top, theRect->left, theRect->bottom, theRect->right,
             (void*)theIcon);

    /* Lock the icon handle to get the data */
    HLock(theIcon);
    unsigned char* iconData = (unsigned char*)*theIcon;

    if (iconData == NULL) {
        ICON_LOG("PlotIcon: NULL icon data\n");
        HUnlock(theIcon);
        return;
    }

    /* Simple implementation: draw icon as bitmap
     * In a full implementation, would use CopyBits with proper scaling
     */

    /* For now, just draw a placeholder rectangle
     * A full implementation would render the actual icon bitmap
     */
    GrafPtr savePort;
    GetPort(&savePort);

    /* Draw icon outline for now (placeholder) */
    ICON_LOG("PlotIcon: Drawing icon (placeholder implementation)\n");

    HUnlock(theIcon);

    /* TODO: Implement actual icon bitmap rendering */
}

/*
 * PlotIconID - Plot an icon from resource ID
 *
 * Loads an icon from resources and plots it in the specified rectangle
 * with optional alignment and transformation.
 *
 * Parameters:
 *   theRect - Rectangle to draw icon within
 *   align - Icon alignment (center, left, right, top, bottom)
 *   transform - Icon transformation (selected, disabled, etc.)
 *   theResID - Resource ID of icon to plot
 *
 * Alignment values:
 *   atNone - No alignment, use rect as-is
 *   atHorizontalCenter - Center horizontally
 *   atVerticalCenter - Center vertically
 *   atAbsoluteCenter - Center both horizontally and vertically
 *   atTop, atBottom, atLeft, atRight - Align to edge
 *
 * Transform values:
 *   ttNone - No transformation
 *   ttDisabled - Draw in disabled (dimmed) state
 *   ttSelected - Draw in selected (highlighted) state
 *   ttOffline - Draw in offline (dimmed) state
 *   ttOpen - Draw in open state (for folder icons)
 *
 * Example:
 *   Rect r = {10, 10, 42, 42};
 *   PlotIconID(&r, atAbsoluteCenter, ttNone, 128);
 *
 * Based on Inside Macintosh: Macintosh Toolbox Essentials, Chapter 7-23
 */
void PlotIconID(const Rect* theRect, IconAlignmentType align, IconTransformType transform, short theResID) {
    if (theRect == NULL) {
        ICON_LOG("PlotIconID: NULL rect\n");
        return;
    }

    ICON_LOG("PlotIconID: resID=%d align=%d transform=%d\n",
             theResID, align, transform);

    /* Load icon from resources */
    Handle iconHandle = GetIcon(theResID);

    if (iconHandle == NULL) {
        ICON_LOG("PlotIconID: Icon resource %d not found\n", theResID);
        return;
    }

    /* Plot the icon with alignment and transform */
    PlotIconHandle(theRect, align, transform, iconHandle);

    /* Note: Don't dispose icon handle - it's cached by Resource Manager */
}

/*
 * GetIcon - Get icon resource
 *
 * Loads an icon resource from the current resource file(s).
 *
 * Parameters:
 *   iconID - Resource ID of icon to load
 *
 * Returns:
 *   Handle to icon data, or NULL if not found
 *
 * Notes:
 * - Icon data is 128 bytes for 32x32 black-and-white icon
 * - Handle is managed by Resource Manager
 * - Don't dispose the handle - call ReleaseResource instead
 * - Icon remains in memory until explicitly released
 *
 * Example:
 *   Handle icon = GetIcon(128);
 *   if (icon != NULL) {
 *       PlotIcon(&myRect, icon);
 *   }
 *
 * Based on Inside Macintosh: Macintosh Toolbox Essentials, Chapter 7-22
 */
Handle GetIcon(short iconID) {
    ICON_LOG("GetIcon: iconID=%d\n", iconID);

    /* Load icon from resources */
    extern Handle GetResource(ResType theType, short theID);
    Handle iconHandle = GetResource(kIconResourceType, iconID);

    if (iconHandle == NULL) {
        ICON_LOG("GetIcon: Icon %d not found in resources\n", iconID);
        return NULL;
    }

    ICON_LOG("GetIcon: Loaded icon %d at %p\n", iconID, (void*)iconHandle);
    return iconHandle;
}

/*
 * PlotIconHandle - Plot an icon from handle with alignment and transform
 *
 * Plots an icon with specified alignment and transformation.
 *
 * Parameters:
 *   theRect - Rectangle to draw icon within
 *   align - Icon alignment
 *   transform - Icon transformation
 *   theIcon - Handle to icon data
 *
 * Based on Inside Macintosh: Macintosh Toolbox Essentials, Chapter 7-24
 */
void PlotIconHandle(const Rect* theRect, IconAlignmentType align, IconTransformType transform, Handle theIcon) {
    if (theRect == NULL || theIcon == NULL) {
        ICON_LOG("PlotIconHandle: NULL parameter\n");
        return;
    }

    ICON_LOG("PlotIconHandle: align=%d transform=%d\n", align, transform);

    /* Calculate aligned rect based on alignment parameter */
    Rect alignedRect = *theRect;

    /* Apply alignment
     * (Simplified - full implementation would properly calculate offsets)
     */
    if (align != kAlignNone) {
        ICON_LOG("PlotIconHandle: Applying alignment\n");
        /* TODO: Calculate proper alignment offsets */
    }

    /* Apply transform
     * (Simplified - full implementation would modify drawing based on transform)
     */
    if (transform != kTransformNone) {
        ICON_LOG("PlotIconHandle: Applying transform %d\n", transform);
        /* TODO: Apply proper transformation (dim for disabled, etc.) */
    }

    /* Plot the icon */
    PlotIcon(&alignedRect, theIcon);
}

/*
 * GetIconSuite - Get complete icon suite from resources
 *
 * Loads all icon representations (32x32, 16x16, color, etc.) for a
 * given resource ID.
 *
 * Parameters:
 *   theIconSuite - Output: handle to icon suite
 *   theResID - Resource ID of icon suite
 *   selector - Which icon types to load (mask of icon types)
 *
 * Returns:
 *   noErr if successful, or resource error code
 *
 * Icon selector values:
 *   svAllLargeData - All 32x32 icons
 *   svAllSmallData - All 16x16 icons
 *   svAllMiniData - All mini icons
 *   svAll32Data - All 32x32 color depths
 *   svAll16Data - All 16x16 color depths
 *   svAllAvailableData - All available icons
 *
 * Example:
 *   Handle iconSuite;
 *   OSErr err = GetIconSuite(&iconSuite, 128, svAllAvailableData);
 *   if (err == noErr) {
 *       PlotIconSuite(&myRect, atAbsoluteCenter, ttNone, iconSuite);
 *       DisposeIconSuite(iconSuite, true);
 *   }
 *
 * Based on Inside Macintosh: Macintosh Toolbox Essentials, Chapter 7-27
 */
OSErr GetIconSuite(Handle* theIconSuite, short theResID, IconSelectorValue selector) {
    if (theIconSuite == NULL) {
        ICON_LOG("GetIconSuite: NULL output parameter\n");
        return paramErr;
    }

    ICON_LOG("GetIconSuite: resID=%d selector=0x%04X\n", theResID, selector);

    /* Allocate icon suite structure
     * (Simplified - full implementation would load all icon variants)
     */
    extern Handle NewHandle(Size byteCount);
    Handle suite = NewHandle(sizeof(Ptr) * 16);  /* Placeholder for icon pointers */

    if (suite == NULL) {
        ICON_LOG("GetIconSuite: Failed to allocate icon suite\n");
        return memFullErr;
    }

    /* Load icon resources based on selector
     * (Simplified - full implementation would load ICN#, ics#, icl8, etc.)
     */
    ICON_LOG("GetIconSuite: Created icon suite at %p\n", (void*)suite);

    *theIconSuite = suite;
    return noErr;
}

/*
 * PlotIconSuite - Plot icon suite
 *
 * Plots the best icon from an icon suite for the current display depth
 * and size.
 *
 * Parameters:
 *   theRect - Rectangle to draw icon within
 *   align - Icon alignment
 *   transform - Icon transformation
 *   theIconSuite - Handle to icon suite
 *
 * Returns:
 *   noErr if successful, or error code
 *
 * Example:
 *   Handle iconSuite;
 *   GetIconSuite(&iconSuite, 128, svAllAvailableData);
 *   PlotIconSuite(&myRect, atAbsoluteCenter, ttSelected, iconSuite);
 *
 * Based on Inside Macintosh: Macintosh Toolbox Essentials, Chapter 7-29
 */
OSErr PlotIconSuite(const Rect* theRect, IconAlignmentType align, IconTransformType transform, Handle theIconSuite) {
    if (theRect == NULL || theIconSuite == NULL) {
        ICON_LOG("PlotIconSuite: NULL parameter\n");
        return paramErr;
    }

    ICON_LOG("PlotIconSuite: suite=%p align=%d transform=%d\n",
             (void*)theIconSuite, align, transform);

    /* Select best icon from suite based on display depth and rect size
     * (Simplified - full implementation would choose optimal icon)
     */

    /* For now, just plot placeholder */
    ICON_LOG("PlotIconSuite: Drawing icon suite (placeholder)\n");

    return noErr;
}

/*
 * DisposeIconSuite - Dispose of icon suite
 *
 * Releases memory used by an icon suite.
 *
 * Parameters:
 *   theIconSuite - Handle to icon suite to dispose
 *   disposeData - If true, dispose icon data; if false, just suite structure
 *
 * Returns:
 *   noErr if successful, or error code
 *
 * Notes:
 * - If disposeData is false, individual icon resources are left in memory
 * - If disposeData is true, all icon resources are released
 *
 * Example:
 *   DisposeIconSuite(iconSuite, true);  // Dispose suite and all icons
 *
 * Based on Inside Macintosh: Macintosh Toolbox Essentials, Chapter 7-30
 */
OSErr DisposeIconSuite(Handle theIconSuite, Boolean disposeData) {
    if (theIconSuite == NULL) {
        ICON_LOG("DisposeIconSuite: NULL suite\n");
        return paramErr;
    }

    ICON_LOG("DisposeIconSuite: suite=%p disposeData=%d\n",
             (void*)theIconSuite, disposeData);

    if (disposeData) {
        /* Dispose all icon resources in suite */
        ICON_LOG("DisposeIconSuite: Disposing icon data\n");
        /* TODO: Iterate through suite and dispose each icon */
    }

    /* Dispose the suite structure itself */
    extern void DisposeHandle(Handle h);
    DisposeHandle(theIconSuite);

    ICON_LOG("DisposeIconSuite: Suite disposed\n");
    return noErr;
}
