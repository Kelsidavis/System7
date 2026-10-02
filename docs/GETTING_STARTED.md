# Getting Started with System 7

Welcome! This guide will help you get System 7 up and running in minutes.

## What is System 7?

This is an open-source reimplementation of Apple's classic Macintosh System 7 operating system. It runs on modern x86 hardware via QEMU emulation and demonstrates how the classic Mac OS worked internally.

**Status**: Proof of concept (~94% of core functionality complete)

> Note: the audit that produced the ~85% figure is from November 2025 and
> predates the 2026 work (desktop patterns, dialog/window redraw fixes, Finder
> search, CPU-named About This Macintosh). Treat its file/line references as
> hints, not gospel.

## Quick Start (5 minutes)

### 1. Install Dependencies

**Ubuntu/Debian**:
```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin grub-efi-amd64-bin mtools xorriso qemu-system-x86 python3 vim-common
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
Useful for debugging. A USB tablet lets the pointer follow the host mouse
without capturing it, and the system uses as much RAM as it is given:
```bash
qemu-system-i386 -cdrom system71.iso -m 1024 -vga std \
    -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 -serial stdio
```

### Headless (No Graphics)
```bash
qemu-system-i386 -cdrom system71.iso -m 1024 -vga std \
    -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 -serial stdio -display none
```

### With Debugger
```bash
make debug
# In another terminal:
gdb kernel.elf -ex "target remote :1234"
```

## Building Variants

```bash
make                         # the kernel, x86, English
make LOCALE_FR=1             # with French as well
make LOCALE_ALL=1            # with all 38 languages; choose with lang=xx at boot
make PLATFORM=arm64          # ARM64 kernel for QEMU's virt machine
make INTEGRATION_TESTS=1     # with the integration tests, which run at boot
                             # and report PASS/FAIL on the serial port
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
- 68K applications (test programs run; real applications untested)
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
│   ├── KNOWN_ISSUES.md           # Current limitations
│   ├── components/               # Deep technical guides
│   └── future/                   # Planned improvements
├── include/                       # Public headers (subsystems)
├── resources/
│   ├── strings/                  # STR# tables, one per language
│   └── device-tree/              # QEMU configuration
├── scripts/                       # Utility scripts
├── Makefile                       # Build system
└── gen_rsrc.py                   # Resource generator
```

## Next Steps

- **Want to explore the code?** Start with the [component guides](components/README.md)
- **Hit an issue?** Check [KNOWN_ISSUES.md](KNOWN_ISSUES.md)
- **Want to contribute?** See [CONTRIBUTING.md](CONTRIBUTING.md)
- **Curious about architecture?** Browse [docs/components/](components/)

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
