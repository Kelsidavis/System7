# Kernel Allocation Policy

Kernel code must use the System 7 Memory Manager APIs (`NewPtr`,
`NewPtrClear`, `DisposePtr`, `NewHandle`, and related routines) rather than
calling standard C allocation functions directly. This keeps ownership and
memory use within the kernel's zone allocator.

The Memory Manager also provides C-library compatibility wrappers backed by
that allocator. They are implementation details, not an alternative for new
kernel code.

## Automated check

Run the repository's allocation policy check with:

```sh
make check-malloc
```

`make check` and CI run this check as part of the quality gate. The checker
scans the C sources under `src/`, excluding `src/MemoryMgr/MemoryManager.c`,
where the compatibility wrappers are implemented. Comment-only lines are
ignored, including the documented `realloc` caveats in `Regions.c`. It is a
source scan, not a compile-time guard; review new allocations and keep the
checker aligned with any intentional exceptions.

## Examples

```c
Ptr buffer = NewPtrClear(byteCount);
if (buffer == NULL) {
    return MemError();
}

/* Use buffer. */
DisposePtr(buffer);
```

Use a `Handle` for data that may move during heap compaction. Lock it before
dereferencing and unlock it when finished; see [Memory Management](MEMORY_MANAGEMENT.md).
