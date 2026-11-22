"""Shared fixtures: a tiny synthetic OpenGP100-style update file (no real firmware needed)."""
import struct
import sys
from pathlib import Path

import hashlib

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gp100mods.opengp100 import slot_packer  # noqa: E402


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


def make_slot(path: Path, table_at=0x400, sdram_len=0x20):
    """A fake slot image: 0x20 header + 1 KiB vector table (aligned) + filler 'code'."""
    img = bytearray(b"\xff" * 0x3000)
    hdr = struct.pack("<7I", 0x80000 + table_at, 0x400, 0x80000 + table_at + 0x400, sdram_len,
                      0x55555555, 0xAAAAAAAA, 0xA5A5A5A5) + b"V210"
    img[0:0x20] = hdr
    struct.pack_into("<2I", img, table_at, 0x20010000, 0x60082801)
    img[0x1000:0x1018] = b"GP100-FACTORY-PRESETS-1"   # capability marker (see features.supports_presets)
    path.write_bytes(bytes(img))


@pytest.fixture
def banks(tmp_path):
    d = tmp_path / "banks"; d.mkdir()
    (d / "rhythm_samples.bin").write_bytes(bytes(0x935E4))
    (d / "rhythm_patterns.bin").write_bytes(bytes(0x447E))
    (d / "factory_cabinets.bin").write_bytes(bytes(40 * 0x1020))
    return d


@pytest.fixture
def update_file(tmp_path, banks, monkeypatch):
    make_stock(tmp_path / "stock.bin"); make_slot(tmp_path / "slot.bin")
    out = tmp_path / "base.bin"
    monkeypatch.setattr(sys, "argv", ["x", "--slot", str(tmp_path / "slot.bin"), "--banks", str(banks),
                                      "--stock", str(tmp_path / "stock.bin"), "--out", str(out)])
    slot_packer.main()
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
