/*
 * CodePatching.h - Code Patching and Jump Table Management APIs
 *
 * This file defines the API for jump table management, code patching,
 * and runtime code modification for Mac OS 7.1 applications.
 */

#ifndef _CODE_PATCHING_H
#define _CODE_PATCHING_H

#include <stdint.h>
#include <stdbool.h>
#include "ApplicationTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 68k Instruction Opcodes
 * ============================================================================ */

/* Jump and branch instructions */
#define OPCODE_JMP_ABSOLUTE         0x4EF9  /* JMP $nnnnnnnn */
#define OPCODE_JSR_ABSOLUTE         0x4EB9  /* JSR $nnnnnnnn */
#define OPCODE_JMP_INDIRECT_A0      0x4ED0  /* JMP (A0) */
#define OPCODE_JSR_INDIRECT_A0      0x4E90  /* JSR (A0) */
#define OPCODE_BRA_SHORT            0x6000  /* BRA.S */
#define OPCODE_BRA_WORD             0x6000  /* BRA.W */
#define OPCODE_BSR_SHORT            0x6100  /* BSR.S */
#define OPCODE_BSR_WORD             0x6100  /* BSR.W */

/* Load and move instructions */
#define OPCODE_MOVE_L_IMM_A0        0x207C  /* MOVE.L #$nnnnnnnn,A0 */
#define OPCODE_MOVE_L_IMM_A1        0x227C  /* MOVE.L #$nnnnnnnn,A1 */
#define OPCODE_LEA_PC_REL           0x41FA  /* LEA offset(PC),A0 */
#define OPCODE_PEA_ABSOLUTE         0x4879  /* PEA $nnnnnnnn */

/* Control instructions */
#define OPCODE_NOP                  0x4E71  /* NOP */
#define OPCODE_RTS                  0x4E75  /* RTS */
#define OPCODE_RTE                  0x4E73  /* RTE */
#define OPCODE_TRAP                 0x4E40  /* TRAP #n */
#define OPCODE_TRAP_UNIMPL          0xA89F  /* _Unimplemented trap */

/* Trap instructions */
#define TRAP_LOADSEG                0xA9F0  /* _LoadSeg */
#define TRAP_UNLOADSEG              0xA9F1  /* _UnloadSeg */
#define TRAP_EXITSHELL              0xA9F4  /* _ExitToShell */
#define TRAP_GETAPPPARMS            0xA9F5  /* _GetAppParms */

/* ============================================================================
 * Jump Table Constants
 * ============================================================================ */

/* Jump table entry states */
#define JUMP_UNRESOLVED             0x00    /* Not yet resolved */
#define JUMP_RESOLVED               0x01    /* Resolved to target */
#define JUMP_SEGMENT_LOADING        0x02    /* Currently loading segment */
#define JUMP_STUB                   0x03    /* Stub for lazy loading */
#define JUMP_PATCHED                0x04    /* Entry has been patched */

/* Jump table sizes */
#define MAX_JUMP_TABLE_ENTRIES      1024    /* Maximum jump table entries */
#define JUMP_ENTRY_SIZE             8       /* Size of jump table entry */
#define DEFAULT_JUMP_TABLE_SIZE     256     /* Default jump table size */

/* Patch types */
#define PATCH_TYPE_ABSOLUTE         0x01    /* Absolute address patch */
#define PATCH_TYPE_RELATIVE         0x02    /* PC-relative patch */
#define PATCH_TYPE_A5_RELATIVE      0x03    /* A5-relative patch */
#define PATCH_TYPE_INDIRECT         0x04    /* Indirect patch */

/* ============================================================================
 * Patch Information Structures
 * ============================================================================ */

typedef struct PatchInfo {
    uint32_t    targetAddr;             /* Target address to patch */
    uint32_t    patchAddr;              /* Patch code address */
    uint32_t    originalCode;           /* Original code at target */
    uint16_t    patchType;              /* Type of patch */
    uint16_t    patchSize;              /* Size of patch in bytes */
    bool        isInstalled;            /* Patch is installed */
    bool        isTemporary;            /* Temporary patch */
} PatchInfo;

typedef struct JumpTableInfo {
    uint16_t    entryCount;             /* Number of entries */
    uint32_t    tableSize;              /* Size of table in bytes */
    uint32_t    baseAddress;            /* Base address of table */
    uint16_t    resolvedEntries;        /* Number of resolved entries */
    uint16_t    unresolvedEntries;      /* Number of unresolved entries */
} JumpTableInfo;

typedef struct CodePatchContext {
    ApplicationControlBlock* acb;       /* Target application */
    uint32_t    patchCount;             /* Number of active patches */
    PatchInfo*  patches;                /* Array of patch info */
    void*       scratchMemory;          /* Scratch memory for patching */
    uint32_t    scratchSize;            /* Size of scratch memory */
} CodePatchContext;

/* ============================================================================
 * Jump Table Management
 * ============================================================================ */

/* Initialize jump table for application */
OSErr InitJumpTable(ApplicationControlBlock* acb);

/* Add entry to jump table */
OSErr AddJumpTableEntry(ApplicationControlBlock* acb, uint16_t segmentID,
                       uint32_t offset, uint32_t* entryAddr);

/* Patch jump table entry */
OSErr PatchJumpTableEntry(ApplicationControlBlock* acb, uint32_t entryAddr,
                         uint32_t newTarget);

/* Resolve jump table references */
OSErr ResolveJumpTableRefs(ApplicationControlBlock* acb);

/* Get jump table information */
OSErr GetJumpTableInfo(ApplicationControlBlock* acb, uint16_t* entryCount,
                      uint32_t* tableSize);

/* Get specific jump table entry */
OSErr GetJumpTableEntry(ApplicationControlBlock* acb, uint16_t index,
                       JumpTableEntry* entry);

/* Validate jump table integrity */
OSErr ValidateJumpTable(ApplicationControlBlock* acb);

/* Dump jump table for debugging */
void DumpJumpTable(ApplicationControlBlock* acb);

/* ============================================================================
 * Code Patching Functions
 * ============================================================================ */

/* Install runtime code patch */
OSErr InstallCodePatch(ApplicationControlBlock* acb, uint32_t targetAddr,
                      void* patchCode, uint32_t patchSize);

/* Remove code patch */
OSErr RemoveCodePatch(ApplicationControlBlock* acb, uint32_t targetAddr);

/* Install temporary patch */
OSErr InstallTemporaryPatch(ApplicationControlBlock* acb, uint32_t targetAddr,
                           void* patchCode, uint32_t patchSize);

/* Remove all temporary patches */
OSErr RemoveTemporaryPatches(ApplicationControlBlock* acb);

/* Get patch information */
OSErr GetPatchInfo(ApplicationControlBlock* acb, uint32_t targetAddr,
                  PatchInfo* info);

/* ============================================================================
 * Instruction Generation
 * ============================================================================ */

/* Generate JMP instruction */
uint32_t GenerateJumpInstruction(uint32_t targetAddr);

/* Generate JSR instruction */
uint32_t GenerateCallInstruction(uint32_t targetAddr);

/* Generate branch instruction */
uint32_t GenerateBranchInstruction(int32_t offset, bool conditional);

/* Generate MOVE.L immediate instruction */
uint64_t GenerateMoveImmediateInstruction(uint8_t reg, uint32_t value);

/* Generate trap instruction */
uint16_t GenerateTrapInstruction(uint16_t trapNumber);

/* Generate NOP instruction */
uint16_t GenerateNopInstruction(void);

/* ============================================================================
 * Code Analysis Functions
 * ============================================================================ */

/* Check if instruction is a jump */
bool IsJumpInstruction(uint16_t opcode);

/* Check if instruction is a call */
bool IsCallInstruction(uint16_t opcode);

/* Check if instruction is a branch */
bool IsBranchInstruction(uint16_t opcode);

/* Check if instruction is a trap */
bool IsTrapInstruction(uint16_t opcode);

/* Get instruction size */
uint16_t GetInstructionSize(uint16_t opcode);

/* Get branch target */
int32_t GetBranchTarget(uint16_t* instruction);

/* Get jump target */
uint32_t GetJumpTarget(uint16_t* instruction);

/* ============================================================================
 * Code Relocation
 * ============================================================================ */

/* Relocate code segment */
OSErr RelocateCodeSegment(uint8_t* codePtr, uint32_t codeSize,
                         uint32_t oldBase, uint32_t newBase);

/* Fix PC-relative references */
OSErr FixPCRelativeRefs(uint8_t* codePtr, uint32_t codeSize,
                       int32_t displacement);

/* Fix A5-relative references */
OSErr FixA5RelativeRefs(uint8_t* codePtr, uint32_t codeSize,
                       Ptr oldA5, Ptr newA5);

/* Fix absolute references */
OSErr FixAbsoluteRefs(uint8_t* codePtr, uint32_t codeSize,
                     uint32_t oldBase, uint32_t newBase);

/* ============================================================================
 * Segment Linking
 * ============================================================================ */

/* Link segment to application */
OSErr LinkSegmentToApp(ApplicationControlBlock* acb, uint16_t segmentID);

/* Unlink segment from application */
OSErr UnlinkSegmentFromApp(ApplicationControlBlock* acb, uint16_t segmentID);

/* Resolve inter-segment references */
OSErr ResolveInterSegmentRefs(ApplicationControlBlock* acb,
                             uint16_t fromSegment, uint16_t toSegment);

/* Create segment stub */
OSErr CreateSegmentStub(ApplicationControlBlock* acb, uint16_t segmentID,
                       uint32_t* stubAddr);

/* Replace segment stub with real code */
OSErr ReplaceSegmentStub(ApplicationControlBlock* acb, uint32_t stubAddr,
                        uint32_t realAddr);

/* ============================================================================
 * Dynamic Code Loading
 * ============================================================================ */

/* Setup lazy loading for segment */
OSErr SetupLazyLoading(ApplicationControlBlock* acb, uint16_t segmentID);

/* Handle lazy load trap */
OSErr HandleLazyLoadTrap(ApplicationControlBlock* acb, uint16_t segmentID);

/* Install lazy load stub */
OSErr InstallLazyLoadStub(uint32_t entryAddr, uint16_t segmentID);

/* Replace lazy load stub */
OSErr ReplaceLazyLoadStub(uint32_t entryAddr, uint32_t targetAddr);

/* ============================================================================
 * Code Verification
 * ============================================================================ */

/* Verify code segment integrity */
OSErr VerifyCodeSegment(SegmentDescriptor* segment);

/* Check code alignment */
bool IsCodeAligned(uint32_t address, uint16_t alignment);

/* Validate instruction sequence */
OSErr ValidateInstructionSequence(uint16_t* instructions, uint32_t count);

/* Check for illegal instructions */
OSErr CheckForIllegalInstructions(uint8_t* codePtr, uint32_t codeSize);

/* ============================================================================
 * Patch Context Management
 * ============================================================================ */

/* Create patch context */
OSErr CreatePatchContext(ApplicationControlBlock* acb,
                        CodePatchContext** context);

/* Destroy patch context */
OSErr DestroyPatchContext(CodePatchContext* context);

/* Begin patch operation */
OSErr BeginPatchOperation(CodePatchContext* context);

/* End patch operation */
OSErr EndPatchOperation(CodePatchContext* context);

/* Backup original code */
OSErr BackupOriginalCode(CodePatchContext* context, uint32_t addr,
                        uint32_t size);

/* Restore original code */
OSErr RestoreOriginalCode(CodePatchContext* context, uint32_t addr);

/* ============================================================================
 * Advanced Patching Features
 * ============================================================================ */

/* Install function hook */
OSErr InstallFunctionHook(ApplicationControlBlock* acb, uint32_t funcAddr,
                         void* hookFunc, void** originalFunc);

/* Remove function hook */
OSErr RemoveFunctionHook(ApplicationControlBlock* acb, uint32_t funcAddr);

/* Install interrupt patch */
OSErr InstallInterruptPatch(ApplicationControlBlock* acb, uint8_t intVector,
                           void* handler);

/* Remove interrupt patch */
OSErr RemoveInterruptPatch(ApplicationControlBlock* acb, uint8_t intVector);

/* Patch system trap */
OSErr PatchSystemTrap(uint16_t trapNumber, void* handler, void** original);

/* Restore system trap */
OSErr RestoreSystemTrap(uint16_t trapNumber);

/* ============================================================================
 * Code Cache Management
 * ============================================================================ */

/* Flush instruction cache */
OSErr FlushInstructionCache(uint32_t addr, uint32_t size);

/* Invalidate code cache */
OSErr InvalidateCodeCache(ApplicationControlBlock* acb);

/* Synchronize code and data caches */
OSErr SynchronizeCaches(uint32_t addr, uint32_t size);

/* ============================================================================
 * Debugging and Diagnostics
 * ============================================================================ */

/* Disassemble instruction */
OSErr DisassembleInstruction(uint16_t* instruction, char* buffer,
                            uint32_t bufferSize);

/* Dump code segment */
void DumpCodeSegment(SegmentDescriptor* segment, uint32_t maxBytes);

/* Trace code execution */
OSErr EnableCodeTracing(ApplicationControlBlock* acb, bool enable);

/* Get execution statistics */
OSErr GetExecutionStats(ApplicationControlBlock* acb, uint32_t* stats);

/* Validate all patches */
OSErr ValidateAllPatches(ApplicationControlBlock* acb);

/* ============================================================================
 * Platform-Specific Functions
 * ============================================================================ */

/* Platform-specific code patching */
OSErr PlatformPatchCode(uint32_t addr, void* patch, uint32_t size);

/* Platform-specific code verification */
OSErr PlatformVerifyCode(uint8_t* code, uint32_t size);

/* Platform-specific cache management */
OSErr PlatformManageCache(uint32_t addr, uint32_t size, uint8_t operation);

#ifdef __cplusplus
}
#endif

#endif /* _CODE_PATCHING_H */