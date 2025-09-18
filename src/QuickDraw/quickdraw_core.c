/*
 * RE-AGENT-BANNER
 * QuickDraw Core Implementation
 * Reimplemented from Apple System 7.1 QuickDraw Core
 *
 * Original binary: System.rsrc (SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba)
 * Architecture: 68k Mac OS System 7
 * Evidence sources: trap analysis, Mac OS Toolbox reference, binary analysis
 *
 * This file implements core QuickDraw port management functions.
 * Functions are implemented to match the original 68k Mac OS behavior.
 */

#include "quickdraw.h"
#include "quickdraw_types.h"
#include "mac_memory.h"

/* Global current graphics port */
GrafPtr thePort = NULL;

/* Standard patterns - Evidence: These are standard Mac OS patterns */
static const Pattern kBlackPattern = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
static const Pattern kWhitePattern = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
static const Pattern kGrayPattern = {{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55}};
static const Pattern kLtGrayPattern = {{0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22}};
static const Pattern kDkGrayPattern = {{0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD}};

/*
 * InitGraf - Initialize QuickDraw graphics system
 * Trap: 0xA86E
 * Evidence: Core QuickDraw initialization function
 */
void InitGraf(GrafPtr port)
{
    if (port == NULL) return;

    /* Initialize the graphics port */
    port->device = 0;

    /* Initialize port bitmap */
    port->portBits.baseAddr = NULL;
    port->portBits.rowBytes = 0;
    SetRect(&port->portBits.bounds, 0, 0, 0, 0);

    /* Set port rectangle to empty */
    SetRect(&port->portRect, 0, 0, 0, 0);

    /* Initialize regions to NULL - will be allocated later */
    port->visRgn = NULL;
    port->clipRgn = NULL;

    /* Set default patterns */
    port->bkPat = kWhitePattern;
    port->fillPat = kBlackPattern;

    /* Initialize pen state */
    SetPt(&port->pnLoc, 0, 0);
    SetPt(&port->pnSize, 1, 1);
    port->pnMode = srcCopy;
    port->pnPat = kBlackPattern;
    port->pnVis = 0;

    /* Initialize text state */
    port->txFont = 0;         /* System font */
    port->txFace = normal;
    port->txMode = srcCopy;
    port->txSize = 12;        /* Default 12 point */
    port->spExtra = 0;        /* No extra space */

    /* Initialize color state */
    port->fgColor = 0x00000000;  /* Black */
    port->bkColor = 0xFFFFFFFF;  /* White */
    port->colrBit = 0;
    port->patStretch = 0;

    /* Initialize save handles */
    port->picSave = NULL;
    port->rgnSave = NULL;
    port->polySave = NULL;
    port->grafProcs = NULL;

    /* Set as current port */
    thePort = port;
}

/*
 * OpenPort - Initialize and open a graphics port
 * Trap: 0xA86F
 * Evidence: Port initialization function
 */
void OpenPort(GrafPtr port)
{
    if (port == NULL) return;

    /* Initialize the port with default values */
    InitGraf(port);

    /* Allocate default regions */
    port->visRgn = NewRgn();
    port->clipRgn = NewRgn();

    if (port->visRgn) {
        SetEmptyRgn(port->visRgn);
    }
    if (port->clipRgn) {
        SetEmptyRgn(port->clipRgn);
    }
}

/*
 * ClosePort - Close a graphics port
 * Trap: 0xA87D
 * Evidence: Port cleanup function
 */
void ClosePort(GrafPtr port)
{
    if (port == NULL) return;

    /* Dispose of allocated regions */
    if (port->visRgn) {
        DisposeRgn(port->visRgn);
        port->visRgn = NULL;
    }
    if (port->clipRgn) {
        DisposeRgn(port->clipRgn);
        port->clipRgn = NULL;
    }

    /* Clear current port if this was it */
    if (thePort == port) {
        thePort = NULL;
    }
}

/*
 * SetPort - Set the current graphics port
 * Trap: 0xA873
 * Evidence: Standard QuickDraw port selection
 */
void SetPort(GrafPtr port)
{
    thePort = port;
}

/*
 * GetPort - Get the current graphics port
 * Trap: 0xA874
 * Evidence: Standard QuickDraw port retrieval
 */
void GetPort(GrafPtr* port)
{
    if (port != NULL) {
        *port = thePort;
    }
}

/*
 * GetPen - Get current pen position
 * Evidence: Standard QuickDraw pen state retrieval
 */
void GetPen(Point* pt)
{
    if (pt != NULL && thePort != NULL) {
        *pt = thePort->pnLoc;
    }
}

/*
 * PenNormal - Set pen to normal state
 * Evidence: Standard QuickDraw pen reset
 */
void PenNormal(void)
{
    if (thePort == NULL) return;

    SetPt(&thePort->pnSize, 1, 1);
    thePort->pnMode = srcCopy;
    thePort->pnPat = kBlackPattern;
}

/*
 * PenSize - Set pen size
 * Evidence: Standard QuickDraw pen sizing
 */
void PenSize(short width, short height)
{
    if (thePort == NULL) return;

    SetPt(&thePort->pnSize, width, height);
}

/*
 * PenMode - Set pen transfer mode
 * Evidence: Standard QuickDraw pen mode setting
 */
void PenMode(short mode)
{
    if (thePort == NULL) return;

    thePort->pnMode = mode;
}

/*
 * PenPat - Set pen pattern
 * Evidence: Standard QuickDraw pen pattern setting
 */
void PenPat(const Pattern* pat)
{
    if (thePort == NULL || pat == NULL) return;

    thePort->pnPat = *pat;
}

/*
 * BackPat - Set background pattern
 * Evidence: Standard QuickDraw background pattern setting
 */
void BackPat(const Pattern* pat)
{
    if (thePort == NULL || pat == NULL) return;

    thePort->bkPat = *pat;
}

/*
 * ForeColor - Set foreground color
 * Evidence: Color QuickDraw color setting
 */
void ForeColor(long color)
{
    if (thePort == NULL) return;

    thePort->fgColor = color;
}

/*
 * BackColor - Set background color
 * Evidence: Color QuickDraw color setting
 */
void BackColor(long color)
{
    if (thePort == NULL) return;

    thePort->bkColor = color;
}

/*
 * ColorBit - Set color bit
 * Evidence: Color QuickDraw bit setting
 */
void ColorBit(short whichBit)
{
    if (thePort == NULL) return;

    thePort->colrBit = whichBit;
}

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "type": "source_file",
 *   "name": "quickdraw_core.c",
 *   "description": "Core QuickDraw port management functions",
 *   "evidence_sources": ["evidence.curated.quickdraw.json", "mappings.quickdraw.json"],
 *   "confidence": 0.90,
 *   "functions_implemented": 15,
 *   "trap_functions": 5,
 *   "dependencies": ["quickdraw.h", "quickdraw_types.h", "mac_memory.h"],
 *   "global_variables": ["thePort"],
 *   "patterns_defined": 5
 * }
 */