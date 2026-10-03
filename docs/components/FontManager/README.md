# Font Manager Implementation

## Overview

System 7.1-compatible Font Manager providing bitmap font support with the Chicago strike embedded in the tree. The manager owns font registration, strike lookup, metrics, measurement, and drawing helpers that QuickDraw depends on.

## Implementation Status

### Phase 1: Core Architecture ✓
- `FontTypes.h` – System 7.1-compatible types
- `FontManager.h` – public API surface
- `FontManagerCore.c` – runtime state, strike selection, `InitFonts()` bootstrap

**Key APIs**
- `InitFonts()` wires the built-in families (Chicago, Geneva, Monaco)
- `GetFontName()`/`GetFNum()` swap between IDs and Pascal names
- `RealFont()` reports whether a requested size/style exists without synthesis
- `TextFont()`/`TextFace()`/`TextSize()`/`TextMode()` update the current GrafPort
- `GetFontMetrics()` + `CharWidth()`/`StringWidth()` return ascent, descent, advance
- `FMSwapFont()` swaps the active strike based on style/size arguments

### Phase 2: Resource Loading ✓
- `FontResources.h` and `FontResourceLoader.c` parse FOND/NFNT blobs
- `FM_LoadFONDResource()` + `FM_LoadNFNTResource()` populate strikes from resources
- `FM_ParseOWTTable()` + `FM_ExtractBitmap()` build usable offset/width tables
- Includes validation and debug dump helpers for reverse-engineering sessions

### Integration Points
- **QuickDraw**: `FM_DrawRun()` and `DrawString()` emit glyphs by calling `FM_DrawChicagoCharInternal()` which reads from `chicago_bitmap` in `src/chicago_font_data.c`
- **Window Manager**: window titles, menu tracking, and dialog chrome all call into `DrawString()` backed by the Font Manager

### Font Data
- Source: `include/chicago_font.h` + `src/chicago_font_data.c`
- Metrics: ascent 12 px, descent 3 px, leading 3 px, printable ASCII 0x20–0x7E
- `ChicagoCharInfo` table supplies bit offsets, ink widths, side bearings, and logical advances; space has an explicit +3 px adjustment to match System 7 spacing

### Text Styles
- `FontStyleSynthesis.c` calculates style-adjusted character/string widths and extra bounds; it does not draw styled glyphs
- `FontManagerCore.c` draws bold glyphs with a one-pixel offset and draws underline after a string. Italic offset is only applied by the Chicago fallback path when no font strike is available; the strike-rendering path does not currently shear glyphs
- Shadow, outline, and condense/extend drawing are not implemented
- `FontScaling.c` contains nearest-neighbour upsizing for larger point sizes, sharing the Chicago strike as a base

### Caching Strategy (current vs. future)
- **Current**: single built-in 12-pt strike kept hot in `g_fmState`
- **Planned**: LRU-managed strike list with memory caps (~256 KB) as additional bitmap sizes/styles land

### Testing Hooks
- Fonts are exercised through desktop and application rendering; there is no dedicated font regression suite yet

### Known Limitations
1. Only the Chicago 12 strike ships in-tree; Geneva and Monaco reuse the same metrics
2. FOND/NFNT loading is connected to `GetResource`; end-to-end coverage against real resource forks and non-Chicago strikes is still limited
3. TrueType support is still out of scope
4. Italic rendering is incomplete: only the no-strike Chicago fallback applies a one-pixel offset; strike-backed glyphs are not sheared
5. Shadow, outline, and condense/extend are measured by style helpers but are not rendered
6. Cache invalidation once additional strikes arrive still needs real-world tuning

### Next Steps
- Validate FOND/NFNT loading from application resource forks and add representative Geneva/Monaco strikes
- Harden the scaling/synthesis paths with regression images so new styles preserve the System 7 look
