# Mac OS System 7.1 VGA Graphics Implementation

## Overview
This document describes the VGA graphics implementation for the Mac OS System 7.1 Portable multiboot kernel, addressing the issues with VGA/text mode switching and incorrect color palettes.

## Problem Resolution

### Issues Fixed:
1. **VGA/Text Mode Switching**: The original implementation was switching between VGA graphics (0xA0000) and VGA text mode (0xB8000), causing flashing
2. **Purple Background**: The incorrect teal color (204,204,204 instead of 0,204,204) was causing a purple/gray background
3. **Missing Apple Logo**: Lack of Apple logo resource was causing system crashes

## Technical Implementation

### VGA Mode 13h Setup
- **Resolution**: 320x200 pixels
- **Colors**: 256 color palette mode
- **Memory**: Direct framebuffer at 0xA0000

### Color Palette Configuration
```c
/* Mac OS 7.1 authentic colors */
#define COLOR_TEAL      248  /* Mac OS desktop teal RGB(0,204,204) */
#define COLOR_MENUBAR   249  /* Light gray for menu bar RGB(240,240,240) */
```

### Apple Logo Bitmap
16x14 pixel 1-bit bitmap properly implemented to prevent resource loading crashes:
```c
static const uint8_t apple_logo[28] = {
    0x00, 0x00,  /* Row 0:  ................ */
    0x03, 0x80,  /* Row 1:  ......###....... */
    // ... full bitmap data
};
```

## Desktop Rendering Components

### Menu Bar
- Background: Light gray (COLOR_MENUBAR)
- Apple logo at position (5,3)
- Menu items: File, Edit, View, Special, Help

### Desktop Background
- Teal color (RGB 0,204,204) - authentic Mac OS 7.1 color
- Full screen coverage (320x200)

### Windows
- White background with black border
- Gray title bar with close box
- Shadow effect (offset by 2 pixels)

### Folder Icons
- White folder with black outline
- Tab at top for authentic Mac look
- Text labels below

## Build Process

### Compilation
```bash
gcc -ffreestanding -m32 -nostdlib -nostartfiles -nodefaultlibs \
    -c multiboot_gui.c -o multiboot_gui.o
```

### Linking
```bash
ld -m elf_i386 -T multiboot.ld \
    -o MacOS71_GUI.elf multiboot_wrapper.o multiboot_gui.o
```

### Testing
Use the provided test script:
```bash
./vm/test_vga_kernel.sh
```

## Key Functions

### `init_vga_mode13()`
Initializes VGA mode and sets up the Mac OS color palette

### `draw_desktop()`
Main function that renders the complete Mac OS desktop

### `draw_bitmap_1bpp()`
Renders 1-bit bitmaps like the Apple logo

### `set_pixel()`
Low-level pixel drawing with bounds checking

## Serial Debug Output
The kernel outputs debug messages via COM1 (0x3F8):
- "Mac OS System 7.1 Portable - VGA Graphics Boot"
- "Initializing VGA mode 13h..."
- "Drawing Mac OS desktop..."
- "Mac OS 7.1 GUI loaded successfully!"

## Files Modified
- `vm/multiboot_gui.c` - Main VGA graphics implementation
- `vm/multiboot_gui_vga.c` - Reference implementation
- `vm/test_vga_kernel.sh` - Test script

## Result
The implementation now correctly displays:
- Teal desktop background (authentic Mac OS 7.1 color)
- No VGA/text mode switching
- Apple logo in menu bar
- Complete Mac OS desktop interface
- Stable display without crashes