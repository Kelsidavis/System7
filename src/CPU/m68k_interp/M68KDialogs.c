/*
 * M68KDialogs.c - the Dialog Manager for a 68K application
 *
 * Dialogs and alerts are native, built from the DLOG, ALRT and DITL
 * resources in the program's file. What the program supplies as code - a
 * ModalDialog filter, a user item's drawing procedure - is called back in
 * the program (M68K_CallProc) when the native Dialog Manager calls it.
 *
 * GetDItem hands the program a handle for an item. A text item's handle is
 * a token: GetIText and SetIText know it. A control item's is a control,
 * made from the item the first time it is asked for, since this Dialog
 * Manager draws plain buttons itself and a program will want to set a check
 * box's value. A user item's is the procedure the program installed.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "DialogManager/DialogManager.h"
#include "ControlManager/ControlManager.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"


/* ------------------------------------------------------------------------
 * Item handles the program holds
 * ------------------------------------------------------------------------ */

enum { kMaxTokens = 256, kMaxUserItems = 64 };

static struct { UInt32 token; Handle native; } gTokens[kMaxTokens];
static int gTokenCount;

static UInt32 TokenFor(Handle native) {
    for (int i = 0; i < gTokenCount; i++) if (gTokens[i].native == native) return gTokens[i].token;
    if (gTokenCount >= kMaxTokens) return 0;
    UInt32 t = M68KHeap_NewHandle(4, true);
    if (!t) return 0;
    gTokens[gTokenCount].token = t;
    gTokens[gTokenCount].native = native;
    gTokenCount++;
    return t;
}

static Handle NativeForToken(UInt32 token) {
    token &= 0x00FFFFFF;
    for (int i = 0; i < gTokenCount; i++) if (gTokens[i].token == token) return gTokens[i].native;
    return NULL;
}

/* The program's text, kept in its token too, for a program that reads **h */
static void RefreshToken(UInt32 token, Handle native) {
    Str255 text;
    GetDialogItemText(native, text);
    if (M68KHeap_SetHandleSize(token, (UInt32)text[0]) == noErr) {
        WriteBytes(M68KHeap_Deref(token), &text[1], text[0]);
    }
}

/* User items: the program's procedure for each */
static struct { DialogPtr d; SInt16 item; UInt32 proc; } gUserItems[kMaxUserItems];
static int gUserItemCount;

static UInt32 UserProc(DialogPtr d, SInt16 item) {
    for (int i = 0; i < gUserItemCount; i++)
        if (gUserItems[i].d == d && gUserItems[i].item == item) return gUserItems[i].proc;
    return 0;
}

/* The native Dialog Manager draws a user item by calling its procedure:
 * PROCEDURE DrawItem(theWindow: WindowPtr; itemNo: INTEGER) */
static void UserItemTrampoline(WindowPtr w, SInt16 item) {
    UInt32 proc = UserProc((DialogPtr)w, item);
    if (!proc) return;
    GrafPtr save;
    GetPort(&save);
    A(7) -= 4;
    W32(A(7), Obj_PortFor((GrafPtr)w));
    A(7) -= 2;
    W16(A(7), (UInt16)item);
    CallProgram(proc);
    SetPort(save);
}

/* ------------------------------------------------------------------------
 * The ModalDialog and Alert filter
 * FUNCTION Filter(theDialog: DialogPtr; VAR theEvent: EventRecord;
 *                 VAR itemHit: INTEGER): BOOLEAN
 * ------------------------------------------------------------------------ */

static UInt32 gFilter;

static Boolean FilterTrampoline(DialogPtr d, EventRecord* e, SInt16* item) {
    if (!gFilter) return false;
    UInt32 sp = A(7);
    A(7) -= 16;
    UInt32 evAddr = A(7);
    A(7) -= 2;
    UInt32 itemAddr = A(7);
    M68KTB_WriteEvent(evAddr, e);
    W16(itemAddr, (UInt16)*item);
    A(7) -= 2;
    W16(A(7), 0);                               /* result */
    A(7) -= 4;
    W32(A(7), Obj_PortFor((GrafPtr)d));
    A(7) -= 4;
    W32(A(7), evAddr);
    A(7) -= 4;
    W32(A(7), itemAddr);
    Boolean handled = false;
    if (CallProgram(gFilter) == noErr) {
        handled = (R16(A(7)) & 0xFF00) != 0;
        M68KTB_ReadEvent(evAddr, e);
        *item = (SInt16)R16(itemAddr);
    }
    A(7) = sp;
    return handled;
}

/* ------------------------------------------------------------------------ */

static DialogPtr PopDialog(void) {
    return (DialogPtr)Obj_Port(Pop32());
}

static UInt32 Adopt(DialogPtr d, UInt32 storage) {
    if (!d) return 0;
    return Obj_NewWindowRecord((WindowPtr)d, storage);
}

/* FUNCTION GetNewDialog(dialogID: INTEGER; dStorage: Ptr; behind: WindowPtr): DialogPtr */
TRAP(Trap_GetNewDialog) {
    UNUSED;
    UInt32 behind = Pop32();
    UInt32 storage = Pop32();
    SInt16 id = (SInt16)Pop16();
    WindowPtr b = behind == 0xFFFFFFFF ? (WindowPtr)-1L : behind ? (WindowPtr)Obj_Port(behind) : NULL;
    DialogPtr d = GetNewDialog(id, NULL, b);
    Result32(Adopt(d, storage));
    Obj_SyncWindows();
    return noErr;
}

/* FUNCTION NewDialog(dStorage: Ptr; boundsRect: Rect; title: Str255;
 *   visible: BOOLEAN; procID: INTEGER; behind: WindowPtr; goAwayFlag: BOOLEAN;
 *   refCon: LONGINT; items: Handle): DialogPtr */
TRAP(Trap_NewDialog) {
    UNUSED;
    UInt32 items = Pop32();
    SInt32 refCon = (SInt32)Pop32();
    Boolean goAway = PopBool();
    UInt32 behind = Pop32();
    SInt16 procID = (SInt16)Pop16();
    Boolean visible = PopBool();
    Str255 title;
    ReadPString(Pop32(), title);
    Rect bounds;
    ReadRect(Pop32(), &bounds);
    UInt32 storage = Pop32();
    UInt32 n = M68KHeap_GetHandleSize(items);
    Handle ditl = n ? NewHandle((Size)n) : NULL;
    if (ditl) ReadBytes(M68KHeap_Deref(items), *ditl, n);
    WindowPtr b = behind == 0xFFFFFFFF ? (WindowPtr)-1L : behind ? (WindowPtr)Obj_Port(behind) : NULL;
    DialogPtr d = ditl ? NewDialog(NULL, &bounds, (ConstStr255Param)title, visible, procID, b,
                                   goAway, refCon, ditl) : NULL;
    Result32(Adopt(d, storage));
    Obj_SyncWindows();
    return noErr;
}

static void ForgetDialog(DialogPtr d) {
    for (int i = gUserItemCount - 1; i >= 0; i--)
        if (gUserItems[i].d == d) gUserItems[i] = gUserItems[--gUserItemCount];
    Obj_ForgetWindow((WindowPtr)d);
}

TRAP(Trap_DisposeDialog) {
    UNUSED;
    DialogPtr d = PopDialog();
    if (d) {
        Obj_LeavePort((GrafPtr)d);
        ForgetDialog(d);
        DisposeDialog(d);
        Obj_SyncWindows();
    }
    return noErr;
}

TRAP(Trap_CloseDialog) {
    UNUSED;
    DialogPtr d = PopDialog();
    if (d) {
        Obj_LeavePort((GrafPtr)d);
        ForgetDialog(d);
        CloseDialog(d);
        Obj_SyncWindows();
    }
    return noErr;
}

/* PROCEDURE ModalDialog(filterProc: ProcPtr; VAR itemHit: INTEGER) */
TRAP(Trap_ModalDialog) {
    UNUSED;
    UInt32 var = Pop32();
    UInt32 filter = Pop32();
    UInt32 saved = gFilter;
    gFilter = filter;
    SInt16 item = 0;
    ModalDialog(filter ? FilterTrampoline : NULL, &item);
    gFilter = saved;
    W16(var, (UInt16)item);
    Obj_SyncWindows();
    return noErr;
}

/* FUNCTION IsDialogEvent(theEvent: EventRecord): BOOLEAN */
TRAP(Trap_IsDialogEvent) {
    UNUSED;
    EventRecord e;
    M68KTB_ReadEvent(Pop32(), &e);
    ResultBool(IsDialogEvent(&e));
    return noErr;
}

/* FUNCTION DialogSelect(theEvent: EventRecord; VAR theDialog: DialogPtr;
 *   VAR itemHit: INTEGER): BOOLEAN */
TRAP(Trap_DialogSelect) {
    UNUSED;
    UInt32 itemVar = Pop32();
    UInt32 dlgVar = Pop32();
    EventRecord e;
    M68KTB_ReadEvent(Pop32(), &e);
    DialogPtr d = NULL;
    SInt16 item = 0;
    Boolean hit = DialogSelect(&e, &d, &item);
    W32(dlgVar, d ? Obj_PortFor((GrafPtr)d) : 0);
    W16(itemVar, (UInt16)item);
    ResultBool(hit);
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_DrawDialog) {
    UNUSED;
    DialogPtr d = PopDialog();
    if (d) DrawDialog(d);
    return noErr;
}

TRAP(Trap_UpdtDialog) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    DialogPtr d = PopDialog();
    if (d) {
        if (rgn) UpdateDialog(d, rgn);
        else DrawDialog(d);
    }
    return noErr;
}

/* Make a control of a control item the program asks for */
static Handle RealizeControl(DialogPtr d, SInt16 item, SInt16 type, Handle h, const Rect* box) {
    if (h && DM_IsControlOf((WindowPtr)d, h)) return h;
    static const SInt16 kProc[] = { 0, 1, 2, 0 };    /* push button, check box, radio */
    Str255 title;
    title[0] = 0;
    if (h && *h) memcpy(title, *h, (size_t)((UInt8*)*h)[0] + 1);
    SInt16 kind = (SInt16)(type & 3);
    if (kind == 3) return h;                     /* resource control: as it is */
    ControlHandle c = NewControl((WindowPtr)d, box, (ConstStr255Param)title, true, 0, 0, 1,
                                 kProc[kind], 0);
    if (!c) return h;
    SetDialogItem(d, item, type, (Handle)c, box);
    return (Handle)c;
}

/* PROCEDURE GetDItem(theDialog: DialogPtr; itemNo: INTEGER; VAR itemType: INTEGER;
 *   VAR item: Handle; VAR box: Rect) */
TRAP(Trap_GetDItem) {
    UNUSED;
    UInt32 boxVar = Pop32(), itemVar = Pop32(), typeVar = Pop32();
    SInt16 itemNo = (SInt16)Pop16();
    DialogPtr d = PopDialog();
    SInt16 type = 0;
    Handle h = NULL;
    Rect box = { 0, 0, 0, 0 };
    if (d) GetDialogItem(d, itemNo, &type, &h, &box);
    UInt32 result = 0;
    SInt16 base = (SInt16)(type & 0x7F);
    if (base == userItem) {
        result = UserProc(d, itemNo);
    } else if (base & ctrlItem) {
        h = RealizeControl(d, itemNo, type, h, &box);
        result = DM_IsControlOf((WindowPtr)d, h) ? Obj_ControlFor((ControlHandle)h) : TokenFor(h);
    } else if (h) {
        result = TokenFor(h);
        if (base == statText || base == editText) RefreshToken(result, h);
    }
    if (typeVar) W16(typeVar, (UInt16)type);
    if (itemVar) W32(itemVar, result);
    if (boxVar) WriteRect(boxVar, &box);
    return noErr;
}

/* PROCEDURE SetDItem(theDialog: DialogPtr; itemNo: INTEGER; itemType: INTEGER;
 *   item: Handle; box: Rect) */
TRAP(Trap_SetDItem) {
    UNUSED;
    Rect box;
    ReadRect(Pop32(), &box);
    UInt32 item = Pop32();
    SInt16 type = (SInt16)Pop16();
    SInt16 itemNo = (SInt16)Pop16();
    DialogPtr d = PopDialog();
    if (!d) return noErr;
    if ((type & 0x7F) == userItem) {
        int i;
        for (i = 0; i < gUserItemCount; i++)
            if (gUserItems[i].d == d && gUserItems[i].item == itemNo) break;
        if (i == gUserItemCount && gUserItemCount < kMaxUserItems) gUserItemCount++;
        if (i < kMaxUserItems) {
            gUserItems[i].d = d;
            gUserItems[i].item = itemNo;
            gUserItems[i].proc = item;
        }
        SetDialogItem(d, itemNo, type, item ? (Handle)UserItemTrampoline : NULL, &box);
        return noErr;
    }
    Handle native = NativeForToken(item);
    if (!native) native = (Handle)Obj_Control(item);
    SetDialogItem(d, itemNo, type, native, &box);
    return noErr;
}

/* PROCEDURE GetIText(item: Handle; VAR text: Str255) */
TRAP(Trap_GetIText) {
    UNUSED;
    UInt32 var = Pop32();
    UInt32 token = Pop32();
    Handle h = NativeForToken(token);
    Str255 text;
    text[0] = 0;
    if (h) GetDialogItemText(h, text);
    WritePString(var, text);
    return noErr;
}

TRAP(Trap_SetIText) {
    UNUSED;
    Str255 text;
    ReadPString(Pop32(), text);
    UInt32 token = Pop32();
    Handle h = NativeForToken(token);
    if (h) {
        SetDialogItemText(h, (ConstStr255Param)text);
        RefreshToken(token, h);
    }
    return noErr;
}

/* PROCEDURE SelIText(theDialog: DialogPtr; itemNo: INTEGER; strtSel, endSel: INTEGER) */
TRAP(Trap_SelIText) {
    UNUSED;
    SInt16 end = (SInt16)Pop16(), start = (SInt16)Pop16(), item = (SInt16)Pop16();
    DialogPtr d = PopDialog();
    if (d) SelectDialogItemText(d, item, start, end);
    return noErr;
}

TRAP(Trap_HideDItem) {
    UNUSED;
    SInt16 item = (SInt16)Pop16();
    DialogPtr d = PopDialog();
    if (d) HideDialogItem(d, item);
    return noErr;
}

TRAP(Trap_ShowDItem) {
    UNUSED;
    SInt16 item = (SInt16)Pop16();
    DialogPtr d = PopDialog();
    if (d) ShowDialogItem(d, item);
    return noErr;
}

/* FUNCTION FindDItem(theDialog: DialogPtr; thePt: Point): INTEGER - from 0 */
TRAP(Trap_FindDItem) {
    UNUSED;
    Point p = PopPoint();
    DialogPtr d = PopDialog();
    Result16((UInt16)(d ? FindDialogItem(d, p) : -1));
    return noErr;
}

/* FUNCTION Alert(alertID: INTEGER; filterProc: ProcPtr): INTEGER, and the
 * Stop, Note and Caution kinds */
static OSErr AnyAlert(SInt16 (*call)(SInt16, ModalFilterProcPtr)) {
    UInt32 filter = Pop32();
    SInt16 id = (SInt16)Pop16();
    UInt32 saved = gFilter;
    gFilter = filter;
    SInt16 item = call(id, filter ? FilterTrampoline : NULL);
    gFilter = saved;
    Result16((UInt16)item);
    Obj_SyncWindows();
    return noErr;
}
TRAP(Trap_Alert)        { UNUSED; return AnyAlert(Alert); }
TRAP(Trap_StopAlert)    { UNUSED; return AnyAlert(StopAlert); }
TRAP(Trap_NoteAlert)    { UNUSED; return AnyAlert(NoteAlert); }
TRAP(Trap_CautionAlert) { UNUSED; return AnyAlert(CautionAlert); }

/* PROCEDURE ParamText(param0, param1, param2, param3: Str255) */
TRAP(Trap_ParamText) {
    UNUSED;
    Str255 p[4];
    for (int i = 3; i >= 0; i--) ReadPString(Pop32(), p[i]);
    ParamText((ConstStr255Param)p[0], (ConstStr255Param)p[1],
              (ConstStr255Param)p[2], (ConstStr255Param)p[3]);
    return noErr;
}

/* Calls that preload or release dialogs, or set the alert sound: nothing
 * to do here */
TRAP(Trap_PopWord) { UNUSED; (void)Pop16(); return noErr; }
TRAP(Trap_PopLong) { UNUSED; (void)Pop32(); return noErr; }

const M68KTrapEntry kM68KDialogTraps[] = {
    { 0xA97C, Trap_GetNewDialog },  { 0xA97D, Trap_NewDialog },     { 0xA983, Trap_DisposeDialog },
    { 0xA982, Trap_CloseDialog },   { 0xA991, Trap_ModalDialog },   { 0xA97F, Trap_IsDialogEvent },
    { 0xA980, Trap_DialogSelect },  { 0xA981, Trap_DrawDialog },    { 0xA978, Trap_UpdtDialog },
    { 0xA98D, Trap_GetDItem },      { 0xA98E, Trap_SetDItem },      { 0xA990, Trap_GetIText },
    { 0xA98F, Trap_SetIText },      { 0xA97E, Trap_SelIText },      { 0xA827, Trap_HideDItem },
    { 0xA828, Trap_ShowDItem },     { 0xA984, Trap_FindDItem },     { 0xA985, Trap_Alert },
    { 0xA986, Trap_StopAlert },     { 0xA987, Trap_NoteAlert },     { 0xA988, Trap_CautionAlert },
    { 0xA98B, Trap_ParamText },
    { 0xA979, Trap_PopWord },       /* CouldDialog */
    { 0xA97A, Trap_PopWord },       /* FreeDialog */
    { 0xA989, Trap_PopWord },       /* CouldAlert */
    { 0xA98A, Trap_PopWord },       /* FreeAlert */
    { 0xA98C, Trap_PopLong },       /* ErrorSound */
};
const int kM68KDialogTrapCount = (int)(sizeof(kM68KDialogTraps) / sizeof(kM68KDialogTraps[0]));

void M68KDialogs_Finish(void) {
    gTokenCount = 0;
    gUserItemCount = 0;
    gFilter = 0;
}
