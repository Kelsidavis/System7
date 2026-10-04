# Quick Reference - Common Commands

Fast lookup for the most common System 7 development tasks.

## Build & Run

```bash
make              # Build kernel (x86, English)
make run          # Build and run in QEMU
make clean        # Clean all build artifacts
make info         # Show build statistics
```

## Running by Hand

```bash
qemu-system-i386 -cdrom system71.iso -m 1024 -vga std \
    -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 \
    -serial file:/tmp/serial.log
```

## Languages

```bash
make LOCALE_FR=1        # English plus French
make LOCALE_ALL=1       # English plus all 37 locales (38 languages total); choose with lang=xx
```

## Debugging

```bash
make debug                                      # Start QEMU with GDB
gdb kernel.elf -ex "target remote :1234"      # Connect GDB

# In GDB
break InitMenus                # Set breakpoint
continue                       # Resume
next/step                      # Step through code
print variable_name            # Inspect variable
quit                          # Exit GDB
```

## Testing

The repository quality gates are:

```bash
make check           # x86 build/layout, allocators, docs, Python/shell checks, tests, and exports
make analyze         # x86 build with GCC's static analyzer
make analyze-arm     # ARM32 build with GCC's static analyzer
make analyze-arm64   # ARM64 build with GCC's static analyzer
make check-arm64     # ARM64 build and segment-permission checks
make test-input      # native input polling, event queue, platform stubs, and C++ linkage
make test-integration # build the ISO, boot it in QEMU, and collect test results
```

`make check` requires Ruff for Python lint and formatting. See the
[development setup](CONTRIBUTING.md#development-setup) for installing the pinned
tool. `make test-integration` requires `grub-mkrescue` and QEMU.

## File Locations

| What | Where |
|------|-------|
| Main README | `README.md` |
| Documentation | `docs/` |
| Component guides | `docs/components/` |
| Getting started | `docs/GETTING_STARTED.md` |
| API headers | `include/` (organized by subsystem) |
| Implementation | `src/` (organized by subsystem/platform) |
| Localization | `resources/strings/*.json` |
| Build config | `Makefile` |

## Documentation

- [Documentation index](INDEX.md) — start here
- [Getting started](GETTING_STARTED.md) — setup and first run
- [Known issues](KNOWN_ISSUES.md) — limitations and workarounds
- [Component guides](components/README.md) — subsystem references
- [Contributing](CONTRIBUTING.md) — development setup and conventions
- [Claude Code guidance](../CLAUDE.md)

## Common Issues

| Problem | Solution |
|---------|----------|
| Compiler not found | Install the selected platform toolchain; macOS x86: `brew install i686-elf-gcc` |
| `grub-mkrescue not found` | Install `grub-pc-bin` |
| `xorriso not found` | Install `xorriso` |
| `qemu-system-i386 not found` | Install `qemu-system-x86` |
| Build fails with errors | Try `make clean && make` |
| QEMU won't boot | Check `system71.iso` exists |
| No graphics in QEMU | Try `-display sdl` or `-display curses` |
| Serial output not showing | Add `-serial stdio` to QEMU command |

## Development Workflow

1. **Understand component**: Read the relevant guide in `docs/components/`; guides may
   be a Markdown file or a subsystem directory (see [the documentation index](INDEX.md#components)).
2. **Locate code**: Find in `include/` and `src/`
3. **Make change**: Edit the relevant file
4. **Validate**: Run `make check` for the local quality gate
5. **Test behavior**: Run `make run` and verify the change in QEMU when applicable
6. **Check serial**: Look for error messages in QEMU output
7. **Commit**: Stage only the files for this change and create a descriptive commit
8. **Push**: `git push -u origin HEAD` to publish the current branch

## Useful Grep Patterns

```bash
# Find TODO/FIXME comments
rg "TODO|FIXME" include/ src/

# Find HACK comments (workarounds)
rg "HACK:" include/ src/

# Find malloc violations (kernel code shouldn't use malloc)
rg "malloc|free" include/ src/

# Find unimplemented functions
rg "NOT YET IMPLEMENTED" include/ src/

# Find likely function declarations and definitions
rg -n '^[[:alnum:]_[:space:]*]+[[:space:]*][[:alnum:]_]+[[:space:]]*\\(' include/ src/
```

## Environment Variables

```bash
# Set build platform
make PLATFORM=x86          # x86 (default)
make PLATFORM=arm          # ARM (experimental)
make PLATFORM=arm64        # ARM64
make PLATFORM=ppc          # PowerPC (experimental)
```

## Git Quick Ref

```bash
git status                 # See what changed
git diff                   # See changes in detail
git add path/to/file      # Stage only the files for this change
git commit -m "message"    # Create commit
git push -u origin HEAD    # Push the current branch
git log --oneline          # See recent commits
git log -1                 # See latest commit details
```

## Testing Levels

### Level 1: Does it compile?
```bash
make clean && make
```

### Level 2: Does it boot?
```bash
make run
# Watch for messages, check no crashes
```

### Level 3: Do features work?
```bash
make run
# Click around, test specific features
# Check serial output for errors
```

### Level 4: Does it handle edge cases?
```bash
# Intentionally try to break things
# Rapid clicks, unusual sequences, etc.
# Document crashes and unexpected behavior
```

### Level 5: Bare metal?
Physical validation is limited to one UEFI ThinkPad. Broader hardware coverage
is unverified; report the machine and configuration with any new results.

## Resource Generation

```bash
# Generate a resource file from a JSON manifest
python3 gen_rsrc.py patterns.json /tmp/system7-patterns-preview.rsrc

# Regenerate the built-in color icon source
python3 scripts/create_color_icons.py
```

## Key Concepts

- **Zone**: Memory allocation system (not malloc)
- **Resource**: Data type (STR#, PPAT, ICON, etc.)
- **Manager**: System subsystem (Window, Menu, Event)
- **QEMU**: Emulator used for testing
- **Bare metal**: Physical hardware; currently verified on one UEFI ThinkPad
- **HACK**: Workaround in code that may need follow-up

## Need Help?

1. Check `docs/INDEX.md` for docs navigation
2. Search `docs/KNOWN_ISSUES.md` for known problems
3. Read `CLAUDE.md` for development guidance
4. Check `docs/components/` for specific topics
5. Open a GitHub Issue if stuck

---

**Pro Tip**: Bookmark `docs/INDEX.md` — it's the gateway to everything.
