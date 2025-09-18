/*
 * PrintDialogs.h
 *
 * Print Dialog management for System 7.1 Portable
 * Handles page setup and print dialogs with modern UI integration
 *
 * Based on Apple's Print Manager dialogs from Mac OS System 7.1
 */

#ifndef __PRINTDIALOGS__
#define __PRINTDIALOGS__

#include "PrintTypes.h"
#include "Dialogs.h"
#include "Controls.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dialog Item Numbers */
enum {
    /* Page Setup Dialog Items */
    kPageSetupOK = 1,
    kPageSetupCancel = 2,
    kPageSetupPaperSize = 3,
    kPageSetupOrientation = 4,
    kPageSetupScale = 5,
    kPageSetupMargins = 6,

    /* Print Dialog Items */
    kPrintOK = 1,
    kPrintCancel = 2,
    kPrintCopies = 3,
    kPrintPageRange = 4,
    kPrintFromPage = 5,
    kPrintToPage = 6,
    kPrintQuality = 7,
    kPrintDestination = 8,
    kPrintBackground = 9,

    /* Common Items */
    kPrinterName = 10,
    kPrinterSetup = 11,
    kPreview = 12,
    kSaveAsFile = 13
};

/* Dialog Resource IDs */
enum {
    kPageSetupDialogID = -8192,         /* Page setup dialog resource ID */
    kPrintDialogID = -8191,             /* Print dialog resource ID */
    kPrinterSetupDialogID = -8190,      /* Printer setup dialog resource ID */
    kPrintStatusDialogID = -8189,       /* Print status dialog resource ID */
    kPrintPreviewDialogID = -8188       /* Print preview dialog resource ID */
};

/* Page Setup Dialog Functions */
Boolean ShowPageSetupDialog(THPrint hPrint);
Boolean CustomPageSetupDialog(THPrint hPrint, PDlgInitProcPtr customProc);
void InitPageSetupDialog(DialogPtr dialog, THPrint hPrint);
void UpdatePageSetupDialog(DialogPtr dialog, THPrint hPrint);
Boolean HandlePageSetupItem(DialogPtr dialog, short item, THPrint hPrint);

/* Print Dialog Functions */
Boolean ShowPrintDialog(THPrint hPrint);
Boolean CustomPrintDialog(THPrint hPrint, PDlgInitProcPtr customProc);
void InitPrintDialog(DialogPtr dialog, THPrint hPrint);
void UpdatePrintDialog(DialogPtr dialog, THPrint hPrint);
Boolean HandlePrintItem(DialogPtr dialog, short item, THPrint hPrint);

/* Dialog Utilities */
pascal Boolean PrintDialogFilter(DialogPtr dialog, EventRecord *event, short *itemHit);
pascal void PrintDialogHook(DialogPtr dialog, short item);
void DrawDialogPreview(DialogPtr dialog, short item, Rect *itemRect, THPrint hPrint);

/* Printer Setup Functions */
Boolean ShowPrinterSetupDialog(void);
OSErr GetAvailablePrinters(StringPtr printerList[], short *count);
OSErr SelectPrinter(StringPtr printerName);
Boolean ConfigurePrinter(StringPtr printerName);

/* Print Status Functions */
void ShowPrintStatusDialog(TPrStatus *status);
void UpdatePrintStatusDialog(TPrStatus *status);
void HidePrintStatusDialog(void);
Boolean IsPrintStatusDialogVisible(void);

/* Print Preview Functions */
Boolean ShowPrintPreviewDialog(THPrint hPrint, PicHandle hPic);
void DrawPreviewPage(WindowPtr window, THPrint hPrint, PicHandle hPic, short pageNum);
void UpdatePreviewControls(WindowPtr window, short currentPage, short totalPages);

/* Page Setup Options */
struct TPageSetupOptions {
    Boolean showMargins;                /* Show margin controls */
    Boolean showScale;                  /* Show scale control */
    Boolean showOrientation;            /* Show orientation controls */
    Boolean showPaperSize;              /* Show paper size popup */
    Boolean allowCustomPaper;           /* Allow custom paper sizes */
    Boolean showPreview;                /* Show preview area */
};
typedef struct TPageSetupOptions TPageSetupOptions;

/* Print Options */
struct TPrintOptions {
    Boolean showCopies;                 /* Show copies control */
    Boolean showPageRange;              /* Show page range controls */
    Boolean showQuality;                /* Show print quality options */
    Boolean showDestination;            /* Show destination options */
    Boolean showBackground;             /* Show background printing option */
    Boolean showPreview;                /* Show preview button */
    Boolean showSaveFile;               /* Show save to file option */
};
typedef struct TPrintOptions TPrintOptions;

/* Modern Dialog Extensions */
OSErr ShowModernPageSetupDialog(THPrint hPrint, TPageSetupOptions *options);
OSErr ShowModernPrintDialog(THPrint hPrint, TPrintOptions *options);
OSErr ShowPrintPreview(THPrint hPrint, PicHandle hPic, WindowPtr parentWindow);

/* Paper Size Management */
enum {
    kPaperLetter = 0,                   /* 8.5 x 11 inches */
    kPaperLegal = 1,                    /* 8.5 x 14 inches */
    kPaperA4 = 2,                       /* 210 x 297 mm */
    kPaperA3 = 3,                       /* 297 x 420 mm */
    kPaperTabloid = 4,                  /* 11 x 17 inches */
    kPaperCustom = 99                   /* Custom size */
};

struct TPaperSize {
    short paperType;                    /* Paper type constant */
    StringPtr name;                     /* Paper size name */
    short width;                        /* Width in points */
    short height;                       /* Height in points */
    Rect margins;                       /* Default margins */
};
typedef struct TPaperSize TPaperSize;

/* Print Quality Options */
enum {
    kQualityDraft = 0,                  /* Draft quality */
    kQualityNormal = 1,                 /* Normal quality */
    kQualityHigh = 2,                   /* High quality */
    kQualityPhoto = 3                   /* Photo quality */
};

/* Print Destination Options */
enum {
    kDestinationPrinter = 0,            /* Print to printer */
    kDestinationFile = 1,               /* Print to file */
    kDestinationFax = 2,                /* Print to fax */
    kDestinationEmail = 3               /* Print to email */
};

/* Dialog State Management */
struct TDialogState {
    DialogPtr dialog;                   /* Dialog pointer */
    THPrint hPrint;                     /* Print record handle */
    Boolean modified;                   /* Settings modified flag */
    short currentPage;                  /* Current preview page */
    PicHandle previewPic;              /* Preview picture */
    ControlHandle controls[20];         /* Dialog controls */
};
typedef struct TDialogState TDialogState;
typedef TDialogState *TDialogStatePtr;

/* Dialog Callbacks */
typedef pascal Boolean (*DialogFilterProcPtr)(DialogPtr dialog, EventRecord *event, short *itemHit);
typedef pascal void (*DialogItemProcPtr)(DialogPtr dialog, short item);
typedef pascal void (*PreviewDrawProcPtr)(DialogPtr dialog, short item, Rect *itemRect);

/* Dialog Management Functions */
TDialogStatePtr CreateDialogState(DialogPtr dialog, THPrint hPrint);
void DisposeDialogState(TDialogStatePtr state);
void SaveDialogSettings(TDialogStatePtr state);
void RestoreDialogSettings(TDialogStatePtr state);

/* Utility Functions */
void SetDialogItemValue(DialogPtr dialog, short item, long value);
long GetDialogItemValue(DialogPtr dialog, short item);
void SetDialogItemText(DialogPtr dialog, short item, StringPtr text);
void GetDialogItemText(DialogPtr dialog, short item, StringPtr text);
void EnableDialogItem(DialogPtr dialog, short item, Boolean enable);
Boolean IsDialogItemEnabled(DialogPtr dialog, short item);

/* Print Dialog Resources */
OSErr LoadPrintDialogResources(void);
void UnloadPrintDialogResources(void);
Handle GetDialogTemplate(short dialogID);
Handle GetDialogItemList(short dialogID);

#ifdef __cplusplus
}
#endif

#endif /* __PRINTDIALOGS__ */