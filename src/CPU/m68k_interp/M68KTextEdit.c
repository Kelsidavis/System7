/*
 * M68KTextEdit.c - TextEdit for a 68K application
 *
 * The editing is native. The program holds a handle to a TERec in its own
 * memory (IM I-377) and reads it freely - teLength, selStart and selEnd,
 * nLines and lineStarts, and hText, the text itself, a handle of its own
 * kept as a copy of the native text. It also sets destRect and viewRect
 * there to scroll, and may change the text in hText and call TECalText;
 * both are read back before the calls that use them.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "TextEdit/TextEdit.h"
#include "QuickDraw/QuickDraw.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"

enum { kMaxTE = 32, kTEHeader = 96 };

typedef struct {
    TEHandle native;
    UInt32 h;           /* the program's TEHandle */
    UInt32 text;        /* its hText */
} TEMap;

static TEMap gTE[kMaxTE];
static int gTECount;

static TEMap* ByHandle(UInt32 h) {
    h &= 0x00FFFFFF;
    for (int i = 0; i < gTECount; i++) if (gTE[i].h == h) return &gTE[i];
    return NULL;
}

/* The record and the text from the native TextEdit record */
static void SyncOut(TEMap* m) {
    TERec* t = *m->native;
    const SInt32* starts = NULL;
    SInt16 nLines = TE_LineInfo(m->native, &starts);
    UInt32 size = kTEHeader + 2u * (UInt32)(nLines + 1);
    if (M68KHeap_GetHandleSize(m->h) < size) M68KHeap_SetHandleSize(m->h, size);
    UInt32 p = M68KHeap_Deref(m->h);
    if (!p) return;
    /* Scrolled text is drawn from viewRect less the scroll; the program
     * sees that as destRect moved, the way its own TextEdit scrolls */
    SInt16 dh, dv;
    TE_GetScroll(m->native, &dh, &dv);
    Rect dest = t->destRect;
    OffsetRect(&dest, (SInt16)(t->viewRect.left - dh - dest.left),
               (SInt16)(t->viewRect.top - dv - dest.top));
    WriteRect(p + 0, &dest);
    WriteRect(p + 8, &t->viewRect);
    WriteRect(p + 16, &t->selRect);
    W16(p + 24, t->lineHeight);
    W16(p + 26, t->fontAscent);
    WritePoint(p + 28, t->selPoint);
    W16(p + 32, t->selStart);
    W16(p + 34, t->selEnd);
    W16(p + 36, t->active);
    W32(p + 46, (UInt32)t->clickTime);
    W16(p + 50, t->clickLoc);
    W32(p + 52, (UInt32)t->caretTime);
    W16(p + 56, t->caretState);
    W16(p + 58, t->just);
    W16(p + 60, t->teLength);
    W32(p + 62, m->text);
    W16(p + 66, t->recalBack);
    W16(p + 68, t->recalLines);
    W16(p + 70, t->clikStuff);
    W16(p + 72, t->crOnly);
    W16(p + 74, t->txFont);
    W8(p + 76, t->txFace);
    W8(p + 77, 0);
    W16(p + 78, t->txMode);
    W16(p + 80, t->txSize);
    W32(p + 82, Obj_PortFor(t->inPort));
    W16(p + 94, nLines);
    for (int i = 0; i < nLines; i++) W16(p + 96 + 2 * (UInt32)i, (UInt16)(starts ? starts[i] : 0));
    W16(p + 96 + 2 * (UInt32)nLines, t->teLength);

    /* The text */
    UInt32 len = (UInt32)(t->teLength > 0 ? t->teLength : 0);
    if (M68KHeap_GetHandleSize(m->text) != len) M68KHeap_SetHandleSize(m->text, len);
    UInt32 dst = M68KHeap_Deref(m->text);
    if (dst && t->hText && *t->hText) WriteBytes(dst, *t->hText, len);
}

/* What the program set in its record: the rectangles, justification and
 * text style */
static void SyncIn(TEMap* m) {
    TERec* t = *m->native;
    UInt32 p = M68KHeap_Deref(m->h);
    if (!p) return;
    Rect dest;
    ReadRect(p + 0, &dest);
    ReadRect(p + 8, &t->viewRect);
    TE_SetScroll(m->native, (SInt16)(t->viewRect.left - dest.left),
                 (SInt16)(t->viewRect.top - dest.top));
    t->destRect = dest;
    t->just = (SInt16)R16(p + 58);
    t->crOnly = (SInt16)R16(p + 72);
    t->txFont = (SInt16)R16(p + 74);
    t->txFace = R8(p + 76);
    t->txMode = (SInt16)R16(p + 78);
    t->txSize = (SInt16)R16(p + 80);
}

/* If the program changed the text in hText, the native text becomes it */
static void SyncTextIn(TEMap* m) {
    UInt32 p = M68KHeap_Deref(m->h);
    UInt32 len = M68KHeap_GetHandleSize(m->text);
    TERec* t = *m->native;
    Boolean differs = len != (UInt32)t->teLength || (p && R16(p + 60) != (UInt16)t->teLength);
    if (!differs && t->hText && *t->hText) {
        UInt32 src = M68KHeap_Deref(m->text);
        for (UInt32 i = 0; i < len && !differs; i++)
            if (R8(src + i) != (UInt8)(*t->hText)[i]) differs = true;
    }
    if (!differs) return;
    if (p && R16(p + 60) < len) len = R16(p + 60);
    char* buf = len ? (char*)NewPtr((Size)len) : NULL;
    if (len && !buf) return;
    if (buf) ReadBytes(M68KHeap_Deref(m->text), buf, len);
    SInt16 start = (SInt16)R16(p + 32), end = (SInt16)R16(p + 34);
    TESetText(buf ? buf : "", (SInt32)len, m->native);
    if (buf) DisposePtr((Ptr)buf);
    TESetSelect(start, end, m->native);
}

UInt32 Obj_TEFor(struct TERec** te) {
    for (int i = 0; i < gTECount; i++) if (gTE[i].native == te) return gTE[i].h;
    return 0;
}

static TEMap* PopTE(void) {
    TEMap* m = ByHandle(Pop32());
    if (m) SyncIn(m);
    return m;
}

/* The port TextEdit draws in is the record's inPort; the program's copy of
 * that port is brought up to date afterwards */
static void Done(TEMap* m) {
    SyncOut(m);
    Obj_SyncPortOut((*m->native)->inPort);
}

/* FUNCTION TENew(destRect, viewRect: Rect): TEHandle */
TRAP(Trap_TENew) {
    UNUSED;
    Rect view, dest;
    ReadRect(Pop32(), &view);
    ReadRect(Pop32(), &dest);
    if (gTECount >= kMaxTE) {
        Result32(0);
        return noErr;
    }
    TEHandle te = TENew(&dest, &view);
    UInt32 h = te ? M68KHeap_NewHandle(kTEHeader + 2, true) : 0;
    UInt32 text = h ? M68KHeap_NewHandle(0, false) : 0;
    if (!te || !h || !text) {
        if (te) TEDispose(te);
        if (h) M68KHeap_DisposeHandle(h);
        Result32(0);
        return noErr;
    }
    TEMap* m = &gTE[gTECount++];
    m->native = te;
    m->h = h;
    m->text = text;
    SyncOut(m);
    Result32(h);
    return noErr;
}

TRAP(Trap_TEDispose) {
    UNUSED;
    TEMap* m = ByHandle(Pop32());
    if (!m) return noErr;
    TEDispose(m->native);
    M68KHeap_DisposeHandle(m->text);
    M68KHeap_DisposeHandle(m->h);
    *m = gTE[--gTECount];
    return noErr;
}

/* PROCEDURE TESetText(text: Ptr; length: LONGINT; hTE: TEHandle), and
 * TEInsert, which has the same arguments */
static OSErr TextCall(void (*call)(const void*, SInt32, TEHandle)) {
    TEMap* m = PopTE();
    SInt32 len = (SInt32)Pop32();
    UInt32 text = Pop32();
    if (!m || len < 0) return noErr;
    char* buf = len ? (char*)NewPtr((Size)len) : NULL;
    if (len && !buf) return noErr;
    if (buf) ReadBytes(text, buf, (UInt32)len);
    call(buf ? buf : "", len, m->native);
    if (buf) DisposePtr((Ptr)buf);
    Done(m);
    return noErr;
}
TRAP(Trap_TESetText) { UNUSED; return TextCall((void (*)(const void*, SInt32, TEHandle))TESetText); }
TRAP(Trap_TEInsert)  { UNUSED; return TextCall((void (*)(const void*, SInt32, TEHandle))TEInsert); }

TRAP(Trap_TEGetText) {
    UNUSED;
    TEMap* m = ByHandle(Pop32());
    if (m) SyncOut(m);
    Result32(m ? m->text : 0);
    return noErr;
}

#define TE_VERB(name, call) \
    TRAP(name) { UNUSED; TEMap* m = PopTE(); if (m) { call(m->native); Done(m); } return noErr; }
TE_VERB(Trap_TEIdle, TEIdle)
TE_VERB(Trap_TEDelete, TEDelete)
TE_VERB(Trap_TECut, TECut)
TE_VERB(Trap_TECopy, TECopy)
TE_VERB(Trap_TEPaste, TEPaste)
TE_VERB(Trap_TEActivate, TEActivate)
TE_VERB(Trap_TEDeactivate, TEDeactivate)
TE_VERB(Trap_TESelView, TESelView)

TRAP(Trap_TECalText) {
    UNUSED;
    TEMap* m = PopTE();
    if (m) {
        SyncTextIn(m);
        TECalText(m->native);
        Done(m);
    }
    return noErr;
}

/* PROCEDURE TEClick(pt: Point; fExtend: BOOLEAN; hTE: TEHandle) */
TRAP(Trap_TEClick) {
    UNUSED;
    TEMap* m = PopTE();
    Boolean shift = PopBool();
    Point p = PopPoint();
    if (m) {
        TEClick(p, shift, m->native);
        Done(m);
    }
    return noErr;
}

/* PROCEDURE TEKey(key: CHAR; hTE: TEHandle) */
TRAP(Trap_TEKey) {
    UNUSED;
    TEMap* m = PopTE();
    UInt8 key = (UInt8)Pop16();
    if (m) {
        TEKey((char)key, m->native);
        Done(m);
    }
    return noErr;
}

/* PROCEDURE TESetSelect(selStart, selEnd: LONGINT; hTE: TEHandle) */
TRAP(Trap_TESetSelect) {
    UNUSED;
    TEMap* m = PopTE();
    SInt32 end = (SInt32)Pop32(), start = (SInt32)Pop32();
    if (m) {
        TESetSelect(start, end, m->native);
        Done(m);
    }
    return noErr;
}

/* PROCEDURE TEUpdate(rUpdate: Rect; hTE: TEHandle) */
TRAP(Trap_TEUpdate) {
    UNUSED;
    TEMap* m = PopTE();
    Rect r;
    ReadRect(Pop32(), &r);
    if (m) {
        TEUpdate(&r, m->native);
        Done(m);
    }
    return noErr;
}

/* PROCEDURE TEScroll(dh, dv: INTEGER; hTE: TEHandle), and TEPinScroll */
TRAP(Trap_TEScroll) {
    UNUSED;
    TEMap* m = PopTE();
    SInt16 dv = (SInt16)Pop16(), dh = (SInt16)Pop16();
    if (m) {
        TEScroll((SInt16)-dh, (SInt16)-dv, m->native);    /* the native sense is the other way */
        Done(m);
    }
    return noErr;
}

TRAP(Trap_TEPinScroll) {
    UNUSED;
    TEMap* m = PopTE();
    SInt16 dv = (SInt16)Pop16(), dh = (SInt16)Pop16();
    if (m) {
        TEPinScroll((SInt16)-dh, (SInt16)-dv, m->native);
        Done(m);
    }
    return noErr;
}

TRAP(Trap_TESetJust) {
    UNUSED;
    TEMap* m = PopTE();
    SInt16 just = (SInt16)Pop16();
    if (m) {
        TESetJust(just, m->native);
        Done(m);
    }
    return noErr;
}

TRAP(Trap_TEAutoView) {
    UNUSED;
    TEMap* m = PopTE();
    Boolean on = PopBool();
    if (m) {
        TEAutoView(on, m->native);
        Done(m);
    }
    return noErr;
}

void M68KTE_Finish(void) {
    for (int i = 0; i < gTECount; i++) TEDispose(gTE[i].native);
    gTECount = 0;
}

const M68KTrapEntry kM68KTextEditTraps[] = {
    { 0xA9D2, Trap_TENew },         { 0xA9CD, Trap_TEDispose },     { 0xA9CF, Trap_TESetText },
    { 0xA9DE, Trap_TEInsert },      { 0xA9CB, Trap_TEGetText },     { 0xA9DA, Trap_TEIdle },
    { 0xA9D7, Trap_TEDelete },      { 0xA9D6, Trap_TECut },         { 0xA9D5, Trap_TECopy },
    { 0xA9DB, Trap_TEPaste },       { 0xA9D8, Trap_TEActivate },    { 0xA9D9, Trap_TEDeactivate },
    { 0xA811, Trap_TESelView },     { 0xA9D0, Trap_TECalText },     { 0xA9D4, Trap_TEClick },
    { 0xA9DC, Trap_TEKey },         { 0xA9D1, Trap_TESetSelect },   { 0xA9D3, Trap_TEUpdate },
    { 0xA9DD, Trap_TEScroll },      { 0xA812, Trap_TEPinScroll },   { 0xA9DF, Trap_TESetJust },
    { 0xA813, Trap_TEAutoView },
};
const int kM68KTextEditTrapCount = (int)(sizeof(kM68KTextEditTraps) / sizeof(kM68KTextEditTraps[0]));
