/*
 * M68KDecode.c - 68K Instruction Fetch and Effective Address Decoding
 *
 * Provides fetch helpers, EA computation, and EA read/write operations
 * for the Phase-1 MVP 68K interpreter.
 *
 * CROSS-PLATFORM DESIGN:
 * This module uses EXPLICIT BIG-ENDIAN byte ordering for all multi-byte values.
 * This ensures the interpreter works identically on little-endian (x86, ARM, etc)
 * and big-endian (PowerPC, SPARC, etc) host architectures.
 *
 * Key Design Decisions:
 * - Fetch16/Fetch32: Reconstructed from individual bytes in big-endian order
 *   Example: (b0 << 8) | b1  --  NOT *(uint16_t*)&b0
 * - Read16/Read32/Write16/Write32: Always use explicit byte extraction/insertion
 * - No host byte order assumptions anywhere in the code
 * - Alignment checks enforce 68K requirements (2-byte words), not host CPU needs
 *
 * This design enables the 68K interpreter to run on:
 * - x86 (little-endian): Full compatibility
 * - ARM (little-endian): Full compatibility, Raspberry Pi support
 * - PowerPC (big-endian): Should work unchanged
 * - Any other ISA: Should work as long as basic C types work correctly
 */

#include "CPU/M68KInterp.h"
#include "CPU/M68KOpcodes.h"
#include "System71StdLib.h"
#include "CPU/CPULogging.h"
#include <string.h>

/*
 * Forward declarations from M68KOpcodes.c
 */
extern void M68K_Fault(M68KAddressSpace* as, const char* reason);

/*
 * Forward declarations for memory access (to avoid implicit declarations)
 */
UInt8 M68K_Read8(M68KAddressSpace* as, UInt32 addr);
UInt16 M68K_Read16(M68KAddressSpace* as, UInt32 addr);
UInt32 M68K_Read32(M68KAddressSpace* as, UInt32 addr);
void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value);
void M68K_Write16(M68KAddressSpace* as, UInt32 addr, UInt16 value);
void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value);
UInt16 M68K_Fetch16(M68KAddressSpace* as);
UInt32 M68K_Fetch32(M68KAddressSpace* as);
UInt32 M68K_EA_ComputeAddress(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size);
UInt32 M68K_EA_Read(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size);
void M68K_EA_Write(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size, UInt32 value);

/*
 * One-time logging flags
 */
static Boolean g_pcRelLogged = false;

/*
 * Logging helper - logs once per boot
 */
static void M68K_LogOnce(Boolean* flag, const char* message)
{
    if (!*flag) {
        /* Prefix for M68K disassembly output if needed */
        serial_puts(message);
        serial_puts("\n");
        *flag = true;
    }
}

/*
 * Fetch16 - Fetch next 16-bit word from PC (big-endian)
 */
UInt16 M68K_Fetch16(M68KAddressSpace* as)
{
    UInt16 value;
    UInt8 b0, b1;

    as->regs.pc &= M68K_MAX_ADDR - 1;       /* 24-bit, as everywhere */

    b0 = M68K_Read8(as, as->regs.pc);
    b1 = M68K_Read8(as, as->regs.pc + 1);
    value = (b0 << 8) | b1;

    as->regs.pc += 2;
    return value;
}

/*
 * Fetch32 - Fetch next 32-bit long from PC (big-endian)
 */
UInt32 M68K_Fetch32(M68KAddressSpace* as)
{
    UInt32 hi = M68K_Fetch16(as);
    UInt32 lo = M68K_Fetch16(as);
    return (hi << 16) | lo;
}

/* Forward declaration from M68KBackend.c */
extern void* M68K_GetPage(M68KAddressSpace* as, UInt32 addr, Boolean allocate);

/*
 * Read8 - Read byte from address space (paged)
 */
UInt8 M68K_Read8(M68KAddressSpace* as, UInt32 addr)
{
    void* page;
    UInt32 offset;

    page = M68K_GetPage(as, addr, false);  /* Don't allocate on read */
    if (!page) {
        /* "unmapped" marks it a bus error; the address says whose */
        static char why[48];
        snprintf(why, sizeof(why), "read of unmapped address $%06X", (unsigned)(addr & 0xFFFFFF));
        M68K_Fault(as, why);
        return 0;
    }

    offset = addr & (M68K_PAGE_SIZE - 1);
    return ((UInt8*)page)[offset];
}

/*
 * Read16 - Read word from address space (big-endian, paged)
 */
UInt16 M68K_Read16(M68KAddressSpace* as, UInt32 addr)
{
    UInt8 b0, b1;

    /* Check word alignment */
    if (addr & 1) {
        M68K_LOG_ERROR("ADDRESS ERROR: Read16 PC=0x%08X EA=0x%08X (odd address)\\n", as->regs.pc, addr);
        M68K_Fault(as, "Address error: Read16 odd address");
        return 0;
    }

    b0 = M68K_Read8(as, addr);
    b1 = M68K_Read8(as, addr + 1);
    return (b0 << 8) | b1;
}

/*
 * Read32 - Read long from address space (big-endian)
 */
UInt32 M68K_Read32(M68KAddressSpace* as, UInt32 addr)
{
    UInt32 hi, lo;

    /* Check word alignment */
    if (addr & 1) {
        M68K_LOG_ERROR("ADDRESS ERROR: Read32 PC=0x%08X EA=0x%08X (odd address)\\n", as->regs.pc, addr);
        M68K_Fault(as, "Address error: Read32 odd address");
        return 0;
    }

    hi = M68K_Read16(as, addr);
    lo = M68K_Read16(as, addr + 2);
    return (hi << 16) | lo;
}

/*
 * Write8 - Write byte to address space (paged, lazy allocation)
 */
void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value)
{
    void* page;
    UInt32 offset;

    page = M68K_GetPage(as, addr, true);  /* Allocate on write (lazy) */
    if (!page) {
        M68K_Fault(as, "Write8 page allocation failed");
        return;
    }

    offset = addr & (M68K_PAGE_SIZE - 1);
    ((UInt8*)page)[offset] = value;
}

/*
 * Write16 - Write word to address space (big-endian, paged)
 */
void M68K_Write16(M68KAddressSpace* as, UInt32 addr, UInt16 value)
{
    /* Check word alignment */
    if (addr & 1) {
        M68K_LOG_ERROR("ADDRESS ERROR: Write16 PC=0x%08X EA=0x%08X (odd address)\\n", as->regs.pc, addr);
        M68K_Fault(as, "Address error: Write16 odd address");
        return;
    }

    M68K_Write8(as, addr, (value >> 8) & 0xFF);
    M68K_Write8(as, addr + 1, value & 0xFF);
}

/*
 * Write32 - Write long to address space (big-endian)
 */
void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value)
{
    /* Check word alignment */
    if (addr & 1) {
        M68K_LOG_ERROR("ADDRESS ERROR: Write32 PC=0x%08X EA=0x%08X (odd address)\\n", as->regs.pc, addr);
        M68K_Fault(as, "Address error: Write32 odd address");
        return;
    }

    M68K_Write16(as, addr, value >> 16);
    M68K_Write16(as, addr + 2, value & 0xFFFF);
}

/*
 * EA_ComputeAddress - Compute effective address without reading
 * Returns address; for register direct modes, returns register number
 */
UInt32 M68K_EA_ComputeAddress(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size)
{
    UInt32 addr;
    UInt16 ext;
    SInt16 disp;
    UInt8 xn_reg, xn_type;
    SInt32 index;

    switch (mode) {
        case MODE_Dn:
            /* Dn - return register number */
            return reg;

        case MODE_An:
            /* An - return register number */
            return reg;

        case MODE_An_IND:
            /* (An) */
            return as->regs.a[reg];

        case MODE_An_POST:
            /* (An)+ - return current An, will increment after */
            return as->regs.a[reg];

        case MODE_An_PRE:
            /* -(An) - decrement An first, then return */
            if (size == SIZE_BYTE && reg == 7) {
                as->regs.a[reg] -= 2;  /* A7 byte operations use word size */
            } else {
                as->regs.a[reg] -= SIZE_BYTES(size);
            }
            return as->regs.a[reg];

        case MODE_An_DISP:
            /* d16(An) */
            disp = (SInt16)M68K_Fetch16(as);
            return as->regs.a[reg] + disp;

        case MODE_An_INDEX:
            /* d8(An,Xn) */
            ext = M68K_Fetch16(as);
            disp = SIGN_EXTEND_BYTE(ext & 0xFF);
            xn_reg = (ext >> 12) & 0xF;
            xn_type = (ext >> 15) & 1;  /* 0=Dn, 1=An */

            if (xn_type) {
                index = as->regs.a[xn_reg & 7];
            } else {
                index = as->regs.d[xn_reg & 7];
            }

            /* Check index size (bit 11: 0=sign-extend word, 1=long) */
            if (!(ext & 0x800)) {
                index = SIGN_EXTEND_WORD(index & 0xFFFF);
            }

            return as->regs.a[reg] + disp + index;

        case MODE_OTHER:
            switch (reg) {
                case OTHER_ABS_W:
                    /* abs.W */
                    return SIGN_EXTEND_WORD(M68K_Fetch16(as));

                case OTHER_ABS_L:
                    /* abs.L */
                    return M68K_Fetch32(as);

                case OTHER_PC_DISP:
                    /* d16(PC) - PC at start of extension word */
                    M68K_LogOnce(&g_pcRelLogged, "PC-rel enabled: (d16,PC) & (d8,PC,Xn)");
                    addr = as->regs.pc;
                    disp = (SInt16)M68K_Fetch16(as);
                    return addr + disp;

                case OTHER_PC_INDEX:
                    /* d8(PC,Xn) */
                    M68K_LogOnce(&g_pcRelLogged, "PC-rel enabled: (d16,PC) & (d8,PC,Xn)");
                    addr = as->regs.pc;
                    ext = M68K_Fetch16(as);
                    disp = SIGN_EXTEND_BYTE(ext & 0xFF);
                    xn_reg = (ext >> 12) & 0xF;
                    xn_type = (ext >> 15) & 1;

                    if (xn_type) {
                        index = as->regs.a[xn_reg & 7];
                    } else {
                        index = as->regs.d[xn_reg & 7];
                    }

                    if (!(ext & 0x800)) {
                        index = SIGN_EXTEND_WORD(index & 0xFFFF);
                    }

                    return addr + disp + index;

                case OTHER_IMMEDIATE:
                    /* #<data> - return PC, caller fetches immediate */
                    return as->regs.pc;

                default:
                    M68K_Fault(as, "Invalid OTHER mode in EA");
                    return 0;
            }

        default:
            M68K_Fault(as, "Invalid addressing mode in EA");
            return 0;
    }
}

/*
 * EA_Read - Read value from effective address
 */
UInt32 M68K_EA_Read(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size)
{
    UInt32 addr;
    UInt32 value;

    /* Handle register direct modes specially */
    if (mode == MODE_Dn) {
        /* Dn - read from data register */
        value = as->regs.d[reg];
        return value & SIZE_MASK(size);
    }

    if (mode == MODE_An) {
        /* An - read from address register (always 32-bit) */
        return as->regs.a[reg];
    }

    /* Handle immediate mode */
    if (mode == MODE_OTHER && reg == OTHER_IMMEDIATE) {
        if (size == SIZE_BYTE || size == SIZE_WORD) {
            return M68K_Fetch16(as) & SIZE_MASK(size);
        } else {
            return M68K_Fetch32(as);
        }
    }

    /* Compute address and read from memory */
    addr = M68K_EA_ComputeAddress(as, mode, reg, size);

    switch (size) {
        case SIZE_BYTE:
            value = M68K_Read8(as, addr);
            break;
        case SIZE_WORD:
            value = M68K_Read16(as, addr);
            break;
        case SIZE_LONG:
            value = M68K_Read32(as, addr);
            break;
        default:
            M68K_Fault(as, "Invalid size in EA_Read");
            return 0;
    }

    /* Handle postincrement */
    if (mode == MODE_An_POST) {
        if (size == SIZE_BYTE && reg == 7) {
            as->regs.a[reg] += 2;  /* A7 byte operations use word size */
        } else {
            as->regs.a[reg] += SIZE_BYTES(size);
        }
    }

    return value;
}

/*
 * EA_Write - Write value to effective address
 */
void M68K_EA_Write(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size, UInt32 value)
{
    UInt32 addr;

    /* Handle register direct modes specially */
    if (mode == MODE_Dn) {
        /* Dn - write to data register with proper masking */
        switch (size) {
            case SIZE_BYTE:
                as->regs.d[reg] = (as->regs.d[reg] & 0xFFFFFF00) | (value & 0xFF);
                break;
            case SIZE_WORD:
                as->regs.d[reg] = (as->regs.d[reg] & 0xFFFF0000) | (value & 0xFFFF);
                break;
            case SIZE_LONG:
                as->regs.d[reg] = value;
                break;
        }
        return;
    }

    if (mode == MODE_An) {
        /* An - always 32-bit write */
        as->regs.a[reg] = value;
        return;
    }

    /* Compute address and write to memory */
    addr = M68K_EA_ComputeAddress(as, mode, reg, size);

    switch (size) {
        case SIZE_BYTE:
            M68K_Write8(as, addr, value & 0xFF);
            break;
        case SIZE_WORD:
            M68K_Write16(as, addr, value & 0xFFFF);
            break;
        case SIZE_LONG:
            M68K_Write32(as, addr, value);
            break;
        default:
            M68K_Fault(as, "Invalid size in EA_Write");
            return;
    }

    /* Handle postincrement */
    if (mode == MODE_An_POST) {
        if (size == SIZE_BYTE && reg == 7) {
            as->regs.a[reg] += 2;  /* A7 byte operations use word size */
        } else {
            as->regs.a[reg] += SIZE_BYTES(size);
        }
    }
}
