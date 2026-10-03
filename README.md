# System 7 - Portable Open-Source Reimplementation

<img width="793" height="657" alt="System 7 running on modern hardware" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **PROOF OF CONCEPT** - This is an experimental, educational reimplementation of Apple's Macintosh System 7. This is NOT a finished product and should not be considered production-ready software.

## 🎥 As Seen On Action Retro

[![Watch on YouTube](https://img.shields.io/badge/Watch-Action%20Retro-red?style=for-the-badge&logo=youtube)](https://www.youtube.com/watch?v=rJRlHKQqX2M)

> *"I've finally done it. I have discovered the world's most cursed operating system. It's more cursed than ReactOS. It's more cursed than **Hannah Montana Linux**."*

> *"This is literally the AI **sloperating system**."*

> *"No freaking way. This abomination is booting."*

> *"It works at all is just absolutely insane."*
>
> — [Action Retro](https://www.youtube.com/watch?v=rJRlHKQqX2M), installing it on a Pentium 3, a ThinkPad X1 Carbon, and an 11" Intel MacBook Air

---

An open-source reimplementation of Apple Macintosh System 7 for modern x86 hardware, bootable via GRUB2/Multiboot2. This project aims to recreate the classic Mac OS experience while documenting the System 7 architecture through reverse engineering analysis.

## 📖 Quick Links

**New here?** Start with [Getting Started](docs/GETTING_STARTED.md) | **All docs?** See [Documentation Index](docs/INDEX.md) | **Context?** Read [Project Evolution](docs/PROJECT_EVOLUTION.md) | **Contribute?** See [Contributing](docs/CONTRIBUTING.md)

**Using Claude Code?** See [CLAUDE.md](CLAUDE.md) for project-specific guidance

## 💡 About This Project

This started as a disciplined AI-assisted reverse engineering research project (published to Zenodo in 2025) that proved you could reconstruct a bootable System 7 prototype in days. We then asked: **"What if we just kept building?"**

**What happened**: We kept building. Faster. With less testing. Mostly in QEMU. Almost no bare metal validation — which caught up with us the moment someone put it on real hardware and it froze on every machine. Features exist everywhere, but edge cases crash constantly.

**Honest assessment**: This is a **sloperating system**™. It now boots to a responsive desktop on real hardware, which it did not do a week ago, but it is still far more useful for learning *about* System 7 than for *running* it. The code is readable and teaches you things; most subsystems are partially done, and hardware coverage is one confirmed machine deep.

**Why it matters anyway**: It's still the most complete open-source System 7 implementation. Real code. Real architecture. Real bugs that teach you something.

**Read [Project Evolution](docs/PROJECT_EVOLUTION.md)** for the detailed honest story about how this went from rigorous research to the sloppy experiment you're looking at.

### 🔧 What Action Retro Found — And What We Fixed

He booted it on a Pentium 3, a ThinkPad X1 Carbon, and an 11" Intel MacBook Air.
It booted on all three. It then froze on all three, the mouse did nothing, and
GRUB was "goofy" on every single machine. He was right on every count, and
chasing those symptoms turned up five genuine bare-metal bugs:

| What he saw | What was actually wrong | Status |
|---|---|---|
| "We've got the same goofy grub issue" | `set timeout=-1` — GRUB waited **forever** for a keypress, so headless/serial-only machines never booted the kernel at all | ✅ Fixed |
| Froze right after the desktop appeared | **No GDT was ever installed.** Multiboot2 leaves GDTR undefined; the kernel borrowed GRUB's temporary GDT and later allocated over it. The first interrupt then resolved a dead selector → #GP → #DF → silent triple-fault reset | ✅ Fixed |
| Froze with no explanation | CPU exception vectors all pointed at a bare `iret`, so any fault reset the machine with **zero diagnostics**. Now prints the fault name, `eip`, error code and `cr2` | ✅ Fixed |
| "Mouse does nothing" | IRQ2 (the slave-PIC cascade) was never unmasked — so IRQ12 could **never** reach the CPU no matter what. IRQ0 was never unmasked either, so the timer never ticked | ✅ Fixed |
| Hung before printing anything | `serial_putchar` spun **forever** waiting on the UART, deadlocking the boot on machines whose port never reports ready | ✅ Fixed |
| Wouldn't start on a modern ThinkPad at all | The ISO was built **BIOS-only** (`grub-mkrescue -d i386-pc`) — one El Torito entry, no EFI payload. On UEFI-only machines there was nothing for the firmware to execute | ✅ Fixed |

### ✅ It now boots to a responsive desktop on real hardware

Confirmed on a physical ThinkPad booting via UEFI — **not** an emulator. Every
machine in the video froze; this one doesn't. The missing GDT was the real
culprit behind the freezes, and the BIOS-only ISO was why newer machines
wouldn't start at all.

Also verified headless in QEMU on both firmware paths — 5,000 timer ticks at
1 kHz, interrupts dispatching, zero exceptions, no reset:

| Firmware | Result |
|---|---|
| BIOS (SeaBIOS, `qemu-system-i386`) | ✅ reaches event loop, interrupts live |
| UEFI (OVMF, `qemu-system-x86_64`) | ✅ reaches event loop, interrupts live |

**Now true:** 68K applications launch from the Finder and run under the
interpreter — see [Running 68K applications](#running-68k-applications).
Only small test programs have been run so far; real Apple applications are
untested. Broader hardware coverage is thin — one confirmed machine is
not a compatibility matrix. If you have a vintage or modern box to try it on,
we would love your test results — please [open an issue](https://github.com/Kelsidavis/System7/issues).

> **Secure Boot must be off.** The GRUB image is unsigned, so a machine with
> Secure Boot enabled will refuse the stick before GRUB ever appears.

Full roadmap: [BARE_METAL_IMPROVEMENTS.md](docs/BARE_METAL_IMPROVEMENTS.md)

## 🎯 Project Status

**Current State**: Active experimental development. Most features are exercised in QEMU; bare-metal testing is limited to a small number of machines, and compatibility with real System 7 applications remains unverified.

### Latest Updates (October 2026)

#### Desktop, Desk Accessories, and the Toolbox Underneath ✅
- **Desktop Patterns** is laid out as Apple's System 7.5 control panel: one
  pattern tiled in a preview, a scroll bar to step through the collection, and
  a **Set Desktop Pattern** button. Colour patterns (`ppat`) come first,
  including Apple's own 400 and 401, decoded by the Inside Macintosh `ppat`
  layout, then the 32 black-and-white ones
- **Desk accessories work**: clicks and keys reach them, each draws in its own
  window, and closing one (close box or Close Window) closes it properly.
  Calculator gets its arithmetic right, Alarm Clock shows the live time, and
  Key Caps draws a keyboard and shows what you type
- **Memory**: on x86 the application heap takes the machine's free RAM instead
  of a fixed array — about 1 GB under QEMU with `-m 1024` — and About This
  Macintosh names the processor (CPUID)
- **QuickDraw**: `OpenPoly` returns the polygon, so polygon recording no longer
  sticks on and stops every later line from drawing
- **Windows**: screen size comes from the screen (no 640×480 assumption);
  moving, shrinking and zooming repaint what they uncover; updates copy only
  what was redrawn; hidden windows draw nothing
- **Events**: the USB tablet works, its movement is no longer read as wheel
  turns, and a full event queue drops its oldest event rather than new clicks;
  modal dialogs keep the pointer and pass other windows' updates on
- **Finder**: Find asks for a name and reveals each match, Find Again moves to
  the next; menus, the menu-bar clock, alerts and Balloon Help's menu draw
  correctly
- **Resource Manager**: `ReleaseResource`, `DetachResource` and
  `GetIndResource` keep the resource map and cache consistent

#### Previous Session Accomplishments
- ✅ **Bare Metal Fixes**: GDT installation, hybrid BIOS+UEFI ISO, PIC mask
  handling, bounded UART spin, read-only CMOS — all verified on a physical UEFI ThinkPad
- ✅ **Sound Manager**: shared `SndMidiNoteToFreq()` with 37-entry lookup table
  (C3–B5 plus octave fallback for MIDI 0–127), async playback callbacks
  (`FilePlayCompletionUPP`, `SndCallBackProcPtr`), and 4-level priority channel
  routing with per-channel mute/enable (`SndGetActiveChannel()` picks the
  highest-priority active channel)
- ✅ **Window Resize System**: interactive resizing with proper chrome handling, grow box, and desktop cleanup
- ✅ **PS/2 Keyboard Translation**: Full set 1 scancode to Toolbox key code mapping
- ✅ **Platform HAL**: x86 is the validated target; ARM and ARM64 build targets
  exist, while PowerPC remains an experimental scaffold

## Implementation Status

### What Works Fully ✅

- **Hardware Abstraction Layer (HAL)**: x86 implementation is the primary
  validated target
- **Boot System**: Successfully boots via GRUB2/Multiboot2 on x86
- **Serial Logging**: Module-based logging with runtime filtering (Error/Warn/Info/Debug/Trace)
- **Graphics Foundation**: VESA framebuffer (800x600x32, 32-bit colour) with QuickDraw primitives including XOR mode
- **Desktop Rendering**: System 7 menu bar with rainbow Apple logo, icons, and desktop patterns
- **Typography**: Chicago bitmap font with pixel-perfect rendering and proper kerning, extended Mac Roman (0x80-0xFF) for European accented characters
- **Localization**: user-visible strings come from `STR#` resources through the Locale Manager, in 38 languages; build English only, one language, or all of them (`LOCALE_ALL=1`) and pick one at boot with `lang=xx`; CJK multi-byte encoding infrastructure
- **Font Manager**: Font scaling and style synthesis; FOND/NFNT loading is connected through `GetResource`, but validation against real application resource forks remains limited
- **Input System**: PS/2 keyboard and mouse, and USB keyboards, mice and tablets (xHCI)
- **Event Manager**: Cooperative multitasking via WaitNextEvent with unified event queue
- **Memory Manager**: Zone-based allocation with 68K interpreter integration; on x86 the application zone takes the machine's free RAM
- **Menu Manager**: Complete dropdown menus with mouse tracking and SaveBits/RestoreBits
- **File System**: HFS with B-tree implementation, folder windows with VFS enumeration
- **Window Manager**: Dragging, resizing (with grow box), layering, activation
- **Time Manager**: Accurate TSC calibration, microsecond precision, generation checking
- **Resource Manager**: Resource-map lookup, handle-based loading, and resource-fork updates
- **Gestalt Manager**: Multi-architecture system information with architecture detection
- **TextEdit Manager**: Complete text editing with clipboard integration
- **Scrap Manager**: Classic Mac OS clipboard with multiple flavor support
- **SimpleText Application**: Full-featured MDI text editor with cut/copy/paste
- **List Manager**: System 7-compatible list controls with keyboard navigation
- **Control Manager**: Standard and scrollbar controls with CDEF implementation
- **Dialog Manager**: Keyboard navigation, focus rings, keyboard shortcuts
- **Segment Loader**: Portable ISA-agnostic 68K segment loading system with relocation
- **M68K Interpreter**: Runs 68000 applications from an HFS disk: resource fork, A5 world, jump table and `_LoadSeg`, with Toolbox traps for memory, resources, QuickDraw, windows, menus, events, dialogs, controls, TextEdit, files and Standard File bridged to the native managers
- **Sound Manager**: Command processing, MIDI conversion, channel management, callbacks
- **Device Manager**: DCE management, driver installation/removal, and I/O operations
- **Startup Screen**: Complete boot UI with progress tracking, phase management, and splash screen
- **Color Manager**: Color state management with QuickDraw integration
- **Desk Accessories**: Calculator, Alarm Clock, Key Caps, Note Pad and Chooser from the Apple menu, driven through `SystemClick`/`SystemEvent`
- **Control Panels**: Desktop Patterns (with colour patterns), Date & Time, Sound, Mouse, Keyboard, Control Strip

### Partially Implemented ⚠️

- **68K Applications**: The test programs in `tests/m68k` run fully; traps an application uses that are not bridged yet stop it with the trap's name
- **Window Definition Procedures (WDEF)**: Core structure in place, partial dispatch
- **Speech Manager**: API framework and audio passthrough only; speech synthesis engine not implemented
- **Exception Handling (RTE)**: Return from exception partially implemented (currently halts instead of restoring context)
- **Non-x86 platforms**: ARM/ARM64 build in CI, but ARM64 runtime startup is
  incomplete; PowerPC remains experimental (see [Platform Porting Status](docs/future/PORTING_PLAN.md))

### Not Yet Implemented ❌

- **Printing**: No print system
- **Networking**: No AppleTalk or network functionality
- **Apple Events**: Inter-application Apple Event messaging is not implemented
- **Balloon Help**: the Help menu is there; balloons are not
- **Advanced Audio**: Sample playback, mixing (PC speaker limitation)

## 🏗️ Architecture

### Technical Specifications

- **Architecture**: x86 supported; ARM/ARM64 build targets and an experimental
  PowerPC scaffold are selected through the HAL
- **Boot Protocol**: Multiboot2 (x86), platform-specific bootloaders
- **Graphics**: VESA framebuffer, 800x600 @ 32-bit color
- **Memory Layout**: Kernel loads at 1MB physical address (x86)
- **Timing**: Architecture-agnostic with microsecond precision (RDTSC/timer registers)

### Build Information

Build source and object counts depend on the selected platform and options.
Run `make info` to see the current counts for your configuration.

## 🔨 Building

### Requirements

- **GCC** with 32-bit support (`gcc-multilib` on 64-bit)
- **GNU Make**
- **GRUB tools**: `grub-mkrescue` (from `grub2-common` or `grub-pc-bin`)
- **GRUB EFI modules** (`grub-efi-amd64-bin`) and **mtools** — required for the
  UEFI half of the ISO. Without them `grub-mkrescue` still exits 0 but silently
  emits a BIOS-only image that will not boot any modern machine; `make iso`
  now fails loudly if that happens
- **QEMU** for testing (`qemu-system-i386`)
- **Python 3** for resource processing
- **xxd** for binary conversion
- *(Optional)* **powerpc-linux-gnu** cross toolchain for PowerPC builds

### Ubuntu/Debian Installation

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin grub-efi-amd64-bin mtools xorriso qemu-system-x86 python3 vim-common
```

### Build Commands

```bash
# Build kernel (x86 by default)
make

# Build for specific platform
make PLATFORM=x86
make PLATFORM=arm        # requires ARM bare-metal GCC
make PLATFORM=arm64      # requires AArch64 bare-metal GCC
make PLATFORM=ppc        # experimental; requires PowerPC ELF toolchain

# Create bootable ISO
make iso

# Languages: English is always built in; add one, or all 38
make LOCALE_FR=1
make LOCALE_ALL=1

# Build and run in QEMU
make run

# Clean artifacts
make clean

# Display build statistics
make info
```

## 🚀 Running

### Quick Start (QEMU)

```bash
# Standard run with serial logging
make run

# Manually: 1 GB of RAM (the system uses what it is given) and a USB tablet,
# so the pointer follows the host mouse without capturing it
qemu-system-i386 -cdrom system71.iso -m 1024 -vga std \
    -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 \
    -serial file:/tmp/serial.log
```

### QEMU Options

```bash
# With console serial output
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Headless (no graphics display)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# With GDB debugging
make debug
# In another terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Documentation

### Getting Started
- **[Getting Started Guide](docs/GETTING_STARTED.md)** — Step-by-step setup and first run
- **[Known Issues](docs/KNOWN_ISSUES.md)** — Current limitations and workarounds
- **[Contributing Guide](docs/CONTRIBUTING.md)** — How to help with development

### Deep Dives
- **[Component Guides](docs/components/)** — Detailed technical documentation:
  - Control Manager, Dialog Manager, Font Manager, Event Manager
  - Menu Manager, Window Manager, Resource Manager, Serial Logging
- **[Memory Management](docs/MEMORY_MANAGEMENT.md)** — Zone-based allocation system
- **[Project Architecture](docs/)** — Full documentation index

### Localization
- **[Locale Manager](include/LocaleManager/)** — where user-visible strings come from, and `lang=` at boot
- **[String Resources](resources/strings/)** — the `STR#` tables, one file per language
- **[CJK Support](include/TextEncoding/)** — multi-byte encoding infrastructure

### Project Philosophy

**Archaeological Approach** with evidence-based implementation:
1. Backed by Inside Macintosh documentation and MPW Universal Interfaces
2. Important compatibility decisions are grounded in documentation, tests, or
   recorded investigation
3. Goal: behavioral parity with original System 7, not modernization
4. Clean-room implementation (no original Apple source code)

## Running 68K applications

`tests/m68k/build.sh` (needs `gcc-m68k-linux-gnu`, `binutils-m68k-linux-gnu`
and `hfsutils`) builds three test programs — Hello in assembly, Sampler and
Notes in C — and writes them to an HFS disk image:

```sh
sh tests/m68k/build.sh
qemu-system-i386 -cdrom system71.iso -hda tests/m68k/out/apps.img -boot d \
    -m 1024 -vga std -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0
```

The disk mounts on the desktop as "Test Apps"; double-click a program to
launch it. Notes is a small editor with a menu bar, a scrolling TextEdit
window, alerts, a Find dialog and Open/Save. Any 68000 application on an HFS
image can be launched the same way. A trap that is not implemented yet
stops the program with an alert naming it, and the serial log has the trap
and where it was called from.

## 🐛 Known Issues

1. **Icon Drag Artifacts**: Minor visual artifacts during desktop icon dragging
2. **68K Coverage**: Only the Toolbox traps the test programs use are bridged; real Apple applications are untested
3. **No TrueType Support**: Bitmap fonts only (Chicago)
4. **HFS Read-Only**: Virtual file system, no real disk write-back
5. **No Stability Guarantees**: Crashes and unexpected behavior are common

## 🤝 Contributing

This is primarily a learning/research project:

1. **Bug Reports**: File issues with detailed reproduction steps
2. **Testing**: Report results on different hardware/emulators
3. **Documentation**: Improve existing docs or add new guides

## 📖 Essential References

- **Inside Macintosh** (1992-1994): Official Apple Toolbox documentation
- **MPW Universal Interfaces 3.2**: Canonical header files and struct definitions
- **Guide to Macintosh Family Hardware**: Hardware architecture reference

### Helpful Tools

- **Mini vMac**: System 7 emulator for behavioral reference
- **ResEdit**: Resource editor for studying System 7 resources
- **Ghidra/IDA**: For ROM disassembly analysis

## ⚖️ Legal

This is a **clean-room reimplementation** for educational and preservation purposes:

- **No Apple source code** was used
- Based on public documentation and black-box analysis only
- "System 7", "Macintosh", "QuickDraw" are Apple Inc. trademarks
- Not affiliated with, endorsed by, or sponsored by Apple Inc.

**Original System 7 ROM and software remain property of Apple Inc.**

## 🙏 Acknowledgments

- **Apple Computer, Inc.** for creating the original System 7
- **Inside Macintosh authors** for comprehensive documentation
- **Classic Mac preservation community** for keeping the platform alive
- **68k.news and Macintosh Garden** for resource archives

## 🔮 Future Direction

**Planned Work**:

- Run real 68K applications and bridge the traps they need
- Add TrueType font support
- CJK bitmap font resources for Japanese, Chinese, and Korean rendering
- Implement additional controls (text fields, pop-ups, sliders)
- Disk write-back for HFS file system
- Advanced Sound Manager features (mixing, sampling)
- Balloon Help, and setting alarms in Alarm Clock

---

**Status**: Experimental - Educational - In Development

**Last Updated**: October 2026

For questions, issues, or discussion, please use GitHub Issues.
