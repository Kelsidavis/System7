# System 7X Nanokernel Memory Manager

A modern, C23-based 64-bit memory management system for the System 7X nanokernel layer.

## Overview

This memory manager provides two key components:

1. **Physical Memory Manager (PMM)** - Bitmap-based allocator for 4KB pages
2. **Kernel Heap** - Simple first-fit allocator for variable-sized kernel allocations

## Features

- ✅ **Pure C23** - Uses modern C features like `nullptr`, `auto`, designated initializers
- ✅ **Freestanding** - No libc dependencies, suitable for bare-metal execution
- ✅ **64-bit Native** - Handles memory sizes from 8MB to multiple terabytes
- ✅ **Page-aligned** - All allocations are 4KB aligned for optimal performance
- ✅ **Deterministic** - No undefined behavior, predictable operation
- ✅ **Well-tested** - Comprehensive test suite included

## Architecture

### Physical Memory Manager

```
┌─────────────────────────────────────────────┐
│         Physical Memory (8 GiB)             │
├─────────────────────────────────────────────┤
│  Bitmap (2 MB)    │    Free Pages           │
│  [###########.............................]  │
└─────────────────────────────────────────────┘
   ^                 ^
   │                 │
   Allocated         Available for allocation
```

**Features:**
- O(n) allocation (linear scan of bitmap)
- O(1) deallocation (clear bit)
- 1 bit per page overhead (0.003% for 4KB pages)
- Self-hosted bitmap (uses beginning of memory region)

### Kernel Heap

```
┌──────────────────────────────────────────┐
│           Kernel Heap (6 MB)             │
├──────────────────────────────────────────┤
│ [Block A│Free│Block B│────Free────│···] │
│  Used    List  Used      List            │
└──────────────────────────────────────────┘
   ^       ^      ^         ^
   │       │      │         │
   Header  Next   Header    Available space
```

**Features:**
- First-fit allocation strategy
- Block splitting to reduce fragmentation
- LIFO free list for cache locality
- Page-aligned allocations
- Supports `kmalloc`, `kfree`, `krealloc`

## API Reference

### Physical Memory Manager

```c
void     pmm_init(uint64_t mem_size_bytes, uintptr_t phys_base);
void    *pmm_alloc_page(void);
void     pmm_free_page(void *addr);
uint64_t pmm_total_pages(void);
uint64_t pmm_free_pages(void);
```

### Kernel Heap

```c
void  kheap_init(uintptr_t heap_start, uintptr_t heap_end);
void *kmalloc(size_t size);
void  kfree(void *ptr);
void *krealloc(void *ptr, size_t new_size);
```

### Diagnostics

```c
void mem_print_stats(void);  // Prints memory usage statistics
```

## Usage Example

```c
#include "Nanokernel/nk_memory.h"

void kernel_main(void) {
    // Initialize PMM with 8 GiB starting at 1 MB
    pmm_init(8ull * 1024 * 1024 * 1024, 0x100000);

    // Initialize kernel heap (6 MB region)
    kheap_init(0x200000, 0x800000);

    // Allocate memory
    void *buffer = kmalloc(1 << 20);  // 1 MiB

    // Use the buffer
    // ...

    // Free memory
    kfree(buffer);

    // Print statistics
    mem_print_stats();
}
```

## Testing

A comprehensive test suite is provided in `nk_memory_test.c`:

```c
void nk_memory_run_tests(void);  // Run all tests
void nk_memory_example(void);    // Run usage example
```

Tests cover:
- PMM basic operations
- PMM allocation/deallocation
- Kernel heap basic allocation
- Kernel heap reallocation
- Stress testing with 100+ allocations

## Performance Characteristics

| Operation           | Time Complexity | Space Overhead |
|---------------------|-----------------|----------------|
| PMM Allocation      | O(n) pages      | 1 bit/page     |
| PMM Deallocation    | O(1)            | -              |
| Heap Allocation     | O(n) blocks     | 1 header/block |
| Heap Deallocation   | O(1)            | -              |
| Heap Reallocation   | O(n) bytes      | -              |

## Memory Layout

Typical configuration for 8 GiB system:

```
0x000000 - 0x0FFFFF   : Low Memory (1 MB, reserved)
0x100000 - 0x2FFFFF   : PMM Bitmap (~2 MB)
0x200000 - 0x7FFFFF   : Kernel Heap (6 MB)
0x800000 - 0x1FFFFFFFF: Physical Pages (managed by PMM)
```

## Future Enhancements

- [ ] Coalescing adjacent free blocks in kernel heap
- [ ] Free list segregation by size class
- [ ] Slab allocator for fixed-size objects
- [ ] Virtual memory integration (page tables)
- [ ] Guard pages for overflow detection
- [ ] Memory protection bits
- [ ] NUMA-aware allocation

## Comparison with Classic Mac Memory Manager

| Feature               | Classic Mac MM | Nanokernel MM |
|-----------------------|----------------|---------------|
| Address Space         | 32-bit         | 64-bit        |
| Block Alignment       | 8 bytes        | 4096 bytes    |
| Relocatable Blocks    | Yes (Handles)  | No            |
| Zone Support          | Yes            | No            |
| Master Pointer Table  | Yes            | No            |
| Compaction            | Yes            | No (not needed)|
| Performance           | O(n) lists     | O(n) bitmap   |

The Classic Mac Memory Manager is preserved in `src/MemoryMgr/` for System 7.1
emulation, while the Nanokernel Memory Manager provides modern infrastructure
for the kernel itself.

## License

Part of the System 7X project.

## Authors

- Original C23 design by user
- Implementation and integration by Claude
