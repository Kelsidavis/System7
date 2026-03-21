/*
 * M68KOpcodes.c - 68K Instruction Handlers (Phase-1 MVP)
 *
 * Implements opcode handlers for the MVP instruction set:
 * MOVE, MOVEA, LEA, PEA, CLR, NOT, ADD, SUB, CMP, LINK, UNLK,
 * JSR, JMP, BRA, Bcc, BSR, RTS, TRAP
 */

#include "CPU/M68KInterp.h"
#include "CPU/M68KOpcodes.h"
#include "System71StdLib.h"
#include "CPU/CPULogging.h"
#include <string.h>

/*
 * External declarations from M68KDecode.c
 */
extern UInt16 M68K_Fetch16(M68KAddressSpace* as);
extern UInt32 M68K_Fetch32(M68KAddressSpace* as);
extern UInt8 M68K_Read8(M68KAddressSpace* as, UInt32 addr);
extern UInt16 M68K_Read16(M68KAddressSpace* as, UInt32 addr);
extern UInt32 M68K_Read32(M68KAddressSpace* as, UInt32 addr);
extern void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value);
extern void M68K_Write16(M68KAddressSpace* as, UInt32 addr, UInt16 value);
extern void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value);
extern UInt32 M68K_EA_ComputeAddress(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size);
extern UInt32 M68K_EA_Read(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size);
extern void M68K_EA_Write(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size, UInt32 value);

/*
 * Exception Handler - Raise 68K exception
 */
static void M68K_RaiseException(M68KAddressSpace* as, UInt16 vector, const char* reason)
{
    UInt32 vectorAddr;
    UInt32 handlerPC;
    const char* vecName;

    /* Map vector number to name */
    switch (vector) {
        case M68K_VEC_BUS_ERROR:      vecName = "BUS ERROR"; break;
        case M68K_VEC_ADDRESS_ERROR:  vecName = "ADDRESS ERROR"; break;
        case M68K_VEC_ILLEGAL:        vecName = "ILLEGAL"; break;
        case M68K_VEC_DIVIDE_ZERO:    vecName = "DIVIDE_ZERO"; break;
        case M68K_VEC_CHK:            vecName = "CHK"; break;
        case M68K_VEC_TRAPV:          vecName = "TRAPV"; break;
        case M68K_VEC_PRIVILEGE:      vecName = "PRIVILEGE"; break;
        case M68K_VEC_TRACE:          vecName = "TRACE"; break;
        case M68K_VEC_LINE_A:         vecName = "LINE_A"; break;
        case M68K_VEC_LINE_F:         vecName = "LINE_F"; break;
        default:                      vecName = "UNKNOWN"; break;
    }

    M68K_LOG_ERROR("EXCEPTION vec=%d (%s) at PC=0x%08X: %s\n",
                 vector, vecName, as->regs.pc, reason);

    as->lastException = vector;

    /* Read exception vector from memory (vectors at 0x0000 + vec*4) */
    vectorAddr = vector * 4;
    if (vectorAddr + 3 < M68K_MAX_ADDR) {
        extern UInt8 M68K_Read8(M68KAddressSpace* as, UInt32 addr);
        handlerPC = (M68K_Read8(as, vectorAddr) << 24) |
                   (M68K_Read8(as, vectorAddr + 1) << 16) |
                   (M68K_Read8(as, vectorAddr + 2) << 8) |
                   M68K_Read8(as, vectorAddr + 3);

        /* If handler is NULL or invalid, halt */
        if (handlerPC == 0 || handlerPC >= M68K_MAX_ADDR) {
            M68K_LOG_ERROR("Exception handler NULL or invalid (0x%08X), halting\n", handlerPC);
            as->halted = true;
        } else {
            /* For now, just log and halt (RTE stub not yet implemented) */
            M68K_LOG_WARN("Exception handler at 0x%08X (not invoking yet, halting)\n", handlerPC);
            as->halted = true;
        }
    } else {
        M68K_LOG_ERROR("Exception vector table not initialized, halting\n");
        as->halted = true;
    }
}

/*
 * Fault Handler (legacy wrapper)
 */
void M68K_Fault(M68KAddressSpace* as, const char* reason)
{
    /* Classify fault and raise appropriate exception */
    if (strstr(reason, "Address error") || strstr(reason, "odd address")) {
        M68K_RaiseException(as, M68K_VEC_ADDRESS_ERROR, reason);
    } else if (strstr(reason, "out of bounds") || strstr(reason, "unmapped")) {
        M68K_RaiseException(as, M68K_VEC_BUS_ERROR, reason);
    } else if (strstr(reason, "Illegal") || strstr(reason, "ILLEGAL")) {
        M68K_RaiseException(as, M68K_VEC_ILLEGAL, reason);
    } else {
        /* Generic fault - use ILLEGAL */
        M68K_RaiseException(as, M68K_VEC_ILLEGAL, reason);
    }
}

/*
 * CCR Flag Helpers
 */
static inline void M68K_SetFlag(M68KAddressSpace* as, UInt16 flag)
{
    as->regs.sr |= flag;
}

static inline void M68K_ClearFlag(M68KAddressSpace* as, UInt16 flag)
{
    as->regs.sr &= ~flag;
}

static inline Boolean M68K_TestFlag(M68KAddressSpace* as, UInt16 flag)
{
    return (as->regs.sr & flag) != 0;
}

static void M68K_SetNZ(M68KAddressSpace* as, UInt32 value, M68KSize size)
{
    /* Set Z if value is zero */
    if ((value & SIZE_MASK(size)) == 0) {
        M68K_SetFlag(as, CCR_Z);
    } else {
        M68K_ClearFlag(as, CCR_Z);
    }

    /* Set N if MSB is set */
    if (value & SIZE_SIGN_BIT(size)) {
        M68K_SetFlag(as, CCR_N);
    } else {
        M68K_ClearFlag(as, CCR_N);
    }
}

/*
 * Condition Code Testing
 */
Boolean M68K_TestCondition(UInt16 sr, M68KCondition cc)
{
    Boolean c = (sr & CCR_C) != 0;
    Boolean v = (sr & CCR_V) != 0;
    Boolean z = (sr & CCR_Z) != 0;
    Boolean n = (sr & CCR_N) != 0;

    switch (cc) {
        case CC_T:  return true;
        case CC_F:  return false;
        case CC_HI: return !c && !z;
        case CC_LS: return c || z;
        case CC_CC: return !c;
        case CC_CS: return c;
        case CC_NE: return !z;
        case CC_EQ: return z;
        case CC_VC: return !v;
        case CC_VS: return v;
        case CC_PL: return !n;
        case CC_MI: return n;
        case CC_GE: return (n && v) || (!n && !v);
        case CC_LT: return (n && !v) || (!n && v);
        case CC_GT: return !z && ((n && v) || (!n && !v));
        case CC_LE: return z || (n && !v) || (!n && v);
        default:    return false;
    }
}

/*
 * Stack Push/Pop Helpers
 */
static void M68K_Push32(M68KAddressSpace* as, UInt32 value)
{
    as->regs.a[7] -= 4;
    M68K_Write32(as, as->regs.a[7], value);
}

static UInt32 M68K_Pop32(M68KAddressSpace* as)
{
    UInt32 value = M68K_Read32(as, as->regs.a[7]);
    as->regs.a[7] += 4;
    return value;
}

static void __attribute__((unused)) M68K_Push16(M68KAddressSpace* as, UInt16 value)
{
    as->regs.a[7] -= 2;
    M68K_Write16(as, as->regs.a[7], value);
}

static UInt16 M68K_Pop16(M68KAddressSpace* as)
{
    UInt16 value = M68K_Read16(as, as->regs.a[7]);
    as->regs.a[7] += 2;
    return value;
}

/*
 * MOVE - Move data
 */
void M68K_Op_MOVE(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size_bits = (opcode >> 12) & 3;
    M68KSize size;
    UInt8 dst_reg = (opcode >> 9) & 7;
    UInt8 dst_mode = (opcode >> 6) & 7;
    UInt8 src_mode = (opcode >> 3) & 7;
    UInt8 src_reg = opcode & 7;
    UInt32 value;

    /* Decode size: 01=byte, 11=word, 10=long */
    if (size_bits == 1) size = SIZE_BYTE;
    else if (size_bits == 3) size = SIZE_WORD;
    else if (size_bits == 2) size = SIZE_LONG;
    else {
        M68K_Fault(as, "Invalid MOVE size");
        return;
    }

    /* Read source */
    value = M68K_EA_Read(as, src_mode, src_reg, size);

    /* Write destination */
    M68K_EA_Write(as, dst_mode, dst_reg, size, value);

    /* Set flags */
    M68K_SetNZ(as, value, size);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * MOVEA - Move to address register
 */
void M68K_Op_MOVEA(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size_bits = (opcode >> 12) & 3;
    M68KSize size = (size_bits == 3) ? SIZE_WORD : SIZE_LONG;
    UInt8 dst_reg = (opcode >> 9) & 7;
    UInt8 src_mode = (opcode >> 3) & 7;
    UInt8 src_reg = opcode & 7;
    UInt32 value;

    /* Read source */
    value = M68K_EA_Read(as, src_mode, src_reg, size);

    /* Sign-extend if word */
    if (size == SIZE_WORD) {
        value = SIGN_EXTEND_WORD(value);
    }

    /* Write to address register */
    as->regs.a[dst_reg] = value;

    /* MOVEA does not affect flags */
}

/*
 * LEA - Load Effective Address
 */
void M68K_Op_LEA(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dst_reg = (opcode >> 9) & 7;
    UInt8 src_mode = (opcode >> 3) & 7;
    UInt8 src_reg = opcode & 7;
    UInt32 addr;

    /* Compute effective address */
    addr = M68K_EA_ComputeAddress(as, src_mode, src_reg, SIZE_LONG);

    /* Write to address register */
    as->regs.a[dst_reg] = addr;

    /* LEA does not affect flags */
}

/*
 * PEA - Push Effective Address
 */
void M68K_Op_PEA(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 src_mode = (opcode >> 3) & 7;
    UInt8 src_reg = opcode & 7;
    UInt32 addr;

    /* Compute effective address */
    addr = M68K_EA_ComputeAddress(as, src_mode, src_reg, SIZE_LONG);

    /* Push to stack */
    M68K_Push32(as, addr);

    /* PEA does not affect flags */
}

/*
 * CLR - Clear operand
 */
void M68K_Op_CLR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;

    /* Write zero */
    M68K_EA_Write(as, mode, reg, size, 0);

    /* Set flags: Z=1, N=V=C=0 */
    M68K_SetFlag(as, CCR_Z);
    M68K_ClearFlag(as, CCR_N | CCR_V | CCR_C);
}

/*
 * NOT - Logical complement
 */
void M68K_Op_NOT(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 value;

    /* Read value */
    value = M68K_EA_Read(as, mode, reg, size);

    /* Complement */
    value = (~value) & SIZE_MASK(size);

    /* Write back */
    M68K_EA_Write(as, mode, reg, size, value);

    /* Set flags */
    M68K_SetNZ(as, value, size);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * ADD - Add to data register
 */
void M68K_Op_ADD(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;  /* 0=EA+Dn->EA, 1=Dn+EA->Dn */
    UInt8 size = (opcode >> 6) & 3;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src, dst, result;
    UInt32 mask = SIZE_MASK(size);

    if (dir == 1) {
        /* Dn + EA -> Dn */
        src = M68K_EA_Read(as, ea_mode, ea_reg, size);
        dst = as->regs.d[reg] & mask;
        result = (dst + src) & mask;
        as->regs.d[reg] = (as->regs.d[reg] & ~mask) | result;
    } else {
        /* EA + Dn -> EA */
        dst = M68K_EA_Read(as, ea_mode, ea_reg, size);
        src = as->regs.d[reg] & mask;
        result = (dst + src) & mask;
        M68K_EA_Write(as, ea_mode, ea_reg, size, result);
    }

    /* Set flags */
    M68K_SetNZ(as, result, size);

    /* Set C and X if carry occurred */
    if ((dst + src) > mask) {
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    /* Set V if signed overflow occurred (src and dst same sign, result different) */
    {
        UInt32 sign_bit = SIZE_SIGN_BIT(size);
        Boolean src_neg = (src & sign_bit) != 0;
        Boolean dst_neg = (dst & sign_bit) != 0;
        Boolean res_neg = (result & sign_bit) != 0;

        if (src_neg == dst_neg && res_neg != dst_neg) {
            M68K_SetFlag(as, CCR_V);
        } else {
            M68K_ClearFlag(as, CCR_V);
        }
    }
}

/*
 * SUB - Subtract from data register
 */
void M68K_Op_SUB(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;  /* 0=Dn-EA->EA, 1=Dn-EA->Dn */
    UInt8 size = (opcode >> 6) & 3;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src, dst, result;
    UInt32 mask = SIZE_MASK(size);

    if (dir == 1) {
        /* Dn - EA -> Dn */
        dst = as->regs.d[reg] & mask;
        src = M68K_EA_Read(as, ea_mode, ea_reg, size);
        result = (dst - src) & mask;
        as->regs.d[reg] = (as->regs.d[reg] & ~mask) | result;
    } else {
        /* EA - Dn -> EA */
        dst = M68K_EA_Read(as, ea_mode, ea_reg, size);
        src = as->regs.d[reg] & mask;
        result = (dst - src) & mask;
        M68K_EA_Write(as, ea_mode, ea_reg, size, result);
    }

    /* Set flags */
    M68K_SetNZ(as, result, size);

    /* Set C and X if borrow occurred */
    if (src > dst) {
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    /* Set V if signed overflow occurred (src and dst different sign, result different from dst) */
    {
        UInt32 sign_bit = SIZE_SIGN_BIT(size);
        Boolean src_neg = (src & sign_bit) != 0;
        Boolean dst_neg = (dst & sign_bit) != 0;
        Boolean res_neg = (result & sign_bit) != 0;

        if (src_neg != dst_neg && res_neg != dst_neg) {
            M68K_SetFlag(as, CCR_V);
        } else {
            M68K_ClearFlag(as, CCR_V);
        }
    }
}

/*
 * CMP - Compare
 */
void M68K_Op_CMP(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src, dst, result;
    UInt32 mask = SIZE_MASK(size);

    /* Dn - EA */
    dst = as->regs.d[reg] & mask;
    src = M68K_EA_Read(as, ea_mode, ea_reg, size);
    result = (dst - src) & mask;

    /* Set flags (don't write result) */
    M68K_SetNZ(as, result, size);

    /* Set C if borrow occurred */
    if (src > dst) {
        M68K_SetFlag(as, CCR_C);
    } else {
        M68K_ClearFlag(as, CCR_C);
    }

    /* Clear V for MVP */
    M68K_ClearFlag(as, CCR_V);
}

/*
 * LINK - Link and allocate
 */
void M68K_Op_LINK(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = opcode & 7;
    SInt16 disp;

    /* Push An */
    M68K_Push32(as, as->regs.a[reg]);

    /* An = SP */
    as->regs.a[reg] = as->regs.a[7];

    /* SP = SP + displacement */
    disp = (SInt16)M68K_Fetch16(as);
    as->regs.a[7] += disp;

    /* LINK does not affect flags */
}

/*
 * UNLK - Unlink
 */
void M68K_Op_UNLK(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = opcode & 7;

    /* SP = An */
    as->regs.a[7] = as->regs.a[reg];

    /* Pop An */
    as->regs.a[reg] = M68K_Pop32(as);

    /* UNLK does not affect flags */
}

/*
 * JSR - Jump to subroutine
 */
void M68K_Op_JSR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 target;

    /* Compute target address */
    target = M68K_EA_ComputeAddress(as, mode, reg, SIZE_LONG);

    /* Push return address */
    M68K_Push32(as, as->regs.pc);

    /* Jump - log with A5-relative info if using d16(A5) */
    if (mode == MODE_An_DISP && reg == 5) {
        /* JSR d16(A5) - log A5-relative offset */
        SInt32 offsetFromA5 = (SInt32)target - (SInt32)as->regs.a[5];
        serial_printf("[M68K] JSR (A5%+d) -> 0x%08X\n", offsetFromA5, target);
    } else {
        serial_printf("[M68K] JSR 0x%08X -> 0x%08X\n", as->regs.pc - 2, target);
    }
    as->regs.pc = target;

    /* JSR does not affect flags */
}

/*
 * JMP - Jump
 */
void M68K_Op_JMP(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 target;

    /* Compute target address */
    target = M68K_EA_ComputeAddress(as, mode, reg, SIZE_LONG);

    /* Jump */
    serial_printf("[M68K] JMP 0x%08X -> 0x%08X\n", as->regs.pc - 2, target);
    as->regs.pc = target;

    /* JMP does not affect flags */
}

/*
 * BRA - Branch always
 */
void M68K_Op_BRA(M68KAddressSpace* as, UInt16 opcode)
{
    SInt32 disp;
    UInt32 target;

    disp = (SInt8)(opcode & 0xFF);
    if (disp == 0) {
        /* 16-bit displacement */
        disp = SIGN_EXTEND_WORD(M68K_Fetch16(as));
    }

    target = as->regs.pc + disp;
    serial_printf("[M68K] BRA 0x%08X -> 0x%08X (disp=%d)\n", as->regs.pc - 2, target, disp);
    as->regs.pc = target;

    /* BRA does not affect flags */
}

/*
 * BSR - Branch to subroutine
 */
void M68K_Op_BSR(M68KAddressSpace* as, UInt16 opcode)
{
    SInt32 disp;
    UInt32 target;

    disp = (SInt8)(opcode & 0xFF);
    if (disp == 0) {
        /* 16-bit displacement */
        disp = SIGN_EXTEND_WORD(M68K_Fetch16(as));
    }

    target = as->regs.pc + disp;

    /* Push return address */
    M68K_Push32(as, as->regs.pc);

    /* Jump */
    serial_printf("[M68K] BSR 0x%08X -> 0x%08X (disp=%d)\n", as->regs.pc - 2, target, disp);
    as->regs.pc = target;

    /* BSR does not affect flags */
}

/*
 * Bcc - Branch conditionally
 */
void M68K_Op_Bcc(M68KAddressSpace* as, UInt16 opcode)
{
    M68KCondition cc = (opcode >> 8) & 0xF;
    SInt32 disp;
    UInt32 target;

    disp = (SInt8)(opcode & 0xFF);
    if (disp == 0) {
        /* 16-bit displacement */
        disp = SIGN_EXTEND_WORD(M68K_Fetch16(as));
    }

    if (M68K_TestCondition(as->regs.sr, cc)) {
        target = as->regs.pc + disp;
        serial_printf("[M68K] Bcc (cc=%d) taken: 0x%08X -> 0x%08X\n", cc, as->regs.pc - 2, target);
        as->regs.pc = target;
    } else {
        serial_printf("[M68K] Bcc (cc=%d) not taken\n", cc);
    }

    /* Bcc does not affect flags */
}

/*
 * RTS - Return from subroutine
 */
void M68K_Op_RTS(M68KAddressSpace* as, UInt16 opcode)
{
    UInt32 return_addr;

    (void)opcode;

    /* Pop return address */
    return_addr = M68K_Pop32(as);

    /* Jump */
    serial_printf("[M68K] RTS to 0x%08X\n", return_addr);
    as->regs.pc = return_addr;

    /* RTS does not affect flags */
}

/*
 * RTE - Return from exception (stub)
 * Encoding: 0100 1110 0111 0011 (0x4E73)
 */
void M68K_Op_RTE(M68KAddressSpace* as, UInt16 opcode)
{
    (void)opcode;

    serial_printf("[M68K] RTE (stub) at PC=0x%08X - halting\n", as->regs.pc - 2);

    /* For now, RTE is a stub that just halts */
    /* Full implementation would pop SR and PC from supervisor stack */
    as->halted = true;
}

/*
 * STOP - Load status register and stop (stub)
 * Encoding: 0100 1110 0111 0010 (0x4E72) + immediate SR value
 */
void M68K_Op_STOP(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 sr_value;

    (void)opcode;

    /* Fetch immediate SR value */
    sr_value = M68K_Fetch16(as);

    serial_printf("[M68K] STOP #0x%04X at PC=0x%08X - treated as NOP\n",
                 sr_value, as->regs.pc - 4);

    /* For now, STOP is a NOP (don't actually stop execution) */
    /* Full implementation would halt until interrupt */
}

/*
 * Scc - Set according to condition
 * Encoding: 0101 cccc 11xx xxxx (0x50C0-0x5FFF)
 * If condition true, set EA byte to 0xFF; else set to 0x00
 */
void M68K_Op_Scc(M68KAddressSpace* as, UInt16 opcode)
{
    M68KCondition cc = (M68KCondition)((opcode >> 8) & 0xF);
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt8 value;

    /* Test condition */
    if (M68K_TestCondition(as->regs.sr, cc)) {
        value = 0xFF;
        serial_printf("[M68K] Scc (cc=%d) true -> set 0xFF\n", cc);
    } else {
        value = 0x00;
        serial_printf("[M68K] Scc (cc=%d) false -> set 0x00\n", cc);
    }

    /* Write byte to EA */
    M68K_EA_Write(as, mode, reg, SIZE_BYTE, value);

    /* Scc does not affect flags */
}

/*
 * DBcc - Decrement and branch conditionally
 * Encoding: 0101 cccc 1100 1rrr (0x50C8-0x5FC8)
 * If cc false: Dn--, if Dn != -1 then branch; else fall through
 * If cc true: fall through
 */
void M68K_Op_DBcc(M68KAddressSpace* as, UInt16 opcode)
{
    M68KCondition cc = (M68KCondition)((opcode >> 8) & 0xF);
    UInt8 reg = opcode & 7;
    SInt16 disp;
    UInt32 target;

    /* Fetch displacement */
    disp = (SInt16)M68K_Fetch16(as);

    /* Test condition */
    if (!M68K_TestCondition(as->regs.sr, cc)) {
        /* Condition false - decrement and test */
        SInt16 counter = (SInt16)(as->regs.d[reg] & 0xFFFF);
        counter--;
        as->regs.d[reg] = (as->regs.d[reg] & 0xFFFF0000) | (counter & 0xFFFF);

        if (counter != -1) {
            /* Branch */
            target = (as->regs.pc - 2) + disp;  /* PC-2 because we already fetched disp */
            serial_printf("[M68K] DBcc (cc=%d) false, D%d=%d -> branch to 0x%08X\n",
                         cc, reg, counter, target);
            as->regs.pc = target;
        } else {
            /* Counter expired - fall through */
            serial_printf("[M68K] DBcc (cc=%d) false, D%d=-1 -> fall through\n", cc, reg);
        }
    } else {
        /* Condition true - fall through */
        serial_printf("[M68K] DBcc (cc=%d) true -> fall through\n", cc);
    }

    /* DBcc does not affect flags */
}

/*
 * TRAP - A-line trap
 */
void M68K_Op_TRAP(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 trap_num = opcode & 0x0FFF;
    UInt32 saved_pc = as->regs.pc;
    OSErr err;

    serial_printf("[M68K] TRAP $A%03X at PC=0x%08X\n", trap_num, saved_pc - 2);

    /* Look up trap handler */
    if (as->trapHandlers[trap_num & 0xFF]) {
        /* Call handler */
        err = as->trapHandlers[trap_num & 0xFF](
            as->trapContexts[trap_num & 0xFF],
            &as->regs.pc,
            as->regs.d  /* Pass registers array */
        );

        if (err != noErr) {
            serial_printf("[M68K] TRAP handler returned error %d\n", err);
            M68K_Fault(as, "TRAP handler error");
            return;
        }
    } else {
        serial_printf("[M68K] WARNING: Unhandled TRAP $A%03X\n", trap_num);
    }

    /* PC is now at next instruction (set by handler or unchanged) */
    /* TRAP does not affect flags */
}

/*
 * MOVEQ - Move quick (sign-extended 8-bit immediate to Dn)
 * Encoding: 0111 rrr0 dddd dddd
 * Format: MOVEQ #imm8, Dn
 */
void M68K_Op_MOVEQ(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dn = (opcode >> 9) & 7;
    SInt8 imm = opcode & 0xFF;
    SInt32 value = imm;  /* Sign-extend */

    /* Write to Dn */
    as->regs.d[dn] = value;

    /* Set flags based on result */
    M68K_ClearFlag(as, CCR_N | CCR_Z | CCR_V | CCR_C);
    if (value == 0) {
        M68K_SetFlag(as, CCR_Z);
    }
    if (value < 0) {
        M68K_SetFlag(as, CCR_N);
    }
}

/*
 * TST - Test operand (set flags based on operand value)
 * Encoding: 0100 1010 ssxx xrrr
 * Format: TST.<size> <ea>
 */
void M68K_Op_TST(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size_bits = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    M68KSize size;
    UInt32 value;
    SInt32 signed_value;

    /* Decode size */
    if (size_bits == 0) {
        size = SIZE_BYTE;
    } else if (size_bits == 1) {
        size = SIZE_WORD;
    } else if (size_bits == 2) {
        size = SIZE_LONG;
    } else {
        M68K_Fault(as, "Invalid TST size");
        return;
    }

    /* Read operand */
    value = M68K_EA_Read(as, mode, reg, size);

    /* Sign-extend for flag calculation */
    if (size == SIZE_BYTE) {
        signed_value = SIGN_EXTEND_BYTE(value);
    } else if (size == SIZE_WORD) {
        signed_value = SIGN_EXTEND_WORD(value);
    } else {
        signed_value = value;
    }

    /* Set flags */
    M68K_ClearFlag(as, CCR_N | CCR_Z | CCR_V | CCR_C);
    if (signed_value == 0) {
        M68K_SetFlag(as, CCR_Z);
    }
    if (signed_value < 0) {
        M68K_SetFlag(as, CCR_N);
    }
}

/*
 * EXT - Sign-extend data register
 * Encoding: 0100 1000 1oxx xrrr (o=0: byte->word, o=1: word->long)
 * Format: EXT.W Dn  (byte->word) or EXT.L Dn (word->long)
 */
void M68K_Op_EXT(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dn = opcode & 7;
    UInt8 opmode = (opcode >> 6) & 7;
    SInt32 value;

    if (opmode == 2) {
        /* EXT.W - byte to word */
        value = SIGN_EXTEND_BYTE(as->regs.d[dn] & 0xFF);
        as->regs.d[dn] = (as->regs.d[dn] & 0xFFFF0000) | (value & 0xFFFF);
    } else if (opmode == 3) {
        /* EXT.L - word to long */
        value = SIGN_EXTEND_WORD(as->regs.d[dn] & 0xFFFF);
        as->regs.d[dn] = value;
    } else {
        M68K_Fault(as, "Invalid EXT opmode");
        return;
    }

    /* Set flags */
    M68K_ClearFlag(as, CCR_N | CCR_Z | CCR_V | CCR_C);
    if (value == 0) {
        M68K_SetFlag(as, CCR_Z);
    }
    if (value < 0) {
        M68K_SetFlag(as, CCR_N);
    }
}

/*
 * SWAP - Swap register halves
 * Encoding: 0100 1000 0100 0rrr
 * Format: SWAP Dn
 */
void M68K_Op_SWAP(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dn = opcode & 7;
    UInt32 value = as->regs.d[dn];
    UInt32 swapped;

    /* Swap upper and lower 16 bits */
    swapped = ((value & 0xFFFF) << 16) | ((value >> 16) & 0xFFFF);
    as->regs.d[dn] = swapped;

    /* Set flags based on result */
    M68K_ClearFlag(as, CCR_N | CCR_Z | CCR_V | CCR_C);
    if (swapped == 0) {
        M68K_SetFlag(as, CCR_Z);
    }
    if (swapped & 0x80000000) {
        M68K_SetFlag(as, CCR_N);
    }
}

/*
 * ADDQ - Add quick (1-8 to EA)
 * Encoding: 0101 ddd0 ssxx xrrr
 * Format: ADDQ #<data>, <ea>
 */
void M68K_Op_ADDQ(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 data = (opcode >> 9) & 7;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 immediate, operand, result;
    UInt32 mask = SIZE_MASK(size);

    /* Data field: 0 means 8 */
    immediate = (data == 0) ? 8 : data;

    /* Read operand */
    operand = M68K_EA_Read(as, mode, reg, size);

    /* Perform addition */
    result = (operand + immediate) & mask;

    /* Write result */
    M68K_EA_Write(as, mode, reg, size, result);

    /* Set flags (unless An direct) */
    if (mode != MODE_An) {
        M68K_SetNZ(as, result, size);

        /* Set C and X if carry occurred */
        if ((operand + immediate) > mask) {
            M68K_SetFlag(as, CCR_C | CCR_X);
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }

        /* Set V if overflow */
        {
            UInt32 sign_bit = SIZE_SIGN_BIT(size);
            Boolean op_neg = (operand & sign_bit) != 0;
            Boolean res_neg = (result & sign_bit) != 0;
            Boolean imm_neg = false; /* Immediate is always positive */

            if (op_neg == imm_neg && res_neg != op_neg) {
                M68K_SetFlag(as, CCR_V);
            } else {
                M68K_ClearFlag(as, CCR_V);
            }
        }
    }
}

/*
 * SUBQ - Subtract quick (1-8 from EA)
 * Encoding: 0101 ddd1 ssxx xrrr
 * Format: SUBQ #<data>, <ea>
 */
void M68K_Op_SUBQ(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 data = (opcode >> 9) & 7;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 immediate, operand, result;
    UInt32 mask = SIZE_MASK(size);

    /* Data field: 0 means 8 */
    immediate = (data == 0) ? 8 : data;

    /* Read operand */
    operand = M68K_EA_Read(as, mode, reg, size);

    /* Perform subtraction */
    result = (operand - immediate) & mask;

    /* Write result */
    M68K_EA_Write(as, mode, reg, size, result);

    /* Set flags (unless An direct) */
    if (mode != MODE_An) {
        M68K_SetNZ(as, result, size);

        /* Set C and X if borrow occurred */
        if (immediate > operand) {
            M68K_SetFlag(as, CCR_C | CCR_X);
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }

        /* Set V if overflow */
        {
            UInt32 sign_bit = SIZE_SIGN_BIT(size);
            Boolean op_neg = (operand & sign_bit) != 0;
            Boolean res_neg = (result & sign_bit) != 0;
            Boolean imm_neg = false; /* Immediate is always positive */

            if (op_neg != imm_neg && res_neg != op_neg) {
                M68K_SetFlag(as, CCR_V);
            } else {
                M68K_ClearFlag(as, CCR_V);
            }
        }
    }
}

/*
 * AND - Logical AND
 * Encoding: 1100 rrrd ssxx xrrr (d=direction: 0=EA&Dn->Dn, 1=Dn&EA->EA)
 * Format: AND <ea>, Dn  or  AND Dn, <ea>
 */
void M68K_Op_AND(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src, dst, result;
    UInt32 mask = SIZE_MASK(size);

    if (dir == 0) {
        /* EA & Dn -> Dn */
        src = M68K_EA_Read(as, ea_mode, ea_reg, size);
        dst = as->regs.d[reg] & mask;
        result = (dst & src) & mask;
        as->regs.d[reg] = (as->regs.d[reg] & ~mask) | result;
    } else {
        /* Dn & EA -> EA */
        dst = M68K_EA_Read(as, ea_mode, ea_reg, size);
        src = as->regs.d[reg] & mask;
        result = (dst & src) & mask;
        M68K_EA_Write(as, ea_mode, ea_reg, size, result);
    }

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * OR - Logical OR
 * Encoding: 1000 rrrd ssxx xrrr (d=direction)
 * Format: OR <ea>, Dn  or  OR Dn, <ea>
 */
void M68K_Op_OR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src, dst, result;
    UInt32 mask = SIZE_MASK(size);

    if (dir == 0) {
        /* EA | Dn -> Dn */
        src = M68K_EA_Read(as, ea_mode, ea_reg, size);
        dst = as->regs.d[reg] & mask;
        result = (dst | src) & mask;
        as->regs.d[reg] = (as->regs.d[reg] & ~mask) | result;
    } else {
        /* Dn | EA -> EA */
        dst = M68K_EA_Read(as, ea_mode, ea_reg, size);
        src = as->regs.d[reg] & mask;
        result = (dst | src) & mask;
        M68K_EA_Write(as, ea_mode, ea_reg, size, result);
    }

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * EOR - Logical Exclusive OR
 * Encoding: 1011 rrr1 ssxx xrrr
 * Format: EOR Dn, <ea>
 */
void M68K_Op_EOR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src, dst, result;
    UInt32 mask = SIZE_MASK(size);

    /* Dn ^ EA -> EA */
    dst = M68K_EA_Read(as, ea_mode, ea_reg, size);
    src = as->regs.d[reg] & mask;
    result = (dst ^ src) & mask;
    M68K_EA_Write(as, ea_mode, ea_reg, size, result);

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * NOP - No operation
 * Encoding: 0100 1110 0111 0001 (0x4E71)
 */
void M68K_Op_NOP(M68KAddressSpace* as, UInt16 opcode)
{
    (void)as;
    (void)opcode;
    /* Do nothing */
}

/*
 * ADDA - Add to address register
 * Encoding: 1101 rrrs 11xx xrrr (s=0: word, s=1: long)
 * Format: ADDA <ea>, An
 */
void M68K_Op_ADDA(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 size_bit = (opcode >> 8) & 1;
    M68KSize size = size_bit ? SIZE_LONG : SIZE_WORD;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src;

    /* Read source */
    src = M68K_EA_Read(as, ea_mode, ea_reg, size);

    /* Sign-extend if word */
    if (size == SIZE_WORD) {
        src = SIGN_EXTEND_WORD(src);
    }

    /* Add to address register */
    as->regs.a[reg] += src;

    /* ADDA does not affect flags */
}

/*
 * SUBA - Subtract from address register
 * Encoding: 1001 rrrs 11xx xrrr (s=0: word, s=1: long)
 * Format: SUBA <ea>, An
 */
void M68K_Op_SUBA(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 size_bit = (opcode >> 8) & 1;
    M68KSize size = size_bit ? SIZE_LONG : SIZE_WORD;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src;

    /* Read source */
    src = M68K_EA_Read(as, ea_mode, ea_reg, size);

    /* Sign-extend if word */
    if (size == SIZE_WORD) {
        src = SIGN_EXTEND_WORD(src);
    }

    /* Subtract from address register */
    as->regs.a[reg] -= src;

    /* SUBA does not affect flags */
}

/*
 * CMPA - Compare address register
 * Encoding: 1011 rrrs 11xx xrrr (s=0: word, s=1: long)
 * Format: CMPA <ea>, An
 */
void M68K_Op_CMPA(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 size_bit = (opcode >> 8) & 1;
    M68KSize size = size_bit ? SIZE_LONG : SIZE_WORD;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src, dst, result;

    /* Read source */
    src = M68K_EA_Read(as, ea_mode, ea_reg, size);

    /* Sign-extend if word */
    if (size == SIZE_WORD) {
        src = SIGN_EXTEND_WORD(src);
    }

    /* Compare An - src */
    dst = as->regs.a[reg];
    result = dst - src;

    /* Set flags */
    M68K_SetNZ(as, result, SIZE_LONG);

    /* Set C if borrow occurred */
    if (src > dst) {
        M68K_SetFlag(as, CCR_C);
    } else {
        M68K_ClearFlag(as, CCR_C);
    }

    /* Set V if overflow */
    {
        Boolean src_neg = (src & 0x80000000) != 0;
        Boolean dst_neg = (dst & 0x80000000) != 0;
        Boolean res_neg = (result & 0x80000000) != 0;

        if (src_neg != dst_neg && res_neg != dst_neg) {
            M68K_SetFlag(as, CCR_V);
        } else {
            M68K_ClearFlag(as, CCR_V);
        }
    }
}

/*
 * MOVEM - Move multiple registers
 * Encoding: 0100 1d00 1sxx xrrr (d=direction: 0=regs->mem, 1=mem->regs)
 * Format: MOVEM regs, <ea>  or  MOVEM <ea>, regs
 */
void M68K_Op_MOVEM(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dir = (opcode >> 10) & 1;
    UInt8 size_bit = (opcode >> 6) & 1;
    M68KSize size = size_bit ? SIZE_LONG : SIZE_WORD;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt16 reglist;
    UInt32 addr;
    int i;

    /* Fetch register list mask */
    reglist = M68K_Fetch16(as);

    /* Compute base address */
    addr = M68K_EA_ComputeAddress(as, mode, reg, size);

    if (dir == 0) {
        /* Registers to memory */
        if (mode == MODE_An_PRE) {
            /* Predecrement: process in reverse order (A7-A0, D7-D0) */
            for (i = 15; i >= 0; i--) {
                if (reglist & (1 << i)) {
                    if (size == SIZE_LONG) {
                        addr -= 4;
                        if (i >= 8) {
                            M68K_Write32(as, addr, as->regs.a[i - 8]);
                        } else {
                            M68K_Write32(as, addr, as->regs.d[i]);
                        }
                    } else {
                        addr -= 2;
                        if (i >= 8) {
                            M68K_Write16(as, addr, as->regs.a[i - 8] & 0xFFFF);
                        } else {
                            M68K_Write16(as, addr, as->regs.d[i] & 0xFFFF);
                        }
                    }
                }
            }
            /* Update address register */
            as->regs.a[reg] = addr;
        } else {
            /* Normal order (D0-D7, A0-A7) */
            for (i = 0; i < 16; i++) {
                if (reglist & (1 << i)) {
                    if (i < 8) {
                        /* D0-D7 */
                        if (size == SIZE_LONG) {
                            M68K_Write32(as, addr, as->regs.d[i]);
                            addr += 4;
                        } else {
                            M68K_Write16(as, addr, as->regs.d[i] & 0xFFFF);
                            addr += 2;
                        }
                    } else {
                        /* A0-A7 */
                        if (size == SIZE_LONG) {
                            M68K_Write32(as, addr, as->regs.a[i - 8]);
                            addr += 4;
                        } else {
                            M68K_Write16(as, addr, as->regs.a[i - 8] & 0xFFFF);
                            addr += 2;
                        }
                    }
                }
            }
        }
    } else {
        /* Memory to registers */
        for (i = 0; i < 16; i++) {
            if (reglist & (1 << i)) {
                if (i < 8) {
                    /* D0-D7 */
                    if (size == SIZE_LONG) {
                        as->regs.d[i] = M68K_Read32(as, addr);
                        addr += 4;
                    } else {
                        as->regs.d[i] = SIGN_EXTEND_WORD(M68K_Read16(as, addr));
                        addr += 2;
                    }
                } else {
                    /* A0-A7 */
                    if (size == SIZE_LONG) {
                        as->regs.a[i - 8] = M68K_Read32(as, addr);
                        addr += 4;
                    } else {
                        as->regs.a[i - 8] = SIGN_EXTEND_WORD(M68K_Read16(as, addr));
                        addr += 2;
                    }
                }
            }
        }
        /* Update address register if postincrement */
        if (mode == MODE_An_POST) {
            as->regs.a[reg] = addr;
        }
    }

    /* MOVEM does not affect flags */
}

/*
 * LSL - Logical Shift Left
 * Encoding: 1110 cccD ss00 1rrr (D=direction, c=count/reg, s=size)
 * Format: LSL Dx, Dy  or  LSL #<data>, Dy
 */
void M68K_Op_LSL(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 count_reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;  /* 0=immediate, 1=register */
    UInt8 size = (opcode >> 6) & 3;
    UInt8 data_reg = opcode & 7;
    UInt32 value, result, count;
    UInt32 mask = SIZE_MASK(size);

    /* Get shift count */
    if (dir) {
        count = as->regs.d[count_reg] & 0x3F;  /* Modulo 64 */
    } else {
        count = count_reg ? count_reg : 8;  /* 0 means 8 */
    }

    /* Read operand */
    value = as->regs.d[data_reg] & mask;

    /* Perform shift */
    if (count == 0) {
        result = value;
        M68K_ClearFlag(as, CCR_C);
    } else if (count < SIZE_BYTES(size) * 8) {
        result = (value << count) & mask;
        /* Last bit shifted out goes to C and X */
        if (value & (1 << (SIZE_BYTES(size) * 8 - count))) {
            M68K_SetFlag(as, CCR_C | CCR_X);
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }
    } else {
        result = 0;
        if (count == SIZE_BYTES(size) * 8) {
            /* Shifted by exactly bit width - last bit goes to C */
            if (value & 1) {
                M68K_SetFlag(as, CCR_C | CCR_X);
            } else {
                M68K_ClearFlag(as, CCR_C | CCR_X);
            }
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }
    }

    /* Write result */
    as->regs.d[data_reg] = (as->regs.d[data_reg] & ~mask) | result;

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V);
}

/*
 * LSR - Logical Shift Right
 * Encoding: 1110 cccD ss00 1rrr (D=direction, c=count/reg, s=size)
 * Format: LSR Dx, Dy  or  LSR #<data>, Dy
 */
void M68K_Op_LSR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 count_reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 data_reg = opcode & 7;
    UInt32 value, result, count;
    UInt32 mask = SIZE_MASK(size);

    /* Get shift count */
    if (dir) {
        count = as->regs.d[count_reg] & 0x3F;
    } else {
        count = count_reg ? count_reg : 8;
    }

    /* Read operand */
    value = as->regs.d[data_reg] & mask;

    /* Perform shift */
    if (count == 0) {
        result = value;
        M68K_ClearFlag(as, CCR_C);
    } else if (count < SIZE_BYTES(size) * 8) {
        result = (value >> count) & mask;
        /* Last bit shifted out goes to C and X */
        if (value & (1 << (count - 1))) {
            M68K_SetFlag(as, CCR_C | CCR_X);
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }
    } else {
        result = 0;
        if (count == SIZE_BYTES(size) * 8) {
            /* Shifted by exactly bit width */
            if (value & (1 << (SIZE_BYTES(size) * 8 - 1))) {
                M68K_SetFlag(as, CCR_C | CCR_X);
            } else {
                M68K_ClearFlag(as, CCR_C | CCR_X);
            }
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }
    }

    /* Write result */
    as->regs.d[data_reg] = (as->regs.d[data_reg] & ~mask) | result;

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V);
}

/*
 * ASL - Arithmetic Shift Left
 * Encoding: 1110 cccD ss10 0rrr
 * Format: ASL Dx, Dy  or  ASL #<data>, Dy
 */
void M68K_Op_ASL(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 count_reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 data_reg = opcode & 7;
    UInt32 value, result, count;
    UInt32 mask = SIZE_MASK(size);
    UInt32 sign_bit = SIZE_SIGN_BIT(size);

    /* Get shift count */
    if (dir) {
        count = as->regs.d[count_reg] & 0x3F;
    } else {
        count = count_reg ? count_reg : 8;
    }

    /* Read operand */
    value = as->regs.d[data_reg] & mask;

    /* Perform shift */
    if (count == 0) {
        result = value;
        M68K_ClearFlag(as, CCR_C | CCR_V);
    } else if (count < SIZE_BYTES(size) * 8) {
        result = (value << count) & mask;

        /* Last bit shifted out goes to C and X */
        if (value & (1 << (SIZE_BYTES(size) * 8 - count))) {
            M68K_SetFlag(as, CCR_C | CCR_X);
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }

        /* Check for overflow: sign bit changed during shift */
        UInt32 original_sign = value & sign_bit;
        UInt32 result_sign = result & sign_bit;
        if (original_sign != result_sign) {
            M68K_SetFlag(as, CCR_V);
        } else {
            M68K_ClearFlag(as, CCR_V);
        }
    } else {
        result = 0;
        M68K_ClearFlag(as, CCR_C | CCR_X);
        if (value != 0) {
            M68K_SetFlag(as, CCR_V);
        } else {
            M68K_ClearFlag(as, CCR_V);
        }
    }

    /* Write result */
    as->regs.d[data_reg] = (as->regs.d[data_reg] & ~mask) | result;

    /* Set flags */
    M68K_SetNZ(as, result, size);
}

/*
 * ASR - Arithmetic Shift Right
 * Encoding: 1110 cccD ss00 0rrr
 * Format: ASR Dx, Dy  or  ASR #<data>, Dy
 */
void M68K_Op_ASR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 count_reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 data_reg = opcode & 7;
    UInt32 value, result, count;
    UInt32 mask = SIZE_MASK(size);
    UInt32 sign_bit = SIZE_SIGN_BIT(size);
    SInt32 signed_value;

    /* Get shift count */
    if (dir) {
        count = as->regs.d[count_reg] & 0x3F;
    } else {
        count = count_reg ? count_reg : 8;
    }

    /* Read operand and sign-extend */
    value = as->regs.d[data_reg] & mask;
    if (size == SIZE_BYTE) {
        signed_value = SIGN_EXTEND_BYTE(value);
    } else if (size == SIZE_WORD) {
        signed_value = SIGN_EXTEND_WORD(value);
    } else {
        signed_value = value;
    }

    /* Perform arithmetic shift */
    if (count == 0) {
        result = value;
        M68K_ClearFlag(as, CCR_C);
    } else if (count < SIZE_BYTES(size) * 8) {
        result = (signed_value >> count) & mask;
        /* Last bit shifted out goes to C and X */
        if (value & (1 << (count - 1))) {
            M68K_SetFlag(as, CCR_C | CCR_X);
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }
    } else {
        /* Shifted all bits - fill with sign bit */
        if (value & sign_bit) {
            result = mask;  /* All 1s */
            M68K_SetFlag(as, CCR_C | CCR_X);
        } else {
            result = 0;
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }
    }

    /* Write result */
    as->regs.d[data_reg] = (as->regs.d[data_reg] & ~mask) | result;

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V);  /* ASR never sets V */
}

/*
 * MULU - Unsigned multiply
 * Encoding: 1100 rrr0 11xx xrrr
 * Format: MULU <ea>, Dn  (16x16 -> 32)
 */
void M68K_Op_MULU(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 src, dst, result;

    /* Read source (word) */
    src = M68K_EA_Read(as, ea_mode, ea_reg, SIZE_WORD) & 0xFFFF;

    /* Read destination (low word) */
    dst = as->regs.d[reg] & 0xFFFF;

    /* Unsigned multiply */
    result = src * dst;

    /* Write result (full 32 bits) */
    as->regs.d[reg] = result;

    /* Set flags */
    M68K_SetNZ(as, result, SIZE_LONG);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * MULS - Signed multiply
 * Encoding: 1100 rrr1 11xx xrrr
 * Format: MULS <ea>, Dn  (16x16 -> 32)
 */
void M68K_Op_MULS(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    SInt32 src, dst, result;

    /* Read source (word, sign-extended) */
    src = SIGN_EXTEND_WORD(M68K_EA_Read(as, ea_mode, ea_reg, SIZE_WORD));

    /* Read destination (low word, sign-extended) */
    dst = SIGN_EXTEND_WORD(as->regs.d[reg] & 0xFFFF);

    /* Signed multiply */
    result = src * dst;

    /* Write result (full 32 bits) */
    as->regs.d[reg] = result;

    /* Set flags */
    M68K_SetNZ(as, result, SIZE_LONG);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * BTST - Bit test
 * Encoding: 0000 rrr1 00xx xrrr (register form) or 0000 1000 00xx xrrr (immediate form)
 * Format: BTST Dn, <ea>  or  BTST #<data>, <ea>
 */
void M68K_Op_BTST(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 bit_num_reg = (opcode >> 9) & 7;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 bit_num, value;
    M68KSize size;

    /* Determine bit number */
    if ((opcode & 0x0100) == 0x0100) {
        /* Register form */
        bit_num = as->regs.d[bit_num_reg];
    } else {
        /* Immediate form */
        bit_num = M68K_Fetch16(as) & 0xFF;
    }

    /* Determine size based on addressing mode */
    if (mode == MODE_Dn) {
        size = SIZE_LONG;
        bit_num &= 31;  /* Modulo 32 for registers */
    } else {
        size = SIZE_BYTE;
        bit_num &= 7;   /* Modulo 8 for memory */
    }

    /* Read operand */
    value = M68K_EA_Read(as, mode, reg, size);

    /* Test bit and set Z flag */
    if (value & (1 << bit_num)) {
        M68K_ClearFlag(as, CCR_Z);  /* Bit is set, Z=0 */
    } else {
        M68K_SetFlag(as, CCR_Z);    /* Bit is clear, Z=1 */
    }

    /* BTST does not affect other flags */
}

/*
 * BSET - Bit set
 * Encoding: 0000 rrr1 11xx xrrr (register form) or 0000 1000 11xx xrrr (immediate form)
 * Format: BSET Dn, <ea>  or  BSET #<data>, <ea>
 */
void M68K_Op_BSET(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 bit_num_reg = (opcode >> 9) & 7;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 bit_num, value;
    M68KSize size;

    /* Determine bit number */
    if ((opcode & 0x0100) == 0x0100) {
        bit_num = as->regs.d[bit_num_reg];
    } else {
        bit_num = M68K_Fetch16(as) & 0xFF;
    }

    /* Determine size */
    if (mode == MODE_Dn) {
        size = SIZE_LONG;
        bit_num &= 31;
    } else {
        size = SIZE_BYTE;
        bit_num &= 7;
    }

    /* Read operand */
    value = M68K_EA_Read(as, mode, reg, size);

    /* Test old bit value (Z flag) */
    if (value & (1 << bit_num)) {
        M68K_ClearFlag(as, CCR_Z);
    } else {
        M68K_SetFlag(as, CCR_Z);
    }

    /* Set bit */
    value |= (1 << bit_num);

    /* Write back */
    M68K_EA_Write(as, mode, reg, size, value);
}

/*
 * BCLR - Bit clear
 * Encoding: 0000 rrr1 10xx xrrr (register form) or 0000 1000 10xx xrrr (immediate form)
 * Format: BCLR Dn, <ea>  or  BCLR #<data>, <ea>
 */
void M68K_Op_BCLR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 bit_num_reg = (opcode >> 9) & 7;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 bit_num, value;
    M68KSize size;

    /* Determine bit number */
    if ((opcode & 0x0100) == 0x0100) {
        bit_num = as->regs.d[bit_num_reg];
    } else {
        bit_num = M68K_Fetch16(as) & 0xFF;
    }

    /* Determine size */
    if (mode == MODE_Dn) {
        size = SIZE_LONG;
        bit_num &= 31;
    } else {
        size = SIZE_BYTE;
        bit_num &= 7;
    }

    /* Read operand */
    value = M68K_EA_Read(as, mode, reg, size);

    /* Test old bit value (Z flag) */
    if (value & (1 << bit_num)) {
        M68K_ClearFlag(as, CCR_Z);
    } else {
        M68K_SetFlag(as, CCR_Z);
    }

    /* Clear bit */
    value &= ~(1 << bit_num);

    /* Write back */
    M68K_EA_Write(as, mode, reg, size, value);
}

/*
 * BCHG - Bit change (toggle)
 * Encoding: 0000 rrr1 01xx xrrr (register form) or 0000 1000 01xx xrrr (immediate form)
 * Format: BCHG Dn, <ea>  or  BCHG #<data>, <ea>
 */
void M68K_Op_BCHG(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 bit_num_reg = (opcode >> 9) & 7;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 bit_num, value;
    M68KSize size;

    /* Determine bit number */
    if ((opcode & 0x0100) == 0x0100) {
        bit_num = as->regs.d[bit_num_reg];
    } else {
        bit_num = M68K_Fetch16(as) & 0xFF;
    }

    /* Determine size */
    if (mode == MODE_Dn) {
        size = SIZE_LONG;
        bit_num &= 31;
    } else {
        size = SIZE_BYTE;
        bit_num &= 7;
    }

    /* Read operand */
    value = M68K_EA_Read(as, mode, reg, size);

    /* Test old bit value (Z flag) */
    if (value & (1 << bit_num)) {
        M68K_ClearFlag(as, CCR_Z);
    } else {
        M68K_SetFlag(as, CCR_Z);
    }

    /* Toggle bit */
    value ^= (1 << bit_num);

    /* Write back */
    M68K_EA_Write(as, mode, reg, size, value);
}

/*
 * DIVU - Unsigned divide
 * Encoding: 1000 rrr0 11xx xrrr
 * Format: DIVU <ea>, Dn  (32÷16 -> 16r:16q)
 */
void M68K_Op_DIVU(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    UInt32 dividend, divisor, quotient, remainder;

    /* Read divisor (word) */
    divisor = M68K_EA_Read(as, ea_mode, ea_reg, SIZE_WORD) & 0xFFFF;

    /* Check for divide by zero */
    if (divisor == 0) {
        M68K_Fault(as, "Division by zero");
        return;
    }

    /* Read dividend (long) */
    dividend = as->regs.d[reg];

    /* Unsigned divide */
    quotient = dividend / divisor;
    remainder = dividend % divisor;

    /* Check for overflow (quotient doesn't fit in 16 bits) */
    if (quotient > 0xFFFF) {
        M68K_SetFlag(as, CCR_V);
        /* Result undefined on overflow, don't update Dn */
        return;
    }

    /* Pack result: high word = remainder, low word = quotient */
    as->regs.d[reg] = ((remainder & 0xFFFF) << 16) | (quotient & 0xFFFF);

    /* Set flags */
    M68K_SetNZ(as, quotient, SIZE_WORD);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * DIVS - Signed divide
 * Encoding: 1000 rrr1 11xx xrrr
 * Format: DIVS <ea>, Dn  (32÷16 -> 16r:16q)
 */
void M68K_Op_DIVS(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    SInt32 dividend, divisor, quotient, remainder;

    /* Read divisor (word, sign-extended) */
    divisor = SIGN_EXTEND_WORD(M68K_EA_Read(as, ea_mode, ea_reg, SIZE_WORD));

    /* Check for divide by zero */
    if (divisor == 0) {
        M68K_Fault(as, "Division by zero");
        return;
    }

    /* Read dividend (long) */
    dividend = (SInt32)as->regs.d[reg];

    /* Signed divide */
    quotient = dividend / divisor;
    remainder = dividend % divisor;

    /* Check for overflow (quotient doesn't fit in 16 bits) */
    if (quotient < -32768 || quotient > 32767) {
        M68K_SetFlag(as, CCR_V);
        /* Result undefined on overflow */
        return;
    }

    /* Pack result: high word = remainder, low word = quotient */
    as->regs.d[reg] = ((remainder & 0xFFFF) << 16) | (quotient & 0xFFFF);

    /* Set flags */
    M68K_SetNZ(as, quotient & 0xFFFF, SIZE_WORD);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * ROL - Rotate left
 * Encoding: 1110 cccD ss01 1rrr
 * Format: ROL Dx, Dy  or  ROL #<data>, Dy
 */
void M68K_Op_ROL(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 count_reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 data_reg = opcode & 7;
    UInt32 value, result, count;
    UInt32 mask = SIZE_MASK(size);
    UInt8 bit_width = SIZE_BYTES(size) * 8;

    /* Get rotate count */
    if (dir) {
        count = as->regs.d[count_reg] & 0x3F;
    } else {
        count = count_reg ? count_reg : 8;
    }

    /* Read operand */
    value = as->regs.d[data_reg] & mask;

    /* Perform rotation */
    if (count == 0) {
        result = value;
        M68K_ClearFlag(as, CCR_C);
    } else {
        count = count % bit_width;  /* Normalize count */
        if (count == 0) {
            result = value;
            /* C flag gets MSB */
            if (value & (1 << (bit_width - 1))) {
                M68K_SetFlag(as, CCR_C);
            } else {
                M68K_ClearFlag(as, CCR_C);
            }
        } else {
            result = ((value << count) | (value >> (bit_width - count))) & mask;
            /* C flag gets last bit rotated out (which is now LSB) */
            if (result & 1) {
                M68K_SetFlag(as, CCR_C);
            } else {
                M68K_ClearFlag(as, CCR_C);
            }
        }
    }

    /* Write result */
    as->regs.d[data_reg] = (as->regs.d[data_reg] & ~mask) | result;

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V);
}

/*
 * ROR - Rotate right
 * Encoding: 1110 cccD ss01 0rrr
 * Format: ROR Dx, Dy  or  ROR #<data>, Dy
 */
void M68K_Op_ROR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 count_reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 data_reg = opcode & 7;
    UInt32 value, result, count;
    UInt32 mask = SIZE_MASK(size);
    UInt8 bit_width = SIZE_BYTES(size) * 8;

    /* Get rotate count */
    if (dir) {
        count = as->regs.d[count_reg] & 0x3F;
    } else {
        count = count_reg ? count_reg : 8;
    }

    /* Read operand */
    value = as->regs.d[data_reg] & mask;

    /* Perform rotation */
    if (count == 0) {
        result = value;
        M68K_ClearFlag(as, CCR_C);
    } else {
        count = count % bit_width;  /* Normalize count */
        if (count == 0) {
            result = value;
            /* C flag gets LSB */
            if (value & 1) {
                M68K_SetFlag(as, CCR_C);
            } else {
                M68K_ClearFlag(as, CCR_C);
            }
        } else {
            result = ((value >> count) | (value << (bit_width - count))) & mask;
            /* C flag gets last bit rotated out (which is now MSB) */
            if (result & (1 << (bit_width - 1))) {
                M68K_SetFlag(as, CCR_C);
            } else {
                M68K_ClearFlag(as, CCR_C);
            }
        }
    }

    /* Write result */
    as->regs.d[data_reg] = (as->regs.d[data_reg] & ~mask) | result;

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V);
}

/*
 * NEG - Negate
 * Encoding: 0100 0100 ssxx xrrr
 * Format: NEG <ea>
 */
void M68K_Op_NEG(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 value, result;
    UInt32 mask = SIZE_MASK(size);

    /* Read operand */
    value = M68K_EA_Read(as, mode, reg, size);

    /* Negate (0 - value) */
    result = (0 - value) & mask;

    /* Write result */
    M68K_EA_Write(as, mode, reg, size, result);

    /* Set flags */
    M68K_SetNZ(as, result, size);

    /* C and X set if result is non-zero */
    if (result != 0) {
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    /* V set if overflow occurred (value was 0x80... for the size) */
    if (value == SIZE_SIGN_BIT(size)) {
        M68K_SetFlag(as, CCR_V);
    } else {
        M68K_ClearFlag(as, CCR_V);
    }
}

/*
 * ROXL - Rotate left through extend
 * Encoding: 1110 cccD ss01 0rrr
 * Format: ROXL Dx, Dy  or  ROXL #<data>, Dy
 */
void M68K_Op_ROXL(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 count_reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 data_reg = opcode & 7;
    UInt32 value, result, count;
    UInt32 mask = SIZE_MASK(size);
    UInt8 bit_width = SIZE_BYTES(size) * 8;
    Boolean x_flag;

    /* Get rotate count */
    if (dir) {
        count = as->regs.d[count_reg] & 0x3F;
    } else {
        count = count_reg ? count_reg : 8;
    }

    /* Read operand */
    value = as->regs.d[data_reg] & mask;

    /* Get X flag state */
    x_flag = M68K_TestFlag(as, CCR_X);

    /* Perform rotation through extend */
    if (count == 0) {
        result = value;
        /* C gets X */
        if (x_flag) {
            M68K_SetFlag(as, CCR_C);
        } else {
            M68K_ClearFlag(as, CCR_C);
        }
    } else {
        count = count % (bit_width + 1);  /* Modulo (bit_width + 1) because X is included */
        result = value;

        for (UInt32 i = 0; i < count; i++) {
            Boolean msb = (result & (1 << (bit_width - 1))) != 0;
            result = ((result << 1) | (x_flag ? 1 : 0)) & mask;
            x_flag = msb;
        }

        /* C and X get last bit rotated out */
        if (x_flag) {
            M68K_SetFlag(as, CCR_C | CCR_X);
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }
    }

    /* Write result */
    as->regs.d[data_reg] = (as->regs.d[data_reg] & ~mask) | result;

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V);
}

/*
 * ROXR - Rotate right through extend
 * Encoding: 1110 cccD ss01 0rrr
 * Format: ROXR Dx, Dy  or  ROXR #<data>, Dy
 */
void M68K_Op_ROXR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 count_reg = (opcode >> 9) & 7;
    UInt8 dir = (opcode >> 8) & 1;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 data_reg = opcode & 7;
    UInt32 value, result, count;
    UInt32 mask = SIZE_MASK(size);
    UInt8 bit_width = SIZE_BYTES(size) * 8;
    Boolean x_flag;

    /* Get rotate count */
    if (dir) {
        count = as->regs.d[count_reg] & 0x3F;
    } else {
        count = count_reg ? count_reg : 8;
    }

    /* Read operand */
    value = as->regs.d[data_reg] & mask;

    /* Get X flag state */
    x_flag = M68K_TestFlag(as, CCR_X);

    /* Perform rotation through extend */
    if (count == 0) {
        result = value;
        /* C gets X */
        if (x_flag) {
            M68K_SetFlag(as, CCR_C);
        } else {
            M68K_ClearFlag(as, CCR_C);
        }
    } else {
        count = count % (bit_width + 1);
        result = value;

        for (UInt32 i = 0; i < count; i++) {
            Boolean lsb = (result & 1) != 0;
            result = ((result >> 1) | ((x_flag ? 1 : 0) << (bit_width - 1))) & mask;
            x_flag = lsb;
        }

        /* C and X get last bit rotated out */
        if (x_flag) {
            M68K_SetFlag(as, CCR_C | CCR_X);
        } else {
            M68K_ClearFlag(as, CCR_C | CCR_X);
        }
    }

    /* Write result */
    as->regs.d[data_reg] = (as->regs.d[data_reg] & ~mask) | result;

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V);
}

/*
 * ADDX - Add with extend
 * Encoding: 1101 rrr1 ss00 mrrr (m=0: data regs, m=1: address regs with predecrement)
 * Format: ADDX Dx, Dy  or  ADDX -(Ax), -(Ay)
 */
void M68K_Op_ADDX(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dst_reg = (opcode >> 9) & 7;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 1;
    UInt8 src_reg = opcode & 7;
    UInt32 src, dst, result;
    UInt32 mask = SIZE_MASK(size);
    Boolean x = M68K_TestFlag(as, CCR_X);

    if (mode == 0) {
        /* Data register mode */
        src = as->regs.d[src_reg] & mask;
        dst = as->regs.d[dst_reg] & mask;
        result = (dst + src + (x ? 1 : 0)) & mask;
        as->regs.d[dst_reg] = (as->regs.d[dst_reg] & ~mask) | result;
    } else {
        /* Predecrement mode */
        UInt8 byte_count = SIZE_BYTES(size);
        as->regs.a[src_reg] -= byte_count;
        as->regs.a[dst_reg] -= byte_count;

        src = M68K_EA_Read(as, MODE_An_IND, src_reg, size);
        dst = M68K_EA_Read(as, MODE_An_IND, dst_reg, size);
        result = (dst + src + (x ? 1 : 0)) & mask;

        M68K_EA_Write(as, MODE_An_IND, dst_reg, size, result);
    }

    /* Set flags (Z cleared only if result is non-zero) */
    if (result != 0) {
        M68K_ClearFlag(as, CCR_Z);
    }

    if (result & SIZE_SIGN_BIT(size)) {
        M68K_SetFlag(as, CCR_N);
    } else {
        M68K_ClearFlag(as, CCR_N);
    }

    /* Carry */
    if ((dst + src + (x ? 1 : 0)) > mask) {
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    /* Overflow */
    {
        UInt32 sign_bit = SIZE_SIGN_BIT(size);
        Boolean src_neg = (src & sign_bit) != 0;
        Boolean dst_neg = (dst & sign_bit) != 0;
        Boolean res_neg = (result & sign_bit) != 0;

        if (src_neg == dst_neg && res_neg != dst_neg) {
            M68K_SetFlag(as, CCR_V);
        } else {
            M68K_ClearFlag(as, CCR_V);
        }
    }
}

/*
 * SUBX - Subtract with extend
 * Encoding: 1001 rrr1 ss00 mrrr
 * Format: SUBX Dx, Dy  or  SUBX -(Ax), -(Ay)
 */
void M68K_Op_SUBX(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dst_reg = (opcode >> 9) & 7;
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 1;
    UInt8 src_reg = opcode & 7;
    UInt32 src, dst, result;
    UInt32 mask = SIZE_MASK(size);
    Boolean x = M68K_TestFlag(as, CCR_X);

    if (mode == 0) {
        /* Data register mode */
        src = as->regs.d[src_reg] & mask;
        dst = as->regs.d[dst_reg] & mask;
        result = (dst - src - (x ? 1 : 0)) & mask;
        as->regs.d[dst_reg] = (as->regs.d[dst_reg] & ~mask) | result;
    } else {
        /* Predecrement mode */
        UInt8 byte_count = SIZE_BYTES(size);
        as->regs.a[src_reg] -= byte_count;
        as->regs.a[dst_reg] -= byte_count;

        src = M68K_EA_Read(as, MODE_An_IND, src_reg, size);
        dst = M68K_EA_Read(as, MODE_An_IND, dst_reg, size);
        result = (dst - src - (x ? 1 : 0)) & mask;

        M68K_EA_Write(as, MODE_An_IND, dst_reg, size, result);
    }

    /* Set flags (Z cleared only if result is non-zero) */
    if (result != 0) {
        M68K_ClearFlag(as, CCR_Z);
    }

    if (result & SIZE_SIGN_BIT(size)) {
        M68K_SetFlag(as, CCR_N);
    } else {
        M68K_ClearFlag(as, CCR_N);
    }

    /* Borrow */
    if ((src + (x ? 1 : 0)) > dst) {
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    /* Overflow */
    {
        UInt32 sign_bit = SIZE_SIGN_BIT(size);
        Boolean src_neg = (src & sign_bit) != 0;
        Boolean dst_neg = (dst & sign_bit) != 0;
        Boolean res_neg = (result & sign_bit) != 0;

        if (src_neg != dst_neg && res_neg != dst_neg) {
            M68K_SetFlag(as, CCR_V);
        } else {
            M68K_ClearFlag(as, CCR_V);
        }
    }
}

/*
 * NEGX - Negate with extend
 * Encoding: 0100 0000 ssxx xrrr
 * Format: NEGX <ea>
 */
void M68K_Op_NEGX(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 value, result;
    UInt32 mask = SIZE_MASK(size);
    Boolean x = M68K_TestFlag(as, CCR_X);

    /* Read operand */
    value = M68K_EA_Read(as, mode, reg, size);

    /* Negate with extend (0 - value - X) */
    result = (0 - value - (x ? 1 : 0)) & mask;

    /* Write result */
    M68K_EA_Write(as, mode, reg, size, result);

    /* Set flags (Z cleared only if result is non-zero) */
    if (result != 0) {
        M68K_ClearFlag(as, CCR_Z);
    }

    if (result & SIZE_SIGN_BIT(size)) {
        M68K_SetFlag(as, CCR_N);
    } else {
        M68K_ClearFlag(as, CCR_N);
    }

    /* C and X */
    if (value != 0 || x) {
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    /* V */
    if (value == SIZE_SIGN_BIT(size) && x) {
        M68K_SetFlag(as, CCR_V);
    } else {
        M68K_ClearFlag(as, CCR_V);
    }
}

/*
 * CHK - Check register against bounds
 * Encoding: 0100 rrr1 10xx xrrr
 * Format: CHK <ea>, Dn
 */
void M68K_Op_CHK(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 reg = (opcode >> 9) & 7;
    UInt8 ea_mode = (opcode >> 3) & 7;
    UInt8 ea_reg = opcode & 7;
    SInt32 value, upper_bound;

    /* Read upper bound (word, sign-extended) */
    upper_bound = SIGN_EXTEND_WORD(M68K_EA_Read(as, ea_mode, ea_reg, SIZE_WORD));

    /* Read value to check (low word of Dn, sign-extended) */
    value = SIGN_EXTEND_WORD(as->regs.d[reg] & 0xFFFF);

    /* Check if value < 0 or value > upper_bound */
    if (value < 0) {
        M68K_SetFlag(as, CCR_N);
        M68K_Fault(as, "CHK failed: value < 0");
    } else if (value > upper_bound) {
        M68K_ClearFlag(as, CCR_N);
        M68K_Fault(as, "CHK failed: value > bound");
    } else {
        /* Value is within bounds, continue */
        M68K_ClearFlag(as, CCR_N);
    }
}

/*
 * TAS - Test and set
 * Encoding: 0100 1010 11xx xrrr
 * Format: TAS <ea>
 */
void M68K_Op_TAS(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt8 value;

    /* Read byte */
    value = M68K_EA_Read(as, mode, reg, SIZE_BYTE) & 0xFF;

    /* Test and set flags */
    M68K_SetNZ(as, value, SIZE_BYTE);
    M68K_ClearFlag(as, CCR_V | CCR_C);

    /* Set bit 7 */
    value |= 0x80;

    /* Write back */
    M68K_EA_Write(as, mode, reg, SIZE_BYTE, value);
}

/*
 * CMPI - Compare immediate
 * Encoding: 0000 1100 ssxx xrrr
 * Format: CMPI #<data>, <ea>
 */
void M68K_Op_CMPI(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 immediate, operand, result;
    UInt32 mask = SIZE_MASK(size);

    /* Fetch immediate value */
    if (size == SIZE_BYTE || size == SIZE_WORD) {
        immediate = M68K_Fetch16(as) & mask;
    } else {
        immediate = M68K_Fetch32(as);
    }

    /* Read operand */
    operand = M68K_EA_Read(as, mode, reg, size);

    /* Compare (operand - immediate) */
    result = (operand - immediate) & mask;

    /* Set flags */
    M68K_SetNZ(as, result, size);

    if (immediate > operand) {
        M68K_SetFlag(as, CCR_C);
    } else {
        M68K_ClearFlag(as, CCR_C);
    }

    /* V flag */
    {
        UInt32 sign_bit = SIZE_SIGN_BIT(size);
        Boolean src_neg = (immediate & sign_bit) != 0;
        Boolean dst_neg = (operand & sign_bit) != 0;
        Boolean res_neg = (result & sign_bit) != 0;

        if (src_neg != dst_neg && res_neg != dst_neg) {
            M68K_SetFlag(as, CCR_V);
        } else {
            M68K_ClearFlag(as, CCR_V);
        }
    }
}

/*
 * ADDI - Add Immediate
 * Encoding: 0000 0110 ssxx xrrr + immediate data
 */
void M68K_Op_ADDI(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 immediate, dest, result;
    UInt32 mask = SIZE_MASK(size);
    UInt32 sign_bit = SIZE_SIGN_BIT(size);

    /* Fetch immediate value */
    if (size == SIZE_BYTE) {
        immediate = M68K_Fetch16(as) & 0xFF;
    } else if (size == SIZE_WORD) {
        immediate = M68K_Fetch16(as);
    } else {
        immediate = M68K_Fetch32(as);
    }

    /* Read destination */
    dest = M68K_EA_Read(as, mode, reg, size);

    /* Perform addition */
    result = (dest + immediate) & mask;

    /* Write result */
    M68K_EA_Write(as, mode, reg, size, result);

    /* Set flags */
    M68K_SetNZ(as, result, size);

    /* Carry flag */
    if (result < dest || result < immediate) {
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    /* Overflow flag */
    Boolean dest_neg = (dest & sign_bit) != 0;
    Boolean imm_neg = (immediate & sign_bit) != 0;
    Boolean res_neg = (result & sign_bit) != 0;
    if ((dest_neg == imm_neg) && (dest_neg != res_neg)) {
        M68K_SetFlag(as, CCR_V);
    } else {
        M68K_ClearFlag(as, CCR_V);
    }
}

/*
 * SUBI - Subtract Immediate
 * Encoding: 0000 0100 ssxx xrrr + immediate data
 */
void M68K_Op_SUBI(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 immediate, dest, result;
    UInt32 mask = SIZE_MASK(size);
    UInt32 sign_bit = SIZE_SIGN_BIT(size);

    /* Fetch immediate value */
    if (size == SIZE_BYTE) {
        immediate = M68K_Fetch16(as) & 0xFF;
    } else if (size == SIZE_WORD) {
        immediate = M68K_Fetch16(as);
    } else {
        immediate = M68K_Fetch32(as);
    }

    /* Read destination */
    dest = M68K_EA_Read(as, mode, reg, size);

    /* Perform subtraction */
    result = (dest - immediate) & mask;

    /* Write result */
    M68K_EA_Write(as, mode, reg, size, result);

    /* Set flags */
    M68K_SetNZ(as, result, size);

    /* Carry flag */
    if (immediate > dest) {
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    /* Overflow flag */
    Boolean dest_neg = (dest & sign_bit) != 0;
    Boolean imm_neg = (immediate & sign_bit) != 0;
    Boolean res_neg = (result & sign_bit) != 0;
    if ((dest_neg != imm_neg) && (dest_neg != res_neg)) {
        M68K_SetFlag(as, CCR_V);
    } else {
        M68K_ClearFlag(as, CCR_V);
    }
}

/*
 * ANDI - AND Immediate
 * Encoding: 0000 0010 ssxx xrrr + immediate data
 */
void M68K_Op_ANDI(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 immediate, dest, result;
    UInt32 mask = SIZE_MASK(size);

    /* Fetch immediate value */
    if (size == SIZE_BYTE) {
        immediate = M68K_Fetch16(as) & 0xFF;
    } else if (size == SIZE_WORD) {
        immediate = M68K_Fetch16(as);
    } else {
        immediate = M68K_Fetch32(as);
    }

    /* Read destination */
    dest = M68K_EA_Read(as, mode, reg, size);

    /* Perform AND */
    result = (dest & immediate) & mask;

    /* Write result */
    M68K_EA_Write(as, mode, reg, size, result);

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * ORI - OR Immediate
 * Encoding: 0000 0000 ssxx xrrr + immediate data
 */
void M68K_Op_ORI(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 immediate, dest, result;
    UInt32 mask = SIZE_MASK(size);

    /* Fetch immediate value */
    if (size == SIZE_BYTE) {
        immediate = M68K_Fetch16(as) & 0xFF;
    } else if (size == SIZE_WORD) {
        immediate = M68K_Fetch16(as);
    } else {
        immediate = M68K_Fetch32(as);
    }

    /* Read destination */
    dest = M68K_EA_Read(as, mode, reg, size);

    /* Perform OR */
    result = (dest | immediate) & mask;

    /* Write result */
    M68K_EA_Write(as, mode, reg, size, result);

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * EORI - Exclusive OR Immediate
 * Encoding: 0000 1010 ssxx xrrr + immediate data
 */
void M68K_Op_EORI(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 size = (opcode >> 6) & 3;
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt32 immediate, dest, result;
    UInt32 mask = SIZE_MASK(size);

    /* Fetch immediate value */
    if (size == SIZE_BYTE) {
        immediate = M68K_Fetch16(as) & 0xFF;
    } else if (size == SIZE_WORD) {
        immediate = M68K_Fetch16(as);
    } else {
        immediate = M68K_Fetch32(as);
    }

    /* Read destination */
    dest = M68K_EA_Read(as, mode, reg, size);

    /* Perform EOR */
    result = (dest ^ immediate) & mask;

    /* Write result */
    M68K_EA_Write(as, mode, reg, size, result);

    /* Set flags */
    M68K_SetNZ(as, result, size);
    M68K_ClearFlag(as, CCR_V | CCR_C);
}

/*
 * ABCD - Add Decimal with Extend
 * Encoding: 1100 rrr1 0000 0rrr (register to register)
 *           1100 rrr1 0000 1rrr (memory to memory with predecrement)
 */
void M68K_Op_ABCD(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dst_reg = (opcode >> 9) & 7;
    UInt8 src_reg = opcode & 7;
    UInt8 mode = (opcode >> 3) & 1;
    UInt8 src, dst, result;
    UInt8 x_flag = M68K_TestFlag(as, CCR_X) ? 1 : 0;

    /* Read source and destination */
    if (mode == 0) {
        /* Register to register */
        src = as->regs.d[src_reg] & 0xFF;
        dst = as->regs.d[dst_reg] & 0xFF;
    } else {
        /* Memory to memory (predecrement) */
        as->regs.a[src_reg] -= 1;
        as->regs.a[dst_reg] -= 1;
        src = M68K_Read8(as, as->regs.a[src_reg]);
        dst = M68K_Read8(as, as->regs.a[dst_reg]);
    }

    /* BCD addition: add low nibbles, then high nibbles */
    UInt8 low_nibble = (src & 0x0F) + (dst & 0x0F) + x_flag;
    UInt8 carry = 0;

    if (low_nibble > 9) {
        low_nibble += 6;
        carry = 1;
    }

    UInt8 high_nibble = ((src >> 4) & 0x0F) + ((dst >> 4) & 0x0F) + carry;

    if (high_nibble > 9) {
        high_nibble += 6;
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    result = ((high_nibble & 0x0F) << 4) | (low_nibble & 0x0F);

    /* Write result */
    if (mode == 0) {
        as->regs.d[dst_reg] = (as->regs.d[dst_reg] & 0xFFFFFF00) | result;
    } else {
        M68K_Write8(as, as->regs.a[dst_reg], result);
    }

    /* Z flag: cleared if result non-zero, unchanged otherwise */
    if (result != 0) {
        M68K_ClearFlag(as, CCR_Z);
    }
}

/*
 * SBCD - Subtract Decimal with Extend
 * Encoding: 1000 rrr1 0000 0rrr (register to register)
 *           1000 rrr1 0000 1rrr (memory to memory with predecrement)
 */
void M68K_Op_SBCD(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dst_reg = (opcode >> 9) & 7;
    UInt8 src_reg = opcode & 7;
    UInt8 mode = (opcode >> 3) & 1;
    UInt8 src, dst, result;
    UInt8 x_flag = M68K_TestFlag(as, CCR_X) ? 1 : 0;

    /* Read source and destination */
    if (mode == 0) {
        /* Register to register */
        src = as->regs.d[src_reg] & 0xFF;
        dst = as->regs.d[dst_reg] & 0xFF;
    } else {
        /* Memory to memory (predecrement) */
        as->regs.a[src_reg] -= 1;
        as->regs.a[dst_reg] -= 1;
        src = M68K_Read8(as, as->regs.a[src_reg]);
        dst = M68K_Read8(as, as->regs.a[dst_reg]);
    }

    /* BCD subtraction: subtract low nibbles, then high nibbles */
    SInt16 low_nibble = (dst & 0x0F) - (src & 0x0F) - x_flag;
    UInt8 borrow = 0;

    if (low_nibble < 0) {
        low_nibble -= 6;
        borrow = 1;
    }

    SInt16 high_nibble = ((dst >> 4) & 0x0F) - ((src >> 4) & 0x0F) - borrow;

    if (high_nibble < 0) {
        high_nibble -= 6;
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    result = ((high_nibble & 0x0F) << 4) | (low_nibble & 0x0F);

    /* Write result */
    if (mode == 0) {
        as->regs.d[dst_reg] = (as->regs.d[dst_reg] & 0xFFFFFF00) | result;
    } else {
        M68K_Write8(as, as->regs.a[dst_reg], result);
    }

    /* Z flag: cleared if result non-zero, unchanged otherwise */
    if (result != 0) {
        M68K_ClearFlag(as, CCR_Z);
    }
}

/*
 * NBCD - Negate Decimal with Extend
 * Encoding: 0100 1000 00xx xrrr
 */
void M68K_Op_NBCD(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt8 dest, result;
    UInt8 x_flag = M68K_TestFlag(as, CCR_X) ? 1 : 0;

    /* Read destination */
    dest = M68K_EA_Read(as, mode, reg, SIZE_BYTE) & 0xFF;

    /* BCD negation: 0 - dest - X */
    SInt16 low_nibble = 0 - (dest & 0x0F) - x_flag;
    UInt8 borrow = 0;

    if (low_nibble < 0) {
        low_nibble -= 6;
        borrow = 1;
    }

    SInt16 high_nibble = 0 - ((dest >> 4) & 0x0F) - borrow;

    if (high_nibble < 0) {
        high_nibble -= 6;
        M68K_SetFlag(as, CCR_C | CCR_X);
    } else {
        M68K_ClearFlag(as, CCR_C | CCR_X);
    }

    result = ((high_nibble & 0x0F) << 4) | (low_nibble & 0x0F);

    /* Write result */
    M68K_EA_Write(as, mode, reg, SIZE_BYTE, result);

    /* Z flag: cleared if result non-zero, unchanged otherwise */
    if (result != 0) {
        M68K_ClearFlag(as, CCR_Z);
    }
}

/*
 * MOVEP - Move Peripheral Data
 * Encoding: 0000 rrr1 ss00 1rrr (memory to register)
 *           0000 rrr1 ss01 1rrr (register to memory)
 */
void M68K_Op_MOVEP(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 data_reg = (opcode >> 9) & 7;
    UInt8 addr_reg = opcode & 7;
    UInt8 dir = (opcode >> 7) & 1;
    UInt8 size = (opcode >> 6) & 1;  /* 0 = word, 1 = long */
    SInt16 displacement;
    UInt32 addr;

    /* Fetch displacement */
    displacement = (SInt16)M68K_Fetch16(as);
    addr = as->regs.a[addr_reg] + displacement;

    if (dir == 0) {
        /* Memory to register */
        if (size == 0) {
            /* Word */
            UInt16 value = (M68K_Read8(as, addr) << 8) |
                          M68K_Read8(as, addr + 2);
            as->regs.d[data_reg] = (as->regs.d[data_reg] & 0xFFFF0000) | value;
        } else {
            /* Long */
            UInt32 value = (M68K_Read8(as, addr) << 24) |
                          (M68K_Read8(as, addr + 2) << 16) |
                          (M68K_Read8(as, addr + 4) << 8) |
                          M68K_Read8(as, addr + 6);
            as->regs.d[data_reg] = value;
        }
    } else {
        /* Register to memory */
        if (size == 0) {
            /* Word */
            UInt16 value = as->regs.d[data_reg] & 0xFFFF;
            M68K_Write8(as, addr, (value >> 8) & 0xFF);
            M68K_Write8(as, addr + 2, value & 0xFF);
        } else {
            /* Long */
            UInt32 value = as->regs.d[data_reg];
            M68K_Write8(as, addr, (value >> 24) & 0xFF);
            M68K_Write8(as, addr + 2, (value >> 16) & 0xFF);
            M68K_Write8(as, addr + 4, (value >> 8) & 0xFF);
            M68K_Write8(as, addr + 6, value & 0xFF);
        }
    }
}

/*
 * CMPM - Compare Memory to Memory
 * Encoding: 1011 rrr1 ss00 1rrr
 */
void M68K_Op_CMPM(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 ay_reg = (opcode >> 9) & 7;
    UInt8 ax_reg = opcode & 7;
    UInt8 size = (opcode >> 6) & 3;
    UInt32 src, dst, result;
    UInt32 mask = SIZE_MASK(size);
    UInt32 sign_bit = SIZE_SIGN_BIT(size);
    UInt8 byte_count = SIZE_BYTES(size);

    /* Read from (Ax)+ */
    if (size == SIZE_BYTE) {
        src = M68K_Read8(as, as->regs.a[ax_reg]);
    } else if (size == SIZE_WORD) {
        src = M68K_Read16(as, as->regs.a[ax_reg]);
    } else {
        src = M68K_Read32(as, as->regs.a[ax_reg]);
    }
    as->regs.a[ax_reg] += byte_count;

    /* Read from (Ay)+ */
    if (size == SIZE_BYTE) {
        dst = M68K_Read8(as, as->regs.a[ay_reg]);
    } else if (size == SIZE_WORD) {
        dst = M68K_Read16(as, as->regs.a[ay_reg]);
    } else {
        dst = M68K_Read32(as, as->regs.a[ay_reg]);
    }
    as->regs.a[ay_reg] += byte_count;

    /* Perform comparison (dst - src) */
    result = (dst - src) & mask;

    /* Set flags */
    M68K_SetNZ(as, result, size);

    /* Carry flag */
    if (src > dst) {
        M68K_SetFlag(as, CCR_C);
    } else {
        M68K_ClearFlag(as, CCR_C);
    }

    /* Overflow flag */
    Boolean dst_neg = (dst & sign_bit) != 0;
    Boolean src_neg = (src & sign_bit) != 0;
    Boolean res_neg = (result & sign_bit) != 0;
    if ((dst_neg != src_neg) && (dst_neg != res_neg)) {
        M68K_SetFlag(as, CCR_V);
    } else {
        M68K_ClearFlag(as, CCR_V);
    }
}

/*
 * ILLEGAL - Illegal Instruction
 * Encoding: 0100 1010 1111 1100
 */
void M68K_Op_ILLEGAL(M68KAddressSpace* as, UInt16 opcode)
{
    (void)opcode;
    M68K_Fault(as, "ILLEGAL instruction executed");
}

/*
 * RESET - Reset External Devices
 * Encoding: 0100 1110 0111 0000
 *
 * Supervisor only - asserts RESET line to external devices.
 * In our emulator, this is a no-op since we have no external hardware.
 */
void M68K_Op_RESET(M68KAddressSpace* as, UInt16 opcode)
{
    (void)opcode;

    /* Check supervisor mode */
    if (!(as->regs.sr & SR_S)) {
        M68K_Fault(as, "RESET in user mode (privilege violation)");
        return;
    }

    /* In a real system, this would pulse the RESET line.
     * In our emulator, we just acknowledge it. */
    M68K_LOG_DEBUG("RESET instruction executed (no-op in emulator)\n");
}

/*
 * TRAPV - Trap on Overflow
 * Encoding: 0100 1110 0111 0110
 */
void M68K_Op_TRAPV(M68KAddressSpace* as, UInt16 opcode)
{
    (void)opcode;

    /* Check overflow flag */
    if (M68K_TestFlag(as, CCR_V)) {
        M68K_Fault(as, "TRAPV: overflow exception");
    }
}

/*
 * RTR - Return and Restore Condition Codes
 * Encoding: 0100 1110 0111 0111
 */
void M68K_Op_RTR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 ccr;
    UInt32 ret_addr;

    (void)opcode;

    /* Pop CCR from stack */
    ccr = M68K_Pop16(as);
    as->regs.sr = (as->regs.sr & 0xFF00) | (ccr & 0x001F);

    /* Pop return address from stack */
    ret_addr = M68K_Pop32(as);
    as->regs.pc = ret_addr;
}

/*
 * ANDI to CCR - AND Immediate to Condition Code Register
 * Encoding: 0000 0010 0011 1100
 */
void M68K_Op_ANDI_CCR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 immediate;

    (void)opcode;

    /* Fetch immediate byte */
    immediate = M68K_Fetch16(as) & 0xFF;

    /* AND with CCR (lower byte of SR) */
    as->regs.sr = (as->regs.sr & 0xFF00) | ((as->regs.sr & 0x00FF) & immediate);
}

/*
 * ANDI to SR - AND Immediate to Status Register
 * Encoding: 0000 0010 0111 1100
 */
void M68K_Op_ANDI_SR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 immediate;

    (void)opcode;

    /* Check supervisor mode */
    if (!(as->regs.sr & SR_S)) {
        M68K_Fault(as, "ANDI to SR in user mode (privilege violation)");
        return;
    }

    /* Fetch immediate word */
    immediate = M68K_Fetch16(as);

    /* AND with entire SR */
    as->regs.sr &= immediate;
}

/*
 * ORI to CCR - OR Immediate to Condition Code Register
 * Encoding: 0000 0000 0011 1100
 */
void M68K_Op_ORI_CCR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 immediate;

    (void)opcode;

    /* Fetch immediate byte */
    immediate = M68K_Fetch16(as) & 0xFF;

    /* OR with CCR (lower byte of SR) */
    as->regs.sr = (as->regs.sr & 0xFF00) | ((as->regs.sr & 0x00FF) | immediate);
}

/*
 * ORI to SR - OR Immediate to Status Register
 * Encoding: 0000 0000 0111 1100
 */
void M68K_Op_ORI_SR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 immediate;

    (void)opcode;

    /* Check supervisor mode */
    if (!(as->regs.sr & SR_S)) {
        M68K_Fault(as, "ORI to SR in user mode (privilege violation)");
        return;
    }

    /* Fetch immediate word */
    immediate = M68K_Fetch16(as);

    /* OR with entire SR */
    as->regs.sr |= immediate;
}

/*
 * EORI to CCR - Exclusive OR Immediate to Condition Code Register
 * Encoding: 0000 1010 0011 1100
 */
void M68K_Op_EORI_CCR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 immediate;

    (void)opcode;

    /* Fetch immediate byte */
    immediate = M68K_Fetch16(as) & 0xFF;

    /* EOR with CCR (lower byte of SR) */
    as->regs.sr = (as->regs.sr & 0xFF00) | ((as->regs.sr & 0x00FF) ^ immediate);
}

/*
 * EORI to SR - Exclusive OR Immediate to Status Register
 * Encoding: 0000 1010 0111 1100
 */
void M68K_Op_EORI_SR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt16 immediate;

    (void)opcode;

    /* Check supervisor mode */
    if (!(as->regs.sr & SR_S)) {
        M68K_Fault(as, "EORI to SR in user mode (privilege violation)");
        return;
    }

    /* Fetch immediate word */
    immediate = M68K_Fetch16(as);

    /* EOR with entire SR */
    as->regs.sr ^= immediate;
}

/*
 * MOVE to CCR - Move to Condition Code Register
 * Encoding: 0100 0100 1100 0000 + EA
 */
void M68K_Op_MOVE_CCR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt16 value;

    /* Read source operand (word) */
    value = M68K_EA_Read(as, mode, reg, SIZE_WORD);

    /* Move to CCR (lower byte of SR) */
    as->regs.sr = (as->regs.sr & 0xFF00) | (value & 0x001F);
}

/*
 * MOVE to SR - Move to Status Register
 * Encoding: 0100 0110 1100 0000 + EA
 */
void M68K_Op_MOVE_SR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt16 value;

    /* Check supervisor mode */
    if (!(as->regs.sr & SR_S)) {
        M68K_Fault(as, "MOVE to SR in user mode (privilege violation)");
        return;
    }

    /* Read source operand (word) */
    value = M68K_EA_Read(as, mode, reg, SIZE_WORD);

    /* Move to SR */
    as->regs.sr = value;
}

/*
 * MOVE from SR - Move from Status Register
 * Encoding: 0100 0000 1100 0000 + EA
 */
void M68K_Op_MOVE_FROM_SR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;

    /* Write SR to destination */
    M68K_EA_Write(as, mode, reg, SIZE_WORD, as->regs.sr);
}

/*
 * MOVE USP - Move User Stack Pointer
 * Encoding: 0100 1110 0110 drrr
 * d = direction (0 = An to USP, 1 = USP to An)
 */
void M68K_Op_MOVE_USP(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 dir = (opcode >> 3) & 1;
    UInt8 reg = opcode & 7;

    /* Check supervisor mode */
    if (!(as->regs.sr & SR_S)) {
        M68K_Fault(as, "MOVE USP in user mode (privilege violation)");
        return;
    }

    if (dir == 0) {
        /* An to USP */
        as->regs.usp = as->regs.a[reg];
    } else {
        /* USP to An */
        as->regs.a[reg] = as->regs.usp;
    }
}

/*
 * ============================================================================
 * UNDOCUMENTED 68000 FEATURES
 * ============================================================================
 * The following opcodes and behaviors are undocumented in the original 68000
 * manual but work on real hardware. Some software relies on these quirks.
 */

/*
 * MOVE from CCR - Move Condition Code Register to Destination
 * Encoding: 0100 0010 1100 0000 + EA
 *
 * UNDOCUMENTED on 68000 (officially added in 68010)
 * Despite not being in the manual, MOVE CCR,<ea> works on 68000 hardware.
 * It reads only the lower byte of SR (the CCR portion).
 *
 * Some Mac software discovered and used this before the 68010 existed.
 */
void M68K_Op_MOVE_FROM_CCR(M68KAddressSpace* as, UInt16 opcode)
{
    UInt8 mode = (opcode >> 3) & 7;
    UInt8 reg = opcode & 7;
    UInt16 ccr_value;

    /* Read only CCR (lower byte of SR), with upper byte zeroed.
     * This is the key difference from MOVE from SR which reads the full SR. */
    ccr_value = as->regs.sr & 0x001F;

    /* Write to destination */
    M68K_EA_Write(as, mode, reg, SIZE_WORD, ccr_value);
}

/*
 * Address Error Detection
 *
 * The 68000 requires word and long accesses to be aligned on even addresses.
 * Accessing word/long data at odd addresses triggers an address error exception.
 *
 * This function should be called before any word/long memory access.
 */
void M68K_CheckAddressAlignment(M68KAddressSpace* as, UInt32 addr, M68KSize size)
{
    if (size != SIZE_BYTE && (addr & 1)) {
        /* Address error - odd address for word/long access */
        M68K_LOG_ERROR("Address error: %s access at odd address 0x%08X\n",
                      size == SIZE_WORD ? "WORD" : "LONG", addr);
        M68K_Fault(as, "Address error: unaligned word/long access");
    }
}

/*
 * Undocumented ADDX/SUBX Zero Flag Behavior
 *
 * The Z flag for ADDX/SUBX has special undocumented behavior:
 * - Z is cleared if the result is non-zero
 * - Z is UNCHANGED if the result is zero (not set!)
 *
 * This allows multi-precision arithmetic to correctly detect all-zero results
 * across multiple operations. Our implementations already handle this correctly.
 */

/*
 * Undocumented NBCD/SBCD/ABCD Zero Flag Behavior
 *
 * Similar to ADDX/SUBX, BCD operations have special Z flag behavior:
 * - Z is cleared if result is non-zero
 * - Z is UNCHANGED if result is zero
 *
 * Our implementations already handle this correctly.
 */

/*
 * MOVEM Register List Quirk
 *
 * UNDOCUMENTED: If the address register used for -(An) mode is also in the
 * register list, the 68000 stores the ORIGINAL value before decrementing,
 * not the decremented value. This is different from the 68010+.
 *
 * Example: MOVEM D0-D7/A0-A7,-(A7)
 * On 68000: A7 is stored with its original value (before any decrement)
 * On 68010+: A7 is stored with decremented value
 *
 * Our implementation currently stores the original value, matching 68000 behavior.
 */

/*
 * RESET Timing Quirk
 *
 * UNDOCUMENTED: The RESET instruction asserts the RESET line for 124 clock cycles.
 * This is longer than most instructions. Some software may use RESET as a delay.
 *
 * In our emulator, we treat it as a no-op with no delay.
 */

/*
 * Privilege Violation Stack Frame
 *
 * UNDOCUMENTED DETAIL: When a privilege violation occurs, the 68000 pushes:
 * 1. SR (with S bit still set to 0 - the value that caused the violation)
 * 2. PC (pointing to the instruction AFTER the privileged instruction)
 *
 * Our fault handler should ideally save state for exception handling.
 */

/*
 * STOP Instruction Quirk
 *
 * UNDOCUMENTED: STOP loads the immediate value into SR, then stops.
 * If the new SR value clears the S bit while already in user mode,
 * a privilege violation occurs BEFORE the processor stops.
 *
 * Our implementation treats STOP as NOP in both modes.
 */

/*
 * CHK Instruction Details
 *
 * UNDOCUMENTED FLAG BEHAVIOR:
 * - N flag is set if Dn < 0 (negative)
 * - N flag is cleared if Dn > <ea> (too large)
 * - Z, V, C flags are undefined (not modified on some CPUs, random on others)
 *
 * Our implementation sets N appropriately and leaves other flags unchanged.
 */

/*
 * TAS Instruction Bus Lock
 *
 * UNDOCUMENTED: TAS performs a read-modify-write cycle that locks the bus.
 * This ensures atomicity in multiprocessor systems.
 *
 * In our single-threaded emulator, this is automatic.
 */

/*
 * Illegal Instruction Patterns
 *
 * Some illegal opcodes produce predictable results on real 68000 hardware:
 *
 * 1. Line-A (0xAxxx): Trap vector 10 - Used for Mac Toolbox traps ✅ Implemented
 * 2. Line-F (0xFxxx): Trap vector 11 - Used for FPU emulation (not implemented)
 * 3. 0x4AFC (ILLEGAL): Official illegal instruction ✅ Implemented
 * 4. Unimplemented effective addressing modes: Cause illegal instruction trap
 *
 * Most "illegal" opcodes will trigger our fault handler.
 */

/*
 * DBcc with cc=true Quirk
 *
 * UNDOCUMENTED: When DBcc condition is true:
 * - The register is NOT decremented
 * - The branch is NOT taken
 * - Execution continues with next instruction
 *
 * This is correctly implemented in our DBcc handler.
 */

/*
 * Sign Extension Behavior
 *
 * UNDOCUMENTED DETAIL: MOVEA.W sign-extends the 16-bit source to 32 bits.
 * This is different from MOVE.W which zero-extends when moving to Dn.
 *
 * Our implementation correctly sign-extends for MOVEA.
 */

/*
 * Stack Pointer Alignment
 *
 * BEST PRACTICE (not enforced by 68000 hardware):
 * The stack pointer should always remain even-aligned.
 * Operations that push/pop bytes should adjust by 2, not 1.
 *
 * Our implementation assumes proper alignment but doesn't enforce it.
 */
