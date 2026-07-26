# Using Claude Code with System 7

This file contains guidance for using Claude Code (Claude in the IDE, Claude Code CLI, or claude.ai/code) with the System 7 project.

## Quick Start

### Build & Run
```bash
make              # Build kernel (x86, English)
make run          # Build and run in QEMU
make clean        # Clean build artifacts
```

### Add Languages
```bash
make LOCALE_FR=1  # Add French to build
make LOCALE_DE=1 LOCALE_JA=1  # Multiple languages
```

### Debug
```bash
make debug        # Build with GDB support and start QEMU
# In another terminal: gdb kernel.elf -ex "target remote :1234"
```

## Project Structure

```
System7/
├── include/          # Public headers (organized by subsystem)
├── src/              # Implementation files (mirrors include structure)
├── docs/             # Documentation (start with docs/INDEX.md)
├── scripts/          # Development utilities
├── resources/
│   ├── strings/      # Localization (one .json per language)
│   └── device-tree/  # QEMU device tree files
├── Makefile          # Build system
└── README.md         # Main documentation
```

## Common Tasks with Claude

### Understanding a Component
1. Read the component guide in `docs/components/`
2. Look at the header in `include/ComponentName/`
3. Study the implementation in `src/ComponentName/`
4. Ask Claude: "Explain how X works in this codebase"

### Finding What's Broken
1. Check `docs/KNOWN_ISSUES.md`
2. Search for `TODO` or `FIXME` comments
3. Run `grep -r "HACK" include/ src/`
4. Look at `docs/IMPLEMENTATION_STATUS_AUDIT.md`

### Adding a Feature
1. Decide which component it belongs to
2. Check if it's already partially implemented
3. Look for TODOs in that component
4. Ask Claude: "What would need to change to add X?"

### Fixing a Bug
1. Reproduce in QEMU: `make run`
2. Check serial output: look for error messages
3. Ask Claude to help trace the issue
4. Make minimal fix, test in QEMU first
5. Be skeptical of "works in QEMU = works everywhere"

### Testing Your Changes
```bash
make clean && make run    # Quick rebuild + test
make debug                # Run with GDB for breakpoints
```

## Important Context

### This is Experimental
- **QEMU-only testing**: Most features only tested in emulation
- **Bare metal unknown**: Real hardware behavior is unpredictable
- **Sloppy code**: Expect `HACK:` comments and incomplete implementations
- **Edge cases crash**: Don't trust error handling

### Don't Assume
- ❌ "Works in QEMU" ≠ "Works on real hardware"
- ❌ "Compiles cleanly" ≠ "Actually works correctly"
- ❌ "No malloc violations" ≠ "Memory-safe"
- ❌ "Boots successfully" ≠ "All features work"

### What's Actually Solid
- ✅ Boot sequence (works in QEMU)
- ✅ Window/Menu rendering (works in QEMU)
- ✅ SimpleText editor (works in QEMU)
- ✅ File browsing (works in QEMU)
- ⚠️ Everything else: untested on real hardware

## Code Style Guidelines

### Comments
- Explain **WHY**, not WHAT
- If you see a "HACK:" prefix, investigate carefully
- "TODO:" comments indicate incomplete implementations

### Naming
- Function names should be descriptive
- Variable names should indicate purpose
- Avoid abbreviations in public APIs

### Structures
- Use meaningful field names
- Include offsets in comments where they matter
- Reference evidence (addresses, Inside Macintosh sections)

### Memory
- **Never** use malloc/free in kernel code
- Use zone-based allocation instead
- If you see malloc(), it's a bug

### Testing
- Test in QEMU first
- Don't assume bare metal will work the same
- Document what you tested and what you didn't

## Useful Files

| File | Purpose |
|------|---------|
| `docs/INDEX.md` | Documentation guide (start here) |
| `docs/PROJECT_EVOLUTION.md` | Why this is sloppy and how it got that way |
| `docs/IMPLEMENTATION_STATUS_AUDIT.md` | What's implemented, what's not |
| `docs/KNOWN_ISSUES.md` | Known bugs and limitations |
| `docs/components/README.md` | Component documentation index |
| `IMPLEMENTATION_PRIORITIES.md` | Roadmap and planned work |
| `.github/workflows/ci.yml` | CI/CD configuration and test setup |
| `Makefile` | Build system and compilation flags |

## Getting Help

### What to Ask Claude
✅ **Good questions:**
- "Explain how the Window Manager works"
- "What would need to change to add X feature?"
- "Why might this crash on bare metal?"
- "Where should this code go?"
- "Is this similar to any existing pattern?"

❌ **Avoid asking:**
- "Will this work?" (We don't know on bare metal)
- "Is this production-ready?" (No)
- "Can this run real Mac software?" (Probably not)
- "Why is the code so messy?" (Read PROJECT_EVOLUTION.md)

### What to Check First
1. `docs/INDEX.md` — Navigation guide
2. `docs/PROJECT_EVOLUTION.md` — Context and history
3. `docs/KNOWN_ISSUES.md` — Known problems
4. `docs/components/` — Component-specific guides
5. GitHub Issues — Others' questions and findings

## Development Workflow

### Recommended Approach
1. Understand what you're changing (read the component guide)
2. Make a minimal change
3. Test in QEMU: `make run`
4. Check for new issues
5. Document what you tested (QEMU/bare metal)
6. Submit PR with honest assessment of testing

### Red Flags
- 🚩 Code that compiles but doesn't have obvious effect
- 🚩 Features that "probably work" but are untested
- 🚩 Changes that only work because QEMU abstracts something
- 🚩 Not testing on bare metal (if possible)

## When Something Breaks

### Debugging in QEMU
1. Run with `make debug`
2. In another terminal: `gdb kernel.elf -ex "target remote :1234"`
3. Common breakpoints: `break InitMenus`, `break DrawMenuBar`
4. Watch serial output: QEMU logs go to stderr

### Serial Logging
- All printf/serial_printf output appears in QEMU serial
- `-serial stdio` shows output in your terminal
- `-serial file:/tmp/log.txt` saves to a file

### Testing on Bare Metal
- This project is mostly untested on real hardware
- If you have vintage Mac hardware, report your findings!
- Document exact machine model and what works/breaks

## Environment Setup

### Ubuntu/Debian
```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### macOS
```bash
brew install i386-elf-toolchain qemu xorriso
```

### Other Distros
- Install equivalent packages for: GCC (32-bit), Make, GRUB tools, QEMU, xorriso, Python3, xxd

## Tips & Tricks

### Faster Testing
```bash
make clean && make run  # Quick compile + QEMU boot
```

### Multiple Languages
```bash
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_JA=1 LOCALE_ZH=1 run
```

### Check Build Issues
```bash
make PLATFORM=x86  # Verbose output (no QEMU)
make info          # Statistics and diagnostics
```

### Generate Resources
```bash
python3 gen_rsrc.py resources/strings/en.json output.rsrc
```

## Known Limitations

- **Bare metal support**: Minimal, mostly untested
- **Real applications**: M68K interpreter exists but untested
- **Stability**: Crashes are normal
- **Edge cases**: Most subsystems don't handle them
- **Performance**: Optimized for QEMU, not real hardware
- **Printing**: Not implemented
- **Networking**: Not implemented

## Questions?

If you can't find answers:
1. Check [docs/INDEX.md](docs/INDEX.md)
2. Look at [docs/PROJECT_EVOLUTION.md](docs/PROJECT_EVOLUTION.md)
3. Search existing GitHub Issues
4. Ask in a new GitHub Issue with context
5. Reference IMPLEMENTATION_PRIORITIES.md for planned work

---

**Remember**: This is an experimental OS. Be skeptical of anything that only works in QEMU. Test thoroughly. Document your findings.
