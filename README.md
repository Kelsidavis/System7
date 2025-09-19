<img width="646" height="405" alt="text" src="https://github.com/user-attachments/assets/f7fd55e1-73b5-4032-a3ae-910b86054994" />


![System7 prototype boot (QEMU)](https://github.com/user-attachments/assets/f7fd55e1-73b5-4032-a3ae-910b86054994)

# System7

**System7** is an educational, clean-room style re-implementation of Apple’s **System 7.1** operating environment for modern platforms.

## Purpose

* Explore and document the internals of a historically significant operating system.
* Provide a modern, open-source re-creation of the System 7.1 experience.
* Support research, teaching, and digital preservation.

## Methodology

Development was guided by:

* **Reverse engineering** of publicly available System 7.1 binaries.
* Historical documentation, manuals, and community research.
* Community-circulated disassemblies/decompilations used **only as orientation aids** to understand structure and flow.

All code in this repository is an **original re-implementation**. No Apple source code or other copyrighted materials have been copied.

## Technical Overview

Implemented/under way:

* **Bootloader & System Init** — low-level startup, memory map, basic device bring-up.
* **Memory Manager (early)** — heap/handle primitives; block movement stubs.
* **File System Layer (stubs)** — early HFS abstractions and path handling.
* **Graphics (proto)** — early QuickDraw-style primitives and screen buffer handling.
* **Event Manager (skeleton)** — structure for dispatching input and system events.
* **UI Shell (very early)** — placeholders for menu/desktop constructs.

## Current Status

* ✅ **Boots in QEMU** to the early system shell with framebuffer output (development image).
* 🛠️ Minimal demo path for verifying boot, drawing, and basic system loops.

## Roadmap / Next Steps

**Short-term targets**

1. **SVGA Video (QEMU stdvga / VBE 2.0 LFB)**

   * Implement VBE info & mode set (0x4F00/0x4F01/0x4F02).
   * Prefer linear framebuffer modes (e.g., 800×600 or 1024×768, 32-bpp).
   * Add pitch/stride-aware blitting; double-buffered present to reduce tearing.
   * Hook drawing pipeline to QuickDraw-like primitives.

2. **Keyboard Input (i8042 / PS/2)**

   * Handle scan set 1 (QEMU default) via port 0x60/0x64.
   * IRQ1 dispatch → translate make/break codes → enqueue to Event Manager.
   * Key repeat and modifier state bookkeeping.

3. **Mouse Input (PS/2 / IntelliMouse)**

   * Enable streaming, parse 3- or 4-byte packets (IMPS/2 wheel optional).
   * IRQ12 dispatch → delta aggregation, cursor update, click state → events.
   * Basic acceleration and bounds clamping.

**Milestones that follow**

* **Window Manager pass 1** (move/resize, invalidation, region ops).
* **Menu Manager pass 1** (menu bar, tracking, command dispatch).
* **Disk I/O layer** (block driver abstraction; boot image tooling).
* **Timer/WaitNextEvent** integration for cooperative scheduling.

## Building & Running

```bash
# Example (adjust paths/toolchain as needed)
make

# Run with QEMU (stdvga for upcoming VBE work)
qemu-system-i386 \
  -m 64M \
  -vga std \
  -drive file=build/system7.img,format=raw,if=ide \
  -boot c \
  -serial stdio \
  -no-reboot
```

**Dev tips**

* Start with **640×480×32** or **800×600×32** VBE modes for simpler math.
* Use `-d guest_errors` on QEMU when debugging port I/O.
* Keep a hexdump of VBE control/mode info blocks for verification.

## Legal Notes

* **Apple, Macintosh, and System 7** are trademarks of Apple Inc.
* This project is **not affiliated with or endorsed by Apple**.
* All rights to the original System 7 software remain with Apple.
* Provided **solely for educational and research purposes**.

## Contributing

Issues, PRs, and historical references are welcome. Please keep contributions clean-room and avoid including third-party copyrighted code.

---

## License

```
MIT License

Copyright (c) 2025 Kelsi Davis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

