/*
 * PrintPreview.h
 *
 * Print Preview functionality for System 7.1 Portable
 * Handles print preview display, pagination, and navigation
 *
 * Based on Apple's Print Manager preview from Mac OS System 7.1
 */

#ifndef __PRINTPREVIEW__
#define __PRINTPREVIEW__

#include "PrintTypes.h"
#include "Windows.h"
#include "Controls.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Preview Window Types */
enum {
    kPreviewWindowType = 1024,          /* Preview window type */
    kPreviewControlType = 1025          /* Preview control type */
};

/* Preview Display Modes */
enum {
    kPreviewModePage = 0,               /* Single page view */
    kPreviewModeSpread = 1,             /* Two page spread */
    kPreviewModeThumbnails = 2,         /* Thumbnail grid */
    kPreviewModeOverview = 3            /* Document overview */
};

/* Preview Zoom Levels */
enum {
    kZoomFitPage = -1,                  /* Fit page to window */
    kZoomFitWidth = -2,                 /* Fit width to window */
    kZoomActualSize = 100,              /* 100% zoom */
    kZoomMinimum = 25,                  /* Minimum zoom */
    kZoomMaximum = 400                  /* Maximum zoom */
};

/* Preview Navigation */
enum {
    kNavFirstPage = 1,                  /* Go to first page */
    kNavPreviousPage = 2,               /* Go to previous page */
    kNavNextPage = 3,                   /* Go to next page */
    kNavLastPage = 4,                   /* Go to last page */
    kNavGoToPage = 5                    /* Go to specific page */
};

/* Preview Control IDs */
enum {
    kPreviewPageControl = 1,            /* Page navigation control */
    kPreviewZoomControl = 2,            /* Zoom control */
    kPreviewModeControl = 3,            /* Display mode control */
    kPreviewPrintButton = 4,            /* Print button */
    kPreviewCloseButton = 5,            /* Close button */
    kPreviewPageField = 6,              /* Page number field */
    kPreviewZoomField = 7,              /* Zoom percentage field */
    kPreviewStatusText = 8              /* Status text */
};

/* Preview Window State */
struct TPreviewWindow {
    WindowPtr window;                   /* Preview window */
    THPrint hPrint;                     /* Print record */
    PicHandle hPicture;                 /* Document picture */
    short currentPage;                  /* Current page number */
    short totalPages;                   /* Total number of pages */
    short displayMode;                  /* Display mode */
    short zoomLevel;                    /* Zoom percentage */
    Boolean showMargins;                /* Show margin guides */
    Boolean showRulers;                 /* Show rulers */
    Boolean showGrid;                   /* Show grid */
    Rect contentRect;                   /* Content display area */
    Rect pageRect;                      /* Current page rectangle */
    RgnHandle updateRgn;               /* Update region */
    ControlHandle controls[16];         /* Window controls */
    Handle pageData;                    /* Page rendering data */
    long lastUpdateTime;                /* Last update time */
};
typedef struct TPreviewWindow TPreviewWindow;
typedef TPreviewWindow *TPreviewWindowPtr;

/* Preview Page Info */
struct TPreviewPageInfo {
    short pageNumber;                   /* Page number */
    Rect pageRect;                      /* Page rectangle */
    Rect printRect;                     /* Printable rectangle */
    Rect contentRect;                   /* Content rectangle */
    Boolean hasContent;                 /* Page has content */
    long renderTime;                    /* Rendering time */
    Handle pageData;                    /* Page-specific data */
};
typedef struct TPreviewPageInfo TPreviewPageInfo;
typedef TPreviewPageInfo *TPreviewPageInfoPtr;

/* Preview Functions */

/* Window Management */
OSErr CreatePreviewWindow(THPrint hPrint, PicHandle hPicture, TPreviewWindowPtr *preview);
OSErr ShowPreviewWindow(TPreviewWindowPtr preview);
OSErr HidePreviewWindow(TPreviewWindowPtr preview);
OSErr DisposePreviewWindow(TPreviewWindowPtr preview);
OSErr UpdatePreviewWindow(TPreviewWindowPtr preview);

/* Navigation */
OSErr SetPreviewPage(TPreviewWindowPtr preview, short pageNumber);
OSErr NextPreviewPage(TPreviewWindowPtr preview);
OSErr PreviousPreviewPage(TPreviewWindowPtr preview);
OSErr FirstPreviewPage(TPreviewWindowPtr preview);
OSErr LastPreviewPage(TPreviewWindowPtr preview);

/* Display Control */
OSErr SetPreviewZoom(TPreviewWindowPtr preview, short zoomLevel);
OSErr SetPreviewDisplayMode(TPreviewWindowPtr preview, short displayMode);
OSErr FitPreviewToWindow(TPreviewWindowPtr preview);
OSErr SetPreviewOptions(TPreviewWindowPtr preview, Boolean showMargins,
                       Boolean showRulers, Boolean showGrid);

/* Page Rendering */
OSErr RenderPreviewPage(TPreviewWindowPtr preview, short pageNumber);
OSErr DrawPreviewPage(TPreviewWindowPtr preview, short pageNumber, Rect *drawRect);
OSErr InvalidatePreviewPage(TPreviewWindowPtr preview, short pageNumber);
OSErr RefreshPreviewDisplay(TPreviewWindowPtr preview);

/* Event Handling */
Boolean HandlePreviewEvent(TPreviewWindowPtr preview, EventRecord *event);
Boolean HandlePreviewClick(TPreviewWindowPtr preview, Point where, short modifiers);
Boolean HandlePreviewKey(TPreviewWindowPtr preview, char key, short modifiers);
void HandlePreviewUpdate(TPreviewWindowPtr preview);
void HandlePreviewActivate(TPreviewWindowPtr preview, Boolean activate);

/* Control Handling */
void HandlePreviewControlClick(TPreviewWindowPtr preview, ControlHandle control, Point where);
void UpdatePreviewControls(TPreviewWindowPtr preview);
void EnablePreviewControl(TPreviewWindowPtr preview, short controlID, Boolean enable);
void SetPreviewControlValue(TPreviewWindowPtr preview, short controlID, short value);
short GetPreviewControlValue(TPreviewWindowPtr preview, short controlID);

/* Page Information */
OSErr GetPreviewPageCount(TPreviewWindowPtr preview, short *pageCount);
OSErr GetPreviewPageInfo(TPreviewWindowPtr preview, short pageNumber, TPreviewPageInfoPtr info);
OSErr GetPreviewCurrentPage(TPreviewWindowPtr preview, short *pageNumber);
OSErr SetPreviewPageRange(TPreviewWindowPtr preview, short firstPage, short lastPage);

/* Printing from Preview */
OSErr PrintFromPreview(TPreviewWindowPtr preview);
OSErr PrintPreviewPage(TPreviewWindowPtr preview, short pageNumber);
OSErr PrintPreviewRange(TPreviewWindowPtr preview, short firstPage, short lastPage);

/* Export Functions */
OSErr ExportPreviewToPICT(TPreviewWindowPtr preview, short pageNumber, PicHandle *picture);
OSErr ExportPreviewToPDF(TPreviewWindowPtr preview, FSSpec *file);
OSErr SavePreviewAsImage(TPreviewWindowPtr preview, short pageNumber, FSSpec *file, OSType format);

/* Utility Functions */
OSErr CalculatePreviewLayout(TPreviewWindowPtr preview);
OSErr GetPreviewDisplayRect(TPreviewWindowPtr preview, Rect *displayRect);
OSErr ConvertPreviewCoordinates(TPreviewWindowPtr preview, Point *point, Boolean toPage);
OSErr GetPreviewScrollPosition(TPreviewWindowPtr preview, Point *position);
OSErr SetPreviewScrollPosition(TPreviewWindowPtr preview, Point position);

/* Advanced Preview Features */
struct TPreviewSearchInfo {
    StringPtr searchText;               /* Text to search for */
    short startPage;                    /* Starting page */
    short currentPage;                  /* Current match page */
    short matchCount;                   /* Number of matches */
    Boolean caseSensitive;              /* Case sensitive search */
    Boolean wholeWord;                  /* Whole word search */
    Rect matchRects[32];               /* Match rectangles */
};
typedef struct TPreviewSearchInfo TPreviewSearchInfo;

OSErr SearchPreviewText(TPreviewWindowPtr preview, TPreviewSearchInfo *searchInfo);
OSErr FindNextPreviewMatch(TPreviewWindowPtr preview, TPreviewSearchInfo *searchInfo);
OSErr HighlightPreviewMatch(TPreviewWindowPtr preview, Rect *matchRect);

/* Annotation Support */
struct TPreviewAnnotation {
    short annotationType;               /* Annotation type */
    Rect bounds;                        /* Annotation bounds */
    StringPtr text;                     /* Annotation text */
    RGBColor color;                     /* Annotation color */
    short pageNumber;                   /* Associated page */
    Handle data;                        /* Annotation data */
};
typedef struct TPreviewAnnotation TPreviewAnnotation;

enum {
    kAnnotationNote = 1,                /* Text note */
    kAnnotationHighlight = 2,           /* Highlight */
    kAnnotationStamp = 3,               /* Stamp */
    kAnnotationDrawing = 4              /* Drawing */
};

OSErr AddPreviewAnnotation(TPreviewWindowPtr preview, TPreviewAnnotation *annotation);
OSErr RemovePreviewAnnotation(TPreviewWindowPtr preview, short annotationID);
OSErr GetPreviewAnnotations(TPreviewWindowPtr preview, TPreviewAnnotation annotations[], short *count);
OSErr DrawPreviewAnnotations(TPreviewWindowPtr preview, short pageNumber);

/* Multiple Document Support */
struct TPreviewDocumentList {
    short documentCount;                /* Number of documents */
    TPreviewWindowPtr documents[8];     /* Document windows */
    short activeDocument;               /* Active document index */
};
typedef struct TPreviewDocumentList TPreviewDocumentList;

OSErr CreatePreviewDocumentList(TPreviewDocumentList **docList);
OSErr AddPreviewDocument(TPreviewDocumentList *docList, TPreviewWindowPtr preview);
OSErr RemovePreviewDocument(TPreviewDocumentList *docList, TPreviewWindowPtr preview);
OSErr SetActivePreviewDocument(TPreviewDocumentList *docList, short documentIndex);

/* Configuration */
struct TPreviewPreferences {
    short defaultZoom;                  /* Default zoom level */
    short defaultMode;                  /* Default display mode */
    Boolean showMargins;                /* Show margins by default */
    Boolean showRulers;                 /* Show rulers by default */
    Boolean showGrid;                   /* Show grid by default */
    Boolean antiAlias;                  /* Use anti-aliasing */
    RGBColor backgroundColor;           /* Background color */
    RGBColor paperColor;                /* Paper color */
    short cachePages;                   /* Number of pages to cache */
};
typedef struct TPreviewPreferences TPreviewPreferences;

OSErr LoadPreviewPreferences(TPreviewPreferences *prefs);
OSErr SavePreviewPreferences(TPreviewPreferences *prefs);
OSErr SetPreviewPreferences(TPreviewWindowPtr preview, TPreviewPreferences *prefs);

/* Callback Functions */
typedef pascal void (*PreviewUpdateProcPtr)(TPreviewWindowPtr preview, short pageNumber);
typedef pascal Boolean (*PreviewEventProcPtr)(TPreviewWindowPtr preview, EventRecord *event);
typedef pascal void (*PreviewDrawProcPtr)(TPreviewWindowPtr preview, short pageNumber, Rect *drawRect);

OSErr SetPreviewCallbacks(TPreviewWindowPtr preview, PreviewUpdateProcPtr updateProc,
                         PreviewEventProcPtr eventProc, PreviewDrawProcPtr drawProc);

#ifdef __cplusplus
}
#endif

#endif /* __PRINTPREVIEW__ */