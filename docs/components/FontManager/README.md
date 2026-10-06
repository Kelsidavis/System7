# Font Manager Implementation

## Overview

The Font Manager provides bitmap font support with the Chicago strike embedded in the tree. It owns font registration, strike lookup, metrics, measurement, and drawing helpers used by QuickDraw. Original font and API fidelity remain incomplete; the limitations below describe the current implementation.

## Core

- `FontTypes.h` – font constants and runtime structures
- `FontManager.h` – public API surface
- `FontManagerCore.c` – runtime state, strike selection, `InitFonts()` bootstrap

**Key APIs**

- `InitFonts()` wires the built-in families (Chicago, Geneva, Monaco)
- `GetFontName()`/`GetFNum()` swap between IDs and Pascal names
- `RealFont()` queries whether a requested family and size exists without synthesis; its resource-backed path still needs correction
- `TextFont()`/`TextFace()`/`TextSize()`/`TextMode()` update the current GrafPort
- `GetFontMetrics()` + `CharWidth()`/`StringWidth()` return ascent, descent, advance
- `FMSwapFont()` swaps the active strike based on style/size arguments

## Resource Loading

The resource format follows Apple's *Inside Macintosh: Text*: [font family resources](https://dev.os9.ca/techpubs/mac/Text/Text-269.html) and [font association tables](https://dev.os9.ca/techpubs/mac/Text/Text-271.html).

- `FM_LoadFONDResource()` decodes big-endian fields and association entries, converts the stored count-minus-one to a count, and retains the complete resource copy. Optional tables remain encoded; their offsets are checked for bounds and overlap with associations.
- Loading and validation preserve the caller's handle state and share the same bounds checks.
- `Font_FONDResourceParsing` checks header fields, associations, optional-table retention, every truncation of its fixture, malformed table offsets, and size-distance overflow.
- The NFNT loader and offset/width helpers still need work: the loader copies only the header, the tables are interpreted incorrectly, and the strike bitmap/location ownership does not match the drawing path. End-to-end resource font rendering is not verified.

### Integration Points

- **QuickDraw**: the Chicago fallback used by `DrawString()` reads `chicago_bitmap` in `src/chicago_font_data.c` and scales it to the port's requested point size
- **Window Manager**: window titles, menu tracking, and dialog chrome all call into `DrawString()` backed by the Font Manager

### Font Data

- Source: `include/chicago_font.h` + `src/chicago_font_data.c`
- Metrics: ascent 12 px, descent 3 px, leading 3 px, printable ASCII 0x20–0x7E
- `ChicagoCharInfo` table supplies bit offsets, ink widths, side bearings, and logical advances; space has an explicit +3 px adjustment to match System 7 spacing

### Text Styles

- `FontStyleMetrics.c` calculates style-adjusted character/string widths and extra bounds; it does not draw styled glyphs
- `FontManagerCore.c` draws bold glyphs with a one-pixel offset and draws underline after a string. Italic offset is only applied by the Chicago fallback path when no font strike is available; the strike-rendering path does not currently shear glyphs
- Shadow, outline, and condense/extend drawing are not implemented
- The Chicago fallback samples its bitmap at the requested point size, using shared size-aware advances and metrics. `FontScaling.c` delegates glyph synthesis and text drawing to this renderer rather than approximating larger sizes with repeated glyphs. Mac Roman compositions and symbols use the same scaling path as ASCII.
- `Draw_FontSizeScaling` checks 9-, 12-, and 24-point ink bounds, pen advances, shared metrics, and accented-character widths in the kernel integration suite.

### Caching

The strike list starts with built-in Chicago 12 and can append resource strikes. It has no memory cap or eviction policy. `FlushFonts()` resets the current strike but does not reclaim cached resource strikes.

### Testing

The kernel integration suite exercises size-dependent Chicago drawing and FOND parsing. It does not yet prove original-font rendering, full style synthesis, or resource-backed font selection.

### Known Limitations

1. Only the Chicago 12 strike ships in-tree; Geneva and Monaco fall back to scaled Chicago, which is not equivalent to their original glyphs
2. NFNT parsing and strike rendering are incomplete; resource-backed availability and selection still need regression coverage
3. TrueType support is not implemented
4. Italic rendering is incomplete: only the no-strike Chicago fallback applies a one-pixel offset; strike-backed glyphs are not sheared
5. Shadow, outline, and condense/extend are measured by style helpers but are not rendered
6. Resource strike ownership, reclamation, and cache invalidation need correction

### Next Steps

- Validate FOND/NFNT loading from application resource forks and add representative Geneva/Monaco strikes
- Harden the scaling/synthesis paths with regression images so new styles preserve the System 7 look
