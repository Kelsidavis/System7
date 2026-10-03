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
make INTEGRATION_TESTS=1 && make iso   # integration tests run at boot
```

## Languages

```bash
make LOCALE_FR=1        # English plus French
make LOCALE_ALL=1       # all 38; choose one at boot with lang=xx
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

```bash
make run                                    # Quick test in QEMU
make info                                   # Show build configuration
make clean && make run                     # Clean rebuild + test
qemu-system-i386 -cdrom system71.iso ...   # Manual QEMU launch
```

## File Locations

| What | Where |
|------|-------|
| Main README | `README.md` |
| Documentation | `docs/` |
| Component guides | `docs/components/` |
| Getting started | `docs/GETTING_STARTED.md` |
| API headers | `include/ComponentName/` |
| Implementation | `src/ComponentName/` |
| Localization | `resources/strings/*.json` |
| Build config | `Makefile` |

## Documentation

```bash
# Navigate documentation
docs/INDEX.md              # Start here for docs
docs/GETTING_STARTED.md    # 5-minute quick start
docs/PROJECT_EVOLUTION.md  # Understand the project
docs/KNOWN_ISSUES.md       # Known limitations
docs/components/           # Component deep dives
CLAUDE.md                  # Claude Code guidance
```

## Common Issues

| Problem | Solution |
|---------|----------|
| `gcc: command not found` | Install `build-essential` |
| `grub-mkrescue not found` | Install `grub-pc-bin` |
| `xorriso not found` | Install `xorriso` |
| `qemu-system-i386 not found` | Install `qemu-system-x86` |
| Build fails with errors | Try `make clean && make` |
| QEMU won't boot | Check `system71.iso` exists |
| No graphics in QEMU | Try `-display sdl` or `-display curses` |
| Serial output not showing | Add `-serial stdio` to QEMU command |

## Development Workflow

1. **Understand component**: Read `docs/components/ComponentName/`
2. **Locate code**: Find in `include/` and `src/`
3. **Make change**: Edit the relevant file
4. **Rebuild**: `make clean && make run`
5. **Test**: Boot in QEMU, verify behavior
6. **Check serial**: Look for error messages in QEMU output
7. **Commit**: Create descriptive commit message
8. **Push**: `git push origin main`

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

# Find specific function definitions
rg "^[a-zA-Z_][a-zA-Z0-9_]*\\(" include/ src/
```

## Environment Variables

```bash
# Set build platform
make PLATFORM=x86          # x86 (default)
make PLATFORM=arm          # ARM (experimental)
make PLATFORM=ppc          # PowerPC (experimental)
```

## Git Quick Ref

```bash
git status                 # See what changed
git diff                   # See changes in detail
git add .                  # Stage all changes
git commit -m "message"    # Create commit
git push origin main       # Push to GitHub
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
```bash
# This is mostly untested
# If you have hardware, try it and document findings!
```

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
- **Bare metal**: Real hardware (untested)
- **HACK**: Workaround in code that may need follow-up

## Documentation Files

- **README.md** — Start here
- **CLAUDE.md** — Claude Code guidance
- **docs/INDEX.md** — Docs navigation
- **docs/GETTING_STARTED.md** — Quick start
- **docs/PROJECT_EVOLUTION.md** — Project history
- **docs/KNOWN_ISSUES.md** — Known problems
- **docs/components/** — Component guides

## Need Help?

1. Check `docs/INDEX.md` for docs navigation
2. Search `docs/KNOWN_ISSUES.md` for known problems
3. Read `CLAUDE.md` for development guidance
4. Check `docs/components/` for specific topics
5. Open a GitHub Issue if stuck

---

**Pro Tip**: Bookmark `docs/INDEX.md` — it's the gateway to everything.
