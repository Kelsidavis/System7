/*
 * PrintManager.h - System 7.1 Print Manager Interface
 *
 * This file provides the Print Manager interface for System 7.1 applications.
 * The Print Manager provides a standard interface for printing documents
 * through various printer drivers including PostScript printers.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#ifndef PRINTMANAGER_H
#define PRINTMANAGER_H

#include "../DeskManager/Types.h"
#include "../QuickDraw/QuickDraw.h"
#include "../DialogManager/Dialogs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Print Manager Constants */
#define PRINT_DRAFT_LOOP    0
#define PRINT_SPOOL_LOOP    1
#define PRINT_USER1_LOOP    2
#define PRINT_USER2_LOOP    3

/* Paper feed constants */
#define FEED_CUT_SHEET      0
#define FEED_FANFOLD        1
#define FEED_MECH_CUT       2
#define FEED_OTHER          3

/* Print orientation constants */
#define PRINT_PORTRAIT      0
#define PRINT_LANDSCAPE     1

/* Print Manager error codes */
enum {
    noErr = 0,
    iPrAbort = 128,
    iPrSavPFil = -1,
    controlErr = -17,
    statusErr = -18,
    readErr = -19,
    writErr = -20,
    badUnitErr = -21,
    unitEmptyErr = -22,
    openErr = -23,
    closErr = -24,
    dRemovErr = -25,
    dInstErr = -26,
    abortErr = -27,
    iIOAbort = -28,
    notActiveErr = -29,
    noSuchRsl = -194,
    badPrintRec = -195
};

/* Print Information Record */
typedef struct TPrInfo {
    short iDev;              /* Device type identifier */
    short iVRes;             /* Vertical resolution (dpi) */
    short iHRes;             /* Horizontal resolution (dpi) */
    Rect rPage;              /* Page rectangle */
} TPrInfo;

/* Print Style Record */
typedef struct TPrStl {
    short wDev;              /* Device number */
    short iPageV;            /* Vertical page scaling */
    short iPageH;            /* Horizontal page scaling */
    signed char bPort;       /* Orientation */
    signed char feed;        /* Paper feed method */
} TPrStl;

/* Print Job Record */
typedef struct TPrJob {
    short iFstPage;          /* First page to print */
    short iLstPage;          /* Last page to print */
    short iCopies;           /* Number of copies */
    signed char bJDocLoop;   /* Document loop method */
    Boolean fFromUsr;        /* User specified settings */
    ProcPtr pIdleProc;       /* Idle procedure */
    Ptr pFileName;           /* File name for spooling */
    short iFileVol;          /* Volume reference */
    signed char bFileVers;   /* File version */
    signed char bJobX;       /* Job extension */
} TPrJob;

/* Extended Print Information */
typedef struct TPrXInfo {
    short iRowBytes;         /* Row bytes for bitmap */
    short iBandV;            /* Vertical band size */
    short iBandH;            /* Horizontal band size */
    short iDevBytes;         /* Device bytes per band */
    short iBands;            /* Number of bands */
    signed char bPatScale;   /* Pattern scale factor */
    signed char bUlThick;    /* Underline thickness */
    signed char bUlOffset;   /* Underline offset */
    signed char bUlShadow;   /* Underline shadow */
    signed char scan;        /* Scan direction */
    signed char bXInfoX;     /* Extended info byte */
} TPrXInfo;

/* Print Status Record */
typedef struct TPrStatus {
    short iTotPages;         /* Total pages */
    short iCurPage;          /* Current page */
    short iTotCopies;        /* Total copies */
    short iCurCopy;          /* Current copy */
    short iTotBands;         /* Total bands */
    short iCurBand;          /* Current band */
    Boolean fPgDirty;        /* Page has been marked */
    Boolean fImaging;        /* Currently imaging */
} TPrStatus;

/* Complete Print Record */
typedef struct TPrint {
    short iPrVersion;        /* Print record version */
    TPrInfo prInfo;          /* Print information */
    Rect rPaper;             /* Paper rectangle */
    TPrStl prStl;            /* Print style */
    TPrInfo prInfoPT;        /* PostScript print info */
    TPrXInfo prXInfo;        /* Extended print info */
    TPrJob prJob;            /* Print job settings */
    short printX[19];        /* Extended settings */
} TPrint, *TPPrint, **THPrint;

/* Type definitions */
typedef GrafPtr TPPrPort;
typedef Rect* TPRect;

/* Print Manager Functions */
void PrOpen(void);
void PrClose(void);
TPPrPort PrOpenDoc(THPrint hPrint, TPPrPort pPrPort, Ptr pIOBuf);
void PrCloseDoc(TPPrPort pPrPort);
void PrOpenPage(TPPrPort pPrPort, TPRect pPageFrame);
void PrClosePage(TPPrPort pPrPort);
void PrPicFile(THPrint hPrint, TPPrPort pPrPort, Ptr pIOBuf,
               Ptr pDevBuf, TPrStatus* prStatus);
Boolean PrJobDialog(THPrint hPrint);
Boolean PrStlDialog(THPrint hPrint);
void PrJobMerge(THPrint hPrintSrc, THPrint hPrintDst);
Boolean PrValidate(THPrint hPrint);

/* Utility functions */
THPrint PrNewPrintRecord(void);
void PrDisposePrintRecord(THPrint hPrint);
void PrDefault(THPrint hPrint);
OSErr PrError(void);
void PrSetError(OSErr err);
Boolean PrGetDocInfo(const StringPtr docName, short vRefNum);
void PrGeneral(Ptr pData);
void PrDrvrOpen(void);
void PrDrvrClose(void);
Boolean PrDlgMain(THPrint hPrint, ProcPtr pDlgInit);
TPPrPort PrOpenPictFile(short vRefNum, short dirID,
                        const StringPtr fileName);
void PrClosePictFile(TPPrPort pPrPort);

#ifdef __cplusplus
}
#endif

#endif /* PRINTMANAGER_H */