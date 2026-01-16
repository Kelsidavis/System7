/*
 * TextEditTest.c - TextEdit Test Window
 *
 * Creates a simple window with TextEdit field for testing
 */

#include "TextEdit/TextEdit.h"
#include "WindowManager/WindowManager.h"
#include "EventManager/EventManager.h"
#include "QuickDraw/QuickDraw.h"

/* Boolean constants */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Extended TERec with additional fields - must match TextEdit.c */
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
#include "MemoryMgr/MemoryManager.h"
#include <string.h>
#include "TextEdit/TELogging.h"

/* Public test harness entry points */
void TETestInit(void);
void TETestHandleEvent(EventRecord *event);
void TETestRun(void);
void TETestCleanup(void);

/* Debug logging */
#define TEST_LOG(...) TE_LOG_DEBUG("TETest: " __VA_ARGS__)

/* Constants */
#define kTestWindowID   128
#define kTextMargin     10

/* Globals */
static WindowPtr g_testWindow = NULL;
static TEHandle g_testTE = NULL;

/* Forward declarations */
static void CreateTestWindow(void);
static void DrawTestWindow(void);

/* ============================================================================
 * Test Window Creation
 * ============================================================================ */

/*
 * TETestInit - Initialize TextEdit test window
 */
void TETestInit(void) {
    TEST_LOG("Initializing TextEdit test\n");

    /* Initialize TextEdit */
    TEInit();

    /* Create test window */
    CreateTestWindow();
}

/*
 * CreateTestWindow - Create a window with TextEdit field
 */
static void CreateTestWindow(void) {
    Rect bounds;
    Rect destRect, viewRect;
    static unsigned char title[] = {14, 'T','e','x','t','E','d','i','t',' ','T','e','s','t'};
    char *sampleText = "Welcome to TextEdit!\r\rType here to test text editing.\r"
                       "Try selecting text with the mouse.\r"
                       "Use arrow keys to navigate.\r"
                       "Cut, copy, and paste with Cmd-X, Cmd-C, Cmd-V.";

    /* Create window */
    SetRect(&bounds, 100, 100, 500, 400);
    g_testWindow = NewWindow(NULL, &bounds, title, TRUE,
                            documentProc, (WindowPtr)-1L, TRUE, 0L);

    if (!g_testWindow) {
        TEST_LOG("Failed to create test window\n");
        return;
    }

    /* Set up TextEdit rectangles */
    destRect = bounds;
    destRect.left += kTextMargin;
    destRect.right -= kTextMargin;
    destRect.top += kTextMargin;
    destRect.bottom -= kTextMargin;
    viewRect = destRect;

    /* Create TextEdit record */
    SetPort((GrafPtr)g_testWindow);
    g_testTE = TENew(&destRect, &viewRect);

    if (!g_testTE) {
        TEST_LOG("Failed to create TextEdit record\n");
        DisposeWindow(g_testWindow);
        g_testWindow = NULL;
        return;
    }

    /* Set sample text */
    TESetText(sampleText, strlen(sampleText), g_testTE);

    /* Activate TE */
    TEActivate(g_testTE);

    TEST_LOG("Created test window with TextEdit\n");
}

/* ============================================================================
 * Event Handling
 * ============================================================================ */

/*
 * TETestHandleEvent - Handle events for test window
 */
void TETestHandleEvent(EventRecord *event) {
    WindowPtr window;
    Point pt;
    char ch;

    if (!g_testWindow || !g_testTE) return;

    switch (event->what) {
        case mouseDown:
            /* Find which window */
            if (FindWindow(event->where, &window) == inContent) {
                if (window == g_testWindow) {
                    /* Convert to local coordinates */
                    pt = event->where;
                    GlobalToLocal(&pt);

                    /* Check for shift key */
                    Boolean extendSelection = (event->modifiers & shiftKey) != 0;

                    /* Handle click in TE */
                    TEClick(pt, extendSelection, g_testTE);
                }
            }
            break;

        case keyDown:
        case autoKey:
            /* Handle key if our window is active */
            if (FrontWindow() == g_testWindow) {
                ch = event->message & charCodeMask;

                /* Check for command key */
                if (event->modifiers & cmdKey) {
                    switch (ch) {
                        case 'x':
                        case 'X':
                            TECut(g_testTE);
                            break;
                        case 'c':
                        case 'C':
                            TECopy(g_testTE);
                            break;
                        case 'v':
                        case 'V':
                            TEPaste(g_testTE);
                            break;
                        case 'a':
                        case 'A':
                            /* Select all */
                            TESetSelect(0, (*g_testTE)->teLength, g_testTE);
                            break;
                        default:
                            /* Other command - pass to TE */
                            TEKey(ch, g_testTE);
                            break;
                    }
                } else {
                    /* Regular key */
                    TEKey(ch, g_testTE);
                }
            }
            break;

        case updateEvt:
            /* Check if it's our window */
            window = (WindowPtr)(uintptr_t)event->message;
            if (window == g_testWindow) {
                DrawTestWindow();
            }
            break;

        case activateEvt:
            /* Handle activation */
            if ((WindowPtr)(uintptr_t)event->message == g_testWindow) {
                if (event->modifiers & activeFlag) {
                    TEActivate(g_testTE);
                } else {
                    TEDeactivate(g_testTE);
                }
            }
            break;

        case nullEvent:
            /* Handle idle for caret blinking */
            if (FrontWindow() == g_testWindow) {
                TEIdle(g_testTE);
            }
            break;
    }
}

/*
 * DrawTestWindow - Draw test window content
 */
static void DrawTestWindow(void) {
    Rect updateRect;

    if (!g_testWindow || !g_testTE) return;

    SetPort((GrafPtr)g_testWindow);

    /* Begin update */
    BeginUpdate(g_testWindow);

    /* Get update region */
    SetRect(&updateRect, 0, 0, 640, 480);  /* Use window dimensions */

    /* Draw TE content */
    TEUpdate(&updateRect, g_testTE);

    /* End update */
    EndUpdate(g_testWindow);
}

/* ============================================================================
 * Test Integration
 * ============================================================================ */

/*
 * TETestRun - Run TextEdit test (call from main event loop)
 */
void TETestRun(void) {
    EventRecord event;

    /* Process events */
    if (GetNextEvent(everyEvent, &event)) {
        TETestHandleEvent(&event);
    } else {
        /* Null event - handle idle */
        event.what = nullEvent;
        TETestHandleEvent(&event);
    }
}

/*
 * TETestCleanup - Clean up test window
 */
void TETestCleanup(void) {
    if (g_testTE) {
        TEDispose(g_testTE);
        g_testTE = NULL;
    }

    if (g_testWindow) {
        DisposeWindow(g_testWindow);
        g_testWindow = NULL;
    }

    TEST_LOG("Cleaned up TextEdit test\n");
}
