/*
 * CodeParser.h - Big-Endian Safe CODE Resource Parser
 *
 * Provides portable, endian-aware parsing of 68K CODE resources.
 * All classic Mac resources are big-endian regardless of host ISA.
 */

#ifndef CODE_PARSER_H
#define CODE_PARSER_H

#include "SystemTypes.h"
#include "CPU/CPUBackend.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Big-Endian Safe Read Helpers
 *
 * These functions safely read big-endian data from CODE resources,
 * handling alignment and endianness conversion.
 */

static inline UInt16 BE_Read16(const void* ptr) {
    const UInt8* p = (const UInt8*)ptr;
    return ((UInt16)p[0] << 8) | (UInt16)p[1];
}

static inline UInt32 BE_Read32(const void* ptr) {
    const UInt8* p = (const UInt8*)ptr;
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) |
           ((UInt32)p[2] << 8)  | (UInt32)p[3];
}

static inline void BE_Write16(void* ptr, UInt16 value) {
    UInt8* p = (UInt8*)ptr;
    p[0] = (value >> 8) & 0xFF;
    p[1] = value & 0xFF;
}

static inline void BE_Write32(void* ptr, UInt32 value) {
    UInt8* p = (UInt8*)ptr;
    p[0] = (value >> 24) & 0xFF;
    p[1] = (value >> 16) & 0xFF;
    p[2] = (value >> 8) & 0xFF;
    p[3] = value & 0xFF;
}

/*
 * CODE 0 Structure (Classic 68K Format)
 *
 * CODE 0 has the following layout:
 *   Offset  Size  Description
 *   +0      4     AboveA5 size (bytes above A5)
 *   +4      4     BelowA5 size (bytes below A5)
 *   +8      4     JT size (jump table size in bytes)
 *   +12     4     JT offset from A5
 *   +16     ...   Jump table data (entries)
 *
 * Jump table entry format (8 bytes per entry):
 *   Offset  Size  Description
 *   +0      2     Offset within segment (or stub)
 *   +2      2     Instruction word (often 0x4EF9 = JMP)
 *   +4      4     Target address (to be patched)
 *
 * Note: Sizes and offsets are in big-endian format
 */

#define CODE0_ABOVE_A5_OFFSET   0
#define CODE0_BELOW_A5_OFFSET   4
#define CODE0_JT_SIZE_OFFSET    8
#define CODE0_JT_OFFSET_OFFSET  12
#define CODE0_HEADER_SIZE       16

#define JT_ENTRY_SIZE           8

/*
 * CODE N Structure (Segment Format)
 *
 * CODE resources 1..N have this layout:
 *   Offset  Size  Description
 *   +0      2     Offset of this segment's first jump table entry, in bytes
 *                 from the start of the jump table
 *   +2      2     How many jump table entries belong to this segment
 *   +4      ...   Code bytes
 *
 * This was described as an entry offset and a flags word, and the first word
 * was added to the segment base to find where to start executing. A near
 * model segment is not entered that way - it is entered through one of its
 * jump table entries - and that word is a jump table offset, so treating it
 * as a distance into the code lands part way through an instruction.
 *
 * Which entries belong to which segment is written here, in the segment. It
 * used to be worked out from a rule invented to make the stub installer and
 * _LoadSeg agree with each other, which they had not.
 *
 * Some linkers add a small prologue:
 *   +0      2     0x3F3C (MOVE.W #imm,-(SP) prologue)
 *   +2      2     Segment number
 *   +4      2     0xA9F0 (_LoadSeg trap)
 *
 * The actual entry point may skip this prologue.
 */

#define CODEN_FIRST_JT_ENTRY    0
#define CODEN_JT_ENTRY_COUNT    2
#define CODEN_HEADER_SIZE       4

/*
 * CODE Resource Validation
 */
OSErr ValidateCODE0(const void* data, Size size);
OSErr ValidateCODEN(const void* data, Size size, SInt16 segID);

/*
 * Relocation Table Parsing
 *
 * Classic 68K code often has embedded relocation data or uses
 * specific instruction patterns that need fixup. We scan for:
 * - JMP/JSR instructions targeting jump table
 * - Absolute address references needing segment base fixup
 * - A5-relative LEA/MOVE instructions
 */

/*
 * BuildRelocationTable - Extract relocations from CODE segment
 *
 * Scans CODE segment for relocation patterns and builds portable
 * relocation table.
 *
 * @param codeData          CODE segment data
 * @param size              Size of code
 * @param segID             Segment ID
 * @param relocTable        Output relocation table
 * @return                  OSErr
 */
OSErr BuildRelocationTable(const void* codeData, Size size, SInt16 segID,
                           RelocTable* relocTable);

/*
 * FreeRelocationTable - Free relocation table memory
 */
void FreeRelocationTable(RelocTable* relocTable);

/*
 * Jump Table Entry Utilities
 */

/*
 * GetJumpTableEntry - Read JT entry at index
 *
 * @param jtData            Jump table data
 * @param jtIndex           Index (0-based)
 * @param outOffset         Output: offset within segment
 * @param outInstruction    Output: instruction word
 * @param outTarget         Output: target address (pre-relocation)
 * @return                  OSErr
 */
OSErr GetJumpTableEntry(const void* jtData, UInt16 jtIndex,
                       UInt16* outOffset, UInt16* outInstruction,
                       UInt32* outTarget);

/*
 * SetJumpTableEntry - Write JT entry
 */
OSErr SetJumpTableEntry(void* jtData, UInt16 jtIndex,
                       UInt16 offset, UInt16 instruction, UInt32 target);

#ifdef __cplusplus
}
#endif

#endif /* CODE_PARSER_H */
