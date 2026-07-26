# Session Handoff — Bare Metal & Redraw Work

Context transfer for continuing this work in a new session.
**Last updated:** 2026-07-26. All work is committed and pushed to `main`
(`origin/main` = `327f01b`). Working tree clean.

---

## 1. Situation

This project was featured by [Action Retro](https://www.youtube.com/watch?v=rJRlHKQqX2M)
as "the world's most cursed operating system" / "the AI sloperating system". He
booted it on real hardware (Pentium 3, ThinkPad X1 Carbon, 11" Intel MacBook
Air). It booted on all three and then froze on all three, with a "goofy grub
issue" on every machine.

This session chased those symptoms. **Every one of them turned out to be a real
bug**, and most were more serious than the visible symptom suggested.

The user (project owner, `Kelsidavis/System7`) is testing on a **physical UEFI
ThinkPad** and reporting back. That feedback loop is the highest-value input —
prefer it over QEMU-only reasoning.

---

## 2. What was fixed (all verified, all pushed)

### Boot / platform

| Fix | Detail |
|---|---|
| **No GDT was ever installed** | Multiboot2 leaves GDTR undefined; the kernel borrowed GRUB's temporary GDT and later allocated over it. The first interrupt resolved a dead selector → #GP → #DF → silent triple-fault reset. New `src/Platform/x86/gdt.c`. **This was the main real-hardware freeze.** |
| **BIOS-only ISO** | `grub-mkrescue -d i386-pc` emitted one El Torito entry, no EFI payload — could not boot any UEFI-only machine. Now hybrid; `make verify-iso` fails the build if either boot image is missing. |
| **GRUB `timeout=-1`** | Waited forever for a keypress → headless/serial-only machines never booted. Now `timeout=5` + serial console mirroring at 38400 baud. |
| **Unbounded UART spin** | `serial_putchar` spun forever on THR-empty, deadlocking boot on machines whose port never reports ready. Now bounded. |
| **PIC inherited firmware mask** | `pic_init` saved/restored the BIOS mask. IRQ0 was never unmasked (timer dead) and IRQ2 cascade was never unmasked (**IRQ12 mouse could never work**). Now masks all, unmasks explicitly. |
| **Exceptions triple-faulted silently** | Vectors 0–31 pointed at bare `iret`, wrong for error-code-pushing faults. Now per-vector stubs + `exception_dispatch()` printing fault name, `eip`, error code, `cr2`. |
| **RTC permanently reconfigured CMOS** | `rtc_init()` forced binary mode into battery-backed CMOS. Most BIOSes expect BCD → CMOS error → **clock/settings reset on the user's own machine**. Now strictly read-only; also fixed 12-hour mode and torn reads. **User confirmed working on hardware.** |

### Time

| Fix | Detail |
|---|---|
| **`udiv64` was broken** | Never shifted the divisor left to align. `100/3` → 3; `1000000/16667` → 32767. `TickCount()` *is* `udiv64(µs, 16667)`, so the system clock **saturated instead of counting**. Copy-pasted into 4 files; all fixed. |
| **TSC calibration circular** | CPUID 0x15/0x16 are Skylake-era; older CPUs fell to a fallback that calibrated against `TickCount()`, which needs the time base being calibrated. Always timed out → fabricated **1 MHz** vs real GHz. Now calibrates from PIT channel 2. QEMU reports 3899 MHz. |
| **Modal loops counted iterations, not time** | `DragWindow`'s "~1 second at 60Hz" was 60 *iterations* on an unpaced loop running ~10⁵/sec — expired in <1 ms. **Fatal for touchpads specifically** (no reports while a finger is still). `TrackMenu`'s 1,000,000-iteration cap expired in ~2 s of wall time. All now tick-based. |

### Input / UI

| Fix | Detail |
|---|---|
| **Menu button read was type-punned** | `extern volatile uint8_t g_mouseState` — but on x86 that symbol is a **struct** whose first member is `int16_t x` (cursor position). Invisible to compiler and linker. "Is the button down" was really "is the cursor on an odd X pixel". Correct on arm64, where it *is* a `uint8_t`. Now uses `GetMouseButtons()`. |
| **Menu tap opened and closed in one gesture** | The opening click's own release armed selection. Now requires steady-up ~33 ms **and** menu open ~200 ms. |
| **Title bar erased** | `Finder_GetWindowBounds` preferred `visRgn` (content, excludes title bar) over `strucRgn`, so the desktop erase painted over the chrome. |
| **Icon labels clipped** (`ystem Folder`) | `IconLabel_Draw` centred captions with no bound; first column centre x≈36 vs ~90px caption → negative x. Now clamped to the port. |
| **Resize corrupted the heap** | The offscreen GWorld was **never resized** — `NewGWorld` only at creation, `DisposeGWorld` only at close. Growing 477×317→534×382 overran by ~211 KB, poisoning the `WindowRecord` (`portRect` read `0xCDCD`/`0xABAB`). Explained icons-outside-window, missing labels, blank content, freeze. |
| **Update events flooded the queue** | `CheckWindowsNeedingUpdate` posted an updateEvt per dirty window on *every* `GetNextEvent`; no dedup, 32-slot queue → full in µs → **`PostEvent` then rejected mouse and keyboard events too**. Now synthesised on demand in `GetNextEvent`. |
| **PaintBehind aliased `clipRgn = visRgn`** | Two owners for one region. The identical bug had already been found and fixed elsewhere in the same file; this copy was missed. |

---

## 3. Open issues (see `docs/KNOWN_ISSUES.md` for full detail)

### REGION-001 — regions are rectangles
`struct Region` holds only `rgnSize` + `rgnBBox`. `DiffRgn()` ends in
`CopyRgn(srcRgnA, dstRgn)` — **subtracts nothing**. `XorRgn()` returns the union
bbox. Representational, not a missing few lines.

⚠️ **Do NOT build on `DiffRgn`.** I tried; it erases the entire desktop including
every window. Caught on a screenshot before it reached hardware.

Consequences: `DragWindow`'s uncovered region is the whole old rect; the desktop
erase can only hold back one window.

### ARCH-001 — competing redraw paths (mostly fixed)
Root cause fixed (the PostEvent flood). Three direct redraws removed; the dead
`DrawFolderWindowContents` placeholder deleted.

**Still present:** `WindowEvents.c`, `finder_main.c` ×2, `folder_window.c`.
These are the genuine update handlers and Finder paths — they need individual
review, **not** blind deletion.

### ~~Unexplained layout discrepancy~~ — RESOLVED
The status line rendering at y≈304–312 instead of y=414 was a **double
coordinate conversion in the glyph rasteriser**, not a Finder or redraw problem:
`DrawChar` mapped the pen local→global with `QD_LocalToPixel`, then
`FM_DrawChicagoCharInternal` subtracted `portBits.bounds` a second time. Only
text was affected, which is why the separator line drawn from the same pen one
row above landed correctly.

The correct origin depends on **where `baseAddr` points**, not on bounds alone —
the tree has three port configurations and the old code was right for two of
them. See REDRAW-004's predecessor entry in `docs/KNOWN_ISSUES.md` for the table.

Note this did **not** explain the *"blank Macintosh HD window"* report — that is
now filed separately as **REDRAW-004**, with a reproduction.

### ~~REDRAW-004 — blank window content at boot~~ — FIXED
This was the user's *"blank Macintosh HD window"*. Two bugs: `PaintOne` erased
content without ever adding it to `updateRgn`, so whether content survived
depended purely on whether the erase ran before or after the app's draw; and the
update synthesis added in 293388f went into `event_manager.c`'s `GetNextEvent`,
**which is not the one that links** — `ENABLE_PROCESS_COOP` routes it to
`Proc_GetNextEvent` in `ProcessMgr/EventIntegration.c`, which had no synthesis at
all. Also had to defer repaints of covered windows (REGION-001 fallout). Full
detail in `docs/KNOWN_ISSUES.md`.

⚠️ **Two `GetNextEvent` definitions exist and the non-obvious one wins.** Check
`nm build/obj/**/*.o | grep " T GetNextEvent"` before editing the event path.
Same trap as `DrawText` (`QuickDraw/Text.c` is dead; `FontManagerCore.c` links).
Assume nothing in this tree is the only copy — verify against the linked symbol.

### Not started
- **68K interpreter is not wired up** — real Mac apps still don't run. Exists in
  `include/CPU/M68KInterp.h` + opcode tables; needs an execution entry point,
  segment-loader integration, and a trap dispatcher (~200 traps). Est. 3–4 weeks.
- Bare-metal storage (ATA), USB, networking — all stubs.

---

## 4. How to work on this

### Build & verify
```bash
make clean && make && make iso     # verify-iso fails if not hybrid BIOS+UEFI
```

### Look at the screen (essential — do not debug redraw blind)
```bash
scripts/screenshot.sh out.png [boot_delay] [click_x click_y]
```
Boots headless, dumps the framebuffer as PNG plus its serial log. Attaches a
**USB tablet** so monitor `mouse_move` takes absolute coordinates.

### Driving synthetic input — hard-won details
- **PS/2 deltas are 9-bit signed.** Large monitor `mouse_move` jumps are clamped;
  the cursor must be walked in steps of ≤100 px. This silently defeated several
  of my early drag tests (they opened the Apple menu instead).
- A **USB tablet** gives absolute positioning, but clicks did not register
  through that path in my harness — PS/2 clicks did. Unresolved, and since
  confirmed again: a tablet `mouse_button` on the Apple menu does nothing, while
  the same click over PS/2 opens it. Drop the `-device usb-tablet` and walk the
  cursor with relative `mouse_move` deltas (≤80 px per step, tracking the
  position yourself) to drive menus and icons reliably.
- Whether the tablet is attached also changes whether the Finder window paints
  its content at all — see **REDRAW-004**. Capture both ways before concluding
  anything about a missing redraw.
- Reproduce a drag: walk to the title bar (~250,91), `mouse_button 1`, several
  small moves, `mouse_button 0`.
- Reproduce a resize: walk to the grow box (~483,413), same pattern.

### Traps that cost me time
- **`serial_printf` is filtered** through `SysLogClassifyMessage`/`SysLogEmit` —
  messages can vanish entirely. Use `serial_puts` for boot-critical or
  fault-path diagnostics.
- ~~**The in-tree `snprintf` does not support `%lx`**~~ — **fixed.** It used to
  print the literal text and silently shift every following argument. The
  formatter now handles the full set of flags, width, precision and the length
  modifiers (`hh h l ll z j t`), and returns the C99 would-have-written length.
  `make test-stdlib` compares it against the host libc.
- **Run `make test-stdlib` after touching `src/System71StdLib.c`.** It extracts
  the pure string/memory/format routines, compiles them natively and diffs them
  against the host libc with guard bytes around every destination. Two shipped
  bugs came out of that file — `strncpy` writing one byte past its destination
  for all 211 callers, and the `%lx` argument shift.
- **Never log from interrupt context** — `serial_puts` busy-waits at 38400 baud
  (~¼ ms/char); a 20-char line stalls a 1 kHz handler for milliseconds.
- Allocator poison bytes: `0xCD` = inter-size padding fill, `0xAB` = `CANARY_BYTE`.
  Seeing `-12851` / `-21589` in an `SInt16` means freed/past-the-end memory.

### Working style the user expects
- **Verify by running, not by reasoning.** I shipped one "fix" that only
  compiled — it triple-faulted instantly. Boot it.
- **State what was actually tested vs. inferred**, and correct earlier claims in
  the docs when evidence overturns them (I had to do this twice).
- The user is direct and technically sharp; they spotted the architectural
  problem (*"multiple code paths for everything is gonna make troubleshooting
  constantly difficult"*) before I raised it, and they were right — removing a
  workaround **was** the fix for REDRAW-002.

---

## 5. Suggested next steps, in order

1. **Deal with the 34 never-compiled source files (ARCH-002).** Whole subsystems
   — all of `TextEdit/`, most of `DialogManager/`, `SoundManager/`,
   `HFS_Catalog.c`, `HFS_Volume.c`, `QuickDraw/Text.c` — are never built, yet
   define functions whose live copy is elsewhere. Nothing marks them as dead and
   grep finds them first. Delete or clearly mark them; run
   `python3 scripts/find-shadowed-defs.py` for the list.
2. **Ask the user to re-flash and retest.** Several fixes are unconfirmed on
   hardware: menu timing, title bar, icon labels, resize, update events. The
   input-starvation fix in particular may change behaviour broadly.
3. **Finish ARCH-001** — review the four remaining direct-redraw sites.
4. **Consider implementing real regions** (REGION-001) — a genuine sub-project
   (new representation + all boolean ops + clipper/blitter), but it would fix
   drag repaint, multi-window desktop erase, and clipping generally.
5. **68K interpreter** — the largest remaining feature gap.
