# Gallery's resources: two pictures made by hand, in the formats Inside
# Macintosh: Imaging With QuickDraw, appendix A, gives - a version 1 one and
# a version 2 one with colour and an 8-bit image - for DrawPicture to play.
import struct as _s

def _rect(top, left, bottom, right):
    return _s.pack(">hhhh", top, left, bottom, right)

def _pict_v1():
    ops = b"\x11\x01"
    ops += b"\x31" + _rect(5, 5, 20, 30)                    # PaintRect
    ops += b"\x50" + _rect(5, 40, 35, 75)                   # FrameOval
    ops += b"\x20" + _s.pack(">hhhh", 35, 5, 25, 30)        # Line
    ops += b"\x98" + _s.pack(">h", 2) + _rect(0, 0, 8, 16)  # PackBitsRect, 1 bit
    ops += _rect(0, 0, 8, 16) + _rect(22, 40, 30, 56) + _s.pack(">h", 0)
    for y in range(8):                                      # rows under 8 bytes: unpacked
        ops += b"\xAA\xAA" if y % 2 == 0 else b"\x55\x55"
    ops += b"\xFF"
    body = _rect(0, 0, 40, 80) + ops
    return _s.pack(">H", len(body) + 2) + body

def _pict_v2():
    out = bytearray()
    def op(code, data=b""):
        if len(out) % 2:
            out.append(0)
        out.extend(_s.pack(">H", code) + data)
    op(0x0011, b"\x02\xFF")
    op(0x0C00, _s.pack(">hhIIhhhhI", -2, 0, 0x480000, 0x480000, 0, 0, 40, 80, 0))
    op(0x001E)
    op(0x0001, _s.pack(">h", 10) + _rect(0, 0, 40, 80))
    op(0x001A, _s.pack(">HHH", 0xFFFF, 0, 0))               # red
    op(0x0031, _rect(5, 5, 20, 30))
    op(0x001A, _s.pack(">HHH", 0, 0, 0xFFFF))               # blue
    op(0x0051, _rect(5, 40, 20, 75))
    op(0x001A, _s.pack(">HHH", 0, 0, 0))
    pm = _s.pack(">H", 0x8000 | 16) + _rect(0, 0, 8, 16)
    pm += _s.pack(">hhIIIhhhhIII", 0, 0, 0, 0x480000, 0x480000, 0, 8, 1, 8, 0, 0, 0)
    ctab = _s.pack(">IHh", 0, 0, 3)
    for i, (r, g, b) in enumerate([(0xFFFF,) * 3, (0xFFFF, 0, 0), (0, 0xFFFF, 0), (0, 0, 0xFFFF)]):
        ctab += _s.pack(">HHHH", i, r, g, b)
    rows = b""
    for y in range(8):
        packed = b"\xFD\x01\xFD\x02\xFD\x03\xFD\x00"        # four each of 1, 2, 3, 0
        rows += bytes([len(packed)]) + packed
    op(0x0098, pm + ctab + _rect(0, 0, 8, 16) + _rect(24, 5, 36, 77) + _s.pack(">h", 0) + rows)
    op(0x00FF)
    body = _rect(0, 0, 40, 80) + bytes(out)
    return _s.pack(">H", (len(body) + 2) & 0xFFFF) + body

RESOURCES = [
    (b"PICT", 128, _pict_v1()),
    (b"PICT", 129, _pict_v2()),
]
