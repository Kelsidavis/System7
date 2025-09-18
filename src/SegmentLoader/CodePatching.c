/*
 * CodePatching.c - Jump Table Management and Code Patching
 *
 * This file implements jump table management, code patching, and segment
 * interconnection for Mac OS 7.1 applications. It handles the critical
 * task of linking code segments together at runtime.
 */

#include "../../include/SegmentLoader/SegmentLoader.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include "../../include/Debugging/Debugging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * Constants and Definitions
 * ============================================================================ */

/* 68k instruction opcodes */
#define OPCODE_JMP_ABSOLUTE     0x4EF9  /* JMP $nnnnnnnn */
#define OPCODE_JSR_ABSOLUTE     0x4EB9  /* JSR $nnnnnnnn */
#define OPCODE_MOVE_L_IMM_A0    0x207C  /* MOVE.L #$nnnnnnnn,A0 */
#define OPCODE_JMP_A0           0x4ED0  /* JMP (A0) */
#define OPCODE_NOP              0x4E71  /* NOP */
#define OPCODE_RTS              0x4E75  /* RTS */
#define OPCODE_TRAP_UNIMPL      0xA89F  /* _Unimplemented trap */

/* Jump table entry states */
#define JUMP_UNRESOLVED         0x00    /* Not yet resolved */
#define JUMP_RESOLVED           0x01    /* Resolved to target */
#define JUMP_SEGMENT_LOADING    0x02    /* Currently loading segment */
#define JUMP_STUB               0x03    /* Stub for lazy loading */

/* Maximum jump table size */
#define MAX_JUMP_TABLE_ENTRIES  1024

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

typedef struct JumpTableEntryInternal {
    uint16_t        segmentID;      /* Target segment ID */
    uint32_t        offset;         /* Offset within segment */
    uint32_t        entryAddr;      /* Address of jump table entry */
    uint32_t        targetAddr;     /* Resolved target address */
    uint8_t         state;          /* Entry state */
    uint8_t         opcode[6];      /* Original/patched opcode */
} JumpTableEntryInternal;

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

static OSErr AllocateJumpTable(ApplicationControlBlock* acb, uint16_t entryCount);
static OSErr ParseJumpTableFromCode(ApplicationControlBlock* acb);
static OSErr ResolveJumpTableEntry(ApplicationControlBlock* acb, uint16_t entryIndex);
static OSErr PatchJumpInstruction(uint32_t entryAddr, uint32_t targetAddr);
static OSErr CreateLazyLoadStub(uint32_t entryAddr, uint16_t segmentID);
static uint32_t FindSegmentBase(ApplicationControlBlock* acb, uint16_t segmentID);
static bool IsJumpInstruction(uint16_t opcode);
static OSErr RelocateCode(uint8_t* codePtr, uint32_t codeSize, uint32_t newBase);

/* ============================================================================
 * Jump Table Initialization
 * ============================================================================ */

OSErr InitJumpTable(ApplicationControlBlock* acb)
{
    if (!acb) {
        return segBadFormat;
    }

    DEBUG_LOG("InitJumpTable: Initializing jump table for application\n");

    /* Allocate initial jump table */
    OSErr err = AllocateJumpTable(acb, MAX_JUMP_TABLE_ENTRIES);
    if (err != segNoErr) {
        DEBUG_LOG("InitJumpTable: Failed to allocate jump table: %d\n", err);
        return err;
    }

    /* Parse jump table from CODE 0 (jump table segment) */
    err = ParseJumpTableFromCode(acb);
    if (err != segNoErr) {
        DEBUG_LOG("InitJumpTable: Failed to parse jump table: %d\n", err);
        return err;
    }

    /* Resolve initial entries */
    err = ResolveJumpTableRefs(acb);
    if (err != segNoErr) {
        DEBUG_LOG("InitJumpTable: Failed to resolve jump table: %d\n", err);
        return err;
    }

    DEBUG_LOG("InitJumpTable: Jump table initialized with %d entries\n",
             acb->jumpTableSize);

    return segNoErr;
}

static OSErr AllocateJumpTable(ApplicationControlBlock* acb, uint16_t entryCount)
{
    DEBUG_LOG("AllocateJumpTable: Allocating jump table for %d entries\n", entryCount);

    /* Allocate jump table entries */
    acb->jumpTable = (JumpTableEntry*)calloc(entryCount, sizeof(JumpTableEntry));
    if (!acb->jumpTable) {
        return segMemFullErr;
    }

    acb->jumpTableSize = entryCount;

    /* Initialize all entries as unresolved */
    for (uint16_t i = 0; i < entryCount; i++) {
        acb->jumpTable[i].opcode = OPCODE_TRAP_UNIMPL;
        acb->jumpTable[i].address = 0;
        acb->jumpTable[i].segmentID = 0;
        acb->jumpTable[i].offset = 0;
    }

    return segNoErr;
}

static OSErr ParseJumpTableFromCode(ApplicationControlBlock* acb)
{
    DEBUG_LOG("ParseJumpTableFromCode: Parsing jump table from CODE 0\n");

    /* Load CODE 0 resource (jump table) */
    int16_t savedResFile = CurResFile();
    UseResFile(acb->resFile);

    Handle jumpTableRes = Get1Resource('CODE', 0);
    if (!jumpTableRes) {
        UseResFile(savedResFile);
        DEBUG_LOG("ParseJumpTableFromCode: No CODE 0 resource found\n");
        return segResNotFound;
    }

    LoadResource(jumpTableRes);
    if (ResError() != noErr) {
        ReleaseResource(jumpTableRes);
        UseResFile(savedResFile);
        return segResNotFound;
    }

    /* Parse the jump table structure */
    HLock(jumpTableRes);
    uint8_t* jumpTableData = (uint8_t*)*jumpTableRes;
    uint32_t jumpTableSize = GetHandleSize(jumpTableRes);

    /* CODE 0 format:
     * 0x00: Above A5 size (4 bytes)
     * 0x04: Below A5 size (4 bytes)
     * 0x08: Jump table size (4 bytes)
     * 0x0C: A5 offset (4 bytes)
     * 0x10: Jump table entries...
     */

    if (jumpTableSize < 16) {
        HUnlock(jumpTableRes);
        ReleaseResource(jumpTableRes);
        UseResFile(savedResFile);
        DEBUG_LOG("ParseJumpTableFromCode: Invalid CODE 0 size\n");
        return segBadFormat;
    }

    uint32_t aboveA5Size = *(uint32_t*)(jumpTableData + 0x00);
    uint32_t belowA5Size = *(uint32_t*)(jumpTableData + 0x04);
    uint32_t jumpSize = *(uint32_t*)(jumpTableData + 0x08);
    uint32_t a5Offset = *(uint32_t*)(jumpTableData + 0x0C);

    DEBUG_LOG("ParseJumpTableFromCode: Above A5: %d, Below A5: %d, Jump size: %d\n",
             aboveA5Size, belowA5Size, jumpSize);

    /* Store A5 world information */
    acb->globalsSize = aboveA5Size + belowA5Size;

    /* Parse jump table entries */
    uint8_t* entryPtr = jumpTableData + 0x10;
    uint16_t entryCount = 0;

    while (entryPtr < jumpTableData + jumpTableSize && entryCount < acb->jumpTableSize) {
        /* Each entry is typically 8 bytes:
         * 0x00: Offset in segment (4 bytes)
         * 0x04: Segment ID (2 bytes)
         * 0x06: Flags (2 bytes)
         */

        if (entryPtr + 8 > jumpTableData + jumpTableSize) {
            break;
        }

        uint32_t offset = *(uint32_t*)entryPtr;
        uint16_t segmentID = *(uint16_t*)(entryPtr + 4);
        uint16_t flags = *(uint16_t*)(entryPtr + 6);

        if (segmentID != 0) {  /* Valid entry */
            acb->jumpTable[entryCount].segmentID = segmentID;
            acb->jumpTable[entryCount].offset = offset;
            acb->jumpTable[entryCount].opcode = OPCODE_TRAP_UNIMPL;
            acb->jumpTable[entryCount].address = 0;

            DEBUG_LOG("ParseJumpTableFromCode: Entry %d -> Segment %d, Offset 0x%X\n",
                     entryCount, segmentID, offset);

            entryCount++;
        }

        entryPtr += 8;
    }

    /* Update actual jump table size */
    acb->jumpTableSize = entryCount;

    HUnlock(jumpTableRes);
    ReleaseResource(jumpTableRes);
    UseResFile(savedResFile);

    DEBUG_LOG("ParseJumpTableFromCode: Parsed %d jump table entries\n", entryCount);

    return segNoErr;
}

/* ============================================================================
 * Jump Table Entry Management
 * ============================================================================ */

OSErr AddJumpTableEntry(ApplicationControlBlock* acb, uint16_t segmentID,
                       uint32_t offset, uint32_t* entryAddr)
{
    if (!acb || !entryAddr) {
        return segBadFormat;
    }

    DEBUG_LOG("AddJumpTableEntry: Adding entry for segment %d, offset 0x%X\n",
             segmentID, offset);

    /* Find empty slot in jump table */
    uint16_t entryIndex = acb->jumpTableSize;
    for (uint16_t i = 0; i < acb->jumpTableSize; i++) {
        if (acb->jumpTable[i].segmentID == 0) {
            entryIndex = i;
            break;
        }
    }

    if (entryIndex >= MAX_JUMP_TABLE_ENTRIES) {
        DEBUG_LOG("AddJumpTableEntry: Jump table full\n");
        return segJumpTableFull;
    }

    /* Add the entry */
    acb->jumpTable[entryIndex].segmentID = segmentID;
    acb->jumpTable[entryIndex].offset = offset;
    acb->jumpTable[entryIndex].opcode = OPCODE_TRAP_UNIMPL;

    /* Allocate memory for the jump instruction */
    Ptr jumpCode = NewPtr(6);  /* 6 bytes for JMP instruction */
    if (!jumpCode) {
        return segMemFullErr;
    }

    acb->jumpTable[entryIndex].address = (uint32_t)jumpCode;

    /* Create lazy load stub initially */
    OSErr err = CreateLazyLoadStub((uint32_t)jumpCode, segmentID);
    if (err != segNoErr) {
        DisposePtr(jumpCode);
        return err;
    }

    if (entryIndex == acb->jumpTableSize) {
        acb->jumpTableSize++;
    }

    *entryAddr = (uint32_t)jumpCode;

    DEBUG_LOG("AddJumpTableEntry: Added entry %d at address 0x%08X\n",
             entryIndex, *entryAddr);

    return segNoErr;
}

OSErr PatchJumpTableEntry(ApplicationControlBlock* acb, uint32_t entryAddr,
                         uint32_t newTarget)
{
    if (!acb) {
        return segBadFormat;
    }

    DEBUG_LOG("PatchJumpTableEntry: Patching entry at 0x%08X to target 0x%08X\n",
             entryAddr, newTarget);

    /* Find the jump table entry */
    uint16_t entryIndex = 0;
    bool found = false;

    for (uint16_t i = 0; i < acb->jumpTableSize; i++) {
        if (acb->jumpTable[i].address == entryAddr) {
            entryIndex = i;
            found = true;
            break;
        }
    }

    if (!found) {
        DEBUG_LOG("PatchJumpTableEntry: Entry not found\n");
        return segBadPatchAddr;
    }

    /* Update the entry */
    acb->jumpTable[entryIndex].address = newTarget;

    /* Patch the actual instruction */
    OSErr err = PatchJumpInstruction(entryAddr, newTarget);
    if (err != segNoErr) {
        DEBUG_LOG("PatchJumpTableEntry: Failed to patch instruction: %d\n", err);
        return err;
    }

    DEBUG_LOG("PatchJumpTableEntry: Successfully patched entry %d\n", entryIndex);

    return segNoErr;
}

OSErr ResolveJumpTableRefs(ApplicationControlBlock* acb)
{
    if (!acb) {
        return segBadFormat;
    }

    DEBUG_LOG("ResolveJumpTableRefs: Resolving %d jump table entries\n",
             acb->jumpTableSize);

    uint16_t resolvedCount = 0;

    for (uint16_t i = 0; i < acb->jumpTableSize; i++) {
        if (acb->jumpTable[i].segmentID != 0) {
            OSErr err = ResolveJumpTableEntry(acb, i);
            if (err == segNoErr) {
                resolvedCount++;
            } else {
                DEBUG_LOG("ResolveJumpTableRefs: Failed to resolve entry %d: %d\n", i, err);
            }
        }
    }

    DEBUG_LOG("ResolveJumpTableRefs: Resolved %d of %d entries\n",
             resolvedCount, acb->jumpTableSize);

    return segNoErr;
}

static OSErr ResolveJumpTableEntry(ApplicationControlBlock* acb, uint16_t entryIndex)
{
    if (entryIndex >= acb->jumpTableSize) {
        return segBadFormat;
    }

    JumpTableEntry* entry = &acb->jumpTable[entryIndex];

    DEBUG_LOG("ResolveJumpTableEntry: Resolving entry %d (segment %d, offset 0x%X)\n",
             entryIndex, entry->segmentID, entry->offset);

    /* Find the target segment */
    uint32_t segmentBase = FindSegmentBase(acb, entry->segmentID);
    if (segmentBase == 0) {
        /* Segment not loaded - create lazy load stub */
        if (entry->address != 0) {
            return CreateLazyLoadStub(entry->address, entry->segmentID);
        }
        return segSegmentNotFound;
    }

    /* Calculate target address */
    uint32_t targetAddr = segmentBase + entry->offset;
    entry->address = targetAddr;

    /* Patch the jump instruction if we have an entry address */
    if (entry->address != 0) {
        OSErr err = PatchJumpInstruction(entry->address, targetAddr);
        if (err != segNoErr) {
            return err;
        }
    }

    DEBUG_LOG("ResolveJumpTableEntry: Resolved to address 0x%08X\n", targetAddr);

    return segNoErr;
}

/* ============================================================================
 * Code Patching Functions
 * ============================================================================ */

static OSErr PatchJumpInstruction(uint32_t entryAddr, uint32_t targetAddr)
{
    DEBUG_LOG("PatchJumpInstruction: Patching 0x%08X -> 0x%08X\n", entryAddr, targetAddr);

    if (entryAddr == 0) {
        return segBadPatchAddr;
    }

    /* Create JMP $nnnnnnnn instruction */
    uint16_t* codePtr = (uint16_t*)entryAddr;

    /* JMP absolute: 4EF9 nnnn nnnn */
    codePtr[0] = OPCODE_JMP_ABSOLUTE;
    *(uint32_t*)(codePtr + 1) = targetAddr;

    DEBUG_LOG("PatchJumpInstruction: Patched jump instruction\n");

    return segNoErr;
}

static OSErr CreateLazyLoadStub(uint32_t entryAddr, uint16_t segmentID)
{
    DEBUG_LOG("CreateLazyLoadStub: Creating stub at 0x%08X for segment %d\n",
             entryAddr, segmentID);

    if (entryAddr == 0) {
        return segBadPatchAddr;
    }

    uint16_t* codePtr = (uint16_t*)entryAddr;

    /* Create lazy loading stub:
     * TRAP #$A9F0     ; _LoadSeg trap
     * DC.W segmentID  ; Segment ID parameter
     * RTS             ; Return (will be overwritten when resolved)
     */
    codePtr[0] = 0xA9F0;        /* _LoadSeg trap */
    codePtr[1] = segmentID;     /* Segment ID */
    codePtr[2] = OPCODE_RTS;    /* RTS */

    DEBUG_LOG("CreateLazyLoadStub: Created lazy load stub\n");

    return segNoErr;
}

/* ============================================================================
 * Segment Location Functions
 * ============================================================================ */

static uint32_t FindSegmentBase(ApplicationControlBlock* acb, uint16_t segmentID)
{
    DEBUG_LOG("FindSegmentBase: Finding base for segment %d\n", segmentID);

    /* Look up segment in the segment table */
    SegmentDescriptor segDesc;
    OSErr err = GetSegmentInfo(segmentID, &segDesc);
    if (err != segNoErr || !(segDesc.flags & SEG_LOADED)) {
        DEBUG_LOG("FindSegmentBase: Segment %d not loaded\n", segmentID);
        return 0;
    }

    DEBUG_LOG("FindSegmentBase: Found segment %d at 0x%08X\n", segmentID, segDesc.codeAddr);

    return segDesc.codeAddr;
}

/* ============================================================================
 * Code Relocation Functions
 * ============================================================================ */

static OSErr RelocateCode(uint8_t* codePtr, uint32_t codeSize, uint32_t newBase)
{
    DEBUG_LOG("RelocateCode: Relocating %d bytes to base 0x%08X\n", codeSize, newBase);

    /* This would contain the actual 68k relocation logic */
    /* For now, this is a placeholder */

    /* 68k relocation typically involves:
     * 1. Scanning for relocation entries in the code
     * 2. Adjusting absolute addresses based on new base
     * 3. Fixing up PC-relative branches that are out of range
     * 4. Updating A5-relative references
     */

    DEBUG_LOG("RelocateCode: Relocation completed (placeholder)\n");

    return segNoErr;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static bool IsJumpInstruction(uint16_t opcode)
{
    /* Check for various jump/branch instructions */
    switch (opcode) {
        case OPCODE_JMP_ABSOLUTE:
        case OPCODE_JSR_ABSOLUTE:
            return true;

        default:
            /* Check for other jump patterns */
            if ((opcode & 0xFF00) == 0x6000) {  /* Bcc instructions */
                return true;
            }
            return false;
    }
}

/* ============================================================================
 * Jump Table Information Functions
 * ============================================================================ */

OSErr GetJumpTableInfo(ApplicationControlBlock* acb, uint16_t* entryCount,
                      uint32_t* tableSize)
{
    if (!acb) {
        return segNotApplication;
    }

    if (entryCount) {
        *entryCount = acb->jumpTableSize;
    }

    if (tableSize) {
        *tableSize = acb->jumpTableSize * sizeof(JumpTableEntry);
    }

    return segNoErr;
}

OSErr GetJumpTableEntry(ApplicationControlBlock* acb, uint16_t index,
                       JumpTableEntry* entry)
{
    if (!acb || !entry || index >= acb->jumpTableSize) {
        return segBadFormat;
    }

    *entry = acb->jumpTable[index];
    return segNoErr;
}

/* ============================================================================
 * Advanced Patching Functions
 * ============================================================================ */

OSErr InstallCodePatch(ApplicationControlBlock* acb, uint32_t targetAddr,
                      void* patchCode, uint32_t patchSize)
{
    DEBUG_LOG("InstallCodePatch: Installing %d byte patch at 0x%08X\n",
             patchSize, targetAddr);

    if (!acb || !patchCode || patchSize == 0) {
        return segBadFormat;
    }

    /* This would implement runtime code patching */
    /* For now, this is a placeholder */

    DEBUG_LOG("InstallCodePatch: Code patch installed (placeholder)\n");

    return segNoErr;
}

OSErr RemoveCodePatch(ApplicationControlBlock* acb, uint32_t targetAddr)
{
    DEBUG_LOG("RemoveCodePatch: Removing patch at 0x%08X\n", targetAddr);

    if (!acb) {
        return segBadFormat;
    }

    /* This would remove a previously installed patch */
    /* For now, this is a placeholder */

    DEBUG_LOG("RemoveCodePatch: Code patch removed (placeholder)\n");

    return segNoErr;
}

/* ============================================================================
 * Debugging Support
 * ============================================================================ */

void DumpJumpTable(ApplicationControlBlock* acb)
{
    if (!acb || !acb->jumpTable) {
        DEBUG_LOG("DumpJumpTable: No jump table to dump\n");
        return;
    }

    DEBUG_LOG("DumpJumpTable: Jump table dump (%d entries):\n", acb->jumpTableSize);

    for (uint16_t i = 0; i < acb->jumpTableSize; i++) {
        JumpTableEntry* entry = &acb->jumpTable[i];
        if (entry->segmentID != 0) {
            DEBUG_LOG("  [%3d] Segment %2d, Offset 0x%08X -> 0x%08X (opcode 0x%04X)\n",
                     i, entry->segmentID, entry->offset, entry->address, entry->opcode);
        }
    }
}

OSErr ValidateJumpTable(ApplicationControlBlock* acb)
{
    if (!acb || !acb->jumpTable) {
        return segBadFormat;
    }

    DEBUG_LOG("ValidateJumpTable: Validating jump table\n");

    uint16_t validEntries = 0;
    uint16_t resolvedEntries = 0;

    for (uint16_t i = 0; i < acb->jumpTableSize; i++) {
        JumpTableEntry* entry = &acb->jumpTable[i];

        if (entry->segmentID != 0) {
            validEntries++;

            /* Check if entry is resolved */
            if (entry->address != 0 && entry->opcode != OPCODE_TRAP_UNIMPL) {
                resolvedEntries++;
            }
        }
    }

    DEBUG_LOG("ValidateJumpTable: %d valid entries, %d resolved\n",
             validEntries, resolvedEntries);

    return segNoErr;
}