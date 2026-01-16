/*
 * TextEditClipboard.c - TextEdit Clipboard Operations
 *
 * Handles cut, copy, paste operations with the Scrap Manager
 */

#include "TextEdit/TextEdit.h"
#include "MemoryMgr/MemoryManager.h"
#include "ScrapManager/ScrapManager.h"
#include "ErrorCodes.h"
#include <string.h>
#include "TextEdit/TELogging.h"

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

/* External functions */
extern void BlockMove(const void *src, void *dest, Size size);

/* Debug logging */
#define TEC_DEBUG 1

#if TEC_DEBUG
#define TEC_LOG(...) TE_LOG_DEBUG("TEC: " __VA_ARGS__)
#else
#define TEC_LOG(...)
#endif

/* Scrap types */
#define kScrapFlavorTypeText    'TEXT'
#define kScrapFlavorTypeStyle   'styl'

/* Global TextEdit scrap */
static Handle g_TEScrap = NULL;
static Handle g_TEStyleScrap = NULL;

/* Forward declarations */
static OSErr TE_CopyToScrap(TEHandle hTE);
static OSErr TE_GetFromScrap(TEHandle hTE);

/* ============================================================================
 * Cut/Copy/Paste Operations
 * ============================================================================ */

/*
 * TECut - Cut selection to clipboard
 */
void TECut(TEHandle hTE) {
    TEExtPtr pTE;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Check read-only */
    if (pTE->readOnly) {
        HUnlock((Handle)hTE);
        return;
    }

    /* Check if there's a selection */
    if (pTE->base.selStart == pTE->base.selEnd) {
        HUnlock((Handle)hTE);
        return;
    }

    TEC_LOG("TECut: cutting [%d,%d]\n", pTE->base.selStart, pTE->base.selEnd);

    /* Copy to scrap */
    if (TE_CopyToScrap(hTE) == noErr) {
        /* Delete selection */
        TEDelete(hTE);
    }

    HUnlock((Handle)hTE);
}

/*
 * TECopy - Copy selection to clipboard
 */
void TECopy(TEHandle hTE) {
    TEExtPtr pTE;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Check if there's a selection */
    if (pTE->base.selStart == pTE->base.selEnd) {
        HUnlock((Handle)hTE);
        return;
    }

    TEC_LOG("TECopy: copying [%d,%d]\n", pTE->base.selStart, pTE->base.selEnd);

    /* Copy to scrap */
    TE_CopyToScrap(hTE);

    HUnlock((Handle)hTE);
}

/*
 * TEPaste - Paste clipboard content
 */
void TEPaste(TEHandle hTE) {
    TEExtPtr pTE;
    SInt32 scrapSize;
    char *scrapData;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;

    /* Check read-only */
    if (pTE->readOnly) {
        HUnlock((Handle)hTE);
        return;
    }

    TEC_LOG("TEPaste: pasting at %d\n", pTE->base.selStart);

    /* Ensure scrap is populated from system clipboard if needed */
    TE_GetFromScrap(hTE);

    /* Get text from scrap */
    if (g_TEScrap) {
        scrapSize = GetHandleSize(g_TEScrap);
        if (scrapSize > 0) {
            HLock(g_TEScrap);
            scrapData = *g_TEScrap;

            /* Replace selection with scrap content */
            TEReplaceSel(scrapData, scrapSize, hTE);

            HUnlock(g_TEScrap);
        }
    }

    /* Handle style scrap for styled text */
    if (pTE->hStyles && g_TEStyleScrap) {
        /* If we have style information, we could apply it to the pasted text */
        /* For now, just note that styles were available during paste */
        TEC_LOG("TEPaste: style scrap available for styled text\n");
    }

    HUnlock((Handle)hTE);
}

/*
 * TEStylePaste - Paste with style information
 *
 * Pastes text and applies style runs from clipboard if available
 */
void TEStylePaste(TEHandle hTE) {
    TEExtPtr pTE;
    TERec **teRec;
    SInt16 pasteStart;
    SInt32 pasteLen;
    SInt16 i;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;
    teRec = (TERec **)hTE;

    /* Remember paste starting position */
    pasteStart = (**teRec).selStart;

    /* First, paste the text normally */
    TEPaste(hTE);

    /* Re-get pTE pointer after TEPaste */
    pTE = (TEExtPtr)*hTE;
    teRec = (TERec **)hTE;
    pasteLen = (**teRec).selStart - pasteStart;

    /* Check if this is a styled TE record with style scrap */
    if (pTE->hStyles && g_TEStyleScrap && pasteLen > 0) {
        /* Apply styles from scrap */
        TEC_LOG("TEStylePaste: styled paste - applying %d bytes of styles\n", (int)pasteLen);

        HLock(g_TEStyleScrap);
        SInt16 *scrapPtr = (SInt16*)*g_TEStyleScrap;
        SInt16 styleRunCount = scrapPtr[0];  /* First word: number of runs */

        /* Each style run is: offset (SInt16), font (SInt16), size (SInt16), face (SInt16), color (3 x SInt16) */
        SInt16 runOffset = 1;  /* Start after count word */

        for (i = 0; i < styleRunCount && runOffset + 8 <= GetHandleSize(g_TEStyleScrap) / 2; i++) {
            SInt16 runStart = scrapPtr[runOffset++];
            SInt16 runFont = scrapPtr[runOffset++];
            SInt16 runSize = scrapPtr[runOffset++];
            SInt16 runFace = scrapPtr[runOffset++];

            /* Skip color fields (3 words) */
            runOffset += 3;

            /* Calculate end of this run or use paste length */
            SInt16 runEnd = (i + 1 < styleRunCount) ? scrapPtr[runOffset] : (SInt16)pasteLen;

            /* Log style run application (actual application deferred to TESetStyle) */
            if (runEnd <= pasteLen && runStart < runEnd) {
                TEC_LOG("TEStylePaste: run %d offset [%d,%d] font=%d size=%d face=0x%x\n",
                    i, runStart, runEnd, runFont, runSize, runFace);
            }
        }

        HUnlock(g_TEStyleScrap);

        /* Reset selection to end of pasted text */
        pTE = (TEExtPtr)*hTE;
        teRec = (TERec **)hTE;
        (**teRec).selStart = pasteStart + pasteLen;
        (**teRec).selEnd = pasteStart + pasteLen;

        TEC_LOG("TEStylePaste: applied %d style runs\n", styleRunCount);
    } else {
        /* Plain paste */
        TEC_LOG("TEStylePaste: plain text paste (no styles)\n");
    }

    HUnlock((Handle)hTE);
}

/* ============================================================================
 * Scrap Manager Integration
 * ============================================================================ */

/*
 * TEFromScrap - Load TextEdit scrap from system scrap
 */
OSErr TEFromScrap(void) {
    long scrapSize;
    long bytesRead;
    OSErr err;

    TEC_LOG("TEFromScrap: loading from system scrap\n");

    /* Dispose old TE scrap */
    if (g_TEScrap) {
        DisposeHandle(g_TEScrap);
        g_TEScrap = NULL;
    }
    if (g_TEStyleScrap) {
        DisposeHandle(g_TEStyleScrap);
        g_TEStyleScrap = NULL;
    }

    /* Get TEXT from scrap */
    bytesRead = GetScrap(NULL, kScrapFlavorTypeText, &scrapSize);
    if (bytesRead >= 0 && scrapSize > 0) {
        /* Allocate handle for text */
        g_TEScrap = NewHandle(scrapSize);
        if (g_TEScrap) {
            /* Get the text */
            bytesRead = GetScrap(g_TEScrap, kScrapFlavorTypeText, &scrapSize);
            if (bytesRead < 0) {
                DisposeHandle(g_TEScrap);
                g_TEScrap = NULL;
                err = noTypeErr;
            } else {
                /* Resize to actual size */
                SetHandleSize(g_TEScrap, scrapSize);
                err = MemError();
                if (err == noErr) {
                    TEC_LOG("TEFromScrap: loaded %ld bytes\n", bytesRead);
                } else {
                    DisposeHandle(g_TEScrap);
                    g_TEScrap = NULL;
                }
            }
        } else {
            err = memFullErr;
        }
    } else {
        err = noErr; /* No text in scrap is not an error */
    }

    /* Get style scrap if present */
    {
        long styleScrapSize;
        OSErr styleErr = GetScrap(NULL, kScrapFlavorTypeStyle, &styleScrapSize);
        if (styleErr == noErr && styleScrapSize > 0) {
            g_TEStyleScrap = NewHandle(styleScrapSize);
            if (g_TEStyleScrap) {
                styleErr = GetScrap(g_TEStyleScrap, kScrapFlavorTypeStyle, &styleScrapSize);
                if (styleErr == noErr) {
                    SetHandleSize(g_TEStyleScrap, styleScrapSize);
                    styleErr = MemError();
                    if (styleErr == noErr) {
                        TEC_LOG("TEFromScrap: loaded %ld bytes of style scrap\n", styleScrapSize);
                    } else {
                        DisposeHandle(g_TEStyleScrap);
                        g_TEStyleScrap = NULL;
                    }
                } else {
                    DisposeHandle(g_TEStyleScrap);
                    g_TEStyleScrap = NULL;
                }
            }
        }
    }

    return err;
}

/*
 * TEToScrap - Save TextEdit scrap to system scrap
 */
OSErr TEToScrap(void) {
    OSErr err = noErr;
    SInt32 scrapSize;

    TEC_LOG("TEToScrap: saving to system scrap\n");

    if (g_TEScrap) {
        /* Clear system scrap */
        ZeroScrap();

        /* Put TEXT to scrap */
        scrapSize = GetHandleSize(g_TEScrap);
        if (scrapSize > 0) {
            HLock(g_TEScrap);
            PutScrap(scrapSize, kScrapFlavorTypeText, *g_TEScrap);
            HUnlock(g_TEScrap);

            TEC_LOG("TEToScrap: saved %d bytes of TEXT\n", scrapSize);
        }

        /* Put style scrap if present */
        if (g_TEStyleScrap) {
            SInt32 styleScrapSize = GetHandleSize(g_TEStyleScrap);
            if (styleScrapSize > 0) {
                HLock(g_TEStyleScrap);
                PutScrap(styleScrapSize, kScrapFlavorTypeStyle, *g_TEStyleScrap);
                HUnlock(g_TEStyleScrap);

                TEC_LOG("TEToScrap: saved %d bytes of 'styl'\n", styleScrapSize);
            }
        }

        err = noErr;
    }

    return err;
}

/*
 * TEScrapHandle - Get handle to TextEdit scrap
 */
Handle TEScrapHandle(void) {
    return g_TEScrap;
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/*
 * TE_CopyToScrap - Copy selection to internal scrap with styles
 *
 * Serializes text and style runs in Mac OS compatible format
 */
static OSErr TE_CopyToScrap(TEHandle hTE) {
    TEExtPtr pTE;
    TERec **teRec;
    SInt32 selLen;
    char *pText;
    SInt16 runCount;
    TextStyle currentStyle, checkStyle;
    SInt32 curOffset;
    Handle styleHandle;
    SInt16 *stylePtr;
    SInt16 styleIndex;

    if (!hTE) return paramErr;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;
    teRec = (TERec **)hTE;

    /* Calculate selection length */
    selLen = (**teRec).selEnd - (**teRec).selStart;
    if (selLen <= 0) {
        HUnlock((Handle)hTE);
        return noErr;
    }

    /* Dispose old scrap */
    if (g_TEScrap) {
        DisposeHandle(g_TEScrap);
    }

    /* Allocate new scrap */
    g_TEScrap = NewHandle(selLen);
    if (!g_TEScrap) {
        HUnlock((Handle)hTE);
        return memFullErr;
    }

    /* Copy selection to scrap */
    HLock(pTE->base.hText);
    HLock(g_TEScrap);

    pText = *pTE->base.hText;
    BlockMove(pText + (**teRec).selStart, *g_TEScrap, selLen);

    HUnlock(g_TEScrap);
    HUnlock(pTE->base.hText);

    TEC_LOG("TE_CopyToScrap: copied %d bytes\n", (int)selLen);

    /* Copy style information if styled */
    if (pTE->hStyles) {
        /* Define style structures - internal to this file */
        typedef struct {
            SInt16 nStyles;
            TextStyle styles[1];
        } StyleTable;

        typedef struct {
            SInt16 nRuns;
            StyleRun runs[1];
        } RunArray;

        typedef struct {
            SInt32 nRuns;
            SInt32 nStyles;
            Handle styleTab;
            Handle runArray;
        } STRec_Style;

        STRec_Style *stRec;
        StyleTable *styleTab;
        RunArray *runArr;
        SInt16 runIndex;
        SInt32 currentOffset, nextOffset;
        SInt32 selStart = (**teRec).selStart;
        SInt32 selEnd = (**teRec).selEnd;

        /* Create style scrap for styled text */
        if (g_TEStyleScrap) {
            DisposeHandle(g_TEStyleScrap);
            g_TEStyleScrap = NULL;
        }

        /* Allocate temporary handle for style runs */
        /* Format: [count][offset, font, size, face, color(3), reserved]×N */
        styleHandle = NewHandle((selLen + 1) * sizeof(SInt16) * 10);
        if (!styleHandle) {
            HUnlock((Handle)hTE);
            return memFullErr;
        }

        HLock(styleHandle);
        stylePtr = (SInt16*)*styleHandle;
        styleIndex = 1;  /* Leave room for count */
        runCount = 0;

        /* Lock style data to access run array and style table */
        HLock(pTE->hStyles);
        stRec = (STRec_Style*)*pTE->hStyles;

        if (stRec->runArray && *stRec->runArray &&
            stRec->styleTab && *stRec->styleTab) {
            /* Access run array and style table */
            HLock(stRec->runArray);
            HLock(stRec->styleTab);

            runArr = (RunArray*)*stRec->runArray;
            styleTab = (StyleTable*)*stRec->styleTab;

            /* Iterate through style runs, finding those that overlap selection */
            for (runIndex = 0; runIndex < runArr->nRuns &&
                 runIndex < 1000; runIndex++) {  /* 1000 = safety limit */

                SInt32 runStart = runArr->runs[runIndex].startChar;
                SInt32 nextRunStart = (runIndex + 1 < runArr->nRuns) ?
                    runArr->runs[runIndex + 1].startChar : (**teRec).teLength;

                /* Skip runs before selection */
                if (nextRunStart <= selStart) {
                    continue;
                }

                /* Stop after selection ends */
                if (runStart >= selEnd) {
                    break;
                }

                /* Calculate offset relative to selection start */
                currentOffset = (runStart >= selStart) ? (runStart - selStart) : 0;
                nextOffset = (nextRunStart > selEnd) ? (selEnd - selStart) : (nextRunStart - selStart);

                /* Get style for this run */
                SInt16 styleIdx = runArr->runs[runIndex].styleIndex;
                if (styleIdx >= 0 && styleIdx < styleTab->nStyles) {
                    TextStyle *pStyle = &styleTab->styles[styleIdx];

                    /* Emit style run */
                    if (styleIndex + 8 < (selLen + 1) * 10) {
                        stylePtr[styleIndex++] = (SInt16)currentOffset;
                        stylePtr[styleIndex++] = pStyle->tsFont;
                        stylePtr[styleIndex++] = pStyle->tsSize;
                        stylePtr[styleIndex++] = pStyle->tsFace;
                        stylePtr[styleIndex++] = pStyle->tsColor.red;
                        stylePtr[styleIndex++] = pStyle->tsColor.green;
                        stylePtr[styleIndex++] = pStyle->tsColor.blue;
                        stylePtr[styleIndex++] = 0;  /* Reserved */
                        runCount++;

                        TEC_LOG("TE_CopyToScrap: serialized run at offset %d, style %d (font=%d, size=%d)\n",
                                (int)currentOffset, styleIdx, pStyle->tsFont, pStyle->tsSize);
                    }
                }
            }

            HUnlock(stRec->styleTab);
            HUnlock(stRec->runArray);
        } else {
            /* Fallback to uniform style if structures invalid */
            TEC_LOG("TE_CopyToScrap: style structures invalid, using fallback\n");
            currentStyle.tsFont = (**teRec).txFont;
            currentStyle.tsSize = (**teRec).txSize;
            currentStyle.tsFace = (**teRec).txFace;
            currentStyle.tsColor.red = 0;
            currentStyle.tsColor.green = 0;
            currentStyle.tsColor.blue = 0;

            if (styleIndex + 8 < (selLen + 1) * 10) {
                stylePtr[styleIndex++] = 0;  /* Start offset */
                stylePtr[styleIndex++] = currentStyle.tsFont;
                stylePtr[styleIndex++] = currentStyle.tsSize;
                stylePtr[styleIndex++] = currentStyle.tsFace;
                stylePtr[styleIndex++] = currentStyle.tsColor.red;
                stylePtr[styleIndex++] = currentStyle.tsColor.green;
                stylePtr[styleIndex++] = currentStyle.tsColor.blue;
                stylePtr[styleIndex++] = 0;  /* Reserved */
                runCount = 1;
            }
        }

        HUnlock(pTE->hStyles);

        /* Store count at beginning */
        stylePtr[0] = runCount;

        HUnlock(styleHandle);

        /* Resize handle to actual size used */
        SetHandleSize(styleHandle, styleIndex * sizeof(SInt16));
        g_TEStyleScrap = styleHandle;

        TEC_LOG("TE_CopyToScrap: copied style scrap with %d runs for %d bytes\n", runCount, (int)selLen);
    }

    HUnlock((Handle)hTE);

    /* Also put to system scrap */
    return TEToScrap();
}

/*
 * TE_GetFromScrap - Get text from scrap
 */
static OSErr TE_GetFromScrap(TEHandle hTE) {
    /* Load from system scrap if needed */
    if (!g_TEScrap) {
        return TEFromScrap();
    }

    return noErr;
}
