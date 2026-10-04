#!/usr/bin/env python3
"""Wrap 68K code as a Macintosh application, in MacBinary II.

usage: mkapp.py CODE1.bin OUT.bin NAME [BELOW_A5]

The application has one code segment, entered at its first byte through the
only jump table entry; CODE 0 gives the A5 world's sizes and that entry, and
SIZE -1 asks for a 384K partition. MacBinary carries both forks and the
Finder type and creator, which is how hcopy -m puts a file on an HFS disk.
"""

import struct
import sys
import time


def resource_fork(resources):
    """resources: list of (type, id, bytes). Inside Macintosh: More
    Macintosh Toolbox, 1-121."""
    data = b""
    offsets = []
    for _, _, body in resources:
        offsets.append(len(data))
        data += struct.pack(">I", len(body)) + body

    types = []
    for rtype, rid, _ in resources:
        if rtype not in types:
            types.append(rtype)

    type_list = struct.pack(">h", len(types) - 1)
    ref_lists = b""
    ref_base = 2 + 8 * len(types)  # from the start of the type list
    for rtype in types:
        refs = [
            (rid, off) for (t, rid, _), off in zip(resources, offsets) if t == rtype
        ]
        type_list += rtype + struct.pack(
            ">hH", len(refs) - 1, ref_base + len(ref_lists)
        )
        for rid, off in refs:
            ref_lists += struct.pack(">hhI I", rid, -1, off & 0xFFFFFF, 0)

    header_len = 256
    map_header = 16 + 4 + 2 + 2 + 2 + 2
    type_list_off = map_header
    name_list_off = map_header + len(type_list) + len(ref_lists)
    map_len = name_list_off
    data_off = header_len
    map_off = data_off + len(data)
    header = struct.pack(">IIII", data_off, map_off, len(data), map_len)
    rmap = header + struct.pack(">IhhHH", 0, 0, 0, type_list_off, name_list_off)
    rmap += type_list + ref_lists
    return header + b"\0" * (header_len - 16) + data + rmap


def crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def macbinary(name, ftype, creator, data_fork, rsrc_fork):
    now = int(time.time()) + 2082844800  # seconds since 1904
    hdr = bytearray(128)
    nb = name.encode("mac_roman")
    hdr[1] = len(nb)
    hdr[2 : 2 + len(nb)] = nb
    hdr[65:69] = ftype
    hdr[69:73] = creator
    hdr[83:87] = struct.pack(">I", len(data_fork))
    hdr[87:91] = struct.pack(">I", len(rsrc_fork))
    hdr[91:95] = struct.pack(">I", now)
    hdr[95:99] = struct.pack(">I", now)
    hdr[122] = 129
    hdr[123] = 129
    hdr[124:126] = struct.pack(">H", crc16(bytes(hdr[:124])))

    def pad(b):
        return b + b"\0" * (-len(b) % 128)

    return bytes(hdr) + pad(data_fork) + pad(rsrc_fork)


# ---- Resource builders, for an application's NAME.r.py --------------------
# Each returns the resource's data in the layout Inside Macintosh gives it.


def pstr(s):
    b = s.encode("mac_roman")
    return bytes([len(b)]) + b


def rect(top, left, bottom, right):
    return struct.pack(">hhhh", top, left, bottom, right)


def menu(menu_id, title, items):
    """items: list of (text, key) - key "" for none; text "-" is a line.
    Every item enabled but lines (IM I-364)."""
    flags = 1
    body = b""
    for i, (text, key) in enumerate(items, 1):
        if text != "-" and i < 32:
            flags |= 1 << i
        body += pstr(text) + bytes([0, ord(key) if key else 0, 0, 0])
    return struct.pack(">hhhIi", menu_id, 0, 0, 0, flags) + pstr(title) + body + b"\0"


def mbar(*ids):
    return struct.pack(">h", len(ids)) + b"".join(struct.pack(">h", i) for i in ids)


def wind(bounds, title, proc=0, visible=True, go_away=True, refcon=0):
    return (
        rect(*bounds)
        + struct.pack(
            ">hhhi", proc, 0x100 if visible else 0, 0x100 if go_away else 0, refcon
        )
        + pstr(title)
    )


BUTTON, CHECKBOX, RADIO, STATTEXT, EDITTEXT, USERITEM = 4, 5, 6, 8, 16, 0


def ditl(*items):
    """items: (type, rect, text)"""
    out = struct.pack(">h", len(items) - 1)
    for kind, r, text in items:
        data = text.encode("mac_roman")
        out += b"\0\0\0\0" + rect(*r) + bytes([kind, len(data)]) + data
        if len(data) & 1:
            out += b"\0"
    return out


def alrt(bounds, ditl_id, stages=0x5555):
    return rect(*bounds) + struct.pack(">hH", ditl_id, stages)


def dlog(bounds, ditl_id, title="", proc=1, visible=True, go_away=False, refcon=0):
    return (
        rect(*bounds)
        + struct.pack(
            ">hhhih",
            proc,
            0x100 if visible else 0,
            0x100 if go_away else 0,
            refcon,
            ditl_id,
        )
        + pstr(title)
    )


def main():
    code_path, out_path, name = sys.argv[1:4]
    below_a5 = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0x400
    code = open(code_path, "rb").read()

    jt = struct.pack(">HHhH", 0, 0x3F3C, 1, 0xA9F0)  # CODE 1 +0, _LoadSeg
    code0 = struct.pack(">IIII", 32 + len(jt), below_a5, len(jt), 32) + jt
    code1 = struct.pack(">HH", 0, 1) + code
    size = struct.pack(">HII", 0x0080, 384 * 1024, 384 * 1024)
    resources = [(b"CODE", 0, code0), (b"CODE", 1, code1), (b"SIZE", -1, size)]

    # The application's other resources, if it describes any
    import os

    spec = os.path.join(os.path.dirname(os.path.abspath(__file__)), name + ".r.py")
    if os.path.exists(spec):
        env = dict(globals())
        exec(open(spec).read(), env)
        resources += env["RESOURCES"]

    fork = resource_fork(resources)
    open(out_path, "wb").write(macbinary(name, b"APPL", b"????", b"", fork))


if __name__ == "__main__":
    main()
