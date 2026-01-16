/*
 * MacPaint_Main.c - MacPaint Application Entry Point
 *
 * Implements the main MacPaint application for System 7.1 Portable
 * This is the integration point between the converted MacPaint code
 * and the System 7.1 application framework.
 */

#include "SystemTypes.h"
#include "Apps/MacPaint.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include "Finder/finder.h"
#include <string.h>

/*
 * APPLICATION STARTUP
 */

/**
 * MacPaintMain - Main entry point for MacPaint application
 *
 * This function is called by the System 7.1 launcher when MacPaint is opened.
 * It initializes the application, sets up windows, and enters the event loop.
 *
 * ARGC/ARGV Support:
 * - argv[1] may contain path to document file to open
 * - argc > 1 indicates file was passed from Finder
 */
int MacPaintMain(int argc, char **argv)
{
    OSErr err;

    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaintMain: ENTRY\n");

    /* Initialize MacPaint core */
    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaintMain: Calling MacPaint_Initialize\n");
    err = MacPaint_Initialize();
    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaintMain: MacPaint_Initialize returned %d\n", err);
    if (err != noErr) {
        serial_logf(kLogModuleGeneral, kLogLevelError, "[MACPAINT] MacPaintMain: Initialize failed, going to cleanup\n");
        goto cleanup;
    }

    /* Initialize System 7.1 integration */
    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaintMain: Calling MacPaint_InitializeSystem\n");
    err = MacPaint_InitializeSystem();
    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaintMain: MacPaint_InitializeSystem returned %d\n", err);
    if (err != noErr) {
        serial_logf(kLogModuleGeneral, kLogLevelError, "[MACPAINT] MacPaintMain: InitializeSystem failed, going to cleanup\n");
        goto cleanup;
    }

    /* Create main window */
    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaintMain: Calling MacPaint_CreateMainWindow\n");
    err = MacPaint_CreateMainWindow();
    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaintMain: MacPaint_CreateMainWindow returned %d\n", err);
    if (err != noErr) {
        serial_logf(kLogModuleGeneral, kLogLevelError, "[MACPAINT] MacPaintMain: CreateMainWindow failed, going to cleanup\n");
        goto cleanup;
    }

    /* Create new document */
    err = MacPaint_NewDocument();
    if (err != noErr) {
        goto cleanup;
    }

    /* If a file was passed, try to open it */
    if (argc > 1 && argv[1]) {
        MacPaint_OpenDocument(argv[1]);
        MacPaint_SetDocumentName(argv[1]);
    } else {
        MacPaint_SetDocumentName("Untitled");
    }

    /* Initialize menu bar */
    MacPaint_InitializeMenuBar();

    /* Enter main event loop - runs until user quits */
    MacPaint_RunEventLoop();

cleanup:
    /* Prepare for shutdown (save prompts, etc) */
    MacPaint_PrepareForShutdown();

    /* Clean up system integration */
    MacPaint_ShutdownSystem();

    /* Clean up core */
    MacPaint_Shutdown();
    return err;
}

/*
 * EVENT HANDLING - Implemented in MacPaint_Menus.c
 * - MacPaint_HandleMouseDown: mouse clicks and tool selection
 * - MacPaint_HandleMouseDrag: continuous drawing during drag
 * - MacPaint_HandleMouseUp: release and finalize drawing
 * - MacPaint_HandleKeyDown: keyboard shortcuts and tool selection
 *
 * MENU HANDLING - Implemented in MacPaint_Menus.c
 * - MacPaint_HandleMenuCommand: File, Edit, Aids menu operations
 */

/*
 * RENDERING AND UPDATES
 */

/**
 * MacPaint_Update - Redraw window content
 * Called when window needs to be redrawn (resize, expose, etc)
 */
void MacPaint_Update(void)
{
    MacPaint_Render();
}

/*
 * RESOURCE LOADING
 */

/**
 * MacPaint_LoadResources - Load patterns and brushes from resources
 *
 * MacPaint stores patterns and brushes in the resource fork:
 * - PAT# resource (ID 0): Pattern table
 * - BRUS resource: Brush definitions
 * - PICT resources: Icons for tools and patterns
 */
OSErr MacPaint_LoadResources(void)
{
    /* TODO: Implement resource loading */
    return noErr;
}

/*
 * DOCUMENT STATE
 */

/**
 * MacPaint_PromptSave - Prompt user to save if document is dirty
 *
 * Returns:
 * - noErr: Document saved or discarded
 * - userCanceledErr: User cancelled operation
 */
OSErr MacPaint_PromptSave(void)
{
    /* TODO: Implement save prompt dialog */
    return noErr;
}

/*
 * DEBUGGING AND DIAGNOSTICS
 */

/**
 * MacPaint_CheckMemory - Verify heap integrity and report memory status
 */
void MacPaint_CheckMemory(void)
{
    Size freeBytes = FreeMem();
    Size totalBytes = 0;
    /* TODO: Report memory statistics */
}

/*
 * CONVERSION NOTES FOR DEVELOPERS
 *
 * This file integrates the converted MacPaint code with System 7.1.
 * The following conversions were made from the original Pascal/68k source:
 *
 * 1. PASCAL TO C CONVERSION (MacPaint.p):
 *    - PROCEDURE/FUNCTION declarations → C function prototypes
 *    - VAR declarations → static global or local variables
 *    - TYPE records → C structs
 *    - String[255] → C char arrays
 *    - Boolean → boolean (or int where needed)
 *    - EXTERNAL procedures → Functions in MacPaint_Core.c
 *
 * 2. 68K ASSEMBLY TO C CONVERSION:
 *
 *    a) MyHeapAsm.a (Memory Manager):
 *       - MoreMasters → MoreMasters() [calls Memory Manager]
 *       - FreeMem → FreeMem() [calls Memory Manager]
 *       - MaxMem → MaxMem() [calls Memory Manager]
 *       - NewHandle → NewHandle() [calls Memory Manager]
 *       - DisposHandle → DisposeHandle() [calls Memory Manager]
 *       - HLock/HUnlock → HLock/HUnlock() [bit manipulation]
 *       - HPurge/HNoPurge → HPurge/HNoPurge() [bit manipulation]
 *
 *    b) MyTools.a (Toolbox Traps):
 *       - Direct .WORD definitions of trap codes
 *       - These are replaced by System 7.1 Toolbox function calls
 *       - Examples: InitCurs, SetCursor, HideCursor, ShowCursor, etc
 *       - All become function calls to our Toolbox implementations
 *
 *    c) PaintAsm.a (Painting Algorithms):
 *       - PixelTrue → MacPaint_PixelTrue() [bit extraction]
 *       - SetPixel/ClearPixel → MacPaint_SetPixel/ClearPixel()
 *       - InvertBuf → MacPaint_InvertBuf() [XOR all bits]
 *       - ZeroBuf → MacPaint_ZeroBuf() [memset]
 *       - DrawBrush → MacPaint_DrawBrush() [pattern drawing]
 *       - ExpandPat → MacPaint_ExpandPattern() [pattern expansion]
 *       - TrimBBox → MacPaint_TrimBBox() [bounds calculation]
 *       - PackBits/UnpackBits → MacPaint_PackBits/UnpackBits() [RLE codec]
 *       - CalcEdges → MacPaint_CalcEdges() [edge detection]
 *
 * 3. SYSTEM-SPECIFIC ADAPTATIONS:
 *    - BitMap structures use QuickDraw format
 *    - Window/Port management uses System 7.1 Window Manager
 *    - Event handling adapted to System 7.1 Event Manager
 *    - File I/O uses System 7.1 File Manager
 *    - Menu handling through System 7.1 Menu Manager
 *    - Drawing through System 7.1 QuickDraw (monochrome)
 *
 * 4. KNOWN LIMITATIONS:
 *    - Printing support simplified (no style-based formatting yet)
 *    - Text tool partially implemented
 *    - Some advanced drawing modes not yet ported
 *    - Undo/Redo limited (single level currently)
 *    - Scrap format may differ from original
 *
 * 5. BUILD INTEGRATION:
 *    - Added to Makefile: src/Apps/MacPaint/MacPaint_Core.c, MacPaint_Main.c
 *    - Header: include/Apps/MacPaint.h
 *    - Compiled as part of System 7.1 kernel
 *    - Launched as document-based application from Finder
 *
 * For questions about specific conversions, see the comments in MacPaint_Core.c
 */

/*
 * FINDER LAUNCHER WRAPPER FUNCTIONS
 *
 * These functions provide the interface that the Finder expects to launch
 * and manage MacPaint. They follow the same pattern as SimpleText and other
 * System 7.1 applications.
 */

/* Global state tracking */
static int gMacPaintIsRunning = 0;
static char gMacPaintOpenFilePath[256] = {0};

/**
 * MacPaint_Launch - Launch MacPaint application
 *
 * Called by Finder when user opens MacPaint from Applications folder.
 * Sets up global state and calls MacPaintMain with no arguments.
 */
void MacPaint_Launch(void)
{
    int argc = 1;
    char *argv[2] = { "MacPaint", NULL };

    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaint_Launch: ENTRY\n");
    gMacPaintIsRunning = 1;
    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaint_Launch: Calling MacPaintMain\n");
    MacPaintMain(argc, argv);
    serial_logf(kLogModuleGeneral, kLogLevelInfo, "[MACPAINT] MacPaint_Launch: MacPaintMain returned\n");
    gMacPaintIsRunning = 0;
}

/**
 * MacPaint_Init - Initialize MacPaint
 *
 * Separate initialization function for cases where Finder needs to
 * initialize MacPaint before other operations.
 */
void MacPaint_Init(void)
{
    gMacPaintIsRunning = 1;
}

/**
 * MacPaint_Quit - Quit MacPaint gracefully
 *
 * Called by Finder to shut down MacPaint. Sets flag to allow
 * event loop to exit cleanly.
 */
void MacPaint_Quit(void)
{
    gMacPaintIsRunning = 0;
    MacPaint_RequestQuit();
}

/**
 * MacPaint_IsRunning - Check if MacPaint is currently running
 *
 * Returns: Boolean value (non-zero = running, 0 = not running)
 */
Boolean MacPaint_IsRunning(void)
{
    return (Boolean)gMacPaintIsRunning;
}

/**
 * MacPaint_OpenFile - Open a file in MacPaint
 *
 * Called by Finder when user drops a file on MacPaint or
 * opens a document associated with MacPaint.
 *
 * Parameters:
 * - path: C string path to the file to open
 */
void MacPaint_OpenFile(const char* path)
{
    if (path && path[0]) {
        strncpy(gMacPaintOpenFilePath, path, sizeof(gMacPaintOpenFilePath) - 1);
        gMacPaintOpenFilePath[sizeof(gMacPaintOpenFilePath) - 1] = '\0';

        /* If MacPaint is already running, open the file in the existing window */
        if (gMacPaintIsRunning) {
            MacPaint_OpenDocument(path);
            MacPaint_SetDocumentName(path);
        } else {
            /* MacPaint will open this file when launched */
            int argc = 2;
            /* Use already-copied buffer for MacPaintMain argv compatibility */
            char *argv[3] = { (char*)"MacPaint", gMacPaintOpenFilePath, NULL };
            gMacPaintIsRunning = 1;
            MacPaintMain(argc, argv);
            gMacPaintIsRunning = 0;
        }
    }
}
