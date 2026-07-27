/*
 * M68KInterp.h - 68K Interpreter CPU Backend
 *
 * Implements ICPUBackend for 68K code execution on any host ISA (x86, ARM, etc).
 * Uses software interpretation of 68K instructions with explicit big-endian
 * byte ordering to ensure cross-platform compatibility.
 *
 * Platform Independence:
 * - All multi-byte values use explicit big-endian construction (not host byte order)
 * - Memory access is abstracted through paged system (architecture-agnostic)
 * - Alignment checks follow 68K rules (2-byte alignment), not host requirements
 * - Register file is generic (no host CPU registers used)
 *
 * Supported Host Architectures:
 * - x86/x86-64: Full support via direct interpretation
 * - ARM (all variants): Full support via direct interpretation
 * - PowerPC, MIPS, etc.: Should work with no modifications
 */

#ifndef M68K_INTERP_H
#define M68K_INTERP_H

#include "CPU/CPUBackend.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * M68K Register File
 */
typedef struct M68KRegs {
    UInt32 d[8];              /* D0-D7 data registers */
    UInt32 a[8];              /* A0-A7 address registers (A7 = SP) */
    UInt32 pc;                /* Program counter */
    UInt16 sr;                /* Status register */
    UInt32 usp;               /* User stack pointer */
    UInt32 ssp;               /* Supervisor stack pointer */
} M68KRegs;

/*
 * 68K Exception Vectors
 */
#define M68K_VEC_RESET_SSP      0   /* Reset: Initial SSP */
#define M68K_VEC_RESET_PC       1   /* Reset: Initial PC */
#define M68K_VEC_BUS_ERROR      2   /* Bus error */
#define M68K_VEC_ADDRESS_ERROR  3   /* Address error */
#define M68K_VEC_ILLEGAL        4   /* Illegal instruction */
#define M68K_VEC_DIVIDE_ZERO    5   /* Integer divide by zero */
#define M68K_VEC_CHK            6   /* CHK instruction */
#define M68K_VEC_TRAPV          7   /* TRAPV instruction */
#define M68K_VEC_PRIVILEGE      8   /* Privilege violation */
#define M68K_VEC_TRACE          9   /* Trace */
#define M68K_VEC_LINE_A         10  /* Line 1010 emulator */
#define M68K_VEC_LINE_F         11  /* Line 1111 emulator */

/*
 * Paged Memory Constants
 */
#define M68K_PAGE_SIZE      4096        /* 4KB pages */
#define M68K_PAGE_SHIFT     12          /* log2(4096) */
#define M68K_MAX_ADDR       0x1000000   /* 16MB virtual address space */
#define M68K_NUM_PAGES      4096        /* 16MB / 4KB */
#define M68K_LOW_MEM_SIZE   0x10000     /* 64KB low memory (always present) */
#define M68K_LOW_MEM_PAGES  16          /* 64KB / 4KB */

/*
 * M68K Address Space Implementation
 */
typedef struct M68KAddressSpace {
    void* pageTable[M68K_NUM_PAGES];  /* Sparse page table (NULL = not allocated) */
    UInt32 baseAddr;          /* Base address (typically 0) */

    M68KRegs regs;            /* CPU registers */

    /* Trap table (A-line traps 0xA000-0xAFFF) */
    CPUTrapHandler trapHandlers[256];
    void* trapContexts[256];

    /* Segment tracking */
    void* codeSegments[256];
    UInt32 codeSegBases[256];
    Size codeSegSizes[256];
    int numCodeSegs;

    /* Execution state */
    Boolean halted;           /* CPU halted due to fault or completion */
    UInt16 lastException;     /* Last exception vector number */
} M68KAddressSpace;

/*
 * M68K Code Handle Implementation
 */
typedef struct M68KCodeHandle {
    void* hostMemory;         /* Host-mapped memory */
    UInt32 cpuAddr;           /* CPU address */
    Size size;                /* Size */
    int segIndex;             /* Index in address space segment table */
} M68KCodeHandle;

/*
 * M68K Backend Initialization
 */
OSErr M68KBackend_Initialize(void);

/*
 * M68K Interpreter Core (exposed for testing)
 */
OSErr M68K_Execute(M68KAddressSpace* as, UInt32 startPC, UInt32 maxInstructions);
OSErr M68K_Step(M68KAddressSpace* as);

/* Run five instructions of known result. Silent unless one comes out wrong.
 * Nothing else in the system executes 68K code, so this is what would notice
 * the interpreter breaking. */
void M68K_SelfTest(void);

#ifdef __cplusplus
}
#endif

#endif /* M68K_INTERP_H */
