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
UInt32 M68K_EA_ReadRMW(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size);
void M68K_EA_WriteRMW(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size, UInt32 value);

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

    /* Instructions are words, on every 68K: a jump to an odd address is an
     * address error even on the 68020, which lets data be anywhere */
    if (as->regs.pc & 1) {
        M68K_Fault(as, "Address error: instruction fetch from an odd address");
        return 0x4E71;                      /* a NOP: the fault has stopped it */
    }

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

    /* Any address: the 68020 reads and writes words and longs at odd
     * addresses, and programs written for it count on that */

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

    /* Any address: the 68020 reads and writes words and longs at odd
     * addresses, and programs written for it count on that */

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
    /* Any address: the 68020 reads and writes words and longs at odd
     * addresses, and programs written for it count on that */

    M68K_Write8(as, addr, (value >> 8) & 0xFF);
    M68K_Write8(as, addr + 1, value & 0xFF);
}

/*
 * Write32 - Write long to address space (big-endian)
 */
void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value)
{
    /* Any address: the 68020 reads and writes words and longs at odd
     * addresses, and programs written for it count on that */

    M68K_Write16(as, addr, value >> 16);
    M68K_Write16(as, addr + 2, value & 0xFFFF);
}

/*
 * Indexed addressing: (d8,An,Xn) and (d8,PC,Xn), and everything the 68020
 * put behind the same two modes.
 *
 * The extension word's bit 8 says which form. Clear, it is the brief form:
 * an 8-bit displacement and an index register, scaled by 1, 2, 4 or 8
 * (bits 10-9; the 68000 ignored them, and compilers for it leave them 0).
 * Set, it is the full form (MC68020 User's Manual 2.2): the base register
 * and the index each suppressible, a 16- or 32-bit base displacement, and
 * optionally a long read through memory - before the index is added
 * (preindexed) or after (postindexed) - with an outer displacement.
 *
 * 'base' is An, or the address of the extension word for the PC forms.
 * The brief form's scale bits were ignored here, and a full extension word
 * was read as a brief one: 68020 code ran on with addresses that were not
 * the ones it meant.
 */
static UInt32 M68K_IndexedAddress(M68KAddressSpace* as, UInt32 base)
{
    UInt16 ext = M68K_Fetch16(as);
    UInt8 xn = (ext >> 12) & 0xF;
    SInt32 index = (xn & 8) ? (SInt32)as->regs.a[xn & 7] : (SInt32)as->regs.d[xn & 7];
    if (!(ext & 0x0800)) index = SIGN_EXTEND_WORD(index & 0xFFFF);
    index *= 1 << ((ext >> 9) & 3);

    if (!(ext & 0x0100)) {
        return base + SIGN_EXTEND_BYTE(ext & 0xFF) + index;
    }

    if (ext & 0x0080) base = 0;                     /* BS: base suppressed */
    if (ext & 0x0040) index = 0;                    /* IS: index suppressed */
    SInt32 bd = 0;
    switch ((ext >> 4) & 3) {
        case 2: bd = SIGN_EXTEND_WORD(M68K_Fetch16(as)); break;
        case 3: bd = (SInt32)M68K_Fetch32(as); break;
        case 1: break;                              /* null displacement */
        default:
            M68K_Fault(as, "reserved base displacement size in extension word");
            return 0;
    }
    UInt8 iis = ext & 7;
    if (iis == 0) return base + bd + index;         /* no memory indirection */
    if ((ext & 0x0040) && iis > 3) {
        M68K_Fault(as, "reserved indirection in extension word");
        return 0;
    }
    if (iis == 4) {
        M68K_Fault(as, "reserved indirection in extension word");
        return 0;
    }
    SInt32 od = 0;
    switch (iis & 3) {
        case 2: od = SIGN_EXTEND_WORD(M68K_Fetch16(as)); break;
        case 3: od = (SInt32)M68K_Fetch32(as); break;
        default: break;                             /* null outer displacement */
    }
    if (iis & 4) {                                  /* postindexed: ([bd,base],Xn,od) */
        return M68K_Read32(as, base + bd) + index + od;
    }
    return M68K_Read32(as, base + bd + index) + od; /* preindexed: ([bd,base,Xn],od) */
}

/*
 * EA_ComputeAddress - Compute effective address without reading
 * Returns address; for register direct modes, returns register number
 */
UInt32 M68K_EA_ComputeAddress(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size)
{
    UInt32 addr;
    SInt16 disp;

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
            /* (d8,An,Xn), and the 68020's forms */
            return M68K_IndexedAddress(as, as->regs.a[reg]);

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
                    /* (d8,PC,Xn), and the 68020's forms: PC is the
                     * extension word's address */
                    M68K_LogOnce(&g_pcRelLogged, "PC-rel enabled: (d16,PC) & (d8,PC,Xn)");
                    return M68K_IndexedAddress(as, as->regs.pc);

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

/*
 * Read-modify-write operands: ADDQ.W #4,58(SP), NOT.L -(A0), BSET #3,(A2)+.
 *
 * The operand's address is worked out once, by the read, and the write goes
 * back to it. Reading and then writing through the ordinary calls worked it
 * out twice - which fetched a displacement word twice, so the write landed
 * at the base plus the next instruction's first word, and decremented or
 * incremented an address register twice.
 */
UInt32 M68K_EA_ReadRMW(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size)
{
    if (mode == MODE_Dn || mode == MODE_An || (mode == MODE_OTHER && reg == OTHER_IMMEDIATE)) {
        as->rmwValid = false;
        return M68K_EA_Read(as, mode, reg, size);
    }
    UInt32 addr = M68K_EA_ComputeAddress(as, mode, reg, size);
    if (mode == MODE_An_POST) {
        as->regs.a[reg] += (size == SIZE_BYTE && reg == 7) ? 2 : SIZE_BYTES(size);
    }
    as->rmwAddr = addr;
    as->rmwValid = true;
    switch (size) {
        case SIZE_BYTE: return M68K_Read8(as, addr);
        case SIZE_WORD: return M68K_Read16(as, addr);
        default:        return M68K_Read32(as, addr);
    }
}

void M68K_EA_WriteRMW(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size, UInt32 value)
{
    if (!as->rmwValid) {
        M68K_EA_Write(as, mode, reg, size, value);
        return;
    }
    as->rmwValid = false;
    switch (size) {
        case SIZE_BYTE: M68K_Write8(as, as->rmwAddr, (UInt8)value); break;
        case SIZE_WORD: M68K_Write16(as, as->rmwAddr, (UInt16)value); break;
        default:        M68K_Write32(as, as->rmwAddr, value); break;
    }
}
