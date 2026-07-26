# Getting Started with System 7

Welcome! This guide will help you get System 7 up and running in minutes.

## What is System 7?

This is an open-source reimplementation of Apple's classic Macintosh System 7 operating system. It runs on modern x86 hardware via QEMU emulation and demonstrates how the classic Mac OS worked internally.

**Status**: Proof of concept (~94% of core functionality complete)

## Quick Start (5 minutes)

### 1. Install Dependencies

**Ubuntu/Debian**:
```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

**macOS**:
```bash
# Requires Homebrew: https://brew.sh
brew install i386-elf-toolchain qemu xorriso
```

### 2. Clone & Build

```bash
git clone https://github.com/Mikecraft1224/System7.git
cd System7
make run
```

This builds the kernel and starts it in QEMU. You should see the System 7 desktop appear.

## Running System 7

### Standard Run
```bash
make run
```

### With Serial Output
Useful for debugging:
```bash
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M
```

### Headless (No Graphics)
```bash
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M
```

### With Debugger
```bash
make debug
# In another terminal:
gdb kernel.elf -ex "target remote :1234"
```

## Building Variants

### English Only (Default)
```bash
make
```

### With Additional Language
```bash
make LOCALE_FR=1     # Add French
make LOCALE_DE=1     # Add German
make LOCALE_JA=1     # Add Japanese
make LOCALE_ZH=1     # Add Simplified Chinese
```

### All Languages
```bash
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 ... # (see README.md)
```

## What Works

✅ **Desktop & GUI**
- System 7 menu bar with Apple logo
- Icon dragging
- Window management
- Desktop patterns

✅ **Applications**
- SimpleText editor (working MDI text editor with save/load)

✅ **System Features**
- Localization (37 languages)
- PS/2 keyboard and mouse
- File browser (HFS virtual filesystem)
- Sound Manager with MIDI
- Font rendering (Chicago bitmap font)

⚠️ **Partially Working**
- M68K application execution (loader ready, execution needs work)
- Window/Control/Dialog frameworks
- Some System tools

❌ **Not Implemented**
- Printing
- Networking
- Real hard drive access
- TrueType fonts

## Exploring the System

### Open Files
Click the file manager icon or use File > Open to browse the virtual filesystem.

### Change Language
Restart System 7 and select a different language at boot (if compiled with locale support).

### Use SimpleText
Click the SimpleText icon to open the included text editor.

### Check Serial Output
Debug information appears in the QEMU serial console or log file.

## Directory Map

```
System7/
├── README.md                      # Main documentation
├── docs/
│   ├── GETTING_STARTED.md        # This file
│   ├── CONTRIBUTING.md           # How to contribute
│   ├── TRANSLATIONS.md           # Multi-language READMEs
│   ├── KNOWN_ISSUES.md           # Current limitations
│   ├── components/               # Deep technical guides
│   └── future/                   # Planned improvements
├── include/                       # Public headers (subsystems)
├── resources/
│   ├── strings/                  # Localization (one per language)
│   └── device-tree/              # QEMU configuration
├── scripts/                       # Utility scripts
├── Makefile                       # Build system
└── gen_rsrc.py                   # Resource generator
```

## Next Steps

- **Want to explore the code?** See [IMPLEMENTATION_STATUS_AUDIT.md](IMPLEMENTATION_STATUS_AUDIT.md)
- **Hit an issue?** Check [KNOWN_ISSUES.md](KNOWN_ISSUES.md)
- **Want to contribute?** See [CONTRIBUTING.md](CONTRIBUTING.md)
- **Curious about architecture?** Browse [docs/components/](components/)
- **Looking for translations?** See [TRANSLATIONS.md](TRANSLATIONS.md)

## Troubleshooting

### Build fails
- Ensure you have all dependencies: `gcc-multilib`, `grub-pc-bin`, `xorriso`
- Try `make clean` then `make`

### QEMU won't start
- Verify `qemu-system-i386` is installed
- Try `qemu-system-i386 --version` to check

### Can't see graphics
- Ensure SDL support: `qemu-system-i386 -display help`
- Try headless mode: `make run` should work with default VGA

### Serial output is empty
- Serial logging uses printf - compile with `PLATFORM=x86`
- Check kernel.log or specify: `-serial file:/tmp/serial.log`

## Questions?

- **GitHub Issues**: Report bugs and ask questions
- **Documentation**: Check `docs/` folder
- **Code**: Comments include Finding IDs referencing Inside Macintosh

---

**Enjoy exploring classic Mac OS!** 🖥️✨
