# QuickDraw Portable Implementation

This directory contains a complete portable implementation of Apple's QuickDraw graphics system, based on the Mac OS 7.1 source code. QuickDraw was the foundation of all Macintosh graphics from 1984 through Mac OS X.

## Overview

QuickDraw provides:
- 2D graphics primitives (lines, rectangles, ovals, arcs, polygons)
- Bitmap and pixmap operations with scaling and transfer modes
- Complex region management and clipping
- 32-bit Color QuickDraw with color tables and dithering
- Pattern fills and texture operations
- Text rendering with multiple fonts and styles
- Coordinate system transformations
- Graphics ports for multiple drawing contexts

## Architecture

### Core Components

- **QuickDrawCore.c** - Basic drawing primitives and GrafPort management
- **ColorQuickDraw.c** - 32-bit color support, color tables, pixel depth handling
- **Regions.c** - Region arithmetic, clipping, hit testing
- **Bitmaps.c** - Bitmap/pixmap operations, CopyBits, scaling
- **Patterns.c** - Pattern fills, dithering, texture operations
- **Text.c** - Text drawing, font management, character width calculations
- **Coordinates.c** - Coordinate system transformations, scaling

### Headers

- **QDTypes.h** - Core data structures (GrafPort, Rect, Point, RGBColor, etc.)
- **QuickDraw.h** - Main QuickDraw API
- **ColorQuickDraw.h** - Color extensions
- **QDRegions.h** - Region management API

### Platform Abstraction

- **QuickDrawPlatform.h** - Platform abstraction interface
- Platform-specific implementations handle actual rendering

## Key Features

### Drawing Primitives
- Lines with arbitrary pen sizes and patterns
- Rectangles, ovals, rounded rectangles, arcs
- Polygons with complex shapes
- All primitives support frame, paint, erase, invert, and fill operations

### Transfer Modes
Complete set of 16 QuickDraw transfer modes:
- `srcCopy`, `srcOr`, `srcXor`, `srcBic`
- `notSrcCopy`, `notSrcOr`, `notSrcXor`, `notSrcBic`
- `patCopy`, `patOr`, `patXor`, `patBic`
- `notPatCopy`, `notPatOr`, `notPatXor`, `notPatBic`

Arithmetic modes:
- `blend`, `addPin`, `addOver`, `subPin`, `addMax`, `subOver`, `adMin`
- `ditherCopy`, `transparent`

### Region System
- Efficient scan-line based representation
- Boolean operations: union, intersection, difference, XOR
- Complex clipping and hit testing
- Memory-optimized storage

### Color Support
- RGB color model with 16-bit components (0-65535)
- Color tables for indexed color modes
- Automatic color matching and dithering
- Support for 1-bit, 8-bit, 16-bit, and 32-bit pixel depths
- Graphics device management

### Pattern System
- 8x8 pixel patterns for fills and pen drawing
- Standard patterns: white, black, gray levels, cross-hatch, dots
- Pattern stretching and transformation
- Dithering for color quantization

## API Compatibility

This implementation maintains full API compatibility with:
- Classic QuickDraw (Mac OS 1.0+)
- Color QuickDraw (Mac OS 4.1+)
- 32-Bit QuickDraw (System 7.0+)

## Usage Example

```c
#include "QuickDraw.h"

// Initialize QuickDraw
QDGlobals qd;
InitGraf(&qd);

// Create a graphics port
GrafPort myPort;
OpenPort(&myPort);

// Set drawing attributes
PenSize(2, 2);
PenPat(QD_BLACK_PATTERN);

// Draw a rectangle
Rect r;
SetRect(&r, 10, 10, 100, 100);
FrameRect(&r);

// Draw some text
MoveTo(10, 120);
DrawString("\pHello, QuickDraw!");

// Clean up
ClosePort(&myPort);
```

## Color Graphics Example

```c
#include "QuickDraw.h"
#include "ColorQuickDraw.h"

// Initialize Color QuickDraw
QDGlobals qd;
InitGraf(&qd);

// Create a color graphics port
CGrafPort myColorPort;
OpenCPort(&myColorPort);

// Set RGB colors
RGBColor red = {65535, 0, 0};
RGBColor blue = {0, 0, 65535};

RGBForeColor(&red);
RGBBackColor(&blue);

// Draw with color
Rect r;
SetRect(&r, 10, 10, 100, 100);
PaintRect(&r);

// Clean up
CloseCPort(&myColorPort);
```

## Platform Integration

To integrate with a specific platform:

1. Implement the functions declared in `QuickDrawPlatform.h`
2. Link against the QuickDraw library
3. Initialize QuickDraw before use

Required platform functions:
- `QDPlatform_Initialize()` - Initialize graphics subsystem
- `QDPlatform_DrawLine()` - Draw line primitive
- `QDPlatform_DrawShape()` - Draw shape primitive
- `QDPlatform_SetPixel()` - Set individual pixel
- `QDPlatform_CopyBits()` - Bitmap copy operation

## Building

To build the QuickDraw library:

```bash
# Compile all source files
gcc -c QuickDrawCore.c ColorQuickDraw.c Regions.c Bitmaps.c Patterns.c Text.c Coordinates.c

# Create static library
ar rcs libquickdraw.a *.o

# Or create shared library
gcc -shared -o libquickdraw.so *.o
```

## Testing

The implementation includes comprehensive test coverage for:
- Basic drawing primitives
- Region operations
- Color management
- Pattern operations
- Text rendering
- Coordinate transformations

## Performance

Optimizations included:
- Efficient region scan-line algorithms
- Memory-optimized data structures
- Platform-specific acceleration hooks
- Minimal memory allocations in hot paths

## Thread Safety

This implementation is thread-safe when:
- Each thread uses separate graphics ports
- Global QuickDraw state is properly protected
- Platform layer implements thread-safe rendering

## Memory Management

- Automatic cleanup of graphics resources
- Handle-based memory management compatible with Mac OS
- Efficient region memory allocation
- Platform-abstracted graphics memory

## Limitations

Current limitations:
- No QuickDraw GX support (different graphics system)
- No PostScript printing support
- Platform layer must be implemented for each target
- Some complex region operations use simplified algorithms

## Future Enhancements

Planned improvements:
- Hardware acceleration support
- Anti-aliased drawing
- Advanced text layout
- Modern font support
- GPU-accelerated operations

## License

Copyright (c) 2025 - System 7.1 Portable Project
Based on Apple Macintosh System Software 7.1 QuickDraw

This code is provided for research and educational purposes.