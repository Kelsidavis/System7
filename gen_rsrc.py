#!/usr/bin/env python3
"""
gen_rsrc.py  —  Minimal Mac OS .rsrc file generator (classic Resource Manager format)

Currently supports generating 'PAT ' (8x8, 1-bpp classic pattern) resources.
You pass a JSON manifest describing resources and their data; we write a faithful
resource file with a data area and a resource map (types, refs, name list).

USAGE
-----
    python3 gen_rsrc.py patterns.json Patterns.rsrc

MANIFEST FORMAT (JSON)
----------------------
{
  "resources": [
    {
      "type": "PAT ",
      "id": 128,
      "name": "Classic Gray",
      "data": { "bytes": ["0xAA","0x55","0xAA","0x55","0xAA","0x55","0xAA","0x55"] }
    },
    {
      "type": "PAT ",
      "id": 129,
      "name": "Light Dither",
      "data": { "bytes": ["0x88","0x22","0x88","0x22","0x88","0x22","0x88","0x22"] }
    },
    {
      "type": "PAT ",
      "id": 130,
      "name": "Custom",
      "data": { "rows": [
        "10101010",
        "01010101",
        "10101010",
        "01010101",
        "10101010",
        "01010101",
        "10101010",
        "01010101"
      ]}
    }
  ]
}

DATA ENCODING
-------------
For 'PAT ', you can specify the 8 rows as:
  - bytes: 8 byte strings like "0xAA", or integers 0..255
  - rows : 8 strings of 8 chars each, '1' = set bit, '0' = clear bit (leftmost char is bit7)

OUTPUT
------
A classic Resource file with:
  - Header: dataOffset, mapOffset, dataLength, mapLength (big-endian)
  - Data area: for each resource: 4-byte big-endian length, followed by the raw data bytes
  - Map area: includes type list, reference lists, and name list.
This is suitable for loaders that expect real Mac .rsrc files.

LIMITATIONS
-----------
- Only 'PAT ' is implemented. (Extendable for 'ppat' and others later.)
- Names are stored in the map's name list as Pascal strings (max 255 chars).

(c) 2025 — Kelsi + helper
"""
import sys, json, struct, os

def be16(x): return struct.pack(">H", x & 0xFFFF)
def be16s(x): return struct.pack(">h", x if -32768 <= x <= 32767 else ((x+0x8000)&0xFFFF)-0x8000)
def be32(x): return struct.pack(">I", x & 0xFFFFFFFF)

FOURCC = lambda s: s.encode("mac_roman")[:4].ljust(4, b'\x00')

class Resource:
    __slots__ = ("rtype","rid","name","data")
    def __init__(self, rtype: bytes, rid: int, name: str, data: bytes):
        self.rtype = rtype  # 4 bytes
        self.rid   = rid    # int16
        self.name  = name or ""
        self.data  = data   # raw data (without 4-byte length)

def parse_pat_data(entry):
    d = entry.get("data", {})
    if "bytes" in d:
        arr = d["bytes"]
        bytes_ = bytearray()
        for x in arr:
            if isinstance(x, str):
                bytes_.append(int(x, 0))
            else:
                bytes_.append(int(x) & 0xFF)
        if len(bytes_) != 8:
            raise ValueError("PAT 'bytes' must have exactly 8 entries")
        return bytes(bytes_)
    elif "rows" in d:
        rows = d["rows"]
        if len(rows) != 8:
            raise ValueError("PAT 'rows' must have 8 strings of 8 chars ('0'/'1')")
        out = bytearray(8)
        for y, row in enumerate(rows):
            if len(row) != 8: raise ValueError("Each PAT row must be length 8")
            b = 0
            for x, ch in enumerate(row):
                if ch not in ("0","1"): raise ValueError("PAT row chars must be '0' or '1'")
                bit = 1 if ch == "1" else 0
                b |= (bit << (7 - x))  # leftmost char = bit7
            out[y] = b
        return bytes(out)
    else:
        raise ValueError("PAT data requires 'bytes' or 'rows'")

def parse_ppat8_data(entry):
    """Parse PPAT8 color pattern data"""
    d = entry.get("data", {}).get("ppat8", {})

    # Parse palette (list of RGBA quads)
    palette = d.get("palette", [])
    if not palette or len(palette) < 1 or len(palette) > 256:
        raise ValueError("PPAT8 palette must have 1-256 entries")

    # Parse indices (8x8 grid)
    indices = d.get("indices", [])
    if len(indices) != 8:
        raise ValueError("PPAT8 indices must have 8 rows")

    # Build binary format
    out = bytearray()

    # Magic header
    out.extend(b"PPAT8\x00")

    # Width and height (big-endian 16-bit)
    out.extend(be16(8))  # width
    out.extend(be16(8))  # height

    # Palette length
    out.extend(be16(len(palette)))

    # Palette entries (RGBA)
    for color in palette:
        if len(color) != 4:
            raise ValueError("Each palette entry must be [R, G, B, A]")
        out.extend(bytes(color))

    # Indices (64 bytes, one per pixel)
    for row_str in indices:
        row_indices = list(map(int, row_str.split()))
        if len(row_indices) != 8:
            raise ValueError(f"Each index row must have 8 values")
        for idx in row_indices:
            if idx >= len(palette):
                raise ValueError(f"Index {idx} out of palette range")
            out.append(idx)

    return bytes(out)

def parse_str_data(entry):
    """Parse STR (single string) resource data."""
    d = entry.get("data", {})
    s = d.get("string", "")
    encoded = s.encode("mac_roman", errors="replace")
    if len(encoded) > 255:
        encoded = encoded[:255]
    return bytes([len(encoded)]) + encoded

def parse_str_list_data(entry):
    """Parse STR# (string list) resource data.

    Format: 2-byte big-endian count + sequence of Pascal strings.
    """
    d = entry.get("data", {})
    strings = d.get("strings", [])
    out = bytearray()
    # 2-byte big-endian count of strings
    out += struct.pack(">H", len(strings))
    for s in strings:
        encoded = s.encode("mac_roman", errors="replace")
        if len(encoded) > 255:
            encoded = encoded[:255]
        out.append(len(encoded))  # Pascal length byte
        out += encoded
    return bytes(out)

def parse_ppat_raw(entry):
    """Parse raw ppat hex data"""
    d = entry.get("data", {})
    hex_str = d.get("hex", "")
    # Remove any spaces and convert hex string to bytes
    hex_str = hex_str.replace(" ", "")
    return bytes.fromhex(hex_str)

def parse_manifest(manifest):
    resources = []
    for ent in manifest.get("resources", []):
        rtype = ent["type"]
        rid = int(ent["id"])
        name = ent.get("name","")
        if rtype == "PAT ":
            data = parse_pat_data(ent)
        elif rtype == "ppat":
            if "ppat8" in ent.get("data", {}):
                data = parse_ppat8_data(ent)
            elif "hex" in ent.get("data", {}):
                data = parse_ppat_raw(ent)
            elif "bytes" in ent.get("data", {}):
                # Handle bytes format (list of hex strings)
                bytes_list = ent.get("data", {}).get("bytes", [])
                byte_values = []
                for hex_str in bytes_list:
                    # Convert "0x50" -> 80 (decimal)
                    if hex_str.startswith("0x"):
                        byte_values.append(int(hex_str, 16))
                    else:
                        byte_values.append(int(hex_str))
                data = bytes(byte_values)
            else:
                # Skip ppat entries with from_png for now
                continue
        elif rtype == "ppat_raw":
            # Treat ppat_raw as ppat type
            data = parse_ppat_raw(ent)
            rtype = "ppat"  # Convert to standard ppat type
        elif rtype == "STR#":
            data = parse_str_list_data(ent)
        elif rtype == "STR ":
            data = parse_str_data(ent)
        else:
            raise NotImplementedError(f"Unsupported type: {rtype}")
        resources.append(Resource(FOURCC(rtype), rid, name, data))
    return resources

def build_rsrc(resources):
    # Group by type
    by_type = {}
    for r in resources:
        by_type.setdefault(r.rtype, []).append(r)
    # Sort within type by id
    for lst in by_type.values():
        lst.sort(key=lambda r: r.rid)
    # Build data area
    data_area = bytearray()
    data_offsets = {}  # (rtype,rid) -> offset from start of data
    for rtype, lst in by_type.items():
        for r in lst:
            off = len(data_area)
            data_offsets[(rtype, r.rid)] = off
            data_area += be32(len(r.data)) + r.data

    # Build name list: Pascal strings
    name_list = bytearray()
    name_offsets = {}  # name string -> offset (reuse identical names)
    res_name_offset = {}  # (rtype,rid) -> offset or -1
    for rtype, lst in by_type.items():
        for r in lst:
            if r.name == "":
                res_name_offset[(rtype, r.rid)] = -1
                continue
            if r.name in name_offsets:
                res_name_offset[(rtype, r.rid)] = name_offsets[r.name]
                continue
            off = len(name_list)
            nm = r.name.encode("mac_roman", errors="replace")
            if len(nm) > 255: nm = nm[:255]
            name_list.append(len(nm))
            name_list += nm
            name_offsets[r.name] = off
            res_name_offset[(rtype, r.rid)] = off

    # Build type list + reference lists
    # type list format:
    #   int16 (numTypes-1)
    #   [ for each type ]
    #       ResType(4) | int16 (numRes-1) | int16 (offset to ref list from start of type list)
    #
    # ref list entries (for each type):
    #       int16 id | int16 name offset (from name list start or -1) |
    #       1 byte attributes | 3 bytes data offset (from start of data area) | 4 bytes handle (reserved)
    #
    type_records = []
    ref_lists = bytearray()
    # offset within type list: after 2-byte count and N type records comes ref_lists.
    # First, compute sizes to know the offsets.
    num_types = len(by_type)
    # Each type record is 8 bytes
    type_list_header_size = 2
    type_recs_size = num_types * 8
    # We'll append ref_lists immediately after type records; compute per-type offsets as we go.
    running_ref_off = 0
    # We'll assemble after knowing ref sizes; but to know offsets, we need to write sequentially.
    type_entries_tmp = []  # (rtype, numRes, ref_off_from_type_list_start, refs_bytes)
    for rtype, lst in by_type.items():
        refs = bytearray()
        for r in lst:
            nm_off = res_name_offset[(rtype, r.rid)]
            refs += struct.pack(">h", r.rid)
            refs += struct.pack(">h", nm_off if nm_off != -1 else -1)
            refs += bytes([0x00])  # attributes
            data_off = data_offsets[(rtype, r.rid)]
            # 3-byte big-endian data offset from start of data area
            refs += (data_off & 0xFFFFFF).to_bytes(3, "big")
            refs += b"\x00\x00\x00\x00"  # handle (not used on disk)
        type_entries_tmp.append((rtype, len(lst), running_ref_off, refs))
        running_ref_off += len(refs)

    # Now we can write type list block
    type_list = bytearray()
    type_list += be16(num_types - 1)
    # Remember: offsets are from start of type_list
    for rtype, numRes, ref_off, refs in type_entries_tmp:
        type_list += rtype
        type_list += be16(numRes - 1)
        type_list += be16(type_list_header_size + type_recs_size + ref_off)
    # Append refs
    for _, _, _, refs in type_entries_tmp:
        type_list += refs

    # Build the full map area:
    # Map header (copy of file header) + map attrs… but minimally we must include:
    #   at offsets 24..26: int16 typeListOffset (from start of map)
    #   at offsets 26..28: int16 nameListOffset (from start of map)
    # Classic map header is 22 bytes of unknowns + 4 we care about; we’ll mirror common layout:
    map_header = bytearray(28)
    # We'll fill data/map offsets after layout.

    # Assemble the map (header + type list + name list)
    map_area = bytearray()
    map_area += map_header
    type_list_off = len(map_area)
    map_area += type_list
    name_list_off = len(map_area)
    map_area += name_list

    # Now we know final sizes; we can fill the file header and map header copies
    data_off = 256  # We place header at 0..15, then pad to 256 for alignment (classic habit)
    data_len = len(data_area)
    map_off = data_off + data_len
    map_len = len(map_area)

    # File header
    header = bytearray()
    header += be32(data_off)
    header += be32(map_off)
    header += be32(data_len)
    header += be32(map_len)

    # Put the same 16-byte header at the start of the map (classic format mirrors header in map)
    # Also set typeListOffset and nameListOffset inside the map header (relative to start of map)
    # Map header layout (first 28 bytes):
    #   0:  copy of file header (16 bytes)
    #   16: handle to next map (4 bytes, 0)
    #   20: file ref num (2 bytes, 0)
    #   22: attributes (2 bytes, 0)
    #   24: typeListOffset (2 bytes)
    #   26: nameListOffset (2 bytes)
    map_area[0:16] = header
    map_area[24:26] = be16(type_list_off)  # offset from start of map
    map_area[26:28] = be16(name_list_off)

    # Final file layout with classic padding to 256 boundary before data
    pad0 = b"\x00" * (data_off - len(header))
    file_bytes = header + pad0 + data_area + map_area

    return file_bytes

def main():
    if len(sys.argv) < 3:
        print("Usage: gen_rsrc.py <manifest.json> [additional.json ...] <out.rsrc>")
        sys.exit(2)

    # Last arg is output file, rest are input JSON files
    json_paths = sys.argv[1:-1]
    out_path = sys.argv[-1]

    # Merge all JSON files
    merged_resources = []
    for json_path in json_paths:
        if os.path.exists(json_path):
            manifest = json.load(open(json_path, "r", encoding="utf-8"))
            merged_resources.extend(manifest.get("resources", []))
            print(f"Loaded {len(manifest.get('resources', []))} resources from {json_path}")

    # Create merged manifest
    manifest = {"resources": merged_resources}
    resources = parse_manifest(manifest)
    if not resources:
        print("No resources in manifests")
        sys.exit(1)
    out = build_rsrc(resources)
    with open(out_path, "wb") as f:
        f.write(out)
    print(f"Wrote {len(out)} bytes to {out_path}")

if __name__ == "__main__":
    main()
