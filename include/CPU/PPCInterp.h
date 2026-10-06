/*
 * PPCInterp.h - PowerPC Interpreter CPU Backend
 *
 * Implements ICPUBackend for PowerPC code execution on any host ISA (x86, ARM, etc).
 * Uses software interpretation of PowerPC instructions with explicit big-endian
 * byte ordering to ensure cross-platform compatibility.
 *
 * PowerPC Architecture:
 * - RISC design with fixed 32-bit instruction width
 * - 32 general-purpose registers (GPR0-GPR31)
 * - 32 floating-point registers (FPR0-FPR31)
 * - Condition register (CR) with 8 4-bit fields
 * - Link register (LR) for function returns
 * - Count register (CTR) for loops
 * - Big-endian byte order (like 68K)
 * - Load/store architecture (only load/store access memory)
 *
 * Supported PowerPC:
 * - PowerPC 601, 603, 604, G3, G4 (32-bit)
 * - User-mode instructions only (no supervisor mode initially)
 */

#ifndef PPC_INTERP_H
#define PPC_INTERP_H

#include "CPU/CPUBackend.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PowerPC Register File
 */
typedef struct PPCRegs {
    UInt32 gpr[32];           /* GPR0-GPR31 general-purpose registers */
    double fpr[32];           /* FPR0-FPR31 floating-point registers */
    UInt32 pc;                /* Program counter (CIA - Current Instruction Address) */
    UInt32 lr;                /* Link register (for function returns) */
    UInt32 ctr;               /* Count register (for loops) */
    UInt32 cr;                /* Condition register (8 4-bit fields) */
    UInt32 xer;               /* Fixed-point exception register */
    UInt32 fpscr;             /* Floating-point status and control register */
    UInt32 msr;               /* Machine state register */
    UInt32 tbl;               /* Time base lower */
    UInt32 tbu;               /* Time base upper */
    UInt32 dec;               /* Decrementer */
    UInt32 pvr;               /* Processor version register */

    /* Exception/Interrupt registers */
    UInt32 srr0;              /* Save/restore register 0 (exception PC) */
    UInt32 srr1;              /* Save/restore register 1 (exception MSR) */
    UInt32 dar;               /* Data address register (fault address) */
    UInt32 dsisr;             /* DSI status register (fault status) */

    /* Memory management */
    UInt32 sdr1;              /* Page table base register */
    UInt32 sr[16];            /* Segment registers SR0-SR15 */

    /* OS scratch registers */
    UInt32 sprg[4];           /* SPRG0-SPRG3 general purpose */

    /* Block address translation (BAT) registers */
    UInt32 ibat[8];           /* IBAT0U, IBAT0L, IBAT1U, IBAT1L, ... IBAT3U, IBAT3L */
    UInt32 dbat[8];           /* DBAT0U, DBAT0L, DBAT1U, DBAT1L, ... DBAT3U, DBAT3L */

    /* Hardware implementation dependent */
    UInt32 hid0;              /* Hardware implementation register 0 */
    UInt32 hid1;              /* Hardware implementation register 1 */
    UInt32 l2cr;              /* L2 cache control register (G3/G4) */
    UInt32 ictc;              /* Instruction cache throttling control */

    /* Debug/performance registers */
    UInt32 iabr;              /* Instruction address breakpoint */
    UInt32 dabr;              /* Data address breakpoint */
    UInt32 ear;               /* External access register */

    /* Thermal management (G3/G4) */
    UInt32 thrm[3];           /* THRM1-THRM3: thermal management registers */

    /* AltiVec/VMX vector registers (G4/G5) */
    UInt32 vr[32][4];         /* VR0-VR31: 32 128-bit vector registers (stored as 4x32-bit) */
    UInt32 vscr;              /* Vector status and control register */
    UInt32 vrsave;            /* Vector register save register */
} PPCRegs;

/*
 * PowerPC Condition Register Fields (CR0-CR7)
 * Each field has 4 bits: LT, GT, EQ, SO
 */
#define PPC_CR_LT(n)  (0x80000000 >> (n*4 + 0))  /* Less than */
#define PPC_CR_GT(n)  (0x80000000 >> (n*4 + 1))  /* Greater than */
#define PPC_CR_EQ(n)  (0x80000000 >> (n*4 + 2))  /* Equal */
#define PPC_CR_SO(n)  (0x80000000 >> (n*4 + 3))  /* Summary overflow */

/*
 * PowerPC XER Register Bits
 */
#define PPC_XER_SO    0x80000000  /* Summary overflow */
#define PPC_XER_OV    0x40000000  /* Overflow */
#define PPC_XER_CA    0x20000000  /* Carry */

/*
 * Paged Memory Constants (same as M68K for compatibility)
 */
#define PPC_PAGE_SIZE      4096        /* 4KB pages */
#define PPC_PAGE_SHIFT     12          /* log2(4096) */
#define PPC_MAX_ADDR       0x1000000   /* 16MB virtual address space (for now) */
#define PPC_NUM_PAGES      4096        /* 16MB / 4KB */
#define PPC_ALLOC_BASE     0x1000      /* Keep the null page unmapped */

/*
 * PowerPC Address Space Implementation
 */
typedef struct PPCAddressSpace {
    void* pageTable[PPC_NUM_PAGES];  /* Sparse page table (NULL = not allocated) */
    UInt32 baseAddr;          /* Base address (typically 0) */
    UInt32 nextAlloc;         /* Next address for code or data reservations */

    PPCRegs regs;             /* CPU registers */

    /* Trap table (for Mac Toolbox traps via sc instruction) */
    CPUTrapHandler trapHandlers[256];
    void* trapContexts[256];

    /* Segment tracking */
    void* codeSegments[256];
    UInt32 codeSegBases[256];
    Size codeSegSizes[256];
    int numCodeSegs;

    /* Execution state */
    Boolean halted;           /* CPU halted due to fault or completion */
    UInt16 lastException;     /* Last exception code */
} PPCAddressSpace;

/*
 * PowerPC Code Handle Implementation
 */
typedef struct PPCCodeHandle {
    void* hostMemory;         /* Host-mapped memory */
    UInt32 cpuAddr;           /* CPU address */
    Size size;                /* Size */
    int segIndex;             /* Index in address space segment table */
} PPCCodeHandle;

/*
 * PowerPC Backend Initialization
 */
OSErr PPCBackend_Initialize(void);

/*
 * PowerPC Interpreter Core (exposed for testing)
 */
OSErr PPC_Execute(PPCAddressSpace* as, UInt32 startPC, UInt32 maxInstructions);
OSErr PPC_Step(PPCAddressSpace* as);

#ifdef __cplusplus
}
#endif

#endif /* PPC_INTERP_H */
