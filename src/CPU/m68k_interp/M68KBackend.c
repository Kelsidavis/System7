/*
 * M68KBackend.c - 68K Interpreter CPU Backend Implementation
 *
 * Implements ICPUBackend interface for 68K code execution via interpretation.
 * Runs on any host ISA (x86, ARM, Raspberry Pi, etc.) by interpreting 68K instructions.
 *
 * PLATFORM SUPPORT:
 * - x86 (IA32): Fully supported, primary development platform
 * - ARM (ARMv6, ARMv7, ARMv8): Fully supported, enables 68K compatibility on Raspberry Pi 3/4/5
 * - Other architectures: Should work with no source modifications due to explicit byte ordering
 *
 * CROSS-PLATFORM GUARANTEES:
 * - All 68K values are stored in big-endian format (Motorola byte order)
 * - Memory operations use explicit byte reconstruction, never assume host endianness
 * - Page allocation is generic and works on all architectures
 * - No inline assembly or architecture-specific tricks
 *
 * ARM RASPBERRY PI INTEGRATION:
 * The 68K interpreter is automatically compiled and linked when PLATFORM=arm,
 * allowing System 7.1 classic Mac applications to run on Raspberry Pi 3/4/5.
 * Build with:
 *   make PLATFORM=arm PI_MODEL=pi3 clean all   # Raspberry Pi 3
 *   make PLATFORM=arm PI_MODEL=pi4 clean all   # Raspberry Pi 4
 *   make PLATFORM=arm PI_MODEL=pi5 clean all   # Raspberry Pi 5
 */

#include "CPU/M68KInterp.h"
#include "CPU/CPUBackend.h"
#include "CPU/LowMemGlobals.h"
#include "SegmentLoader/SegmentLoader.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include "CPU/CPULogging.h"
#include <string.h>

/* Forward declarations of ICPUBackend methods */
static OSErr M68K_CreateAddressSpace(void* processHandle, CPUAddressSpace* out);
static OSErr M68K_DestroyAddressSpace(CPUAddressSpace as);
static OSErr M68K_MapExecutable(CPUAddressSpace as, const void* image, Size len,
                                CPUMapFlags flags, CPUCodeHandle* outHandle,
                                CPUAddr* outBase);
static OSErr M68K_UnmapExecutable(CPUAddressSpace as, CPUCodeHandle handle);
static OSErr M68K_SetRegisterA5(CPUAddressSpace as, CPUAddr a5);
static OSErr M68K_SetStacks(CPUAddressSpace as, CPUAddr usp, CPUAddr ssp);
static OSErr M68K_InstallTrap(CPUAddressSpace as, TrapNumber trapNum,
                              CPUTrapHandler handler, void* context);
static OSErr M68K_WriteJumpTableSlot(CPUAddressSpace as, CPUAddr slotAddr,
                                     CPUAddr target);
static OSErr M68K_MakeLazyJTStub(CPUAddressSpace as, CPUAddr slotAddr,
                                 SInt16 segID, SInt16 entryIndex);
static OSErr M68K_EnterAt(CPUAddressSpace as, CPUAddr entry, CPUEnterFlags flags);
static OSErr M68K_Relocate(CPUAddressSpace as, CPUCodeHandle code,
                           const RelocTable* relocs, CPUAddr segBase,
                           CPUAddr jtBase, CPUAddr a5Base);
static OSErr M68K_AllocateMemory(CPUAddressSpace as, Size size,
                                 CPUMapFlags flags, CPUAddr* outAddr);
static OSErr M68K_WriteMemory(CPUAddressSpace as, CPUAddr addr,
                              const void* data, Size len);
static OSErr M68K_ReadMemory(CPUAddressSpace as, CPUAddr addr,
                             void* data, Size len);

/*
 * Global M68K Backend Instance
 */
const ICPUBackend gM68KInterpreterBackend = {
    .CreateAddressSpace = M68K_CreateAddressSpace,
    .DestroyAddressSpace = M68K_DestroyAddressSpace,
    .MapExecutable = M68K_MapExecutable,
    .UnmapExecutable = M68K_UnmapExecutable,
    .SetRegisterA5 = M68K_SetRegisterA5,
    .SetStacks = M68K_SetStacks,
    .InstallTrap = M68K_InstallTrap,
    .WriteJumpTableSlot = M68K_WriteJumpTableSlot,
    .MakeLazyJTStub = M68K_MakeLazyJTStub,
    .EnterAt = M68K_EnterAt,
    .Relocate = M68K_Relocate,
    .AllocateMemory = M68K_AllocateMemory,
    .WriteMemory = M68K_WriteMemory,
    .ReadMemory = M68K_ReadMemory
};

/*
 * M68K Backend Initialization
 */
OSErr M68KBackend_Initialize(void)
{
    return CPUBackend_Register("m68k_interp", &gM68KInterpreterBackend);
}

/*
 * CreateAddressSpace - Allocate M68K address space
 */
static OSErr M68K_CreateAddressSpace(void* processHandle, CPUAddressSpace* out)
{
    M68KAddressSpace* as;

    (void)processHandle; /* Unused for now */

    M68K_LOG_INFO("CreateAddressSpace: allocating M68KAddressSpace struct size=%u\n",
                  (unsigned)sizeof(M68KAddressSpace));
    as = (M68KAddressSpace*)NewPtr(sizeof(M68KAddressSpace));
    if (!as) {
        M68K_LOG_ERROR("FAIL: struct allocation memFullErr, MemError=%d\n", MemError());
        return memFullErr;
    }

    memset(as, 0, sizeof(M68KAddressSpace));
    as->baseAddr = 0;

    /* Initialize page table (all NULL = not allocated) */
    memset(as->pageTable, 0, sizeof(as->pageTable));

    /* Pre-allocate low memory pages (0x0000-0xFFFF = first 16 pages) */
    M68K_LOG_INFO("CreateAddressSpace: pre-allocating %d low memory pages (%u KB)\n",
                  M68K_LOW_MEM_PAGES, M68K_LOW_MEM_SIZE / 1024);

    for (int i = 0; i < M68K_LOW_MEM_PAGES; i++) {
        as->pageTable[i] = NewPtr(M68K_PAGE_SIZE);
        if (!as->pageTable[i]) {
            M68K_LOG_ERROR("FAIL: low memory page %d allocation failed, MemError=%d\n",
                         i, MemError());
            /* Free already allocated pages */
            for (int j = 0; j < i; j++) {
                if (as->pageTable[j]) {
                    DisposePtr((Ptr)as->pageTable[j]);
                }
            }
            DisposePtr((Ptr)as);
            return memFullErr;
        }
        memset(as->pageTable[i], 0, M68K_PAGE_SIZE);
    }

    M68K_LOG_INFO("CreateAddressSpace: low memory allocated, sparse 16MB virtual space ready\n");

    /* Initialize registers */
    as->faultReason = NULL;
    as->faultPC = 0;
    memset(&as->regs, 0, sizeof(M68KRegs));
    as->regs.sr = 0x2700; /* Supervisor mode, interrupts disabled */

    /* Initialize low memory globals system */
    LMInit(as);

    *out = (CPUAddressSpace)as;
    return noErr;
}

/*
 * DestroyAddressSpace - Free address space
 */
static OSErr M68K_DestroyAddressSpace(CPUAddressSpace as)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;

    if (!mas) {
        return paramErr;
    }

    /* Free all allocated pages */
    for (int i = 0; i < M68K_NUM_PAGES; i++) {
        if (mas->pageTable[i]) {
            if (!MemoryManager_IsHeapPointer(mas->pageTable[i])) {
                DisposePtr((Ptr)mas->pageTable[i]);
            }
            mas->pageTable[i] = NULL;
        }
    }

    DisposePtr((Ptr)mas);
    return noErr;
}

/* Forward declaration */
void* M68K_GetPage(M68KAddressSpace* as, UInt32 addr, Boolean allocate);

/*
 * M68K_MemCopy - Copy data to paged memory (lazy page allocation)
 */
static OSErr M68K_MemCopy(M68KAddressSpace* as, UInt32 addr, const void* src, Size len)
{
    const UInt8* srcBytes = (const UInt8*)src;

    for (Size i = 0; i < len; i++) {
        void* page = M68K_GetPage(as, addr + i, true);
        if (!page) {
            return memFullErr;
        }
        UInt32 offset = (addr + i) & (M68K_PAGE_SIZE - 1);
        ((UInt8*)page)[offset] = srcBytes[i];
    }
    return noErr;
}

/*
 * M68K_GetPage - Get page for address, allocating if needed (lazy allocation)
 * Returns NULL if address out of range or allocation fails
 */
void* M68K_GetPage(M68KAddressSpace* as, UInt32 addr, Boolean allocate)
{
    UInt32 pageNum;
    void* page;

    /* Check address range */
    if (addr >= M68K_MAX_ADDR) {
        return NULL;
    }

    pageNum = addr >> M68K_PAGE_SHIFT;
    page = as->pageTable[pageNum];

    /* If page not allocated and allocation requested, allocate now */
    if (!page && allocate) {
        page = NewPtr(M68K_PAGE_SIZE);
        if (page) {
            memset(page, 0, M68K_PAGE_SIZE);
            as->pageTable[pageNum] = page;
            M68K_LOG_DEBUG("Allocated page %u for addr 0x%08X\n", pageNum, addr);
        } else {
            serial_printf("[M68K] FAIL: page %u allocation failed, MemError=%d\n",
                         pageNum, MemError());
        }
    }

    return page;
}

/*
 * MapExecutable - Map code into address space
 */
static OSErr M68K_MapExecutable(CPUAddressSpace as, const void* image, Size len,
                                CPUMapFlags flags, CPUCodeHandle* outHandle,
                                CPUAddr* outBase)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;
    M68KCodeHandle* handle;
    UInt32 addr;

    if (!mas || !image || !outHandle || !outBase) {
        return paramErr;
    }

    /* Allocate code handle */
    handle = (M68KCodeHandle*)NewPtr(sizeof(M68KCodeHandle));
    if (!handle) {
        return memFullErr;
    }

    /* Find free address space (simple bump allocator) */
    addr = 0x1000; /* Start at 4K to avoid null pointers */
    for (int i = 0; i < mas->numCodeSegs; i++) {
        UInt32 end = mas->codeSegBases[i] + mas->codeSegSizes[i];
        if (end > addr) {
            addr = end;
        }
    }

    /* Align to 16-byte boundary */
    addr = (addr + 15) & ~15;

    /* Check bounds */
    if (addr + len > M68K_MAX_ADDR) {
        DisposePtr((Ptr)handle);
        return memFullErr;
    }

    /* Copy code into address space (allocates pages as needed) */
    if (M68K_MemCopy(mas, addr, image, len) != noErr) {
        DisposePtr((Ptr)handle);
        return memFullErr;
    }

    /* Track segment */
    if (mas->numCodeSegs < 256) {
        void* firstPage = M68K_GetPage(mas, addr, false);  /* Already allocated */
        mas->codeSegments[mas->numCodeSegs] = firstPage ? (UInt8*)firstPage + (addr & (M68K_PAGE_SIZE - 1)) : NULL;
        mas->codeSegBases[mas->numCodeSegs] = addr;
        mas->codeSegSizes[mas->numCodeSegs] = len;
        handle->segIndex = mas->numCodeSegs;
        mas->numCodeSegs++;
    } else {
        DisposePtr((Ptr)handle);
        return memFullErr;
    }

    handle->hostMemory = M68K_GetPage(mas, addr, false);
    if (handle->hostMemory) {
        handle->hostMemory = (UInt8*)handle->hostMemory + (addr & (M68K_PAGE_SIZE - 1));
    }
    handle->cpuAddr = addr;
    handle->size = len;

    *outHandle = (CPUCodeHandle)handle;
    *outBase = addr;

    return noErr;
}

/*
 * UnmapExecutable - Unmap code segment
 */
static OSErr M68K_UnmapExecutable(CPUAddressSpace as, CPUCodeHandle handle)
{
    M68KCodeHandle* mhandle = (M68KCodeHandle*)handle;

    (void)as; /* Unused */

    if (!mhandle) {
        return paramErr;
    }

    /* For now, just free the handle (memory stays allocated) */
    DisposePtr((Ptr)mhandle);
    return noErr;
}

/*
 * SetRegisterA5 - Set A5 register
 */
static OSErr M68K_SetRegisterA5(CPUAddressSpace as, CPUAddr a5)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;

    if (!mas) {
        return paramErr;
    }

    mas->regs.a[5] = a5;
    return noErr;
}

/*
 * SetStacks - Configure stacks
 */
static OSErr M68K_SetStacks(CPUAddressSpace as, CPUAddr usp, CPUAddr ssp)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;

    if (!mas) {
        return paramErr;
    }

    mas->regs.usp = usp;
    mas->regs.ssp = ssp;
    mas->regs.a[7] = usp; /* A7 = USP initially */

    return noErr;
}

/*
 * InstallTrap - Install trap handler
 */
static OSErr M68K_InstallTrap(CPUAddressSpace as, TrapNumber trapNum,
                              CPUTrapHandler handler, void* context)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;
    int slot;

    if (!mas) {
        return paramErr;
    }

    slot = M68K_TrapSlot((UInt16)trapNum);
    if (slot < 0) {
        return paramErr;
    }

    mas->trapHandlers[slot] = handler;
    mas->trapContexts[slot] = context;

    return noErr;
}

/*
 * WriteJumpTableSlot - Patch jump table entry
 */
static OSErr M68K_WriteJumpTableSlot(CPUAddressSpace as, CPUAddr slotAddr,
                                     CPUAddr target)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;

    if (!mas || slotAddr >= M68K_MAX_ADDR) {
        return paramErr;
    }

    /* Write 68K JMP instruction using paged access: 0x4EF9 + 32-bit address */
    extern void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value);
    M68K_Write8(mas, slotAddr + 0, 0x4E);
    M68K_Write8(mas, slotAddr + 1, 0xF9);
    M68K_Write8(mas, slotAddr + 2, (target >> 24) & 0xFF);
    M68K_Write8(mas, slotAddr + 3, (target >> 16) & 0xFF);
    M68K_Write8(mas, slotAddr + 4, (target >> 8) & 0xFF);
    M68K_Write8(mas, slotAddr + 5, target & 0xFF);

    return noErr;
}

/*
 * MakeLazyJTStub - Create lazy-loading stub
 */
static OSErr M68K_MakeLazyJTStub(CPUAddressSpace as, CPUAddr slotAddr,
                                 SInt16 segID, SInt16 entryIndex)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;

    if (!mas || slotAddr >= M68K_MAX_ADDR) {
        return paramErr;
    }

    extern void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value);

    /*
     * Create lazy stub that triggers _LoadSeg:
     *
     *   +0: 0x3F3C  MOVE.W #segID,-(SP)
     *   +2: segID (16-bit)
     *   +4: 0xA9F0  _LoadSeg trap
     *   +6: 0x4E75  RTS (return after load)
     *
     * Note: entryIndex is embedded in the trap context
     */
    M68K_Write8(mas, slotAddr + 0, 0x3F);
    M68K_Write8(mas, slotAddr + 1, 0x3C);
    M68K_Write8(mas, slotAddr + 2, (segID >> 8) & 0xFF);
    M68K_Write8(mas, slotAddr + 3, segID & 0xFF);
    M68K_Write8(mas, slotAddr + 4, 0xA9);
    M68K_Write8(mas, slotAddr + 5, 0xF0);
    M68K_Write8(mas, slotAddr + 6, 0x4E);
    M68K_Write8(mas, slotAddr + 7, 0x75);

    (void)entryIndex; /* Stored in trap handler context */

    return noErr;
}

/*
 * EnterAt - Begin execution
 */
static OSErr M68K_EnterAt(CPUAddressSpace as, CPUAddr entry, CPUEnterFlags flags)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;
    UInt32 max_instructions = 100000;  /* Safety limit */

    if (!mas) {
        return paramErr;
    }

    M68K_LOG_DEBUG("EnterAt: entry=0x%08X flags=0x%04X\n", entry, flags);

    /*
     * A program with no stack cannot run - the first thing almost any 68K
     * code does is push something. Entering anyway produced a page allocation
     * failure at whatever address a push off a zero A7 lands on, which says
     * nothing about the actual mistake. Say the actual mistake.
     */
    if (mas->regs.a[7] == 0) {
        serial_puts("[M68K] EnterAt: no stack - call SetStacks before entering\n");
        return -1;
    }

    /* Clear halted flag */
    mas->halted = false;

    /* Execute from entry point */
    M68K_Execute(mas, entry, max_instructions);

    /*
     * Report what happened. This returned noErr whether the code ran to
     * completion, faulted on an illegal instruction, or spun until the
     * instruction limit - so a caller could not tell a working program from a
     * crashed one, and the segment loader's smoke test reported success for a
     * run that had faulted on its first instruction.
     */
    if (mas->halted) {
        if (mas->lastException) {
            char b[220];
            /* The registers that decide where a program goes: A5 addresses
             * the jump table and globals, A7 is the stack. A fault report
             * without them says where execution died but not why it was
             * there. */
            snprintf(b, sizeof(b),
                     "[M68K] %s at PC=0x%08X (A5=0x%08X A7=0x%08X D0=0x%08X)\n",
                     mas->faultReason ? mas->faultReason : "fault",
                     (unsigned)mas->faultPC,
                     (unsigned)mas->regs.a[5], (unsigned)mas->regs.a[7],
                     (unsigned)mas->regs.d[0]);
            serial_puts(b);
            return -1;
        }
        M68K_LOG_INFO("Execution halted at PC=0x%08X\n", mas->regs.pc);
        return noErr;
    }

    serial_puts("[M68K] ran the instruction limit without halting\n");
    return -1;
}

/*
 * Relocate - Apply relocations
 */
static OSErr M68K_Relocate(CPUAddressSpace as, CPUCodeHandle code,
                           const RelocTable* relocs, CPUAddr segBase,
                           CPUAddr jtBase, CPUAddr a5Base)
{
    M68KCodeHandle* mhandle = (M68KCodeHandle*)code;
    M68KAddressSpace* mas = (M68KAddressSpace*)as;
    UInt8* codeData;
    const char* kindName;

    if (!mas || !mhandle || !relocs) {
        return paramErr;
    }

    codeData = (UInt8*)mhandle->hostMemory;
    if (!codeData) {
        serial_printf("[RELOC] ERROR: hostMemory is NULL\n");
        return paramErr;
    }

    serial_printf("[RELOC] Applying %d relocations to segment at 0x%08X\n",
                  relocs->count, segBase);

    /* Apply each relocation */
    for (UInt16 i = 0; i < relocs->count; i++) {
        const RelocEntry* reloc = &relocs->entries[i];
        UInt32 offset = reloc->atOffset;
        UInt32 value = 0;
        SInt32 pcrel_offset;
        UInt32 patch_pc;

        if (offset + 4 > mhandle->size) {
            serial_printf("[RELOC] ERROR: offset 0x%X exceeds segment size 0x%X\n",
                         offset, mhandle->size);
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
                /* Patch A5-relative offset */
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
                /* Check for overflow: jtIndex * 8 + jtBase */
                {
                    UInt32 jtOffset = (UInt32)reloc->jtIndex * 8;
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
                /* PC-relative 16-bit branch/call (68K BRA, Bcc, BSR) */
                kindName = "PC_REL16";
                /* PC points to instruction AFTER the displacement word */
                patch_pc = segBase + offset + 2;
                /* Calculate target address */
                value = segBase + reloc->addend;
                /* Calculate PC-relative offset */
                pcrel_offset = (SInt32)value - (SInt32)patch_pc;
                /* Check 16-bit signed range */
                if (pcrel_offset < -32768 || pcrel_offset > 32767) {
                    serial_printf("[RELOC] ERROR: PC_REL16 out of range: offset=%d\n", pcrel_offset);
                    return segmentRelocErr;
                }
                /* Patch as big-endian 16-bit */
                codeData[offset + 0] = (pcrel_offset >> 8) & 0xFF;
                codeData[offset + 1] = pcrel_offset & 0xFF;
                serial_printf("[RELOC] apply kind=%s at off=0x%X -> disp=%+d (target=0x%08X PC=0x%08X)\n",
                             kindName, offset, pcrel_offset, value, patch_pc);
                break;

            case kRelocPCRel32:
                /* PC-relative 32-bit (rare on 68K, more common on PPC) */
                kindName = "PC_REL32";
                patch_pc = segBase + offset + 4;
                value = segBase + reloc->addend;
                pcrel_offset = (SInt32)value - (SInt32)patch_pc;
                codeData[offset + 0] = (pcrel_offset >> 24) & 0xFF;
                codeData[offset + 1] = (pcrel_offset >> 16) & 0xFF;
                codeData[offset + 2] = (pcrel_offset >> 8) & 0xFF;
                codeData[offset + 3] = pcrel_offset & 0xFF;
                serial_printf("[RELOC] apply kind=%s at off=0x%X -> disp=%+d (target=0x%08X PC=0x%08X)\n",
                             kindName, offset, pcrel_offset, value, patch_pc);
                break;

            case kRelocSegmentRef:
                /* Reference to another segment (for cross-segment calls/data) */
                kindName = "SEG_REF";
                /* For now, treat as absolute (would need segment table lookup) */
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
static OSErr M68K_AllocateMemory(CPUAddressSpace as, Size size,
                                 CPUMapFlags flags, CPUAddr* outAddr)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;
    UInt32 addr;

    if (!mas || !outAddr) {
        return paramErr;
    }

    /* Find free space (simple bump allocator) */
    addr = 0x10000; /* Start at 64K */
    for (int i = 0; i < mas->numCodeSegs; i++) {
        UInt32 end = mas->codeSegBases[i] + mas->codeSegSizes[i];
        if (end > addr) {
            addr = end;
        }
    }

    /* Align to 16-byte boundary */
    addr = (addr + 15) & ~15;

    /* Check bounds */
    if (addr + size > M68K_MAX_ADDR) {
        return memFullErr;
    }

    /* Zero memory */
    for (Size i = 0; i < size; i++) { extern void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value); M68K_Write8(mas, addr + i, 0); }

    *outAddr = addr;

    (void)flags; /* Unused for now */

    return noErr;
}

/*
 * WriteMemory - Write to CPU address space
 */
static OSErr M68K_WriteMemory(CPUAddressSpace as, CPUAddr addr,
                              const void* data, Size len)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;

    if (!mas || !data || addr + len > M68K_MAX_ADDR) {
        return paramErr;
    }

    { extern OSErr M68K_MemCopy(M68KAddressSpace* as, UInt32 addr, const void* src, Size len); return M68K_MemCopy(mas, addr, data, len); }
    return noErr;
}

/*
 * ReadMemory - Read from CPU address space
 */
static OSErr M68K_ReadMemory(CPUAddressSpace as, CPUAddr addr,
                             void* data, Size len)
{
    M68KAddressSpace* mas = (M68KAddressSpace*)as;

    if (!mas || !data || addr + len > M68K_MAX_ADDR) {
        return paramErr;
    }

    { extern UInt8 M68K_Read8(M68KAddressSpace* as, UInt32 addr); for (Size i = 0; i < len; i++) { ((UInt8*)data)[i] = M68K_Read8(mas, addr + i); } }
    return noErr;
}

/*
 * Opcode Handler Declarations (from M68KOpcodes.c)
 */
extern void M68K_Op_MOVE(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVEA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_LEA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_PEA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CLR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_NOT(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ADD(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUB(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CMP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_LINK(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_UNLK(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_JSR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_JMP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BRA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BSR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_Bcc(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_RTS(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_RTE(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_STOP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_Scc(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_DBcc(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_TRAP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVEQ(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_TST(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EXT(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SWAP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ADDQ(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUBQ(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_AND(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_OR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EOR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_NOP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ADDA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUBA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CMPA(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVEM(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_LSL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_LSR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ASL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ASR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MULU(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MULS(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_DIVU(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_DIVS(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BTST(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BSET(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BCLR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_BCHG(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ROL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ROR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_NEG(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ROXL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ROXR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ADDX(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUBX(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_NEGX(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CHK(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_TAS(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CMPI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ADDI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SUBI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ANDI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ORI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EORI(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ABCD(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_SBCD(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_NBCD(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVEP(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_CMPM(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ILLEGAL(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_RESET(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_TRAPV(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_RTR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ANDI_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ANDI_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ORI_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_ORI_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EORI_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_EORI_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVE_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVE_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVE_FROM_SR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVE_FROM_CCR(M68KAddressSpace* as, UInt16 opcode);
extern void M68K_Op_MOVE_USP(M68KAddressSpace* as, UInt16 opcode);
extern UInt16 M68K_Fetch16(M68KAddressSpace* as);
extern void M68K_Fault(M68KAddressSpace* as, const char* reason);

/*
 * M68K_Step - Fetch and execute one instruction
 */
OSErr M68K_Step(M68KAddressSpace* as)
{
    UInt16 opcode;

    if (!as) {
        return paramErr;
    }

    if (as->halted) {
        return noErr;
    }

    /* Fetch opcode */
    opcode = M68K_Fetch16(as);

    /* Decode and dispatch */
    if ((opcode & 0xF000) == 0x0000) {
        /* 0xxx - Bit manipulation, MOVEP, immediate */
        if ((opcode & 0xF1C0) == 0x0100) {
            /* BTST with register */
            M68K_Op_BTST(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x0800) {
            /* BTST with immediate */
            M68K_Op_BTST(as, opcode);
        } else if ((opcode & 0xF1C0) == 0x01C0) {
            /* BSET with register */
            M68K_Op_BSET(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x08C0) {
            /* BSET with immediate */
            M68K_Op_BSET(as, opcode);
        } else if ((opcode & 0xF1C0) == 0x0180) {
            /* BCLR with register */
            M68K_Op_BCLR(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x0880) {
            /* BCLR with immediate */
            M68K_Op_BCLR(as, opcode);
        } else if ((opcode & 0xF1C0) == 0x0140) {
            /* BCHG with register */
            M68K_Op_BCHG(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x0840) {
            /* BCHG with immediate */
            M68K_Op_BCHG(as, opcode);
        } else if ((opcode & 0xFF00) == 0x0C00) {
            /* CMPI - compare immediate */
            M68K_Op_CMPI(as, opcode);
        } else if ((opcode & 0xFF00) == 0x0600) {
            /* ADDI - add immediate */
            M68K_Op_ADDI(as, opcode);
        } else if ((opcode & 0xFF00) == 0x0400) {
            /* SUBI - subtract immediate */
            M68K_Op_SUBI(as, opcode);
        } else if ((opcode & 0xFF00) == 0x0200) {
            /* ANDI - AND immediate */
            M68K_Op_ANDI(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x003C) {
            /* ORI to CCR */
            M68K_Op_ORI_CCR(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x007C) {
            /* ORI to SR */
            M68K_Op_ORI_SR(as, opcode);
        } else if ((opcode & 0xFF00) == 0x0000) {
            /* ORI - OR immediate */
            M68K_Op_ORI(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x023C) {
            /* ANDI to CCR */
            M68K_Op_ANDI_CCR(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x027C) {
            /* ANDI to SR */
            M68K_Op_ANDI_SR(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x0A3C) {
            /* EORI to CCR */
            M68K_Op_EORI_CCR(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x0A7C) {
            /* EORI to SR */
            M68K_Op_EORI_SR(as, opcode);
        } else if ((opcode & 0xFF00) == 0x0A00) {
            /* EORI - EOR immediate */
            M68K_Op_EORI(as, opcode);
        } else if ((opcode & 0xF1F8) == 0x0108) {
            /* MOVEP - move peripheral data */
            M68K_Op_MOVEP(as, opcode);
        } else if ((opcode & 0xFF00) == 0x4200) {
            M68K_Op_CLR(as, opcode);
        } else if ((opcode & 0xFF00) == 0x4600) {
            M68K_Op_NOT(as, opcode);
        } else if ((opcode & 0xC000) == 0x0000) {
            /* Could be MOVE with size bits 01/10/11 */
            M68K_Op_MOVE(as, opcode);
        } else {
            M68K_Fault(as, "Unimplemented 0xxx opcode");
        }
    } else if ((opcode & 0xF000) == 0x1000 || (opcode & 0xF000) == 0x2000 ||
               (opcode & 0xF000) == 0x3000 || (opcode & 0xF000) == 0x4000) {
        /*
         * MOVE.B/.W/.L (0x1000, 0x3000, 0x2000) and the 0x4xxx group.
         *
         * The test used to be (opcode & 0xC000) against 0x0000, 0x4000 and
         * 0x8000 - three quarters of the whole opcode space. Everything from
         * 0x4000 to 0xBFFF landed here, so every branch below it was
         * unreachable: MOVEQ, ADDQ and SUBQ, Bcc and BRA and BSR, OR and DIV,
         * SUB, CMP, and the A-line traps a Macintosh program uses to call the
         * Toolbox at all. Worse than unreached - the body reads bits 13-12 as
         * a MOVE size, and for MOVEQ (0x7xxx) that reads as 3, so a MOVEQ was
         * executed as a MOVE.W of whatever those bits happened to address.
         *
         * The body only ever meant the MOVE sizes and the 0x4xxx group, which
         * is what it now says.
         */
        /* MOVE family - check for size bits in upper nibble */
        UInt8 size_bits = (opcode >> 12) & 3;
        if (size_bits == 1 || size_bits == 2 || size_bits == 3) {
            /* MOVE.B (01), MOVE.L (10), MOVE.W (11) */
            if ((opcode & 0x01C0) == 0x0040) {
                /* MOVEA - bit 6 set */
                M68K_Op_MOVEA(as, opcode);
            } else {
                M68K_Op_MOVE(as, opcode);
            }
        } else if ((opcode & 0xF1C0) == 0x41C0) {
            /* LEA */
            M68K_Op_LEA(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x4840) {
            /* PEA */
            M68K_Op_PEA(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x4E80) {
            /* JSR */
            M68K_Op_JSR(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x4EC0) {
            /* JMP */
            M68K_Op_JMP(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x4E75) {
            /* RTS */
            M68K_Op_RTS(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x4E73) {
            /* RTE */
            M68K_Op_RTE(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x4E72) {
            /* STOP */
            M68K_Op_STOP(as, opcode);
        } else if ((opcode & 0xFFF8) == 0x4E50) {
            /* LINK */
            M68K_Op_LINK(as, opcode);
        } else if ((opcode & 0xFFF8) == 0x4E58) {
            /* UNLK */
            M68K_Op_UNLK(as, opcode);
        } else if ((opcode & 0xFF00) == 0x4200) {
            /* CLR */
            M68K_Op_CLR(as, opcode);
        } else if ((opcode & 0xFF00) == 0x4600) {
            /* NOT */
            M68K_Op_NOT(as, opcode);
        } else if ((opcode & 0xFF00) == 0x4A00) {
            /* TST */
            M68K_Op_TST(as, opcode);
        } else if ((opcode & 0xFFF8) == 0x4840) {
            /* SWAP */
            M68K_Op_SWAP(as, opcode);
        } else if ((opcode & 0xFFF8) == 0x4880 || (opcode & 0xFFF8) == 0x48C0) {
            /* EXT.W or EXT.L */
            M68K_Op_EXT(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x4E71) {
            /* NOP */
            M68K_Op_NOP(as, opcode);
        } else if ((opcode & 0xFB80) == 0x4880) {
            /* MOVEM */
            M68K_Op_MOVEM(as, opcode);
        } else if ((opcode & 0xFF00) == 0x4400) {
            /* NEG */
            M68K_Op_NEG(as, opcode);
        } else if ((opcode & 0xFF00) == 0x4000) {
            /* NEGX - negate with extend */
            M68K_Op_NEGX(as, opcode);
        } else if ((opcode & 0xF1C0) == 0x4180) {
            /* CHK - check register against bounds */
            M68K_Op_CHK(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x4AC0) {
            /* TAS - test and set */
            M68K_Op_TAS(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x4800) {
            /* NBCD - negate decimal with extend */
            M68K_Op_NBCD(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x4AFC) {
            /* ILLEGAL */
            M68K_Op_ILLEGAL(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x4E70) {
            /* RESET */
            M68K_Op_RESET(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x4E76) {
            /* TRAPV */
            M68K_Op_TRAPV(as, opcode);
        } else if ((opcode & 0xFFFF) == 0x4E77) {
            /* RTR */
            M68K_Op_RTR(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x44C0) {
            /* MOVE to CCR */
            M68K_Op_MOVE_CCR(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x46C0) {
            /* MOVE to SR */
            M68K_Op_MOVE_SR(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x40C0) {
            /* MOVE from SR */
            M68K_Op_MOVE_FROM_SR(as, opcode);
        } else if ((opcode & 0xFFC0) == 0x42C0) {
            /* MOVE from CCR (undocumented on 68000, official in 68010+) */
            M68K_Op_MOVE_FROM_CCR(as, opcode);
        } else if ((opcode & 0xFFF8) == 0x4E60) {
            /* MOVE USP */
            M68K_Op_MOVE_USP(as, opcode);
        } else {
            M68K_Fault(as, "Unimplemented 4xxx opcode");
        }
    } else if ((opcode & 0xF000) == 0x5000) {
        /* 5xxx - Scc, DBcc, ADDQ, SUBQ */
        if ((opcode & 0xF0C0) == 0x50C0) {
            /* Scc or DBcc - both have 0101 cccc 11xx xxxx pattern */
            if ((opcode & 0x0038) == 0x0008) {
                /* DBcc - register mode (bits 5-3 = 001) */
                M68K_Op_DBcc(as, opcode);
            } else {
                /* Scc - other modes */
                M68K_Op_Scc(as, opcode);
            }
        } else {
            /* ADDQ or SUBQ */
            if ((opcode & 0x0100) == 0x0000) {
                /* ADDQ - bit 8 = 0 */
                M68K_Op_ADDQ(as, opcode);
            } else {
                /* SUBQ - bit 8 = 1 */
                M68K_Op_SUBQ(as, opcode);
            }
        }
    } else if ((opcode & 0xF000) == 0x7000) {
        /* 7xxx - MOVEQ */
        M68K_Op_MOVEQ(as, opcode);
    } else if ((opcode & 0xF000) == 0x6000) {
        /* 6xxx - Branch instructions */
        if ((opcode & 0xFF00) == 0x6000) {
            M68K_Op_BRA(as, opcode);
        } else if ((opcode & 0xFF00) == 0x6100) {
            M68K_Op_BSR(as, opcode);
        } else {
            M68K_Op_Bcc(as, opcode);
        }
    } else if ((opcode & 0xF000) == 0x8000) {
        /* 8xxx - OR/DIVU/DIVS/SBCD */
        if ((opcode & 0x01C0) == 0x00C0) {
            /* DIVU - bits 8-6 = 011 */
            M68K_Op_DIVU(as, opcode);
        } else if ((opcode & 0x01C0) == 0x01C0) {
            /* DIVS - bits 8-6 = 111 */
            M68K_Op_DIVS(as, opcode);
        } else if ((opcode & 0xF1F0) == 0x8100) {
            /* SBCD - subtract decimal with extend */
            M68K_Op_SBCD(as, opcode);
        } else {
            /* OR */
            M68K_Op_OR(as, opcode);
        }
    } else if ((opcode & 0xF000) == 0x9000) {
        /* 9xxx - SUB/SUBA/SUBX */
        if ((opcode & 0x00C0) == 0x00C0) {
            /* SUBA - bits 7-6 = 11 */
            M68K_Op_SUBA(as, opcode);
        } else if ((opcode & 0xF130) == 0x9100) {
            /* SUBX - bits 8 = 1, bits 5-4 = 00 */
            M68K_Op_SUBX(as, opcode);
        } else {
            /* SUB */
            M68K_Op_SUB(as, opcode);
        }
    } else if ((opcode & 0xF000) == 0xA000) {
        /* Axxx - A-line trap */
        M68K_Op_TRAP(as, opcode);
    } else if ((opcode & 0xF100) == 0xB000) {
        /* Bxxx - CMP/CMPA/EOR/CMPM */
        if ((opcode & 0x00C0) == 0x00C0) {
            /* CMPA - bits 7-6 = 11 */
            M68K_Op_CMPA(as, opcode);
        } else if ((opcode & 0xF138) == 0xB108) {
            /* CMPM - compare memory to memory */
            M68K_Op_CMPM(as, opcode);
        } else if ((opcode & 0x0100) == 0x0100) {
            /* EOR - bit 8 = 1 */
            M68K_Op_EOR(as, opcode);
        } else {
            /* CMP */
            M68K_Op_CMP(as, opcode);
        }
    } else if ((opcode & 0xF000) == 0xD000) {
        /* Dxxx - ADD/ADDA/ADDX */
        if ((opcode & 0x00C0) == 0x00C0) {
            /* ADDA - bits 7-6 = 11 */
            M68K_Op_ADDA(as, opcode);
        } else if ((opcode & 0xF130) == 0xD100) {
            /* ADDX - bits 8 = 1, bits 5-4 = 00 */
            M68K_Op_ADDX(as, opcode);
        } else {
            /* ADD */
            M68K_Op_ADD(as, opcode);
        }
    } else if ((opcode & 0xF000) == 0xC000) {
        /* Cxxx - AND/MULU/MULS/ABCD */
        if ((opcode & 0x01C0) == 0x00C0) {
            /* MULU - bits 8-6 = 011 */
            M68K_Op_MULU(as, opcode);
        } else if ((opcode & 0x01C0) == 0x01C0) {
            /* MULS - bits 8-6 = 111 */
            M68K_Op_MULS(as, opcode);
        } else if ((opcode & 0xF1F0) == 0xC100) {
            /* ABCD - add decimal with extend */
            M68K_Op_ABCD(as, opcode);
        } else {
            /* AND */
            M68K_Op_AND(as, opcode);
        }
    } else if ((opcode & 0xF000) == 0xE000) {
        /*
         * Exxx - shift and rotate.
         *
         * Bits 4-3 say which family and bit 8 says which direction:
         *
         *     00 arithmetic   01 logical   10 rotate with extend   11 rotate
         *     bit 8: 0 = right, 1 = left
         *
         * This read bits 4-3 correctly and then paired them with the wrong
         * instructions - type 2 called ASL where 2 is the extend rotate, type
         * 3 called ROXL/ROXR where 3 is the plain rotate, and both the type 0
         * and type 1 branches decided rotate-versus-shift by re-testing the
         * same bits they had already switched on, which is always true. So a
         * plain LSL never ran: it was dispatched to ROL, and LSR, ASL, ROXL
         * and ROXR were unreachable.
         */
        UInt8 type = (opcode >> 3) & 3;
        Boolean left = (opcode & 0x0100) != 0;

        switch (type) {
        case 0:
            if (left) M68K_Op_ASL(as, opcode); else M68K_Op_ASR(as, opcode);
            break;
        case 1:
            if (left) M68K_Op_LSL(as, opcode); else M68K_Op_LSR(as, opcode);
            break;
        case 2:
            if (left) M68K_Op_ROXL(as, opcode); else M68K_Op_ROXR(as, opcode);
            break;
        default:
            if (left) M68K_Op_ROL(as, opcode); else M68K_Op_ROR(as, opcode);
            break;
        }
    } else {
        serial_printf("[M68K] ILLEGAL opcode 0x%04X at PC=0x%08X\n", opcode, as->regs.pc - 2);
        M68K_Fault(as, "Illegal opcode");
    }

    return noErr;
}

/*
 * M68K_Execute - Execute up to maxInstructions
 */
OSErr M68K_Execute(M68KAddressSpace* as, UInt32 startPC, UInt32 maxInstructions)
{
    UInt32 count = 0;

    if (!as) {
        return paramErr;
    }

    as->regs.pc = startPC;
    as->halted = false;

    while (count < maxInstructions && !as->halted) {
        M68K_Step(as);
        count++;
    }

    return noErr;
}

/*
 * M68K_SelfTest - run programs whose results are not in doubt.
 *
 * Nothing in the system executes 68K code yet, so nothing notices when the
 * interpreter is wrong. Two faults that had been present from the start were
 * found by running five instructions; these cases widen that to the parts a
 * real program leans on - memory operands, condition flags, branches - and
 * each is small enough that its expected result can be read off the manual.
 *
 * Adding a case is one table entry. The runner says nothing unless a case
 * comes out wrong.
 */

/* reg 0-7 are D0-D7, 8-15 are A0-A7 */
typedef struct { UInt8 reg; UInt32 value; } M68KExpect;

typedef struct {
    const char*       name;
    const UInt8*      code;
    UInt16            codeLen;
    UInt16            steps;
    const M68KExpect* expect;
    UInt16            expectCount;
    UInt32            expectPCOffset;   /* from the load address */
} M68KTestCase;

/* MOVEQ #$42,D0; ADDQ.L #1,D0; MOVE.L D0,D1; LSL.L #2,D1; SUBQ.L #3,D1 */
static const UInt8 kProgArith[] = {
    0x70, 0x42,  0x52, 0x80,  0x22, 0x00,  0xE5, 0x89,  0x57, 0x81,
};
static const M68KExpect kWantArith[] = { {0, 0x43}, {1, 0x109} };

/* MOVE.L #$12345678,D0; MOVEA.L #$00020000,A0; MOVE.L D0,(A0); MOVE.L (A0),D1 */
static const UInt8 kProgAddr[] = {
    0x20, 0x3C, 0x12, 0x34, 0x56, 0x78,
    0x20, 0x7C, 0x00, 0x02, 0x00, 0x00,
    0x20, 0x80,
    0x22, 0x10,
};
static const M68KExpect kWantAddr[] = {
    {0, 0x12345678}, {1, 0x12345678}, {8, 0x00020000},
};

/* MOVEQ #5,D0; SUBQ.L #5,D0; BEQ +2; MOVEQ #$7F,D1; MOVEQ #1,D2 */
static const UInt8 kProgBranch[] = {
    0x70, 0x05,
    0x5B, 0x80,
    0x67, 0x02,
    0x72, 0x7F,
    0x74, 0x01,
};
static const M68KExpect kWantBranch[] = { {0, 0}, {1, 0}, {2, 1} };


/* MOVE.L #$FFFFFFFF,D0; MOVEQ #0,D1; MOVE.B D0,D1; MOVEQ #0,D2; MOVE.W D0,D2
 * A byte or word move touches only that much of the destination register. */
static const UInt8 kProgSizes[] = {
    0x20, 0x3C, 0xFF, 0xFF, 0xFF, 0xFF,
    0x72, 0x00,
    0x12, 0x00,
    0x74, 0x00,
    0x34, 0x00,
};
static const M68KExpect kWantSizes[] = {
    {0, 0xFFFFFFFF}, {1, 0x000000FF}, {2, 0x0000FFFF},
};

/* MOVEA.L #$20000,A0; MOVE.L #$AABBCCDD,D0; MOVE.L D0,(A0)+;
 * MOVE.L #$11223344,D0; MOVE.L D0,(A0)+; MOVE.L -(A0),D1 */
static const UInt8 kProgIncr[] = {
    0x20, 0x7C, 0x00, 0x02, 0x00, 0x00,
    0x20, 0x3C, 0xAA, 0xBB, 0xCC, 0xDD,
    0x20, 0xC0,
    0x20, 0x3C, 0x11, 0x22, 0x33, 0x44,
    0x20, 0xC0,
    0x22, 0x20,
};
static const M68KExpect kWantIncr[] = {
    {1, 0x11223344}, {8, 0x00020004},
};


/* MOVEA.L #$30000,A7; BSR.S +4; MOVEQ #2,D5; NOP; MOVEQ #3,D3; RTS
 *
 * The subroutine at +$C sets D3 and returns; the return lands on the MOVEQ
 * that sets D5. A7 must come back to where it started, which is what says
 * the return address was pushed and popped rather than merely jumped over. */
static const UInt8 kProgCall[] = {
    0x2E, 0x7C, 0x00, 0x03, 0x00, 0x00,   /* MOVEA.L #$00030000,A7 */
    0x61, 0x04,                           /* BSR.S   +4            */
    0x7A, 0x02,                           /* MOVEQ   #2,D5         */
    0x4E, 0x71,                           /* NOP                   */
    0x76, 0x03,                           /* MOVEQ   #3,D3         */
    0x4E, 0x75,                           /* RTS                   */
};
static const M68KExpect kWantCall[] = {
    {3, 3}, {5, 2}, {15, 0x00030000},     /* D3, D5, A7 */
};


/* MOVEM saves and restores registers around a call - it is in the prologue of
 * almost every compiled function. The mask is written backwards for the
 * predecrement form, which is the part worth checking. */
static const UInt8 kProgMovem[] = {
    0x2E, 0x7C, 0x00, 0x03, 0x00, 0x00,   /* MOVEA.L #$00030000,A7   */
    0x70, 0x11,                           /* MOVEQ   #$11,D0         */
    0x72, 0x22,                           /* MOVEQ   #$22,D1         */
    0x24, 0x7C, 0x00, 0x04, 0x00, 0x00,   /* MOVEA.L #$00040000,A2   */
    0x48, 0xE7, 0xC0, 0x20,               /* MOVEM.L D0-D1/A2,-(A7)  */
    0x70, 0x00,                           /* MOVEQ   #0,D0           */
    0x72, 0x00,                           /* MOVEQ   #0,D1           */
    0x24, 0x7C, 0x00, 0x00, 0x00, 0x00,   /* MOVEA.L #0,A2           */
    0x4C, 0xDF, 0x04, 0x03,               /* MOVEM.L (A7)+,D0-D1/A2  */
};
/* The list spans both register files, which is where the reversed mask of the
 * predecrement form and the plain mask of the postincrement form have to
 * agree about which register is which. */
static const M68KExpect kWantMovem[] = {
    {0, 0x11}, {1, 0x22}, {10, 0x00040000}, {15, 0x00030000},
};

/* MOVE.L #100,D0; MOVEQ #7,D1; DIVU D1,D0; MOVEQ #6,D2; MOVEQ #7,D3; MULU D3,D2
 * DIVU leaves the quotient in the low word and the remainder in the high one:
 * 100 / 7 is 14 remainder 2. */
static const UInt8 kProgMulDiv[] = {
    0x20, 0x3C, 0x00, 0x00, 0x00, 0x64,   /* MOVE.L #100,D0 */
    0x72, 0x07,                           /* MOVEQ  #7,D1   */
    0x80, 0xC1,                           /* DIVU   D1,D0   */
    0x74, 0x06,                           /* MOVEQ  #6,D2   */
    0x76, 0x07,                           /* MOVEQ  #7,D3   */
    0xC4, 0xC3,                           /* MULU   D3,D2   */
};
static const M68KExpect kWantMulDiv[] = {
    {0, 0x0002000E}, {2, 42},
};


/* Displacement and indexed operands - d16(An) and d8(An,Dn.L).
 * Both reach the same longword, written once and read back two ways. */
static const UInt8 kProgDisp[] = {
    0x20, 0x7C, 0x00, 0x02, 0x00, 0x00,   /* MOVEA.L #$00020000,A0     */
    0x20, 0x3C, 0xDE, 0xAD, 0xBE, 0xEF,   /* MOVE.L  #$DEADBEEF,D0     */
    0x21, 0x40, 0x00, 0x10,               /* MOVE.L  D0,$10(A0)        */
    0x22, 0x28, 0x00, 0x10,               /* MOVE.L  $10(A0),D1        */
    0x74, 0x04,                           /* MOVEQ   #4,D2             */
    0x26, 0x30, 0x28, 0x0C,               /* MOVE.L  $0C(A0,D2.L),D3   */
};
static const M68KExpect kWantDisp[] = {
    {1, 0xDEADBEEF}, {3, 0xDEADBEEF}, {8, 0x00020000},
};

/* LINK builds a stack frame and UNLK takes it down again; compiled code puts
 * one around any function with locals. A7 must come back to where it started
 * and A6 must be restored to what it held before. */
static const UInt8 kProgLink[] = {
    0x2E, 0x7C, 0x00, 0x03, 0x00, 0x00,   /* MOVEA.L #$00030000,A7 */
    0x4E, 0x56, 0xFF, 0xF8,               /* LINK    A6,#-8        */
    0x70, 0x09,                           /* MOVEQ   #9,D0         */
    0x4E, 0x5E,                           /* UNLK    A6            */
};
static const M68KExpect kWantLink[] = {
    {0, 9}, {14, 0}, {15, 0x00030000},    /* D0, A6, A7 */
};

/* DBRA counts the low word of the register down past zero, which is how every
 * 68K loop ends: four passes leave the counter at $FFFF, not at zero. */
static const UInt8 kProgLoop[] = {
    0x70, 0x03,                           /* MOVEQ #3,D0      */
    0x72, 0x00,                           /* MOVEQ #0,D1      */
    0x52, 0x81,                           /* ADDQ.L #1,D1     */
    0x51, 0xC8, 0xFF, 0xFC,               /* DBRA  D0,-4      */
};
static const M68KExpect kWantLoop[] = {
    {0, 0x0000FFFF}, {1, 4},
};


/* A comparison of -1 against 1 separates the signed and unsigned conditions:
 * signed it is less-than, unsigned it is not, and the two branches must
 * disagree. Getting this wrong makes loops and bounds checks run backwards. */
static const UInt8 kProgCompare[] = {
    0x70, 0xFF,                           /* MOVEQ #-1,D0   */
    0x72, 0x01,                           /* MOVEQ #1,D1    */
    0xB0, 0x81,                           /* CMP.L D1,D0    */
    0x6D, 0x04,                           /* BLT   +4  (taken)     */
    0x74, 0x03,                           /* MOVEQ #3,D2 (skipped) */
    0x4E, 0x71,                           /* NOP            */
    0x65, 0x02,                           /* BCS   +2  (not taken) */
    0x76, 0x05,                           /* MOVEQ #5,D3    */
    0x78, 0x07,                           /* MOVEQ #7,D4    */
};
static const M68KExpect kWantCompare[] = {
    {2, 0}, {3, 5}, {4, 7},
};

/* Adding one to the largest positive long overflows into the most negative,
 * which is what V is for. */
static const UInt8 kProgOverflow[] = {
    0x20, 0x3C, 0x7F, 0xFF, 0xFF, 0xFF,   /* MOVE.L #$7FFFFFFF,D0 */
    0x52, 0x80,                           /* ADDQ.L #1,D0         */
    0x69, 0x02,                           /* BVS    +2 (taken)    */
    0x4E, 0x71,                           /* NOP       (skipped)  */
    0x7A, 0x01,                           /* MOVEQ  #1,D5         */
};
static const M68KExpect kWantOverflow[] = {
    {0, 0x80000000}, {5, 1},
};

static const M68KTestCase kM68KTests[] = {
    { "arithmetic", kProgArith,  sizeof(kProgArith),  5,
      kWantArith,  2, sizeof(kProgArith) },
    { "addressing", kProgAddr,   sizeof(kProgAddr),   4,
      kWantAddr,   3, sizeof(kProgAddr) },
    { "flags and branch", kProgBranch, sizeof(kProgBranch), 4,
      kWantBranch, 3, sizeof(kProgBranch) },
    { "operand sizes", kProgSizes, sizeof(kProgSizes), 5,
      kWantSizes, 3, sizeof(kProgSizes) },
    { "increment addressing", kProgIncr, sizeof(kProgIncr), 6,
      kWantIncr, 2, sizeof(kProgIncr) },
    /* Five steps land on the MOVEQ after the call, at offset $A. */
    { "call and return", kProgCall, sizeof(kProgCall), 5,
      kWantCall, 3, 0x0A },
    { "movem", kProgMovem, sizeof(kProgMovem), 9,
      kWantMovem, 4, sizeof(kProgMovem) },
    { "multiply and divide", kProgMulDiv, sizeof(kProgMulDiv), 6,
      kWantMulDiv, 2, sizeof(kProgMulDiv) },
    { "displacement and index", kProgDisp, sizeof(kProgDisp), 6,
      kWantDisp, 3, sizeof(kProgDisp) },
    { "link and unlink", kProgLink, sizeof(kProgLink), 4,
      kWantLink, 3, sizeof(kProgLink) },
    /* Two setup instructions, then four passes of body-and-branch. */
    { "dbra loop", kProgLoop, sizeof(kProgLoop), 10,
      kWantLoop, 2, sizeof(kProgLoop) },
    { "signed and unsigned compare", kProgCompare, sizeof(kProgCompare), 7,
      kWantCompare, 3, sizeof(kProgCompare) },
    { "overflow", kProgOverflow, sizeof(kProgOverflow), 4,
      kWantOverflow, 2, sizeof(kProgOverflow) },
};


/* The A-line traps the test installs, and proof of which one ran. */
static Boolean gM68KTrapFired = false;

static OSErr M68K_TestTrapHandler(void* context, CPUAddr* pc, CPUAddr* registers)
{
    (void)context;
    (void)pc;                      /* leave the PC where the trap left it */
    gM68KTrapFired = true;
    registers[3] = 0x5A5A5A5A;     /* a handler can change the registers */
    return noErr;
}

/* Installed on a second Toolbox trap sharing the first one's low byte. */
static OSErr M68K_TestOtherTrapHandler(void* context, CPUAddr* pc, CPUAddr* registers)
{
    (void)context;
    (void)pc;
    registers[4] = 0xC3C3C3C3;
    return noErr;
}

/*
 * A-line traps are how a Macintosh program calls the Toolbox: the opcode is
 * not an instruction at all, it is a request. Nothing in this system issues
 * one yet, so this checks the path exists - that an $Axxx opcode reaches the
 * installed handler and that what the handler does to the registers sticks.
 */
static void M68K_SelfTestTrap(const ICPUBackend* be, UInt32 base)
{
    extern void serial_puts(const char*);

    /* MOVEQ #7,D0 ; $A9FF ; $A8FF
     *
     * The two traps share a low byte and differ only above it. They used to
     * be the same table slot, so the second would have run the first's
     * handler. */
    static const UInt8 prog[] = { 0x70, 0x07, 0xA9, 0xFF, 0xA8, 0xFF };

    CPUAddressSpace as = NULL;
    M68KAddressSpace* mas;
    char b[140];

    if (be->CreateAddressSpace(NULL, &as) != noErr || !as) return;

    gM68KTrapFired = false;

    if (be->WriteMemory(as, base, prog, sizeof(prog)) != noErr ||
        be->InstallTrap(as, 0xA9FF, M68K_TestTrapHandler, NULL) != noErr ||
        be->InstallTrap(as, 0xA8FF, M68K_TestOtherTrapHandler, NULL) != noErr) {
        serial_puts("[M68K] a-line trap: could not set up\n");
        be->DestroyAddressSpace(as);
        return;
    }

    mas = (M68KAddressSpace*)as;
    mas->regs.pc = base;
    mas->halted = false;

    M68K_Step(mas);   /* MOVEQ */
    M68K_Step(mas);   /* $A9FF */
    M68K_Step(mas);   /* $A8FF */

    if (!gM68KTrapFired) {
        serial_puts("[M68K] a-line trap FAILED: handler never ran\n");
    } else if (mas->regs.d[0] != 7 ||
               mas->regs.d[3] != 0x5A5A5A5A ||
               mas->regs.d[4] != 0xC3C3C3C3) {
        snprintf(b, sizeof(b),
                 "[M68K] a-line trap FAILED: D0=%08x D3=%08x D4=%08x"
                 " (want 00000007 5a5a5a5a c3c3c3c3)\n",
                 (unsigned)mas->regs.d[0], (unsigned)mas->regs.d[3],
                 (unsigned)mas->regs.d[4]);
        serial_puts(b);
    }

    be->DestroyAddressSpace(as);
}

void M68K_SelfTest(void)
{
    extern void serial_puts(const char*);

    const UInt32 base = 0x10000;
    const ICPUBackend* be = CPUBackend_GetDefault();
    char b[160];

    if (!be) {
        serial_puts("[M68K] self-test: no backend\n");
        return;
    }

    for (unsigned t = 0; t < sizeof(kM68KTests) / sizeof(kM68KTests[0]); t++) {
        const M68KTestCase* tc = &kM68KTests[t];
        CPUAddressSpace as = NULL;
        M68KAddressSpace* mas;
        Boolean ok = true;

        if (be->CreateAddressSpace(NULL, &as) != noErr || !as) {
            snprintf(b, sizeof(b), "[M68K] %s: no address space\n", tc->name);
            serial_puts(b);
            continue;
        }

        if (be->WriteMemory(as, base, tc->code, tc->codeLen) != noErr) {
            snprintf(b, sizeof(b), "[M68K] %s: could not load\n", tc->name);
            serial_puts(b);
            be->DestroyAddressSpace(as);
            continue;
        }

        mas = (M68KAddressSpace*)as;
        mas->regs.pc = base;
        mas->halted = false;

        for (UInt16 i = 0; i < tc->steps; i++) {
            if (M68K_Step(mas) != noErr || mas->halted) {
                snprintf(b, sizeof(b), "[M68K] %s FAILED: stopped at instruction %u\n",
                         tc->name, (unsigned)i);
                serial_puts(b);
                ok = false;
                break;
            }
        }

        for (UInt16 e = 0; ok && e < tc->expectCount; e++) {
            UInt8 r = tc->expect[e].reg;
            UInt32 got = (r < 8) ? mas->regs.d[r] : mas->regs.a[r - 8];
            if (got != tc->expect[e].value) {
                snprintf(b, sizeof(b), "[M68K] %s FAILED: %c%u = %08x, want %08x\n",
                         tc->name, (r < 8) ? 'D' : 'A', (unsigned)(r & 7),
                         (unsigned)got, (unsigned)tc->expect[e].value);
                serial_puts(b);
                ok = false;
            }
        }

        if (ok && tc->expectPCOffset &&
            mas->regs.pc != base + tc->expectPCOffset) {
            snprintf(b, sizeof(b), "[M68K] %s FAILED: PC = %08x, want %08x\n",
                     tc->name, (unsigned)mas->regs.pc,
                     (unsigned)(base + tc->expectPCOffset));
            serial_puts(b);
        }

        be->DestroyAddressSpace(as);
    }

    M68K_SelfTestTrap(be, base);
}
