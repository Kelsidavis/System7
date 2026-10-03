# Memory Management Guidelines

## How Big the Heaps Are

There are two zones. The **system zone** is a fixed 2 MB array in the kernel.
The **application zone**, where windows, their offscreen buffers, dialogs and
documents live, is sized from the machine:

- **x86**: it takes the RAM from the end of the kernel image to the top of the
  contiguous memory the boot loader reports, less 1 MB
  (`Platform_GetFreeMemory` in `src/Platform/x86/hal_boot.c`). Under QEMU with
  `-m 1024` that is about 1 GB, with 65,536 master pointers.
- **Other platforms**, or an x86 machine that reports no memory, fall back to a
  fixed array: 24 MB (8 MB on x86).

Every window keeps a 32-bit offscreen buffer the size of its content, so a
full-screen window at 800x600 needs 1.9 MB. The 68K emulator sees only the
first 16 MB of the application zone, which is all a 68K program can address.

## ⚠️ CRITICAL: Do Not Use Standard C Allocators

**Do not call `malloc()`, `free()`, `calloc()`, or `realloc()` directly in new
kernel code.** Use the Toolbox Memory Manager API so allocation intent and
ownership remain explicit.

### Why?

Mac OS System 7 uses a proprietary **Memory Manager** with its own heap structures. The Memory Manager maintains:
- Relocatable blocks (Handles)
- Non-relocatable blocks (Pointers)
- Heap compaction and defragmentation
- Master pointer tables
- Circular linked lists for free blocks

The project's C-library compatibility functions are implemented by the Memory
Manager and backed by its zone allocator, not by a separate host heap. The
policy is still to prefer `NewPtr`/`DisposePtr` and handle APIs: they expose the
Toolbox semantics explicitly and are covered by the repository's allocation
audit. See [Kernel Allocation Policy](MALLOC_PREVENTION.md).

---

## ✅ Correct Usage

### Basic Allocation

| ❌ WRONG (Standard C) | ✅ CORRECT (Memory Manager) |
|----------------------|----------------------------|
| `void* ptr = malloc(1024);` | `Ptr ptr = NewPtr(1024);` |
| `void* ptr = calloc(1, 1024);` | `Ptr ptr = NewPtrClear(1024);` |
| `void* ptr = calloc(10, sizeof(Type));` | `Ptr ptr = NewPtrClear(10 * sizeof(Type));` |
| `free(ptr);` | `DisposePtr(ptr);` |

### Error Checking

```c
// Memory Manager style
Ptr buffer = NewPtr(1024);
if (!buffer) {
    OSErr err = MemError();
    // Handle error: err will be memFullErr (-108)
    return err;
}

// Use buffer...

DisposePtr(buffer);
```

### Realloc Pattern

There is no Toolbox `realloc` routine. To resize a pointer allocation, allocate
a replacement, copy the retained bytes, then dispose the old pointer:

```c
// OLD (Standard C):
newPtr = realloc(oldPtr, newSize);

// NEW (Memory Manager):
Ptr newPtr = NewPtr(newSize);
if (!newPtr) {
    return MemError();
}

if (oldPtr) {
    Size copySize = (newSize < oldSize) ? newSize : oldSize;
    BlockMove(oldPtr, newPtr, copySize);
    DisposePtr(oldPtr);
}

// Now use newPtr...
```

### Handles (Relocatable Memory)

For larger allocations that can be moved during heap compaction:

```c
Handle h = NewHandle(32768);  // 32KB
if (!h) return MemError();

// Lock before dereferencing
HLock(h);
Ptr data = *h;
// Use data...
HUnlock(h);

// When done
DisposeHandle(h);
```

---

## Allocation Audit

`make check-malloc` runs the source scanner in
[`scripts/check_malloc_violations.sh`](../scripts/check_malloc_violations.sh).
`make check` and CI include this check. It is a source-level audit rather than
a compile-time ban; see [Kernel Allocation Policy](MALLOC_PREVENTION.md) for its
scope and exception.

---

## 🔧 Memory Manager API Reference

### Non-Relocatable Blocks (Pointers)

```c
Ptr     NewPtr(Size byteCount);              // Allocate
Ptr     NewPtrClear(Size byteCount);         // Allocate and zero
void    DisposePtr(Ptr p);                   // Free
Size    GetPtrSize(Ptr p);                   // Get size
void    SetPtrSize(Ptr p, Size newSize);     // Resize (if possible)
OSErr   MemError(void);                      // Get last error
```

### Relocatable Blocks (Handles)

```c
Handle  NewHandle(Size byteCount);           // Allocate relocatable
Handle  NewHandleClear(Size byteCount);      // Allocate and zero
void    DisposeHandle(Handle h);             // Free
Size    GetHandleSize(Handle h);             // Get size
void    SetHandleSize(Handle h, Size newSize); // Resize
void    HLock(Handle h);                     // Lock (prevent relocation)
void    HUnlock(Handle h);                   // Unlock
```

### Utility Functions

```c
void    BlockMove(const void* src, void* dst, Size count);  // Fast copy
Size    FreeMem(void);                       // Available memory
Size    MaxMem(Size* grow);                  // Maximum contiguous block
void    PurgeMem(Size cbNeeded);             // Purge purgeable blocks
Size    CompactMem(Size cbNeeded);           // Compact heap
```

---

---

## 📚 Additional Resources

- **Inside Macintosh: Memory** - Original Apple documentation
- `include/MemoryMgr/MemoryManager.h` - API declarations
- `src/MemoryMgr/MemoryManager.c` - Implementation

---

## 🐛 Troubleshooting

### "Broken circular link" Error

**Cause:** Mixed malloc/free with NewPtr/DisposePtr
**Solution:** Audit code for standard C allocators and convert to Memory Manager

### Memory Leak

**Cause:** Allocated with NewPtr but never called DisposePtr
**Solution:** Ensure all NewPtr/NewHandle calls have matching Dispose calls

---

## ✅ Summary

1. **ALWAYS** use Memory Manager (NewPtr/DisposePtr)
2. **NEVER** use standard C allocators (malloc/free)
3. **RUN** `make check-malloc` after allocation-related changes
4. **REVIEW** this document when in doubt

**Following these guidelines prevents heap corruption and ensures system stability.**
