/*
 * PageLayout.h
 *
 * Page Layout and Formatting for System 7.1 Portable
 * Handles page setup, margins, scaling, and layout calculations
 *
 * Based on Apple's Print Manager page layout from Mac OS System 7.1
 */

#ifndef __PAGELAYOUT__
#define __PAGELAYOUT__

#include "PrintTypes.h"
#include "QuickDraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Page Layout Constants */
enum {
    kPointsPerInch = 72,                /* Points per inch */
    kPointsPerCm = 28,                  /* Approximate points per centimeter */
    kMaxMargin = 144,                   /* Maximum margin in points (2 inches) */
    kMinMargin = 18,                    /* Minimum margin in points (0.25 inches) */
    kDefaultMargin = 72,                /* Default margin in points (1 inch) */
    kMaxScale = 400,                    /* Maximum scale percentage */
    kMinScale = 25                      /* Minimum scale percentage */
};

/* Page Orientation */
enum {
    kPortrait = 0,                      /* Portrait orientation */
    kLandscape = 1,                     /* Landscape orientation */
    kReversePortrait = 2,               /* Reverse portrait */
    kReverseLandscape = 3               /* Reverse landscape */
};

/* Paper Size Definitions */
struct TPaperDimensions {
    short width;                        /* Width in points */
    short height;                       /* Height in points */
    StringPtr name;                     /* Paper size name */
    Boolean isMetric;                   /* Uses metric measurements */
};
typedef struct TPaperDimensions TPaperDimensions;

/* Standard Paper Sizes */
extern const TPaperDimensions gStandardPaperSizes[];
extern const short gStandardPaperSizeCount;

/* Page Layout Information */
struct TPageLayout {
    TPaperDimensions paperSize;         /* Paper dimensions */
    short orientation;                  /* Page orientation */
    Rect margins;                       /* Page margins (top, left, bottom, right) */
    short scale;                        /* Scale percentage (100 = 100%) */
    Boolean centerHorizontally;         /* Center content horizontally */
    Boolean centerVertically;           /* Center content vertically */
    Boolean fitToPage;                  /* Fit content to page */
    Boolean maintainAspectRatio;        /* Maintain aspect ratio when scaling */
    Rect printableArea;                 /* Calculated printable area */
    Rect contentArea;                   /* Area available for content */
    short resolution;                   /* Print resolution in DPI */
    Boolean colorMode;                  /* Color or black & white */
};
typedef struct TPageLayout TPageLayout;
typedef TPageLayout *TPageLayoutPtr;

/* Page Setup Functions */
OSErr InitPageLayout(TPageLayoutPtr layout);
OSErr SetPaperSize(TPageLayoutPtr layout, short paperType);
OSErr SetCustomPaperSize(TPageLayoutPtr layout, short width, short height);
OSErr SetPageOrientation(TPageLayoutPtr layout, short orientation);
OSErr SetPageMargins(TPageLayoutPtr layout, short top, short left, short bottom, short right);
OSErr SetPageScale(TPageLayoutPtr layout, short scale);

/* Layout Calculations */
OSErr CalculatePrintableArea(TPageLayoutPtr layout);
OSErr CalculateContentArea(TPageLayoutPtr layout);
OSErr CalculateScaledDimensions(TPageLayoutPtr layout, Rect *sourceRect, Rect *scaledRect);
OSErr FitRectToPage(TPageLayoutPtr layout, Rect *sourceRect, Rect *fittedRect);

/* Page Layout Validation */
Boolean ValidatePageLayout(TPageLayoutPtr layout);
OSErr CorrectPageLayout(TPageLayoutPtr layout);
Boolean IsValidPaperSize(short width, short height);
Boolean IsValidMargin(short margin);
Boolean IsValidScale(short scale);

/* Page Layout Utilities */
OSErr CopyPageLayout(TPageLayoutPtr source, TPageLayoutPtr dest);
Boolean ComparePageLayouts(TPageLayoutPtr layout1, TPageLayoutPtr layout2);
OSErr ResetPageLayoutToDefaults(TPageLayoutPtr layout);

/* Coordinate Conversion */
void PointsToPixels(short points, short resolution, short *pixels);
void PixelsToPoints(short pixels, short resolution, short *points);
void InchesToPoints(Fixed inches, short *points);
void PointsToInches(short points, Fixed *inches);
void CentimetersToPoints(Fixed cm, short *points);
void PointsToCentimeters(short points, Fixed *cm);

/* Page Transformation */
struct TPageTransform {
    Fixed scaleX;                       /* Horizontal scale factor */
    Fixed scaleY;                       /* Vertical scale factor */
    short translateX;                   /* Horizontal translation */
    short translateY;                   /* Vertical translation */
    short rotation;                     /* Rotation angle in degrees */
    Boolean flipHorizontal;             /* Flip horizontally */
    Boolean flipVertical;               /* Flip vertically */
};
typedef struct TPageTransform TPageTransform;
typedef TPageTransform *TPageTransformPtr;

OSErr CalculatePageTransform(TPageLayoutPtr layout, Rect *sourceRect, TPageTransformPtr transform);
OSErr ApplyPageTransform(TPageTransformPtr transform, Point *point);
OSErr ApplyPageTransformToRect(TPageTransformPtr transform, Rect *rect);

/* Print Resolution Support */
struct TResolutionInfo {
    short xResolution;                  /* Horizontal resolution in DPI */
    short yResolution;                  /* Vertical resolution in DPI */
    Boolean isSupported;                /* Is this resolution supported */
    StringPtr description;              /* Resolution description */
};
typedef struct TResolutionInfo TResolutionInfo;

OSErr GetSupportedResolutions(TResolutionInfo resolutions[], short *count);
OSErr SetPrintResolution(TPageLayoutPtr layout, short xRes, short yRes);
OSErr GetOptimalResolution(TPageLayoutPtr layout, short *xRes, short *yRes);

/* Page Imaging */
struct TPageImage {
    BitMap bitmap;                      /* Page bitmap */
    PicHandle picture;                  /* Page picture */
    Rect sourceRect;                    /* Source rectangle */
    Rect destRect;                      /* Destination rectangle */
    short imageType;                    /* Image type (bitmap, picture, text) */
    Handle imageData;                   /* Image data handle */
};
typedef struct TPageImage TPageImage;
typedef TPageImage *TPageImagePtr;

OSErr CreatePageImage(TPageLayoutPtr layout, TPageImagePtr *image);
OSErr DisposePageImage(TPageImagePtr image);
OSErr RenderPageToImage(TPageLayoutPtr layout, PicHandle sourcePic, TPageImagePtr image);
OSErr ScaleImageToPage(TPageImagePtr image, TPageLayoutPtr layout);

/* Multi-Page Layout */
struct TMultiPageLayout {
    short pagesPerSheet;                /* Number of pages per sheet */
    short rows;                         /* Number of rows */
    short columns;                      /* Number of columns */
    short pageSpacing;                  /* Spacing between pages */
    Boolean drawBorders;                /* Draw borders around pages */
    Boolean drawPageNumbers;            /* Draw page numbers */
    TPageLayout sheetLayout;            /* Layout for the sheet */
};
typedef struct TMultiPageLayout TMultiPageLayout;
typedef TMultiPageLayout *TMultiPageLayoutPtr;

OSErr SetupMultiPageLayout(TMultiPageLayoutPtr multiLayout, short pagesPerSheet);
OSErr CalculatePagePositions(TMultiPageLayoutPtr multiLayout, Rect pageRects[]);
OSErr RenderMultiPageSheet(TMultiPageLayoutPtr multiLayout, PicHandle pages[], short pageCount);

/* Page Numbering */
struct TPageNumbering {
    Boolean enabled;                    /* Page numbering enabled */
    short position;                     /* Position on page */
    short startNumber;                  /* Starting page number */
    StringPtr prefix;                   /* Number prefix */
    StringPtr suffix;                   /* Number suffix */
    short fontID;                       /* Font for page numbers */
    short fontSize;                     /* Font size */
    short fontStyle;                    /* Font style */
};
typedef struct TPageNumbering TPageNumbering;

enum {
    kPageNumberTopLeft = 0,
    kPageNumberTopCenter = 1,
    kPageNumberTopRight = 2,
    kPageNumberBottomLeft = 3,
    kPageNumberBottomCenter = 4,
    kPageNumberBottomRight = 5
};

OSErr SetupPageNumbering(TPageNumbering *numbering, Boolean enabled, short position);
OSErr DrawPageNumber(TPageNumbering *numbering, short pageNumber, Rect *pageRect);

/* Print Preview Layout */
struct TPreviewLayout {
    TPageLayout pageLayout;             /* Base page layout */
    short zoomLevel;                    /* Zoom level percentage */
    Boolean showMargins;                /* Show margin guides */
    Boolean showPrintableArea;          /* Show printable area outline */
    Boolean showRulers;                 /* Show rulers */
    Boolean showPageBreaks;             /* Show page break indicators */
    short backgroundColor;              /* Background color */
    short paperColor;                   /* Paper color */
};
typedef struct TPreviewLayout TPreviewLayout;
typedef TPreviewLayout *TPreviewLayoutPtr;

OSErr SetupPreviewLayout(TPreviewLayoutPtr preview, TPageLayoutPtr pageLayout);
OSErr DrawPreviewPage(TPreviewLayoutPtr preview, PicHandle pageContent, Rect *drawRect);
OSErr DrawPreviewGuides(TPreviewLayoutPtr preview, Rect *pageRect);

/* Layout Export/Import */
OSErr SavePageLayoutToResource(TPageLayoutPtr layout, short resourceID);
OSErr LoadPageLayoutFromResource(short resourceID, TPageLayoutPtr layout);
OSErr SavePageLayoutToFile(TPageLayoutPtr layout, FSSpec *file);
OSErr LoadPageLayoutFromFile(FSSpec *file, TPageLayoutPtr layout);

/* Layout Utilities */
OSErr GetDefaultPageLayout(TPageLayoutPtr layout);
OSErr GetPageLayoutForPrinter(StringPtr printerName, TPageLayoutPtr layout);
OSErr ValidatePageLayoutForPrinter(StringPtr printerName, TPageLayoutPtr layout);

/* Advanced Layout Features */
struct TAdvancedLayout {
    Boolean useColorManagement;         /* Use color management */
    Boolean optimizeForSpeed;           /* Optimize for printing speed */
    Boolean optimizeForQuality;         /* Optimize for print quality */
    Boolean printInReverse;             /* Print pages in reverse order */
    Boolean collatePages;               /* Collate multiple copies */
    short duplexMode;                   /* Duplex printing mode */
    Boolean usePostScript;              /* Use PostScript output */
    Boolean downloadFonts;              /* Download fonts to printer */
};
typedef struct TAdvancedLayout TAdvancedLayout;

enum {
    kDuplexNone = 0,                    /* No duplex */
    kDuplexLongEdge = 1,                /* Long edge binding */
    kDuplexShortEdge = 2                /* Short edge binding */
};

OSErr SetAdvancedLayoutOptions(TPageLayoutPtr layout, TAdvancedLayout *advanced);
OSErr GetAdvancedLayoutOptions(TPageLayoutPtr layout, TAdvancedLayout *advanced);

#ifdef __cplusplus
}
#endif

#endif /* __PAGELAYOUT__ */