/*
 * PrintManager_HAL.c - Hardware Abstraction Layer Implementation
 *
 * Provides platform-specific printing functionality for macOS, Windows, and Linux.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include "PrintManager/PrintManager_HAL.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Platform-specific context */
#ifdef PM_PLATFORM_MACOS
static PrintContext_MacOS gPrintContext;
#elif defined(PM_PLATFORM_WINDOWS)
static PrintContext_Windows gPrintContext;
#elif defined(PM_PLATFORM_LINUX)
static PrintContext_Linux gPrintContext;
#endif

/* Initialize HAL */
OSErr PrintManager_HAL_Init(void) {
#ifdef PM_PLATFORM_MACOS
    OSStatus status = PMCreateSession(&gPrintContext.printSession);
    if (status != noErr) {
        return status;
    }

    status = PMCreatePageFormat(&gPrintContext.pageFormat);
    if (status != noErr) {
        PMRelease(gPrintContext.printSession);
        return status;
    }

    status = PMSessionDefaultPageFormat(gPrintContext.printSession,
                                        gPrintContext.pageFormat);
    if (status != noErr) {
        PMRelease(gPrintContext.pageFormat);
        PMRelease(gPrintContext.printSession);
        return status;
    }

    status = PMCreatePrintSettings(&gPrintContext.printSettings);
    if (status != noErr) {
        PMRelease(gPrintContext.pageFormat);
        PMRelease(gPrintContext.printSession);
        return status;
    }

    status = PMSessionDefaultPrintSettings(gPrintContext.printSession,
                                           gPrintContext.printSettings);

    return status;

#elif defined(PM_PLATFORM_WINDOWS)
    memset(&gPrintContext, 0, sizeof(gPrintContext));

    /* Get default printer */
    DWORD size = 0;
    GetDefaultPrinter(NULL, &size);
    if (size > 0) {
        char* printerName = (char*)malloc(size);
        if (GetDefaultPrinter(printerName, &size)) {
            gPrintContext.printDC = CreateDC(NULL, printerName, NULL, NULL);
        }
        free(printerName);
    }

    return (gPrintContext.printDC != NULL) ? noErr : -1;

#elif defined(PM_PLATFORM_LINUX)
    /* Initialize CUPS */
    gPrintContext.num_dests = cupsGetDests(&gPrintContext.dest);
    if (gPrintContext.num_dests > 0) {
        /* Get default printer */
        gPrintContext.current_dest = cupsGetDest(NULL, NULL,
                                                 gPrintContext.num_dests,
                                                 gPrintContext.dest);
    }

    return noErr;

#else
    /* Platform not supported */
    return -1;
#endif
}

/* Cleanup HAL */
void PrintManager_HAL_Cleanup(void) {
#ifdef PM_PLATFORM_MACOS
    if (gPrintContext.printSettings) {
        PMRelease(gPrintContext.printSettings);
        gPrintContext.printSettings = NULL;
    }
    if (gPrintContext.pageFormat) {
        PMRelease(gPrintContext.pageFormat);
        gPrintContext.pageFormat = NULL;
    }
    if (gPrintContext.printSession) {
        PMRelease(gPrintContext.printSession);
        gPrintContext.printSession = NULL;
    }

#elif defined(PM_PLATFORM_WINDOWS)
    if (gPrintContext.printDC) {
        DeleteDC(gPrintContext.printDC);
        gPrintContext.printDC = NULL;
    }
    if (gPrintContext.devMode) {
        free(gPrintContext.devMode);
        gPrintContext.devMode = NULL;
    }

#elif defined(PM_PLATFORM_LINUX)
    if (gPrintContext.dest) {
        cupsFreeDests(gPrintContext.num_dests, gPrintContext.dest);
        gPrintContext.dest = NULL;
        gPrintContext.num_dests = 0;
    }
    if (gPrintContext.ps_buffer) {
        free(gPrintContext.ps_buffer);
        gPrintContext.ps_buffer = NULL;
    }
#endif
}

/* Open printer driver */
OSErr PrintManager_HAL_OpenDriver(void) {
#ifdef PM_PLATFORM_MACOS
    /* Driver is implicit in print session */
    return noErr;

#elif defined(PM_PLATFORM_WINDOWS)
    /* Open default printer */
    if (!gPrintContext.hPrinter) {
        DWORD size = 0;
        GetDefaultPrinter(NULL, &size);
        if (size > 0) {
            char* printerName = (char*)malloc(size);
            if (GetDefaultPrinter(printerName, &size)) {
                OpenPrinter(printerName, &gPrintContext.hPrinter, NULL);
            }
            free(printerName);
        }
    }
    return (gPrintContext.hPrinter != NULL) ? noErr : -1;

#elif defined(PM_PLATFORM_LINUX)
    /* CUPS handles driver implicitly */
    return noErr;

#else
    return -1;
#endif
}

/* Close printer driver */
void PrintManager_HAL_CloseDriver(void) {
#ifdef PM_PLATFORM_WINDOWS
    if (gPrintContext.hPrinter) {
        ClosePrinter(gPrintContext.hPrinter);
        gPrintContext.hPrinter = NULL;
    }
#endif
}

/* Get default print settings */
void PrintManager_HAL_GetDefaultSettings(TPPrint pPrint) {
    if (!pPrint) return;

#ifdef PM_PLATFORM_MACOS
    PMRect paperRect;
    PMGetAdjustedPaperRect(gPrintContext.pageFormat, &paperRect);

    pPrint->rPaper.left = (short)paperRect.left;
    pPrint->rPaper.top = (short)paperRect.top;
    pPrint->rPaper.right = (short)paperRect.right;
    pPrint->rPaper.bottom = (short)paperRect.bottom;

    PMRect pageRect;
    PMGetAdjustedPageRect(gPrintContext.pageFormat, &pageRect);

    pPrint->prInfo.rPage.left = (short)pageRect.left;
    pPrint->prInfo.rPage.top = (short)pageRect.top;
    pPrint->prInfo.rPage.right = (short)pageRect.right;
    pPrint->prInfo.rPage.bottom = (short)pageRect.bottom;

    /* Get resolution */
    PMResolution res;
    PMPrinter printer;
    PMSessionGetCurrentPrinter(gPrintContext.printSession, &printer);
    PMPrinterGetOutputResolution(printer, gPrintContext.printSettings, &res);

    pPrint->prInfo.iHRes = (short)res.hRes;
    pPrint->prInfo.iVRes = (short)res.vRes;

#elif defined(PM_PLATFORM_WINDOWS)
    if (gPrintContext.printDC) {
        /* Get page dimensions */
        int width = GetDeviceCaps(gPrintContext.printDC, HORZRES);
        int height = GetDeviceCaps(gPrintContext.printDC, VERTRES);

        pPrint->prInfo.rPage.left = 0;
        pPrint->prInfo.rPage.top = 0;
        pPrint->prInfo.rPage.right = width;
        pPrint->prInfo.rPage.bottom = height;

        /* Get physical page size */
        int physWidth = GetDeviceCaps(gPrintContext.printDC, PHYSICALWIDTH);
        int physHeight = GetDeviceCaps(gPrintContext.printDC, PHYSICALHEIGHT);
        int offsetX = GetDeviceCaps(gPrintContext.printDC, PHYSICALOFFSETX);
        int offsetY = GetDeviceCaps(gPrintContext.printDC, PHYSICALOFFSETY);

        pPrint->rPaper.left = -offsetX;
        pPrint->rPaper.top = -offsetY;
        pPrint->rPaper.right = physWidth - offsetX;
        pPrint->rPaper.bottom = physHeight - offsetY;

        /* Get resolution */
        pPrint->prInfo.iHRes = GetDeviceCaps(gPrintContext.printDC, LOGPIXELSX);
        pPrint->prInfo.iVRes = GetDeviceCaps(gPrintContext.printDC, LOGPIXELSY);
    }

#elif defined(PM_PLATFORM_LINUX)
    /* Default to US Letter at 72 DPI */
    pPrint->prInfo.rPage.left = 0;
    pPrint->prInfo.rPage.top = 0;
    pPrint->prInfo.rPage.right = 612;
    pPrint->prInfo.rPage.bottom = 792;

    pPrint->rPaper.left = -36;
    pPrint->rPaper.top = -36;
    pPrint->rPaper.right = 648;
    pPrint->rPaper.bottom = 828;

    pPrint->prInfo.iHRes = 72;
    pPrint->prInfo.iVRes = 72;
#endif
}

/* Show page setup dialog */
Boolean PrintManager_HAL_ShowPageSetup(TPPrint pPrint) {
#ifdef PM_PLATFORM_MACOS
    Boolean accepted = false;
    OSStatus status = PMSessionPageSetupDialog(gPrintContext.printSession,
                                               gPrintContext.pageFormat,
                                               &accepted);

    if (status == noErr && accepted) {
        /* Update print record with new settings */
        PrintManager_HAL_GetDefaultSettings(pPrint);
        return true;
    }
    return false;

#elif defined(PM_PLATFORM_WINDOWS)
    PAGESETUPDLG psd;
    memset(&psd, 0, sizeof(psd));
    psd.lStructSize = sizeof(psd);
    psd.Flags = PSD_DEFAULTMINMARGINS | PSD_RETURNDEFAULT;

    if (PageSetupDlg(&psd)) {
        /* Update print record */
        pPrint->rPaper.left = psd.rtMargin.left / 100;
        pPrint->rPaper.top = psd.rtMargin.top / 100;
        pPrint->rPaper.right = psd.ptPaperSize.x / 100 - psd.rtMargin.right / 100;
        pPrint->rPaper.bottom = psd.ptPaperSize.y / 100 - psd.rtMargin.bottom / 100;

        if (psd.hDevMode) {
            GlobalFree(psd.hDevMode);
        }
        if (psd.hDevNames) {
            GlobalFree(psd.hDevNames);
        }
        return true;
    }
    return false;

#elif defined(PM_PLATFORM_LINUX)
    /* Linux typically doesn't have page setup dialog */
    /* Could launch a GTK dialog here if available */
    return false;

#else
    return false;
#endif
}

/* Show print dialog */
Boolean PrintManager_HAL_ShowPrintDialog(TPPrint pPrint) {
#ifdef PM_PLATFORM_MACOS
    Boolean accepted = false;
    OSStatus status = PMSessionPrintDialog(gPrintContext.printSession,
                                           gPrintContext.printSettings,
                                           gPrintContext.pageFormat,
                                           &accepted);

    if (status == noErr && accepted) {
        /* Get page range */
        UInt32 firstPage, lastPage;
        PMGetFirstPage(gPrintContext.printSettings, &firstPage);
        PMGetLastPage(gPrintContext.printSettings, &lastPage);
        pPrint->prJob.iFstPage = (short)firstPage;
        pPrint->prJob.iLstPage = (short)lastPage;

        /* Get copies */
        UInt32 copies;
        PMGetCopies(gPrintContext.printSettings, &copies);
        pPrint->prJob.iCopies = (short)copies;

        return true;
    }
    return false;

#elif defined(PM_PLATFORM_WINDOWS)
    PRINTDLG pd;
    memset(&pd, 0, sizeof(pd));
    pd.lStructSize = sizeof(pd);
    pd.Flags = PD_USEDEVMODECOPIESANDCOLLATE | PD_RETURNDC;
    pd.nFromPage = pPrint->prJob.iFstPage;
    pd.nToPage = pPrint->prJob.iLstPage;
    pd.nMinPage = 1;
    pd.nMaxPage = 9999;
    pd.nCopies = pPrint->prJob.iCopies;

    if (PrintDlg(&pd)) {
        /* Update print record */
        if (pd.Flags & PD_PAGENUMS) {
            pPrint->prJob.iFstPage = pd.nFromPage;
            pPrint->prJob.iLstPage = pd.nToPage;
        }
        pPrint->prJob.iCopies = pd.nCopies;

        if (gPrintContext.printDC && gPrintContext.printDC != pd.hDC) {
            DeleteDC(gPrintContext.printDC);
        }
        gPrintContext.printDC = pd.hDC;

        if (pd.hDevMode) {
            GlobalFree(pd.hDevMode);
        }
        if (pd.hDevNames) {
            GlobalFree(pd.hDevNames);
        }
        return true;
    }
    return false;

#elif defined(PM_PLATFORM_LINUX)
    /* Linux typically uses GTK or Qt dialogs */
    /* For now, accept default settings */
    return true;

#else
    return false;
#endif
}

/* Begin document */
OSErr PrintManager_HAL_BeginDocument(TPPrint pPrint, TPPrPort pPrPort) {
#ifdef PM_PLATFORM_MACOS
    OSStatus status = PMSessionBeginCGDocumentNoDialog(gPrintContext.printSession,
                                                       gPrintContext.printSettings,
                                                       gPrintContext.pageFormat);
    return status;

#elif defined(PM_PLATFORM_WINDOWS)
    memset(&gPrintContext.docInfo, 0, sizeof(DOCINFO));
    gPrintContext.docInfo.cbSize = sizeof(DOCINFO);
    gPrintContext.docInfo.lpszDocName = "System7.1 Print Job";

    if (StartDoc(gPrintContext.printDC, &gPrintContext.docInfo) > 0) {
        return noErr;
    }
    return -1;

#elif defined(PM_PLATFORM_LINUX)
    /* Prepare for PostScript generation */
    return noErr;

#else
    return -1;
#endif
}

/* End document */
void PrintManager_HAL_EndDocument(void) {
#ifdef PM_PLATFORM_MACOS
    PMSessionEndDocumentNoDialog(gPrintContext.printSession);

#elif defined(PM_PLATFORM_WINDOWS)
    EndDoc(gPrintContext.printDC);

#elif defined(PM_PLATFORM_LINUX)
    /* Complete PostScript generation */
#endif
}

/* Begin page */
OSErr PrintManager_HAL_BeginPage(short pageNum, const Rect* pageRect) {
#ifdef PM_PLATFORM_MACOS
    OSStatus status = PMSessionBeginPageNoDialog(gPrintContext.printSession,
                                                 gPrintContext.pageFormat,
                                                 NULL);

    if (status == noErr) {
        status = PMSessionGetCGGraphicsContext(gPrintContext.printSession,
                                               &gPrintContext.cgContext);
    }
    return status;

#elif defined(PM_PLATFORM_WINDOWS)
    if (StartPage(gPrintContext.printDC) > 0) {
        return noErr;
    }
    return -1;

#elif defined(PM_PLATFORM_LINUX)
    /* Generate PostScript page header */
    return noErr;

#else
    return -1;
#endif
}

/* End page */
void PrintManager_HAL_EndPage(void) {
#ifdef PM_PLATFORM_MACOS
    PMSessionEndPageNoDialog(gPrintContext.printSession);
    gPrintContext.cgContext = NULL;

#elif defined(PM_PLATFORM_WINDOWS)
    EndPage(gPrintContext.printDC);

#elif defined(PM_PLATFORM_LINUX)
    /* Generate PostScript page footer */
#endif
}

/* Print spool file */
OSErr PrintManager_HAL_PrintSpoolFile(const char* spoolPath) {
#ifdef PM_PLATFORM_MACOS
    /* macOS handles spooling internally */
    return noErr;

#elif defined(PM_PLATFORM_WINDOWS)
    /* Windows handles spooling internally */
    return noErr;

#elif defined(PM_PLATFORM_LINUX)
    /* Use CUPS to print PostScript file */
    if (gPrintContext.current_dest) {
        int job_id = cupsPrintFile(gPrintContext.current_dest->name,
                                   spoolPath, "System7.1 Print Job",
                                   0, NULL);
        return (job_id > 0) ? noErr : -1;
    }
    return -1;

#else
    return -1;
#endif
}

/* Get printer list */
OSErr PrintManager_HAL_GetPrinterList(StringPtr printers[], short* count) {
#ifdef PM_PLATFORM_MACOS
    CFArrayRef printerList;
    OSStatus status = PMServerCreatePrinterList(kPMServerLocal, &printerList);

    if (status == noErr && printerList) {
        *count = (short)CFArrayGetCount(printerList);
        for (short i = 0; i < *count && i < 32; i++) {
            PMPrinter printer = (PMPrinter)CFArrayGetValueAtIndex(printerList, i);
            CFStringRef name = PMPrinterGetID(printer);

            if (name) {
                char buffer[256];
                CFStringGetCString(name, buffer, sizeof(buffer),
                                  kCFStringEncodingUTF8);
                printers[i] = (StringPtr)malloc(strlen(buffer) + 1);
                printers[i][0] = strlen(buffer);
                memcpy(&printers[i][1], buffer, strlen(buffer));
            }
        }
        CFRelease(printerList);
        return noErr;
    }
    return status;

#elif defined(PM_PLATFORM_WINDOWS)
    DWORD size = 0;
    DWORD numPrinters = 0;
    EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &size, &numPrinters);

    if (size > 0) {
        PRINTER_INFO_2* info = (PRINTER_INFO_2*)malloc(size);
        if (EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 2, (LPBYTE)info, size,
                        &size, &numPrinters)) {
            *count = (short)min(numPrinters, 32);
            for (short i = 0; i < *count; i++) {
                printers[i] = (StringPtr)malloc(strlen(info[i].pPrinterName) + 1);
                printers[i][0] = strlen(info[i].pPrinterName);
                memcpy(&printers[i][1], info[i].pPrinterName,
                      strlen(info[i].pPrinterName));
            }
        }
        free(info);
        return noErr;
    }
    return -1;

#elif defined(PM_PLATFORM_LINUX)
    *count = (short)min(gPrintContext.num_dests, 32);
    for (short i = 0; i < *count; i++) {
        printers[i] = (StringPtr)malloc(strlen(gPrintContext.dest[i].name) + 1);
        printers[i][0] = strlen(gPrintContext.dest[i].name);
        memcpy(&printers[i][1], gPrintContext.dest[i].name,
              strlen(gPrintContext.dest[i].name));
    }
    return noErr;

#else
    *count = 0;
    return -1;
#endif
}

/* Check if printer available */
Boolean PrintManager_HAL_IsPrinterAvailable(void) {
#ifdef PM_PLATFORM_MACOS
    PMPrinter printer;
    OSStatus status = PMSessionGetCurrentPrinter(gPrintContext.printSession,
                                                 &printer);
    return (status == noErr && printer != NULL);

#elif defined(PM_PLATFORM_WINDOWS)
    return (gPrintContext.printDC != NULL);

#elif defined(PM_PLATFORM_LINUX)
    return (gPrintContext.num_dests > 0);

#else
    return false;
#endif
}

/* Draw text */
void PrintManager_HAL_DrawText(const char* text, short x, short y) {
#ifdef PM_PLATFORM_MACOS
    if (gPrintContext.cgContext) {
        CGContextShowTextAtPoint(gPrintContext.cgContext, x, y,
                                 text, strlen(text));
    }

#elif defined(PM_PLATFORM_WINDOWS)
    if (gPrintContext.printDC) {
        TextOut(gPrintContext.printDC, x, y, text, strlen(text));
    }

#elif defined(PM_PLATFORM_LINUX)
    /* Generate PostScript text commands */
#endif
}

/* Draw line */
void PrintManager_HAL_DrawLine(short x1, short y1, short x2, short y2) {
#ifdef PM_PLATFORM_MACOS
    if (gPrintContext.cgContext) {
        CGContextMoveToPoint(gPrintContext.cgContext, x1, y1);
        CGContextAddLineToPoint(gPrintContext.cgContext, x2, y2);
        CGContextStrokePath(gPrintContext.cgContext);
    }

#elif defined(PM_PLATFORM_WINDOWS)
    if (gPrintContext.printDC) {
        MoveToEx(gPrintContext.printDC, x1, y1, NULL);
        LineTo(gPrintContext.printDC, x2, y2);
    }

#elif defined(PM_PLATFORM_LINUX)
    /* Generate PostScript line commands */
#endif
}

/* Draw rectangle */
void PrintManager_HAL_DrawRect(const Rect* rect, Boolean filled) {
#ifdef PM_PLATFORM_MACOS
    if (gPrintContext.cgContext) {
        CGRect cgRect = CGRectMake(rect->left, rect->top,
                                   rect->right - rect->left,
                                   rect->bottom - rect->top);
        if (filled) {
            CGContextFillRect(gPrintContext.cgContext, cgRect);
        } else {
            CGContextStrokeRect(gPrintContext.cgContext, cgRect);
        }
    }

#elif defined(PM_PLATFORM_WINDOWS)
    if (gPrintContext.printDC) {
        if (filled) {
            RECT winRect = {rect->left, rect->top, rect->right, rect->bottom};
            FillRect(gPrintContext.printDC, &winRect,
                    (HBRUSH)GetStockObject(BLACK_BRUSH));
        } else {
            Rectangle(gPrintContext.printDC, rect->left, rect->top,
                     rect->right, rect->bottom);
        }
    }

#elif defined(PM_PLATFORM_LINUX)
    /* Generate PostScript rectangle commands */
#endif
}