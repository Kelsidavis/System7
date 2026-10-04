#!/usr/bin/env python3
"""
Create color variants of System 7 icons.
Generates 32x32 ARGB data arrays for trash and HD icons.
"""

from pathlib import Path


def create_color_trash_full():
    """Create a color trash icon (full) - silver/gray metallic with shading"""
    pixels = []

    for y in range(32):
        for x in range(32):
            # Default transparent
            pixel = 0x00000000

            # Lid handle (rows 1)
            if y == 1 and 11 <= x <= 20:
                pixel = 0xFF404040  # Dark gray handle

            # Lid top (row 3)
            elif y == 3 and 2 <= x <= 29:
                pixel = 0xFF606060  # Medium gray lid

            # Lid bottom/can top separator (row 5)
            elif y == 5 and 6 <= x <= 25:
                pixel = 0xFF505050  # Dark line

            # Can body (rows 6-26)
            elif 6 <= y < 26 and 6 <= x <= 25:
                # Outline
                if x == 6 or x == 25:
                    pixel = 0xFF404040  # Side outlines
                # Handle holes (row 7)
                elif y == 7 and (x == 10 or x == 21):
                    pixel = 0xFF303030  # Handle holes
                # Vertical stripes for full trash
                elif y >= 8 and y < 26:
                    # Alternate stripe pattern
                    if (x - 7) % 3 == 0:
                        pixel = 0xFF505050  # Dark stripe
                    else:
                        pixel = 0xFFA0A0A0  # Light gray body

            # Bottom (row 26)
            elif y == 26 and 6 <= x <= 25:
                pixel = 0xFF505050  # Bottom edge

            # Bottom rounded corners (row 27)
            elif y == 27 and 7 <= x <= 24:
                pixel = 0xFF404040  # Bottom shadow

            pixels.append(pixel)

    return pixels


def create_color_trash_empty():
    """Create a color trash icon (empty) - silver/gray metallic"""
    pixels = []

    for y in range(32):
        for x in range(32):
            # Default transparent
            pixel = 0x00000000

            # Lid handle (rows 1)
            if y == 1 and 11 <= x <= 20:
                pixel = 0xFF404040  # Dark gray handle

            # Lid top (row 3)
            elif y == 3 and 2 <= x <= 29:
                pixel = 0xFF606060  # Medium gray lid

            # Lid bottom/can top separator (row 5)
            elif y == 5 and 6 <= x <= 25:
                pixel = 0xFF505050  # Dark line

            # Can body (rows 6-26)
            elif 6 <= y < 26 and 6 <= x <= 25:
                # Outline
                if x == 6 or x == 25:
                    pixel = 0xFF404040  # Side outlines
                # Handle holes (row 7)
                elif y == 7 and (x in [10, 11, 20, 21]):
                    pixel = 0xFF303030  # Handle holes (wider for empty)
                # Vertical lines only (empty)
                elif y >= 8 and y < 26:
                    # Just vertical lines, no full stripes
                    if (x - 7) % 4 == 0:
                        pixel = 0xFF606060  # Faint vertical line
                    else:
                        pixel = 0xFFB0B0B0  # Lighter gray body (empty)

            # Bottom (row 26)
            elif y == 26 and 6 <= x <= 25:
                pixel = 0xFF505050  # Bottom edge

            # Bottom rounded corners (row 27)
            elif y == 27 and 7 <= x <= 24:
                pixel = 0xFF404040  # Bottom shadow

            pixels.append(pixel)

    return pixels


def create_color_hd_icon():
    """Create a color hard drive icon - beige/platinum classic Mac color"""
    pixels = []

    for y in range(32):
        for x in range(32):
            # Default transparent
            pixel = 0x00000000

            # Top edge (row 0)
            if y == 0 and 1 <= x <= 30:
                pixel = 0xFF808080  # Gray top edge

            # Body (rows 1-9)
            elif 1 <= y <= 9:
                # Left and right edges
                if x == 0 or x == 31:
                    if 1 <= x <= 30:
                        pixel = 0xFF808080  # Side edges
                # Main body
                elif 1 <= x <= 30:
                    # Activity light (row 6, around x=3-4)
                    if y == 6 and 3 <= x <= 4:
                        pixel = 0xFF00FF00  # Green activity LED
                    # Drive label area (lighter)
                    elif 2 <= y <= 8 and 10 <= x <= 28:
                        pixel = 0xFFE0E0D0  # Light platinum/beige
                    else:
                        pixel = 0xFFD0D0C0  # Platinum/beige body

            # Bottom edge (row 10)
            elif y == 10 and 1 <= x <= 30:
                pixel = 0xFF808080  # Gray bottom edge

            # Shadow (row 11)
            elif y == 11 and 2 <= x <= 29:
                pixel = 0x80000000  # Semi-transparent shadow

            pixels.append(pixel)

    return pixels


def create_color_folder_icon():
    """Create the tilted blue folder used by the classic Finder."""
    pixels = [0] * 1024

    def inside_polygon(x, y, points):
        inside = False
        previous = len(points) - 1
        for current, (x1, y1) in enumerate(points):
            x2, y2 = points[previous]
            if (y1 > y) != (y2 > y):
                crossing = (x2 - x1) * (y - y1) / (y2 - y1) + x1
                if x < crossing:
                    inside = not inside
            previous = current
        return inside

    shadow = [
        (4, 8),
        (12, 8),
        (15, 10),
        (28, 11),
        (31, 14),
        (29, 24),
        (26, 29),
        (9, 25),
        (3, 19),
    ]
    tab = [(2, 5), (11, 5), (15, 9), (27, 10), (30, 13), (27, 18), (5, 14), (2, 11)]
    face = [(4, 12), (28, 15), (26, 26), (23, 28), (7, 23), (2, 18)]

    for y in range(32):
        for x in range(32):
            pixel = 0
            if inside_polygon(x, y, shadow):
                pixel = 0xFF343434
            if inside_polygon(x, y, tab):
                pixel = 0xFF8883FF if y < 11 else 0xFF534DC2
            if inside_polygon(x, y, face):
                pixel = 0xFFC2BFFF if y < 18 else 0xFF8883FF
                if y == 12 or (x == 4 and y >= 15):
                    pixel = 0xFF000000
            if (
                pixel
                and any(
                    not inside_polygon(nx, ny, face)
                    for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))
                )
                and inside_polygon(x, y, face)
            ):
                pixel = 0xFF000000
            pixels[y * 32 + x] = pixel

    return pixels


def format_color_array(pixels, name):
    """Format pixel data as C uint32_t array"""
    lines = []
    for i in range(0, len(pixels), 8):
        line = ", ".join(f"0x{p:08X}" for p in pixels[i : i + 8])
        if i < len(pixels) - 8:
            line += ","
        lines.append(f"    {line}")
    return f"const uint32_t {name}[1024] = {{\n" + "\n".join(lines) + "\n};"


# Generate color icon data
trash_full_color = create_color_trash_full()
trash_empty_color = create_color_trash_empty()
hd_color = create_color_hd_icon()
folder_color = create_color_folder_icon()

# Create the C source file
c_code = """/* Color Icon Variants for System 7.1
 * 32x32 ARGB color icons
 */

#include <stdint.h>
#include <stddef.h>

/* Color trash full icon - 32x32 ARGB */
"""

c_code += format_color_array(trash_full_color, "icon_TrashFull_color") + "\n\n"
c_code += "/* Color trash empty icon - 32x32 ARGB */\n"
c_code += format_color_array(trash_empty_color, "icon_TrashEmpty_color") + "\n\n"
c_code += "/* Color hard drive icon - 32x32 ARGB */\n"
c_code += format_color_array(hd_color, "icon_HD_color") + "\n"
c_code += "\n/* Color folder icon - 32x32 ARGB */\n"
c_code += format_color_array(folder_color, "icon_Folder_color") + "\n"

# Save the color icons
output_path = Path(__file__).resolve().parents[1] / "src" / "color_icons.c"
with output_path.open("w") as f:
    f.write(c_code)

print(f"Created {output_path} with color variants")
