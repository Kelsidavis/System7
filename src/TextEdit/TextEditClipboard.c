/*
 * TextEditClipboard.c - TextEdit Clipboard Operations
 *
 * Handles cut, copy, paste operations with the Scrap Manager
 */

#include "TextEdit/TextEdit.h"
#include "MemoryMgr/MemoryManager.h"
#include "ScrapManager/ScrapManager.h"
#include "FontManager/FontManager.h"
#include "ErrorCodes.h"
#include "ToolboxCompat.h"
#include <string.h>
#include "TextEdit/TELogging.h"
#include "TextEdit/TextEditInternal.h"

/* Boolean constants */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Debug logging */
#define TEC_DEBUG 1

#if TEC_DEBUG
#define TEC_LOG(...) TE_LOG_DEBUG("TEC: " __VA_ARGS__)
#else
#define TEC_LOG(...)
#endif

/* Scrap types */
#define kScrapFlavorTypeText    FOURCC('T', 'E', 'X', 'T')
#define kScrapFlavorTypeStyle   FOURCC('s', 't', 'y', 'l')

/* Global TextEdit scrap */
static Handle g_TEScrap = NULL;
static Handle g_TEStyleScrap = NULL;

static UInt16 TE_ReadBigEndian16(const UInt8 *bytes) {
    return (UInt16)(((UInt16)bytes[0] << 8) | bytes[1]);
}

static UInt32 TE_ReadBigEndian32(const UInt8 *bytes) {
    return ((UInt32)bytes[0] << 24) | ((UInt32)bytes[1] << 16) |
           ((UInt32)bytes[2] << 8) | bytes[3];
}

static void TE_WriteBigEndian16(UInt8 *bytes, UInt16 value) {
    bytes[0] = (UInt8)(value >> 8);
    bytes[1] = (UInt8)value;
}

static void TE_WriteBigEndian32(UInt8 *bytes, UInt32 value) {
    bytes[0] = (UInt8)(value >> 24);
    bytes[1] = (UInt8)(value >> 16);
    bytes[2] = (UInt8)(value >> 8);
    bytes[3] = (UInt8)value;
}

static Handle TE_DecodeStyleScrap(Handle classicScrap) {
    const u32 recordHeaderSize = sizeof(SInt16);
    const u32 elementSize = 20;
    u32 classicSize;
    SInt16 styleCount;
    Handle nativeScrap;

    if (!classicScrap) return NULL;
    classicSize = GetHandleSize(classicScrap);
    if (classicSize < recordHeaderSize) return NULL;

    HLock(classicScrap);
    const UInt8 *bytes = (const UInt8 *)*classicScrap;
    styleCount = (SInt16)TE_ReadBigEndian16(bytes);
    if (styleCount < 0 || styleCount > 1601 ||
        recordHeaderSize + (u32)styleCount * elementSize > classicSize) {
        HUnlock(classicScrap);
        return NULL;
    }

    nativeScrap = NewHandleClear(recordHeaderSize +
                                 (u32)styleCount * sizeof(ScrpSTElement));
    if (!nativeScrap) {
        HUnlock(classicScrap);
        return NULL;
    }
    HLock(nativeScrap);
    StScrpRec *record = (StScrpRec *)HandleDataAligned(nativeScrap);
    if (!record) {
        HUnlock(nativeScrap);
        DisposeHandle(nativeScrap);
        HUnlock(classicScrap);
        return NULL;
    }

    record->scrpNStyles = styleCount;
    for (SInt16 i = 0; i < styleCount; i++) {
        const UInt8 *source = bytes + recordHeaderSize + (u32)i * elementSize;
        ScrpSTElement *style = &record->scrpStyleTab[i];
        style->scrpStartChar = (SInt32)TE_ReadBigEndian32(source);
        style->scrpHeight = (SInt16)TE_ReadBigEndian16(source + 4);
        style->scrpAscent = (SInt16)TE_ReadBigEndian16(source + 6);
        style->scrpFont = (SInt16)TE_ReadBigEndian16(source + 8);
        style->scrpFace = source[10];
        style->scrpSize = (SInt16)TE_ReadBigEndian16(source + 12);
        style->scrpColor.red = TE_ReadBigEndian16(source + 14);
        style->scrpColor.green = TE_ReadBigEndian16(source + 16);
        style->scrpColor.blue = TE_ReadBigEndian16(source + 18);
    }

    HUnlock(nativeScrap);
    HUnlock(classicScrap);
    return nativeScrap;
}

static Handle TE_EncodeStyleScrap(Handle nativeScrap) {
    const u32 recordHeaderSize = sizeof(SInt16);
    const u32 elementSize = 20;
    u32 nativeSize;
    u32 classicSize;
    Handle classicScrap;

    if (!nativeScrap) return NULL;
    nativeSize = GetHandleSize(nativeScrap);
    if (nativeSize < recordHeaderSize) return NULL;

    HLock(nativeScrap);
    StScrpRec *record = (StScrpRec *)HandleDataAligned(nativeScrap);
    if (!record || record->scrpNStyles < 0 || record->scrpNStyles > 1601 ||
        recordHeaderSize + (u32)record->scrpNStyles * sizeof(ScrpSTElement) >
            nativeSize) {
        HUnlock(nativeScrap);
        return NULL;
    }

    classicSize = recordHeaderSize + (u32)record->scrpNStyles * elementSize;
    classicScrap = NewHandleClear(classicSize);
    if (!classicScrap) {
        HUnlock(nativeScrap);
        return NULL;
    }
    HLock(classicScrap);
    UInt8 *bytes = (UInt8 *)*classicScrap;
    TE_WriteBigEndian16(bytes, (UInt16)record->scrpNStyles);
    for (SInt16 i = 0; i < record->scrpNStyles; i++) {
        const ScrpSTElement *style = &record->scrpStyleTab[i];
        UInt8 *destination = bytes + recordHeaderSize + (u32)i * elementSize;
        TE_WriteBigEndian32(destination, (UInt32)style->scrpStartChar);
        TE_WriteBigEndian16(destination + 4, (UInt16)style->scrpHeight);
        TE_WriteBigEndian16(destination + 6, (UInt16)style->scrpAscent);
        TE_WriteBigEndian16(destination + 8, (UInt16)style->scrpFont);
        destination[10] = style->scrpFace;
        TE_WriteBigEndian16(destination + 12, (UInt16)style->scrpSize);
        TE_WriteBigEndian16(destination + 14, style->scrpColor.red);
        TE_WriteBigEndian16(destination + 16, style->scrpColor.green);
        TE_WriteBigEndian16(destination + 18, style->scrpColor.blue);
    }
    HUnlock(classicScrap);
    HUnlock(nativeScrap);
    return classicScrap;
}

/* Forward declarations */
static OSErr TE_CopyToScrap(TEHandle hTE);
static OSErr TE_GetFromScrap(TEHandle hTE);

static Handle TE_CreateStyleScrap(TEExtPtr pTE, SInt32 selectionStart,
                                  SInt32 selectionEnd) {
    STRec *styleRec;
    TEStyleTable *styleTable;
    TERunArray *runArray;
    SInt16 runCount = 0;
    Handle styleScrap;

    if (!pTE->hStyles) return NULL;
    HLock(pTE->hStyles);
    styleRec = (STRec *)HandleDataAligned(pTE->hStyles);
    if (!styleRec || !styleRec->runArray || !styleRec->styleTab ||
        styleRec->nRuns <= 0) {
        HUnlock(pTE->hStyles);
        return NULL;
    }
    HLock(styleRec->runArray);
    HLock(styleRec->styleTab);
    runArray = (TERunArray *)HandleDataAligned(styleRec->runArray);
    styleTable = (TEStyleTable *)HandleDataAligned(styleRec->styleTab);
    if (!runArray || !styleTable || runArray->nRuns <= 0 ||
        styleTable->nStyles <= 0) {
        HUnlock(styleRec->styleTab);
        HUnlock(styleRec->runArray);
        HUnlock(pTE->hStyles);
        return NULL;
    }

    for (SInt16 i = 0; i < runArray->nRuns; i++) {
        SInt32 runStart = runArray->runs[i].startChar;
        SInt32 runEnd = (i + 1 < runArray->nRuns) ?
            runArray->runs[i + 1].startChar : pTE->base.teLength;
        if (runEnd > selectionStart && runStart < selectionEnd) runCount++;
    }
    if (runCount == 0) runCount = 1;
    if (runCount > 1601) {
        HUnlock(styleRec->styleTab);
        HUnlock(styleRec->runArray);
        HUnlock(pTE->hStyles);
        return NULL;
    }

    styleScrap = NewHandleClear(sizeof(SInt16) +
                                (u32)runCount * sizeof(ScrpSTElement));
    if (styleScrap) {
        HLock(styleScrap);
        StScrpRec *scrap = (StScrpRec *)HandleDataAligned(styleScrap);
        if (scrap) {
            scrap->scrpNStyles = runCount;
            SInt16 outIndex = 0;
            GrafPtr savedPort = NULL;
            GetPort(&savedPort);
            if (pTE->base.inPort) SetPort(pTE->base.inPort);

            for (SInt16 i = 0; i < runArray->nRuns && outIndex < runCount; i++) {
                SInt32 runStart = runArray->runs[i].startChar;
                SInt32 runEnd = (i + 1 < runArray->nRuns) ?
                    runArray->runs[i + 1].startChar : pTE->base.teLength;
                if (runEnd <= selectionStart || runStart >= selectionEnd) continue;

                SInt16 styleIndex = runArray->runs[i].styleIndex;
                if (styleIndex < 0 || styleIndex >= styleTable->nStyles) continue;
                TextStyle *source = &styleTable->styles[styleIndex];
                ScrpSTElement *destination = &scrap->scrpStyleTab[outIndex++];
                destination->scrpStartChar =
                    (runStart > selectionStart ? runStart : selectionStart) -
                    selectionStart;
                destination->scrpFont = source->tsFont;
                destination->scrpFace = source->tsFace;
                destination->scrpSize = source->tsSize;
                destination->scrpColor = source->tsColor;

                GrafPort *port = (GrafPort *)pTE->base.inPort;
                SInt16 oldFont = port ? port->txFont : 0;
                SInt16 oldSize = port ? port->txSize : 12;
                Style oldFace = port ? port->txFace : normal;
                if (port) {
                    TextFont(source->tsFont);
                    TextSize(source->tsSize);
                    TextFace(source->tsFace);
                }
                FMetricRec metrics;
                GetFontMetrics(&metrics);
                destination->scrpHeight = (SInt16)(metrics.ascent +
                    metrics.descent + metrics.leading);
                destination->scrpAscent = (SInt16)metrics.ascent;
                if (port) {
                    TextFont(oldFont);
                    TextSize(oldSize);
                    TextFace(oldFace);
                }
            }
            if (savedPort) SetPort(savedPort);
            if (outIndex != runCount) {
                HUnlock(styleScrap);
                DisposeHandle(styleScrap);
                styleScrap = NULL;
            }
        } else {
            HUnlock(styleScrap);
            DisposeHandle(styleScrap);
            styleScrap = NULL;
        }
        if (styleScrap) HUnlock(styleScrap);
    }

    HUnlock(styleRec->styleTab);
    HUnlock(styleRec->runArray);
    HUnlock(pTE->hStyles);
    return styleScrap;
}

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

    /* selStart/selEnd are 32-bit: %d would pass 4-byte ints. */
    TEC_LOG("TECut: cutting [%ld,%ld]\n", (long)pTE->base.selStart, (long)pTE->base.selEnd);

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

    /* selStart/selEnd are 32-bit: %d would pass 4-byte ints. */
    TEC_LOG("TECopy: copying [%ld,%ld]\n", (long)pTE->base.selStart, (long)pTE->base.selEnd);

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

    /* selStart is 32-bit: %d would pass a 4-byte int to printf. */
    TEC_LOG("TEPaste: pasting at %ld\n", (long)pTE->base.selStart);

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

    HUnlock((Handle)hTE);
}

void TEStylePaste(TEHandle hTE) {
    TEExtPtr pTE;
    SInt32 pasteStart;
    SInt32 pasteEnd;
    Boolean styled;

    if (!hTE) return;

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;
    pasteStart = pTE->base.selStart;
    styled = pTE->hStyles != NULL;
    HUnlock((Handle)hTE);

    if (!styled) {
        TEPaste(hTE);
        return;
    }

    TEFromScrap();
    TEPaste(hTE);

    HLock((Handle)hTE);
    pTE = (TEExtPtr)*hTE;
    pasteEnd = pTE->base.selStart;
    HUnlock((Handle)hTE);

    if (pasteEnd > pasteStart && g_TEStyleScrap) {
        TEUseStyleScrap(pasteStart, pasteEnd, g_TEStyleScrap, false, hTE);
        TEUpdate(NULL, hTE);
    }
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
                        Handle decoded = TE_DecodeStyleScrap(g_TEStyleScrap);
                        DisposeHandle(g_TEStyleScrap);
                        g_TEStyleScrap = decoded;
                        if (decoded) {
                            TEC_LOG("TEFromScrap: loaded %ld bytes of style scrap\n",
                                    styleScrapSize);
                        }
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

            /* scrapSize is 32-bit: %d would pass a 4-byte int to printf. */
            TEC_LOG("TEToScrap: saved %ld bytes of TEXT\n", (long)scrapSize);
        }

        /* Put style scrap if present */
        if (g_TEStyleScrap) {
            Handle classicStyleScrap = TE_EncodeStyleScrap(g_TEStyleScrap);
            if (classicStyleScrap) {
                SInt32 styleScrapSize = GetHandleSize(classicStyleScrap);
                if (styleScrapSize > 0) {
                    HLock(classicStyleScrap);
                    PutScrap(styleScrapSize, kScrapFlavorTypeStyle,
                             *classicStyleScrap);
                    HUnlock(classicStyleScrap);

                    /* styleScrapSize is 32-bit: %d would pass a 4-byte int. */
                    TEC_LOG("TEToScrap: saved %ld bytes of 'styl'\n",
                            (long)styleScrapSize);
                }
                DisposeHandle(classicStyleScrap);
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

    /* selLen is 32-bit: %d would pass a 4-byte int to printf. */
    TEC_LOG("TE_CopyToScrap: copied %ld bytes\n", (long)selLen);

    if (g_TEStyleScrap) {
        DisposeHandle(g_TEStyleScrap);
        g_TEStyleScrap = NULL;
    }
    if (pTE->hStyles) {
        g_TEStyleScrap = TE_CreateStyleScrap(pTE, (**teRec).selStart,
                                             (**teRec).selEnd);
        if (!g_TEStyleScrap) {
            HUnlock((Handle)hTE);
            return memFullErr;
        }
    }

    HUnlock((Handle)hTE);

    /* Also put to system scrap */
    return TEToScrap();
}

/*
 * TE_GetFromScrap - Get text from scrap
 */
static OSErr TE_GetFromScrap(TEHandle hTE) {
    (void)hTE;
    /* Load from system scrap if needed */
    if (!g_TEScrap) {
        return TEFromScrap();
    }

    return noErr;
}
