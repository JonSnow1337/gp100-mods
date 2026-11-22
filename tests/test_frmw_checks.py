"""The two boot-path bugs found on the real pedal must be caught by the checker."""
import struct
import sys

from conftest import make_slot, make_stock
from gp100mods import frmw
from gp100mods.opengp100 import slot_packer


def build(tmp_path, banks, monkeypatch, **slot_kw):
    make_stock(tmp_path / "stock.bin"); make_slot(tmp_path / "slot.bin", **slot_kw)
    out = tmp_path / "o.bin"
    monkeypatch.setattr(sys, "argv", ["x", "--slot", str(tmp_path / "slot.bin"), "--banks", str(banks),
                                      "--stock", str(tmp_path / "stock.bin"), "--out", str(out)])
    slot_packer.main()
    return out


def test_good_slot_passes(tmp_path, banks, monkeypatch):
    assert frmw.parse(build(tmp_path, banks, monkeypatch).read_bytes())["ok"]


def test_packer_refuses_misaligned_vector_table(tmp_path, banks, monkeypatch):
    try:
        build(tmp_path, banks, monkeypatch, table_at=0x20)
    except SystemExit as e:
        assert "1 KiB aligned" in str(e)
    else:
        raise AssertionError("misaligned table was accepted")


def test_packer_refuses_zero_sdram_length(tmp_path, banks, monkeypatch):
    try:
        build(tmp_path, banks, monkeypatch, sdram_len=0)
    except SystemExit as e:
        assert "zero payload word" in str(e)
    else:
        raise AssertionError("zero payload word was accepted")


def test_checker_flags_zero_payload_word(tmp_path, banks, monkeypatch):
    data = bytearray(build(tmp_path, banks, monkeypatch).read_bytes())
    s0 = frmw.parse(bytes(data))["sections"][0]["file_offset"]
    struct.pack_into("<I", data, s0 + 12, 0)                       # sdram_len = 0
    data[0x0E] = sum(data[0x30:]) & 0xFF
    r = frmw.parse(bytes(data))
    assert not r["ok"] and any(c["check"] == "slot_payload_words_nonzero" and not c["ok"] for c in r["checks"])
