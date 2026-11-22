"""Edit an OpenGP100 update file (the .bin the official GP-100 app installs): replace the boot animation area in
section 0 and re-seal the FRMW container. Pure Python, no build tools."""
from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

from . import features, frmw, presets as P
from .bootanim import format as F

SLOT_BASE = 0x80000
AREA_REL = F.FLASH_OFFSET - SLOT_BASE            # offset of the animation area inside section 0


@dataclass
class Info:
    ok: bool
    message: str
    supports_animation: bool = False
    supports_flags: bool = False
    flags: int = 0
    has_custom_animation: bool = False
    frames: int = 0
    total_ms: int = 0
    area_bytes_used: int = 0
    presets: int = 0


def _sections(data: bytes):
    r = frmw.parse(data)
    return r, [(s["file_offset"], s["size"]) for s in r["sections"]]


def inspect(path) -> Info:
    data = Path(path).read_bytes()
    if data[:4] != b"FRMW" or len(data) < 0x100:
        return Info(False, "This is not a GP-100 firmware update file (it should start with 'FRMW').")
    try:
        r, secs = _sections(data)
    except Exception as e:                       # noqa: BLE001
        return Info(False, f"not a firmware update file ({e})")
    if not r["ok"]:
        bad = [c["check"] for c in r["checks"] if not c["ok"]]
        return Info(False, "firmware checks failed: " + ", ".join(bad))
    off, size = secs[0]
    s0 = data[off:off + size]
    if len(s0) < AREA_REL + F.HEADER_BYTES or s0[AREA_REL:AREA_REL + 4] != F.MAGIC:
        return Info(True, "valid update file, but this firmware has no boot-animation area "
                          "(build it with the boot-animation patch)")
    try:
        anim = F.parse_area(s0[AREA_REL:])
    except ValueError as e:
        return Info(True, f"boot-animation area is damaged ({e})", supports_animation=True)
    n = len(anim.frames)
    fl = features.read(s0)
    used = len(F.build_area(anim)) if n else F.HEADER_BYTES
    block = s0[AREA_REL + _round_up(used):]
    try:
        count = len(P.parse_block(block))
    except ValueError:
        count = 0
    return Info(True, "ready", True, fl is not None, fl or 0, n > 0, n, anim.total_ms, len(F.build_area(anim)) if n else 0, count)


def _round_up(n: int) -> int:
    return (n + 0xFFF) & ~0xFFF


def _seal(data: bytes, sections: list) -> bytes:
    off, table = 0x20, b""
    for sec in sections:
        table += struct.pack("<II", off, len(sec)); off += len(sec)
    body = table + b"".join(sections)
    head = bytearray(data[:frmw.HEADER_SIZE])
    struct.pack_into("<I", head, 4, len(body))
    head[0x0E] = sum(body) & 0xFF
    out = bytes(head) + body
    if not frmw.parse(out)["ok"]:
        raise RuntimeError("internal error: result fails the firmware checks")
    return out


def compose(section0: bytes, valeton, anim: F.Animation | None = None, patches=(), flags: int | None = None, fills=(), presets=()) -> bytes:
    """OpenGP100 section 0 (as bundled) + the user's verified Valeton parts + switch patches + boot animation
    (None = built-in logo) -> a complete update file for the GP-100 app.

    `valeton` is a gp100mods.valeton.Parts (container header + official sections 1-3).
    fills: Valeton-derived regions to restore from the user's verified official file (bundle.Fill); each result is checked
    against its SHA-256, so a wrong or damaged official file can never produce a firmware.
    flags: feature flags to store in the firmware (None = leave the block as bundled); needs a firmware that has the block.
    presets: preset records (presets.Preset.record bytes, None = leave the slot empty), slot P01 first. They are stored after the
    boot animation (docs: patch 0004) and shown in a slot until something is saved there.
    patches: iterable of {"offset", "old", "new"} relative to section 0. Each patch is refused unless the bytes there are
    exactly `old` (or already `new`), so a patch can only ever be applied to the build it was written for."""
    sec0 = bytearray(section0)
    if len(sec0) < AREA_REL + F.HEADER_BYTES or bytes(sec0[AREA_REL:AREA_REL + 4]) != F.MAGIC:
        raise ValueError("this firmware has no boot-animation area")
    area = F.build_area(anim) if anim and anim.frames else F.empty_area()
    area += b"\xff" * (-len(area) % 4)
    if fills:
        import hashlib
        official = valeton.section0 if getattr(valeton, "file", b"") else b""
        if not official:
            raise ValueError("the official Valeton firmware is needed to build this version")
        for f in fills:
            region = bytearray(official[f.source:f.source + f.length])
            for off, data in f.patch:
                region[off:off + len(data)] = data
            if len(region) != f.length or hashlib.sha256(region).hexdigest() != f.sha256:
                raise ValueError(f"the official firmware does not match what {f.name} needs")
            sec0[f.dest:f.dest + f.length] = region
    if flags is not None:
        if not features.supports(bytes(sec0)):
            raise ValueError("this firmware version has no feature switches")
        sec0[features.REL:features.REL + features.BLOCK_BYTES] = features.build_block(flags)
    for p in patches:
        o, old, new = p["offset"], p["old"], p["new"]
        if o + len(old) > AREA_REL:
            raise ValueError("a patch may not touch the boot-animation area")
        if o < features.REL + features.BLOCK_BYTES and o + len(old) > features.REL:
            raise ValueError("a patch may not touch the feature-flags block")
        here = bytes(sec0[o:o + len(old)])
        if here == new:
            continue
        if here != old:
            raise ValueError(f"patch at 0x{o:x} does not match this firmware build; refusing to apply it")
        sec0[o:o + len(new)] = new
    area += b"\xff" * (_round_up(len(area)) - len(area))              # the preset block starts on a 4 KiB boundary
    area += P.build_block(presets)                                  # always present, so stale presets of an older install never show
    if len(area) > F.AREA_BYTES:
        raise ValueError("the boot animation and the presets do not fit together; use a shorter animation or fewer presets")
    sec0 = bytes(sec0[:AREA_REL]) + area
    return _seal(valeton.header, [sec0, *valeton.sections])


def section0_of(update_file_bytes: bytes) -> bytes:
    """Developer helper: section 0 of a complete update file."""
    r, secs = _sections(update_file_bytes)
    o, s = secs[0]
    return update_file_bytes[o:o + s]


def write_animation(src, dst, anim: F.Animation | None) -> None:
    """Developer helper: write `dst` = the complete update file `src` with its boot animation replaced."""
    from .valeton import Parts
    if Path(src).resolve() == Path(dst).resolve():
        raise ValueError("choose a different output file; the original is never overwritten")
    info = inspect(src)
    if not info.supports_animation:
        raise ValueError(info.message)
    data = Path(src).read_bytes()
    r, secs = _sections(data)
    parts = Parts(data[:frmw.HEADER_SIZE], tuple(data[o:o + s] for o, s in secs[1:4]))
    Path(dst).write_bytes(compose(section0_of(data), parts, anim))
