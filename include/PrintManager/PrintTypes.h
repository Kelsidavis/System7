/*
 * PrintTypes.h
 *
 * Print Manager data structures and types for System 7.1 Portable
 * Defines all print records, structures, and type definitions
 *
 * Based on Apple's Print Manager structures from Mac OS System 7.1
 */

#ifndef __PRINTTYPES__
#define __PRINTTYPES__

#include "Types.h"
#include "QuickDraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward Declarations */
typedef struct TPrint TPrint, *TPPrint, **THPrint;
typedef struct TPrPort TPrPort, *TPPrPort;
typedef struct TPrDlg TPrDlg, *TPPrDlg;

/* Print Manager Procedure Types */
typedef pascal void (*PrIdleProcPtr)(void);
typedef pascal void (*PItemProcPtr)(DialogPtr theDialog, short item);
typedef pascal TPPrDlg (*PDlgInitProcPtr)(THPrint hPrint);

/* Basic Types */
typedef Rect *TPRect;                   /* Pointer to a rectangle */

/* Print Information Record */
/* Contains the parameters needed for page composition */
struct TPrInfo {
    short iDev;                         /* Font manager/QuickDraw device code */
    short iVRes;                        /* Vertical resolution in device coordinates */
    short iHRes;                        /* Horizontal resolution in device coordinates */
    Rect rPage;                         /* Printable page rectangle in device coordinates */
};
typedef struct TPrInfo TPrInfo;
typedef TPrInfo *TPPrInfo;

/* Print Style Record */
/* Contains printer configuration and usage information */
struct TPrStl {
    short wDev;                         /* Driver number, Hi byte=RefNum, Lo byte=variant */
    short iPageV;                       /* Vertical page size */
    short iPageH;                       /* Horizontal page size */
    char bPort;                         /* I/O port number */
    TFeed feed;                         /* Paper feed type */
};
typedef struct TPrStl TPrStl;
typedef TPrStl *TPPrStl;

/* Print Extended Information Record */
/* Contains print-time extended information */
struct TPrXInfo {
    short iRowBytes;                    /* Band's row bytes */
    short iBandV;                       /* Vertical size of band in device coordinates */
    short iBandH;                       /* Horizontal size of band */
    short iDevBytes;                    /* Device bytes */
    short iBands;                       /* Number of bands per page */
    char bPatScale;                     /* Pattern scaling */
    char bUlThick;                      /* Underline thickness */
    char bUlOffset;                     /* Underline offset */
    char bUlShadow;                     /* Underline shadow */
    TScan scan;                         /* Band scan direction */
    char bXInfoX;                       /* Extra byte */
};
typedef struct TPrXInfo TPrXInfo;
typedef TPrXInfo *TPPrXInfo;

/* Print Job Record */
/* Contains print "form" for a single print request */
struct TPrJob {
    short iFstPage;                     /* First page in range */
    short iLstPage;                     /* Last page in range */
    short iCopies;                      /* Number of copies */
    char bJDocLoop;                     /* Document style: Draft, Spool, etc. */
    Boolean fFromUsr;                   /* Printing from user app (not PrApp) flag */
    PrIdleProcPtr pIdleProc;           /* Procedure called while waiting on I/O */
    StringPtr pFileName;                /* Spool file name: NIL for default */
    short iFileVol;                     /* Spool file volume, set to 0 initially */
    char bFileVers;                     /* Spool file version, set to 0 initially */
    char bJobX;                         /* Extra byte */
};
typedef struct TPrJob TPrJob;
typedef TPrJob *TPPrJob;

/* Universal Print Record */
/* The main 120-byte printing record */
struct TPrint {
    short iPrVersion;                   /* Printing software version (2 bytes) */
    TPrInfo prInfo;                     /* PrInfo data for current style (14 bytes) */
    Rect rPaper;                        /* Paper rectangle offset from rPage (8 bytes) */
    TPrStl prStl;                       /* Print request style (8 bytes) */
    TPrInfo prInfoPT;                   /* Print-time imaging metrics (14 bytes) */
    TPrXInfo prXInfo;                   /* Print-time expanded info (16 bytes) */
    TPrJob prJob;                       /* Print job request (20 bytes) */
    short printX[19];                   /* Spare bytes to fill to 120 bytes */
};

/* Print Status Record */
/* Contains print information during printing */
struct TPrStatus {
    short iTotPages;                    /* Total pages in print file */
    short iCurPage;                     /* Current page number */
    short iTotCopies;                   /* Total copies requested */
    short iCurCopy;                     /* Current copy number */
    short iTotBands;                    /* Total bands per page */
    short iCurBand;                     /* Current band number */
    Boolean fPgDirty;                   /* True if current page has been written to */
    Boolean fImaging;                   /* Set while in band's DrawPic call */
    THPrint hPrint;                     /* Handle to active printer record */
    TPPrPort pPrPort;                   /* Pointer to active PrPort */
    PicHandle hPic;                     /* Handle to active picture */
};
typedef struct TPrStatus TPrStatus;
typedef TPrStatus *TPPrStatus;

/* Print Port Record */
/* A GrafPort with additional printing information */
struct TPrPort {
    GrafPort gPort;                     /* The printer's GrafPort */
    QDProcs gProcs;                     /* Printer's QuickDraw procedures */
    long lGParam1;                      /* Private parameter storage (16 bytes total) */
    long lGParam2;
    long lGParam3;
    long lGParam4;
    Boolean fOurPtr;                    /* Whether PrPort allocation was done by us */
    Boolean fOurBits;                   /* Whether BitMap allocation was done by us */
};

/* Print File Page Directory */
struct TPfPgDir {
    short iPages;                       /* Number of pages */
    long iPgPos[129];                   /* Array of page positions (0..iPfMaxPgs) */
};
typedef struct TPfPgDir TPfPgDir;
typedef TPfPgDir *TPPfPgDir, **THPfPgDir;

/* Print Dialog Record */
/* Used for custom printing dialogs */
struct TPrDlg {
    DialogRecord Dlg;                   /* The dialog window */
    ModalFilterProcPtr pFltrProc;       /* The filter procedure */
    PItemProcPtr pItemProc;             /* The item evaluating procedure */
    THPrint hPrintUsr;                  /* The user's print record */
    Boolean fDoIt;                      /* Do it flag */
    Boolean fDone;                      /* Done flag */
    long lUser1;                        /* User data (4 longs for global data) */
    long lUser2;
    long lUser3;
    long lUser4;
};

/* General Data Block for PrGeneral */
struct TGnlData {
    short iOpCode;                      /* Operation code */
    short iError;                       /* Error code */
    long lReserved;                     /* Reserved field */
    /* More fields depending on operation */
};
typedef struct TGnlData TGnlData;

/* Resolution Range */
struct TRslRg {
    short iMin;                         /* Minimum resolution */
    short iMax;                         /* Maximum resolution */
};
typedef struct TRslRg TRslRg;

/* Resolution Record */
struct TRslRec {
    short iXRsl;                        /* X resolution */
    short iYRsl;                        /* Y resolution */
};
typedef struct TRslRec TRslRec;

/* Get Resolution Block */
struct TGetRslBlk {
    short iOpCode;                      /* Operation code */
    short iError;                       /* Error code */
    long lReserved;                     /* Reserved */
    short iRgType;                      /* Range type */
    TRslRg xRslRg;                      /* X resolution range */
    TRslRg yRslRg;                      /* Y resolution range */
    short iRslRecCnt;                   /* Resolution record count */
    TRslRec rgRslRec[27];              /* Array of resolution records */
};
typedef struct TGetRslBlk TGetRslBlk;

/* Set Resolution Block */
struct TSetRslBlk {
    short iOpCode;                      /* Operation code */
    short iError;                       /* Error code */
    long lReserved;                     /* Reserved */
    THPrint hPrint;                     /* Print record handle */
    short iXRsl;                        /* X resolution */
    short iYRsl;                        /* Y resolution */
};
typedef struct TSetRslBlk TSetRslBlk;

/* Draft Bits Block */
struct TDftBitsBlk {
    short iOpCode;                      /* Operation code */
    short iError;                       /* Error code */
    long lReserved;                     /* Reserved */
    THPrint hPrint;                     /* Print record handle */
};
typedef struct TDftBitsBlk TDftBitsBlk;

/* Get Rotation Block */
struct TGetRotnBlk {
    short iOpCode;                      /* Operation code */
    short iError;                       /* Error code */
    long lReserved;                     /* Reserved */
    THPrint hPrint;                     /* Print record handle */
    Boolean fLandscape;                 /* Landscape orientation flag */
    char bXtra;                         /* Extra byte */
};
typedef struct TGetRotnBlk TGetRotnBlk;

/* Print Manager Global Variables Structure */
struct TPrVars {
    short iPrErr;                       /* Current print error */
    char bDocLoop;                      /* Document style flags */
    char bUser1;                        /* User byte 1 */
    long lUser1;                        /* User long 1 */
    long lUser2;                        /* User long 2 */
    long lUser3;                        /* User long 3 */
    short iPrRefNum;                    /* Print driver resource file refnum */
};
typedef struct TPrVars TPrVars;

/* Modern Print Manager Extensions */
struct TPrintManagerState {
    Boolean initialized;                /* Print Manager initialized flag */
    short currentPrinter;              /* Current printer index */
    short printerCount;                /* Number of available printers */
    Handle printerList;                /* Handle to printer list */
    Boolean backgroundPrintingEnabled; /* Background printing enabled */
    long printJobID;                   /* Current print job ID */
};
typedef struct TPrintManagerState TPrintManagerState;

/* Print Queue Entry */
struct TPrintQueueEntry {
    long jobID;                        /* Unique job ID */
    StringPtr documentName;            /* Document name */
    StringPtr printerName;             /* Printer name */
    THPrint hPrint;                    /* Print record */
    PicHandle hPic;                    /* Picture to print */
    unsigned long timeSubmitted;       /* Time job was submitted */
    short status;                      /* Job status */
    short percentComplete;             /* Percentage complete */
    struct TPrintQueueEntry *next;     /* Next entry in queue */
};
typedef struct TPrintQueueEntry TPrintQueueEntry;
typedef TPrintQueueEntry *TPrintQueueEntryPtr;

/* Print Queue Status */
enum {
    kPrintJobPending = 0,              /* Job is pending */
    kPrintJobPrinting = 1,             /* Job is printing */
    kPrintJobCompleted = 2,            /* Job completed successfully */
    kPrintJobError = 3,                /* Job completed with error */
    kPrintJobCancelled = 4             /* Job was cancelled */
};

#ifdef __cplusplus
}
#endif

#endif /* __PRINTTYPES__ */