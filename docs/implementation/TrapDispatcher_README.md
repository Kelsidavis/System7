# Mac OS 7.1 Trap Dispatcher - Portable C Implementation

## Overview

This directory contains a complete portable C implementation of the Mac OS 7.1 Trap Dispatcher, converted from the original 68k assembly code found in `OS/TrapDispatcher/`. The Trap Dispatcher is one of the most critical components of Mac OS, as it routes all system calls (A-line traps) and handles F-line traps for unimplemented opcodes.

## Original Assembly Analysis

The original implementation consisted of these key files:
- `OS/TrapDispatcher/Dispatch.a` - Main trap dispatcher (Andy Hertzfeld, 1982; optimized by Gary Davidian, 1988)
- `OS/TrapDispatcher/DispatchPatch.a` - Optimized dispatchers for Mac Plus, SE, and II
- `OS/TrapDispatcher/TrapDispatcher.make` - Build configuration

### Key Features Preserved

1. **A-line Trap Dispatch** - Routes Mac OS system calls (A000-AFFF range)
2. **F-line Trap Handling** - Handles unimplemented opcodes and coprocessor instructions
3. **Dual Trap Tables** - Separate OS (256 entries) and Toolbox (1024 entries) tables
4. **Register Conventions** - Preserves Mac OS register passing and saving conventions
5. **Trap Patching** - Supports runtime patching with come-from header mechanism
6. **Extended Tables** - Mac Plus/SE compatibility with extended trap ranges
7. **Cache Coherency** - Instruction cache flushing for self-modifying code

## Architecture

### Trap Format

A-line traps follow the format: `1010 tabc nnnn nnnn`

- `t=1`: Toolbox trap, `t=0`: OS trap
- `a=1`: AutoPop flag (remove extra return address)
- `b,c`: Modifier bits for register preservation
- `nnnnnnnn`: 8-bit trap number

### Memory Layout

```
OS Table:      256 entries x 4 bytes = 1024 bytes  (0x0400-0x0800)
Toolbox Table: 1024 entries x 4 bytes = 4096 bytes (0x0E00-0x1E00)
Extended Table: 512 entries x 4 bytes = 2048 bytes (allocated dynamically)
```

### Register Conventions

**OS Traps (A000-A7FF):**
- Parameters passed in registers (D0, A0-A1)
- D1.W contains the trap word
- Register preservation controlled by modifier bits:
  - Bit 8 clear: Save D1-D2/A0-A1 (standard)
  - Bit 8 set: Save D1-D2/A1 only (A0 not saved)
  - Bit 9 set: Save D1-D2/A1 instead of D1-D2/A0-A1

**Toolbox Traps (A800-AFFF):**
- Follow Pascal calling conventions
- Parameters passed on stack
- All registers preserved except D0-D2/A0-A1
- AutoPop removes extra return address if bit 10 set

## Files

### Core Implementation
- **`include/TrapDispatcher.h`** - Public API and data structures
- **`src/TrapDispatcher.c`** - Main implementation with all trap dispatch logic
- **`src/TrapDispatcherExample.c`** - Complete example and test program
- **`TrapDispatcher.mk`** - Build system for the trap dispatcher

### Key Functions

#### Initialization
```c
int TrapDispatcher_Initialize(void);
void TrapDispatcher_Cleanup(void);
int TrapDispatcher_InitializeExtendedTable(void);
```

#### Trap Dispatch
```c
int32_t TrapDispatcher_DispatchATrap(TrapContext *context);
int32_t TrapDispatcher_DispatchFTrap(FLineTrapContext *context);
```

#### Trap Management
```c
TrapHandler TrapDispatcher_GetTrapAddress(uint16_t trap_number, uint16_t trap_word);
int TrapDispatcher_SetTrapAddress(uint16_t trap_number, uint16_t trap_word, TrapHandler handler);
```

#### System Integration
```c
void TrapDispatcher_FlushCache(void);
void TrapDispatcher_SetCacheFlushFunction(void (*flush_func)(void));
FLineTrapHandler TrapDispatcher_SetFLineHandler(FLineTrapHandler handler);
```

## Usage Example

```c
#include "TrapDispatcher.h"

// Example trap handler
int32_t my_trap_handler(TrapContext *context) {
    printf("Trap called with D0=0x%08X\n", context->d0);
    return 0;  // noErr
}

int main() {
    // Initialize the dispatcher
    TrapDispatcher_Initialize();

    // Install a trap handler for toolbox trap 0x100
    TrapDispatcher_SetTrapAddress(0x100,
                                 (1 << TRAP_NEW_BIT) | (1 << TRAP_TOOLBOX_BIT),
                                 my_trap_handler);

    // Dispatch a trap (normally called by exception handler)
    TrapContext context = {0};
    uint16_t trap_instruction = 0xA900;  // Toolbox trap A900
    context.pc = (uint32_t)&trap_instruction;

    int32_t result = TrapDispatcher_DispatchATrap(&context);

    // Cleanup
    TrapDispatcher_Cleanup();
    return 0;
}
```

## Building

```bash
# Build the example program
make -f TrapDispatcher.mk all

# Run the test suite
make -f TrapDispatcher.mk test

# Build as a static library
make -f TrapDispatcher.mk lib

# Clean build artifacts
make -f TrapDispatcher.mk clean
```

## Integration Notes

### Exception Handler Integration

To integrate with a 68k emulator or modern system:

1. Install exception handlers for vectors 0x28 (Line-A) and 0x2C (Line-F)
2. Extract CPU context and call `TrapDispatcher_DispatchATrap()` or `TrapDispatcher_DispatchFTrap()`
3. Restore CPU context with any modifications made by trap handlers

### Cache Coherency

The dispatcher supports platform-specific cache flushing:

```c
void my_cache_flush(void) {
    // Platform-specific instruction cache flush
    __builtin___clear_cache(0, (void*)-1);
}

TrapDispatcher_SetCacheFlushFunction(my_cache_flush);
```

### System Error Handling

Provide a `SysError()` function for system error reporting:

```c
void SysError(int error_code) {
    fprintf(stderr, "System Error %d\n", error_code);
    // Handle system error appropriately
}
```

## Mac OS Compatibility

This implementation maintains exact compatibility with Mac OS 7.1 trap dispatch behavior:

1. **Trap Numbering** - Uses original Mac OS trap numbering scheme
2. **Register Conventions** - Preserves exact register usage patterns
3. **Stack Management** - Handles AutoPop and stack frame manipulation
4. **Error Codes** - Returns standard Mac OS error codes
5. **Patch Mechanism** - Supports come-from patch headers for runtime patching

## Performance

The C implementation is optimized for modern platforms while preserving Mac OS semantics:

- Direct table lookup (O(1) trap dispatch)
- Minimal overhead for common trap types
- Efficient come-from patch resolution
- Optional extended table support

## Testing

The included test program validates:
- Basic A-line and F-line trap dispatch
- Trap handler installation and patching
- Extended table functionality
- Register preservation
- Cache flushing integration
- Error handling

## Platform Support

Tested on:
- Linux x86_64
- Linux ARM64
- macOS x86_64
- macOS ARM64

The implementation uses standard C99 and should compile on any POSIX-compliant system.

## Critical Importance for Mac OS Compatibility

The Trap Dispatcher is **the most critical component** for Mac OS compatibility. Without it:

- No Mac applications can run (all use A-line traps)
- System calls cannot be routed to implementations
- Toolbox functions are completely inaccessible
- OS-level functionality is unavailable

This portable implementation provides the foundation for running Mac OS software on modern platforms by translating 68k trap instructions into C function calls while preserving all original behavioral semantics.

## Historical Context

The original trap dispatcher was written by Andy Hertzfeld in July 1982 and was fundamental to the original Macintosh architecture. Gary Davidian's 1988 optimization improved performance significantly. This C conversion preserves that optimized logic while making it portable to modern architectures.

The trap system enabled Mac OS to:
- Provide a stable API across different hardware
- Support runtime patching and system extensions
- Maintain backward compatibility across Mac OS versions
- Enable efficient system call dispatch

This portable implementation continues that legacy for modern Mac OS emulation and compatibility layers.