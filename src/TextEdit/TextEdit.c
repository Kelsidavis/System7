/*
 * TextEdit.c - System 7.1 TextEdit Manager Core
 *
 * Single-style text editing core implementation
 * Handles creation, disposal, and basic text management
 */

/* Include stdio.h first to avoid FILE type conflict with SystemTypes.h */
#include <stdio.h>
#include <string.h>

/* Define _FILE_DEFINED to prevent SystemTypes.h from redefining FILE */
#define _FILE_DEFINED

#include "TextEdit/TextEdit.h"
#include "MemoryMgr/MemoryManager.h"
#include "FontManager/FontManager.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "FileManager.h"
#include "TextEdit/TELogging.h"

/* Boolean constants */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Debug logging */
#define TE_DEBUG 1

#if TE_DEBUG
#define TE_LOG(...) TE_LOG_DEBUG("TE: " __VA_ARGS__)
#else
#define TE_LOG(...)
#endif

/* Constants */
#define TE_INITIAL_BUFFER   256     /* Initial text buffer size */
#define TE_CARET_BLINK      30      /* Caret blink interval (ticks) */

/* Extended TERec with additional fields */
typedef struct TEExtRec {
    TERec       base;           /* Standard TERec */
    Handle      hLines;         /* Line starts array */
    SInt16      nLines;         /* Number of lines */
    Handle      hStyles;        /* Style record handle */
    Boolean     dirty;          /* Needs recalc */
    Boolean     readOnly;       /* Read-only flag */
    Boolean     wordWrap;       /* Word wrap flag */
    SInt16      dragAnchor;     /* Drag selection anchor */
    Boolean     inDragSel;      /* In drag selection */
    UInt32      lastClickTime;  /* For double/triple click */
    SInt16      clickCount;     /* Click count */
    SInt16      viewDH;         /* Horizontal scroll */
    SInt16      viewDV;         /* Vertical scroll */
    Boolean     autoViewEnabled;/* Auto-scroll flag */
} TEExtRec;

typedef TEExtRec *TEExtPtr, **TEExtHandle;

/* Globals */
static Boolean g_teInitialized = FALSE;

/* External functions */
extern UInt32 TickCount(void);
extern GrafPtr g_currentPort;
extern void BlockMove(const void *src, void *dest, Size size);
extern OSErr MemError(void);
#define FMGetFontMetrics GetFontMetrics  /* Use our FontManager function */

/* Event constants */
#ifndef charCodeMask
#define charCodeMask 0x000000FF
#endif
#ifndef shiftKey
#define shiftKey 0x0200
#endif

/* Forward declarations for internal functions */
static void TE_InitRecord(TEHandle hTE, const Rect *destRect, const Rect *viewRect);
static OSErr TE_GrowTextBuffer(TEHandle hTE, SInt32 newSize);
static void TE_SetDefaultStyle(TEHandle hTE);

/* ============================================================================
 * Initialization
 * ============================================================================ */

/*
 * TEInit - Initialize TextEdit Manager
 */
void TEInit(void) {
    if (!g_teInitialized) {
        TE_LOG("Initializing TextEdit Manager\n");
        g_teInitialized = TRUE;

        /* Initialize Font Manager if needed */
        InitFonts();
    }
}

/* ============================================================================
 * Creation and Disposal
 * ============================================================================ */

/*
 * TENew - Create a new single-style TextEdit record
 */
TEHandle TENew(const Rect *destRect, const Rect *viewRect) {
    TEHandle hTE;
    TEExtPtr pTE;

    TE_LOG("TENew: dest=(%d,%d,%d,%d) view=(%d,%d,%d,%d)\n",
           destRect->top, destRect->left, destRect->bottom, destRect->right,
           viewRect->top, viewRect->left, viewRect->bottom, viewRect->right);

    /* Initialize TE if needed */
    if (!g_teInitialized) {
        TEInit();
    }

    /* Allocate TEExtRec */
    hTE = (TEHandle)NewHandleClear(sizeof(TEExtRec));
    if (!hTE) {
        TE_LOG("TENew: Failed to allocate TERec\n");
        return NULL;
    }

    /* Lock and initialize */
    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Initialize the record */
    TE_InitRecord(hTE, destRect, viewRect);

    /* Set default style */
    TE_SetDefaultStyle(hTE);

    /* Allocate initial text buffer */
    pTE->base.hText = NewHandle(TE_INITIAL_BUFFER);
    if (!pTE->base.hText) {
        HUnlock((Handle)hTE);
        DisposeHandle((Handle)hTE);
        TE_LOG("TENew: Failed to allocate text buffer\n");
        return NULL;
    }

    /* Allocate line starts array */
    pTE->hLines = NewHandle(sizeof(SInt32) * 32);  /* Room for 32 lines initially */
    if (!pTE->hLines) {
        DisposeHandle(pTE->base.hText);
        HUnlock((Handle)hTE);
        DisposeHandle((Handle)hTE);
        TE_LOG("TENew: Failed to allocate line starts\n");
        return NULL;
    }

    /* Initialize with one line at offset 0 */
    HLock(pTE->hLines);
    *((SInt32*)*pTE->hLines) = 0;
    HUnlock(pTE->hLines);
    pTE->nLines = 1;

    HUnlock((Handle)hTE);

    TE_LOG("TENew: Created TE handle %p\n", hTE);

    return hTE;
}

/*
 * TEStyleNew - Create a new multi-style TextEdit record
 */
TEHandle TEStyleNew(const Rect *destRect, const Rect *viewRect) {
    TEHandle hTE;
    TEExtPtr pTE;
    Handle hStyles;
    STRec* pStyles;

    TE_LOG("TEStyleNew: Creating styled TE\n");

    /* Create basic TE record */
    hTE = TENew(destRect, viewRect);
    if (!hTE) {
        return NULL;
    }

    /* Allocate style record */
    hStyles = NewHandleClear(sizeof(STRec));
    if (!hStyles) {
        TEDispose(hTE);
        return NULL;
    }

    /* Initialize style record */
    HLock(hStyles);
    pStyles = (STRec*)*hStyles;
    pStyles->nRuns = 0;
    pStyles->nStyles = 0;
    pStyles->styleTab = NewHandle(sizeof(TextStyle) * 16);  /* Initial style table */
    pStyles->runArray = NewHandle(sizeof(StyleRun) * 16);   /* Initial run array */
    pStyles->lineHeights = NewHandle(sizeof(LHElement) * 32); /* Line heights */
    HUnlock(hStyles);

    /* Attach to TE record */
    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;
    pTE->hStyles = hStyles;
    HUnlock((Handle)hTE);

    TE_LOG("TEStyleNew: Created styled TE handle %p\n", hTE);

    return hTE;
}

/*
 * TEDispose - Dispose of a TextEdit record
 */
void TEDispose(TEHandle hTE) {
    TEExtPtr pTE;

    if (!hTE) return;

    TE_LOG("TEDispose: Disposing TE handle %p\n", hTE);

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Dispose text buffer */
    if (pTE->base.hText) {
        DisposeHandle(pTE->base.hText);
    }

    /* Dispose line starts */
    if (pTE->hLines) {
        DisposeHandle(pTE->hLines);
    }

    /* Dispose style record if present */
    if (pTE->hStyles) {
        Handle hStyles = pTE->hStyles;
        STRec* pStyles;
        HLock(hStyles);
        pStyles = (STRec*)*hStyles;
        if (pStyles->styleTab) DisposeHandle(pStyles->styleTab);
        if (pStyles->runArray) DisposeHandle(pStyles->runArray);
        if (pStyles->lineHeights) DisposeHandle(pStyles->lineHeights);
        HUnlock(hStyles);
        DisposeHandle(hStyles);
    }

    HUnlock((Handle)hTE);
    DisposeHandle((Handle)hTE);
}

/* ============================================================================
 * Text Manipulation
 * ============================================================================ */

/*
 * TESetText - Set the text content
 */
void TESetText(const void *text, SInt32 length, TEHandle hTE) {
    TEExtPtr pTE;
    char *pText;

    if (!hTE || !text || length < 0) return;

    TE_LOG("TESetText: Setting %d bytes of text\n", length);

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Ensure buffer is large enough */
    if (length > GetHandleSize(pTE->base.hText)) {
        if (TE_GrowTextBuffer(hTE, length + TE_INITIAL_BUFFER) != noErr) {
            HUnlock((Handle)hTE);
            return;
        }
    }

    /* Copy text */
    HLock(pTE->base.hText);
    pText = *pTE->base.hText;
    BlockMove(text, pText, length);
    HUnlock(pTE->base.hText);

    /* Update length */
    pTE->base.teLength = length;

    /* Reset selection */
    pTE->base.selStart = 0;
    pTE->base.selEnd = 0;

    /* Mark dirty for recalc */
    pTE->dirty = TRUE;

    /* Recalculate lines */
    TE_RecalcLines(hTE);

    HUnlock((Handle)hTE);
}

/*
 * TEGetText - Get the text handle
 */
Handle TEGetText(TEHandle hTE) {
    TEExtPtr pTE;
    if (!hTE) return NULL;
    pTE = (TEExtPtr)*hTE;
    return pTE->base.hText;
}

/*
 * TEInsert - Insert text at current selection
 */
void TEInsert(const void *text, SInt32 length, TEHandle hTE) {
    TEReplaceSel(text, length, hTE);
}

/*
 * TEDelete - Delete current selection
 */
void TEDelete(TEHandle hTE) {
    TEReplaceSel("", 0, hTE);
}

/*
 * TEReplaceSel - Replace selection with text
 */
void TEReplaceSel(const void *text, SInt32 length, TEHandle hTE) {
    TEExtPtr pTE;
    char *pText;
    SInt32 selLen, newLen, moveLen;

    if (!hTE || length < 0) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Check read-only */
    if (pTE->readOnly) {
        HUnlock((Handle)hTE);
        return;
    }

    TE_LOG("TEReplaceSel: Replacing sel [%d,%d] with %d bytes\n",
           pTE->base.selStart, pTE->base.selEnd, length);

    /* Calculate sizes */
    selLen = pTE->base.selEnd - pTE->base.selStart;
    newLen = pTE->base.teLength - selLen + length;

    /* Check limit */
    if (newLen > teMaxLength) {
        HUnlock((Handle)hTE);
        return;
    }

    /* Grow buffer if needed */
    if (newLen > GetHandleSize(pTE->base.hText)) {
        if (TE_GrowTextBuffer(hTE, newLen + TE_INITIAL_BUFFER) != noErr) {
            HUnlock((Handle)hTE);
            return;
        }
    }

    /* Perform replacement */
    HLock(pTE->base.hText);
    pText = *pTE->base.hText;

    /* Move text after selection if needed */
    if (pTE->base.selEnd < pTE->base.teLength) {
        moveLen = pTE->base.teLength - pTE->base.selEnd;
        BlockMove(pText + pTE->base.selEnd,
                  pText + pTE->base.selStart + length,
                  moveLen);
    }

    /* Insert new text */
    if (text && length > 0) {
        BlockMove(text, pText + pTE->base.selStart, length);
    }

    HUnlock(pTE->base.hText);

    /* Update state */
    pTE->base.teLength = newLen;
    pTE->base.selStart = pTE->base.selStart + length;
    pTE->base.selEnd = pTE->base.selStart;
    pTE->dirty = TRUE;

    /* Recalculate lines */
    TE_RecalcLines(hTE);

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Selection Management
 * ============================================================================ */

/*
 * TESetSelect - Set selection range
 */
void TESetSelect(SInt32 selStart, SInt32 selEnd, TEHandle hTE) {
    TEExtPtr pTE;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Clamp to valid range */
    if (selStart < 0) selStart = 0;
    if (selStart > pTE->base.teLength) selStart = pTE->base.teLength;
    if (selEnd < 0) selEnd = 0;
    if (selEnd > pTE->base.teLength) selEnd = pTE->base.teLength;

    /* Ensure start <= end */
    if (selStart > selEnd) {
        SInt32 temp = selStart;
        selStart = selEnd;
        selEnd = temp;
    }

    TE_LOG("TESetSelect: [%d,%d] -> [%d,%d]\n",
           pTE->base.selStart, pTE->base.selEnd, selStart, selEnd);

    /* Invalidate old selection */
    TE_InvalidateSelection(hTE);

    /* Set new selection */
    pTE->base.selStart = selStart;
    pTE->base.selEnd = selEnd;

    /* Invalidate new selection */
    TE_InvalidateSelection(hTE);

    /* Reset caret blink */
    pTE->base.caretState = 0xFF;
    pTE->base.caretTime = TickCount();

    HUnlock((Handle)hTE);
}

/*
 * TEGetSelection - Get selection range
 */
void TEGetSelection(SInt32 *selStart, SInt32 *selEnd, TEHandle hTE) {
    TEExtPtr pTE;
    if (!hTE || !selStart || !selEnd) return;

    pTE = (TEExtPtr)*hTE;
    *selStart = pTE->base.selStart;
    *selEnd = pTE->base.selEnd;
}

/* ============================================================================
 * Activation
 * ============================================================================ */

/*
 * TEActivate - Activate TextEdit record
 */
void TEActivate(TEHandle hTE) {
    TEExtPtr pTE;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    TE_LOG("TEActivate: Activating TE\n");

    pTE->base.active = 1;
    pTE->base.caretState = 0xFF;
    pTE->base.caretTime = TickCount();

    /* Force caret visible */
    TE_UpdateCaret(hTE, TRUE);

    HUnlock((Handle)hTE);
}

/*
 * TEDeactivate - Deactivate TextEdit record
 */
void TEDeactivate(TEHandle hTE) {
    TEExtPtr pTE;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    TE_LOG("TEDeactivate: Deactivating TE\n");

    /* Hide caret */
    if (pTE->base.caretState) {
        TE_UpdateCaret(hTE, FALSE);
    }

    pTE->base.active = 0;
    pTE->base.caretState = 0;

    HUnlock((Handle)hTE);
}

/*
 * TEIsActive - Check if TextEdit is active
 */
Boolean TEIsActive(TEHandle hTE) {
    TEExtPtr pTE;
    if (!hTE) return FALSE;
    pTE = (TEExtPtr)*hTE;
    return pTE->base.active != 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/*
 * TESetJust - Set text justification
 */
void TESetJust(SInt16 just, TEHandle hTE) {
    TEExtPtr pTE;
    if (!hTE) return;

    TE_LOG("TESetJust: just=%d\n", just);

    pTE = (TEExtPtr)*hTE;
    pTE->base.just = just;
    pTE->dirty = TRUE;
}

/*
 * TESetWordWrap - Enable/disable word wrap
 */
void TESetWordWrap(Boolean wrap, TEHandle hTE) {
    TEExtPtr pTE;
    if (!hTE) return;

    TE_LOG("TESetWordWrap: wrap=%d\n", wrap);

    pTE = (TEExtPtr)*hTE;
    pTE->wordWrap = wrap;
    pTE->dirty = TRUE;
    TE_RecalcLines(hTE);
}

/* ============================================================================
 * Information
 * ============================================================================ */

/*
 * TEGetHeight - Get height of line range
 */
SInt16 TEGetHeight(SInt32 endLine, SInt32 startLine, TEHandle hTE) {
    TEExtPtr pTE;
    SInt16 height;

    if (!hTE) return 0;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Simple calculation for single-style */
    height = (endLine - startLine) * pTE->base.lineHeight;

    HUnlock((Handle)hTE);

    return height;
}

/*
 * TEGetLine - Get line number for offset
 */
SInt16 TEGetLine(SInt16 offset, TEHandle hTE) {
    return (SInt16)TE_OffsetToLine(hTE, offset);
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/*
 * TE_InitRecord - Initialize a TERec structure
 */
static void TE_InitRecord(TEHandle hTE, const Rect *destRect, const Rect *viewRect) {
    TEExtPtr pTE = (TEExtPtr)*hTE;

    /* Copy rectangles */
    pTE->base.destRect = *destRect;
    pTE->base.viewRect = *viewRect;

    /* Initialize state */
    pTE->base.active = 0;
    pTE->base.selStart = 0;
    pTE->base.selEnd = 0;
    pTE->base.teLength = 0;
    pTE->base.just = teJustLeft;
    pTE->wordWrap = TRUE;
    pTE->readOnly = FALSE;
    pTE->dirty = FALSE;

    /* Scrolling */
    pTE->viewDH = 0;
    pTE->viewDV = 0;

    /* Caret */
    pTE->base.caretState = 0;
    pTE->base.caretTime = 0;

    /* Lines */
    pTE->nLines = 0;

    /* Click tracking */
    pTE->clickCount = 0;
    pTE->lastClickTime = 0;
    pTE->base.clickLoc = 0;
    pTE->dragAnchor = 0;
    pTE->inDragSel = FALSE;
    pTE->autoViewEnabled = TRUE;

    /* Port */
    pTE->base.inPort = g_currentPort;

    /* CR only (classic Mac line endings) */
    pTE->base.crOnly = -1;
}

/*
 * TE_SetDefaultStyle - Set default text style
 */
static void TE_SetDefaultStyle(TEHandle hTE) {
    TEExtPtr pTE = (TEExtPtr)*hTE;

    /* Default to Chicago 12 */
    pTE->base.txFont = chicagoFont;
    pTE->base.txSize = 12;
    pTE->base.txFace = normal;

    /* Get font metrics */
    TextFont(pTE->base.txFont);
    TextSize(pTE->base.txSize);
    TextFace(pTE->base.txFace);

    /* Get line height and ascent from Font Manager */
    FMetricRec metrics;
    FMGetFontMetrics(&metrics);

    pTE->base.lineHeight = metrics.ascent + metrics.descent + metrics.leading;
    pTE->base.fontAscent = metrics.ascent;

    TE_LOG("Default style: font=%d size=%d height=%d ascent=%d\n",
           pTE->base.txFont, pTE->base.txSize, pTE->base.lineHeight, pTE->base.fontAscent);
}

/*
 * TE_GrowTextBuffer - Grow the text buffer
 */
static OSErr TE_GrowTextBuffer(TEHandle hTE, SInt32 newSize) {
    TEExtPtr pTE = (TEExtPtr)*hTE;

    TE_LOG("TE_GrowTextBuffer: Growing to %d bytes\n", newSize);

    SetHandleSize(pTE->base.hText, newSize);
    return MemError();
}

/* ============================================================================
 * Application Integration
 * ============================================================================ */

static Boolean g_textEditRunning = FALSE;
static TEHandle g_appTE = NULL;
static WindowPtr g_textEditWindow = NULL;

/*
 * TextEdit_InitApp - Initialize TextEdit as an application
 */
void TextEdit_InitApp(void) {
    if (!g_teInitialized) {
        TEInit();
    }
    g_textEditRunning = TRUE;

    /* Create window for TextEdit */
    if (!g_textEditWindow) {
        Rect windowBounds = {50, 50, 450, 650};
        Str255 title;
        /* Build Pascal string for "SimpleText" */
        strcpy((char*)&title[1], "SimpleText");
        title[0] = 10;  /* Pascal string length */
        g_textEditWindow = NewWindow(NULL, &windowBounds, title, TRUE,
                                     documentProc, (WindowPtr)-1L, TRUE, 0L);

        if (g_textEditWindow) {
            /* Create text edit record for the window */
            SetPort((GrafPtr)g_textEditWindow);
            Rect destRect = {10, 10, 390, 590};
            Rect viewRect = destRect;
            g_appTE = TENew(&destRect, &viewRect);

            if (g_appTE) {
                TEActivate(g_appTE);
                ShowWindow(g_textEditWindow);
                SelectWindow(g_textEditWindow);
            }
        }
    }

    TE_LOG("TextEdit_InitApp: Initialized with window\n");
}

/*
 * TextEdit_IsRunning - Check if TextEdit app is running
 */
Boolean TextEdit_IsRunning(void) {
    return g_textEditRunning;
}

/*
 * TextEdit_HandleEvent - Handle events for TextEdit app
 */
void TextEdit_HandleEvent(EventRecord* evt) {

    if (!g_appTE || !g_textEditWindow) return;

    /* Only handle events for our window */
    if (evt->what == updateEvt) {
        if ((WindowPtr)evt->message == g_textEditWindow) {
            BeginUpdate(g_textEditWindow);
            EraseRect(&((GrafPtr)g_textEditWindow)->portRect);
            TEUpdate(&((GrafPtr)g_textEditWindow)->portRect, g_appTE);
            EndUpdate(g_textEditWindow);
        }
    } else if (evt->what == mouseDown) {
        WindowPtr whichWindow;
        short part = FindWindow(evt->where, &whichWindow);

        if (whichWindow == g_textEditWindow && part == inContent) {
            /* Click in TextEdit window content */
            SelectWindow(g_textEditWindow);
            SetPort((GrafPtr)g_textEditWindow);
            Point pt = evt->where;
            GlobalToLocal(&pt);
            Boolean extendSelection = (evt->modifiers & shiftKey) != 0;
            TEClick(pt, extendSelection, g_appTE);
        }
    } else if ((evt->what == keyDown || evt->what == autoKey) &&
               FrontWindow() == g_textEditWindow) {
        /* Key press while TextEdit is front */
        char ch = evt->message & charCodeMask;
        TEKey(ch, g_appTE);
        /* Force update */
        InvalRect(&((GrafPtr)g_textEditWindow)->portRect);
    } else if (evt->what == activateEvt) {
        if ((WindowPtr)evt->message == g_textEditWindow) {
            if (evt->modifiers & activeFlag) {
                TEActivate(g_appTE);
            } else {
                TEDeactivate(g_appTE);
            }
        }
    } else if (evt->what == nullEvent && FrontWindow() == g_textEditWindow) {
        /* Idle for caret blinking */
        TEIdle(g_appTE);
    }
}

/*
 * TextEdit_LoadFile - Load a file into SimpleText
 */
void TextEdit_LoadFile(const char* path) {
    char buffer[32768];  /* 32KB buffer */
    const char* sampleText;

    /* Ensure window and TE exist */
    if (!g_textEditWindow || !g_appTE) {
        TextEdit_InitApp();
        if (!g_textEditWindow || !g_appTE) return;
    }

    /* Bring window to front */
    ShowWindow(g_textEditWindow);
    SelectWindow(g_textEditWindow);
    SetPort((GrafPtr)g_textEditWindow);

    /* Attempt to load file from VFS */
    {
        Handle fileBuffer = NULL;
        UInt32 fileSize = 0;
        Str255 pathPascal;
        FileRefNum refNum = -1;
        OSErr err;
        UInt32 bytesRead;

        /* Convert C string path to Pascal string for file system */
        size_t pathLen = strlen(path);
        if (pathLen > 255) pathLen = 255;
        pathPascal[0] = (unsigned char)pathLen;
        memcpy(&pathPascal[1], path, pathLen);

        /* Try to open the file */
        err = FSOpen(pathPascal, 0, &refNum);
        if (err == noErr && refNum >= 0) {
            /* Get file size */
            err = FSGetEOF(refNum, &fileSize);
            if (err == noErr && fileSize > 0 && fileSize < 1000000) {  /* Max 1MB text files */
                /* Allocate buffer for file content */
                fileBuffer = NewHandle(fileSize);
                if (fileBuffer) {
                    HLock(fileBuffer);
                    bytesRead = fileSize;
                    err = FSRead(refNum, &bytesRead, *fileBuffer);
                    HUnlock(fileBuffer);

                    if (err == noErr && bytesRead > 0) {
                        /* Successfully read file - set as text */
                        TESetText(*fileBuffer, bytesRead, g_appTE);
                        DisposeHandle(fileBuffer);
                        FSClose(refNum);
                        TE_LOG("TextEdit_LoadFile: Loaded %u bytes from %s\n", bytesRead, path);
                        goto update_title;
                    }
                    DisposeHandle(fileBuffer);
                }
            }
            FSClose(refNum);
        }

        /* If file loading failed, show sample text or error message */
        if (strstr(path, "Read Me") || strstr(path, "readme")) {
            sampleText = "Welcome to System 7.1!\r\r"
                        "This is a minimal implementation of a classic Mac OS-style system.\r\r"
                        "Features:\r"
                        "- Window Manager with draggable, resizable windows\r"
                        "- Finder with desktop icons and folder navigation\r"
                        "- TextEdit API for text editing\r"
                        "- SimpleText application for viewing text files\r\r"
                        "Double-click icons to open them.\r"
                        "Drag windows by their title bars.\r"
                        "Close windows with the close box.";
        } else if (strstr(path, "About")) {
            sampleText = "About This Macintosh\r\r"
                        "System 7.1 Compatible OS\r"
                        "Version 0.1\r\r"
                        "Memory: 4MB\r"
                        "Processor: x86\r\r"
                        "This system implements a subset of the\r"
                        "classic Macintosh Toolbox APIs.";
        } else {
            snprintf(buffer, sizeof(buffer), "SimpleText\r\r"
                    "Could not load file: %s\r\r"
                    "The file may not exist, or there was an error reading it.", path);
            TESetText(buffer, strlen(buffer), g_appTE);
            goto update_title;
        }

        /* Set the sample text */
        TESetText(sampleText, strlen(sampleText), g_appTE);
    }

update_title:
    /* Update window title with filename */
    const char* filename = path;
    const char* slash = path;
    while (*slash) {
        if (*slash == '/') filename = slash + 1;
        slash++;
    }

    Str255 title;
    size_t len = strlen(filename);
    if (len > 255) len = 255;
    title[0] = (unsigned char)len;
    memcpy(&title[1], filename, len);
    SetWTitle(g_textEditWindow, title);

    /* Force redraw */
    InvalRect(&((GrafPtr)g_textEditWindow)->portRect);

    TE_LOG("TextEdit_LoadFile: Loaded sample text for %s\n", path);
}

/* These functions are implemented in other TextEdit source files */
extern void TE_RecalcLines(TEHandle hTE);
extern SInt32 TE_OffsetToLine(TEHandle hTE, SInt32 offset);
extern void TE_InvalidateSelection(TEHandle hTE);
extern void TE_UpdateCaret(TEHandle hTE, Boolean forceOn);
