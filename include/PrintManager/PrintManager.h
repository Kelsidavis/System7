/*
 * PrintManager.h
 *
 * Main Print Manager API for System 7.1 Portable
 * Provides complete Mac OS Print Manager compatibility with modern printing support
 *
 * Based on Apple's Print Manager from Mac OS System 7.1
 */

#ifndef __PRINTMANAGER__
#define __PRINTMANAGER__

#include "Types.h"
#include "QuickDraw.h"
#include "Dialogs.h"
#include "PrintTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Print Manager Constants */
enum {
    iPFMaxPgs = 128,                    /* Maximum pages in print file */
    iPrPgFract = 120,                   /* Page scale factor. ptPgSize is in units of 1/iPrPgFract */
    iPrPgFst = 1,                       /* First page constant */
    iPrPgMax = 9999,                    /* Maximum page number */
    iPrRelease = 3,                     /* Current version number */
    iPrSavPFil = -1,                    /* Save print file flag */
    iPrAbort = 0x0080,                  /* Abort printing flag */
    iPrDevCtl = 7,                      /* PrDevCtl procedure control number */
    lPrReset = 0x00010000,              /* PrDevCtl reset parameter */
    lPrLineFeed = 0x00030000,           /* PrDevCtl line feed parameter */
    lPrLFStd = 0x0003FFFF,              /* Standard paper advance */
    lPrLFSixth = 0x0003FFFF,            /* 1/6 inch paper advance */
    lPrPageEnd = 0x00020000,            /* End page parameter */
    lPrDocOpen = 0x00010000,            /* Document open parameter */
    lPrPageOpen = 0x00040000,           /* Page open parameter */
    lPrPageClose = 0x00020000,          /* Page close parameter */
    lPrDocClose = 0x00050000,           /* Document close parameter */
    iFMgrCtl = 8,                       /* Font Manager tail-hook control */
    iMscCtl = 9,                        /* Miscellaneous control */
    iPvtCtl = 10                        /* Private control */
};

/* Error Constants */
enum {
    iMemFullErr = -108,                 /* Out of memory error */
    iIOAbort = -27,                     /* I/O abort error */
    pPrGlobals = 0x00000944,            /* Print variables low memory area */
    bDraftLoop = 0,                     /* Draft quality loop */
    bSpoolLoop = 1,                     /* Spool quality loop */
    bUser1Loop = 2,                     /* User loop 1 */
    bUser2Loop = 3,                     /* User loop 2 */
    fNewRunBit = 2,                     /* New run bit flag */
    fHiResOK = 3,                       /* High resolution OK flag */
    fWeOpenedRF = 4                     /* We opened resource file flag */
};

/* Driver Constants */
enum {
    iPrBitsCtl = 4,                     /* Bitmap control */
    lScreenBits = 0,                    /* Screen bitmap parameter */
    lPaintBits = 1,                     /* Paint bitmap parameter */
    lHiScreenBits = 0x00000002,         /* High resolution screen bitmap */
    lHiPaintBits = 0x00000003,          /* High resolution paint bitmap */
    iPrIOCtl = 5,                       /* I/O control */
    iPrEvtCtl = 6,                      /* Event control */
    lPrEvtAll = 0x0002FFFD,             /* Event control for entire screen */
    lPrEvtTop = 0x0001FFFD,             /* Event control for top folder */
    iPrDrvrRef = -3                     /* Print driver reference number */
};

/* Print Driver Operations */
enum {
    getRslDataOp = 4,                   /* Get resolution data operation */
    setRslOp = 5,                       /* Set resolution operation */
    draftBitsOp = 6,                    /* Draft bits operation */
    noDraftBitsOp = 7,                  /* No draft bits operation */
    getRotnOp = 8,                      /* Get rotation operation */
    NoSuchRsl = 1,                      /* No such resolution error */
    OpNotImpl = 2,                      /* Operation not implemented */
    RgType1 = 1                         /* Range type 1 */
};

/* Paper Feed Types */
enum {
    feedCut = 0,                        /* Cut sheet feed */
    feedFanfold = 1,                    /* Fanfold feed */
    feedMechCut = 2,                    /* Mechanical cut feed */
    feedOther = 3                       /* Other feed type */
};
typedef unsigned char TFeed;

/* Scan Directions */
enum {
    scanTB = 0,                         /* Top to bottom scan */
    scanBT = 1,                         /* Bottom to top scan */
    scanLR = 2,                         /* Left to right scan */
    scanRL = 3                          /* Right to left scan */
};
typedef unsigned char TScan;

/* State Constants */
enum {
    bPrDevOpen = 1,                     /* Device open state */
    bPrDocOpen = 2,                     /* Document open state */
    bPrPageOpen = 3,                    /* Page open state */
    bPrPrinting = 4,                    /* Printing state */
    bPrPageClose = 5,                   /* Page close state */
    bPrDocClose = 6,                    /* Document close state */
    bPrDevClose = 0                     /* Device close state */
};

/* Printer Types */
enum {
    bDevCItoh = 1,                      /* ImageWriter */
    bDevDaisy = 2,                      /* Daisy wheel printer */
    bDevLaser = 3                       /* LaserWriter */
};

/* Dialog Constants */
enum {
    iOK = 1,                            /* OK button */
    iCancel = 2,                        /* Cancel button */
    iPrStlDlg = 0xE000,                 /* Style dialog ID */
    iPrJobDlg = 0xE001,                 /* Job dialog ID */
    iPrCfgDlg = 0xE002,                 /* Configuration dialog ID */
    iPgFeedAx = 0xE00A,                 /* Page feed dialog ID */
    iPicSizAx = 0xE00B,                 /* Picture size alert ID */
    iIOAbrtAx = 0xE00C                  /* I/O abort alert ID */
};

/* Print Manager API Functions */

/* Core Print Manager Functions */
pascal void PrOpen(void);
pascal void PrClose(void);
pascal void PrintDefault(THPrint hPrint);
pascal Boolean PrValidate(THPrint hPrint);
pascal void PrPurge(void);
pascal void PrNoPurge(void);

/* Print Dialog Functions */
pascal Boolean PrStlDialog(THPrint hPrint);
pascal Boolean PrJobDialog(THPrint hPrint);
pascal TPPrDlg PrStlInit(THPrint hPrint);
pascal TPPrDlg PrJobInit(THPrint hPrint);
pascal Boolean PrDlgMain(THPrint hPrint, PDlgInitProcPtr pDlgInit);
pascal void PrJobMerge(THPrint hPrintSrc, THPrint hPrintDst);

/* Print Document Functions */
pascal TPPrPort PrOpenDoc(THPrint hPrint, TPPrPort pPrPort, Ptr pIOBuf);
pascal void PrCloseDoc(TPPrPort pPrPort);
pascal void PrOpenPage(TPPrPort pPrPort, TPRect pPageFrame);
pascal void PrClosePage(TPPrPort pPrPort);

/* Print File Functions */
pascal void PrPicFile(THPrint hPrint, TPPrPort pPrPort, Ptr pIOBuf,
                     Ptr pDevBuf, TPrStatus *prStatus);

/* Error Handling */
pascal short PrError(void);
pascal void PrSetError(short iErr);

/* Driver Interface Functions */
pascal void PrDrvrOpen(void);
pascal void PrDrvrClose(void);
pascal Handle PrDrvrDCE(void);
pascal short PrDrvrVers(void);
pascal void PrCtlCall(short iWhichCtl, long lParam1, long lParam2, long lParam3);

/* General Purpose Function */
pascal void PrGeneral(Ptr pData);

/* Print Manager Globals Access */
#define sPrDrvr ".Print"
extern short gPrError;                  /* Current print error */
extern Handle gPrDrvrDCE;              /* Print driver DCE handle */

/* Utility Functions (System7.1-Portable Extensions) */
OSErr InitPrintManager(void);
void CleanupPrintManager(void);
Boolean IsPrinterAvailable(void);
OSErr GetPrinterList(StringPtr printerNames[], short *count);
OSErr SetCurrentPrinter(StringPtr printerName);
OSErr GetCurrentPrinter(StringPtr printerName);

/* Modern Printing Extensions */
OSErr ConvertToPDF(THPrint hPrint, PicHandle hPic, FSSpec *pdfFile);
OSErr PrintToPDF(THPrint hPrint, PicHandle hPic, StringPtr fileName);
OSErr ShowPrintPreview(THPrint hPrint, PicHandle hPic, WindowPtr window);

#ifdef __cplusplus
}
#endif

#endif /* __PRINTMANAGER__ */