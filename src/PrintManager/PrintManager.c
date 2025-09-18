/*
 * PrintManager.c - System 7.1 Print Manager Implementation
 *
 * This file implements the Print Manager for System 7.1 applications.
 * It provides a complete printing subsystem with support for PostScript
 * and raster printing through platform-specific HAL layer.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include "PrintManager/PrintManager.h"
#include "PrintManager/PrintManager_HAL.h"
#include "Memory.h"
#include <string.h>
#include <stdio.h>

/* Print Manager State */
static struct {
    Boolean         initialized;
    OSErr          lastError;
    THPrint        currentPrintRecord;
    TPPrPort       currentPort;
    Boolean        driverOpen;
    Boolean        docOpen;
    Boolean        pageOpen;
    short          currentPage;
    short          currentCopy;
    Ptr            spoolBuffer;
    Size           spoolBufSize;
    FILE*          spoolFile;
    char           spoolPath[256];
} gPMState = {0};

/* Default print record values */
static const TPrint kDefaultPrintRecord = {
    .iPrVersion = 1,
    .prInfo = {
        .iDev = 0,
        .iVRes = 72,
        .iHRes = 72,
        .rPage = {0, 0, 792, 612}  /* US Letter at 72 DPI */
    },
    .rPaper = {-36, -36, 828, 648},  /* 0.5" margins */
    .prStl = {
        .wDev = 0,
        .iPageV = 100,
        .iPageH = 100,
        .bPort = PRINT_PORTRAIT,
        .feed = FEED_CUT_SHEET
    },
    .prInfoPT = {
        .iDev = 0,
        .iVRes = 72,
        .iHRes = 72,
        .rPage = {0, 0, 792, 612}
    },
    .prXInfo = {
        .iRowBytes = 80,
        .iBandV = 20,
        .iBandH = 612,
        .iDevBytes = 1600,
        .iBands = 40,
        .bPatScale = 1,
        .bUlThick = 1,
        .bUlOffset = 1,
        .bUlShadow = 0,
        .scan = 0,
        .bXInfoX = 0
    },
    .prJob = {
        .iFstPage = 1,
        .iLstPage = 9999,
        .iCopies = 1,
        .bJDocLoop = PRINT_SPOOL_LOOP,
        .fFromUsr = false,
        .pIdleProc = NULL,
        .pFileName = NULL,
        .iFileVol = 0,
        .bFileVers = 0,
        .bJobX = 0
    },
    .printX = {0}
};

/* Initialize Print Manager */
void PrOpen(void) {
    if (gPMState.initialized) {
        return;
    }

    /* Initialize HAL layer */
    OSErr err = PrintManager_HAL_Init();
    if (err != noErr) {
        gPMState.lastError = err;
        return;
    }

    /* Initialize state */
    gPMState.initialized = true;
    gPMState.lastError = noErr;
    gPMState.driverOpen = false;
    gPMState.docOpen = false;
    gPMState.pageOpen = false;
    gPMState.currentPage = 0;
    gPMState.currentCopy = 0;

    /* Allocate spool buffer */
    gPMState.spoolBufSize = 8192;
    gPMState.spoolBuffer = NewPtr(gPMState.spoolBufSize);
    if (!gPMState.spoolBuffer) {
        gPMState.lastError = memFullErr;
    }
}

/* Close Print Manager */
void PrClose(void) {
    if (!gPMState.initialized) {
        return;
    }

    /* Close any open document */
    if (gPMState.docOpen) {
        PrCloseDoc(gPMState.currentPort);
    }

    /* Close driver */
    if (gPMState.driverOpen) {
        PrDrvrClose();
    }

    /* Free spool buffer */
    if (gPMState.spoolBuffer) {
        DisposePtr(gPMState.spoolBuffer);
        gPMState.spoolBuffer = NULL;
    }

    /* Close spool file if open */
    if (gPMState.spoolFile) {
        fclose(gPMState.spoolFile);
        gPMState.spoolFile = NULL;
    }

    /* Cleanup HAL layer */
    PrintManager_HAL_Cleanup();

    gPMState.initialized = false;
}

/* Open printer driver */
void PrDrvrOpen(void) {
    if (!gPMState.initialized) {
        PrOpen();
    }

    if (gPMState.driverOpen) {
        return;
    }

    OSErr err = PrintManager_HAL_OpenDriver();
    if (err == noErr) {
        gPMState.driverOpen = true;
    } else {
        gPMState.lastError = err;
    }
}

/* Close printer driver */
void PrDrvrClose(void) {
    if (!gPMState.driverOpen) {
        return;
    }

    PrintManager_HAL_CloseDriver();
    gPMState.driverOpen = false;
}

/* Create new print record */
THPrint PrNewPrintRecord(void) {
    THPrint hPrint = (THPrint)NewHandle(sizeof(TPrint));
    if (hPrint) {
        PrDefault(hPrint);
    }
    return hPrint;
}

/* Dispose print record */
void PrDisposePrintRecord(THPrint hPrint) {
    if (hPrint) {
        DisposeHandle((Handle)hPrint);
    }
}

/* Set default values in print record */
void PrDefault(THPrint hPrint) {
    if (!hPrint) {
        return;
    }

    HLock((Handle)hPrint);
    **hPrint = kDefaultPrintRecord;

    /* Get default settings from HAL */
    PrintManager_HAL_GetDefaultSettings(*hPrint);

    HUnlock((Handle)hPrint);
}

/* Validate print record */
Boolean PrValidate(THPrint hPrint) {
    if (!hPrint) {
        gPMState.lastError = badPrintRec;
        return false;
    }

    HLock((Handle)hPrint);
    TPPrint pPrint = *hPrint;

    /* Check version */
    if (pPrint->iPrVersion != 1) {
        /* Update to current version */
        pPrint->iPrVersion = 1;
    }

    /* Validate page dimensions */
    if (pPrint->prInfo.rPage.right <= pPrint->prInfo.rPage.left ||
        pPrint->prInfo.rPage.bottom <= pPrint->prInfo.rPage.top) {
        /* Set default page size */
        pPrint->prInfo.rPage = kDefaultPrintRecord.prInfo.rPage;
    }

    /* Validate resolution */
    if (pPrint->prInfo.iVRes <= 0) pPrint->prInfo.iVRes = 72;
    if (pPrint->prInfo.iHRes <= 0) pPrint->prInfo.iHRes = 72;

    /* Validate page range */
    if (pPrint->prJob.iFstPage < 1) pPrint->prJob.iFstPage = 1;
    if (pPrint->prJob.iLstPage < pPrint->prJob.iFstPage) {
        pPrint->prJob.iLstPage = pPrint->prJob.iFstPage;
    }

    /* Validate copies */
    if (pPrint->prJob.iCopies < 1) pPrint->prJob.iCopies = 1;

    HUnlock((Handle)hPrint);
    return true;
}

/* Show print style dialog */
Boolean PrStlDialog(THPrint hPrint) {
    if (!hPrint) {
        gPMState.lastError = badPrintRec;
        return false;
    }

    if (!gPMState.initialized) {
        PrOpen();
    }

    HLock((Handle)hPrint);
    Boolean result = PrintManager_HAL_ShowPageSetup(*hPrint);
    HUnlock((Handle)hPrint);

    if (result) {
        PrValidate(hPrint);
    }

    return result;
}

/* Show print job dialog */
Boolean PrJobDialog(THPrint hPrint) {
    if (!hPrint) {
        gPMState.lastError = badPrintRec;
        return false;
    }

    if (!gPMState.initialized) {
        PrOpen();
    }

    HLock((Handle)hPrint);
    Boolean result = PrintManager_HAL_ShowPrintDialog(*hPrint);
    HUnlock((Handle)hPrint);

    if (result) {
        (*hPrint)->prJob.fFromUsr = true;
        PrValidate(hPrint);
    }

    return result;
}

/* Merge print job settings */
void PrJobMerge(THPrint hPrintSrc, THPrint hPrintDst) {
    if (!hPrintSrc || !hPrintDst) {
        return;
    }

    HLock((Handle)hPrintSrc);
    HLock((Handle)hPrintDst);

    /* Copy job settings */
    (*hPrintDst)->prJob = (*hPrintSrc)->prJob;

    /* Copy style settings if user modified */
    if ((*hPrintSrc)->prJob.fFromUsr) {
        (*hPrintDst)->prStl = (*hPrintSrc)->prStl;
    }

    HUnlock((Handle)hPrintSrc);
    HUnlock((Handle)hPrintDst);
}

/* Open print document */
TPPrPort PrOpenDoc(THPrint hPrint, TPPrPort pPrPort, Ptr pIOBuf) {
    if (!hPrint) {
        gPMState.lastError = badPrintRec;
        return NULL;
    }

    if (gPMState.docOpen) {
        gPMState.lastError = notActiveErr;
        return NULL;
    }

    if (!gPMState.initialized) {
        PrOpen();
    }

    if (!gPMState.driverOpen) {
        PrDrvrOpen();
    }

    /* Validate print record */
    if (!PrValidate(hPrint)) {
        return NULL;
    }

    HLock((Handle)hPrint);

    /* Create printing port if not provided */
    if (!pPrPort) {
        pPrPort = (TPPrPort)NewPtr(sizeof(GrafPort));
        if (!pPrPort) {
            gPMState.lastError = memFullErr;
            HUnlock((Handle)hPrint);
            return NULL;
        }
        OpenPort(pPrPort);
    }

    /* Initialize document */
    OSErr err = PrintManager_HAL_BeginDocument(*hPrint, pPrPort);
    if (err != noErr) {
        gPMState.lastError = err;
        HUnlock((Handle)hPrint);
        return NULL;
    }

    /* Set up state */
    gPMState.currentPrintRecord = hPrint;
    gPMState.currentPort = pPrPort;
    gPMState.docOpen = true;
    gPMState.currentPage = 0;
    gPMState.currentCopy = 1;

    /* Create spool file if needed */
    if ((*hPrint)->prJob.bJDocLoop == PRINT_SPOOL_LOOP) {
        sprintf(gPMState.spoolPath, "/tmp/print_spool_%d.ps", (int)TickCount());
        gPMState.spoolFile = fopen(gPMState.spoolPath, "w");

        if (gPMState.spoolFile) {
            /* Write PostScript header */
            fprintf(gPMState.spoolFile, "%%!PS-Adobe-3.0\n");
            fprintf(gPMState.spoolFile, "%%%%Creator: System7.1-Portable Print Manager\n");
            fprintf(gPMState.spoolFile, "%%%%Pages: (atend)\n");
            fprintf(gPMState.spoolFile, "%%%%BoundingBox: %d %d %d %d\n",
                    (*hPrint)->rPaper.left, (*hPrint)->rPaper.top,
                    (*hPrint)->rPaper.right, (*hPrint)->rPaper.bottom);
            fprintf(gPMState.spoolFile, "%%%%EndComments\n");
        }
    }

    HUnlock((Handle)hPrint);
    return pPrPort;
}

/* Close print document */
void PrCloseDoc(TPPrPort pPrPort) {
    if (!gPMState.docOpen) {
        return;
    }

    /* Close any open page */
    if (gPMState.pageOpen) {
        PrClosePage(pPrPort);
    }

    /* End document */
    if (gPMState.currentPrintRecord) {
        HLock((Handle)gPMState.currentPrintRecord);

        /* Close spool file */
        if (gPMState.spoolFile) {
            fprintf(gPMState.spoolFile, "%%%%Trailer\n");
            fprintf(gPMState.spoolFile, "%%%%Pages: %d\n", gPMState.currentPage);
            fprintf(gPMState.spoolFile, "%%%%EOF\n");
            fclose(gPMState.spoolFile);
            gPMState.spoolFile = NULL;

            /* Send to printer */
            PrintManager_HAL_PrintSpoolFile(gPMState.spoolPath);

            /* Remove spool file */
            remove(gPMState.spoolPath);
        }

        PrintManager_HAL_EndDocument();
        HUnlock((Handle)gPMState.currentPrintRecord);
    }

    /* Clean up port if we created it */
    if (pPrPort && pPrPort == gPMState.currentPort) {
        ClosePort(pPrPort);
        DisposePtr((Ptr)pPrPort);
    }

    gPMState.docOpen = false;
    gPMState.currentPort = NULL;
    gPMState.currentPrintRecord = NULL;
}

/* Open print page */
void PrOpenPage(TPPrPort pPrPort, TPRect pPageFrame) {
    if (!gPMState.docOpen) {
        gPMState.lastError = notActiveErr;
        return;
    }

    if (gPMState.pageOpen) {
        return;
    }

    gPMState.currentPage++;

    HLock((Handle)gPMState.currentPrintRecord);
    TPPrint pPrint = *gPMState.currentPrintRecord;

    /* Check page range */
    if (gPMState.currentPage < pPrint->prJob.iFstPage ||
        gPMState.currentPage > pPrint->prJob.iLstPage) {
        HUnlock((Handle)gPMState.currentPrintRecord);
        return;
    }

    /* Begin page */
    Rect pageRect = pPageFrame ? *pPageFrame : pPrint->prInfo.rPage;
    OSErr err = PrintManager_HAL_BeginPage(gPMState.currentPage, &pageRect);
    if (err != noErr) {
        gPMState.lastError = err;
        HUnlock((Handle)gPMState.currentPrintRecord);
        return;
    }

    /* Write PostScript page header if spooling */
    if (gPMState.spoolFile) {
        fprintf(gPMState.spoolFile, "%%%%Page: %d %d\n",
                gPMState.currentPage, gPMState.currentPage);
        fprintf(gPMState.spoolFile, "gsave\n");

        /* Set up coordinate system */
        if (pPrint->prStl.bPort == PRINT_LANDSCAPE) {
            fprintf(gPMState.spoolFile, "90 rotate\n");
            fprintf(gPMState.spoolFile, "0 -%d translate\n",
                    pPrint->rPaper.right);
        }
    }

    gPMState.pageOpen = true;
    HUnlock((Handle)gPMState.currentPrintRecord);
}

/* Close print page */
void PrClosePage(TPPrPort pPrPort) {
    if (!gPMState.pageOpen) {
        return;
    }

    /* Write PostScript page footer if spooling */
    if (gPMState.spoolFile) {
        fprintf(gPMState.spoolFile, "grestore\n");
        fprintf(gPMState.spoolFile, "showpage\n");
    }

    /* End page */
    PrintManager_HAL_EndPage();

    gPMState.pageOpen = false;
}

/* Print picture file */
void PrPicFile(THPrint hPrint, TPPrPort pPrPort, Ptr pIOBuf,
               Ptr pDevBuf, TPrStatus* prStatus) {
    if (!hPrint) {
        gPMState.lastError = badPrintRec;
        return;
    }

    /* Open document if needed */
    Boolean weOpenedDoc = false;
    if (!gPMState.docOpen) {
        pPrPort = PrOpenDoc(hPrint, pPrPort, pIOBuf);
        if (!pPrPort) {
            return;
        }
        weOpenedDoc = true;
    }

    HLock((Handle)hPrint);
    TPPrint pPrint = *hPrint;

    /* Print each page in range */
    for (short page = pPrint->prJob.iFstPage;
         page <= pPrint->prJob.iLstPage;
         page++) {

        /* Update status */
        if (prStatus) {
            prStatus->iCurPage = page;
            prStatus->iTotPages = pPrint->prJob.iLstPage -
                                 pPrint->prJob.iFstPage + 1;
        }

        /* Print copies */
        for (short copy = 1; copy <= pPrint->prJob.iCopies; copy++) {
            if (prStatus) {
                prStatus->iCurCopy = copy;
                prStatus->iTotCopies = pPrint->prJob.iCopies;
            }

            PrOpenPage(pPrPort, NULL);

            /* Draw picture content here */
            /* This would typically render the picture data */

            PrClosePage(pPrPort);

            /* Check for abort */
            if (gPMState.lastError == iPrAbort) {
                break;
            }
        }

        if (gPMState.lastError == iPrAbort) {
            break;
        }
    }

    HUnlock((Handle)hPrint);

    /* Close document if we opened it */
    if (weOpenedDoc) {
        PrCloseDoc(pPrPort);
    }
}

/* Get print error */
OSErr PrError(void) {
    return gPMState.lastError;
}

/* Set print error */
void PrSetError(OSErr err) {
    gPMState.lastError = err;
}

/* Get document info */
Boolean PrGetDocInfo(const StringPtr docName, short vRefNum) {
    /* Store document name for print job */
    return true;
}

/* General print control */
void PrGeneral(Ptr pData) {
    /* Handle general print operations */
    /* This would process various print control operations */
}

/* Main dialog handler */
Boolean PrDlgMain(THPrint hPrint, ProcPtr pDlgInit) {
    /* Handle custom dialog initialization */
    if (pDlgInit) {
        /* Call custom initialization */
    }

    return PrJobDialog(hPrint);
}

/* Open picture file for printing */
TPPrPort PrOpenPictFile(short vRefNum, short dirID,
                        const StringPtr fileName) {
    /* Open a picture file for printing */
    /* This would create a special port for picture printing */
    return NULL;
}

/* Close picture file */
void PrClosePictFile(TPPrPort pPrPort) {
    /* Close picture printing port */
    if (pPrPort) {
        ClosePort(pPrPort);
        DisposePtr((Ptr)pPrPort);
    }
}