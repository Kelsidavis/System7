/*
 * M68K68020.c - the instructions the 68020 added for programs
 *
 * What a program compiled for the 68020 (and so the 68030 and 68040,
 * which add nothing a program uses but MOVE16 and the FPU) can contain:
 * 32-bit multiply and divide, the bit-field instructions, EXTB, LINK.L,
 * CHK.L, CHK2 and CMP2, CAS and CAS2, PACK and UNPK, RTD, TRAPcc and BKPT.
 * The 68020's addressing modes are in M68KDecode.c, its 32-bit branches
 * with the others in M68KOpcodes.c. The supervisor's instructions (MOVEC,
 * MOVES, the MMU's and caches') a program does not run, and CALLM and RTM
 * only the 68020 ever had and nothing used.
 *
 * Source: MC68020 User's Manual, third edition, and the M68000 Family
 * Programmer's Reference Manual.
 */

#include "CPU/M68KInterp.h"
#include "CPU/M68KOpcodes.h"
#include "M68KDecode.h"
#include "System71StdLib.h"

/* ------------------------------------------------------------------------
 * Condition codes
 * ------------------------------------------------------------------------ */

static void SetFlags(M68KAddressSpace* as, Boolean n, Boolean z, Boolean v, Boolean c) {
    UInt16 sr = as->regs.sr & ~(CCR_N | CCR_Z | CCR_V | CCR_C);
    if (n) sr |= CCR_N;
    if (z) sr |= CCR_Z;
    if (v) sr |= CCR_V;
    if (c) sr |= CCR_C;
    as->regs.sr = sr;
}

/* As CMP sets them: dst - src, at size */
static void CompareFlags(M68KAddressSpace* as, M68KSize size, UInt32 src, UInt32 dst) {
    UInt32 mask = SIZE_MASK(size), sign = SIZE_SIGN_BIT(size);
    src &= mask;
    dst &= mask;
    UInt32 r = (dst - src) & mask;
    Boolean v = ((dst ^ src) & (dst ^ r) & sign) != 0;
    SetFlags(as, (r & sign) != 0, r == 0, v, src > dst);
}

static UInt32 GetReg(M68KAddressSpace* as, UInt8 da) {
    return (da & 8) ? as->regs.a[da & 7] : as->regs.d[da & 7];
}

/* A data register written at a size: the rest of it kept */
static void SetDataSized(M68KAddressSpace* as, UInt8 r, M68KSize size, UInt32 v) {
    UInt32 mask = SIZE_MASK(size);
    as->regs.d[r] = (as->regs.d[r] & ~mask) | (v & mask);
}

/* ------------------------------------------------------------------------
 * MULS.L, MULU.L: 0100 1100 00 <ea>, then 0 Dl s sz 0000000 Dh
 * ------------------------------------------------------------------------ */

void M68K_Op_MULL(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 ext = M68K_Fetch16(as);
    UInt32 src = M68K_EA_Read(as, (opcode >> 3) & 7, opcode & 7, SIZE_LONG);
    UInt8 dl = (ext >> 12) & 7, dh = ext & 7;
    Boolean isSigned = (ext & 0x0800) != 0, quad = (ext & 0x0400) != 0;
    UInt32 dst = as->regs.d[dl];
    UInt64 product;
    Boolean overflow;
    if (isSigned) {
        SInt64 p = (SInt64)(SInt32)src * (SInt64)(SInt32)dst;
        product = (UInt64)p;
        overflow = p != (SInt64)(SInt32)p;
    } else {
        product = (UInt64)src * (UInt64)dst;
        overflow = (product >> 32) != 0;
    }
    if (quad) {
        as->regs.d[dl] = (UInt32)product;
        as->regs.d[dh] = (UInt32)(product >> 32);
        SetFlags(as, (product >> 63) != 0, product == 0, false, false);
    } else {
        as->regs.d[dl] = (UInt32)product;
        SetFlags(as, ((UInt32)product >> 31) != 0, (UInt32)product == 0, overflow, false);
    }
}

/* ------------------------------------------------------------------------
 * DIVS.L, DIVU.L, DIVSL.L, DIVUL.L: 0100 1100 01 <ea>, then 0 Dq s sz 0 Dr.
 * With sz, Dr:Dq is a 64-bit dividend. Without it Dq is divided alone, and
 * the remainder goes to Dr unless Dr is Dq. On overflow V is set and the
 * registers are left as they were.
 * ------------------------------------------------------------------------ */

void M68K_Op_DIVL(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 ext = M68K_Fetch16(as);
    UInt32 divisor = M68K_EA_Read(as, (opcode >> 3) & 7, opcode & 7, SIZE_LONG);
    UInt8 dq = (ext >> 12) & 7, dr = ext & 7;
    Boolean isSigned = (ext & 0x0800) != 0, quad = (ext & 0x0400) != 0;
    if (divisor == 0) {
        M68K_Fault(as, "Division by zero");
        return;
    }
    UInt32 quotient, remainder;
    if (isSigned) {
        SInt64 dividend = quad ? (SInt64)(((UInt64)as->regs.d[dr] << 32) | as->regs.d[dq])
                               : (SInt64)(SInt32)as->regs.d[dq];
        SInt64 d = (SInt32)divisor;
        if (dividend == INT64_MIN && d == -1) { SetFlags(as, false, false, true, false); return; }
        SInt64 q = dividend / d, r = dividend % d;
        if (q != (SInt64)(SInt32)q) { SetFlags(as, false, false, true, false); return; }
        quotient = (UInt32)q;
        remainder = (UInt32)r;
    } else {
        UInt64 dividend = quad ? (((UInt64)as->regs.d[dr] << 32) | as->regs.d[dq]) : as->regs.d[dq];
        UInt64 q = dividend / divisor, r = dividend % divisor;
        if (q >> 32) { SetFlags(as, false, false, true, false); return; }
        quotient = (UInt32)q;
        remainder = (UInt32)r;
    }
    if (quad || dr != dq) as->regs.d[dr] = remainder;
    as->regs.d[dq] = quotient;
    SetFlags(as, (quotient >> 31) != 0, quotient == 0, false, false);
}

/* EXTB.L Dn: 0100 1001 1100 0rrr - a byte sign-extended to 32 bits */
void M68K_Op_EXTB(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 r = opcode & 7;
    UInt32 v = (UInt32)SIGN_EXTEND_BYTE(as->regs.d[r] & 0xFF);
    as->regs.d[r] = v;
    SetFlags(as, (v >> 31) != 0, v == 0, false, false);
}

/* LINK.L An,#d32: 0100 1000 0000 1rrr */
void M68K_Op_LINKL(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 r = opcode & 7;
    SInt32 disp = (SInt32)M68K_Fetch32(as);
    as->regs.a[7] -= 4;
    M68K_Write32(as, as->regs.a[7], as->regs.a[r]);
    as->regs.a[r] = as->regs.a[7];
    as->regs.a[7] += (UInt32)disp;
}

/* CHK.L <ea>,Dn: 0100 rrr1 00 <ea> */
void M68K_Op_CHKL(M68KAddressSpace* as, UInt16 opcode)
{
    SInt32 bound = (SInt32)M68K_EA_Read(as, (opcode >> 3) & 7, opcode & 7, SIZE_LONG);
    SInt32 value = (SInt32)as->regs.d[(opcode >> 9) & 7];
    if (value < 0) {
        as->regs.sr |= CCR_N;
        M68K_Fault(as, "CHK failed: value < 0");
    } else if (value > bound) {
        as->regs.sr &= ~CCR_N;
        M68K_Fault(as, "CHK failed: value > bound");
    }
}

/* ------------------------------------------------------------------------
 * CMP2 and CHK2: 0000 0ss0 11 <ea>, then D/A reg c 00000000000 - a register
 * against a lower and an upper bound at <ea>. Z if it equals either, C if
 * it is outside them; CHK2 traps on C. Bounds whose lower half has its top
 * bit set are compared as signed (an address register always is).
 * ------------------------------------------------------------------------ */

void M68K_Op_CMP2(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 ext = M68K_Fetch16(as);
    M68KSize size = (M68KSize)((opcode >> 9) & 3);
    UInt32 addr = M68K_EA_ComputeAddress(as, (opcode >> 3) & 7, opcode & 7, size);
    UInt8 da = (ext >> 12) & 0xF;
    Boolean isAddr = (da & 8) != 0;
    UInt32 mask = SIZE_MASK(size), sign = SIZE_SIGN_BIT(size);
    SInt64 lower, upper, value;
    UInt32 lo, hi;
    switch (size) {
        case SIZE_BYTE: lo = M68K_Read8(as, addr); hi = M68K_Read8(as, addr + 1); break;
        case SIZE_WORD: lo = M68K_Read16(as, addr); hi = M68K_Read16(as, addr + 2); break;
        default:        lo = M68K_Read32(as, addr); hi = M68K_Read32(as, addr + 4); break;
    }
    UInt32 reg = GetReg(as, da);
    Boolean signedCompare = isAddr || (lo & sign);
    if (signedCompare) {
        lower = size == SIZE_BYTE ? (SInt8)lo : size == SIZE_WORD ? (SInt16)lo : (SInt32)lo;
        upper = size == SIZE_BYTE ? (SInt8)hi : size == SIZE_WORD ? (SInt16)hi : (SInt32)hi;
        if (isAddr) value = (SInt32)reg;
        else value = size == SIZE_BYTE ? (SInt8)reg : size == SIZE_WORD ? (SInt16)reg : (SInt32)reg;
    } else {
        lower = lo;
        upper = hi;
        value = reg & mask;
    }
    Boolean equal = value == lower || value == upper;
    Boolean outside = value < lower || value > upper;
    UInt16 sr = as->regs.sr & ~(CCR_Z | CCR_C);
    if (equal) sr |= CCR_Z;
    if (outside) sr |= CCR_C;
    as->regs.sr = sr;
    if (outside && (ext & 0x0800)) M68K_Fault(as, "CHK2 failed: out of bounds");
}

/* ------------------------------------------------------------------------
 * CAS Dc,Du,<ea>: 0000 1ss0 11 <ea>, then 0000000 Du 000 Dc. The operand
 * compared with Dc; equal, Du is written to it, otherwise it goes into Dc.
 * CAS2 (ea field 111100): two registers' operands, both or neither.
 * ------------------------------------------------------------------------ */

void M68K_Op_CAS(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 sz = (opcode >> 9) & 3;               /* 1 byte, 2 word, 3 long */
    M68KSize size = (M68KSize)(sz - 1);

    if ((opcode & 0x3F) == 0x3C) {
        /* CAS2 Dc1:Dc2,Du1:Du2,(Rn1):(Rn2) */
        if (size == SIZE_BYTE) { M68K_Fault(as, "CAS2.B is not an instruction"); return; }
        UInt16 e1 = M68K_Fetch16(as), e2 = M68K_Fetch16(as);
        UInt32 a1 = GetReg(as, (e1 >> 12) & 0xF), a2 = GetReg(as, (e2 >> 12) & 0xF);
        UInt8 dc1 = e1 & 7, du1 = (e1 >> 6) & 7, dc2 = e2 & 7, du2 = (e2 >> 6) & 7;
        UInt32 v1 = size == SIZE_WORD ? M68K_Read16(as, a1) : M68K_Read32(as, a1);
        UInt32 v2 = size == SIZE_WORD ? M68K_Read16(as, a2) : M68K_Read32(as, a2);
        CompareFlags(as, size, as->regs.d[dc1], v1);
        if (as->regs.sr & CCR_Z) CompareFlags(as, size, as->regs.d[dc2], v2);
        if (as->regs.sr & CCR_Z) {
            if (size == SIZE_WORD) { M68K_Write16(as, a1, (UInt16)as->regs.d[du1]); M68K_Write16(as, a2, (UInt16)as->regs.d[du2]); }
            else { M68K_Write32(as, a1, as->regs.d[du1]); M68K_Write32(as, a2, as->regs.d[du2]); }
        } else {
            SetDataSized(as, dc1, size, v1);
            SetDataSized(as, dc2, size, v2);
        }
        return;
    }

    UInt16 ext = M68K_Fetch16(as);
    UInt8 dc = ext & 7, du = (ext >> 6) & 7;
    UInt8 mode = (opcode >> 3) & 7, reg = opcode & 7;
    UInt32 v = M68K_EA_ReadRMW(as, mode, reg, size);
    CompareFlags(as, size, as->regs.d[dc], v);
    if (as->regs.sr & CCR_Z) M68K_EA_WriteRMW(as, mode, reg, size, as->regs.d[du]);
    else SetDataSized(as, dc, size, v);
}

/* ------------------------------------------------------------------------
 * Bit fields: 1110 1ttt 11 <ea>, then 0 Dn Do offset Dw width.
 *
 * A field is width bits (1-32) starting offset bits from the most
 * significant bit of a register - wrapping round it - or of the byte at
 * <ea>, in which case a register offset is signed and may reach any byte.
 * N is the field's top bit, Z whether it is all clear; V and C are clear.
 * ------------------------------------------------------------------------ */

enum { kBFTST = 0, kBFEXTU, kBFCHG, kBFEXTS, kBFCLR, kBFFFO, kBFSET, kBFINS };

void M68K_Op_Bitfield(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 ext = M68K_Fetch16(as);
    int op = (opcode >> 8) & 7;
    UInt8 mode = (opcode >> 3) & 7, reg = opcode & 7;
    UInt8 dn = (ext >> 12) & 7;
    SInt32 offset = (ext & 0x0800) ? (SInt32)as->regs.d[(ext >> 6) & 7] : (SInt32)((ext >> 6) & 31);
    UInt32 width = (ext & 0x0020) ? (as->regs.d[ext & 7] & 31) : (ext & 31);
    if (width == 0) width = 32;
    UInt32 ones = width == 32 ? 0xFFFFFFFFu : ((1u << width) - 1);

    UInt32 field;
    UInt32 addr = 0;
    UInt64 window = 0;                          /* the bytes the field lies in */
    int bytes = 0, shift = 0;
    if (mode == MODE_Dn) {
        UInt32 rot = (UInt32)offset & 31;
        UInt32 v = as->regs.d[reg];
        UInt32 aligned = rot ? (v << rot) | (v >> (32 - rot)) : v;   /* the field at the top */
        field = aligned >> (32 - width);
    } else {
        addr = M68K_EA_ComputeAddress(as, mode, reg, SIZE_BYTE);
        addr += (UInt32)(offset >> 3);          /* arithmetic: a negative offset goes back */
        int bit = offset & 7;
        bytes = (bit + (int)width + 7) / 8;
        for (int i = 0; i < bytes; i++) window = (window << 8) | M68K_Read8(as, addr + (UInt32)i);
        shift = bytes * 8 - bit - (int)width;
        field = (UInt32)(window >> shift) & ones;
    }

    UInt32 flagsOf = field;
    UInt32 newField = field;
    Boolean write = false;
    switch (op) {
    case kBFTST: break;
    case kBFEXTU: as->regs.d[dn] = field; break;
    case kBFEXTS:
        as->regs.d[dn] = (width < 32 && (field >> (width - 1)) & 1) ? field | ~ones : field;
        break;
    case kBFFFO: {
        UInt32 i = 0;
        while (i < width && !((field >> (width - 1 - i)) & 1)) i++;
        as->regs.d[dn] = (UInt32)offset + i;
        break;
    }
    case kBFCHG: newField = ~field & ones; write = true; break;
    case kBFCLR: newField = 0; write = true; break;
    case kBFSET: newField = ones; write = true; break;
    case kBFINS:
        newField = as->regs.d[dn] & ones;
        flagsOf = newField;
        write = true;
        break;
    }
    SetFlags(as, (flagsOf >> (width - 1)) & 1, flagsOf == 0, false, false);

    if (!write) return;
    if (mode == MODE_Dn) {
        UInt32 rot = (UInt32)offset & 31;
        UInt32 fieldMask = (width == 32 ? 0xFFFFFFFFu : ones << (32 - width));
        UInt32 placedMask = rot ? (fieldMask >> rot) | (fieldMask << (32 - rot)) : fieldMask;
        UInt32 placed = newField << (32 - width);
        if (width == 32) placed = newField;
        placed = rot ? (placed >> rot) | (placed << (32 - rot)) : placed;
        as->regs.d[reg] = (as->regs.d[reg] & ~placedMask) | (placed & placedMask);
    } else {
        UInt64 m = (UInt64)ones << shift;
        window = (window & ~m) | ((UInt64)newField << shift);
        for (int i = bytes - 1; i >= 0; i--) {
            M68K_Write8(as, addr + (UInt32)i, (UInt8)window);
            window >>= 8;
        }
    }
}

/* ------------------------------------------------------------------------
 * PACK and UNPK: 1000 yyy1 0100 rxxx and 1000 yyy1 1000 rxxx, then the
 * adjustment. BCD digits between a byte and a word's two low nibbles,
 * register to register or -(Ax) to -(Ay). The condition codes are left.
 * ------------------------------------------------------------------------ */

void M68K_Op_PACK(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 adj = M68K_Fetch16(as);
    UInt8 x = opcode & 7, y = (opcode >> 9) & 7;
    if (opcode & 8) {
        as->regs.a[x] -= 1;
        UInt16 lo = M68K_Read8(as, as->regs.a[x]);
        as->regs.a[x] -= 1;
        UInt16 hi = M68K_Read8(as, as->regs.a[x]);
        UInt16 v = (UInt16)(((hi << 8) | lo) + adj);
        as->regs.a[y] -= 1;
        M68K_Write8(as, as->regs.a[y], (UInt8)(((v >> 4) & 0xF0) | (v & 0x0F)));
    } else {
        UInt16 v = (UInt16)((as->regs.d[x] & 0xFFFF) + adj);
        SetDataSized(as, y, SIZE_BYTE, ((v >> 4) & 0xF0) | (v & 0x0F));
    }
}

void M68K_Op_UNPK(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 adj = M68K_Fetch16(as);
    UInt8 x = opcode & 7, y = (opcode >> 9) & 7;
    if (opcode & 8) {
        as->regs.a[x] -= 1;
        UInt16 b = M68K_Read8(as, as->regs.a[x]);
        UInt16 v = (UInt16)((((b << 4) & 0x0F00) | (b & 0x000F)) + adj);
        as->regs.a[y] -= 1;
        M68K_Write8(as, as->regs.a[y], (UInt8)v);
        as->regs.a[y] -= 1;
        M68K_Write8(as, as->regs.a[y], (UInt8)(v >> 8));
    } else {
        UInt16 b = as->regs.d[x] & 0xFF;
        UInt16 v = (UInt16)((((b << 4) & 0x0F00) | (b & 0x000F)) + adj);
        SetDataSized(as, y, SIZE_WORD, v);
    }
}

/* RTD #d16: 0100 1110 0111 0100 - return, then the arguments off the stack */
void M68K_Op_RTD(M68KAddressSpace* as, UInt16 opcode)
{
    (void)opcode;
    SInt32 disp = SIGN_EXTEND_WORD(M68K_Fetch16(as));
    UInt32 ret = M68K_Read32(as, as->regs.a[7]);
    as->regs.a[7] += 4 + (UInt32)disp;
    as->regs.pc = ret;
}

/* TRAPcc: 0101 cccc 1111 1xxx - 010 a word operand, 011 a long, 100 none */
void M68K_Op_TRAPcc(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 form = opcode & 7;
    if (form == 2) (void)M68K_Fetch16(as);
    else if (form == 3) (void)M68K_Fetch32(as);
    if (M68K_TestCondition(as->regs.sr, (M68KCondition)((opcode >> 8) & 0xF))) {
        M68K_Fault(as, "TRAPcc: condition true");
    }
}

/* BKPT #n: 0100 1000 0100 1nnn - for a debugger; with none, illegal */
void M68K_Op_BKPT(M68KAddressSpace* as, UInt16 opcode)
{
    (void)opcode;
    M68K_Fault(as, "BKPT with no debugger");
}
