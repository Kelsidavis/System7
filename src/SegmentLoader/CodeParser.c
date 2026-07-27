/*
 * CodeParser.c - Big-Endian Safe CODE Resource Parser
 *
 * Portable parsing of 68K CODE resources with proper endian handling.
 */

#include "SegmentLoader/CodeParser.h"
#include "SegmentLoader/SegmentLoader.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include <string.h>

/*
 * ValidateCODE0 - Validate CODE 0 resource
 */
OSErr ValidateCODE0(const void* data, Size size)
{
    if (!data) {
        return paramErr;
    }

    /* CODE 0 must be at least header size */
    if (size < CODE0_HEADER_SIZE) {
        return segmentBadFormat;
    }

    /* Read sizes and validate */
    UInt32 aboveA5 = BE_Read32((const UInt8*)data + CODE0_ABOVE_A5_OFFSET);
    UInt32 belowA5 = BE_Read32((const UInt8*)data + CODE0_BELOW_A5_OFFSET);
    UInt32 jtSize = BE_Read32((const UInt8*)data + CODE0_JT_SIZE_OFFSET);

    /* Sanity checks */
    if (aboveA5 > 1024 * 1024 || belowA5 > 1024 * 1024) {
        return segmentBadFormat; /* Unreasonably large */
    }

    if (jtSize > aboveA5) {
        return segmentBadFormat; /* JT can't be larger than above-A5 */
    }

    /* Check that jump table fits in resource */
    if (CODE0_HEADER_SIZE + jtSize > size) {
        return segmentBadFormat;
    }

    return noErr;
}

/*
 * ValidateCODEN - Validate CODE N resource
 */
OSErr ValidateCODEN(const void* data, Size size, SInt16 segID)
{
    if (!data) {
        return paramErr;
    }

    /* CODE N must have at least minimal header */
    if (size < CODEN_HEADER_SIZE) {
        return segmentBadFormat;
    }

    (void)segID; /* Unused for now */

    return noErr;
}

/*
 * ParseCODE0 - Parse CODE 0 to extract A5 world metadata
 */
OSErr ParseCODE0(const void* code0Data, Size size, CODE0Info* info)
{
    OSErr err;

    if (!code0Data || !info) {
        return paramErr;
    }

    /* Validate first */
    err = ValidateCODE0(code0Data, size);
    if (err != noErr) {
        return err;
    }

    const UInt8* data = (const UInt8*)code0Data;

    /* Extract A5 world sizes */
    info->a5AboveSize = BE_Read32(data + CODE0_ABOVE_A5_OFFSET);
    info->a5BelowSize = BE_Read32(data + CODE0_BELOW_A5_OFFSET);

    /* Extract jump table metadata */
    UInt32 jtSize = BE_Read32(data + CODE0_JT_SIZE_OFFSET);
    info->jtOffsetFromA5 = BE_Read32(data + CODE0_JT_OFFSET_OFFSET);

    /* Calculate JT entry count */
    info->jtEntrySize = JT_ENTRY_SIZE;
    if (jtSize > 0 && info->jtEntrySize > 0) {
        info->jtCount = jtSize / info->jtEntrySize;
    } else {
        info->jtCount = 0;
    }

    info->flags = 0;
    info->reserved = 0;

    return noErr;
}

/*
 * ParseCODEN - Parse CODE N to extract segment metadata
 */
OSErr ParseCODEN(const void* codeData, Size size, SInt16 segID,
                CODEInfo* info)
{
    OSErr err;

    if (!codeData || !info) {
        return paramErr;
    }

    /* Validate first */
    err = ValidateCODEN(codeData, size, segID);
    if (err != noErr) {
        return err;
    }

    const UInt8* data = (const UInt8*)codeData;

    /* Extract entry offset and flags */
    info->firstJTEntry = BE_Read16(data + CODEN_FIRST_JT_ENTRY);
    info->jtEntryCount = BE_Read16(data + CODEN_JT_ENTRY_COUNT);

    /* Check for linker prologue */
    info->prologueSkip = 0;
    /* Reads as far as data[9], so it needs ten bytes, not six. A segment of
     * six to nine bytes had its prologue decided on whatever followed it. */
    if (size >= 10) {
        UInt16 word0 = BE_Read16(data + 4);
        UInt16 word2 = BE_Read16(data + 8);

        /* Check for classic linker prologue:
         *   0x3F3C  MOVE.W #imm,-(SP)
         *   0xA9F0  _LoadSeg trap
         */
        if (word0 == 0x3F3C && word2 == 0xA9F0) {
            info->prologueSkip = 6;
        }
    }

    info->codeSize = size;
    info->segID = segID;

    /* Initialize empty relocation table */
    info->relocTable.count = 0;
    info->relocTable.entries = NULL;

    return noErr;
}

/*
 * BuildRelocationTable - Build relocation table from CODE segment
 *
 * This scans for common 68K relocation patterns:
 * - JMP/JSR to jump table
 * - Absolute addresses needing segment base fixup
 * - A5-relative LEA/MOVE instructions
 */
OSErr BuildRelocationTable(const void* codeData, Size size, SInt16 segID,
                           RelocTable* relocTable)
{
    if (!codeData || !relocTable) {
        return paramErr;
    }

    (void)codeData;
    (void)size;
    (void)segID;

    /*
     * A near-model CODE segment carries no relocations, and none are invented
     * for it here.
     *
     * This used to scan the segment two bytes at a time looking for opcodes
     * that take an absolute long operand - JMP, JSR, PEA, MOVE.L - and rewrite
     * whatever followed them. Nothing in that scan could tell an instruction
     * from the data or the second half of a longer instruction that happened
     * to read like one, and the operands it found were not relocations to
     * begin with.
     *
     * System 7 links a near-model application through its jump table: a call
     * into another segment goes to an A5-relative jump table entry, and
     * globals are addressed from A5. Both move with the process rather than
     * with the segment, so a loaded segment needs no fixing up at all. The
     * scan was rewriting correct code - MOVE.L $00000904,D0 reads CurrentA5,
     * an ordinary low memory global, and its address came out as the segment
     * base plus $904, pointing at nothing.
     *
     * Far-model segments do carry relocation tables, in the segment itself.
     * When those are supported the table gets read from the segment; it does
     * not get guessed from the instruction stream.
     */
    relocTable->count = 0;
    relocTable->entries = NULL;

    return noErr;
}

/*
 * FreeRelocationTable - Free relocation table
 */
void FreeRelocationTable(RelocTable* relocTable)
{
    if (relocTable && relocTable->entries) {
        DisposePtr((Ptr)relocTable->entries);
        relocTable->entries = NULL;
        relocTable->count = 0;
    }
}

/*
 * GetJumpTableEntry - Read jump table entry
 */
OSErr GetJumpTableEntry(const void* jtData, UInt16 jtIndex,
                       UInt16* outOffset, UInt16* outInstruction,
                       UInt32* outTarget)
{
    if (!jtData) {
        return paramErr;
    }

    const UInt8* entry = (const UInt8*)jtData + (jtIndex * JT_ENTRY_SIZE);

    if (outOffset) {
        *outOffset = BE_Read16(entry + 0);
    }

    if (outInstruction) {
        *outInstruction = BE_Read16(entry + 2);
    }

    if (outTarget) {
        *outTarget = BE_Read32(entry + 4);
    }

    return noErr;
}

/*
 * SetJumpTableEntry - Write jump table entry
 */
OSErr SetJumpTableEntry(void* jtData, UInt16 jtIndex,
                       UInt16 offset, UInt16 instruction, UInt32 target)
{
    if (!jtData) {
        return paramErr;
    }

    UInt8* entry = (UInt8*)jtData + (jtIndex * JT_ENTRY_SIZE);

    BE_Write16(entry + 0, offset);
    BE_Write16(entry + 2, instruction);
    BE_Write32(entry + 4, target);

    return noErr;
}
