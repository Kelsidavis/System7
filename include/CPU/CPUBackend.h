/*
 * CPUBackend.h - Portable CPU Backend Interface for Segment Loader
 *
 * This interface abstracts all ISA-specific operations, allowing the
 * segment loader to remain completely portable across different CPU
 * architectures (68K interpreter, PPC JIT, native modules, etc.)
 *
 * Design principles:
 * - NO host ISA assumptions leak upward
 * - All addresses are opaque CPUAddr types
 * - Binary parsing is always big-endian (68K format)
 * - Relocations use ISA-neutral abstract records
 */

#ifndef CPU_BACKEND_H
#define CPU_BACKEND_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque Types - ISA-specific internals hidden from segment loader
 */

/* Opaque handle to CPU address space for a process */
typedef struct CPUAddressSpaceImpl* CPUAddressSpace;

/* Opaque handle to mapped executable code segment */
typedef struct CPUCodeHandleImpl* CPUCodeHandle;

/* CPU address - may map to 68K virtual, x86 physical, or translated address */
typedef UInt32 CPUAddr;

/*
 * CPU Mapping Flags
 */
typedef enum {
    kCPUMapExecutable  = 0x0001,  /* Code segment (vs data) */
    kCPUMapLocked      = 0x0002,  /* Pin in memory (non-purgeable) */
    kCPUMapPurgeable   = 0x0004,  /* May be unloaded on memory pressure */
    kCPUMapA5World     = 0x0008   /* Part of A5 world (below/above) */
} CPUMapFlags;

/*
 * CPU Entry Flags
 */
typedef enum {
    kEnterApp          = 0x0001,  /* Application main entry */
    kEnterTrap         = 0x0002,  /* Trap handler entry */
    kEnterSegment      = 0x0004   /* Segment lazy load entry */
} CPUEnterFlags;

/*
 * Relocation Types (ISA-neutral abstract representation)
 */
typedef enum {
    kRelocAbsSegBase   = 1,   /* Absolute segment base address */
    kRelocA5Relative   = 2,   /* A5-relative data access */
    kRelocJTImport     = 3,   /* Jump table import */
    kRelocPCRel16      = 4,   /* PC-relative 16-bit (68K) */
    kRelocPCRel32      = 5,   /* PC-relative 32-bit */
    kRelocSegmentRef   = 6    /* Reference to another segment */
} RelocKind;

/*
 * Relocation Entry (portable)
 */
typedef struct RelocEntry {
    RelocKind kind;           /* Type of relocation */
    UInt32 atOffset;          /* Offset within segment to patch */
    SInt32 addend;            /* Addend to apply */
    UInt16 targetSegment;     /* Target segment ID (if applicable) */
    UInt16 jtIndex;           /* Jump table index (if kRelocJTImport) */
} RelocEntry;

/*
 * Relocation Table (portable)
 */
typedef struct RelocTable {
    UInt16 count;
    RelocEntry* entries;
} RelocTable;

/*
 * Trap Handler Context
 */
typedef UInt16 TrapNumber;
typedef OSErr (*CPUTrapHandler)(void* context, CPUAddr* pc, CPUAddr* registers);

/*
 * ICPUBackend - CPU Backend Interface
 *
 * Each CPU backend (m68k_interp, ppc_jit, native_abi) implements this interface.
 */
typedef struct ICPUBackend {
    /*
     * CreateAddressSpace - Create CPU address space for process
     *
     * @param processHandle Process handle (opaque to backend)
     * @param out           Output address space handle
     * @return              OSErr (noErr on success)
     */
    OSErr (*CreateAddressSpace)(void* processHandle, CPUAddressSpace* out);

    /*
     * DestroyAddressSpace - Clean up address space
     */
    OSErr (*DestroyAddressSpace)(CPUAddressSpace as);

    /*
     * MapExecutable - Map executable code into process address space
     *
     * @param as            Address space
     * @param image         Pointer to code bytes (host memory)
     * @param len           Length of code
     * @param flags         Mapping flags
     * @param outHandle     Output code handle
     * @param outBase       Output base address in CPU address space
     * @return              OSErr (noErr on success)
     */
    OSErr (*MapExecutable)(CPUAddressSpace as, const void* image, Size len,
                          CPUMapFlags flags, CPUCodeHandle* outHandle,
                          CPUAddr* outBase);

    /*
     * UnmapExecutable - Unmap code segment
     */
    OSErr (*UnmapExecutable)(CPUAddressSpace as, CPUCodeHandle handle);

    /*
     * SetRegisterA5 - Set A5 register for process
     *
     * @param as            Address space
     * @param a5            A5 value (CPU address)
     * @return              OSErr
     */
    OSErr (*SetRegisterA5)(CPUAddressSpace as, CPUAddr a5);

    /*
     * SetStacks - Configure user and supervisor stacks
     *
     * @param as            Address space
     * @param usp           User stack pointer
     * @param ssp           Supervisor stack pointer (may be 0 if unused)
     * @return              OSErr
     */
    OSErr (*SetStacks)(CPUAddressSpace as, CPUAddr usp, CPUAddr ssp);

    /*
     * InstallTrap - Install trap handler for _LoadSeg, etc.
     *
     * @param as            Address space
     * @param trapNum       Trap number
     * @param handler       Handler function
     * @param context       Context pointer for handler
     * @return              OSErr
     */
    OSErr (*InstallTrap)(CPUAddressSpace as, TrapNumber trapNum,
                        CPUTrapHandler handler, void* context);

    /*
     * WriteJumpTableSlot - put a jump table entry into its loaded form
     *
     * The segment number stays in the entry so an unload can tell which
     * segment the entry belongs to without consulting anything else.
     *
     * @param as            Address space
     * @param slotAddr      Address of jump table slot
     * @param segID         Segment the entry belongs to
     * @param target        Address of the routine within the loaded segment
     * @return              OSErr
     */
    OSErr (*WriteJumpTableSlot)(CPUAddressSpace as, CPUAddr slotAddr,
                               SInt16 segID, CPUAddr target);

    /*
     * MakeLazyJTStub - put a jump table entry into its unloaded form
     *
     * Both forms are entered two bytes into the entry, so a caller jumping
     * through the table does not care whether the segment has been loaded
     * yet - which is the whole point of the jump table.
     *
     * @param as            Address space
     * @param slotAddr      Address of JT slot to fill
     * @param segID         Segment to load on first call
     * @param routineOffset Where in that segment the routine begins
     * @return              OSErr
     */
    OSErr (*MakeLazyJTStub)(CPUAddressSpace as, CPUAddr slotAddr,
                           SInt16 segID, UInt16 routineOffset);

    /*
     * EnterAt - Begin execution at address
     *
     * @param as            Address space
     * @param entry         Entry point address
     * @param flags         Entry flags
     * @return              OSErr (typically doesn't return if kEnterApp)
     */
    OSErr (*EnterAt)(CPUAddressSpace as, CPUAddr entry, CPUEnterFlags flags);

    /*
     * Relocate - Apply relocations to code segment
     *
     * @param as            Address space
     * @param code          Code handle to relocate
     * @param relocs        Relocation table
     * @param segBase       This segment's base address
     * @param jtBase        Jump table base address
     * @param a5Base        A5 world base address
     * @return              OSErr
     */
    OSErr (*Relocate)(CPUAddressSpace as, CPUCodeHandle code,
                     const RelocTable* relocs, CPUAddr segBase,
                     CPUAddr jtBase, CPUAddr a5Base);

    /*
     * AllocateMemory - Allocate memory in CPU address space
     *
     * @param as            Address space
     * @param size          Size to allocate
     * @param flags         Mapping flags
     * @param outAddr       Output address
     * @return              OSErr
     */
    OSErr (*AllocateMemory)(CPUAddressSpace as, Size size,
                           CPUMapFlags flags, CPUAddr* outAddr);

    /*
     * WriteMemory - Write data to CPU address space
     *
     * @param as            Address space
     * @param addr          CPU address to write to
     * @param data          Host data to write
     * @param len           Length
     * @return              OSErr
     */
    OSErr (*WriteMemory)(CPUAddressSpace as, CPUAddr addr,
                        const void* data, Size len);

    /*
     * ReadMemory - Read data from CPU address space
     *
     * @param as            Address space
     * @param addr          CPU address to read from
     * @param data          Host buffer
     * @param len           Length
     * @return              OSErr
     */
    OSErr (*ReadMemory)(CPUAddressSpace as, CPUAddr addr,
                       void* data, Size len);
} ICPUBackend;

/*
 * CPU Backend Registry - Allows runtime selection
 */
OSErr CPUBackend_Register(const char* name, const ICPUBackend* backend);
const ICPUBackend* CPUBackend_Get(const char* name);
const ICPUBackend* CPUBackend_GetDefault(void);

/*
 * Implemented backends (forward declarations)
 */
extern const ICPUBackend gM68KInterpreterBackend;

#ifdef __cplusplus
}
#endif

#endif /* CPU_BACKEND_H */
