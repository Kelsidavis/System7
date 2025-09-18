/*
 * PageLayout.c
 *
 * Page Layout and Formatting implementation for System 7.1 Portable
 * Handles page setup, margins, scaling, and layout calculations
 *
 * Based on Apple's Print Manager page layout from Mac OS System 7.1
 */

#include "PageLayout.h"
#include "PrintManager.h"
#include "QuickDraw.h"
#include "Memory.h"
#include <string.h>
#include <math.h>

/* Standard Paper Sizes */
const TPaperDimensions gStandardPaperSizes[] = {
    {612, 792, "\pLetter", false},        /* Letter: 8.5 x 11 inches */
    {612, 1008, "\pLegal", false},        /* Legal: 8.5 x 14 inches */
    {595, 842, "\pA4", true},             /* A4: 210 x 297 mm */
    {842, 1191, "\pA3", true},            /* A3: 297 x 420 mm */
    {792, 1224, "\pTabloid", false},      /* Tabloid: 11 x 17 inches */
    {522, 756, "\pExecutive", false},     /* Executive: 7.25 x 10.5 inches */
    {279, 540, "\p#10 Envelope", false}, /* #10 Envelope: 4.125 x 9.5 inches */
    {312, 624, "\pMonarch Envelope", false}, /* Monarch: 3.875 x 7.5 inches */
    {461, 649, "\pC5 Envelope", true},   /* C5: 162 x 229 mm */
    {499, 709, "\pB5", true},            /* B5: 176 x 250 mm */
    {516, 729, "\pFolio", false}         /* Folio: 8.5 x 13 inches */
};

const short gStandardPaperSizeCount = sizeof(gStandardPaperSizes) / sizeof(TPaperDimensions);

/* Default margins for different paper types */
static const Rect gDefaultMargins = {72, 72, 72, 72}; /* 1 inch on all sides */

/*
 * Page Layout Functions
 */

/*
 * InitPageLayout - Initialize page layout with defaults
 */
OSErr InitPageLayout(TPageLayoutPtr layout)
{
    if (!layout) {
        return paramErr;
    }

    /* Clear structure */
    memset(layout, 0, sizeof(TPageLayout));

    /* Set defaults */
    layout->paperSize = gStandardPaperSizes[0]; /* Letter size */
    layout->orientation = kPortrait;
    layout->margins = gDefaultMargins;
    layout->scale = 100; /* 100% */
    layout->centerHorizontally = false;
    layout->centerVertically = false;
    layout->fitToPage = false;
    layout->maintainAspectRatio = true;
    layout->resolution = 72; /* 72 DPI */
    layout->colorMode = true;

    /* Calculate dependent values */
    return CalculatePrintableArea(layout);
}

/*
 * SetPaperSize - Set paper size by type
 */
OSErr SetPaperSize(TPageLayoutPtr layout, short paperType)
{
    if (!layout || paperType < 0 || paperType >= gStandardPaperSizeCount) {
        return paramErr;
    }

    layout->paperSize = gStandardPaperSizes[paperType];
    return CalculatePrintableArea(layout);
}

/*
 * SetCustomPaperSize - Set custom paper size
 */
OSErr SetCustomPaperSize(TPageLayoutPtr layout, short width, short height)
{
    if (!layout || !IsValidPaperSize(width, height)) {
        return paramErr;
    }

    layout->paperSize.width = width;
    layout->paperSize.height = height;
    layout->paperSize.name = "\pCustom";
    layout->paperSize.isMetric = false;

    return CalculatePrintableArea(layout);
}

/*
 * SetPageOrientation - Set page orientation
 */
OSErr SetPageOrientation(TPageLayoutPtr layout, short orientation)
{
    if (!layout || orientation < kPortrait || orientation > kReverseLandscape) {
        return paramErr;
    }

    /* Swap width/height for landscape orientations */
    if ((layout->orientation == kPortrait || layout->orientation == kReversePortrait) &&
        (orientation == kLandscape || orientation == kReverseLandscape)) {
        /* Switching from portrait to landscape */
        short temp = layout->paperSize.width;
        layout->paperSize.width = layout->paperSize.height;
        layout->paperSize.height = temp;
    } else if ((layout->orientation == kLandscape || layout->orientation == kReverseLandscape) &&
               (orientation == kPortrait || orientation == kReversePortrait)) {
        /* Switching from landscape to portrait */
        short temp = layout->paperSize.width;
        layout->paperSize.width = layout->paperSize.height;
        layout->paperSize.height = temp;
    }

    layout->orientation = orientation;
    return CalculatePrintableArea(layout);
}

/*
 * SetPageMargins - Set page margins
 */
OSErr SetPageMargins(TPageLayoutPtr layout, short top, short left, short bottom, short right)
{
    if (!layout) {
        return paramErr;
    }

    /* Validate margins */
    if (!IsValidMargin(top) || !IsValidMargin(left) ||
        !IsValidMargin(bottom) || !IsValidMargin(right)) {
        return paramErr;
    }

    /* Check that margins don't exceed paper size */
    if (left + right >= layout->paperSize.width ||
        top + bottom >= layout->paperSize.height) {
        return paramErr;
    }

    layout->margins.top = top;
    layout->margins.left = left;
    layout->margins.bottom = bottom;
    layout->margins.right = right;

    return CalculatePrintableArea(layout);
}

/*
 * SetPageScale - Set page scale
 */
OSErr SetPageScale(TPageLayoutPtr layout, short scale)
{
    if (!layout || !IsValidScale(scale)) {
        return paramErr;
    }

    layout->scale = scale;
    return CalculateContentArea(layout);
}

/*
 * Layout Calculations
 */

/*
 * CalculatePrintableArea - Calculate the printable area
 */
OSErr CalculatePrintableArea(TPageLayoutPtr layout)
{
    if (!layout) {
        return paramErr;
    }

    /* Printable area is paper size minus margins */
    layout->printableArea.left = layout->margins.left;
    layout->printableArea.top = layout->margins.top;
    layout->printableArea.right = layout->paperSize.width - layout->margins.right;
    layout->printableArea.bottom = layout->paperSize.height - layout->margins.bottom;

    return CalculateContentArea(layout);
}

/*
 * CalculateContentArea - Calculate the area available for content
 */
OSErr CalculateContentArea(TPageLayoutPtr layout)
{
    if (!layout) {
        return paramErr;
    }

    /* Start with printable area */
    layout->contentArea = layout->printableArea;

    /* Apply scaling */
    if (layout->scale != 100) {
        short width = layout->contentArea.right - layout->contentArea.left;
        short height = layout->contentArea.bottom - layout->contentArea.top;

        width = (width * layout->scale) / 100;
        height = (height * layout->scale) / 100;

        if (layout->centerHorizontally) {
            short centerX = (layout->printableArea.left + layout->printableArea.right) / 2;
            layout->contentArea.left = centerX - width / 2;
            layout->contentArea.right = centerX + width / 2;
        } else {
            layout->contentArea.right = layout->contentArea.left + width;
        }

        if (layout->centerVertically) {
            short centerY = (layout->printableArea.top + layout->printableArea.bottom) / 2;
            layout->contentArea.top = centerY - height / 2;
            layout->contentArea.bottom = centerY + height / 2;
        } else {
            layout->contentArea.bottom = layout->contentArea.top + height;
        }
    }

    return noErr;
}

/*
 * CalculateScaledDimensions - Calculate scaled dimensions for a rectangle
 */
OSErr CalculateScaledDimensions(TPageLayoutPtr layout, Rect *sourceRect, Rect *scaledRect)
{
    if (!layout || !sourceRect || !scaledRect) {
        return paramErr;
    }

    *scaledRect = *sourceRect;

    /* Apply page scale */
    if (layout->scale != 100) {
        short width = sourceRect->right - sourceRect->left;
        short height = sourceRect->bottom - sourceRect->top;

        width = (width * layout->scale) / 100;
        height = (height * layout->scale) / 100;

        scaledRect->right = scaledRect->left + width;
        scaledRect->bottom = scaledRect->top + height;
    }

    return noErr;
}

/*
 * FitRectToPage - Fit a rectangle to the page
 */
OSErr FitRectToPage(TPageLayoutPtr layout, Rect *sourceRect, Rect *fittedRect)
{
    if (!layout || !sourceRect || !fittedRect) {
        return paramErr;
    }

    *fittedRect = *sourceRect;

    if (layout->fitToPage) {
        short sourceWidth = sourceRect->right - sourceRect->left;
        short sourceHeight = sourceRect->bottom - sourceRect->top;
        short contentWidth = layout->contentArea.right - layout->contentArea.left;
        short contentHeight = layout->contentArea.bottom - layout->contentArea.top;

        if (sourceWidth > 0 && sourceHeight > 0) {
            float scaleX = (float)contentWidth / sourceWidth;
            float scaleY = (float)contentHeight / sourceHeight;
            float scale = scaleX < scaleY ? scaleX : scaleY;

            if (layout->maintainAspectRatio) {
                /* Use the smaller scale to maintain aspect ratio */
                sourceWidth = (short)(sourceWidth * scale);
                sourceHeight = (short)(sourceHeight * scale);
            } else {
                /* Scale independently */
                sourceWidth = contentWidth;
                sourceHeight = contentHeight;
            }

            /* Center in content area */
            short centerX = (layout->contentArea.left + layout->contentArea.right) / 2;
            short centerY = (layout->contentArea.top + layout->contentArea.bottom) / 2;

            fittedRect->left = centerX - sourceWidth / 2;
            fittedRect->top = centerY - sourceHeight / 2;
            fittedRect->right = fittedRect->left + sourceWidth;
            fittedRect->bottom = fittedRect->top + sourceHeight;
        }
    }

    return noErr;
}

/*
 * Page Layout Validation
 */

/*
 * ValidatePageLayout - Validate a page layout
 */
Boolean ValidatePageLayout(TPageLayoutPtr layout)
{
    if (!layout) {
        return false;
    }

    /* Check paper size */
    if (!IsValidPaperSize(layout->paperSize.width, layout->paperSize.height)) {
        return false;
    }

    /* Check margins */
    if (!IsValidMargin(layout->margins.top) || !IsValidMargin(layout->margins.left) ||
        !IsValidMargin(layout->margins.bottom) || !IsValidMargin(layout->margins.right)) {
        return false;
    }

    /* Check that margins don't exceed paper size */
    if (layout->margins.left + layout->margins.right >= layout->paperSize.width ||
        layout->margins.top + layout->margins.bottom >= layout->paperSize.height) {
        return false;
    }

    /* Check scale */
    if (!IsValidScale(layout->scale)) {
        return false;
    }

    /* Check orientation */
    if (layout->orientation < kPortrait || layout->orientation > kReverseLandscape) {
        return false;
    }

    return true;
}

/*
 * CorrectPageLayout - Correct invalid values in page layout
 */
OSErr CorrectPageLayout(TPageLayoutPtr layout)
{
    if (!layout) {
        return paramErr;
    }

    /* Correct paper size */
    if (!IsValidPaperSize(layout->paperSize.width, layout->paperSize.height)) {
        layout->paperSize = gStandardPaperSizes[0]; /* Default to Letter */
    }

    /* Correct margins */
    if (!IsValidMargin(layout->margins.top)) {
        layout->margins.top = kDefaultMargin;
    }
    if (!IsValidMargin(layout->margins.left)) {
        layout->margins.left = kDefaultMargin;
    }
    if (!IsValidMargin(layout->margins.bottom)) {
        layout->margins.bottom = kDefaultMargin;
    }
    if (!IsValidMargin(layout->margins.right)) {
        layout->margins.right = kDefaultMargin;
    }

    /* Ensure margins don't exceed paper size */
    short maxHMargin = (layout->paperSize.width - 72) / 2; /* Leave at least 1 inch for content */
    short maxVMargin = (layout->paperSize.height - 72) / 2;

    if (layout->margins.left > maxHMargin) layout->margins.left = maxHMargin;
    if (layout->margins.right > maxHMargin) layout->margins.right = maxHMargin;
    if (layout->margins.top > maxVMargin) layout->margins.top = maxVMargin;
    if (layout->margins.bottom > maxVMargin) layout->margins.bottom = maxVMargin;

    /* Correct scale */
    if (!IsValidScale(layout->scale)) {
        layout->scale = 100;
    }

    /* Correct orientation */
    if (layout->orientation < kPortrait || layout->orientation > kReverseLandscape) {
        layout->orientation = kPortrait;
    }

    /* Recalculate areas */
    return CalculatePrintableArea(layout);
}

/*
 * IsValidPaperSize - Check if paper size is valid
 */
Boolean IsValidPaperSize(short width, short height)
{
    /* Minimum size: 3 x 3 inches (216 x 216 points) */
    /* Maximum size: 17 x 22 inches (1224 x 1584 points) */
    return (width >= 216 && width <= 1584 &&
            height >= 216 && height <= 1584);
}

/*
 * IsValidMargin - Check if margin is valid
 */
Boolean IsValidMargin(short margin)
{
    return (margin >= kMinMargin && margin <= kMaxMargin);
}

/*
 * IsValidScale - Check if scale is valid
 */
Boolean IsValidScale(short scale)
{
    return (scale >= kMinScale && scale <= kMaxScale);
}

/*
 * Page Layout Utilities
 */

/*
 * CopyPageLayout - Copy page layout
 */
OSErr CopyPageLayout(TPageLayoutPtr source, TPageLayoutPtr dest)
{
    if (!source || !dest) {
        return paramErr;
    }

    *dest = *source;
    return noErr;
}

/*
 * ComparePageLayouts - Compare two page layouts
 */
Boolean ComparePageLayouts(TPageLayoutPtr layout1, TPageLayoutPtr layout2)
{
    if (!layout1 || !layout2) {
        return false;
    }

    return (memcmp(layout1, layout2, sizeof(TPageLayout)) == 0);
}

/*
 * ResetPageLayoutToDefaults - Reset page layout to default values
 */
OSErr ResetPageLayoutToDefaults(TPageLayoutPtr layout)
{
    return InitPageLayout(layout);
}

/*
 * Coordinate Conversion Functions
 */

/*
 * PointsToPixels - Convert points to pixels
 */
void PointsToPixels(short points, short resolution, short *pixels)
{
    if (pixels) {
        *pixels = (points * resolution) / kPointsPerInch;
    }
}

/*
 * PixelsToPoints - Convert pixels to points
 */
void PixelsToPoints(short pixels, short resolution, short *points)
{
    if (points) {
        *points = (pixels * kPointsPerInch) / resolution;
    }
}

/*
 * InchesToPoints - Convert inches to points
 */
void InchesToPoints(Fixed inches, short *points)
{
    if (points) {
        *points = (short)((inches * kPointsPerInch) >> 16);
    }
}

/*
 * PointsToInches - Convert points to inches
 */
void PointsToInches(short points, Fixed *inches)
{
    if (inches) {
        *inches = ((long)points << 16) / kPointsPerInch;
    }
}

/*
 * CentimetersToPoints - Convert centimeters to points
 */
void CentimetersToPoints(Fixed cm, short *points)
{
    if (points) {
        *points = (short)((cm * kPointsPerCm) >> 16);
    }
}

/*
 * PointsToCentimeters - Convert points to centimeters
 */
void PointsToCentimeters(short points, Fixed *cm)
{
    if (cm) {
        *cm = ((long)points << 16) / kPointsPerCm;
    }
}

/*
 * Page Transformation Functions
 */

/*
 * CalculatePageTransform - Calculate page transformation
 */
OSErr CalculatePageTransform(TPageLayoutPtr layout, Rect *sourceRect, TPageTransformPtr transform)
{
    if (!layout || !sourceRect || !transform) {
        return paramErr;
    }

    /* Clear transform */
    memset(transform, 0, sizeof(TPageTransform));

    /* Calculate scale factors */
    if (layout->fitToPage) {
        short sourceWidth = sourceRect->right - sourceRect->left;
        short sourceHeight = sourceRect->bottom - sourceRect->top;
        short contentWidth = layout->contentArea.right - layout->contentArea.left;
        short contentHeight = layout->contentArea.bottom - layout->contentArea.top;

        if (sourceWidth > 0 && sourceHeight > 0) {
            transform->scaleX = ((long)contentWidth << 16) / sourceWidth;
            transform->scaleY = ((long)contentHeight << 16) / sourceHeight;

            if (layout->maintainAspectRatio) {
                /* Use the smaller scale for both axes */
                if (transform->scaleX < transform->scaleY) {
                    transform->scaleY = transform->scaleX;
                } else {
                    transform->scaleX = transform->scaleY;
                }
            }
        } else {
            transform->scaleX = 0x10000; /* 1.0 */
            transform->scaleY = 0x10000; /* 1.0 */
        }
    } else {
        transform->scaleX = (layout->scale << 16) / 100;
        transform->scaleY = (layout->scale << 16) / 100;
    }

    /* Calculate translation */
    transform->translateX = layout->contentArea.left - sourceRect->left;
    transform->translateY = layout->contentArea.top - sourceRect->top;

    /* Handle orientation */
    switch (layout->orientation) {
        case kLandscape:
            transform->rotation = 90;
            break;
        case kReversePortrait:
            transform->rotation = 180;
            break;
        case kReverseLandscape:
            transform->rotation = 270;
            break;
        default:
            transform->rotation = 0;
            break;
    }

    return noErr;
}

/*
 * ApplyPageTransform - Apply transformation to a point
 */
OSErr ApplyPageTransform(TPageTransformPtr transform, Point *point)
{
    if (!transform || !point) {
        return paramErr;
    }

    /* Apply scaling */
    point->h = (point->h * transform->scaleX) >> 16;
    point->v = (point->v * transform->scaleY) >> 16;

    /* Apply translation */
    point->h += transform->translateX;
    point->v += transform->translateY;

    /* Apply rotation (simplified - would need proper matrix math) */
    if (transform->rotation != 0) {
        /* For now, just handle 90-degree rotations */
        switch (transform->rotation) {
            case 90:
                {
                    short temp = point->h;
                    point->h = -point->v;
                    point->v = temp;
                }
                break;
            case 180:
                point->h = -point->h;
                point->v = -point->v;
                break;
            case 270:
                {
                    short temp = point->h;
                    point->h = point->v;
                    point->v = -temp;
                }
                break;
        }
    }

    /* Apply flipping */
    if (transform->flipHorizontal) {
        point->h = -point->h;
    }
    if (transform->flipVertical) {
        point->v = -point->v;
    }

    return noErr;
}

/*
 * ApplyPageTransformToRect - Apply transformation to a rectangle
 */
OSErr ApplyPageTransformToRect(TPageTransformPtr transform, Rect *rect)
{
    if (!transform || !rect) {
        return paramErr;
    }

    /* Transform all four corners */
    Point topLeft = {rect->left, rect->top};
    Point topRight = {rect->right, rect->top};
    Point bottomLeft = {rect->left, rect->bottom};
    Point bottomRight = {rect->right, rect->bottom};

    ApplyPageTransform(transform, &topLeft);
    ApplyPageTransform(transform, &topRight);
    ApplyPageTransform(transform, &bottomLeft);
    ApplyPageTransform(transform, &bottomRight);

    /* Find bounding rectangle */
    rect->left = topLeft.h;
    rect->right = topLeft.h;
    rect->top = topLeft.v;
    rect->bottom = topLeft.v;

    if (topRight.h < rect->left) rect->left = topRight.h;
    if (topRight.h > rect->right) rect->right = topRight.h;
    if (topRight.v < rect->top) rect->top = topRight.v;
    if (topRight.v > rect->bottom) rect->bottom = topRight.v;

    if (bottomLeft.h < rect->left) rect->left = bottomLeft.h;
    if (bottomLeft.h > rect->right) rect->right = bottomLeft.h;
    if (bottomLeft.v < rect->top) rect->top = bottomLeft.v;
    if (bottomLeft.v > rect->bottom) rect->bottom = bottomLeft.v;

    if (bottomRight.h < rect->left) rect->left = bottomRight.h;
    if (bottomRight.h > rect->right) rect->right = bottomRight.h;
    if (bottomRight.v < rect->top) rect->top = bottomRight.v;
    if (bottomRight.v > rect->bottom) rect->bottom = bottomRight.v;

    return noErr;
}

/*
 * Utility Functions
 */

/*
 * GetDefaultPageLayout - Get default page layout
 */
OSErr GetDefaultPageLayout(TPageLayoutPtr layout)
{
    return InitPageLayout(layout);
}

/*
 * GetPageLayoutForPrinter - Get page layout optimized for a specific printer
 */
OSErr GetPageLayoutForPrinter(StringPtr printerName, TPageLayoutPtr layout)
{
    if (!printerName || !layout) {
        return paramErr;
    }

    /* Initialize with defaults */
    OSErr err = InitPageLayout(layout);
    if (err != noErr) {
        return err;
    }

    /* Could customize based on printer capabilities */
    /* For now, just return defaults */

    return noErr;
}

/*
 * ValidatePageLayoutForPrinter - Validate layout for a specific printer
 */
OSErr ValidatePageLayoutForPrinter(StringPtr printerName, TPageLayoutPtr layout)
{
    if (!printerName || !layout) {
        return paramErr;
    }

    /* Could check printer-specific constraints */
    /* For now, just use general validation */

    return ValidatePageLayout(layout) ? noErr : paramErr;
}