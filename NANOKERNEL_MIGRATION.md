# Nanokernel Memory Manager Migration

## Overview

This document describes the migration strategy to integrate the C23 nanokernel memory manager as the foundation for the Classic Mac Memory Manager.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│         Classic Mac Memory Manager (API Layer)      │
│  NewPtr, NewHandle, DisposePtr, DisposeHandle, etc. │
│  Zones, Handles, Relocatable blocks, Compaction     │
└─────────────────────────────────────────────────────┘
                        ↓ (uses)
┌─────────────────────────────────────────────────────┐
│         Nanokernel Memory Manager (Backend)         │
│     kmalloc, kfree, krealloc, pmm_alloc_page        │
│     PMM (Physical Memory Manager) + Kernel Heap     │
└─────────────────────────────────────────────────────┘
```

## Current State

- **Classic MM**: Uses static arrays for heap storage
  - `gSystemHeap[2MB]` - System heap
  - `gAppHeap[6MB]` - Application heap
  - Total: 8MB fixed allocation

- **Nanokernel**: Implemented but not integrated
  - PMM manages physical pages (4KB)
  - Kernel heap provides kmalloc/kfree
  - Supports dynamic sizing from 8MB to terabytes

## Migration Strategy

### Phase 1: Boot Integration (CURRENT)

1. Initialize nanokernel memory manager first in boot sequence
2. Allocate Classic MM heap storage from nanokernel
3. Keep all Classic MM APIs unchanged

**Changes Required:**
- `src/main.c` or `src/boot.c`: Add nanokernel init before Classic MM init
- `src/MemoryMgr/MemoryManager.c`: Replace static arrays with kmalloc

### Phase 2: Dynamic Sizing

1. Detect total system memory from multiboot2
2. Allocate Classic MM heaps proportionally
3. Example for 1GB system:
   - System zone: 64MB
   - App zone: 256MB
   - Remaining: Available for nanokernel/future use

### Phase 3: Optimization (Future)

1. Consider using PMM directly for large allocations
2. Page-aligned allocations for better performance
3. Memory protection integration

## Implementation Plan

### Step 1: Initialize Nanokernel at Boot

```c
// In src/main.c or src/boot.c
void kernel_main(void) {
    // Initialize nanokernel memory manager
    pmm_init(total_memory_bytes, 0x100000);  // Start at 1MB
    kheap_init(0x200000, 0x800000);          // 6MB kernel heap

    // Then initialize Classic MM (which will use nanokernel)
    InitMemoryManager();

    // Rest of system init...
}
```

### Step 2: Update Classic MM Initialization

```c
// In src/MemoryMgr/MemoryManager.c
void InitMemoryManager(void) {
    extern void serial_puts(const char* str);
    serial_puts("MM: InitMemoryManager started\n");

    // Allocate system heap from nanokernel
    u8* systemHeap = (u8*)kmalloc(2 * 1024 * 1024);
    if (!systemHeap) {
        serial_puts("MM: FATAL - Failed to allocate system heap\n");
        return;
    }

    // Allocate app heap from nanokernel
    u8* appHeap = (u8*)kmalloc(6 * 1024 * 1024);
    if (!appHeap) {
        serial_puts("MM: FATAL - Failed to allocate app heap\n");
        kfree(systemHeap);
        return;
    }

    // Initialize zones with allocated memory
    InitZone(&gSystemZone, systemHeap, 2 * 1024 * 1024,
             gSystemMasters, sizeof(gSystemMasters)/sizeof(void*));
    gSystemZone.name[0] = 'S'; gSystemZone.name[1] = 0;

    InitZone(&gAppZone, appHeap, 6 * 1024 * 1024,
             gAppMasters, sizeof(gAppMasters)/sizeof(void*));
    gAppZone.name[0] = 'A'; gAppZone.name[1] = 0;

    gCurrentZone = &gAppZone;
    serial_puts("MM: InitMemoryManager complete\n");
}
```

### Step 3: Remove Static Arrays

```c
// OLD (remove):
// static u8 gSystemHeap[2 * 1024 * 1024];
// static u8 gAppHeap[6 * 1024 * 1024];

// NEW (add):
static u8* gSystemHeap = NULL;
static u8* gAppHeap = NULL;
```

## Benefits

1. **Dynamic Sizing**: Memory allocation based on actual available RAM
2. **Unified Backend**: Single memory manager for all allocations
3. **Better Scalability**: Supports systems from 8MB to terabytes
4. **Cleaner Architecture**: Layered design with clear separation
5. **API Compatibility**: All Classic Mac APIs remain unchanged

## Risks and Mitigations

### Risk: Initialization Order
- **Mitigation**: Carefully order boot sequence (nanokernel → Classic MM → rest)

### Risk: Double Free/Corruption
- **Mitigation**: Never call kfree on Classic MM heap allocations (only on heap arrays themselves)

### Risk: Alignment Issues
- **Mitigation**: Nanokernel uses 4KB alignment, Classic MM uses 8-byte alignment (compatible)

## Testing Strategy

1. Boot test: Verify system boots with nanokernel integration
2. Memory test: Run nanokernel test suite (nk_memory_run_tests)
3. Classic MM test: Verify NewPtr/NewHandle/DisposePtr work correctly
4. Window test: Ensure window open/close works (regression test)
5. Stress test: Multiple window operations, memory pressure

## Files Modified

1. `src/main.c` or `src/boot.c` - Add nanokernel initialization
2. `src/MemoryMgr/MemoryManager.c` - Replace static arrays with kmalloc
3. `include/MemoryMgr/MemoryManager.h` - Update heap pointer declarations

## Success Criteria

- [x] Nanokernel memory manager compiles and links
- [ ] Boot sequence initializes nanokernel before Classic MM
- [ ] Classic MM successfully allocates heaps from nanokernel
- [ ] All existing functionality works (windows, menus, dialogs)
- [ ] No memory corruption or crashes
- [ ] Performance is equal or better than static allocation
