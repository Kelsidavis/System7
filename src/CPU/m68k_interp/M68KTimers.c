/*
 * M68KTimers.c - VBL tasks and the Time Manager, for 68K programs
 *
 * A program asks to be called back: every so many ticks (_VInstall, a VBL
 * task), or once after a delay (_InsTime and _PrimeTime, a Time Manager
 * task). On a Macintosh the callbacks come from interrupts. Here they are
 * made between slices of the program's run and while it waits for events,
 * which is often enough for the sixtieth of a second a VBL task counts in,
 * and for the milliseconds most Time Manager tasks ask for.
 *
 * The VBL queue is the system's own, in low memory at $0160 (IM II-350),
 * because programs walk it. Time Manager tasks are kept in a table here
 * with their deadlines; the records are the program's.
 */

#include <string.h>
#include "M68KToolboxInternal.h"
#include "CPU/LowMemGlobals.h"
#include "TimeManager/TimeBase.h"

extern UInt32 TickCount(void);

enum {
    kVBLQueue = 0x0160,             /* QHdr: qFlags, qHead, qTail */
    kVType = 1,
    kVTypErr = -2,
    kQErr = -1
};

/* VBLTask: qLink, qType, vblAddr, vblCount, vblPhase */
enum { kVBLAddr = 6, kVBLCount = 10 };

/* TMTask: qLink, qType (top bit: active), tmAddr, tmCount, tmWakeUp, tmReserved */
enum { kTMType = 4, kTMAddr = 6, kTMCount = 10, kTMWakeUp = 14 };

enum { kMaxTimeTasks = 32 };
static struct {
    UInt32 task;                    /* 0: a free slot */
    UInt64 deadline;                /* microseconds */
    Boolean active;
    Boolean extended;               /* InsXTime's: has tmWakeUp; InsTime's ends before it */
} gTime[kMaxTimeTasks];

static UInt32 gLastTick;
static Boolean gServicing;

static UInt64 Now(void) {
    UnsignedWide w;
    Microseconds(&w);
    return ((UInt64)w.hi << 32) | w.lo;
}

void M68KTimers_Reset(void) {
    W16(kVBLQueue, 0);
    W32(kVBLQueue + 2, 0);
    W32(kVBLQueue + 6, 0);
    memset(gTime, 0, sizeof(gTime));
    gLastTick = TickCount();
    gServicing = false;
}

/* ------------------------------------------------------------------------
 * The queue, as Enqueue and Dequeue keep one
 * ------------------------------------------------------------------------ */

static void QueueAdd(UInt32 hdr, UInt32 elem) {
    W32(elem, 0);
    UInt32 tail = R32(hdr + 6);
    if (R32(hdr + 2) == 0 || tail == 0) W32(hdr + 2, elem);
    else W32(tail, elem);
    W32(hdr + 6, elem);
}

static Boolean QueueRemove(UInt32 hdr, UInt32 elem) {
    UInt32 prev = 0, cur = R32(hdr + 2);
    for (int n = 0; cur && n < 4096; n++) {
        if (cur == elem) {
            UInt32 next = R32(cur);
            if (prev) W32(prev, next); else W32(hdr + 2, next);
            if (R32(hdr + 6) == elem) W32(hdr + 6, prev);
            return true;
        }
        prev = cur;
        cur = R32(cur);
    }
    return false;
}

/* ------------------------------------------------------------------------
 * Calling a task: its registers its own, and everything put back after,
 * as the interrupt handler that calls one on a Macintosh does
 * ------------------------------------------------------------------------ */

static void CallTask(UInt32 proc, UInt32 a0, UInt32 a1) {
    M68KRegs saved = gM68KApp->regs;
    A(0) = a0;
    A(1) = a1;
    CallProgram(proc);
    gM68KApp->regs = saved;
}

void M68KTimers_Service(void) {
    if (!gM68KApp || gM68KApp->halted || gServicing) return;
    gServicing = true;

    /* Time Manager tasks whose time has come */
    UInt64 now = Now();
    for (int i = 0; i < kMaxTimeTasks; i++) {
        if (!gTime[i].task || !gTime[i].active || now < gTime[i].deadline) continue;
        UInt32 t = gTime[i].task;
        gTime[i].active = false;
        W16(t + kTMType, R16(t + kTMType) & 0x7FFF);
        UInt32 proc = R32(t + kTMAddr);
        if (proc) CallTask(proc, 0, t);         /* A1: the task (Inside Macintosh VI, the Time Manager) */
        if (gM68KApp->halted) break;
    }

    /* VBL tasks, once for each tick gone by */
    UInt32 ticks = TickCount();
    if (ticks - gLastTick > 30) gLastTick = ticks - 30;   /* not a backlog of hundreds */
    while (gLastTick != ticks && !gM68KApp->halted) {
        gLastTick++;
        UInt32 t = R32(kVBLQueue + 2);
        for (int n = 0; t && n < 256 && !gM68KApp->halted; n++) {
            UInt32 next = R32(t);               /* before the task can remove itself */
            SInt16 count = (SInt16)R16(t + kVBLCount);
            if (count > 0) {
                W16(t + kVBLCount, (UInt16)(count - 1));
                if (count == 1) CallTask(R32(t + kVBLAddr), t, 0);
            }
            t = next;
        }
    }
    gServicing = false;
}

/* ------------------------------------------------------------------------
 * The traps
 * ------------------------------------------------------------------------ */

/* _VInstall: A0 the task; D0 noErr or vTypErr */
TRAP(Trap_VInstall) {
    UNUSED;
    UInt32 t = A(0);
    if ((SInt16)R16(t + 4) != kVType) { D(0) = (UInt32)(SInt32)kVTypErr; return noErr; }
    QueueAdd(kVBLQueue, t);
    D(0) = noErr;
    return noErr;
}

/* _VRemove: A0 the task; D0 noErr, vTypErr, or qErr if it was not queued */
TRAP(Trap_VRemove) {
    UNUSED;
    UInt32 t = A(0);
    if ((SInt16)R16(t + 4) != kVType) { D(0) = (UInt32)(SInt32)kVTypErr; return noErr; }
    D(0) = (UInt32)(SInt32)(QueueRemove(kVBLQueue, t) ? noErr : kQErr);
    return noErr;
}

static int TimeSlot(UInt32 t) {
    for (int i = 0; i < kMaxTimeTasks; i++) if (gTime[i].task == t) return i;
    return -1;
}

/* _InsTime and _InsXTime: A0 the task, put in the Time Manager's hands but
 * not yet running */
TRAP(Trap_InsTime) {
    UNUSED;
    UInt32 t = A(0);
    int i = TimeSlot(t);
    if (i < 0) i = TimeSlot(0);
    if (i < 0) { D(0) = (UInt32)(SInt32)kQErr; return noErr; }
    gTime[i].task = t;
    gTime[i].active = false;
    gTime[i].deadline = 0;
    gTime[i].extended = (gM68KApp->currentTrap & 0x0400) != 0;  /* InsXTime */
    W16(t + kTMType, R16(t + kTMType) & 0x7FFF);
    if (gTime[i].extended) W32(t + kTMWakeUp, 0);
    D(0) = noErr;
    return noErr;
}

/* _PrimeTime: A0 the task, D0 the delay - milliseconds if positive,
 * microseconds negated if negative. An extended task whose wake-up time is
 * set counts from that time, not from now, so a task that primes itself
 * again each time keeps step. */
TRAP(Trap_PrimeTime) {
    UNUSED;
    UInt32 t = A(0);
    SInt32 count = (SInt32)D(0);
    int i = TimeSlot(t);
    if (i < 0) { D(0) = (UInt32)(SInt32)kQErr; return noErr; }
    UInt64 delay = count >= 0 ? (UInt64)count * 1000 : (UInt64)(-(SInt64)count);
    UInt64 now = Now();
    UInt64 from = now;
    Boolean drift = gTime[i].extended && R32(t + kTMWakeUp) != 0;
    if (drift && gTime[i].deadline && gTime[i].deadline <= now) from = gTime[i].deadline;
    gTime[i].deadline = from + delay;
    gTime[i].active = true;
    W16(t + kTMType, R16(t + kTMType) | 0x8000);
    /* Programs only ask whether tmWakeUp is zero; nonzero, it is ours */
    if (gTime[i].extended) W32(t + kTMWakeUp, (UInt32)(gTime[i].deadline / 1000) | 1);
    D(0) = noErr;
    return noErr;
}

/* _RmvTime: A0 the task, out of the Time Manager's hands. Still running, it
 * leaves in tmCount the time it had left, in negated microseconds. */
TRAP(Trap_RmvTime) {
    UNUSED;
    UInt32 t = A(0);
    int i = TimeSlot(t);
    if (i < 0) { D(0) = (UInt32)(SInt32)kQErr; return noErr; }
    if (gTime[i].active) {
        UInt64 now = Now();
        UInt64 left = gTime[i].deadline > now ? gTime[i].deadline - now : 0;
        if (left > 0x7FFFFFFF) left = 0x7FFFFFFF;
        W32(t + kTMCount, (UInt32)-(SInt32)left);
    } else {
        W32(t + kTMCount, 0);
    }
    W16(t + kTMType, R16(t + kTMType) & 0x7FFF);
    gTime[i].task = 0;
    gTime[i].active = false;
    D(0) = noErr;
    return noErr;
}

/* _Microseconds: the microsecond count, high half in A0, low in D0 */
TRAP(Trap_Microseconds) {
    UNUSED;
    UInt64 now = Now();
    A(0) = (UInt32)(now >> 32);
    D(0) = (UInt32)now;
    return noErr;
}

const M68KTrapEntry kM68KTimerTraps[] = {
    { 0xA033, Trap_VInstall },      { 0xA034, Trap_VRemove },
    { 0xA058, Trap_InsTime },       { 0xA059, Trap_RmvTime },       { 0xA05A, Trap_PrimeTime },
    { 0xA193, Trap_Microseconds },
};
const int kM68KTimerTrapCount = (int)(sizeof(kM68KTimerTraps) / sizeof(kM68KTimerTraps[0]));
