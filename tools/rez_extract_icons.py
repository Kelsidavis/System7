#!/usr/bin/env python3
"""
rez_extract_icons.py

Minimal extractor for classic Mac OS resource forks to pull out ICN# (32x32 1‑bit)
and ics#/SICN (16x16 1‑bit) icons with their original resource IDs.

Usage:
  python3 tools/rez_extract_icons.py INPUT... --out OUTDIR

Accepted inputs:
  - MacBinary (.bin) files (Finder.bin, System.bin, etc.)
  - Raw resource fork files (.rsrc)
  - Best‑effort fallback: treat file bytes as a resource fork if header not present

Output naming:
  OUTDIR/ICN#_<id>_32.png, OUTDIR/ics#_<id>_16.png, OUTDIR/SICN_<id>_16.png

Notes:
  - Only 1‑bit icons are handled here. Color cicn parsing can be added later.
"""

import argparse
import struct
from pathlib import Path
from typing import Dict, List, Tuple, Optional

try:
    from PIL import Image
except Exception:
    raise SystemExit("Pillow is required: pip install pillow")


def read_be16(b: bytes, o: int) -> int:
    return struct.unpack_from(">H", b, o)[0]


def read_be32(b: bytes, o: int) -> int:
    return struct.unpack_from(">I", b, o)[0]


def parse_macbinary(fp: Path) -> bytes:
    data = fp.read_bytes()
    if len(data) < 128:
        return data  # Not MacBinary
    # MacBinary II: data length at 83..86, resource at 87..90
    # Validate first byte is 0, name length sane
    name_len = data[1]
    if data[0] != 0 or name_len > 63:
        return data  # Not MacBinary
    rlen = read_be32(data, 87)
    if rlen == 0:
        return b""
    # Resource fork is stored after the data fork padded to 128-byte boundary
    dlen = read_be32(data, 83)
    data_fork_start = 128
    data_fork_end = data_fork_start + ((dlen + 127) & ~127)
    res_fork_start = data_fork_end
    res_fork_end = res_fork_start + ((rlen + 127) & ~127)
    if res_fork_end > len(data):
        # Fallback: sometimes resource fork placed right after header
        return data
    return data[res_fork_start : res_fork_start + rlen]


def parse_resource_fork(res: bytes) -> Dict[str, List[Tuple[int, bytes]]]:
    if len(res) < 16:
        raise ValueError("Resource fork too small")
    data_off = read_be32(res, 0)
    map_off = read_be32(res, 4)
    data_len = read_be32(res, 8)
    map_len = read_be32(res, 12)
    if map_off + map_len > len(res) or data_off + data_len > len(res):
        raise ValueError("Invalid offsets in resource fork")
    data_sec = res[data_off : data_off + data_len]
    rmap = res[map_off : map_off + map_len]
    # Offsets within map are relative to start of map
    # The type list offset is at byte 24 in the map (per TN 1041).
    type_list_off = read_be16(rmap, 24)
    if type_list_off == 0xFFFF:
        return {}
    # Type list: numTypesMinus1 at type_list_off
    tl_base = type_list_off
    num_types = read_be16(rmap, tl_base) + 1
    out: Dict[str, List[Tuple[int, bytes]]] = {}
    for i in range(num_types):
        te_off = tl_base + 2 + i * 8
        rtype = rmap[te_off : te_off + 4].decode("mac_roman", errors="ignore")
        num_res = read_be16(rmap, te_off + 4) + 1
        ref_list_off = read_be16(rmap, te_off + 6)
        rl_base = tl_base + ref_list_off
        for j in range(num_res):
            re_off = rl_base + j * 12
            res_id = struct.unpack_from(">h", rmap, re_off)[0]
            # Attributes at +4, data offset (24-bit) at +5..+7 relative to data section
            data_ofs_24 = int.from_bytes(rmap[re_off + 5 : re_off + 8], "big")
            # Data record: at data_ofs_24 in data section is 4‑byte length then data
            if data_ofs_24 + 4 > len(data_sec):
                continue
            dlen = read_be32(data_sec, data_ofs_24)
            dstart = data_ofs_24 + 4
            if dstart + dlen > len(data_sec):
                continue
            blob = data_sec[dstart : dstart + dlen]
            out.setdefault(rtype, []).append((res_id, blob))
    return out


def save_icn_bitmap(blob: bytes, size: int, out_png: Path):
    # ICN# data is 1024 bytes for 32×32 image bitmap immediately followed by
    # 1024 bytes mask (sometimes vice versa). Many sets use image+mask.
    # We treat any extra as mask if present; otherwise, paint black on white image.
    bits_per_row = size
    bytes_per_row = bits_per_row // 8
    plane_bytes = bytes_per_row * size
    if len(blob) < plane_bytes:
        return
    img_plane = blob[:plane_bytes]
    mask_plane = (
        blob[plane_bytes : plane_bytes * 2] if len(blob) >= plane_bytes * 2 else None
    )
    # Create RGBA
    im = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    px = im.load()
    for y in range(size):
        row = img_plane[y * bytes_per_row : (y + 1) * bytes_per_row]
        mask_row = (
            mask_plane[y * bytes_per_row : (y + 1) * bytes_per_row]
            if mask_plane
            else None
        )
        for x in range(size):
            byte = row[x // 8]
            bit = (byte >> (7 - (x % 8))) & 1
            if bit:
                alpha = (
                    255
                    if (not mask_row)
                    else (255 if ((mask_row[x // 8] >> (7 - (x % 8))) & 1) else 0)
                )
                px[x, y] = (0, 0, 0, alpha)
            else:
                alpha = (
                    0
                    if (mask_row and ((mask_row[x // 8] >> (7 - (x % 8))) & 1) == 0)
                    else 0
                )
                px[x, y] = (255, 255, 255, alpha)
    im.save(out_png)


def parse_color_table(blob: bytes) -> List[Tuple[int, int, int]]:
    if len(blob) < 8:
        return []
    ctSize = read_be16(blob, 6)
    colors: List[Tuple[int, int, int]] = []
    pos = 8
    for _ in range(ctSize + 1):
        if pos + 8 > len(blob):
            break
        # Value field is often sequential but not guaranteed; we rely on table order.
        r = read_be16(blob, pos + 2) >> 8
        g = read_be16(blob, pos + 4) >> 8
        b = read_be16(blob, pos + 6) >> 8
        colors.append((r, g, b))
        pos += 8
    return colors


def parse_cicn(
    blob: bytes,
    out_png: Path,
    fallback_palettes: Optional[Dict[int, List[Tuple[int, int, int]]]] = None,
):
    # Parse Color Icon (cicn) minimally: PixMap + CTab + pixel data + 1-bit mask
    try:
        if len(blob) < 50:
            return
        # PixMap
        _base_addr = read_be32(blob, 0)
        rowBytes = struct.unpack_from(">H", blob, 4)[0] & 0x3FFF
        top = struct.unpack_from(">h", blob, 6)[0]
        left = struct.unpack_from(">h", blob, 8)[0]
        bottom = struct.unpack_from(">h", blob, 10)[0]
        right = struct.unpack_from(">h", blob, 12)[0]
        _pm_version = read_be16(blob, 14)
        packType = read_be16(blob, 16)
        _pack_size = read_be32(blob, 18)
        _h_res = read_be32(blob, 22)
        _v_res = read_be32(blob, 26)
        _pixel_type = read_be16(blob, 30)
        pixelSize = read_be16(blob, 32)
        cmpCount = read_be16(blob, 34)
        _cmp_size = read_be16(blob, 36)
        _plane_bytes = read_be32(blob, 38)
        pmTable = read_be32(blob, 42)
        _pm_reserved = read_be32(blob, 46)
        width = right - left
        height = bottom - top
        if width <= 0 or height <= 0:
            return
        # Locate color table
        # Heuristic: if pmTable looks like a valid offset into blob and starts with ctSeed, use it.
        ct_off = 50
        if 0 < pmTable < len(blob) - 8:
            ct_off = pmTable
        # Read ColorTable
        if ct_off + 8 > len(blob):
            return
        _ct_seed = read_be32(blob, ct_off)
        _ct_flags = read_be16(blob, ct_off + 4)
        ctSize = read_be16(blob, ct_off + 6)
        ncolors = ctSize + 1
        colors_map: Dict[int, Tuple[int, int, int]] = {}
        pos = ct_off + 8
        for _ in range(ncolors):
            if pos + 8 > len(blob):
                break
            val = read_be16(blob, pos)
            r = read_be16(blob, pos + 2) >> 8
            g = read_be16(blob, pos + 4) >> 8
            b = read_be16(blob, pos + 6) >> 8
            colors_map[val] = (r, g, b)
            pos += 8
        # Pixel data follows color table
        pixel_off = pos
        if cmpCount != 1 or packType not in (0,):
            return
        if pixelSize == 0:
            pixelSize = 1
        if pixelSize not in (1, 2, 4, 8):
            return
        pixel_bytes = rowBytes * height
        if pixel_off + pixel_bytes > len(blob):
            return
        # Optional 1-bit mask after pixels
        mask_off = pixel_off + pixel_bytes
        mask_row_bytes = ((width + 15) // 16) * 2
        mask_bytes = mask_row_bytes * height
        has_mask = mask_off + mask_bytes <= len(blob)
        im = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        px = im.load()

        def sample_pixel(row: bytes, x: int) -> int:
            if pixelSize == 8:
                if x >= len(row):
                    return 0
                return row[x]
            bits = pixelSize * x
            byte_index = bits // 8
            if byte_index >= len(row):
                return 0
            shift = 8 - pixelSize - (bits % 8)
            mask = (1 << pixelSize) - 1
            return (row[byte_index] >> shift) & mask

        palette_fallback: Optional[List[Tuple[int, int, int]]] = None
        if fallback_palettes:
            palette_fallback = fallback_palettes.get(pixelSize)

        def palette_lookup(idx: int) -> Tuple[int, int, int]:
            if idx in colors_map:
                return colors_map[idx]
            if palette_fallback and idx < len(palette_fallback):
                return palette_fallback[idx]
            # If ColorTable omitted entries and no fallback palette, fall back to greyscale ramp.
            span = (1 << pixelSize) - 1
            grey = int(idx / span * 255) if span else 0
            return (grey, grey, grey)

        for y in range(height):
            row = blob[pixel_off + y * rowBytes : pixel_off + (y + 1) * rowBytes]
            mrow = None
            if has_mask:
                mrow = blob[
                    mask_off + y * mask_row_bytes : mask_off + (y + 1) * mask_row_bytes
                ]
            for x in range(width):
                idx = sample_pixel(row, x)
                r, g, b = palette_lookup(idx)
                a = 255
                if mrow is not None:
                    bit = (mrow[x // 8] >> (7 - (x % 8))) & 1
                    a = 255 if bit else 0
                px[x, y] = (r, g, b, a)
        im.save(out_png)
    except Exception:
        return


def process_file(path: Path, outdir: Path):
    raw = path.read_bytes()
    # Try MacBinary first
    resfork = parse_macbinary(path)
    if not resfork:
        # Maybe .rsrc directly
        try:
            resfork = raw
        except Exception:
            return
    try:
        types = parse_resource_fork(resfork)
    except Exception:
        return
    # Gather colour palettes from any clut resources for fallback usage.
    clut_palettes: Dict[int, List[Tuple[int, int, int]]] = {}
    fallback_palettes: Dict[int, List[Tuple[int, int, int]]] = {}
    for rid, blob in types.get("clut", []):
        palette = parse_color_table(blob)
        if not palette:
            continue
        clut_palettes[rid] = palette
        length = len(palette)
        if length == 256 and 8 not in fallback_palettes:
            fallback_palettes[8] = palette
        elif length == 16 and 4 not in fallback_palettes:
            fallback_palettes[4] = palette
        elif length == 4 and 2 not in fallback_palettes:
            fallback_palettes[2] = palette
        elif length == 2 and 1 not in fallback_palettes:
            fallback_palettes[1] = palette
    if 1 not in fallback_palettes:
        fallback_palettes[1] = [(0, 0, 0), (255, 255, 255)]
    # ICN# 32x32 (mono)
    for rid, blob in types.get("ICN#", []):
        out_png = outdir / f"ICN#_{rid}_32.png"
        save_icn_bitmap(blob, 32, out_png)
    # ics# 16x16 (mono)
    for rid, blob in types.get("ics#", []):
        out_png = outdir / f"ics#_{rid}_16.png"
        save_icn_bitmap(blob, 16, out_png)
    for rid, blob in types.get("SICN", []):
        out_png = outdir / f"SICN_{rid}_16.png"
        save_icn_bitmap(blob, 16, out_png)
    # cicn color icons (use palette + optional mask)
    for rid, blob in types.get("cicn", []):
        out_png = outdir / f"cicn_{rid}_32.png"
        parse_cicn(blob, out_png, fallback_palettes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inputs", nargs="+", help="MacBinary .bin or .rsrc files")
    ap.add_argument("--out", required=True, help="Output directory for PNGs")
    args = ap.parse_args()
    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    for inp in args.inputs:
        process_file(Path(inp), outdir)
    print("Extraction completed to:", outdir)


if __name__ == "__main__":
    main()
