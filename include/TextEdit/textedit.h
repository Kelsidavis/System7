/* RE-AGENT-BANNER
 * TextEdit Header - Apple System 7.1 TextEdit Manager
 *
 * Reverse engineered from: System.rsrc
 * SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba
 * Architecture: m68k (68000)
 * ABI: classic_macos
 *
 * Evidence: evidence.curated.textedit.json
 * Mappings: mappings.textedit.json
 * Layouts: layouts.curated.textedit.json
 *
 * TextEdit is the Mac OS Toolbox component responsible for text editing
 * functionality. It provides text input, editing, display, and clipboard
 * operations for applications. This implementation is based on System 7.1.
 */

#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <stdint.h>

/* Forward declarations */
typedef struct TERec TERec;
typedef TERec *TEPtr;
typedef TEPtr *TEHandle;
typedef char **CharsHandle;

/* Basic Mac OS types */
typedef struct {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
} Rect;

typedef struct {
    int16_t v;
    int16_t h;
} Point;

typedef void (*ProcPtr)(void);
typedef struct GrafPort *GrafPtr;
typedef uint8_t Style;
typedef uint16_t RGBColor;
typedef void **Handle;

/* TextEdit constants - Evidence: evidence.curated.textedit.json */
#define teJustLeft   0   /* Left justification */
#define teJustCenter 1   /* Center justification */
#define teJustRight  (-1) /* Right justification */
#define teFAutoScroll 0  /* Auto-scroll feature flag */

/* TextEdit Record Structure - Layout: layouts.curated.textedit.json:TERec */
struct TERec {
    Rect destRect;      /* offset 0: Destination rectangle for text display */
    Rect viewRect;      /* offset 8: View rectangle within destination */
    Rect selRect;       /* offset 16: Rectangle encompassing selection */
    int16_t lineHeight; /* offset 24: Height of each line in pixels */
    int16_t fontAscent; /* offset 26: Ascent of current font */
    Point selPoint;     /* offset 28: Point location of selection */
    int32_t selStart;   /* offset 32: Start character position of selection */
    int32_t selEnd;     /* offset 36: End character position of selection */
    int16_t active;     /* offset 40: Active state flag (0=inactive, 1=active) */
    int16_t wordWrap;   /* offset 42: Word wrap flag */
    ProcPtr clickLoop;  /* offset 44: Click loop handling procedure */
    int32_t clickTime;  /* offset 48: Time of last click for double-click */
    int16_t clickLoc;   /* offset 52: Location of last click */
    int32_t caretTime;  /* offset 54: Time for caret blinking */
    int16_t caretState; /* offset 58: Caret visibility state */
    int16_t just;       /* offset 60: Text justification */
    int16_t teLength;   /* offset 62: Length of text in characters */
    Handle hText;       /* offset 64: Handle to text characters */
    Handle hDispText;   /* offset 68: Handle to display text (passwords) */
    ProcPtr clikLoop;   /* offset 72: Click loop procedure (alt spelling) */
    int16_t recalBack;  /* offset 76: Background recalculation flag */
    int16_t recalLines; /* offset 78: Lines needing recalculation */
    int16_t clikStuff;  /* offset 80: Click-related state information */
    int16_t crOnly;     /* offset 82: Carriage return only flag */
    int16_t txFont;     /* offset 84: Font family ID */
    Style txFace;       /* offset 86: Font style flags (bold, italic, etc.) */
    int16_t txMode;     /* offset 87: Text transfer mode */
    int16_t txSize;     /* offset 89: Font size in points */
    /* Padding for alignment */
    GrafPtr inPort;     /* offset 92: Graphics port for drawing */
    ProcPtr highHook;   /* offset 96: High-level event hook procedure */
    ProcPtr caretHook;  /* offset 100: Caret drawing hook procedure */
};

/* TextEdit Style Record - Layout: layouts.curated.textedit.json:TEStyleRec */
typedef struct TEStyleRec {
    int32_t startChar;  /* offset 0: Starting character position for style run */
    int16_t height;     /* offset 4: Line height for this style */
    int16_t ascent;     /* offset 6: Font ascent for this style */
    int16_t txFont;     /* offset 8: Font family ID */
    Style txFace;       /* offset 10: Font style flags */
    int16_t txSize;     /* offset 12: Font size in points */
    RGBColor txColor;   /* offset 14: Text color */
} TEStyleRec;

typedef TEStyleRec **TEStyleHandle;

/* TextEdit Function Prototypes - Evidence: evidence.curated.textedit.json */

/* Initialization and lifecycle - Trap: 0xA9CC, Evidence: offset 0x00003780 */
void TEInit(void);

/* Create new TextEdit record - Trap: 0xA9D2, Evidence: offset 0x00003f00 */
TEHandle TENew(Rect *destRect, Rect *viewRect);

/* Dispose TextEdit record - Trap: 0xA9CD, Evidence: offset 0x00004180 */
void TEDispose(TEHandle hTE);

/* Text content management - Trap: 0xA9CF, Evidence: offset 0x00004280 */
void TESetText(void *text, int32_t length, TEHandle hTE);

/* Get text handle - Trap: 0xA9CB, Evidence: offset 0x00004350 */
CharsHandle TEGetText(TEHandle hTE);

/* User interaction - Trap: 0xA9D4, Evidence: offset 0x00004450 */
void TEClick(Point pt, int16_t extend, TEHandle hTE);

/* Keyboard input - Trap: 0xA9DC, Evidence: offset 0x00004650 */
void TEKey(int16_t key, TEHandle hTE);

/* Clipboard operations - Traps: 0xA9D6/0xA9D5/0xA9D7, Evidence: offsets 0x00004800-0x00004920 */
void TECut(TEHandle hTE);
void TECopy(TEHandle hTE);
void TEPaste(TEHandle hTE);

/* Text editing - Trap: 0xA9D3, Evidence: offset 0x00004a00 */
void TEDelete(TEHandle hTE);

/* Text insertion - Trap: 0xA9DE, Evidence: offset 0x00004b00 */
void TEInsert(void *text, int32_t length, TEHandle hTE);

/* Selection management - Trap: 0xA9D1, Evidence: offset 0x00004c50 */
void TESetSelect(int32_t selStart, int32_t selEnd, TEHandle hTE);

/* Activation state - Traps: 0xA9D8/0xA9D9, Evidence: offsets 0x00004d20-0x00004d80 */
void TEActivate(TEHandle hTE);
void TEDeactivate(TEHandle hTE);

/* Idle processing for caret - Trap: 0xA9DA, Evidence: offset 0x00004e00 */
void TEIdle(TEHandle hTE);

/* Display update - Trap: 0xA9D3, Evidence: offset 0x00004f00 */
void TEUpdate(void *updateRgn, TEHandle hTE);

/* Scrolling - Trap: 0xA9DD, Evidence: offset 0x00005100 */
void TEScroll(int16_t dh, int16_t dv, TEHandle hTE);

/* Simple text display - Trap: 0xA9CE, Evidence: offset 0x00005200 */
void TETextBox(void *text, int32_t length, Rect *box, int16_t just);

#endif /* TEXTEDIT_H */

/* RE-AGENT-TRAILER-JSON
{
  "component": "TextEdit",
  "file": "include/textedit.h",
  "functions": 19,
  "structures": 2,
  "constants": 4,
  "evidence_refs": 19,
  "provenance_density": 0.087,
  "generated": "2024-09-18"
}
*/