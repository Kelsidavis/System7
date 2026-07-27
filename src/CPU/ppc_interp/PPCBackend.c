/*
 * PPCBackend.c - PowerPC Interpreter CPU Backend Implementation
 *
 * Implements ICPUBackend interface for PowerPC code execution via interpretation.
 * Runs on any host ISA (x86, ARM, Raspberry Pi, etc.) by interpreting PowerPC instructions.
 *
 * PLATFORM SUPPORT:
 * - x86 (IA32): Fully supported
 * - ARM (ARMv6, ARMv7, ARMv8): Fully supported, enables PowerPC compatibility on Raspberry Pi
 * - Other architectures: Should work with no source modifications due to explicit byte ordering
 *
 * CROSS-PLATFORM GUARANTEES:
 * - All PowerPC values are stored in big-endian format (PowerPC byte order)
 * - Memory operations use explicit byte reconstruction, never assume host endianness
 * - Page allocation is generic and works on all architectures
 * - No inline assembly or architecture-specific tricks
 */

#include "CPU/PPCInterp.h"
#include "CPU/PPCOpcodes.h"
#include "CPU/CPUBackend.h"
#include "CPU/LowMemGlobals.h"
#include "SegmentLoader/SegmentLoader.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include <string.h>

/* Forward declarations of ICPUBackend methods */
static OSErr PPC_CreateAddressSpace(void* processHandle, CPUAddressSpace* out);
static OSErr PPC_DestroyAddressSpace(CPUAddressSpace as);
static OSErr PPC_MapExecutable(CPUAddressSpace as, const void* image, Size len,
                               CPUMapFlags flags, CPUCodeHandle* outHandle,
                               CPUAddr* outBase);
static OSErr PPC_UnmapExecutable(CPUAddressSpace as, CPUCodeHandle handle);
static OSErr PPC_SetRegisterA5(CPUAddressSpace as, CPUAddr a5);
static OSErr PPC_SetStacks(CPUAddressSpace as, CPUAddr usp, CPUAddr ssp);
static OSErr PPC_InstallTrap(CPUAddressSpace as, TrapNumber trapNum,
                             CPUTrapHandler handler, void* context);
static OSErr PPC_WriteJumpTableSlot(CPUAddressSpace as, CPUAddr slotAddr,
                                    SInt16 segID, CPUAddr target);
static OSErr PPC_MakeLazyJTStub(CPUAddressSpace as, CPUAddr slotAddr,
                                SInt16 segID, UInt16 routineOffset);
static OSErr PPC_EnterAt(CPUAddressSpace as, CPUAddr entry, CPUEnterFlags flags);
static OSErr PPC_Relocate(CPUAddressSpace as, CPUCodeHandle code,
                          const RelocTable* relocs, CPUAddr segBase,
                          CPUAddr jtBase, CPUAddr a5Base);
static OSErr PPC_AllocateMemory(CPUAddressSpace as, Size size,
                                CPUMapFlags flags, CPUAddr* outAddr);
static OSErr PPC_WriteMemory(CPUAddressSpace as, CPUAddr addr,
                             const void* data, Size len);
static OSErr PPC_ReadMemory(CPUAddressSpace as, CPUAddr addr,
                            void* data, Size len);

/*
 * Global PowerPC Backend Instance
 */
const ICPUBackend gPPCInterpreterBackend = {
    .CreateAddressSpace = PPC_CreateAddressSpace,
    .DestroyAddressSpace = PPC_DestroyAddressSpace,
    .MapExecutable = PPC_MapExecutable,
    .UnmapExecutable = PPC_UnmapExecutable,
    .SetRegisterA5 = PPC_SetRegisterA5,
    .SetStacks = PPC_SetStacks,
    .InstallTrap = PPC_InstallTrap,
    .WriteJumpTableSlot = PPC_WriteJumpTableSlot,
    .MakeLazyJTStub = PPC_MakeLazyJTStub,
    .EnterAt = PPC_EnterAt,
    .Relocate = PPC_Relocate,
    .AllocateMemory = PPC_AllocateMemory,
    .WriteMemory = PPC_WriteMemory,
    .ReadMemory = PPC_ReadMemory
};

/*
 * PowerPC Backend Initialization
 */
OSErr PPCBackend_Initialize(void)
{
    return CPUBackend_Register("ppc_interp", &gPPCInterpreterBackend);
}

/*
 * CreateAddressSpace - Allocate PowerPC address space
 */
static OSErr PPC_CreateAddressSpace(void* processHandle, CPUAddressSpace* out)
{
    PPCAddressSpace* as;

    (void)processHandle; /* Unused for now */

    serial_printf("[PPC] CreateAddressSpace: allocating PPCAddressSpace struct size=%u\n",
                  (unsigned)sizeof(PPCAddressSpace));
    as = (PPCAddressSpace*)NewPtr(sizeof(PPCAddressSpace));
    if (!as) {
        serial_printf("[PPC] FAIL: struct allocation memFullErr, MemError=%d\n", MemError());
        return memFullErr;
    }

    memset(as, 0, sizeof(PPCAddressSpace));
    as->baseAddr = 0;

    /* Initialize page table (all NULL = not allocated) */
    memset(as->pageTable, 0, sizeof(as->pageTable));

    serial_printf("[PPC] CreateAddressSpace: sparse 16MB virtual space ready\n");

    /* Initialize registers */
    memset(&as->regs, 0, sizeof(PPCRegs));
    as->regs.msr = 0x0000; /* User mode initially */

    /* Initialize time base and processor version */
    as->regs.tbl = 0;
    as->regs.tbu = 0;
    as->regs.dec = 0;
    as->regs.pvr = 0x00080200; /* PowerPC 603 processor version */

    *out = (CPUAddressSpace)as;
    return noErr;
}

/*
 * DestroyAddressSpace - Free address space
 */
static OSErr PPC_DestroyAddressSpace(CPUAddressSpace as)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;

    if (!pas) {
        return paramErr;
    }

    /* Free all allocated pages */
    for (int i = 0; i < PPC_NUM_PAGES; i++) {
        if (pas->pageTable[i]) {
            if (!MemoryManager_IsHeapPointer(pas->pageTable[i])) {
                DisposePtr((Ptr)pas->pageTable[i]);
            }
            pas->pageTable[i] = NULL;
        }
    }

    DisposePtr((Ptr)pas);
    return noErr;
}

/* Forward declaration */
void* PPC_GetPage(PPCAddressSpace* as, UInt32 addr, Boolean allocate);

/*
 * PPC_MemCopy - Copy data to paged memory (lazy page allocation)
 */
static OSErr PPC_MemCopy(PPCAddressSpace* as, UInt32 addr, const void* src, Size len)
{
    const UInt8* srcBytes = (const UInt8*)src;

    for (Size i = 0; i < len; i++) {
        void* page = PPC_GetPage(as, addr + i, true);
        if (!page) {
            return memFullErr;
        }
        UInt32 offset = (addr + i) & (PPC_PAGE_SIZE - 1);
        ((UInt8*)page)[offset] = srcBytes[i];
    }
    return noErr;
}

/*
 * PPC_GetPage - Get page for address, allocating if needed (lazy allocation)
 * Returns NULL if address out of range or allocation fails
 */
void* PPC_GetPage(PPCAddressSpace* as, UInt32 addr, Boolean allocate)
{
    UInt32 pageNum;
    void* page;

    /* Check address range */
    if (addr >= PPC_MAX_ADDR) {
        return NULL;
    }

    pageNum = addr >> PPC_PAGE_SHIFT;
    page = as->pageTable[pageNum];

    /* If page not allocated and allocation requested, allocate now */
    if (!page && allocate) {
        page = NewPtr(PPC_PAGE_SIZE);
        if (page) {
            memset(page, 0, PPC_PAGE_SIZE);
            as->pageTable[pageNum] = page;
            serial_printf("[PPC] Allocated page %u for addr 0x%08X\n", pageNum, addr);
        } else {
            serial_printf("[PPC] FAIL: page %u allocation failed, MemError=%d\n",
                         pageNum, MemError());
        }
    }

    return page;
}

/*
 * MapExecutable - Map code into address space
 */
static OSErr PPC_MapExecutable(CPUAddressSpace as, const void* image, Size len,
                               CPUMapFlags flags, CPUCodeHandle* outHandle,
                               CPUAddr* outBase)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;
    PPCCodeHandle* handle;
    UInt32 addr;

    if (!pas || !image || !outHandle || !outBase) {
        return paramErr;
    }

    /* Allocate code handle */
    handle = (PPCCodeHandle*)NewPtr(sizeof(PPCCodeHandle));
    if (!handle) {
        return memFullErr;
    }

    /* Find free address space (simple bump allocator) */
    addr = 0x1000; /* Start at 4K to avoid null pointers */
    for (int i = 0; i < pas->numCodeSegs; i++) {
        UInt32 end = pas->codeSegBases[i] + pas->codeSegSizes[i];
        if (end > addr) {
            addr = end;
        }
    }

    /* Align to 16-byte boundary */
    addr = (addr + 15) & ~15;

    /* Check bounds */
    if (addr + len > PPC_MAX_ADDR) {
        DisposePtr((Ptr)handle);
        return memFullErr;
    }

    /* Copy code into address space (allocates pages as needed) */
    if (PPC_MemCopy(pas, addr, image, len) != noErr) {
        DisposePtr((Ptr)handle);
        return memFullErr;
    }

    /* Track segment */
    if (pas->numCodeSegs < 256) {
        void* firstPage = PPC_GetPage(pas, addr, false);  /* Already allocated */
        pas->codeSegments[pas->numCodeSegs] = firstPage ? (UInt8*)firstPage + (addr & (PPC_PAGE_SIZE - 1)) : NULL;
        pas->codeSegBases[pas->numCodeSegs] = addr;
        pas->codeSegSizes[pas->numCodeSegs] = len;
        handle->segIndex = pas->numCodeSegs;
        pas->numCodeSegs++;
    } else {
        DisposePtr((Ptr)handle);
        return memFullErr;
    }

    handle->hostMemory = PPC_GetPage(pas, addr, false);
    if (handle->hostMemory) {
        handle->hostMemory = (UInt8*)handle->hostMemory + (addr & (PPC_PAGE_SIZE - 1));
    }
    handle->cpuAddr = addr;
    handle->size = len;

    *outHandle = (CPUCodeHandle)handle;
    *outBase = addr;

    (void)flags; /* Unused for now */

    return noErr;
}

/*
 * UnmapExecutable - Unmap code segment
 */
static OSErr PPC_UnmapExecutable(CPUAddressSpace as, CPUCodeHandle handle)
{
    PPCCodeHandle* phandle = (PPCCodeHandle*)handle;

    (void)as; /* Unused */

    if (!phandle) {
        return paramErr;
    }

    /* For now, just free the handle (memory stays allocated) */
    DisposePtr((Ptr)phandle);
    return noErr;
}

/*
 * SetRegisterA5 - Set r13 (SDA base, closest equivalent to 68K A5)
 * PowerPC uses r13 for Small Data Area, similar to 68K's A5 globals
 */
static OSErr PPC_SetRegisterA5(CPUAddressSpace as, CPUAddr a5)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;

    if (!pas) {
        return paramErr;
    }

    pas->regs.gpr[13] = a5; /* r13 = SDA base */
    return noErr;
}

/*
 * SetStacks - Configure stack pointer
 * PowerPC uses r1 as stack pointer
 */
static OSErr PPC_SetStacks(CPUAddressSpace as, CPUAddr usp, CPUAddr ssp)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;

    if (!pas) {
        return paramErr;
    }

    /* PowerPC user mode only has one stack, use usp */
    pas->regs.gpr[1] = usp; /* r1 = stack pointer */

    (void)ssp; /* PowerPC doesn't have separate supervisor stack in user mode */

    return noErr;
}

/*
 * InstallTrap - Install trap handler
 */
static OSErr PPC_InstallTrap(CPUAddressSpace as, TrapNumber trapNum,
                             CPUTrapHandler handler, void* context)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;

    trapNum &= 0x00FF;

    if (!pas) {
        return paramErr;
    }

    pas->trapHandlers[trapNum] = handler;
    pas->trapContexts[trapNum] = context;

    return noErr;
}

/*
 * WriteJumpTableSlot - Patch jump table entry
 */
static OSErr PPC_WriteJumpTableSlot(CPUAddressSpace as, CPUAddr slotAddr,
                                    SInt16 segID, CPUAddr target)
{
    (void)segID; /* PowerPC entries have no room to keep it; 68K ones do */

    PPCAddressSpace* pas = (PPCAddressSpace*)as;
    extern void PPC_Write32(PPCAddressSpace* as, UInt32 addr, UInt32 value);

    if (!pas || slotAddr >= PPC_MAX_ADDR) {
        return paramErr;
    }

    /*
     * Write PowerPC jump sequence:
     *   +0: 0x3D600000 | (target >> 16)    ; lis r11, target@h
     *   +4: 0x616B0000 | (target & 0xFFFF) ; ori r11, r11, target@l
     *   +8: 0x7D6903A6                     ; mtctr r11
     *  +12: 0x4E800420                     ; bctr
     */
    UInt32 lis_insn = 0x3D600000 | ((target >> 16) & 0xFFFF);
    UInt32 ori_insn = 0x616B0000 | (target & 0xFFFF);
    UInt32 mtctr_insn = 0x7D6903A6;
    UInt32 bctr_insn = 0x4E800420;

    PPC_Write32(pas, slotAddr + 0, lis_insn);
    PPC_Write32(pas, slotAddr + 4, ori_insn);
    PPC_Write32(pas, slotAddr + 8, mtctr_insn);
    PPC_Write32(pas, slotAddr + 12, bctr_insn);

    return noErr;
}

/*
 * MakeLazyJTStub - Create lazy-loading stub
 */
static OSErr PPC_MakeLazyJTStub(CPUAddressSpace as, CPUAddr slotAddr,
                                SInt16 segID, UInt16 routineOffset)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;
    extern void PPC_Write32(PPCAddressSpace* as, UInt32 addr, UInt32 value);

    if (!pas || slotAddr >= PPC_MAX_ADDR) {
        return paramErr;
    }

    /*
     * Create lazy stub that triggers _LoadSeg via system call:
     *
     *   +0: 0x38600000 | segID    ; li r3, segID
     *   +4: 0x44000002             ; sc (system call)
     *   +8: 0x4E800020             ; blr (return)
     */
    UInt32 li_insn = 0x38600000 | (segID & 0xFFFF);
    UInt32 sc_insn = 0x44000002;
    UInt32 blr_insn = 0x4E800020;

    PPC_Write32(pas, slotAddr + 0, li_insn);
    PPC_Write32(pas, slotAddr + 4, sc_insn);
    PPC_Write32(pas, slotAddr + 8, blr_insn);

    (void)routineOffset; /* PowerPC segments are entered at their base */

    return noErr;
}

/*
 * EnterAt - Begin execution
 */
static OSErr PPC_EnterAt(CPUAddressSpace as, CPUAddr entry, CPUEnterFlags flags)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;
    UInt32 max_instructions = 100000;  /* Safety limit */

    if (!pas) {
        return paramErr;
    }

    serial_printf("[PPC] EnterAt: entry=0x%08X flags=0x%04X\n", entry, flags);

    /* Clear halted flag */
    pas->halted = false;

    /* Execute from entry point */
    PPC_Execute(pas, entry, max_instructions);

    if (pas->halted) {
        serial_printf("[PPC] Execution halted at PC=0x%08X\n", pas->regs.pc);
    } else {
        serial_printf("[PPC] Execution completed after %u instructions\n", max_instructions);
    }

    (void)flags; /* Unused for now */

    return noErr;
}

/*
 * Relocate - Apply relocations
 */
static OSErr PPC_Relocate(CPUAddressSpace as, CPUCodeHandle code,
                          const RelocTable* relocs, CPUAddr segBase,
                          CPUAddr jtBase, CPUAddr a5Base)
{
    PPCCodeHandle* phandle = (PPCCodeHandle*)code;
    PPCAddressSpace* pas = (PPCAddressSpace*)as;
    UInt8* codeData;
    const char* kindName;

    if (!pas || !phandle || !relocs) {
        return paramErr;
    }

    codeData = (UInt8*)phandle->hostMemory;
    if (!codeData) {
        serial_printf("[RELOC] ERROR: hostMemory is NULL\n");
        return paramErr;
    }

    serial_printf("[RELOC] Applying %d relocations to PowerPC segment at 0x%08X\n",
                  relocs->count, segBase);

    /* Apply each relocation */
    for (UInt16 i = 0; i < relocs->count; i++) {
        const RelocEntry* reloc = &relocs->entries[i];
        UInt32 offset = reloc->atOffset;
        UInt32 value = 0;
        SInt32 pcrel_offset;
        UInt32 patch_pc;

        if (offset + 4 > phandle->size) {
            serial_printf("[RELOC] ERROR: offset 0x%X exceeds segment size 0x%X\n",
                         offset, phandle->size);
            return segmentRelocErr;
        }

        switch (reloc->kind) {
            case kRelocAbsSegBase:
                /* Patch absolute address with segment base */
                kindName = "ABS_SEG_BASE";
                value = segBase + reloc->addend;
                codeData[offset + 0] = (value >> 24) & 0xFF;
                codeData[offset + 1] = (value >> 16) & 0xFF;
                codeData[offset + 2] = (value >> 8) & 0xFF;
                codeData[offset + 3] = value & 0xFF;
                serial_printf("[RELOC] apply kind=%s at off=0x%X -> val=0x%08X (base=0x%08X addend=%d)\n",
                             kindName, offset, value, segBase, reloc->addend);
                break;

            case kRelocA5Relative:
                /* Patch A5-relative offset (r13 SDA base on PowerPC) */
                kindName = "A5_REL";
                value = a5Base + reloc->addend;
                codeData[offset + 0] = (value >> 24) & 0xFF;
                codeData[offset + 1] = (value >> 16) & 0xFF;
                codeData[offset + 2] = (value >> 8) & 0xFF;
                codeData[offset + 3] = value & 0xFF;
                serial_printf("[RELOC] apply kind=%s at off=0x%X -> val=0x%08X (A5=0x%08X addend=%d)\n",
                             kindName, offset, value, a5Base, reloc->addend);
                break;

            case kRelocJTImport:
                /* Patch jump table import */
                kindName = "JT_IMPORT";
                /* Check for overflow: jtIndex * 16 + jtBase */
                {
                    UInt32 jtOffset = (UInt32)reloc->jtIndex * 16;
                    if (jtOffset > UINT32_MAX - jtBase) {
                        serial_printf("[RELOC] ERROR: JT index overflow at entry %d\n", i);
                        return segmentRelocErr;
                    }
                    value = jtBase + jtOffset;
                }
                codeData[offset + 0] = (value >> 24) & 0xFF;
                codeData[offset + 1] = (value >> 16) & 0xFF;
                codeData[offset + 2] = (value >> 8) & 0xFF;
                codeData[offset + 3] = value & 0xFF;
                serial_printf("[RELOC] apply kind=%s at off=0x%X -> val=0x%08X (JT[%d])\n",
                             kindName, offset, value, reloc->jtIndex);
                break;

            case kRelocPCRel16:
                /* PC-relative 16-bit branch (PowerPC B instruction with LI field) */
                kindName = "PC_REL16";
                /* PC points to current instruction */
                patch_pc = segBase + offset;
                /* Calculate target address */
                value = segBase + reloc->addend;
                /* Calculate PC-relative offset */
                pcrel_offset = (SInt32)value - (SInt32)patch_pc;
                /* Check alignment (must be multiple of 4) */
                if (pcrel_offset & 3) {
                    serial_printf("[RELOC] ERROR: PC_REL16 not 4-byte aligned: offset=%d\n", pcrel_offset);
                    return segmentRelocErr;
                }
                /* Check 16-bit signed range (divided by 4 for instruction encoding) */
                if (pcrel_offset < -32768 || pcrel_offset > 32767) {
                    serial_printf("[RELOC] ERROR: PC_REL16 out of range: offset=%d\n", pcrel_offset);
                    return segmentRelocErr;
                }
                /* Patch as big-endian 16-bit in lower halfword of instruction */
                codeData[offset + 2] = (pcrel_offset >> 8) & 0xFF;
                codeData[offset + 3] = pcrel_offset & 0xFF;
                serial_printf("[RELOC] apply kind=%s at off=0x%X -> disp=%+d (target=0x%08X PC=0x%08X)\n",
                             kindName, offset, pcrel_offset, value, patch_pc);
                break;

            case kRelocPCRel32:
                /* PC-relative 32-bit (PowerPC branch with 24-bit LI field) */
                kindName = "PC_REL32";
                patch_pc = segBase + offset;
                value = segBase + reloc->addend;
                pcrel_offset = (SInt32)value - (SInt32)patch_pc;
                /* Check alignment */
                if (pcrel_offset & 3) {
                    serial_printf("[RELOC] ERROR: PC_REL32 not 4-byte aligned: offset=%d\n", pcrel_offset);
                    return segmentRelocErr;
                }
                /* Patch 24-bit field in instruction (shift right 2 for encoding) */
                codeData[offset + 0] = (codeData[offset + 0] & 0xFC) | ((pcrel_offset >> 24) & 0x03);
                codeData[offset + 1] = (pcrel_offset >> 16) & 0xFF;
                codeData[offset + 2] = (pcrel_offset >> 8) & 0xFF;
                codeData[offset + 3] = pcrel_offset & 0xFC; /* Clear LK and AA bits */
                serial_printf("[RELOC] apply kind=%s at off=0x%X -> disp=%+d (target=0x%08X PC=0x%08X)\n",
                             kindName, offset, pcrel_offset, value, patch_pc);
                break;

            case kRelocSegmentRef:
                /* Reference to another segment */
                kindName = "SEG_REF";
                value = segBase + reloc->addend;
                codeData[offset + 0] = (value >> 24) & 0xFF;
                codeData[offset + 1] = (value >> 16) & 0xFF;
                codeData[offset + 2] = (value >> 8) & 0xFF;
                codeData[offset + 3] = value & 0xFF;
                serial_printf("[RELOC] apply kind=%s at off=0x%X -> val=0x%08X (seg=%d addend=%d)\n",
                             kindName, offset, value, reloc->targetSegment, reloc->addend);
                break;

            default:
                serial_printf("[RELOC] ERROR: Unknown relocation kind %d\n", reloc->kind);
                return segmentRelocErr;
        }
    }

    serial_printf("[RELOC] Successfully applied all %d relocations\n", relocs->count);
    return noErr;
}

/*
 * AllocateMemory - Allocate memory in CPU address space
 */
static OSErr PPC_AllocateMemory(CPUAddressSpace as, Size size,
                                CPUMapFlags flags, CPUAddr* outAddr)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;
    UInt32 addr;
    extern void PPC_Write8(PPCAddressSpace* as, UInt32 addr, UInt8 value);

    if (!pas || !outAddr) {
        return paramErr;
    }

    /* Find free space (simple bump allocator) */
    addr = 0x10000; /* Start at 64K */
    for (int i = 0; i < pas->numCodeSegs; i++) {
        UInt32 end = pas->codeSegBases[i] + pas->codeSegSizes[i];
        if (end > addr) {
            addr = end;
        }
    }

    /* Align to 16-byte boundary */
    addr = (addr + 15) & ~15;

    /* Check bounds */
    if (addr + size > PPC_MAX_ADDR) {
        return memFullErr;
    }

    /* Zero memory */
    for (Size i = 0; i < size; i++) {
        PPC_Write8(pas, addr + i, 0);
    }

    *outAddr = addr;

    (void)flags; /* Unused for now */

    return noErr;
}

/*
 * WriteMemory - Write to CPU address space
 */
static OSErr PPC_WriteMemory(CPUAddressSpace as, CPUAddr addr,
                             const void* data, Size len)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;

    if (!pas || !data || addr + len > PPC_MAX_ADDR) {
        return paramErr;
    }

    return PPC_MemCopy(pas, addr, data, len);
}

/*
 * ReadMemory - Read from CPU address space
 */
static OSErr PPC_ReadMemory(CPUAddressSpace as, CPUAddr addr,
                            void* data, Size len)
{
    PPCAddressSpace* pas = (PPCAddressSpace*)as;
    extern UInt8 PPC_Read8(PPCAddressSpace* as, UInt32 addr);

    if (!pas || !data || addr + len > PPC_MAX_ADDR) {
        return paramErr;
    }

    for (Size i = 0; i < len; i++) {
        ((UInt8*)data)[i] = PPC_Read8(pas, addr + i);
    }

    return noErr;
}

/*
 * Opcode Handler Forward Declarations (from PPCOpcodes.c)
 */
extern UInt32 PPC_Fetch32(PPCAddressSpace* as);

/*
 * PPC_Step - Fetch and execute one instruction
 */
OSErr PPC_Step(PPCAddressSpace* as)
{
    UInt32 insn;
    UInt8 primary;
    UInt16 extended;

    if (!as) {
        return paramErr;
    }

    if (as->halted) {
        return noErr;
    }

    /* Fetch instruction (32-bit) */
    insn = PPC_Fetch32(as);

    /* Extract primary opcode (bits 0-5) */
    primary = PPC_PRIMARY_OPCODE(insn);

    /* Decode and dispatch */
    switch (primary) {
        case PPC_OP_TWI:         /* 3 */
            PPC_Op_TWI(as, insn);
            break;

        case PPC_OP_EXT4:        /* 4 - AltiVec/VMX vector operations */
            extended = (insn & 0x7FF);  /* 11-bit extended opcode for AltiVec */
            switch (extended) {
                case PPC_VXO_VADDUBM:
                    PPC_Op_VADDUBM(as, insn);
                    break;

                case PPC_VXO_VADDUHM:
                    PPC_Op_VADDUHM(as, insn);
                    break;

                case PPC_VXO_VADDUWM:
                    PPC_Op_VADDUWM(as, insn);
                    break;

                case PPC_VXO_VSUBUBM:
                    PPC_Op_VSUBUBM(as, insn);
                    break;

                case PPC_VXO_VSUBUHM:
                    PPC_Op_VSUBUHM(as, insn);
                    break;

                case PPC_VXO_VSUBUWM:
                    PPC_Op_VSUBUWM(as, insn);
                    break;

                case PPC_VXO_VAND:
                    PPC_Op_VAND(as, insn);
                    break;

                case PPC_VXO_VANDC:
                    PPC_Op_VANDC(as, insn);
                    break;

                case PPC_VXO_VOR:
                    PPC_Op_VOR(as, insn);
                    break;

                case PPC_VXO_VXOR:
                    PPC_Op_VXOR(as, insn);
                    break;

                case PPC_VXO_VNOR:
                    PPC_Op_VNOR(as, insn);
                    break;

                case PPC_VXO_VORC:
                    PPC_Op_VORC(as, insn);
                    break;

                case PPC_VXO_VSPLTISB:
                    PPC_Op_VSPLTISB(as, insn);
                    break;

                case PPC_VXO_VSPLTISH:
                    PPC_Op_VSPLTISH(as, insn);
                    break;

                case PPC_VXO_VSPLTISW:
                    PPC_Op_VSPLTISW(as, insn);
                    break;

                /* Saturating arithmetic */
                case PPC_VXO_VADDSBS:
                    PPC_Op_VADDSBS(as, insn);
                    break;

                case PPC_VXO_VADDUBS:
                    PPC_Op_VADDUBS(as, insn);
                    break;

                case PPC_VXO_VADDSHS:
                    PPC_Op_VADDSHS(as, insn);
                    break;

                case PPC_VXO_VADDUHS:
                    PPC_Op_VADDUHS(as, insn);
                    break;

                case PPC_VXO_VSUBSBS:
                    PPC_Op_VSUBSBS(as, insn);
                    break;

                case PPC_VXO_VSUBUBS:
                    PPC_Op_VSUBUBS(as, insn);
                    break;

                case PPC_VXO_VSUBSHS:
                    PPC_Op_VSUBSHS(as, insn);
                    break;

                case PPC_VXO_VSUBUHS:
                    PPC_Op_VSUBUHS(as, insn);
                    break;

                /* Shift */
                case PPC_VXO_VSLB:
                    PPC_Op_VSLB(as, insn);
                    break;

                case PPC_VXO_VSRB:
                    PPC_Op_VSRB(as, insn);
                    break;

                case PPC_VXO_VSRAB:
                    PPC_Op_VSRAB(as, insn);
                    break;

                case PPC_VXO_VSLH:
                    PPC_Op_VSLH(as, insn);
                    break;

                case PPC_VXO_VSRH:
                    PPC_Op_VSRH(as, insn);
                    break;

                case PPC_VXO_VSRAW:
                    PPC_Op_VSRAW(as, insn);
                    break;

                /* Pack/Unpack */
                case PPC_VXO_VPKUHUM:
                    PPC_Op_VPKUHUM(as, insn);
                    break;

                case PPC_VXO_VPKUWUM:
                    PPC_Op_VPKUWUM(as, insn);
                    break;

                case PPC_VXO_VPKPX:
                    PPC_Op_VPKPX(as, insn);
                    break;

                case PPC_VXO_VUPKHSB:
                    PPC_Op_VUPKHSB(as, insn);
                    break;

                case PPC_VXO_VUPKLSB:
                    PPC_Op_VUPKLSB(as, insn);
                    break;

                case PPC_VXO_VUPKHSH:
                    PPC_Op_VUPKHSH(as, insn);
                    break;

                case PPC_VXO_VUPKLSH:
                    PPC_Op_VUPKLSH(as, insn);
                    break;

                case PPC_VXO_VUPKHPX:
                    PPC_Op_VUPKHPX(as, insn);
                    break;

                case PPC_VXO_VUPKLPX:
                    PPC_Op_VUPKLPX(as, insn);
                    break;

                /* Merge */
                case PPC_VXO_VMRGHB:
                    PPC_Op_VMRGHB(as, insn);
                    break;

                case PPC_VXO_VMRGLB:
                    PPC_Op_VMRGLB(as, insn);
                    break;

                /* Permute/Select */
                case PPC_VXO_VPERM:
                    PPC_Op_VPERM(as, insn);
                    break;

                case PPC_VXO_VSEL:
                    PPC_Op_VSEL(as, insn);
                    break;

                /* Compare */
                case PPC_VXO_VCMPEQUB:
                    PPC_Op_VCMPEQUB(as, insn);
                    break;

                case PPC_VXO_VCMPGTUB:
                    PPC_Op_VCMPGTUB(as, insn);
                    break;

                case PPC_VXO_VCMPGTSB:
                    PPC_Op_VCMPGTSB(as, insn);
                    break;

                case PPC_VXO_VCMPEQUH:
                    PPC_Op_VCMPEQUH(as, insn);
                    break;

                case PPC_VXO_VCMPEQUW:
                    PPC_Op_VCMPEQUW(as, insn);
                    break;

                /* Splat */
                case PPC_VXO_VSPLTB:
                    PPC_Op_VSPLTB(as, insn);
                    break;

                case PPC_VXO_VSPLTH:
                    PPC_Op_VSPLTH(as, insn);
                    break;

                case PPC_VXO_VSPLTW:
                    PPC_Op_VSPLTW(as, insn);
                    break;

                /* Multiply */
                case PPC_VXO_VMULESB:
                    PPC_Op_VMULESB(as, insn);
                    break;

                case PPC_VXO_VMULOSB:
                    PPC_Op_VMULOSB(as, insn);
                    break;

                case PPC_VXO_VMULEUB:
                    PPC_Op_VMULEUB(as, insn);
                    break;

                case PPC_VXO_VMULOUB:
                    PPC_Op_VMULOUB(as, insn);
                    break;

                case PPC_VXO_VMULESH:
                    PPC_Op_VMULESH(as, insn);
                    break;

                case PPC_VXO_VMULOSH:
                    PPC_Op_VMULOSH(as, insn);
                    break;

                case PPC_VXO_VMULEUH:
                    PPC_Op_VMULEUH(as, insn);
                    break;

                case PPC_VXO_VMULOUH:
                    PPC_Op_VMULOUH(as, insn);
                    break;

                /* Min/Max/Average */
                case PPC_VXO_VMAXSB:
                    PPC_Op_VMAXSB(as, insn);
                    break;

                case PPC_VXO_VMAXUB:
                    PPC_Op_VMAXUB(as, insn);
                    break;

                case PPC_VXO_VMINSB:
                    PPC_Op_VMINSB(as, insn);
                    break;

                case PPC_VXO_VMINUB:
                    PPC_Op_VMINUB(as, insn);
                    break;

                case PPC_VXO_VMAXSH:
                    PPC_Op_VMAXSH(as, insn);
                    break;

                case PPC_VXO_VMINSH:
                    PPC_Op_VMINSH(as, insn);
                    break;

                case PPC_VXO_VAVGSB:
                    PPC_Op_VAVGSB(as, insn);
                    break;

                case PPC_VXO_VAVGUB:
                    PPC_Op_VAVGUB(as, insn);
                    break;

                /* Rotate */
                case PPC_VXO_VRLB:
                    PPC_Op_VRLB(as, insn);
                    break;

                case PPC_VXO_VRLH:
                    PPC_Op_VRLH(as, insn);
                    break;

                case PPC_VXO_VRLW:
                    PPC_Op_VRLW(as, insn);
                    break;

                /* Word Shift */
                case PPC_VXO_VSLW:
                    PPC_Op_VSLW(as, insn);
                    break;

                case PPC_VXO_VSRW:
                    PPC_Op_VSRW(as, insn);
                    break;

                case PPC_VXO_VSLO:
                    PPC_Op_VSLO(as, insn);
                    break;

                case PPC_VXO_VSRO:
                    PPC_Op_VSRO(as, insn);
                    break;

                /* Merge Halfword/Word */
                case PPC_VXO_VMRGHH:
                    PPC_Op_VMRGHH(as, insn);
                    break;

                case PPC_VXO_VMRGLH:
                    PPC_Op_VMRGLH(as, insn);
                    break;

                case PPC_VXO_VMRGHW:
                    PPC_Op_VMRGHW(as, insn);
                    break;

                case PPC_VXO_VMRGLW:
                    PPC_Op_VMRGLW(as, insn);
                    break;

                /* Additional Compare */
                case PPC_VXO_VCMPGTUH:
                    PPC_Op_VCMPGTUH(as, insn);
                    break;

                case PPC_VXO_VCMPGTSH:
                    PPC_Op_VCMPGTSH(as, insn);
                    break;

                case PPC_VXO_VCMPGTUW:
                    PPC_Op_VCMPGTUW(as, insn);
                    break;

                case PPC_VXO_VCMPGTSW:
                    PPC_Op_VCMPGTSW(as, insn);
                    break;

                /* Additional Pack */
                case PPC_VXO_VPKUHUS:
                    PPC_Op_VPKUHUS(as, insn);
                    break;

                case PPC_VXO_VPKUWUS:
                    PPC_Op_VPKUWUS(as, insn);
                    break;

                /* Sum */
                case PPC_VXO_VSUM4UBS:
                    PPC_Op_VSUM4UBS(as, insn);
                    break;

                case PPC_VXO_VSUM4SBS:
                    PPC_Op_VSUM4SBS(as, insn);
                    break;

                case PPC_VXO_VSUM4SHS:
                    PPC_Op_VSUM4SHS(as, insn);
                    break;

                case PPC_VXO_VSUM2SWS:
                    PPC_Op_VSUM2SWS(as, insn);
                    break;

                case PPC_VXO_VSUMSWS:
                    PPC_Op_VSUMSWS(as, insn);
                    break;

                /* Additional Saturating Arithmetic */
                case PPC_VXO_VADDSWS:
                    PPC_Op_VADDSWS(as, insn);
                    break;

                case PPC_VXO_VSUBSWS:
                    PPC_Op_VSUBSWS(as, insn);
                    break;

                case PPC_VXO_VADDUWS:
                    PPC_Op_VADDUWS(as, insn);
                    break;

                case PPC_VXO_VSUBUWS:
                    PPC_Op_VSUBUWS(as, insn);
                    break;

                case PPC_VXO_VADDCUW:
                    PPC_Op_VADDCUW(as, insn);
                    break;

                case PPC_VXO_VSUBCUW:
                    PPC_Op_VSUBCUW(as, insn);
                    break;

                /* Additional Average */
                case PPC_VXO_VAVGSH:
                    PPC_Op_VAVGSH(as, insn);
                    break;

                case PPC_VXO_VAVGUH:
                    PPC_Op_VAVGUH(as, insn);
                    break;

                case PPC_VXO_VAVGSW:
                    PPC_Op_VAVGSW(as, insn);
                    break;

                case PPC_VXO_VAVGUW:
                    PPC_Op_VAVGUW(as, insn);
                    break;

                /* Additional Min/Max */
                case PPC_VXO_VMAXUH:
                    PPC_Op_VMAXUH(as, insn);
                    break;

                case PPC_VXO_VMINUH:
                    PPC_Op_VMINUH(as, insn);
                    break;

                case PPC_VXO_VMAXUW:
                    PPC_Op_VMAXUW(as, insn);
                    break;

                case PPC_VXO_VMINUW:
                    PPC_Op_VMINUW(as, insn);
                    break;

                case PPC_VXO_VMAXSW:
                    PPC_Op_VMAXSW(as, insn);
                    break;

                case PPC_VXO_VMINSW:
                    PPC_Op_VMINSW(as, insn);
                    break;

                /* Multiply-Add */
                case PPC_VXO_VMLADDUHM:
                    PPC_Op_VMLADDUHM(as, insn);
                    break;

                /* Additional Pack with Saturation */
                case PPC_VXO_VPKSWSS:
                    PPC_Op_VPKSWSS(as, insn);
                    break;

                case PPC_VXO_VPKSWUS:
                    PPC_Op_VPKSWUS(as, insn);
                    break;

                case PPC_VXO_VPKSHSS:
                    PPC_Op_VPKSHSS(as, insn);
                    break;

                case PPC_VXO_VPKSHUS:
                    PPC_Op_VPKSHUS(as, insn);
                    break;

                /* Floating-Point Arithmetic */
                case PPC_VXO_VADDFP:
                    PPC_Op_VADDFP(as, insn);
                    break;

                case PPC_VXO_VSUBFP:
                    PPC_Op_VSUBFP(as, insn);
                    break;

                case PPC_VXO_VMADDFP:
                    PPC_Op_VMADDFP(as, insn);
                    break;

                case PPC_VXO_VNMSUBFP:
                    PPC_Op_VNMSUBFP(as, insn);
                    break;

                case PPC_VXO_VMAXFP:
                    PPC_Op_VMAXFP(as, insn);
                    break;

                case PPC_VXO_VMINFP:
                    PPC_Op_VMINFP(as, insn);
                    break;

                /* Floating-Point Conversions */
                case PPC_VXO_VCFUX:
                    PPC_Op_VCFUX(as, insn);
                    break;

                case PPC_VXO_VCFSX:
                    PPC_Op_VCFSX(as, insn);
                    break;

                case PPC_VXO_VCTUXS:
                    PPC_Op_VCTUXS(as, insn);
                    break;

                case PPC_VXO_VCTSXS:
                    PPC_Op_VCTSXS(as, insn);
                    break;

                /* Floating-Point Rounding */
                case PPC_VXO_VRFIN:
                    PPC_Op_VRFIN(as, insn);
                    break;

                case PPC_VXO_VRFIZ:
                    PPC_Op_VRFIZ(as, insn);
                    break;

                case PPC_VXO_VRFIP:
                    PPC_Op_VRFIP(as, insn);
                    break;

                case PPC_VXO_VRFIM:
                    PPC_Op_VRFIM(as, insn);
                    break;

                /* Vector Shift Algebraic Halfword */
                case PPC_VXO_VSRAH:
                    PPC_Op_VSRAH(as, insn);
                    break;

                /* Vector Shift Left Double by Octet Immediate */
                case PPC_VXO_VSLDOI:
                    PPC_Op_VSLDOI(as, insn);
                    break;

                /* Vector Multiply-Add Halfword */
                case PPC_VXO_VMHADDSHS:
                    PPC_Op_VMHADDSHS(as, insn);
                    break;

                case PPC_VXO_VMHRADDSHS:
                    PPC_Op_VMHRADDSHS(as, insn);
                    break;

                case PPC_VXO_VMSUMUBM:
                    PPC_Op_VMSUMUBM(as, insn);
                    break;

                case PPC_VXO_VMSUMUHM:
                    PPC_Op_VMSUMUHM(as, insn);
                    break;

                /* Floating-Point Compare */
                case PPC_VXO_VCMPEQFP:
                    PPC_Op_VCMPEQFP(as, insn);
                    break;

                case PPC_VXO_VCMPGEFP:
                    PPC_Op_VCMPGEFP(as, insn);
                    break;

                case PPC_VXO_VCMPGTFP:
                    PPC_Op_VCMPGTFP(as, insn);
                    break;

                case PPC_VXO_VCMPBFP:
                    PPC_Op_VCMPBFP(as, insn);
                    break;

                /* Floating-Point Estimate */
                case PPC_VXO_VREFP:
                    PPC_Op_VREFP(as, insn);
                    break;

                case PPC_VXO_VRSQRTEFP:
                    PPC_Op_VRSQRTEFP(as, insn);
                    break;

                case PPC_VXO_VEXPTEFP:
                    PPC_Op_VEXPTEFP(as, insn);
                    break;

                case PPC_VXO_VLOGEFP:
                    PPC_Op_VLOGEFP(as, insn);
                    break;

                /* Additional Multiply-Sum */
                case PPC_VXO_VMSUMMBM:
                    PPC_Op_VMSUMMBM(as, insn);
                    break;

                case PPC_VXO_VMSUMSHM:
                    PPC_Op_VMSUMSHM(as, insn);
                    break;

                case PPC_VXO_VMSUMUHS:
                    PPC_Op_VMSUMUHS(as, insn);
                    break;

                case PPC_VXO_VMSUMSHS:
                    PPC_Op_VMSUMSHS(as, insn);
                    break;

                /* Vector Status Register */
                case PPC_XOP_MFVSCR:
                    PPC_Op_MFVSCR(as, insn);
                    break;

                case PPC_XOP_MTVSCR:
                    PPC_Op_MTVSCR(as, insn);
                    break;

                default:
                    PPC_Fault(as, "Unimplemented AltiVec opcode");
                    break;
            }
            break;

        case PPC_OP_MULLI:       /* 7 */
            PPC_Op_MULLI(as, insn);
            break;

        case PPC_OP_SUBFIC:      /* 8 */
            PPC_Op_SUBFIC(as, insn);
            break;

        case PPC_OP_DOZI:        /* 9 - PowerPC 601 difference or zero immediate */
            PPC_Op_DOZI(as, insn);
            break;

        case PPC_OP_CMPLI:       /* 10 */
            PPC_Op_CMPLI(as, insn);
            break;

        case PPC_OP_CMPI:        /* 11 */
            PPC_Op_CMPI(as, insn);
            break;

        case PPC_OP_ADDIC:       /* 12 */
            PPC_Op_ADDIC(as, insn);
            break;

        case PPC_OP_ADDIC_RC:    /* 13 */
            PPC_Op_ADDIC_RC(as, insn);
            break;

        case PPC_OP_ADDI:        /* 14 */
            PPC_Op_ADDI(as, insn);
            break;

        case PPC_OP_ADDIS:       /* 15 */
            PPC_Op_ADDIS(as, insn);
            break;

        case PPC_OP_BC:          /* 16 */
            PPC_Op_BC(as, insn);
            break;

        case PPC_OP_SC:          /* 17 */
            PPC_Op_SC(as, insn);
            break;

        case PPC_OP_B:           /* 18 */
            PPC_Op_B(as, insn);
            break;

        case PPC_OP_EXT19:       /* 19 - Extended opcodes (branches to LR/CTR, CR ops) */
            extended = PPC_EXTENDED_OPCODE(insn);
            switch (extended) {
                case PPC_XOP19_MCRF:
                    PPC_Op_MCRF(as, insn);
                    break;

                case PPC_XOP19_BCLR:
                    PPC_Op_BCLR(as, insn);
                    break;

                case PPC_XOP19_BCCTR:
                    PPC_Op_BCCTR(as, insn);
                    break;

                case PPC_XOP19_CRAND:
                    PPC_Op_CRAND(as, insn);
                    break;

                case PPC_XOP19_CROR:
                    PPC_Op_CROR(as, insn);
                    break;

                case PPC_XOP19_CRXOR:
                    PPC_Op_CRXOR(as, insn);
                    break;

                case PPC_XOP19_ISYNC:
                    PPC_Op_ISYNC(as, insn);
                    break;

                case PPC_XOP19_SYNC:
                    PPC_Op_SYNC(as, insn);
                    break;

                case PPC_XOP19_RFI:
                    PPC_Op_RFI(as, insn);
                    break;

                default:
                    /* Check for other CR ops */
                    if (extended == 225) PPC_Op_CRNAND(as, insn);
                    else if (extended == 33) PPC_Op_CRNOR(as, insn);
                    else if (extended == 289) PPC_Op_CREQV(as, insn);
                    else if (extended == 129) PPC_Op_CRANDC(as, insn);
                    else if (extended == 417) PPC_Op_CRORC(as, insn);
                    else PPC_Fault(as, "Unimplemented opcode 19 extended");
                    break;
            }
            break;

        case PPC_OP_RLWIMI:      /* 20 */
            PPC_Op_RLWIMI(as, insn);
            break;

        case PPC_OP_RLWINM:      /* 21 */
            PPC_Op_RLWINM(as, insn);
            break;

        case PPC_OP_RLMI:        /* 22 - PowerPC 601 rotate left then mask insert */
            PPC_Op_RLMI(as, insn);
            break;

        case PPC_OP_RLWNM:       /* 23 */
            PPC_Op_RLWNM(as, insn);
            break;

        case PPC_OP_ORI:         /* 24 */
            PPC_Op_ORI(as, insn);
            break;

        case PPC_OP_ORIS:        /* 25 */
            PPC_Op_ORIS(as, insn);
            break;

        case PPC_OP_XORI:        /* 26 */
            PPC_Op_XORI(as, insn);
            break;

        case PPC_OP_XORIS:       /* 27 */
            PPC_Op_XORIS(as, insn);
            break;

        case PPC_OP_ANDI_RC:     /* 28 */
            PPC_Op_ANDI_RC(as, insn);
            break;

        case PPC_OP_ANDIS_RC:    /* 29 */
            PPC_Op_ANDIS_RC(as, insn);
            break;

        case PPC_OP_EXT31:       /* 31 - Extended opcodes (arithmetic, logical, load/store) */
            extended = PPC_EXTENDED_OPCODE(insn);
            switch (extended) {
                case PPC_XOP_CMP:
                    PPC_Op_CMP(as, insn);
                    break;

                case PPC_XOP_CMPL:
                    PPC_Op_CMPL(as, insn);
                    break;

                case PPC_XOP_TW:
                    PPC_Op_TW(as, insn);
                    break;

                /* Arithmetic */
                case 8:    /* SUBFC */
                    PPC_Op_SUBFC(as, insn);
                    break;

                case 10:   /* ADDC */
                    PPC_Op_ADDC(as, insn);
                    break;

                case PPC_XOP_MULHWU:
                    PPC_Op_MULHWU(as, insn);
                    break;

                case PPC_XOP_MFCR:
                    PPC_Op_MFCR(as, insn);
                    break;

                case PPC_XOP_LWARX:
                    PPC_Op_LWARX(as, insn);
                    break;

                case PPC_XOP_CNTLZW:
                    PPC_Op_CNTLZW(as, insn);
                    break;

                case PPC_XOP_SUBF:
                    if (insn & 0x00000400) {
                        PPC_Op_SUBFO(as, insn);
                    } else {
                        PPC_Op_SUBF(as, insn);
                    }
                    break;

                case PPC_XOP_DCBST:
                    PPC_Op_DCBST(as, insn);
                    break;

                case PPC_XOP_MULHW:
                    PPC_Op_MULHW(as, insn);
                    break;

                case PPC_XOP_DCBF:
                    PPC_Op_DCBF(as, insn);
                    break;

                case 104:  /* NEG */
                    if (insn & 0x00000400) {
                        PPC_Op_NEGO(as, insn);
                    } else {
                        PPC_Op_NEG(as, insn);
                    }
                    break;

                case PPC_XOP_SUBFE:
                    PPC_Op_SUBFE(as, insn);
                    break;

                case PPC_XOP_ADDE:
                    PPC_Op_ADDE(as, insn);
                    break;

                case PPC_XOP_MTCRF:
                    PPC_Op_MTCRF(as, insn);
                    break;

                case PPC_XOP_STWCX:
                    PPC_Op_STWCX(as, insn);
                    break;

                case PPC_XOP_SUBFZE:
                    PPC_Op_SUBFZE(as, insn);
                    break;

                case PPC_XOP_ADDZE:
                    PPC_Op_ADDZE(as, insn);
                    break;

                case PPC_XOP_SUBFME:
                    PPC_Op_SUBFME(as, insn);
                    break;

                case PPC_XOP_ADDME:
                    PPC_Op_ADDME(as, insn);
                    break;

                case PPC_XOP_ADD:
                    if (insn & 0x00000400) {
                        PPC_Op_ADDO(as, insn);
                    } else {
                        PPC_Op_ADD(as, insn);
                    }
                    break;

                case PPC_XOP_MULLW:
                    if (insn & 0x00000400) {
                        PPC_Op_MULLWO(as, insn);
                    } else {
                        PPC_Op_MULLW(as, insn);
                    }
                    break;

                case PPC_XOP_MFSPR:
                    PPC_Op_MFSPR(as, insn);
                    break;

                case PPC_XOP_DIVWU:
                    PPC_Op_DIVWU(as, insn);
                    break;

                case PPC_XOP_MTSPR:
                    PPC_Op_MTSPR(as, insn);
                    break;

                case PPC_XOP_DIVW:
                    if (insn & 0x00000400) {
                        PPC_Op_DIVWO(as, insn);
                    } else {
                        PPC_Op_DIVW(as, insn);
                    }
                    break;

                case PPC_XOP_LSWX:
                    PPC_Op_LSWX(as, insn);
                    break;

                case PPC_XOP_LSWI:
                    PPC_Op_LSWI(as, insn);
                    break;

                case PPC_XOP_STSWX:
                    PPC_Op_STSWX(as, insn);
                    break;

                case PPC_XOP_STSWI:
                    PPC_Op_STSWI(as, insn);
                    break;

                /* Logical */
                case PPC_XOP_AND:
                    PPC_Op_AND(as, insn);
                    break;

                case PPC_XOP_ANDC:
                    PPC_Op_ANDC(as, insn);
                    break;

                case PPC_XOP_OR:
                    PPC_Op_OR(as, insn);
                    break;

                case PPC_XOP_ORC:
                    PPC_Op_ORC(as, insn);
                    break;

                case PPC_XOP_XOR:
                    PPC_Op_XOR(as, insn);
                    break;

                case PPC_XOP_NAND:
                    PPC_Op_NAND(as, insn);
                    break;

                case PPC_XOP_NOR:
                    PPC_Op_NOR(as, insn);
                    break;

                case PPC_XOP_EQV:
                    PPC_Op_EQV(as, insn);
                    break;

                /* Shifts */
                case PPC_XOP_SLW:
                    PPC_Op_SLW(as, insn);
                    break;

                case PPC_XOP_SRW:
                    PPC_Op_SRW(as, insn);
                    break;

                case PPC_XOP_SRAW:
                    PPC_Op_SRAW(as, insn);
                    break;

                case PPC_XOP_SRAWI:
                    PPC_Op_SRAWI(as, insn);
                    break;

                /* Sign extension */
                case PPC_XOP_EXTSH:
                    PPC_Op_EXTSH(as, insn);
                    break;

                case PPC_XOP_EXTSB:
                    PPC_Op_EXTSB(as, insn);
                    break;

                /* Cache management */
                case PPC_XOP_ICBI:
                    PPC_Op_ICBI(as, insn);
                    break;

                case PPC_XOP_DCBZ:
                    PPC_Op_DCBZ(as, insn);
                    break;

                /* Indexed loads/stores */
                case PPC_XOP_LWZX:
                    PPC_Op_LWZX(as, insn);
                    break;

                case PPC_XOP_LWZUX:
                    PPC_Op_LWZUX(as, insn);
                    break;

                case PPC_XOP_LBZX:
                    PPC_Op_LBZX(as, insn);
                    break;

                case PPC_XOP_LBZUX:
                    PPC_Op_LBZUX(as, insn);
                    break;

                case PPC_XOP_LHZX:
                    PPC_Op_LHZX(as, insn);
                    break;

                case PPC_XOP_LHZUX:
                    PPC_Op_LHZUX(as, insn);
                    break;

                case PPC_XOP_LHAX:
                    PPC_Op_LHAX(as, insn);
                    break;

                case PPC_XOP_LHAUX:
                    PPC_Op_LHAUX(as, insn);
                    break;

                case PPC_XOP_STWX:
                    PPC_Op_STWX(as, insn);
                    break;

                case PPC_XOP_STWUX:
                    PPC_Op_STWUX(as, insn);
                    break;

                case PPC_XOP_STBX:
                    PPC_Op_STBX(as, insn);
                    break;

                case PPC_XOP_STBUX:
                    PPC_Op_STBUX(as, insn);
                    break;

                case PPC_XOP_STHX:
                    PPC_Op_STHX(as, insn);
                    break;

                case PPC_XOP_STHUX:
                    PPC_Op_STHUX(as, insn);
                    break;

                /* Byte-reversed load/store */
                case PPC_XOP_LWBRX:
                    PPC_Op_LWBRX(as, insn);
                    break;

                case PPC_XOP_LHBRX:
                    PPC_Op_LHBRX(as, insn);
                    break;

                case PPC_XOP_STWBRX:
                    PPC_Op_STWBRX(as, insn);
                    break;

                case PPC_XOP_STHBRX:
                    PPC_Op_STHBRX(as, insn);
                    break;

                /* Floating-point indexed load/store */
                case PPC_XOP_LFSX:
                    PPC_Op_LFSX(as, insn);
                    break;

                case PPC_XOP_LFSUX:
                    PPC_Op_LFSUX(as, insn);
                    break;

                case PPC_XOP_LFDX:
                    PPC_Op_LFDX(as, insn);
                    break;

                case PPC_XOP_LFDUX:
                    PPC_Op_LFDUX(as, insn);
                    break;

                case PPC_XOP_STFSX:
                    PPC_Op_STFSX(as, insn);
                    break;

                case PPC_XOP_STFSUX:
                    PPC_Op_STFSUX(as, insn);
                    break;

                case PPC_XOP_STFDX:
                    PPC_Op_STFDX(as, insn);
                    break;

                case PPC_XOP_STFDUX:
                    PPC_Op_STFDUX(as, insn);
                    break;

                case PPC_XOP_STFIWX:
                    PPC_Op_STFIWX(as, insn);
                    break;

                /* Memory ordering */
                case PPC_XOP_EIEIO:
                    PPC_Op_EIEIO(as, insn);
                    break;

                /* System instructions */
                case PPC_XOP_MFMSR:
                    PPC_Op_MFMSR(as, insn);
                    break;

                case PPC_XOP_MTMSR:
                    PPC_Op_MTMSR(as, insn);
                    break;

                /* Segment register operations */
                case PPC_XOP_MFSR:
                    PPC_Op_MFSR(as, insn);
                    break;

                case PPC_XOP_MTSR:
                    PPC_Op_MTSR(as, insn);
                    break;

                case PPC_XOP_MFSRIN:
                    PPC_Op_MFSRIN(as, insn);
                    break;

                case PPC_XOP_MTSRIN:
                    PPC_Op_MTSRIN(as, insn);
                    break;

                /* TLB management */
                case PPC_XOP_TLBIE:
                    PPC_Op_TLBIE(as, insn);
                    break;

                case PPC_XOP_TLBSYNC:
                    PPC_Op_TLBSYNC(as, insn);
                    break;

                case PPC_XOP_TLBIA:
                    PPC_Op_TLBIA(as, insn);
                    break;

                /* Additional cache control */
                case PPC_XOP_DCBI:
                    PPC_Op_DCBI(as, insn);
                    break;

                case PPC_XOP_DCBT:
                    PPC_Op_DCBT(as, insn);
                    break;

                case PPC_XOP_DCBTST:
                    PPC_Op_DCBTST(as, insn);
                    break;

                case PPC_XOP_DCBA:
                    PPC_Op_DCBA(as, insn);
                    break;

                /* External control */
                case PPC_XOP_ECIWX:
                    PPC_Op_ECIWX(as, insn);
                    break;

                case PPC_XOP_ECOWX:
                    PPC_Op_ECOWX(as, insn);
                    break;

                /* Time base access */
                case PPC_XOP_MFTB:
                    PPC_Op_MFTB(as, insn);
                    break;

                /* PowerPC 601 compatibility instructions */
                case PPC_XOP_DOZ:
                    PPC_Op_DOZ(as, insn);
                    break;

                case PPC_XOP_MUL:
                    PPC_Op_MUL(as, insn);
                    break;

                case PPC_XOP_DIV:
                    PPC_Op_DIV(as, insn);
                    break;

                case PPC_XOP_DIVS:
                    PPC_Op_DIVS(as, insn);
                    break;

                case PPC_XOP_ABS:
                    PPC_Op_ABS(as, insn);
                    break;

                case PPC_XOP_NABS:
                    PPC_Op_NABS(as, insn);
                    break;

                case PPC_XOP_CLCS:
                    PPC_Op_CLCS(as, insn);
                    break;

                /* AltiVec vector load/store */
                case PPC_OP_LVX:
                    PPC_Op_LVX(as, insn);
                    break;

                case PPC_OP_STVX:
                    PPC_Op_STVX(as, insn);
                    break;

                case PPC_OP_LVEBX:
                    PPC_Op_LVEBX(as, insn);
                    break;

                case PPC_OP_LVEHX:
                    PPC_Op_LVEHX(as, insn);
                    break;

                case PPC_OP_STVEBX:
                    PPC_Op_STVEBX(as, insn);
                    break;

                case PPC_OP_STVEHX:
                    PPC_Op_STVEHX(as, insn);
                    break;

                /* Additional vector load/store */
                case PPC_OP_LVSL:
                    PPC_Op_LVSL(as, insn);
                    break;

                case PPC_OP_LVSR:
                    PPC_Op_LVSR(as, insn);
                    break;

                case PPC_OP_LVEWX:
                    PPC_Op_LVEWX(as, insn);
                    break;

                case PPC_OP_STVEWX:
                    PPC_Op_STVEWX(as, insn);
                    break;

                case PPC_OP_LVXL:
                    PPC_Op_LVXL(as, insn);
                    break;

                case PPC_OP_STVXL:
                    PPC_Op_STVXL(as, insn);
                    break;

                /* Data Stream Touch (cache hints - NOPs) */
                case PPC_XOP_DST:
                    PPC_Op_DST(as, insn);
                    break;

                case PPC_XOP_DSTT:
                    PPC_Op_DSTT(as, insn);
                    break;

                case PPC_XOP_DSTST:
                    PPC_Op_DSTST(as, insn);
                    break;

                case PPC_XOP_DSTSTT:
                    PPC_Op_DSTSTT(as, insn);
                    break;

                case PPC_XOP_DSS:
                    PPC_Op_DSS(as, insn);
                    break;

                case PPC_XOP_DSSALL:
                    PPC_Op_DSSALL(as, insn);
                    break;

                default:
                    PPC_Fault(as, "Unimplemented opcode 31 extended");
                    break;
            }
            break;

        case PPC_OP_LWZ:         /* 32 */
            PPC_Op_LWZ(as, insn);
            break;

        case PPC_OP_LWZU:        /* 33 */
            PPC_Op_LWZU(as, insn);
            break;

        case PPC_OP_LBZ:         /* 34 */
            PPC_Op_LBZ(as, insn);
            break;

        case PPC_OP_LBZU:        /* 35 */
            PPC_Op_LBZU(as, insn);
            break;

        case PPC_OP_STW:         /* 36 */
            PPC_Op_STW(as, insn);
            break;

        case PPC_OP_STWU:        /* 37 */
            PPC_Op_STWU(as, insn);
            break;

        case PPC_OP_STB:         /* 38 */
            PPC_Op_STB(as, insn);
            break;

        case PPC_OP_STBU:        /* 39 */
            PPC_Op_STBU(as, insn);
            break;

        case PPC_OP_LHZ:         /* 40 */
            PPC_Op_LHZ(as, insn);
            break;

        case PPC_OP_LHZU:        /* 41 */
            PPC_Op_LHZU(as, insn);
            break;

        case PPC_OP_LHA:         /* 42 */
            PPC_Op_LHA(as, insn);
            break;

        case PPC_OP_LHAU:        /* 43 */
            PPC_Op_LHAU(as, insn);
            break;

        case PPC_OP_STH:         /* 44 */
            PPC_Op_STH(as, insn);
            break;

        case PPC_OP_STHU:        /* 45 */
            PPC_Op_STHU(as, insn);
            break;

        case PPC_OP_LMW:         /* 46 */
            PPC_Op_LMW(as, insn);
            break;

        case PPC_OP_STMW:        /* 47 */
            PPC_Op_STMW(as, insn);
            break;

        case PPC_OP_LFS:         /* 48 */
            PPC_Op_LFS(as, insn);
            break;

        case PPC_OP_LFSU:        /* 49 */
            PPC_Op_LFSU(as, insn);
            break;

        case PPC_OP_LFD:         /* 50 */
            PPC_Op_LFD(as, insn);
            break;

        case PPC_OP_LFDU:        /* 51 */
            PPC_Op_LFDU(as, insn);
            break;

        case PPC_OP_STFS:        /* 52 */
            PPC_Op_STFS(as, insn);
            break;

        case PPC_OP_STFSU:       /* 53 */
            PPC_Op_STFSU(as, insn);
            break;

        case PPC_OP_STFD:        /* 54 */
            PPC_Op_STFD(as, insn);
            break;

        case PPC_OP_STFDU:       /* 55 */
            PPC_Op_STFDU(as, insn);
            break;

        case PPC_OP_EXT59:       /* 59 - Single-precision FP arithmetic */
            extended = PPC_EXTENDED_OPCODE(insn);
            switch (extended) {
                case PPC_XOP59_FADDS:
                    PPC_Op_FADDS(as, insn);
                    break;

                case PPC_XOP59_FSUBS:
                    PPC_Op_FSUBS(as, insn);
                    break;

                case PPC_XOP59_FMULS:
                    PPC_Op_FMULS(as, insn);
                    break;

                case PPC_XOP59_FDIVS:
                    PPC_Op_FDIVS(as, insn);
                    break;

                case PPC_XOP59_FSQRTS:
                    PPC_Op_FSQRTS(as, insn);
                    break;

                case PPC_XOP59_FRES:
                    PPC_Op_FRES(as, insn);
                    break;

                case PPC_XOP59_FMADDS:
                    PPC_Op_FMADDS(as, insn);
                    break;

                case PPC_XOP59_FMSUBS:
                    PPC_Op_FMSUBS(as, insn);
                    break;

                case PPC_XOP59_FNMADDS:
                    PPC_Op_FNMADDS(as, insn);
                    break;

                case PPC_XOP59_FNMSUBS:
                    PPC_Op_FNMSUBS(as, insn);
                    break;

                default:
                    PPC_Fault(as, "Unimplemented opcode 59 extended");
                    break;
            }
            break;

        case PPC_OP_EXT63:       /* 63 - Double-precision FP arithmetic */
            extended = PPC_EXTENDED_OPCODE(insn);
            switch (extended) {
                case PPC_XOP63_FCMPU:
                    PPC_Op_FCMPU(as, insn);
                    break;

                case PPC_XOP63_FRSP:
                    PPC_Op_FRSP(as, insn);
                    break;

                case PPC_XOP63_FCTIW:
                    PPC_Op_FCTIW(as, insn);
                    break;

                case PPC_XOP63_FCTIWZ:
                    PPC_Op_FCTIWZ(as, insn);
                    break;

                case PPC_XOP63_FDIV:
                    PPC_Op_FDIV(as, insn);
                    break;

                case PPC_XOP63_FSUB:
                    PPC_Op_FSUB(as, insn);
                    break;

                case PPC_XOP63_FADD:
                    PPC_Op_FADD(as, insn);
                    break;

                case PPC_XOP63_FSQRT:
                    PPC_Op_FSQRT(as, insn);
                    break;

                case PPC_XOP63_FSEL:
                    PPC_Op_FSEL(as, insn);
                    break;

                case PPC_XOP63_FMUL:
                    PPC_Op_FMUL(as, insn);
                    break;

                case PPC_XOP63_FRSQRTE:
                    PPC_Op_FRSQRTE(as, insn);
                    break;

                case PPC_XOP63_FMSUB:
                    PPC_Op_FMSUB(as, insn);
                    break;

                case PPC_XOP63_FMADD:
                    PPC_Op_FMADD(as, insn);
                    break;

                case PPC_XOP63_FNMSUB:
                    PPC_Op_FNMSUB(as, insn);
                    break;

                case PPC_XOP63_FNMADD:
                    PPC_Op_FNMADD(as, insn);
                    break;

                case PPC_XOP63_FCMPO:
                    PPC_Op_FCMPO(as, insn);
                    break;

                case PPC_XOP63_FNEG:
                    PPC_Op_FNEG(as, insn);
                    break;

                case PPC_XOP63_MTFSB1:
                    PPC_Op_MTFSB1(as, insn);
                    break;

                case PPC_XOP63_MTFSB0:
                    PPC_Op_MTFSB0(as, insn);
                    break;

                case PPC_XOP63_FMR:
                    PPC_Op_FMR(as, insn);
                    break;

                case PPC_XOP63_MTFSFI:
                    PPC_Op_MTFSFI(as, insn);
                    break;

                case PPC_XOP63_FNABS:
                    PPC_Op_FNABS(as, insn);
                    break;

                case PPC_XOP63_FABS:
                    PPC_Op_FABS(as, insn);
                    break;

                case PPC_XOP63_MFFS:
                    PPC_Op_MFFS(as, insn);
                    break;

                case PPC_XOP63_MCRFS:
                    PPC_Op_MCRFS(as, insn);
                    break;

                case PPC_XOP63_MTFSF:
                    PPC_Op_MTFSF(as, insn);
                    break;

                default:
                    PPC_Fault(as, "Unimplemented opcode 63 extended");
                    break;
            }
            break;

        default:
            serial_printf("[PPC] ILLEGAL opcode 0x%08X (primary=0x%02X) at PC=0x%08X\n",
                         insn, primary, as->regs.pc - 4);
            PPC_Fault(as, "Illegal opcode");
            break;
    }

    return noErr;
}

/*
 * PPC_Execute - Execute up to maxInstructions
 */
OSErr PPC_Execute(PPCAddressSpace* as, UInt32 startPC, UInt32 maxInstructions)
{
    UInt32 count = 0;

    if (!as) {
        return paramErr;
    }

    as->regs.pc = startPC;
    as->halted = false;

    while (count < maxInstructions && !as->halted) {
        PPC_Step(as);
        count++;
    }

    return noErr;
}
