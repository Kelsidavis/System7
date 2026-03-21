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
    info->entryOffset = BE_Read16(data + CODEN_ENTRY_OFFSET);
    info->flags = BE_Read16(data + CODEN_FLAGS_OFFSET);

    /* Check for linker prologue */
    info->prologueSkip = 0;
    if (size >= 6) {
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

    const UInt8* data = (const UInt8*)codeData;

    /* Scan for 68K relocation patterns:
     * - JMP/JSR absolute.L (jump table or segment references)
     * - PEA absolute.L (push effective address)
     * - MOVE.L absolute.L (absolute data references)
     * - LEA/MOVE with A5-relative addressing (globals)
     */

    /* Count potential relocations */
    UInt16 count = 0;
    for (Size i = 0; i + 6 < size; i += 2) {
        UInt16 opcode = BE_Read16(data + i);

        /* JMP absolute.L (0x4EF9) or JSR absolute.L (0x4EB9) */
        if (opcode == 0x4EF9 || opcode == 0x4EB9) {
            count++;
        }
        /* PEA absolute.L (0x4879) */
        else if (opcode == 0x4879) {
            count++;
        }
        /* MOVE.L absolute.L,Dn (0x20xx, 0x22xx, etc. with mode 111 reg 001) */
        else if ((opcode & 0xF1FF) == 0x2039 || (opcode & 0xF1FF) == 0x2139) {
            count++;
        }
        /* MOVE.L Dn,absolute.L (0x23C0-0x23C7) */
        else if ((opcode & 0xFFF8) == 0x23C0) {
            count++;
        }
        /* LEA d16(A5),An (0x4xED where x=dest reg) */
        else if ((opcode & 0xF0FF) == 0x40ED) {
            count++;
        }
        /* MOVE.L d16(A5),Dn (0x202D, 0x222D, etc.) */
        else if ((opcode & 0xF1FF) == 0x202D) {
            count++;
        }
        /* MOVE.L Dn,d16(A5) (0x2B40-0x2B47) */
        else if ((opcode & 0xFFF8) == 0x2B40) {
            count++;
        }
    }

    /* Allocate relocation entries */
    if (count > 0) {
        /* UInt16 count can't overflow size_t multiplication */
        relocTable->entries = (RelocEntry*)NewPtr((Size)count * sizeof(RelocEntry));
        if (!relocTable->entries) {
            return memFullErr;
        }

        /* Build entries */
        UInt16 idx = 0;
        for (Size i = 0; i + 6 < size && idx < count; i += 2) {
            UInt16 opcode = BE_Read16(data + i);

            /* JMP/JSR absolute.L */
            if (opcode == 0x4EF9 || opcode == 0x4EB9) {
                UInt32 target = BE_Read32(data + i + 2);

                /* Heuristic: if target looks like it's in low memory,
                 * assume it's a jump table reference */
                if (target < 0x10000) {
                    relocTable->entries[idx].kind = kRelocJTImport;
                    relocTable->entries[idx].atOffset = i + 2;
                    relocTable->entries[idx].addend = 0;
                    relocTable->entries[idx].targetSegment = 0;
                    relocTable->entries[idx].jtIndex = target / 8;
                } else {
                    /* Assume absolute segment reference */
                    relocTable->entries[idx].kind = kRelocAbsSegBase;
                    relocTable->entries[idx].atOffset = i + 2;
                    relocTable->entries[idx].addend = target;
                    relocTable->entries[idx].targetSegment = segID;
                    relocTable->entries[idx].jtIndex = 0;
                }
                idx++;
            }
            /* PEA absolute.L */
            else if (opcode == 0x4879) {
                UInt32 target = BE_Read32(data + i + 2);
                relocTable->entries[idx].kind = kRelocAbsSegBase;
                relocTable->entries[idx].atOffset = i + 2;
                relocTable->entries[idx].addend = target;
                relocTable->entries[idx].targetSegment = segID;
                relocTable->entries[idx].jtIndex = 0;
                idx++;
            }
            /* MOVE.L absolute.L,Dn */
            else if ((opcode & 0xF1FF) == 0x2039 || (opcode & 0xF1FF) == 0x2139) {
                UInt32 target = BE_Read32(data + i + 2);
                relocTable->entries[idx].kind = kRelocAbsSegBase;
                relocTable->entries[idx].atOffset = i + 2;
                relocTable->entries[idx].addend = target;
                relocTable->entries[idx].targetSegment = segID;
                relocTable->entries[idx].jtIndex = 0;
                idx++;
            }
            /* MOVE.L Dn,absolute.L */
            else if ((opcode & 0xFFF8) == 0x23C0) {
                UInt32 target = BE_Read32(data + i + 2);
                relocTable->entries[idx].kind = kRelocAbsSegBase;
                relocTable->entries[idx].atOffset = i + 2;
                relocTable->entries[idx].addend = target;
                relocTable->entries[idx].targetSegment = segID;
                relocTable->entries[idx].jtIndex = 0;
                idx++;
            }
            /* LEA d16(A5),An - A5-relative */
            else if ((opcode & 0xF0FF) == 0x40ED) {
                SInt16 offset = (SInt16)BE_Read16(data + i + 2);
                relocTable->entries[idx].kind = kRelocA5Relative;
                relocTable->entries[idx].atOffset = i + 2;
                relocTable->entries[idx].addend = offset;
                relocTable->entries[idx].targetSegment = 0;
                relocTable->entries[idx].jtIndex = 0;
                idx++;
            }
            /* MOVE.L d16(A5),Dn - A5-relative */
            else if ((opcode & 0xF1FF) == 0x202D) {
                SInt16 offset = (SInt16)BE_Read16(data + i + 2);
                relocTable->entries[idx].kind = kRelocA5Relative;
                relocTable->entries[idx].atOffset = i + 2;
                relocTable->entries[idx].addend = offset;
                relocTable->entries[idx].targetSegment = 0;
                relocTable->entries[idx].jtIndex = 0;
                idx++;
            }
            /* MOVE.L Dn,d16(A5) - A5-relative */
            else if ((opcode & 0xFFF8) == 0x2B40) {
                SInt16 offset = (SInt16)BE_Read16(data + i + 2);
                relocTable->entries[idx].kind = kRelocA5Relative;
                relocTable->entries[idx].atOffset = i + 2;
                relocTable->entries[idx].addend = offset;
                relocTable->entries[idx].targetSegment = 0;
                relocTable->entries[idx].jtIndex = 0;
                idx++;
            }
        }

        relocTable->count = idx;
    } else {
        relocTable->count = 0;
        relocTable->entries = NULL;
    }

    (void)segID; /* Unused in this simple implementation */

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
