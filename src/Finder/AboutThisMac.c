/*
 * AboutThisMac.c - System 7-style "About This Macintosh" Window
 *
 * Implements a faithful modeless About window with memory statistics
 * and bar graph, matching classic Mac OS System 7.1 behavior.
 */

#include "SystemTypes.h"
#include <string.h>
#include "System71StdLib.h"
#include "Finder/FinderLogging.h"
#include "Finder/finder.h"
#include "QuickDraw/QuickDraw.h"
#include "Platform/platform_info.h"
#include "Gestalt/Gestalt.h"
#include "Platform/include/boot.h"

/* For debug logging */
extern void serial_printf(const char* fmt, ...);

extern void DisposeGWorld(GWorldPtr offscreenGWorld);
extern void* framebuffer;
extern uint32_t fb_pitch;
extern uint32_t fb_width;
extern uint32_t fb_height;

/* External QuickDraw & Window Manager APIs */
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern void BeginUpdate(WindowPtr window);
extern void EndUpdate(WindowPtr window);
extern void EraseRect(const Rect* r);
extern void FrameRect(const Rect* r);
extern void PaintRect(const Rect* r);
extern void InsetRect(Rect* r, short dh, short dv);
extern void MoveTo(short h, short v);
extern void LineTo(short h, short v);
extern void TextFont(short font);
extern void TextSize(short size);
extern void TextFace(short face);
extern void PenPat(const Pattern* pat);
extern void FillRect(const Rect* r, const Pattern* pat);
extern short StringWidth(ConstStr255Param s);

extern WindowPtr NewWindow(void* wStorage, const Rect* boundsRect,
                          const unsigned char* title, Boolean visible,
                          short procID, WindowPtr behind, Boolean goAwayFlag,
                          long refCon);
extern void ShowWindow(WindowPtr window);
extern void SelectWindow(WindowPtr window);
extern void BringToFront(WindowPtr window);
extern void DisposeWindow(WindowPtr window);
extern WindowPtr FrontWindow(void);
extern void DragWindow(WindowPtr window, Point startPt, const Rect* boundsRect);
extern Boolean TrackGoAway(WindowPtr window, Point pt);

/* External Event Manager */
#include "EventManager/EventManager.h"

/* External memory introspection - try to use real APIs if available */
extern Size FreeMem(void);
extern Size MaxMem(Size* grow);
extern UInt32 TotalRam(void);  /* Platform may provide this */
extern uint32_t g_total_memory_kb;  /* Actual detected RAM from multiboot2 */

/* Serial logging */
/* QuickDraw globals access */
extern QDGlobals qd;

/* Window definition constants */
#define documentProc 0
#define dBoxProc 1
#define plainDBox 2
#define altDBoxProc 3
#define noGrowDocProc 4
#define movableDBoxProc 5

/* Window part codes */
#define inDesk 0
#define inMenuBar 1
#define inSysWindow 2
#define inContent 3
#define inDrag 4
#define inGrow 5
#define inGoAway 6
#define inZoomIn 7
#define inZoomOut 8

/* Event types */
#define updateEvt 6

/* About window identification */
#define kAboutRefCon 0x41424F55L  /* 'ABOU' */

/* Static state - single instance only */
static WindowPtr sAboutWin = NULL;
static UInt32 sLastUpdateTicks = 0;
static Boolean sPendingShowOrToggle = 0;  /* Deferred window creation to avoid event re-entry deadlock */

/* Redraw throttle - only update stats every N ticks (avoid thrashing) */
#define kAboutUpdateThrottle 30  /* ~0.5 seconds at 60 ticks/sec */

/* Memory statistics snapshot */
typedef struct {
    UInt32 totalRAM;
    UInt32 systemUsed;
    UInt32 appUsed;
    UInt32 diskCache;
    UInt32 largestFree;
    UInt32 unused;
} MemSnapshot;

/* --- String helpers (use QuickDraw widths, not char counts) --- */
static void CenterPStringInRect(const Str255 s, const Rect* r, short baseline) {
    short w = StringWidth((ConstStr255Param)s);
    short x = r->left + ((r->right - r->left) - w) / 2;
    MoveTo(x, baseline);
    DrawString((ConstStr255Param)s);
}

static void RightAlignPStringAt(const Str255 s, short rightEdge, short baseline) {
    short w = StringWidth((ConstStr255Param)s);
    MoveTo(rightEdge - w, baseline);
    DrawString((ConstStr255Param)s);
}

/* Safe maker of P-strings from C strings */
static void ToPStr(const char* c, Str255 dst) {
    size_t n = 0;
    while (c[n] && n < 255) n++;
    dst[0] = (unsigned char)n;
    for (size_t i = 0; i < n; i++) {
        dst[1 + i] = (unsigned char)c[i];
    }
}

/* Clean KB formatter (no stdio, no junk) */
static void FormatKB_PStr(UInt32 bytes, Str255 outP) {
    UInt32 kb = bytes >> 10;
    char tmp[16];
    int n = 0;

    if (kb < 1000) {
        /* e.g., 638 for 638K */
        if (kb >= 100) tmp[n++] = '0' + (kb / 100) % 10;
        if (kb >= 10)  tmp[n++] = '0' + (kb / 10) % 10;
        tmp[n++] = '0' + (kb % 10);
        tmp[n++] = 'K';
    } else {
        /* e.g., 3,584K */
        UInt32 thousands = kb / 1000;
        UInt32 rem = kb % 1000;
        /* thousands (no leading zeros) */
        if (thousands >= 100) tmp[n++] = '0' + (thousands / 100) % 10;
        if (thousands >= 10)  tmp[n++] = '0' + (thousands / 10) % 10;
        tmp[n++] = '0' + (thousands % 10);
        tmp[n++] = ',';
        tmp[n++] = '0' + (rem / 100) % 10;
        tmp[n++] = '0' + (rem / 10) % 10;
        tmp[n++] = '0' + (rem % 10);
        tmp[n++] = 'K';
    }
    outP[0] = (unsigned char)n;
    for (int i = 0; i < n; i++) {
        outP[1 + i] = (unsigned char)tmp[i];
    }
}

/* Forward declarations */
static void AboutWindow_CreateIfNeeded(void);
static void GetMemorySnapshot(MemSnapshot* m);

/*
 * GetMemorySnapshot - Gather memory statistics
 *
 * Wired to real Memory Manager statistics when available.
 * Falls back to best-effort introspection if Memory Manager APIs unavailable.
 */
static void GetMemorySnapshot(MemSnapshot* m)
{
    Size grow = 0;
    UInt32 freeBytes = 0;
    (void)freeBytes;  /* Reserved for future use */
    UInt32 totalBytes = 0;

    if (!m) return;

    memset(m, 0, sizeof(MemSnapshot));

    /* Get total RAM from multiboot2 detection (g_total_memory_kb is in KB) */
    totalBytes = (UInt32)g_total_memory_kb * 1024;  /* Convert KB to bytes */
    if (totalBytes == 0) {
        totalBytes = 0x01000000;  /* Fallback: 16 MB if detection failed */
    }

    /* Try FreeMem() and MaxMem() if available - these call real Memory Manager */
    freeBytes = (UInt32)FreeMem();
    m->largestFree = (UInt32)MaxMem(&grow);

    if (m->largestFree == 0 || m->largestFree > totalBytes) {
        /* Fallback if Memory Manager APIs not working */
        m->largestFree = totalBytes / 4;  /* Assume 25% free */
    }

    m->totalRAM = totalBytes;

    /* Calculate actual system and application usage from free memory */
    /* System usage: approximately kernel + driver buffers + Finder heap */
    UInt32 usedTotal = totalBytes - m->largestFree;

    /* Estimate system used as fraction of total (typically 12-20% on classic Mac) */
    m->systemUsed = (totalBytes / 10);  /* ~10% for kernel + drivers + Finder */

    /* Application usage is used memory minus system memory */
    if (usedTotal > m->systemUsed) {
        m->appUsed = usedTotal - m->systemUsed;
    } else {
        m->appUsed = totalBytes / 8;  /* Fallback estimate */
    }

    if (m->appUsed > totalBytes) {
        m->appUsed = totalBytes / 4;
    }

    /* Disk cache - wired to actual cache manager when implemented */
    /* For now, cache is included in application memory */
    m->diskCache = 0;

    /* Compute unused (remaining free memory) */
    if (m->systemUsed + m->appUsed + m->diskCache < m->totalRAM) {
        m->unused = m->totalRAM - m->systemUsed - m->appUsed - m->diskCache;
    } else {
        m->unused = 0;
    }

    FINDER_LOG_DEBUG("AboutThisMac: Memory snapshot - Total:%u System:%u App:%u Cache:%u Free:%u Unused:%u\n",
                     (unsigned int)m->totalRAM,
                     (unsigned int)m->systemUsed,
                     (unsigned int)m->appUsed,
                     (unsigned int)m->diskCache,
                     (unsigned int)m->largestFree,
                     (unsigned int)m->unused);
}

/*
 * FormatKBWithCommas - Format bytes as KB with thousands separators
 */
__attribute__((unused))
static void FormatKBWithCommas(UInt32 bytes, char* buf, size_t bufSize)
{
    UInt32 kb = bytes / 1024;
    UInt32 thousands, remainder;
    char temp[32];
    int i, j;

    if (!buf || bufSize < 16) return;

    if (kb < 1000) {
        /* Simple format */
        FINDER_LOG_DEBUG("FormatKB: %uK\n", (unsigned int)kb);
        temp[0] = '0' + (char)((kb / 100) % 10);
        temp[1] = '0' + (char)((kb / 10) % 10);
        temp[2] = '0' + (char)(kb % 10);
        temp[3] = 'K';
        temp[4] = '\0';
        /* Copy non-leading zeros */
        j = 0;
        for (i = 0; temp[i] == '0' && i < 2; i++);
        for (; temp[i]; i++) buf[j++] = temp[i];
        buf[j] = '\0';
    } else {
        /* Format with comma separator */
        thousands = kb / 1000;
        remainder = kb % 1000;
        /* Manual formatting to avoid sprintf */
        i = 0;
        if (thousands >= 100) temp[i++] = '0' + (char)(thousands / 100);
        if (thousands >= 10) temp[i++] = '0' + (char)((thousands / 10) % 10);
        temp[i++] = '0' + (char)(thousands % 10);
        temp[i++] = ',';
        temp[i++] = '0' + (char)(remainder / 100);
        temp[i++] = '0' + (char)((remainder / 10) % 10);
        temp[i++] = '0' + (char)(remainder % 10);
        temp[i++] = 'K';
        temp[i] = '\0';
        strcpy(buf, temp);
    }
}

/*
 * DrawLabeledValue - Draw "Label: XXX,XXXK" line
 *
 * Calculates colon position for proper alignment and separation of
 * label and value. Label width determines where colon is drawn.
 */

/*
 * AboutWindow_UpdateFramebufferAddress - Recalculate baseAddr after window moves
 */
static void AboutWindow_UpdateFramebufferAddress(void) {
    if (!sAboutWin) return;

    /* Get current window content position in global coordinates */
    /* Use window's content region for accurate global position */
    if (!sAboutWin->contRgn || !*(sAboutWin->contRgn)) return;

    Rect contentGlobal = (*(sAboutWin->contRgn))->rgnBBox;
    SInt16 contentTop = contentGlobal.top;
    SInt16 contentLeft = contentGlobal.left;

    /* Clamp to screen bounds to prevent wrapping */
    extern QDGlobals qd;
    if (contentTop < 0) contentTop = 0;
    if (contentLeft < 0) contentLeft = 0;
    if (contentTop >= qd.screenBits.bounds.bottom) contentTop = qd.screenBits.bounds.bottom - 1;
    if (contentLeft >= qd.screenBits.bounds.right) contentLeft = qd.screenBits.bounds.right - 1;

    /* Recalculate framebuffer offset */
    uint32_t bytes_per_pixel = 4;
    uint32_t offset = contentTop * fb_pitch + contentLeft * bytes_per_pixel;

    /* Update baseAddr to new window position */
    sAboutWin->port.portBits.baseAddr = (Ptr)framebuffer + offset;

    serial_puts("[ABOUT] Updated baseAddr after window move\n");
}

/*
 * AboutWindow_CreateIfNeeded - Create About window if not already open
 */
static void AboutWindow_CreateIfNeeded(void)
{
    Rect bounds;
    unsigned char title[32];

    serial_puts("[ABOUT] CreateIfNeeded: ENTRY\n");

    if (sAboutWin) {
        serial_puts("[ABOUT] CreateIfNeeded: Window already exists, returning\n");
        FINDER_LOG_DEBUG("AboutThisMac: Window already exists at 0x%08x\n", (unsigned int)P2UL(sAboutWin));
        return;
    }

    /* Position window centered-ish on screen (classic About box position) */
    bounds.top = 70;
    bounds.left = 80;
    bounds.bottom = bounds.top + 280;  /* Height: 280 pixels */
    bounds.right = bounds.left + 540;  /* Width: 540 pixels */

    /* Create window with movable dialog box proc, close box enabled */
    const char* titleText = "About This Macintosh";
    title[0] = (unsigned char)strlen(titleText);
    strcpy((char*)&title[1], titleText);

    serial_puts("[ABOUT] CreateIfNeeded: About to call NewWindow\n");

    sAboutWin = NewWindow(NULL, &bounds, title,
                         1,  /* visible */
                         movableDBoxProc,  /* Movable dialog box */
                         (WindowPtr)-1,  /* Behind all windows initially */
                         1,  /* Has close box */
                         kAboutRefCon);

    serial_puts("[ABOUT] CreateIfNeeded: NewWindow returned\n");

    if (!sAboutWin) {
        serial_puts("[ABOUT] CreateIfNeeded: FAILED - window is NULL\n");
        FINDER_LOG_DEBUG("AboutThisMac: FAILED to create window!\n");
        return;
    }

    serial_puts("[ABOUT] CreateIfNeeded: Configuring direct framebuffer rendering\n");

    /* Dispose of offscreenGWorld if it exists */
    if (sAboutWin->offscreenGWorld) {
        serial_puts("[ABOUT] Disposing offscreenGWorld\n");
        DisposeGWorld((GWorldPtr)sAboutWin->offscreenGWorld);
        sAboutWin->offscreenGWorld = NULL;
    }

    /* CRITICAL FIX: Configure direct framebuffer rendering
     * Since baseAddr will point to window's position in framebuffer,
     * bounds must be in local coordinates (0,0,width,height) NOT global coordinates.
     * This prevents FM_DrawChicagoCharInternal from double-adjusting coordinates. */
    Rect originalBounds = sAboutWin->port.portBits.bounds;
    uint32_t bytes_per_pixel = 4;  /* ARGB32 */
    uint32_t offset = originalBounds.top * fb_pitch + originalBounds.left * bytes_per_pixel;

    serial_puts("[ABOUT] Configuring direct framebuffer rendering\n");

    /* Set baseAddr to point to window's content area in framebuffer */
    sAboutWin->port.portBits.baseAddr = (Ptr)framebuffer + offset;
    sAboutWin->port.portBits.rowBytes = (fb_pitch | 0x8000);

    /* Set bounds to LOCAL coordinates (0,0,width,height) since baseAddr is already offset
     * This makes FM_DrawChicagoCharInternal treat it like an offscreen buffer at origin */
    SInt16 width = originalBounds.right - originalBounds.left;
    SInt16 height = originalBounds.bottom - originalBounds.top;

    sAboutWin->port.portBits.bounds.left = 0;
    sAboutWin->port.portBits.bounds.top = 0;
    sAboutWin->port.portBits.bounds.right = width;
    sAboutWin->port.portBits.bounds.bottom = height;

    /* Ensure portRect also matches the content dimensions
     * portRect should already be set correctly, but verify it matches */
    if (sAboutWin->port.portRect.right != width || sAboutWin->port.portRect.bottom != height) {
        serial_puts("[ABOUT] WARNING: portRect dimensions don't match, fixing\n");
        sAboutWin->port.portRect.left = 0;
        sAboutWin->port.portRect.top = 0;
        sAboutWin->port.portRect.right = width;
        sAboutWin->port.portRect.bottom = height;
    }

    serial_puts("[ABOUT] Direct framebuffer configured\n");

    serial_puts("[ABOUT] CreateIfNeeded COMPLETE\n");
    FINDER_LOG_DEBUG("AboutThisMac: Created window at 0x%08x, refCon=0x%08X\n",
                     (unsigned int)P2UL(sAboutWin), (unsigned int)kAboutRefCon);
}

/*
 * AboutWindow_ProcessPendingCreation - Actually create the About window
 *
 * Called from main event loop after event dispatch is complete.
 * This deferred approach prevents event re-entry deadlock.
 */
void AboutWindow_ProcessPendingCreation(void)
{
    if (!sPendingShowOrToggle) {
        return;  /* Nothing to do */
    }

    serial_puts("[ABOUT] ProcessPending: Creating deferred window\n");
    sPendingShowOrToggle = 0;

    if (!sAboutWin) {
        serial_puts("[ABOUT] ProcessPending: Calling AboutWindow_CreateIfNeeded\n");
        AboutWindow_CreateIfNeeded();
    }

    if (!sAboutWin) {
        serial_puts("[ABOUT] ProcessPending: FAILED - window is NULL\n");
        return;
    }

    serial_puts("[ABOUT] ProcessPending: Bringing window to front\n");
    /* Bring to front and select */
    BringToFront(sAboutWin);
    SelectWindow(sAboutWin);

    /* Request update */
    PostEvent(updateEvt, (UInt32)(uintptr_t)sAboutWin);

    serial_puts("[ABOUT] ProcessPending: Window shown successfully\n");
}

/*
 * AboutWindow_ShowOrToggle - Request About window to be shown
 *
 * Public API called from Apple menu handler.
 * DEFERRED: Just sets a flag; actual window creation happens in main loop.
 * This prevents event re-entry deadlock from NewWindow posting events.
 */
void AboutWindow_ShowOrToggle(void)
{
    serial_puts("[ABOUT] ShowOrToggle: CALLED (deferred)\n");
    FINDER_LOG_DEBUG("AboutThisMac: ShowOrToggle called\n");

    if (sAboutWin) {
        /* Window already exists - bring to front immediately */
        serial_puts("[ABOUT] ShowOrToggle: Window exists, bringing to front\n");
        BringToFront(sAboutWin);
        SelectWindow(sAboutWin);
        PostEvent(updateEvt, (UInt32)(uintptr_t)sAboutWin);
        FINDER_LOG_DEBUG("AboutThisMac: Window already shown\n");
        return;
    }

    /* Request deferred creation (avoids event re-entry) */
    serial_puts("[ABOUT] ShowOrToggle: Requesting deferred window creation\n");
    sPendingShowOrToggle = 1;
    FINDER_LOG_DEBUG("AboutThisMac: Deferred window creation requested\n");
}

/*
 * AboutWindow_CloseIf - Close About window if it matches
 */
void AboutWindow_CloseIf(WindowPtr w)
{
    FINDER_LOG_DEBUG("AboutThisMac: CloseIf ENTRY, w=0x%08x, sAboutWin=0x%08x\n",
                     (unsigned int)P2UL(w), (unsigned int)P2UL(sAboutWin));

    if (!w || w != sAboutWin) {
        FINDER_LOG_DEBUG("AboutThisMac: CloseIf - window mismatch, returning\n");
        return;
    }

    FINDER_LOG_DEBUG("AboutThisMac: Closing window\n");

    FINDER_LOG_DEBUG("AboutThisMac: About to call DisposeWindow\n");
    DisposeWindow(sAboutWin);
    FINDER_LOG_DEBUG("AboutThisMac: DisposeWindow returned\n");
    sAboutWin = NULL;
    FINDER_LOG_DEBUG("AboutThisMac: CloseIf EXIT\n");
}

/*
 * AboutWindow_HandleUpdate - Redraw About window contents
 *
 * Called from event dispatcher on updateEvt.
 * Returns true if handled.
 */
Boolean AboutWindow_HandleUpdate(WindowPtr w)
{
    GrafPtr savedPort;
    MemSnapshot mem;
    Rect box, bar, seg;
    short wSys, wApp, wCac;  /* Memory segment widths for bar graph */
    UInt32 currentTicks;
    extern UInt32 TickCount(void);

    if (!w || w != sAboutWin) {
        return 0;  /* false */
    }

    FINDER_LOG_DEBUG("AboutThisMac: HandleUpdate called\n");

    /* Save current port and set to our window */
    GetPort(&savedPort);
    SetPort((GrafPtr)w);

    serial_puts("[ABOUT] UPDATE: Before BeginUpdate\n");

    /* Update baseAddr in case window was moved */
    AboutWindow_UpdateFramebufferAddress();

    BeginUpdate(w);

    serial_puts("[ABOUT] UPDATE: After BeginUpdate\n");

    /* Debug: log port configuration */
    extern void serial_printf(const char* fmt, ...);
    static int update_debug = 0;
    if (update_debug < 1) {
        serial_printf("[ABOUT-DEBUG] portRect=(%d,%d,%d,%d) portBits.bounds=(%d,%d,%d,%d)\n",
                     w->port.portRect.left, w->port.portRect.top,
                     w->port.portRect.right, w->port.portRect.bottom,
                     w->port.portBits.bounds.left, w->port.portBits.bounds.top,
                     w->port.portBits.bounds.right, w->port.portBits.bounds.bottom);
        update_debug++;
    }

    /* Optional throttle: only recompute stats every N ticks to reduce overhead */
    currentTicks = TickCount();
    if (currentTicks - sLastUpdateTicks < kAboutUpdateThrottle) {
        /* Still draw, but use cached stats - for now just note it */
        FINDER_LOG_DEBUG("AboutThisMac: Throttled update (delta=%u ticks)\n",
                         (unsigned int)(currentTicks - sLastUpdateTicks));
    }
    sLastUpdateTicks = currentTicks;

    /* Get window content rect - use local coordinates (0,0) origin
     * Use portRect which has the correct content dimensions */
    Rect contentRect;
    contentRect.top = 0;
    contentRect.left = 0;
    contentRect.right = w->port.portRect.right;
    contentRect.bottom = w->port.portRect.bottom;

    serial_puts("[ABOUT] Drawing content\n");

    /* Clear */
    EraseRect(&contentRect);

    /* Get platform info for display */
    const char *platform_name = platform_get_display_name();
    const char *model_string = platform_get_model_string();
    const char *memory_gb = platform_format_memory_gb();

    serial_puts("[ABOUT] About to draw title\n");

    /* Title: Platform name (detected at boot) - Chicago (System), 12pt, centered */
    Str255 title;
    TextFont(0);            /* System (Chicago) */
    TextSize(12);           /* Use 12pt to avoid scaling */
    TextFace(0);            /* normal */
    ToPStr(platform_name, title);

    serial_puts("[ABOUT] Calling CenterPStringInRect\n");
    CenterPStringInRect(title, &contentRect, contentRect.top + 20);
    serial_puts("[ABOUT] Title drawn\n");

    /* Version and Memory: "System 7 - X GB" - Chicago 11, normal */
    Str255 ver;
    char ver_buf[64];
    sprintf(ver_buf, "System 7.1 - %s", memory_gb);
    TextSize(11);
    TextFace(0);            /* normal */
    ToPStr(ver_buf, ver);
    CenterPStringInRect(ver, &contentRect, contentRect.top + 40);

#if defined(__powerpc__) || defined(__powerpc64__)
    long memPtrLong = 0;
    if (Gestalt(gestaltMemoryMap, &memPtrLong) == noErr && memPtrLong != 0) {
        long *memInfo = (long *)(uintptr_t)memPtrLong;
        uint32_t range_count = (uint32_t)memInfo[0];
        if (range_count > 0) {
            uint32_t base_hi = (uint32_t)memInfo[1];
            uint32_t base_lo = (uint32_t)memInfo[2];
            uint32_t size_hi = (uint32_t)memInfo[3];
            uint32_t size_lo = (uint32_t)memInfo[4];
            uint64_t size_bytes = ((uint64_t)size_hi << 32) | size_lo;
            uint32_t size_mb = (uint32_t)(size_bytes / (1024 * 1024));

            char map_buf[80];
            if (base_hi != 0) {
                sprintf(map_buf, "FW range0: 0x%08x%08x (%u MB)", base_hi, base_lo, size_mb);
            } else {
                sprintf(map_buf, "FW range0: 0x%08x (%u MB)", base_lo, size_mb);
            }
            Str255 extra;
            TextSize(10);
            TextFace(0);
            ToPStr(map_buf, extra);
            CenterPStringInRect(extra, &contentRect, contentRect.top + 60);

            char count_buf[40];
            sprintf(count_buf, "Firmware ranges: %u", range_count);
            ToPStr(count_buf, extra);
            CenterPStringInRect(extra, &contentRect, contentRect.top + 72);
        }
    }
#endif

    /* Model info: model string - Chicago 10, normal */
    if (model_string && model_string[0] != '\0') {
        Str255 model_pstr;
        TextSize(10);
        ToPStr(model_string, model_pstr);
        CenterPStringInRect(model_pstr, &contentRect, contentRect.top + 53);
    }

    /* Get current memory statistics */
    GetMemorySnapshot(&mem);

    /* Body text: Geneva 9 for labels/values */
    TextFont(3);           /* Geneva */
    TextSize(9);
    TextFace(0);           /* normal */

    /* Memory box */
    box.top    = contentRect.top  + 60;
    box.left   = contentRect.left + 20;
    box.right  = contentRect.right - 20;
    box.bottom = box.top + 152;
    FrameRect(&box);

    /* Labels left; values right aligned */
    const short leftCol  = box.left  + 10;
    const short rightCol = box.right - 70;

    /* "Memory" label */
    Str255 memLbl;
    ToPStr("Memory", memLbl);
    MoveTo(leftCol, box.top + 14);
    DrawString(memLbl);
    short y = box.top + 32;             /* first line baseline */
    const short dy = 14;                 /* line spacing */

    /* Built-in Memory */
    Str255 l1, v1;
    ToPStr("Built-in Memory", l1);
    MoveTo(leftCol, y);
    DrawString(l1);
    FormatKB_PStr(mem.totalRAM, v1);
    RightAlignPStringAt(v1, rightCol, y);

    /* Application Memory */
    y += dy;
    Str255 l2, v2;
    ToPStr("Application Memory", l2);
    MoveTo(leftCol, y);
    DrawString(l2);
    FormatKB_PStr(mem.appUsed, v2);
    RightAlignPStringAt(v2, rightCol, y);

    /* Largest Unused Block */
    y += dy;
    Str255 l3, v3;
    ToPStr("Largest Unused Block", l3);
    MoveTo(leftCol, y);
    DrawString(l3);
    FormatKB_PStr(mem.largestFree, v3);
    RightAlignPStringAt(v3, rightCol, y);

    /* Bar rect under stats */
    bar.left  = box.left + 10;
    bar.right = box.right - 30;
    bar.top   = box.top + 80;           /* looks right under three lines */
    bar.bottom= bar.top + 12;
    FrameRect(&bar);

    /* Compute widths (clamped) */
    UInt32 totalKB = mem.totalRAM >> 10;
    UInt32 sysKB   = mem.systemUsed >> 10;
    UInt32 appKB   = mem.appUsed >> 10;
    UInt32 cacheKB = mem.diskCache >> 10;

    if (totalKB <= 0) totalKB = 1;
    UInt32 usedKB = sysKB + appKB + cacheKB;
    if (usedKB > totalKB) usedKB = totalKB;

    short wTotal = bar.right - bar.left;
    wSys   = (short)((sysKB   * wTotal) / totalKB);
    wApp   = (short)((appKB   * wTotal) / totalKB);
    wCac   = (short)((cacheKB * wTotal) / totalKB);

    /* Segment rects */
    seg = bar;

    /* System */
    seg.right = seg.left + wSys;
    PenPat(&qd.dkGray);
    PaintRect(&seg);

    /* Applications */
    seg.left  = seg.right;
    seg.right = seg.left + wApp;
    PenPat(&qd.gray);
    PaintRect(&seg);

    /* Disk Cache (optional, draw only if >0) */
    if (wCac > 0) {
        seg.left  = seg.right;
        seg.right = seg.left + wCac;
        PenPat(&qd.ltGray);
        PaintRect(&seg);
    }

    /* Unused stays white - no fill needed; reframe to restore crisp outline */
    FrameRect(&bar);

    /* Labels below bar */
    TextFont(3);
    TextSize(9);
    Str255 sysS, appS, unusedS;
    ToPStr("System", sysS);
    ToPStr("Applications", appS);
    ToPStr("Unused", unusedS);
    MoveTo(bar.left,  bar.bottom + 12);
    DrawString(sysS);
    MoveTo(bar.left + 60, bar.bottom + 12);
    DrawString(appS);
    RightAlignPStringAt(unusedS, bar.right, bar.bottom + 12);

    /* Footer line - Geneva 10, move higher up */
    Str255 cpy;
    TextFont(3);           /* Geneva */
    TextSize(10);          /* Slightly larger for visibility */
    TextFace(0);           /* normal */
    ToPStr("2025 Kelsi Davis", cpy);
    /* Place below memory box - move higher to ensure visibility */
    short cpyY = box.bottom + 18;
    FINDER_LOG_DEBUG("Drawing footer at Y=%d: '2025 Kelsi Davis'\n", cpyY);
    MoveTo(contentRect.left + ((contentRect.right - contentRect.left) - StringWidth((ConstStr255Param)cpy)) / 2, cpyY);
    DrawString((ConstStr255Param)cpy);
    FINDER_LOG_DEBUG("Footer drawn\n");

    EndUpdate(w);

    /* Restore previous port */
    SetPort(savedPort);

    FINDER_LOG_DEBUG("AboutThisMac: Update complete\n");

    return 1;  /* true - handled */
}

/*
 * AboutWindow_HandleMouseDown - Handle mouse clicks in About window
 *
 * Called from event dispatcher on mouseDown in About window.
 * Returns true if handled.
 */
Boolean AboutWindow_HandleMouseDown(WindowPtr w, short part, Point localPt)
{
    Rect dragBounds;

    if (!w || w != sAboutWin) {
        return 0;  /* false */
    }

    FINDER_LOG_DEBUG("AboutThisMac: HandleMouseDown, part=%d\n", part);

    switch (part) {
        case inDrag:
            FINDER_LOG_DEBUG("AboutThisMac: In inDrag case\n");
            /* Select window before dragging (Mac OS standard behavior) */
            SelectWindow(w);
            /* Allow window to be dragged */
            dragBounds = qd.screenBits.bounds;

            /* Expand horizontal bounds so the window can slide partially off-screen */
            if (w->strucRgn && *w->strucRgn) {
                Rect frame = (*w->strucRgn)->rgnBBox;
                short windowWidth = frame.right - frame.left;
                short windowHeight = frame.bottom - frame.top;

                /* Allow roughly half the window to leave each edge */
                short horizontalSlack = windowWidth / 2;
                short verticalSlack = windowHeight / 2;

                dragBounds.left -= horizontalSlack;
                dragBounds.right += horizontalSlack;
                dragBounds.bottom += verticalSlack;
            }

            /* Never allow the title bar to cross the menu bar */
            if (dragBounds.top < 20) {
                dragBounds.top = 20;
            }

            DragWindow(w, localPt, &dragBounds);
            return 1;  /* true */

        case inGoAway:
            FINDER_LOG_DEBUG("AboutThisMac: In inGoAway case\n");
            /* Handle close box - TrackGoAway hangs, so just close directly */
            FINDER_LOG_DEBUG("AboutThisMac: Close box clicked, closing window\n");
            AboutWindow_CloseIf(w);
            FINDER_LOG_DEBUG("AboutThisMac: After AboutWindow_CloseIf\n");
            return 1;  /* true */

        case inContent:
            /* Just select window if not already front */
            if (FrontWindow() != w) {
                SelectWindow(w);
            }
            return 1;  /* true */

        default:
            break;
    }

    return 0;  /* false - not handled */
}

/*
 * AboutWindow_IsOurs - Check if window is the About window
 *
 * Utility for event dispatcher to identify About window.
 */
Boolean AboutWindow_IsOurs(WindowPtr w)
{
    return (w && w == sAboutWin && w->refCon == kAboutRefCon) ? 1 : 0;
}
