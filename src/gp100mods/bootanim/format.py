"""The "GPBA" boot animation container read by OpenGP100 (see firmware/opengp100/.../ui/boot_animation.h).

All integers little-endian. The area lives at flash offset 0x1CC000 and is at most 1 MiB. Frames are palette-indexed
(8 bit); each frame is stored raw or run-length encoded, whichever is smaller.
"""
import struct
from dataclasses import dataclass, field

MAGIC = b"GPBA"
VERSION = 1
HEADER_BYTES = 0x20
WIDTH, HEIGHT = 160, 128
FLASH_OFFSET = 0x001CC000
AREA_BYTES = 0x00100000
MAX_TOTAL_MS = 10_000
MAX_COLORS = 256


@dataclass
class Animation:
    palette: list                      # RGB565 ints, len 1..256
    frames: list = field(default_factory=list)   # list of (bytes w*h indices, duration_ms)

    @property
    def total_ms(self):
        return sum(d for _, d in self.frames)


def rgb888_to_565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def rgb565_to_888(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def rle_encode(indices: bytes) -> bytes:
    out = bytearray()
    i, n = 0, len(indices)
    while i < n:
        v = indices[i]
        j = i + 1
        while j < n and indices[j] == v and j - i < 255:
            j += 1
        out += bytes((j - i, v))
        i = j
    return bytes(out)


def rle_decode(data: bytes, pixels: int) -> bytes:
    out = bytearray()
    for i in range(0, len(data) - 1, 2):
        out += bytes([data[i + 1]]) * data[i]
        if len(out) >= pixels:
            break
    if len(out) != pixels:
        raise ValueError("RLE frame does not decode to exactly width*height pixels")
    return bytes(out)


def build_area(anim: Animation, check_size: bool = True) -> bytes:
    """Serialise to the byte image stored at FLASH_OFFSET (no padding). check_size=False lets callers measure."""
    if not anim.frames:
        return empty_area()
    if not 1 <= len(anim.palette) <= MAX_COLORS:
        raise ValueError("palette must have 1..256 colours")
    if anim.total_ms == 0 or anim.total_ms > MAX_TOTAL_MS:
        raise ValueError(f"animation length {anim.total_ms} ms is outside 1..{MAX_TOTAL_MS} ms")
    pal = b"".join(struct.pack("<H", c) for c in anim.palette)
    table_off = HEADER_BYTES + len(pal)
    data_start = table_off + 8 * len(anim.frames)
    blobs, entries, cursor = [], [], data_start
    for idx, dur in anim.frames:
        if len(idx) != WIDTH * HEIGHT or max(idx) >= len(anim.palette):
            raise ValueError("frame has the wrong size or an index outside the palette")
        rle = rle_encode(idx)
        enc, blob = (1, rle) if len(rle) < len(idx) else (0, idx)
        entries.append(struct.pack("<IHBB", cursor, int(dur), enc, 0))
        blobs.append(blob)
        cursor += len(blob)
    body = pal + b"".join(entries) + b"".join(blobs)
    total = HEADER_BYTES + len(body)
    if check_size and total > AREA_BYTES:
        raise ValueError(f"animation needs {total} bytes, the area holds {AREA_BYTES}")
    head = MAGIC + struct.pack("<HHHHHHII II", VERSION, HEADER_BYTES, WIDTH, HEIGHT, len(anim.frames),
                               len(anim.palette), total, sum(body) & 0xFFFFFFFF, 0, 0)
    return head + body


def empty_area() -> bytes:
    """Header with no frames: the firmware falls back to its built-in logo."""
    return MAGIC + struct.pack("<HHHHHHII II", VERSION, HEADER_BYTES, WIDTH, HEIGHT, 0, 0, HEADER_BYTES, 0, 0, 0)


def parse_area(area: bytes) -> Animation:
    """Inverse of build_area (also validates like the firmware does). Raises ValueError if not a valid animation."""
    if len(area) < HEADER_BYTES or area[:4] != MAGIC:
        raise ValueError("no GPBA header")
    ver, hsz, w, h, nframes, ncol, total, chk, _, _ = struct.unpack_from("<HHHHHHII II", area, 4)
    if ver != VERSION or hsz != HEADER_BYTES or (w, h) != (WIDTH, HEIGHT):
        raise ValueError("unsupported GPBA version or size")
    if nframes == 0:
        return Animation(palette=[0], frames=[])
    if not (1 <= ncol <= MAX_COLORS) or total > len(area) or total > AREA_BYTES:
        raise ValueError("corrupt GPBA header")
    if sum(area[HEADER_BYTES:total]) & 0xFFFFFFFF != chk:
        raise ValueError("GPBA checksum mismatch")
    pal = [struct.unpack_from("<H", area, HEADER_BYTES + 2 * i)[0] for i in range(ncol)]
    table = HEADER_BYTES + 2 * ncol
    frames = []
    for i in range(nframes):
        off, dur, enc, _ = struct.unpack_from("<IHBB", area, table + 8 * i)
        if enc == 0:
            idx = bytes(area[off:off + WIDTH * HEIGHT])
            if len(idx) != WIDTH * HEIGHT:
                raise ValueError("raw frame truncated")
        elif enc == 1:
            idx = rle_decode(bytes(area[off:total]), WIDTH * HEIGHT)
        else:
            raise ValueError("unknown frame encoding")
        frames.append((idx, dur))
    return Animation(palette=pal, frames=frames)
