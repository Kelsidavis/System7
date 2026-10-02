/*
 * M68KToolboxInternal.h - what the trap handlers share
 *
 * The application being answered, its memory, its stack, and the objects
 * it has been given (windows, regions, menus) - each a record in its own
 * memory standing for a native one.
 */

#ifndef M68K_TOOLBOX_INTERNAL_H
#define M68K_TOOLBOX_INTERNAL_H

#include "SystemTypes.h"
#include "CPU/CPUBackend.h"
#include "CPU/M68KInterp.h"
#include "CPU/M68KHeap.h"
#include "FS/hfs_types.h"

extern M68KAddressSpace* gM68KApp;     /* the application being answered */

extern UInt8 M68K_Read8(M68KAddressSpace* as, UInt32 addr);
extern UInt16 M68K_Read16(M68KAddressSpace* as, UInt32 addr);
extern UInt32 M68K_Read32(M68KAddressSpace* as, UInt32 addr);
extern void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value);
extern void M68K_Write16(M68KAddressSpace* as, UInt32 addr, UInt16 value);
extern void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value);

#define R8(a)       M68K_Read8(gM68KApp, (a))
#define R16(a)      M68K_Read16(gM68KApp, (a))
#define R32(a)      M68K_Read32(gM68KApp, (a))
#define W8(a, v)    M68K_Write8(gM68KApp, (a), (UInt8)(v))
#define W16(a, v)   M68K_Write16(gM68KApp, (a), (UInt16)(v))
#define W32(a, v)   M68K_Write32(gM68KApp, (a), (UInt32)(v))

#define D(n)        (gM68KApp->regs.d[n])
#define A(n)        (gM68KApp->regs.a[n])

/* Pascal arguments, popped in the reverse of the order they were pushed */
static inline UInt16 Pop16(void) { UInt16 v = R16(A(7)); A(7) += 2; return v; }
static inline UInt32 Pop32(void) { UInt32 v = R32(A(7)); A(7) += 4; return v; }
static inline Boolean PopBool(void) { return (Pop16() & 0xFF00) != 0; }
static inline UInt8 PopByte(void) { return (UInt8)(Pop16() >> 8); }   /* Char, Style */
static inline Point PopPoint(void) {
    UInt32 v = Pop32();
    Point p;
    p.v = (SInt16)(v >> 16);
    p.h = (SInt16)(v & 0xFFFF);
    return p;
}

/* A function's result, in the space under its arguments */
static inline void Result16(UInt16 v) { W16(A(7), v); }
static inline void Result32(UInt32 v) { W32(A(7), v); }
static inline void ResultBool(Boolean b) { W16(A(7), b ? 0x0100 : 0); }

/* Records between the two memories */
static inline void ReadRect(UInt32 a, Rect* r) {
    r->top = (SInt16)R16(a); r->left = (SInt16)R16(a + 2);
    r->bottom = (SInt16)R16(a + 4); r->right = (SInt16)R16(a + 6);
}
static inline void WriteRect(UInt32 a, const Rect* r) {
    W16(a, r->top); W16(a + 2, r->left); W16(a + 4, r->bottom); W16(a + 6, r->right);
}
static inline void ReadPoint(UInt32 a, Point* p) { p->v = (SInt16)R16(a); p->h = (SInt16)R16(a + 2); }
static inline void WritePoint(UInt32 a, Point p) { W16(a, p.v); W16(a + 2, p.h); }
static inline void ReadPattern(UInt32 a, Pattern* p) { for (int i = 0; i < 8; i++) p->pat[i] = R8(a + i); }
static inline void WritePattern(UInt32 a, const Pattern* p) { for (int i = 0; i < 8; i++) W8(a + i, p->pat[i]); }
static inline void ReadPString(UInt32 a, Str255 out) {
    out[0] = a ? R8(a) : 0;
    for (int i = 1; i <= out[0]; i++) out[i] = R8(a + i);
}
static inline void WritePString(UInt32 a, ConstStr255Param s) {
    for (int i = 0; i <= s[0]; i++) W8(a + i, s[i]);
}
static inline void ReadBytes(UInt32 a, void* out, UInt32 n) {
    for (UInt32 i = 0; i < n; i++) ((UInt8*)out)[i] = R8(a + i);
}
static inline void WriteBytes(UInt32 a, const void* in, UInt32 n) {
    for (UInt32 i = 0; i < n; i++) W8(a + i, ((const UInt8*)in)[i]);
}

#define TRAP(name) static OSErr name(void* ctx, CPUAddr* pc, CPUAddr* regs)
#define UNUSED (void)ctx; (void)pc; (void)regs

typedef struct {
    UInt16 trap;
    CPUTrapHandler handler;
} M68KTrapEntry;

/* ---- M68KToolbox.c ---- */
UInt32 M68KTB_ResHandleFor(Handle native);   /* the application's copy of a resource */
void M68KTB_CloseResFile(SInt16 refNum);     /* changes written, its resources gone */
enum { kLM_ResErr = 0x0A60 };
void M68KTB_SetResErr(void);                 /* ResError, where the program reads it */
UInt32 M68KTB_ScreenBase(void);
UInt32 M68KTB_QDGlobals(void);               /* address of thePort; 0 before InitGraf */
void M68KTB_TrapEnter(UInt16 trap);          /* around every Toolbox trap: the port */
void M68KTB_TrapLeave(UInt16 trap);

/* ---- M68KObjects.c: native objects the application holds ---- */
UInt32 Obj_PortFor(GrafPtr port);            /* its record, made if need be; 0 for none */
GrafPtr Obj_Port(UInt32 addr);               /* the native port of a record, or NULL */
Boolean Obj_AddPort(UInt32 addr, GrafPtr port);   /* a port the program opened */
void Obj_ForgetPort(GrafPtr port);
Boolean Obj_IsAppWindow(WindowPtr w);
UInt32 Obj_NewWindowRecord(WindowPtr w, UInt32 storage);
void Obj_ForgetWindow(WindowPtr w);
void Obj_SyncPortOut(GrafPtr port);          /* native state into the record */
void Obj_SyncPortIn(GrafPtr port);           /* what the program set, into native */
void Obj_SyncWindows(void);                  /* every window record, after the WM moved things */
UInt32 Obj_RgnFor(RgnHandle rgn);
RgnHandle Obj_Rgn(UInt32 h);
UInt32 Obj_NewRgnRecord(RgnHandle rgn);
void Obj_ForgetRgn(UInt32 h);
void Obj_SyncRgn(RgnHandle rgn);
void Obj_SetThePort(GrafPtr port);           /* and QuickDraw's thePort in the program */
void Obj_LeavePort(GrafPtr going);           /* a port is going: not the current one any more */
UInt32 Obj_ControlFor(ControlHandle c);      /* its ControlRecord handle, made if need be */
ControlHandle Obj_Control(UInt32 h);
void Obj_SyncControl(ControlHandle c);
void Obj_ForgetControl(ControlHandle c);
void Obj_Finish(void);                       /* the program is done: its windows go */

/* ---- M68KPorts.c: ports the program opens, and CopyBits ---- */
Boolean Ports_Bits(GrafPtr port, UInt32* base, UInt16* rowBytes, Rect* bounds);
void Ports_ReadRecord(GrafPtr port, UInt32 record);
void Ports_BeforeDraw(GrafPtr port);         /* the program's bits into the drawing */
void Ports_AfterDraw(GrafPtr port);          /* and the drawing back */
Boolean Ports_IsOffscreen(GrafPtr port);
void Ports_Offset(GrafPtr port, SInt16 dh, SInt16 dv);
void Ports_Finish(void);
void Ports_CopyBits(UInt32 srcBits, UInt32 dstBits, UInt32 srcRect, UInt32 dstRect,
                    SInt16 mode, UInt32 maskRgn);
void M68KQD_Finish(void);
extern const M68KTrapEntry kM68KPortTraps[];
extern const int kM68KPortTrapCount;

/* ---- M68KPictures.c: what is drawn while a picture is open ---- */
Boolean Pict_Recording(GrafPtr port);
void Pict_Rect(int verb, const Rect* r);
void Pict_Oval(int verb, const Rect* r);
void Pict_RRect(int verb, const Rect* r, SInt16 ow, SInt16 oh);
void Pict_Arc(int verb, const Rect* r, SInt16 start, SInt16 arc);
void Pict_Poly(int verb, UInt32 poly);
void Pict_Rgn(int verb, RgnHandle rgn);
void Pict_Line(Point from, Point to);
void Pict_Text(Point at, const char* text, SInt16 n);
void Pict_Bits(const UInt8* bits, SInt16 rowBytes, const Rect* srcRect, const Rect* dstRect,
               SInt16 mode);
void M68KPict_Finish(void);
extern const M68KTrapEntry kM68KPictureTraps[];
extern const int kM68KPictureTrapCount;

extern const M68KTrapEntry kM68KQuickDrawTraps[];
extern const int kM68KQuickDrawTrapCount;
extern const M68KTrapEntry kM68KWindowTraps[];
extern const int kM68KWindowTrapCount;
extern const M68KTrapEntry kM68KMenuTraps[];
extern const int kM68KMenuTrapCount;
extern const M68KTrapEntry kM68KEventTraps[];
extern const int kM68KEventTrapCount;
extern const M68KTrapEntry kM68KDialogTraps[];
extern const int kM68KDialogTrapCount;
extern const M68KTrapEntry kM68KControlTraps[];
extern const int kM68KControlTrapCount;
extern const M68KTrapEntry kM68KTextEditTraps[];
extern const int kM68KTextEditTrapCount;
extern const M68KTrapEntry kM68KUtilityTraps[];
extern const int kM68KUtilityTrapCount;
extern const M68KTrapEntry kM68KFileTraps[];
extern const int kM68KFileTrapCount;
void M68KFiles_Prepare(VRefNum vref, DirID dir);   /* the application's folder: the default */
void M68KFiles_Finish(void);
void M68KUtils_Finish(void);
void M68KDialogs_Finish(void);
UInt32 Obj_TEFor(struct TERec** te);
void M68KTE_Finish(void);

/* Read an event from the program's record, its window made native */
void M68KTB_ReadEvent(UInt32 addr, EventRecord* e);
/* Write an event for the program, its window made the program's */
void M68KTB_WriteEvent(UInt32 addr, const EventRecord* e);

/* Call a procedure in the program; its arguments are already pushed */
static inline OSErr CallProgram(UInt32 proc) { return M68K_CallProc(gM68KApp, proc); }
void M68KMenus_Finish(void);

#endif /* M68K_TOOLBOX_INTERNAL_H */
