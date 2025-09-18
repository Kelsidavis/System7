/*
 * PrintDialogs.c
 *
 * Print Dialog implementation for System 7.1 Portable
 * Handles page setup and print dialogs with modern UI integration
 *
 * Based on Apple's Print Manager dialogs from Mac OS System 7.1
 */

#include "PrintDialogs.h"
#include "PrintManager.h"
#include "PageLayout.h"
#include "Dialogs.h"
#include "Controls.h"
#include "TextEdit.h"
#include "Resources.h"
#include "Memory.h"
#include "Events.h"
#include "MenuMgr.h"
#include <string.h>
#include <stdio.h>

/* Dialog State Management */
static TDialogState *gCurrentDialogState = NULL;
static WindowPtr gPrintStatusWindow = NULL;
static WindowPtr gPreviewWindow = NULL;

/* Dialog Templates */
static DialogPtr gPageSetupDialog = NULL;
static DialogPtr gPrintDialog = NULL;
static DialogPtr gPrinterSetupDialog = NULL;

/* Forward Declarations */
static OSErr CreatePageSetupDialog(THPrint hPrint, DialogPtr *dialog);
static OSErr CreatePrintDialog(THPrint hPrint, DialogPtr *dialog);
static void UpdatePageSetupControls(DialogPtr dialog, THPrint hPrint);
static void UpdatePrintControls(DialogPtr dialog, THPrint hPrint);
static Boolean HandlePageSetupEvent(DialogPtr dialog, EventRecord *event, short *itemHit, THPrint hPrint);
static Boolean HandlePrintEvent(DialogPtr dialog, EventRecord *event, short *itemHit, THPrint hPrint);
static void DrawPreviewItem(DialogPtr dialog, short item, THPrint hPrint);

/*
 * Page Setup Dialog Functions
 */

/*
 * PrStlDialog - Show page setup dialog
 */
pascal Boolean PrStlDialog(THPrint hPrint)
{
    if (!hPrint) {
        PrSetError(paramErr);
        return false;
    }

    return ShowPageSetupDialog(hPrint);
}

/*
 * ShowPageSetupDialog - Display page setup dialog
 */
Boolean ShowPageSetupDialog(THPrint hPrint)
{
    DialogPtr dialog = NULL;
    Boolean result = false;
    OSErr err;

    /* Create the dialog */
    err = CreatePageSetupDialog(hPrint, &dialog);
    if (err != noErr || !dialog) {
        PrSetError(err);
        return false;
    }

    /* Show the dialog */
    ShowWindow((WindowPtr)dialog);
    SelectWindow((WindowPtr)dialog);

    /* Initialize controls */
    UpdatePageSetupControls(dialog, hPrint);

    /* Modal dialog loop */
    short itemHit;
    Boolean done = false;

    while (!done) {
        ModalDialog(PrintDialogFilter, &itemHit);

        switch (itemHit) {
            case kPageSetupOK:
                /* Apply changes to print record */
                result = true;
                done = true;
                break;

            case kPageSetupCancel:
                result = false;
                done = true;
                break;

            case kPageSetupPaperSize:
            case kPageSetupOrientation:
            case kPageSetupScale:
            case kPageSetupMargins:
                /* Handle control changes */
                UpdatePageSetupControls(dialog, hPrint);
                break;

            case kPreview:
                /* Show preview */
                ShowPrintPreviewDialog(hPrint, NULL);
                break;
        }
    }

    /* Cleanup */
    DisposeDialog(dialog);
    PrSetError(noErr);
    return result;
}

/*
 * PrStlInit - Initialize page setup dialog
 */
pascal TPPrDlg PrStlInit(THPrint hPrint)
{
    if (!hPrint) {
        PrSetError(paramErr);
        return NULL;
    }

    /* Create dialog state */
    DialogPtr dialog;
    OSErr err = CreatePageSetupDialog(hPrint, &dialog);
    if (err != noErr || !dialog) {
        PrSetError(err);
        return NULL;
    }

    /* Create and return dialog state */
    TDialogStatePtr state = CreateDialogState(dialog, hPrint);
    if (!state) {
        DisposeDialog(dialog);
        PrSetError(memFullErr);
        return NULL;
    }

    PrSetError(noErr);
    return (TPPrDlg)state;
}

/*
 * Print Dialog Functions
 */

/*
 * PrJobDialog - Show print dialog
 */
pascal Boolean PrJobDialog(THPrint hPrint)
{
    if (!hPrint) {
        PrSetError(paramErr);
        return false;
    }

    return ShowPrintDialog(hPrint);
}

/*
 * ShowPrintDialog - Display print dialog
 */
Boolean ShowPrintDialog(THPrint hPrint)
{
    DialogPtr dialog = NULL;
    Boolean result = false;
    OSErr err;

    /* Create the dialog */
    err = CreatePrintDialog(hPrint, &dialog);
    if (err != noErr || !dialog) {
        PrSetError(err);
        return false;
    }

    /* Show the dialog */
    ShowWindow((WindowPtr)dialog);
    SelectWindow((WindowPtr)dialog);

    /* Initialize controls */
    UpdatePrintControls(dialog, hPrint);

    /* Modal dialog loop */
    short itemHit;
    Boolean done = false;

    while (!done) {
        ModalDialog(PrintDialogFilter, &itemHit);

        switch (itemHit) {
            case kPrintOK:
                /* Apply changes to print record */
                result = true;
                done = true;
                break;

            case kPrintCancel:
                result = false;
                done = true;
                break;

            case kPrintCopies:
            case kPrintPageRange:
            case kPrintFromPage:
            case kPrintToPage:
            case kPrintQuality:
            case kPrintDestination:
                /* Handle control changes */
                UpdatePrintControls(dialog, hPrint);
                break;

            case kPreview:
                /* Show preview */
                ShowPrintPreviewDialog(hPrint, NULL);
                break;

            case kPrinterSetup:
                /* Show printer setup */
                ShowPrinterSetupDialog();
                break;
        }
    }

    /* Cleanup */
    DisposeDialog(dialog);
    PrSetError(noErr);
    return result;
}

/*
 * PrJobInit - Initialize print dialog
 */
pascal TPPrDlg PrJobInit(THPrint hPrint)
{
    if (!hPrint) {
        PrSetError(paramErr);
        return NULL;
    }

    /* Create dialog state */
    DialogPtr dialog;
    OSErr err = CreatePrintDialog(hPrint, &dialog);
    if (err != noErr || !dialog) {
        PrSetError(err);
        return NULL;
    }

    /* Create and return dialog state */
    TDialogStatePtr state = CreateDialogState(dialog, hPrint);
    if (!state) {
        DisposeDialog(dialog);
        PrSetError(memFullErr);
        return NULL;
    }

    PrSetError(noErr);
    return (TPPrDlg)state;
}

/*
 * PrJobMerge - Merge print job settings
 */
pascal void PrJobMerge(THPrint hPrintSrc, THPrint hPrintDst)
{
    if (!hPrintSrc || !hPrintDst) {
        PrSetError(paramErr);
        return;
    }

    TPrint *src = *hPrintSrc;
    TPrint *dst = *hPrintDst;

    /* Merge job-specific settings */
    dst->prJob.iFstPage = src->prJob.iFstPage;
    dst->prJob.iLstPage = src->prJob.iLstPage;
    dst->prJob.iCopies = src->prJob.iCopies;
    dst->prJob.bJDocLoop = src->prJob.bJDocLoop;
    dst->prJob.fFromUsr = src->prJob.fFromUsr;
    dst->prJob.pIdleProc = src->prJob.pIdleProc;

    PrSetError(noErr);
}

/*
 * PrDlgMain - Main dialog handler
 */
pascal Boolean PrDlgMain(THPrint hPrint, PDlgInitProcPtr pDlgInit)
{
    if (!hPrint || !pDlgInit) {
        PrSetError(paramErr);
        return false;
    }

    /* Call custom dialog initialization */
    TPPrDlg customDialog = pDlgInit(hPrint);
    if (!customDialog) {
        PrSetError(memFullErr);
        return false;
    }

    /* Run the custom dialog */
    TDialogStatePtr state = (TDialogStatePtr)customDialog;
    Boolean result = false;

    if (state->dialog) {
        ShowWindow((WindowPtr)state->dialog);

        /* Modal dialog loop */
        short itemHit;
        Boolean done = false;

        while (!done) {
            ModalDialog(PrintDialogFilter, &itemHit);

            switch (itemHit) {
                case kPageSetupOK:
                case kPrintOK:
                    result = true;
                    done = true;
                    break;

                case kPageSetupCancel:
                case kPrintCancel:
                    result = false;
                    done = true;
                    break;

                default:
                    /* Let custom handler process */
                    break;
            }
        }
    }

    /* Cleanup */
    DisposeDialogState(state);
    PrSetError(noErr);
    return result;
}

/*
 * Dialog Utilities
 */

/*
 * PrintDialogFilter - Dialog filter function
 */
pascal Boolean PrintDialogFilter(DialogPtr dialog, EventRecord *event, short *itemHit)
{
    Boolean handled = false;

    if (!dialog || !event || !itemHit) {
        return false;
    }

    switch (event->what) {
        case keyDown:
        case autoKey:
            {
                char key = event->message & charCodeMask;
                if (key == '\r' || key == '\003') { /* Return or Enter */
                    *itemHit = kPageSetupOK; /* Assume OK button */
                    handled = true;
                } else if (key == '\033') { /* Escape */
                    *itemHit = kPageSetupCancel; /* Assume Cancel button */
                    handled = true;
                }
            }
            break;

        case updateEvt:
            if ((WindowPtr)event->message == (WindowPtr)dialog) {
                BeginUpdate((WindowPtr)dialog);
                UpdateDialog(dialog, ((WindowPtr)dialog)->visRgn);

                /* Draw any custom items */
                if (gCurrentDialogState && gCurrentDialogState->hPrint) {
                    DrawPreviewItem(dialog, kPreview, gCurrentDialogState->hPrint);
                }

                EndUpdate((WindowPtr)dialog);
                handled = true;
            }
            break;
    }

    return handled;
}

/*
 * PrintDialogHook - Dialog hook procedure
 */
pascal void PrintDialogHook(DialogPtr dialog, short item)
{
    if (!dialog) {
        return;
    }

    /* Handle specific dialog items */
    switch (item) {
        case kPreview:
            if (gCurrentDialogState && gCurrentDialogState->hPrint) {
                DrawPreviewItem(dialog, item, gCurrentDialogState->hPrint);
            }
            break;
    }
}

/*
 * Printer Setup Functions
 */

/*
 * ShowPrinterSetupDialog - Display printer setup dialog
 */
Boolean ShowPrinterSetupDialog(void)
{
    /* Get available printers */
    StringPtr printerList[16];
    short printerCount = 0;

    OSErr err = GetAvailablePrinters(printerList, &printerCount);
    if (err != noErr || printerCount == 0) {
        /* Show error alert */
        PrSetError(err);
        return false;
    }

    /* Create a simple list dialog */
    DialogPtr dialog = GetNewDialog(kPrinterSetupDialogID, NULL, (WindowPtr)-1);
    if (!dialog) {
        PrSetError(resNotFound);
        return false;
    }

    Boolean result = false;
    short itemHit;
    Boolean done = false;

    ShowWindow((WindowPtr)dialog);

    while (!done) {
        ModalDialog(NULL, &itemHit);

        switch (itemHit) {
            case kPageSetupOK:
                result = true;
                done = true;
                break;

            case kPageSetupCancel:
                result = false;
                done = true;
                break;
        }
    }

    DisposeDialog(dialog);
    PrSetError(noErr);
    return result;
}

/*
 * GetAvailablePrinters - Get list of available printers
 */
OSErr GetAvailablePrinters(StringPtr printerList[], short *count)
{
    if (!count) {
        return paramErr;
    }

    /* Use Print Manager API to get printer list */
    return GetPrinterList(printerList, count);
}

/*
 * Print Status Functions
 */

/*
 * ShowPrintStatusDialog - Display print status dialog
 */
void ShowPrintStatusDialog(TPrStatus *status)
{
    if (!status) {
        return;
    }

    if (!gPrintStatusWindow) {
        /* Create status window */
        Rect bounds = {100, 100, 250, 400};
        gPrintStatusWindow = NewWindow(NULL, &bounds, "\pPrint Status",
                                      true, dBoxProc, (WindowPtr)-1, true, 0);
    }

    if (gPrintStatusWindow) {
        ShowWindow(gPrintStatusWindow);
        SelectWindow(gPrintStatusWindow);
        UpdatePrintStatusDialog(status);
    }
}

/*
 * UpdatePrintStatusDialog - Update print status display
 */
void UpdatePrintStatusDialog(TPrStatus *status)
{
    if (!gPrintStatusWindow || !status) {
        return;
    }

    /* Draw status information */
    SetPort(gPrintStatusWindow);

    char statusText[256];
    sprintf(statusText, "Page %d of %d", status->iCurPage, status->iTotPages);

    Rect textRect = {20, 20, 40, 280};
    MoveTo(textRect.left, textRect.bottom - 5);
    DrawString(c2pstr(statusText));

    /* Draw progress bar */
    Rect progressRect = {50, 20, 70, 280};
    FrameRect(&progressRect);

    if (status->iTotPages > 0) {
        short progress = (status->iCurPage * 260) / status->iTotPages;
        Rect fillRect = progressRect;
        fillRect.right = fillRect.left + progress;
        PaintRect(&fillRect);
    }
}

/*
 * HidePrintStatusDialog - Hide print status dialog
 */
void HidePrintStatusDialog(void)
{
    if (gPrintStatusWindow) {
        HideWindow(gPrintStatusWindow);
    }
}

/*
 * IsPrintStatusDialogVisible - Check if status dialog is visible
 */
Boolean IsPrintStatusDialogVisible(void)
{
    return (gPrintStatusWindow && ((WindowPeek)gPrintStatusWindow)->visible);
}

/*
 * Print Preview Functions
 */

/*
 * ShowPrintPreviewDialog - Display print preview dialog
 */
Boolean ShowPrintPreviewDialog(THPrint hPrint, PicHandle hPic)
{
    if (!hPrint) {
        return false;
    }

    /* Create preview window */
    if (!gPreviewWindow) {
        Rect bounds = {50, 50, 450, 600};
        gPreviewWindow = NewWindow(NULL, &bounds, "\pPrint Preview",
                                  true, documentProc, (WindowPtr)-1, true, 0);
    }

    if (gPreviewWindow) {
        ShowWindow(gPreviewWindow);
        SelectWindow(gPreviewWindow);

        if (hPic) {
            DrawPreviewPage(gPreviewWindow, hPrint, hPic, 1);
        }

        return true;
    }

    return false;
}

/*
 * DrawPreviewPage - Draw a preview page
 */
void DrawPreviewPage(WindowPtr window, THPrint hPrint, PicHandle hPic, short pageNum)
{
    if (!window || !hPrint) {
        return;
    }

    SetPort(window);

    /* Calculate preview area */
    Rect windowRect = window->portRect;
    Rect previewRect;

    short margin = 20;
    previewRect.left = windowRect.left + margin;
    previewRect.top = windowRect.top + margin;
    previewRect.right = windowRect.right - margin;
    previewRect.bottom = windowRect.bottom - margin;

    /* Draw paper background */
    PenNormal();
    PaintRect(&previewRect);
    FrameRect(&previewRect);

    /* Draw content if picture provided */
    if (hPic && *hPic) {
        Rect picFrame = (**hPic).picFrame;
        Rect destRect = previewRect;

        /* Scale to fit preview area */
        short picWidth = picFrame.right - picFrame.left;
        short picHeight = picFrame.bottom - picFrame.top;
        short previewWidth = previewRect.right - previewRect.left - 20;
        short previewHeight = previewRect.bottom - previewRect.top - 20;

        if (picWidth > 0 && picHeight > 0) {
            float scaleX = (float)previewWidth / picWidth;
            float scaleY = (float)previewHeight / picHeight;
            float scale = (scaleX < scaleY) ? scaleX : scaleY;

            short scaledWidth = (short)(picWidth * scale);
            short scaledHeight = (short)(picHeight * scale);

            destRect.left = previewRect.left + (previewWidth - scaledWidth) / 2 + 10;
            destRect.top = previewRect.top + (previewHeight - scaledHeight) / 2 + 10;
            destRect.right = destRect.left + scaledWidth;
            destRect.bottom = destRect.top + scaledHeight;

            DrawPicture(hPic, &destRect);
        }
    }
}

/*
 * Internal Helper Functions
 */

/*
 * CreatePageSetupDialog - Create page setup dialog
 */
static OSErr CreatePageSetupDialog(THPrint hPrint, DialogPtr *dialog)
{
    *dialog = GetNewDialog(kPageSetupDialogID, NULL, (WindowPtr)-1);
    if (!*dialog) {
        return resNotFound;
    }

    return noErr;
}

/*
 * CreatePrintDialog - Create print dialog
 */
static OSErr CreatePrintDialog(THPrint hPrint, DialogPtr *dialog)
{
    *dialog = GetNewDialog(kPrintDialogID, NULL, (WindowPtr)-1);
    if (!*dialog) {
        return resNotFound;
    }

    return noErr;
}

/*
 * UpdatePageSetupControls - Update page setup dialog controls
 */
static void UpdatePageSetupControls(DialogPtr dialog, THPrint hPrint)
{
    if (!dialog || !hPrint) {
        return;
    }

    TPrint *printRec = *hPrint;

    /* Update paper size popup */
    /* Update orientation radio buttons */
    /* Update margin text fields */
    /* Update scale field */
}

/*
 * UpdatePrintControls - Update print dialog controls
 */
static void UpdatePrintControls(DialogPtr dialog, THPrint hPrint)
{
    if (!dialog || !hPrint) {
        return;
    }

    TPrint *printRec = *hPrint;

    /* Update copies field */
    SetDialogItemValue(dialog, kPrintCopies, printRec->prJob.iCopies);

    /* Update page range */
    SetDialogItemValue(dialog, kPrintFromPage, printRec->prJob.iFstPage);
    SetDialogItemValue(dialog, kPrintToPage, printRec->prJob.iLstPage);
}

/*
 * DrawPreviewItem - Draw preview item in dialog
 */
static void DrawPreviewItem(DialogPtr dialog, short item, THPrint hPrint)
{
    if (!dialog || !hPrint) {
        return;
    }

    /* Get item rectangle */
    Rect itemRect;
    Handle itemHandle;
    short itemType;

    GetDialogItem(dialog, item, &itemType, &itemHandle, &itemRect);

    /* Draw mini preview */
    PenNormal();
    FrameRect(&itemRect);

    /* Draw page representation */
    Rect pageRect = itemRect;
    InsetRect(&pageRect, 4, 4);
    PaintRect(&pageRect);
    FrameRect(&pageRect);
}

/*
 * Dialog State Management
 */

/*
 * CreateDialogState - Create dialog state structure
 */
TDialogStatePtr CreateDialogState(DialogPtr dialog, THPrint hPrint)
{
    TDialogStatePtr state = (TDialogStatePtr)NewPtr(sizeof(TDialogState));
    if (!state) {
        return NULL;
    }

    memset(state, 0, sizeof(TDialogState));
    state->dialog = dialog;
    state->hPrint = hPrint;
    state->modified = false;
    state->currentPage = 1;

    gCurrentDialogState = state;
    return state;
}

/*
 * DisposeDialogState - Dispose dialog state structure
 */
void DisposeDialogState(TDialogStatePtr state)
{
    if (state) {
        if (state == gCurrentDialogState) {
            gCurrentDialogState = NULL;
        }
        DisposePtr((Ptr)state);
    }
}

/*
 * Dialog Utility Functions
 */

/*
 * SetDialogItemValue - Set numeric value for dialog item
 */
void SetDialogItemValue(DialogPtr dialog, short item, long value)
{
    if (!dialog) {
        return;
    }

    char valueStr[32];
    sprintf(valueStr, "%ld", value);

    Str255 pValueStr;
    strcpy((char *)pValueStr + 1, valueStr);
    pValueStr[0] = strlen(valueStr);

    SetDialogItemText(dialog, item, pValueStr);
}

/*
 * GetDialogItemValue - Get numeric value from dialog item
 */
long GetDialogItemValue(DialogPtr dialog, short item)
{
    if (!dialog) {
        return 0;
    }

    Str255 valueStr;
    GetDialogItemText(dialog, item, valueStr);

    if (valueStr[0] > 0) {
        valueStr[valueStr[0] + 1] = '\0';
        return atol((char *)valueStr + 1);
    }

    return 0;
}

/*
 * SetDialogItemText - Set text for dialog item
 */
void SetDialogItemText(DialogPtr dialog, short item, StringPtr text)
{
    if (!dialog || !text) {
        return;
    }

    Handle itemHandle;
    Rect itemRect;
    short itemType;

    GetDialogItem(dialog, item, &itemType, &itemHandle, &itemRect);
    if (itemType == editText && itemHandle) {
        SetIText(itemHandle, text);
    }
}

/*
 * GetDialogItemText - Get text from dialog item
 */
void GetDialogItemText(DialogPtr dialog, short item, StringPtr text)
{
    if (!dialog || !text) {
        return;
    }

    Handle itemHandle;
    Rect itemRect;
    short itemType;

    GetDialogItem(dialog, item, &itemType, &itemHandle, &itemRect);
    if (itemType == editText && itemHandle) {
        GetIText(itemHandle, text);
    } else {
        text[0] = 0; /* Empty string */
    }
}