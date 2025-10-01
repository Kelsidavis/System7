/*
 * TerminalManager.h
 * Terminal Manager API - Portable System 7.1 Implementation
 *
 * Provides terminal emulation, character handling, and virtual terminal
 * support with modern terminal standards compatibility.
 *
 * Derived from ROM and public documentation.
 */

#ifndef TERMINALMANAGER_H
#define TERMINALMANAGER_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"
#include "WindowManager/WindowTypes.h"

#include "CommToolbox.h"
#include "ConnectionManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Terminal Manager Version */
#define curTMVersion 2

/* Terminal Manager Environment Record Version */
#define curTermEnvRecVers 0

/* Terminal errors */

/* Terminal flags */

/* Selection types */

/* Cursor types */

/* Terminal types */

/* Terminal data block */
      /* terminal type flags */
    Handle theData;         /* data handle */
    Handle auxData;         /* auxiliary data */
    long reserved;          /* reserved */
} TermDataBlock, *TermDataBlockPtr, **TermDataBlockH;

/* Line data block for peeking */
        /* line index */
    short lineLength;       /* line length */
    Ptr lineData;           /* line data */
    Ptr lineAttributes;     /* line attributes */
    long reserved;          /* reserved */
} LineDataBlock, *LineDataBlockPtr;

/* Terminal environment record */
          /* record version */
    TMTermTypes termType;   /* terminal type */
    short textRows;         /* text rows */
    short textCols;         /* text columns */
    short cellHeight;       /* cell height */
    short cellWidth;        /* cell width */
    short fontID;           /* font ID */
    short fontSize;         /* font size */
    Style fontStyle;        /* font style */
    short numColors;        /* number of colors */
    short colorRes;         /* color resolution */
    TMFlags tmFlags;        /* terminal flags */
    Str255 name;            /* terminal name */
    long reserved;          /* reserved */
} TermEnvironRec, *TermEnvironRecPtr;

/* Terminal record */
                   /* tool procedure ID */
    TMFlags flags;                  /* terminal flags */
    OSErr errCode;                  /* last error */
    long refCon;                    /* reference constant */
    long userData;                  /* user data */
    ProcPtr defProc;                /* default procedure */
    Ptr config;                     /* configuration data */
    Ptr oldConfig;                  /* old configuration */
    TermEnvironRec theEnvirons;     /* environment record */
    long tmPrivate;                 /* private data */
    Ptr recvBuf;                    /* receive buffer */
    Ptr sendBuf;                    /* send buffer */
    ProcPtr breakProc;              /* break procedure */
    ProcPtr cacheProc;              /* cache procedure */
    ProcPtr searchProc;             /* search procedure */
    long reserved1;                 /* reserved */
    long reserved2;                 /* reserved */
} TermRecord, *TermPtr, **TermHandle;

/* Terminal callback procedures */

#if GENERATINGCFM
/* UPP creation/disposal for CFM */
pascal TerminalSendProcUPP NewTerminalSendProc(TerminalSendProcPtr userRoutine);
pascal TerminalBreakProcUPP NewTerminalBreakProc(TerminalBreakProcPtr userRoutine);
pascal TerminalCacheProcUPP NewTerminalCacheProc(TerminalCacheProcPtr userRoutine);
pascal TerminalSearchCallBackUPP NewTerminalSearchCallBackProc(TerminalSearchCallBackProcPtr userRoutine);
pascal TerminalEnvironsProcUPP NewTerminalEnvironsProc(TerminalEnvironsProcPtr userRoutine);
pascal TerminalChooseIdleUPP NewTerminalChooseIdleProc(TerminalChooseIdleProcPtr userRoutine);
pascal TerminalClickLoopUPP NewTerminalClickLoopProc(TerminalClickLoopProcPtr userRoutine);
pascal void DisposeTerminalSendProcUPP(TerminalSendProcUPP userUPP);
pascal void DisposeTerminalBreakProcUPP(TerminalBreakProcUPP userUPP);
pascal void DisposeTerminalCacheProcUPP(TerminalCacheProcUPP userUPP);
pascal void DisposeTerminalSearchCallBackUPP(TerminalSearchCallBackUPP userUPP);
pascal void DisposeTerminalEnvironsProcUPP(TerminalEnvironsProcUPP userUPP);
pascal void DisposeTerminalChooseIdleUPP(TerminalChooseIdleUPP userUPP);
pascal void DisposeTerminalClickLoopUPP(TerminalClickLoopUPP userUPP);
#else
/* Direct procedure pointers for 68K */
#define NewTerminalSendProc(userRoutine) (userRoutine)
#define NewTerminalBreakProc(userRoutine) (userRoutine)
#define NewTerminalCacheProc(userRoutine) (userRoutine)
#define NewTerminalSearchCallBackProc(userRoutine) (userRoutine)
#define NewTerminalEnvironsProc(userRoutine) (userRoutine)
#define NewTerminalChooseIdleProc(userRoutine) (userRoutine)
#define NewTerminalClickLoopProc(userRoutine) (userRoutine)
#define DisposeTerminalSendProcUPP(userUPP)
#define DisposeTerminalBreakProcUPP(userUPP)
#define DisposeTerminalCacheProcUPP(userUPP)
#define DisposeTerminalSearchCallBackUPP(userUPP)
#define DisposeTerminalEnvironsProcUPP(userUPP)
#define DisposeTerminalChooseIdleUPP(userUPP)
#define DisposeTerminalClickLoopUPP(userUPP)
#endif

/* Terminal Manager API */

/* Initialization and Management */
pascal TMErr InitTM(void);
pascal short TMGetTMVersion(void);

/* Tool Management */
pascal void TMGetToolName(short procID, Str255 name);
pascal short TMGetProcID(ConstStr255Param name);

/* Terminal Creation and Disposal */
pascal TermHandle TMNew(const Rect *termRect, const Rect *viewRect, TMFlags flags,
                       short procID, WindowPtr owner, TerminalSendProcUPP sendProc,
                       TerminalCacheProcUPP cacheProc, TerminalBreakProcUPP breakProc,
                       TerminalClickLoopUPP clikLoop, TerminalEnvironsProcUPP environsProc,
                       long refCon, long userData);
pascal void TMDispose(TermHandle hTerm);

/* Terminal State Management */
pascal void TMSetRefCon(TermHandle hTerm, long refCon);
pascal long TMGetRefCon(TermHandle hTerm);
pascal void TMSetUserData(TermHandle hTerm, long userData);
pascal long TMGetUserData(TermHandle hTerm);

/* Event Handling */
pascal void TMKey(TermHandle hTerm, const EventRecord *theEvent);
pascal void TMClick(TermHandle hTerm, const EventRecord *theEvent);
pascal void TMEvent(TermHandle hTerm, const EventRecord *theEvent);

/* Display Operations */
pascal void TMUpdate(TermHandle hTerm, RgnHandle visRgn);
pascal void TMPaint(TermHandle hTerm, const TermDataBlock *theTermData, const Rect *theRect);
pascal void TMActivate(TermHandle hTerm, Boolean activate);
pascal void TMResume(TermHandle hTerm, Boolean resume);
pascal void TMIdle(TermHandle hTerm);

/* Data Operations */
pascal long TMStream(TermHandle hTerm, void *theBuffer, long length, short flags);
pascal void TMReset(TermHandle hTerm);
pascal void TMClear(TermHandle hTerm);

/* Display Management */
pascal void TMResize(TermHandle hTerm, const Rect *newViewRect);
pascal void TMScroll(TermHandle hTerm, short dH, short dV);

/* Selection and Search */
pascal long TMGetSelect(TermHandle hTerm, Handle theData, ResType *theType);
pascal void TMSetSelection(TermHandle hTerm, const Rect *theRect, TMSelTypes selType);
pascal void TMGetLine(TermHandle hTerm, short lineNo, TermDataBlock *theTermData);
pascal void TMPeekLine(TermHandle hTerm, short lineNo, LineDataBlock *theLineData);

/* Search Support */
pascal short TMAddSearch(TermHandle hTerm, ConstStr255Param theString, const Rect *where,
                        TMSearchTypes searchType, TerminalSearchCallBackUPP callBack);
pascal void TMRemoveSearch(TermHandle hTerm, short refNum);
pascal void TMClearSearch(TermHandle hTerm);

/* Cursor Management */
pascal Point TMGetCursor(TermHandle hTerm, TMCursorTypes cursType);

/* Menu Support */
pascal Boolean TMMenu(TermHandle hTerm, short menuID, short item);

/* Terminal Key Support */
pascal Boolean TMDoTermKey(TermHandle hTerm, ConstStr255Param theKey);
pascal short TMCountTermKeys(TermHandle hTerm);
pascal void TMGetIndTermKey(TermHandle hTerm, short id, Str255 theKey);

/* Configuration */
pascal Boolean TMValidate(TermHandle hTerm);
pascal void TMDefault(Ptr *config, short procID, Boolean allocate);
pascal Ptr TMGetConfig(TermHandle hTerm);
pascal short TMSetConfig(TermHandle hTerm, Ptr thePtr);

/* Setup and Configuration */
pascal Handle TMSetupPreflight(short procID, long *magicCookie);
pascal void TMSetupSetup(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                        long *magicCookie);
pascal void TMSetupItem(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                       short *theItem, long *magicCookie);
pascal Boolean TMSetupFilter(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                            EventRecord *theEvent, short *theItem, long *magicCookie);
pascal void TMSetupCleanup(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                          long *magicCookie);
pascal void TMSetupXCleanup(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                           Boolean OKed, long *magicCookie);
pascal void TMSetupPostflight(short procID);

/* Localization */
pascal short TMIntlToEnglish(TermHandle hTerm, Ptr inputPtr, Ptr *outputPtr, short language);
pascal short TMEnglishToIntl(TermHandle hTerm, Ptr inputPtr, Ptr *outputPtr, short language);

/* Tool Information */
pascal Handle TMGetVersion(TermHandle hTerm);
pascal short TMGetTermEnvirons(TermHandle hTerm, TermEnvironRec *theEnvirons);

/* Choose Support */
pascal short TMChoose(TermHandle *hTerm, Point where, TerminalChooseIdleUPP idleProc);
pascal short TMPChoose(TermHandle *hTerm, Point where, ChooseRec *cRec);

/* Modern Extensions */

/* Terminal Emulation Types */

/* Character encoding support */

/* Modern terminal configuration */

/* Color support */

/* Modern Terminal API */
pascal TMErr TMSetModernConfig(TermHandle hTerm, const ModernTermConfig *config);
pascal TMErr TMGetModernConfig(TermHandle hTerm, ModernTermConfig *config);
pascal TMErr TMSetColorPalette(TermHandle hTerm, const TermColorPalette *palette);
pascal TMErr TMGetColorPalette(TermHandle hTerm, TermColorPalette *palette);

/* Unicode support */
pascal TMErr TMStreamUnicode(TermHandle hTerm, const void *buffer, long length, short encoding);
pascal TMErr TMSetEncoding(TermHandle hTerm, short encoding);
pascal short TMGetEncoding(TermHandle hTerm);

/* Remote terminal support */
pascal TMErr TMConnectRemote(TermHandle hTerm, ConstStr255Param hostname, short port,
                            ConstStr255Param username, ConstStr255Param password);
pascal TMErr TMDisconnectRemote(TermHandle hTerm);

/* Screen capture */
pascal TMErr TMCaptureScreen(TermHandle hTerm, Handle *screenData, short format);
pascal TMErr TMRestoreScreen(TermHandle hTerm, Handle screenData, short format);

/* Capture formats */

/* Thread-safe operations */
pascal TMErr TMLockTerminal(TermHandle hTerm);
pascal TMErr TMUnlockTerminal(TermHandle hTerm);

#ifdef __cplusplus
}
#endif

#endif /* TERMINALMANAGER_H */