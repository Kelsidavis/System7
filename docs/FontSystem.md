# System 7.1 Font System Documentation

## Overview

The System 7.1 Portable project includes a complete font system that provides both bitmap and modern font support for the six classic Mac OS fonts. The system requires **no external font installation** and works immediately across all platforms.

## Supported Fonts

| Font | Family ID | Description | Modern Version | System Fallback (Linux) |
|------|-----------|-------------|----------------|-------------------------|
| Chicago | 0 | System font - UI elements and menus | ✅ Chicago.ttf | DejaVu Sans Mono |
| Geneva | 1 | Application font - dialog text | ❌ | Liberation Sans |
| Monaco | 4 | Monospace font - code and terminal | ❌ | DejaVu Sans Mono |
| New York | 2 | Serif font - documents and reading | ❌ | Liberation Serif |
| Courier | 22 | Monospace serif - typewriter style | ❌ | Liberation Mono |
| Helvetica | 21 | Sans serif - clean text | ❌ | Liberation Sans |

## Architecture

### Core Components

1. **SystemFonts** (`src/FontResources/SystemFontData.c`)
   - Static bitmap font data for all six System 7.1 fonts
   - Original Mac OS font metrics and character data
   - Compatible with original System 7.1 font resource format

2. **ModernFontLoader** (`src/FontResources/ModernFontLoader.c`)
   - TrueType/OpenType font file detection and loading
   - Font file name mapping to System 7.1 family IDs
   - Support for TTF, OTF, WOFF, and WOFF2 formats

3. **EmbeddedFonts** (`src/FontResources/EmbeddedFonts.c`)
   - Cross-platform font fallback system
   - Automatic font selection based on size and preference
   - No external dependencies required

### Font Selection Logic

The system automatically chooses the best font based on:

- **Size threshold**: Bitmap fonts for <14pt, modern fonts for ≥14pt
- **Availability**: Falls back gracefully if modern fonts not found
- **Platform**: Uses appropriate system fonts on each OS

### Platform-Specific Fallbacks

| Platform | Chicago | Geneva | Monaco | New York | Courier | Helvetica |
|----------|---------|---------|---------|----------|---------|-----------|
| **Linux** | DejaVu Sans Mono | Liberation Sans | DejaVu Sans Mono | Liberation Serif | Liberation Mono | Liberation Sans |
| **macOS** | Monaco | Geneva | Monaco | Times | Courier | Helvetica |
| **Windows** | Terminal | Arial | Consolas | Times New Roman | Courier New | Arial |

## Usage

### Basic Font System Initialization

```c
#include "FontResources/SystemFonts.h"
#include "FontResources/ModernFontLoader.h"

// Initialize bitmap font system
OSErr err = InitSystemFonts();
if (err != noErr) {
    // Handle error
}

// Initialize embedded font system with fallbacks
err = InitializeEmbeddedFonts();
if (err != noErr) {
    // Handle error
}

// Optionally load modern fonts from directory
LoadModernFonts("/path/to/modern/fonts");
```

### Getting Font Information

```c
// Get system font name for rendering
const char* fontName = GetSystemFontName(kChicagoFont);
// Returns: "DejaVu Sans Mono" on Linux

// Check if modern version is available
Boolean hasModern = IsModernFontAvailable(kChicagoFont);
// Returns: true (Chicago.ttf is included)

// Get optimal font choice
Boolean useModern;
SystemFontPackage* package = GetOptimalFont(kChicagoFont, 16, &useModern);
// For 16pt: useModern = true (≥14pt threshold)
```

### Font Preferences

```c
// Set font preference mode
SetFontPreference(kFontPreferAuto);    // Automatic selection (default)
SetFontPreference(kFontPreferBitmap);  // Always use bitmap fonts
SetFontPreference(kFontPreferModern);  // Prefer modern fonts when available
```

## Building

The font system is built as part of the main project:

```bash
cd src/FontResources
make clean
make                    # Build library and tests
make test               # Run bitmap font tests
make test-modern        # Run modern font integration tests
```

### Test Programs

1. **font_test** - Tests bitmap font system
2. **modern_font_test** - Tests modern font integration
3. **embedded_font_test** - Tests complete embedded system

## Web Integration

The system generates CSS font mappings for web use:

```css
/* Generated at resources/fonts/system7-fonts.css */
.font-Chicago {
    font-family: "Chicago", "DejaVu Sans Mono", sans-serif;
}

.system-font {
    font-family: "Chicago", "Monaco", monospace;
}
```

## Adding More Modern Fonts

To add additional modern font files:

1. Place TTF/OTF files in `resources/fonts/modern/`
2. Use standard naming (e.g., `Geneva.ttf`, `Monaco.ttf`)
3. Re-run `make test-modern` to verify detection

### Font Name Patterns

The system recognizes these filename patterns:

- **Chicago**: `chicago`, `chikargo`, `chikareGo`
- **Geneva**: `geneva`, `finderskeepers`
- **Monaco**: `monaco`
- **New York**: `new york`, `newyork`, `new_york`
- **Courier**: `courier`
- **Helvetica**: `helvetica`

## Font Sources

### Included Fonts

- **Chicago.ttf** (47.4 KB) - From macfonts repository

### Recommended Sources

1. **Urban Renewal Collection** - High-quality recreations
   - https://www.kreativekorp.com/software/fonts/urbanrenewal/

2. **macfonts Repository** - Comprehensive collection
   - https://github.com/JohnDDuncanIII/macfonts

3. **System Fonts** (macOS only)
   - `/System/Library/Fonts/Monaco.ttf`
   - `/System/Library/Fonts/Geneva.ttf`

## Error Handling

The font system provides graceful fallbacks:

- Missing modern fonts → Use bitmap fonts
- Missing bitmap data → Use system fallbacks
- Invalid font files → Skip and continue
- Platform differences → Automatic platform detection

## Performance

- **Bitmap fonts**: Instant loading (embedded data)
- **Modern fonts**: Loaded on demand, cached after first use
- **Memory usage**: ~2MB for all bitmap fonts, varies for modern fonts
- **Startup time**: <10ms for complete system initialization

## Compatibility

- **Platforms**: Linux, macOS, Windows
- **Architectures**: x86_64, ARM64
- **Compilers**: GCC, Clang, MSVC
- **Standards**: C99 compliant

## API Reference

See header files for complete API documentation:
- `include/FontResources/SystemFonts.h` - Bitmap font system
- `include/FontResources/ModernFontLoader.h` - Modern font system
- `src/FontResources/EmbeddedFonts.c` - Embedded system functions