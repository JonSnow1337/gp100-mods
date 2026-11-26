"""Shared fixtures: a tiny synthetic OpenGP100-style update file (no real firmware needed)."""
import struct
import sys
from pathlib import Path

import hashlib

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))


def make_stock(path: Path):
    """A fake 4-section FRMW file with the stock section sizes (contents are arbitrary)."""
    sizes = [0x1000, 0x4000, 0x28500, 0x78000]
    secs = [bytes([i + 1]) * s for i, s in enumerate(sizes)]
    table, off = b"", 0x20
    for s in secs:
        table += struct.pack("<II", off, len(s)); off += len(s)
    body = table + b"".join(secs)
    head = bytearray(b"FRMW" + bytes(0x2C))
    struct.pack_into("<I", head, 4, len(body)); head[0x10:0x15] = b"GP100"; head[0x0E] = sum(body) & 0xFF
    path.write_bytes(bytes(head) + body)


SLOT_HEADER = struct.pack("<7I4s", 0x080020, 0x34951, 0x0B4971, 0x26B179, 0x55555555, 0xAAAAAAAA, 0xA5A5A5A5, b"V210")


def make_opengp_update(path: Path, *, zero_word=False, entry=9, marker=True, used=None):
    """What OpenGP100's own build produces (target gp100_stock_slot + template): a 4-section update file whose section 0 is the
    slot image (header, ITCM trampoline, code, rhythm banks), 0xFF elsewhere. Contents are fake; only the layout is real.
    `used` = section-0 offset where to put a stray non-0xFF byte (to model a build that uses our reserved areas)."""
    sizes = [0x29FAEC, 0x4000, 0x28500, 0x78000]
    secs = [bytes([i + 1]) * s for i, s in enumerate(sizes)]
    sec0 = bytearray(b"\xff" * sizes[0])
    hdr = bytearray(SLOT_HEADER)
    if zero_word:
        hdr[12:16] = bytes(4)                                    # SDRAM payload length 0: the vendor bootloader refuses it
    sec0[:0x20] = hdr
    struct.pack_into("<2I", sec0, 0x20, 0x20020000, entry)       # ITCM trampoline vectors: stack, Thumb entry
    if marker:
        sec0[0x1000:0x1018] = b"GP100-FACTORY-PRESETS-1"          # capability marker (features.supports_presets)
    sec0[0xB3DDC:0xB3DDC + 0x935E4] = bytes(0x935E4)              # rhythm samples at flash 0x133DDC
    sec0[0x147414:0x147414 + 0x447E] = bytes(0x447E)              # rhythm patterns at flash 0x1C7414
    if used is not None:
        sec0[used] = 0x12
    secs[0] = bytes(sec0); secs[2] = bytes(40 * 0x1020)
    table, off = b"", 0x20
    for x in secs:
        table += struct.pack("<II", off, len(x)); off += len(x)
    body = table + b"".join(secs)
    head = bytearray(b"FRMW" + bytes(0x2C))
    struct.pack_into("<I", head, 4, len(body)); head[0x10:0x15] = b"GP100"; head[0x0E] = sum(body) & 0xFF
    path.write_bytes(bytes(head) + body)


@pytest.fixture
def update_file(tmp_path):
    """An OpenGP100 update file as this app expects it: OpenGP100's build output + opengp100.finish."""
    from gp100mods.opengp100 import finish
    raw = tmp_path / "raw.bin"; make_opengp_update(raw)
    out = tmp_path / "base.bin"
    out.write_bytes(finish.finish(raw.read_bytes()))
    return out


def profile_for(data: bytes):
    """A valeton.Profile describing a synthetic 'official' file (the real hashes are not needed in tests)."""
    from gp100mods import frmw, valeton
    secs = frmw.parse(data)["sections"]
    h = lambda b: hashlib.sha256(b).hexdigest()          # noqa: E731
    parts = [data[x["file_offset"]:x["file_end"]] for x in secs[1:4]]
    return valeton.Profile("Synthetic Official FW", h(data), len(data), h(data[:0x30]),
                           tuple(len(p) for p in parts), tuple(h(p) for p in parts))


@pytest.fixture
def official(tmp_path):
    """(path of a synthetic official update file, its Profile)"""
    p = tmp_path / "official.bin"
    make_stock(p)
    return p, profile_for(p.read_bytes())
