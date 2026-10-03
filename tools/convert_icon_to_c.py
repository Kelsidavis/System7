#!/usr/bin/env python3
"""Convert a PNG to the project's 128x128 RGBA startup-icon source file."""

import argparse
from pathlib import Path

from PIL import Image


WIDTH = 128
HEIGHT = 128


def convert_to_c_array(input_file: Path, output_file: Path) -> None:
    with Image.open(input_file) as source_image:
        image = source_image.resize(
            (WIDTH, HEIGHT), Image.Resampling.LANCZOS
        ).convert("RGBA")
    pixels = image.load()

    with output_file.open("w", encoding="utf-8", newline="\n") as output:
        output.write("/*\n")
        output.write(f" * Startup icon generated from {input_file.name}\n")
        output.write(f" * Size: {WIDTH}x{HEIGHT} pixels\n")
        output.write(" * Format: RGBA (32-bit per pixel)\n")
        output.write(" */\n\n")
        output.write('#include "Resources/happy_mac_icon.h"\n\n')
        output.write("const UInt8 gHappyMacIconData[] = {\n")

        for y in range(HEIGHT):
            output.write("    ")
            for x in range(WIDTH):
                red, green, blue, alpha = pixels[x, y]
                output.write(
                    f"0x{red:02X},0x{green:02X},0x{blue:02X},0x{alpha:02X},"
                )
                output.write("\n    " if (x + 1) % 4 == 0 else " ")
            output.write(f" /* row {y} */\n")

        output.write("};\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_png", type=Path, help="source PNG")
    parser.add_argument("output_c", type=Path, help="destination C source")
    args = parser.parse_args()
    convert_to_c_array(args.input_png, args.output_c)


if __name__ == "__main__":
    main()
