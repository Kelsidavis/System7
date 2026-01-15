#!/usr/bin/env python3
"""
Generate embeddable C icon resources from a directory of extracted PNGs.

Usage:
  python3 tools/gen_icons.py /path/to/icons outdir

Conventions:
  - Filenames containing an integer are treated as that resource ID
    e.g., finder_icn_14500.png -> id=14500
  - 16x16 PNGs populate IconFamily.small; >=32 populate IconFamily.large (resized to 32x32)

Outputs:
  include/Resources/icons_generated.h
  src/Resources/generated/icons_generated.c

Dependencies:
  - Pillow (pip install pillow)
"""
import sys, re, os, pathlib, json
from typing import List, Tuple

try:
    from PIL import Image
except Exception as e:
    sys.stderr.write("ERROR: Pillow not installed. Run: pip install pillow\n")
    sys.exit(2)

ICON_H = """
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "Finder/Icon/icon_types.h"

typedef struct { int16_t id; const char* name; const IconFamily* fam; } IconGenEntry;

bool IconGen_FindByID(int16_t id, IconFamily* out);
extern const IconGenEntry gIconGenTable[];
extern const int gIconGenCount;
""".strip()

TEMPLATE_C_PREFIX = """
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "Finder/Icon/icon_types.h"
#include "Resources/icons_generated.h"

/* Auto-generated. Do not edit manually. */

"""

def to_c_ident(name: str) -> str:
    s = re.sub(r"[^A-Za-z0-9]+","_", name)
    if re.match(r"^[0-9]", s):
        s = "_" + s
    return s

def pack_argb32(img: Image.Image) -> List[int]:
    if img.mode != 'RGBA':
        img = img.convert('RGBA')
    w,h = img.size
    pixels = list(img.getdata())
    out = []
    for (r,g,b,a) in pixels:
        out.append(((a & 0xFF)<<24) | ((r & 0xFF)<<16) | ((g & 0xFF)<<8) | (b & 0xFF))
    return out

def emit_array(name: str, values: List[int]) -> str:
    lines = []
    lines.append(f"static const uint32_t {name}[] = {{")
    row = []
    for i,v in enumerate(values):
        row.append(f"0x{v:08X}")
        if (i+1)%8==0:
            lines.append("    " + ", ".join(row) + ",")
            row=[]
    if row:
        lines.append("    " + ", ".join(row) + ",")
    lines.append("};\n")
    return "\n".join(lines)

def main():
    if len(sys.argv) < 3:
        print("Usage: gen_icons.py ICON_DIR OUT_ROOT", file=sys.stderr)
        sys.exit(1)
    icon_dir = pathlib.Path(sys.argv[1])
    out_root = pathlib.Path(sys.argv[2])
    out_inc = out_root / "include/Resources"
    out_src = out_root / "src/Resources/generated"
    out_inc.mkdir(parents=True, exist_ok=True)
    out_src.mkdir(parents=True, exist_ok=True)

    entries = []
    arrays = []
    families = []

    pngs = sorted([p for p in icon_dir.glob("*.png")])
    if not pngs:
        print("No PNGs found in", icon_dir, file=sys.stderr)
        sys.exit(1)

    for png in pngs:
        m = re.search(r"(\d+)", png.stem)
        rid = int(m.group(1)) if m else None
        name = png.stem
        try:
            img = Image.open(png)
        except Exception as e:
            print("Skip", png, e, file=sys.stderr)
            continue
        w,h = img.size
        # Decide target slot
        if w >= 32 or h >= 32:
            tgt = img.resize((32,32), Image.NEAREST)
            arr_name = to_c_ident(f"icon_{name}_32")
            data = pack_argb32(tgt)
            arrays.append(emit_array(arr_name, data))
            fam_name = to_c_ident(f"fam_{name}")
            families.append((fam_name, arr_name, 32,32))
        elif w==16 and h==16:
            tgt = img
            arr_name = to_c_ident(f"icon_{name}_16")
            data = pack_argb32(tgt)
            arrays.append(emit_array(arr_name, data))
            fam_name = to_c_ident(f"fam_{name}")
            families.append((fam_name, arr_name, 16,16))
        else:
            # Resize small to 16x16
            tgt = img.resize((16,16), Image.NEAREST)
            arr_name = to_c_ident(f"icon_{name}_16")
            data = pack_argb32(tgt)
            arrays.append(emit_array(arr_name, data))
            fam_name = to_c_ident(f"fam_{name}")
            families.append((fam_name, arr_name, 16,16))
        entries.append((rid, name, fam_name))

    # Merge families with both sizes under same fam if share base name
    fam_map = {}
    fam_defs = []
    for fam_name, arr_name, w,h in families:
        base = fam_name
        if base not in fam_map:
            fam_map[base] = {"large": None, "small": None}
        if w==32:
            fam_map[base]["large"] = arr_name
        else:
            fam_map[base]["small"] = arr_name

    for base, slots in fam_map.items():
        large = slots["large"]
        small = slots["small"]
        lines = []
        lines.append(f"static const IconFamily {base}_def = {{")
        # large
        if large:
            lines.append(f"    .large = {{32,32,kIconColor32, NULL, NULL, {large}}},")
        else:
            lines.append(f"    .large = {{0,0,kIconColor32, NULL, NULL, NULL}},")
        # small
        if small:
            lines.append(f"    .small = {{16,16,kIconColor32, NULL, NULL, {small}}},")
            lines.append(f"    .hasSmall = true,")
        else:
            lines.append(f"    .small = {{0,0,kIconColor32, NULL, NULL, NULL}},")
            lines.append(f"    .hasSmall = false,")
        lines.append("};\n")
        fam_defs.append("\n".join(lines))

    # Reconcile entries to use base fam definitions
    gen_table = []
    for rid, name, fam_name in entries:
        base = fam_name
        gen_table.append((rid if rid is not None else -1, name, base+"_def"))

    # Write header
    (out_inc/"icons_generated.h").write_text(ICON_H)

    # Write C
    c = [TEMPLATE_C_PREFIX]
    c.extend(arrays)
    c.extend(fam_defs)
    c.append("const IconGenEntry gIconGenTable[] = {\n")
    for rid,name, famref in gen_table:
        c.append(f"    {{{rid}, \"{name}\", &{famref}}},\n")
    c.append("};\n")
    c.append(f"const int gIconGenCount = {len(gen_table)};\n")
    c.append("\n")
    c.append("bool IconGen_FindByID(int16_t id, IconFamily* out) {\n")
    c.append("    /* CRITICAL FIX: Use memcpy instead of struct assignment to avoid ARM64 hang */\n")
    c.append("    extern void* memcpy(void*, const void*, unsigned long);\n")
    c.append("\n")
    c.append("    for (int i=0;i<gIconGenCount;i++){\n")
    c.append("        if (gIconGenTable[i].id==id){\n")
    c.append("            if (out) {\n")
    c.append("                memcpy(out, gIconGenTable[i].fam, sizeof(IconFamily));\n")
    c.append("            }\n")
    c.append("            return true;\n")
    c.append("        }\n")
    c.append("    }\n")
    c.append("\n")
    c.append("    return false;\n}\n")

    (out_src/"icons_generated.c").write_text("\n".join(c))

    print("Generated", len(gen_table), "icons")

if __name__ == '__main__':
    main()
