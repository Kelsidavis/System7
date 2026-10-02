/*
 * M68KUtils.c - the small Toolbox and OS calls a 68K application leans on
 *
 * Numbers to strings and back (Pack 7), dates and times (Pack 6 and the OS
 * calls), the scrap, string comparison, fixed-point and bit arithmetic,
 * Munger, and GetTrapAddress - which a program asks, among other things,
 * whether WaitNextEvent exists.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "SANENumbers.h"
#include "ScrapManager/ScrapManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"

extern void GetDateTime(UInt32* secs);
extern UInt32 TickCount(void);

/* ------------------------------------------------------------------------
 * Pack 7: NumToString and StringToNum - the selector on the stack, the
 * number in D0 and the string at A0
 * ------------------------------------------------------------------------ */

TRAP(Trap_Pack7) {
    UNUSED;
    UInt16 selector = Pop16();
    if (selector == 0) {                                     /* NumToString */
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld", (long)(SInt32)D(0));
        Str255 s;
        c2pstrcpy(s, buf);
        WritePString(A(0), s);
    } else if (selector == 1) {                              /* StringToNum */
        Str255 s;
        ReadPString(A(0), s);
        SInt32 n = 0;
        int i = 1;
        Boolean neg = false;
        if (i <= s[0] && (s[i] == '-' || s[i] == '+')) neg = s[i++] == '-';
        for (; i <= s[0] && s[i] >= '0' && s[i] <= '9'; i++) n = n * 10 + (s[i] - '0');
        D(0) = (UInt32)(neg ? -n : n);
    } else if (selector == 2 || selector == 4) {
        /* PROCEDURE PStr2Dec / CStr2Dec(s; VAR index: INTEGER;
         *   VAR d: decimal; VAR validPrefix: BOOLEAN). A Pascal string's
         * index counts from 1, a C string's from 0. */
        UInt32 valid = Pop32(), dec = Pop32(), index = Pop32(), str = Pop32();
        static UInt8 text[1024];
        int len = 0, base;
        if (selector == 2) {
            len = R8(str);
            for (int i = 0; i < len; i++) text[i] = R8(str + 1 + i);
            base = 1;
        } else {
            while (len < (int)sizeof(text) && (text[len] = R8(str + (UInt32)len)) != 0) len++;
            base = 0;
        }
        int i = (SInt16)R16(index) - base;
        if (i < 0) i = 0;
        SANEDecimal d;
        int validPrefix = 0;
        SANE_Str2Dec(text, len, &i, &d, &validPrefix);
        W16(index, (UInt16)(i + base));
        M68KSANE_WriteDecimal(dec, &d);
        W8(valid, validPrefix ? 1 : 0);
    } else if (selector == 3) {
        /* PROCEDURE Dec2Str(f: decform; d: decimal; VAR s: DecStr). The
         * decform is four bytes, so it is on the stack itself. */
        UInt32 out = Pop32(), dec = Pop32(), form = Pop32();
        SANEDecForm f = { (form >> 24) != 0, (int16_t)(form & 0xFFFF) };
        SANEDecimal d;
        M68KSANE_ReadDecimal(dec, &d);
        UInt8 s[kSANEDecStrLen + 1];
        SANE_Dec2Str(&f, &d, s);
        WritePString(out, s);
    }
    return noErr;
}

/* ------------------------------------------------------------------------
 * Dates and times
 * ------------------------------------------------------------------------ */

static void SecondsToFields(UInt32 secs, int* year, int* month, int* day,
                            int* hour, int* minute, int* second, int* weekday) {
    *second = (int)(secs % 60);
    *minute = (int)(secs / 60 % 60);
    *hour = (int)(secs / 3600 % 24);
    UInt32 days = secs / 86400;
    *weekday = (int)((days + 5) % 7) + 1;           /* 1 Jan 1904 was a Friday */
    int y = 1904;
    for (;;) {
        int len = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
        if (days < (UInt32)len) break;
        days -= (UInt32)len;
        y++;
    }
    static const int kMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    Boolean leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    int m = 0;
    while (m < 12) {
        int len = kMonth[m] + (m == 1 && leap);
        if (days < (UInt32)len) break;
        days -= (UInt32)len;
        m++;
    }
    *year = y;
    *month = m + 1;
    *day = (int)days + 1;
}

/* ReadDateTime (OS): the clock into the long at A0 */
TRAP(Trap_ReadDateTime) {
    UNUSED;
    UInt32 now = 0;
    GetDateTime(&now);
    W32(A(0), now);
    D(0) = 0;
    return noErr;
}

/* Secs2Date: D0 seconds into the DateTimeRec at A0 - year, month, day,
 * hour, minute, second, dayOfWeek, each a word */
TRAP(Trap_Secs2Date) {
    UNUSED;
    int y, mo, d, h, mi, s, wd;
    SecondsToFields(D(0), &y, &mo, &d, &h, &mi, &s, &wd);
    UInt32 r = A(0);
    W16(r, y); W16(r + 2, mo); W16(r + 4, d);
    W16(r + 6, h); W16(r + 8, mi); W16(r + 10, s); W16(r + 12, wd);
    return noErr;
}

/* Date2Secs: the DateTimeRec at A0 into seconds in D0 */
TRAP(Trap_Date2Secs) {
    UNUSED;
    UInt32 r = A(0);
    int y = (SInt16)R16(r), mo = (SInt16)R16(r + 2), d = (SInt16)R16(r + 4);
    int h = (SInt16)R16(r + 6), mi = (SInt16)R16(r + 8), s = (SInt16)R16(r + 10);
    static const int kBefore[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    UInt32 days = 0;
    for (int yy = 1904; yy < y; yy++) days += (yy % 4 == 0 && (yy % 100 != 0 || yy % 400 == 0)) ? 366 : 365;
    if (mo >= 1 && mo <= 12) days += (UInt32)kBefore[mo - 1];
    if (mo > 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) days++;
    days += (UInt32)(d - 1);
    D(0) = days * 86400u + (UInt32)(h * 3600 + mi * 60 + s);
    return noErr;
}

/* Pack 6: IUDateString(dateTime; form; VAR result), selector 0, and
 * IUTimeString(dateTime; wantSeconds: BOOLEAN; VAR result), selector 2 */
TRAP(Trap_Pack6) {
    UNUSED;
    static const char* kMonths[12] = { "January", "February", "March", "April", "May", "June",
                                       "July", "August", "September", "October", "November",
                                       "December" };
    static const char* kDays[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
                                    "Friday", "Saturday" };
    UInt16 selector = Pop16();
    char buf[64];
    buf[0] = '\0';
    UInt32 result = 0;
    if (selector == 0 || selector == 2) {
        result = Pop32();
        UInt16 arg = Pop16();
        UInt32 secs = Pop32();
        int y, mo, d, h, mi, s, wd;
        SecondsToFields(secs, &y, &mo, &d, &h, &mi, &s, &wd);
        if (selector == 0) {
            UInt8 form = (UInt8)(arg >> 8);
            if (form == 0) snprintf(buf, sizeof(buf), "%d/%d/%02d", mo, d, y % 100);
            else if (form == 1) snprintf(buf, sizeof(buf), "%s, %s %d, %d", kDays[wd - 1], kMonths[mo - 1], d, y);
            else snprintf(buf, sizeof(buf), "%.3s, %.3s %d, %d", kDays[wd - 1], kMonths[mo - 1], d, y);
        } else {
            int h12 = h % 12 ? h % 12 : 12;
            if (arg & 0xFF00) snprintf(buf, sizeof(buf), "%d:%02d:%02d %s", h12, mi, s, h < 12 ? "AM" : "PM");
            else snprintf(buf, sizeof(buf), "%d:%02d %s", h12, mi, h < 12 ? "AM" : "PM");
        }
        Str255 p;
        c2pstrcpy(p, buf);
        WritePString(result, p);
    }
    return noErr;
}

/* ------------------------------------------------------------------------
 * The scrap: the native desk scrap, its contents copied in and out
 * ------------------------------------------------------------------------ */

static UInt32 gScrapStuff;      /* the program's ScrapStuff record */

TRAP(Trap_ZeroScrap)   { UNUSED; ZeroScrap(); Result32(0); return noErr; }
TRAP(Trap_LoadScrap)   { UNUSED; LoadScrap(); Result32(0); return noErr; }
TRAP(Trap_UnloadScrap) { UNUSED; UnloadScrap(); Result32(0); return noErr; }

/* FUNCTION PutScrap(length: LONGINT; theType: ResType; source: Ptr): LONGINT */
TRAP(Trap_PutScrap) {
    UNUSED;
    UInt32 src = Pop32();
    ResType type = Pop32();
    SInt32 len = (SInt32)Pop32();
    if (len < 0) len = 0;
    Ptr buf = NewPtr(len ? len : 1);
    if (!buf) {
        Result32((UInt32)(SInt32)memFullErr);
        return noErr;
    }
    ReadBytes(src, buf, (UInt32)len);
    PutScrap(len, type, buf);
    Result32(0);
    DisposePtr(buf);
    return noErr;
}

/* FUNCTION GetScrap(hDest: Handle; theType: ResType; VAR offset: LONGINT): LONGINT */
TRAP(Trap_GetScrap) {
    UNUSED;
    UInt32 offVar = Pop32();
    ResType type = Pop32();
    UInt32 dest = Pop32();
    Handle h = NewHandle(0);
    long offset = 0;
    SInt32 len = h ? (SInt32)GetScrap(h, type, &offset) : (SInt32)memFullErr;
    if (len > 0 && dest) {
        if (M68KHeap_SetHandleSize(dest, (UInt32)len) == noErr) {
            WriteBytes(M68KHeap_Deref(dest), *h, (UInt32)len);
        } else {
            len = memFullErr;
        }
    }
    if (h) DisposeHandle(h);
    if (offVar) W32(offVar, (UInt32)offset);
    Result32((UInt32)len);
    return noErr;
}

/* FUNCTION InfoScrap: PScrapStuff - scrapSize, scrapHandle, scrapCount,
 * scrapState, scrapName; the count is what a program watches for change */
TRAP(Trap_InfoScrap) {
    UNUSED;
    if (!gScrapStuff) gScrapStuff = M68KHeap_NewPtr(16, true);
    if (gScrapStuff) {
        /* The size of what is there, as text or a picture with their
         * headers - a program pastes only if it is not zero */
        SInt32 size = 0;
        Handle h = NewHandle(0);
        if (h) {
            static const OSType kTypes[2] = { 'TEXT', 'PICT' };
            for (int i = 0; i < 2; i++) {
                long off = 0;
                long n = GetScrap(h, kTypes[i], &off);
                if (n > 0) size += (SInt32)n + 8;
            }
            DisposeHandle(h);
        }
        W32(gScrapStuff + 0, (UInt32)size);
        W32(gScrapStuff + 4, 0);
        W16(gScrapStuff + 8, (UInt16)InfoScrap());
        W16(gScrapStuff + 10, 0);
        W32(gScrapStuff + 12, 0);
    }
    Result32(gScrapStuff);
    return noErr;
}

/* ------------------------------------------------------------------------
 * Strings
 * ------------------------------------------------------------------------ */

static UInt8 Upper(UInt8 c, Boolean diacritics) {
    (void)diacritics;
    return (c >= 'a' && c <= 'z') ? (UInt8)(c - 32) : c;
}

/* UprString (OS): D0 bytes at A0 into upper case */
TRAP(Trap_UprString) {
    UNUSED;
    UInt32 n = D(0) & 0xFFFF;
    for (UInt32 i = 0; i < n; i++) W8(A(0) + i, Upper(R8(A(0) + i), false));
    return noErr;
}

/* CmpString (OS): strings at A0 and A1, their lengths in D0's halves;
 * D0 0 if equal. Bit 9 of the trap ignores case. */
TRAP(Trap_CmpString) {
    UNUSED;
    UInt32 n1 = D(0) >> 16, n2 = D(0) & 0xFFFF;
    Boolean caseless = (gM68KApp->currentTrap & 0x0400) == 0;
    UInt32 diff = n1 != n2;
    for (UInt32 i = 0; !diff && i < n1; i++) {
        UInt8 a = R8(A(0) + i), b = R8(A(1) + i);
        if (caseless) {
            a = Upper(a, false);
            b = Upper(b, false);
        }
        diff = a != b;
    }
    D(0) = diff;
    return noErr;
}

/* RelString (OS): as CmpString, but -1, 0 or 1 for the order */
TRAP(Trap_RelString) {
    UNUSED;
    UInt32 n1 = D(0) >> 16, n2 = D(0) & 0xFFFF;
    Boolean caseSens = (gM68KApp->currentTrap & 0x0400) != 0;
    SInt32 r = 0;
    for (UInt32 i = 0; r == 0 && i < n1 && i < n2; i++) {
        UInt8 a = R8(A(0) + i), b = R8(A(1) + i);
        if (!caseSens) {
            a = Upper(a, false);
            b = Upper(b, false);
        }
        r = a < b ? -1 : a > b;
    }
    if (r == 0) r = n1 < n2 ? -1 : n1 > n2;
    D(0) = (UInt32)r;
    return noErr;
}

/* FUNCTION NewString(theString: Str255): StringHandle */
TRAP(Trap_NewString) {
    UNUSED;
    Str255 s;
    ReadPString(Pop32(), s);
    UInt32 h = M68KHeap_NewHandle((UInt32)s[0] + 1, false);
    if (h) WritePString(M68KHeap_Deref(h), s);
    Result32(h);
    return noErr;
}

/* PROCEDURE SetString(theString: StringHandle; strNew: Str255) */
TRAP(Trap_SetString) {
    UNUSED;
    Str255 s;
    ReadPString(Pop32(), s);
    UInt32 h = Pop32();
    if (h && M68KHeap_SetHandleSize(h, (UInt32)s[0] + 1) == noErr) WritePString(M68KHeap_Deref(h), s);
    return noErr;
}

/* FUNCTION Munger(h: Handle; offset: LONGINT; ptr1: Ptr; len1: LONGINT;
 *   ptr2: Ptr; len2: LONGINT): LONGINT - find ptr1 in h from offset and put
 * ptr2 in its place (IM I-468) */
TRAP(Trap_Munger) {
    UNUSED;
    SInt32 len2 = (SInt32)Pop32();
    UInt32 ptr2 = Pop32();
    SInt32 len1 = (SInt32)Pop32();
    UInt32 ptr1 = Pop32();
    SInt32 offset = (SInt32)Pop32();
    UInt32 h = Pop32();
    SInt32 size = (SInt32)M68KHeap_GetHandleSize(h);
    UInt32 data = M68KHeap_Deref(h);
    SInt32 at = -1;
    if (ptr1 == 0) {
        at = offset;                        /* no target: at the offset */
        if (len1 < 0 || at + len1 > size) len1 = size - at;
    } else {
        for (SInt32 i = offset; i + len1 <= size && at < 0; i++) {
            SInt32 k = 0;
            while (k < len1 && R8(data + (UInt32)(i + k)) == R8(ptr1 + (UInt32)k)) k++;
            if (k == len1) at = i;
        }
    }
    if (at < 0 || at > size) {
        Result32((UInt32)-1);
        return noErr;
    }
    if (len2 < 0 || ptr2 == 0) {            /* only finding */
        Result32((UInt32)at);
        return noErr;
    }
    SInt32 newSize = size - len1 + len2;
    UInt8* tail = NULL;
    SInt32 tailLen = size - (at + len1);
    if (tailLen > 0) {
        tail = (UInt8*)NewPtr(tailLen);
        if (!tail) {
            Result32((UInt32)-1);
            return noErr;
        }
        ReadBytes(data + (UInt32)(at + len1), tail, (UInt32)tailLen);
    }
    M68KHeap_SetHandleSize(h, (UInt32)newSize);
    data = M68KHeap_Deref(h);
    for (SInt32 i = 0; i < len2; i++) W8(data + (UInt32)(at + i), R8(ptr2 + (UInt32)i));
    if (tail) {
        WriteBytes(data + (UInt32)(at + len2), tail, (UInt32)tailLen);
        DisposePtr((Ptr)tail);
    }
    Result32((UInt32)(at + len2));
    return noErr;
}

/* ------------------------------------------------------------------------
 * Fixed point and bits
 * ------------------------------------------------------------------------ */

TRAP(Trap_FixMul) {
    UNUSED;
    SInt32 b = (SInt32)Pop32(), a = (SInt32)Pop32();
    Result32((UInt32)(SInt32)(((SInt64)a * b) >> 16));
    return noErr;
}

TRAP(Trap_FixRatio) {
    UNUSED;
    SInt16 den = (SInt16)Pop16(), num = (SInt16)Pop16();
    Result32(den ? (UInt32)(SInt32)(((SInt64)num << 16) / den) : (num < 0 ? 0x80000000u : 0x7FFFFFFFu));
    return noErr;
}

TRAP(Trap_FixRound) {
    UNUSED;
    SInt32 x = (SInt32)Pop32();
    Result16((UInt16)(SInt16)((x + 0x8000) >> 16));
    return noErr;
}

TRAP(Trap_HiWord) { UNUSED; UInt32 x = Pop32(); Result16((UInt16)(x >> 16)); return noErr; }
TRAP(Trap_LoWord) { UNUSED; UInt32 x = Pop32(); Result16((UInt16)x); return noErr; }

/* PROCEDURE LongMul(a, b: LONGINT; VAR result: Int64Bit) */
TRAP(Trap_LongMul) {
    UNUSED;
    UInt32 var = Pop32();
    SInt32 b = (SInt32)Pop32(), a = (SInt32)Pop32();
    SInt64 r = (SInt64)a * b;
    W32(var, (UInt32)((UInt64)r >> 32));
    W32(var + 4, (UInt32)r);
    return noErr;
}

#define BIT_OP(name, expr) \
    TRAP(name) { UNUSED; UInt32 b = Pop32(), a = Pop32(); Result32(expr); return noErr; }
BIT_OP(Trap_BitAnd, a & b)
BIT_OP(Trap_BitOr, a | b)
BIT_OP(Trap_BitXor, a ^ b)

TRAP(Trap_BitNot) { UNUSED; UInt32 a = Pop32(); Result32(~a); return noErr; }

TRAP(Trap_BitShift) {
    UNUSED;
    SInt16 count = (SInt16)Pop16();
    UInt32 a = Pop32();
    count = (SInt16)(count % 32);
    Result32(count >= 0 ? a << count : a >> -count);
    return noErr;
}

/* The bit operations count bits from the high bit of the first byte */
TRAP(Trap_BitTst) {
    UNUSED;
    SInt32 bit = (SInt32)Pop32();
    UInt32 p = Pop32();
    ResultBool((R8(p + (UInt32)(bit >> 3)) & (0x80 >> (bit & 7))) != 0);
    return noErr;
}

TRAP(Trap_BitSet) {
    UNUSED;
    SInt32 bit = (SInt32)Pop32();
    UInt32 p = Pop32() + (UInt32)(bit >> 3);
    W8(p, R8(p) | (0x80 >> (bit & 7)));
    return noErr;
}

TRAP(Trap_BitClr) {
    UNUSED;
    SInt32 bit = (SInt32)Pop32();
    UInt32 p = Pop32() + (UInt32)(bit >> 3);
    W8(p, R8(p) & ~(0x80 >> (bit & 7)));
    return noErr;
}

/* PROCEDURE StuffHex(thingPtr: Ptr; s: Str255) */
TRAP(Trap_StuffHex) {
    UNUSED;
    Str255 s;
    ReadPString(Pop32(), s);
    UInt32 p = Pop32();
    for (int i = 1; i + 1 <= s[0]; i += 2) {
        int v = 0;
        for (int k = 0; k < 2; k++) {
            char c = (char)s[i + k];
            v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10
                          : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0);
        }
        W8(p + (UInt32)(i / 2), v);
    }
    return noErr;
}

/* ------------------------------------------------------------------------
 * Trap addresses
 *
 * A program asks for a trap's address to call it directly, or - most often
 * - to compare with _Unimplemented's and learn whether a call exists. Each
 * answer is a two-byte routine in the program's memory: the trap word with
 * the auto-pop bit, which runs the trap and returns to whoever JSRed to it.
 * A trap this system does not answer gets _Unimplemented's routine.
 * ------------------------------------------------------------------------ */

static UInt32 gStubs;           /* 1024 Toolbox stubs, then 256 OS stubs */

static UInt32 StubFor(UInt16 trapWord) {
    if (!gStubs) {
        gStubs = M68KHeap_NewPtr((1024 + 256) * 4, true);
        if (!gStubs) return 0;
        for (UInt32 i = 0; i < 1024; i++) {
            W16(gStubs + 4 * i, (UInt16)(0xAC00 | i));      /* Toolbox, auto-pop */
            W16(gStubs + 4 * i + 2, 0x4E75);                /* RTS, never reached */
        }
        for (UInt32 i = 0; i < 256; i++) {
            W16(gStubs + 4 * (1024 + i), (UInt16)(0xA000 | i));
            W16(gStubs + 4 * (1024 + i) + 2, 0x4E75);       /* an OS trap returns here */
        }
    }
    int slot = M68K_TrapSlot(trapWord);
    if (slot < 0 || !gM68KApp->trapHandlers[slot]) trapWord = 0xA89F;   /* _Unimplemented */
    return (trapWord & 0x0800) ? gStubs + 4 * (trapWord & 0x3FF)
                               : gStubs + 4 * (1024 + (trapWord & 0xFF));
}

/* GetTrapAddress (OS): D0 the trap number, the address in A0. NGetTrapAddress
 * says which kind with bit 9 of the trap word; the old call tells by number. */
TRAP(Trap_GetTrapAddress) {
    UNUSED;
    UInt16 n = (UInt16)D(0);
    UInt16 word;
    if (gM68KApp->currentTrap & 0x0200) {
        word = (gM68KApp->currentTrap & 0x0400) ? (UInt16)(0xA800 | (n & 0x3FF))
                                                : (UInt16)(0xA000 | (n & 0xFF));
    } else if ((n & 0xF000) == 0xA000) {
        word = n;
    } else {
        word = (n & 0x1FF) >= 0x50 ? (UInt16)(0xA800 | (n & 0x3FF)) : (UInt16)(0xA000 | (n & 0xFF));
    }
    A(0) = StubFor(word);
    D(0) = 0;
    return noErr;
}

/* SetTrapAddress: patching the system is not something a program here can
 * do; the call is accepted and changes nothing */
TRAP(Trap_SetTrapAddress) { UNUSED; D(0) = 0; return noErr; }

void M68KUtils_Finish(void) {
    gStubs = 0;             /* both were in the program's heap, now gone */
    gScrapStuff = 0;
}

TRAP(Trap_Debugger) { UNUSED; return noErr; }
TRAP(Trap_DebugStr) { UNUSED; (void)Pop32(); return noErr; }

const M68KTrapEntry kM68KUtilityTraps[] = {
    { 0xA9EE, Trap_Pack7 },         { 0xA9ED, Trap_Pack6 },         { 0xA039, Trap_ReadDateTime },
    { 0xA9C6, Trap_Secs2Date },     { 0xA9C7, Trap_Date2Secs },
    { 0xA9FC, Trap_ZeroScrap },     { 0xA9FE, Trap_PutScrap },      { 0xA9FD, Trap_GetScrap },
    { 0xA9FB, Trap_LoadScrap },     { 0xA9FA, Trap_UnloadScrap },   { 0xA9F9, Trap_InfoScrap },
    { 0xA054, Trap_UprString },     { 0xA03C, Trap_CmpString },     { 0xA050, Trap_RelString },
    { 0xA906, Trap_NewString },     { 0xA907, Trap_SetString },     { 0xA9E0, Trap_Munger },
    { 0xA868, Trap_FixMul },        { 0xA869, Trap_FixRatio },      { 0xA86C, Trap_FixRound },
    { 0xA86A, Trap_HiWord },        { 0xA86B, Trap_LoWord },        { 0xA867, Trap_LongMul },
    { 0xA858, Trap_BitAnd },        { 0xA85B, Trap_BitOr },         { 0xA859, Trap_BitXor },
    { 0xA85A, Trap_BitNot },        { 0xA85C, Trap_BitShift },      { 0xA85D, Trap_BitTst },
    { 0xA85E, Trap_BitSet },        { 0xA85F, Trap_BitClr },        { 0xA866, Trap_StuffHex },
    { 0xA046, Trap_GetTrapAddress },{ 0xA047, Trap_SetTrapAddress },
    { 0xA9FF, Trap_Debugger },      { 0xABFF, Trap_DebugStr },
};
const int kM68KUtilityTrapCount = (int)(sizeof(kM68KUtilityTraps) / sizeof(kM68KUtilityTraps[0]));
