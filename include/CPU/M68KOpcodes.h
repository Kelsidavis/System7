/*
 * M68KOpcodes.h - 68K Instruction Definitions and Helpers
 *
 * Defines CCR flags, instruction sizes, and helper macros for the
 * Phase-1 MVP 68K interpreter implementation.
 */

#ifndef M68K_OPCODES_H
#define M68K_OPCODES_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CCR (Condition Code Register) Flags - lower byte of SR
 */
#define CCR_C   0x0001   /* Carry */
#define CCR_V   0x0002   /* Overflow */
#define CCR_Z   0x0004   /* Zero */
#define CCR_N   0x0008   /* Negative */
#define CCR_X   0x0010   /* Extend */

/*
 * SR (Status Register) Flags - upper byte
 */
#define SR_T    0x8000   /* Trace */
#define SR_S    0x2000   /* Supervisor */
#define SR_INT_MASK 0x0700  /* Interrupt mask */

/*
 * Instruction Sizes
 */
typedef enum {
    SIZE_BYTE = 0,
    SIZE_WORD = 1,
    SIZE_LONG = 2
} M68KSize;

/*
 * Size Helpers
 */
#define SIZE_BYTES(sz) ((sz) == SIZE_BYTE ? 1 : (sz) == SIZE_WORD ? 2 : 4)
#define SIZE_MASK(sz) ((sz) == SIZE_BYTE ? 0xFF : (sz) == SIZE_WORD ? 0xFFFF : 0xFFFFFFFF)
#define SIZE_SIGN_BIT(sz) ((sz) == SIZE_BYTE ? 0x80 : (sz) == SIZE_WORD ? 0x8000 : 0x80000000)

/*
 * Addressing Mode Encoding
 */
#define EA_MODE(ea) (((ea) >> 3) & 7)
#define EA_REG(ea)  ((ea) & 7)

/*
 * Addressing Modes
 */
typedef enum {
    MODE_Dn = 0,        /* Dn - data register direct */
    MODE_An = 1,        /* An - address register direct */
    MODE_An_IND = 2,    /* (An) - address register indirect */
    MODE_An_POST = 3,   /* (An)+ - postincrement */
    MODE_An_PRE = 4,    /* -(An) - predecrement */
    MODE_An_DISP = 5,   /* d16(An) - displacement */
    MODE_An_INDEX = 6,  /* d8(An,Xn) - indexed */
    MODE_OTHER = 7      /* Special modes (abs, imm, PC-rel) */
} M68KAddrMode;

/*
 * MODE_OTHER sub-modes (determined by register field)
 */
#define OTHER_ABS_W     0   /* abs.W */
#define OTHER_ABS_L     1   /* abs.L */
#define OTHER_PC_DISP   2   /* d16(PC) */
#define OTHER_PC_INDEX  3   /* d8(PC,Xn) */
#define OTHER_IMMEDIATE 4   /* #<data> */

/*
 * Condition Codes (for Bcc, DBcc, Scc)
 */
typedef enum {
    CC_T  = 0x0,   /* True */
    CC_F  = 0x1,   /* False */
    CC_HI = 0x2,   /* High (C=0 and Z=0) */
    CC_LS = 0x3,   /* Low or Same (C=1 or Z=1) */
    CC_CC = 0x4,   /* Carry Clear (C=0) */
    CC_CS = 0x5,   /* Carry Set (C=1) */
    CC_NE = 0x6,   /* Not Equal (Z=0) */
    CC_EQ = 0x7,   /* Equal (Z=1) */
    CC_VC = 0x8,   /* Overflow Clear (V=0) */
    CC_VS = 0x9,   /* Overflow Set (V=1) */
    CC_PL = 0xA,   /* Plus (N=0) */
    CC_MI = 0xB,   /* Minus (N=1) */
    CC_GE = 0xC,   /* Greater or Equal (N=V) */
    CC_LT = 0xD,   /* Less Than (N!=V) */
    CC_GT = 0xE,   /* Greater Than (Z=0 and N=V) */
    CC_LE = 0xF    /* Less or Equal (Z=1 or N!=V) */
} M68KCondition;

/*
 * Opcode Masks and Patterns
 */
#define OP_MOVE_MASK        0xC000
#define OP_MOVE_PATTERN     0x0000   /* MOVE uses 00xx for sizes */

#define OP_MOVEA_MASK       0xC1C0
#define OP_MOVEA_PATTERN    0x0040   /* MOVEA has bit 6 set */

#define OP_LEA_MASK         0xF1C0
#define OP_LEA_PATTERN      0x41C0

#define OP_PEA_MASK         0xFFC0
#define OP_PEA_PATTERN      0x4840

#define OP_CLR_MASK         0xFF00
#define OP_CLR_PATTERN      0x4200

#define OP_NOT_MASK         0xFF00
#define OP_NOT_PATTERN      0x4600

#define OP_JSR_MASK         0xFFC0
#define OP_JSR_PATTERN      0x4E80

#define OP_JMP_MASK         0xFFC0
#define OP_JMP_PATTERN      0x4EC0

#define OP_RTS_MASK         0xFFFF
#define OP_RTS_PATTERN      0x4E75

#define OP_LINK_MASK        0xFFF8
#define OP_LINK_PATTERN     0x4E50

#define OP_UNLK_MASK        0xFFF8
#define OP_UNLK_PATTERN     0x4E58

#define OP_ADD_MASK         0xF000
#define OP_ADD_PATTERN      0xD000

#define OP_SUB_MASK         0xF000
#define OP_SUB_PATTERN      0x9000

#define OP_CMP_MASK         0xF100
#define OP_CMP_PATTERN      0xB000

#define OP_BRA_MASK         0xFF00
#define OP_BRA_PATTERN      0x6000

#define OP_BSR_MASK         0xFF00
#define OP_BSR_PATTERN      0x6100

#define OP_Bcc_MASK         0xF000
#define OP_Bcc_PATTERN      0x6000

#define OP_TRAP_MASK        0xF000
#define OP_TRAP_PATTERN     0xA000   /* A-line trap */

/*
 * Helper Macros
 */
#define EXTRACT_SIZE(op)     (((op) >> 6) & 3)
#define EXTRACT_REG(op)      (((op) >> 9) & 7)
#define EXTRACT_EA(op)       ((op) & 0x3F)
#define EXTRACT_MODE(op)     (((op) >> 3) & 7)
#define EXTRACT_EAREG(op)    ((op) & 7)

#define SIGN_EXTEND_BYTE(b)  ((SInt32)(SInt8)(b))
#define SIGN_EXTEND_WORD(w)  ((SInt32)(SInt16)(w))

/*
 * CCR Test Macros
 */
#define TEST_CC(sr, cc) M68K_TestCondition(sr, cc)

/*
 * Forward declarations
 */
typedef struct M68KAddressSpace M68KAddressSpace;

/*
 * Opcode Handler Function Prototypes
 */
extern void M68K_Fault(M68KAddressSpace* as, const char* reason);
extern Boolean M68K_TestCondition(UInt16 sr, M68KCondition cc);

/* Data movement operations */
extern void M68K_Op_MOVE(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVEA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVEQ(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_LEA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_PEA(M68KAddressSpace* as, UInt16 opcode);

/* Arithmetic operations */
extern void M68K_Op_ADD(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ADDA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ADDQ(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUB(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUBA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUBQ(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CMP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CMPA(M68KAddressSpace* as, UInt16 opcode);

/* Logical operations */
extern void M68K_Op_AND(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_OR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EOR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CLR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_NOT(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_TST(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_NOP(M68KAddressSpace* as, UInt16 opcode);

/* Bit manipulation */
extern void M68K_Op_EXT(M68KAddressSpace* as, UInt16 opcode);

/* The 68020's (M68K68020.c) */
extern void M68K_Op_MULL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_DIVL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EXTB(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_LINKL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CHKL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CMP2(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CAS(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_Bitfield(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_PACK(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_UNPK(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_RTD(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_TRAPcc(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BKPT(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SWAP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EXG(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BTST(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BSET(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BCLR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BCHG(M68KAddressSpace* as, UInt16 opcode);

/* Shift operations */
extern void M68K_Op_LSL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_LSR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ASL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ASR(M68KAddressSpace* as, UInt16 opcode);

/* Rotate operations */
extern void M68K_Op_ROL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ROR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ROXL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ROXR(M68KAddressSpace* as, UInt16 opcode);

/* Multiply/Divide operations */
extern void M68K_Op_MULU(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MULS(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_DIVU(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_DIVS(M68KAddressSpace* as, UInt16 opcode);

/* Negate operations */
extern void M68K_Op_NEG(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_NEGX(M68KAddressSpace* as, UInt16 opcode);

/* Extended precision operations */
extern void M68K_Op_ADDX(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUBX(M68KAddressSpace* as, UInt16 opcode);

/* Bounds checking */
extern void M68K_Op_CHK(M68KAddressSpace* as, UInt16 opcode);

/* Test and set */
extern void M68K_Op_TAS(M68KAddressSpace* as, UInt16 opcode);

/* Compare immediate */
extern void M68K_Op_CMPI(M68KAddressSpace* as, UInt16 opcode);

/* Immediate instruction variants */
extern void M68K_Op_ADDI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUBI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ANDI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ORI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EORI(M68KAddressSpace* as, UInt16 opcode);

/* BCD arithmetic */
extern void M68K_Op_ABCD(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SBCD(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_NBCD(M68KAddressSpace* as, UInt16 opcode);

/* Peripheral data transfer */
extern void M68K_Op_MOVEP(M68KAddressSpace* as, UInt16 opcode);

/* Compare memory */
extern void M68K_Op_CMPM(M68KAddressSpace* as, UInt16 opcode);

/* Supervisor and special operations */
extern void M68K_Op_ILLEGAL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_RESET(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_TRAPV(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_RTR(M68KAddressSpace* as, UInt16 opcode);

/* CCR/SR manipulation */
extern void M68K_Op_ANDI_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ANDI_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ORI_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ORI_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EORI_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EORI_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVE_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVE_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVE_FROM_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVE_USP(M68KAddressSpace* as, UInt16 opcode);

/* Multi-register operations */
extern void M68K_Op_MOVEM(M68KAddressSpace* as, UInt16 opcode);

/* Undocumented 68000 features */
extern void M68K_Op_MOVE_FROM_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_CheckAddressAlignment(M68KAddressSpace* as, UInt32 addr, M68KSize size);

/* Control flow - unconditional */
extern void M68K_Op_JMP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_JSR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_RTS(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_RTE(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BRA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BSR(M68KAddressSpace* as, UInt16 opcode);

/* Control flow - conditional */
extern void M68K_Op_Bcc(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_DBcc(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_Scc(M68KAddressSpace* as, UInt16 opcode);

/* Stack operations */
extern void M68K_Op_LINK(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_UNLK(M68KAddressSpace* as, UInt16 opcode);

/* System operations */
extern void M68K_Op_TRAP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_STOP(M68KAddressSpace* as, UInt16 opcode);

#ifdef __cplusplus
}
#endif

#endif /* M68K_OPCODES_H */
