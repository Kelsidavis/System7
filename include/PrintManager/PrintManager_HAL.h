/*
 * PrintManager_HAL.h - Hardware Abstraction Layer for Print Manager
 *
 * This header defines the platform-specific interface for printing operations.
 * Each platform implements these functions to provide native printing support.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#ifndef PRINTMANAGER_HAL_H
#define PRINTMANAGER_HAL_H

#include "PrintManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform detection */
#ifdef __APPLE__
    #define PM_PLATFORM_MACOS
    #include <ApplicationServices/ApplicationServices.h>
#elif defined(_WIN32)
    #define PM_PLATFORM_WINDOWS
    #include <windows.h>
    #include <winspool.h>
#elif defined(__linux__)
    #define PM_PLATFORM_LINUX
    #include <cups/cups.h>
#endif

/* HAL Initialization */
OSErr PrintManager_HAL_Init(void);
void PrintManager_HAL_Cleanup(void);

/* Driver Management */
OSErr PrintManager_HAL_OpenDriver(void);
void PrintManager_HAL_CloseDriver(void);

/* Print Settings */
void PrintManager_HAL_GetDefaultSettings(TPPrint pPrint);
Boolean PrintManager_HAL_ShowPageSetup(TPPrint pPrint);
Boolean PrintManager_HAL_ShowPrintDialog(TPPrint pPrint);

/* Document Management */
OSErr PrintManager_HAL_BeginDocument(TPPrint pPrint, TPPrPort pPrPort);
void PrintManager_HAL_EndDocument(void);

/* Page Management */
OSErr PrintManager_HAL_BeginPage(short pageNum, const Rect* pageRect);
void PrintManager_HAL_EndPage(void);

/* Spooling */
OSErr PrintManager_HAL_PrintSpoolFile(const char* spoolPath);

/* Printer Information */
OSErr PrintManager_HAL_GetPrinterList(StringPtr printers[], short* count);
OSErr PrintManager_HAL_GetCurrentPrinter(StringPtr printerName);
OSErr PrintManager_HAL_SetCurrentPrinter(const StringPtr printerName);
Boolean PrintManager_HAL_IsPrinterAvailable(void);

/* Graphics Operations */
void PrintManager_HAL_DrawText(const char* text, short x, short y);
void PrintManager_HAL_DrawLine(short x1, short y1, short x2, short y2);
void PrintManager_HAL_DrawRect(const Rect* rect, Boolean filled);
void PrintManager_HAL_DrawBitmap(const BitMap* bitmap, const Rect* srcRect,
                                 const Rect* dstRect);

/* Platform-Specific Context */
#ifdef PM_PLATFORM_MACOS
typedef struct {
    PMPrintSession printSession;
    PMPageFormat pageFormat;
    PMPrintSettings printSettings;
    CGContextRef cgContext;
} PrintContext_MacOS;
#endif

#ifdef PM_PLATFORM_WINDOWS
typedef struct {
    HDC printDC;
    HANDLE hPrinter;
    DOCINFO docInfo;
    DEVMODE* devMode;
} PrintContext_Windows;
#endif

#ifdef PM_PLATFORM_LINUX
typedef struct {
    cups_dest_t* dest;
    int num_dests;
    cups_dest_t* current_dest;
    FILE* ps_file;
    char* ps_buffer;
} PrintContext_Linux;
#endif

#ifdef __cplusplus
}
#endif

#endif /* PRINTMANAGER_HAL_H */