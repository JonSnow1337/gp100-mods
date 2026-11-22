"""Valeton .prst -> OpenGP100 presets, and the GPPR block inside the update file."""
import json
import struct
import zlib

import pytest

from gp100mods import presets as P
from gp100mods.presets import PresetError

# (module, effect name, effectCode, on, x). Codes are OpenGP100's catalog ids (the same ones the Valeton editor uses).
CHAIN = (("PRE", "C-Wah", 1, 0, 1), ("DST", "Green OD", 50331648, 1, 2), ("AMP", "", 0, 1, 3), ("NR", "", 0, 0, 4),
         ("CAB", "", 0, 1, 5), ("EQ", "", 0, 0, 6), ("MOD", "", 0, 0, 0), ("DLY", "", 0, 0, 7), ("RVB", "Room", 201326592, 1, 8))


def _codes():
    cat = P.catalog()
    return [cat["families"][i][0]["id"] for i in range(9)]


def make_prst(path, count=1, name="Test", ctrl=True, exp=True):
    codes = _codes()
    out = ['<?xml version="1.0"?>\n<GP-100>\n']
    for n in range(count):
        out.append(f'<presets ppBank="0" ppName="{name}{n}" ppVolume="40" ppID="{n}" ppBPM="100" ppType="4" ppAuthor="me" ppNotes="">\n')
        for i, (mod, _eff, _code, on, x) in enumerate(CHAIN):
            params = " ".join(f'params_{k}="{k * 5}"' for k in range(15))
            out.append(f'<Effect effectModuleName="{mod}" effectName="e" effectState="{on}" effectCode="{codes[i]}" x="{x}" y="0" {params}/>\n')
        if ctrl:
            out.append('<ppCtrl c11="1" c12="65535" c13="65535" c21="7" c22="65535" c23="65535"/>\n')
        if exp:
            out.append('<ppEXP1 expTarget="1" expVolume="0" expVolumeMin="0" expVolumeMax="99">'
                       f'<ppEXP1_0 expMId="0" expCode="{codes[0]}" expIndex="0" expMin="0" expMax="99"/>'
                       '<ppEXP1_1 expMId="65535" expCode="0" expIndex="0" expMin="0" expMax="99"/>'
                       '<ppEXP1_2 expMId="65535" expCode="0" expIndex="0" expMin="0" expMax="99"/></ppEXP1>\n')
        out.append("</presets>\n")
    out.append("</GP-100>\n")
    path.write_text("".join(out))
    return path


def test_conversion_makes_a_native_valid_record(tmp_path):
    (p,) = P.load_prst(make_prst(tmp_path / "a.prst"))
    r = p.record
    assert len(r) == P.RECORD_SIZE and r[0x308:0x30A] == b"OG"                      # OpenGP100's own format marker
    assert struct.unpack_from("<I", r, P.CRC_OFFSET)[0] == zlib.crc32(r[:P.CRC_OFFSET])
    assert r[2:7] == b"Test0"
    count = r[P.CHAIN_HEADER + 1]
    assert 1 <= count <= P.MAX_BLOCKS
    assert struct.unpack_from("<H", r, 0x272)[0] == 100 and struct.unpack_from("<H", r, 0x274)[0] == 40
    assert r[P.EXP_START] == 0xA1                                                   # expression pedal linked to the wah
    assert r[0x2C2:0x2C4] == b"me"


def test_switched_off_blocks_without_a_footswitch_are_left_out(tmp_path):
    (p,) = P.load_prst(make_prst(tmp_path / "a.prst", ctrl=False, exp=False))
    families = {p.record[P.BLOCK_START + i * P.BLOCK_SIZE] - 1 for i in range(p.record[P.CHAIN_HEADER + 1])}
    assert families == {1, 2, 4, 8}                                                 # DST, AMP, CAB, RVB are on; the rest off
    (q,) = P.load_prst(make_prst(tmp_path / "b.prst"))
    fams = {q.record[P.BLOCK_START + i * P.BLOCK_SIZE] - 1 for i in range(q.record[P.CHAIN_HEADER + 1])}
    assert 7 in fams and 0 in fams                                                  # delay (FS2) and wah (expression) stay


def test_order_follows_x_and_slots_follow_id(tmp_path):
    ps = P.load_prst(make_prst(tmp_path / "a.prst", count=3))
    assert [p.name for p in ps] == ["Test0", "Test1", "Test2"]


def test_block_roundtrip_and_limits(tmp_path):
    ps = P.load_prst(make_prst(tmp_path / "a.prst", count=2))
    block = P.build_block([ps[0].record, None, ps[1].record])
    got = P.parse_block(block)
    assert got == [ps[0].record, None, ps[1].record]
    with pytest.raises(PresetError):
        P.build_block([ps[0].record] * 100)
    damaged = bytearray(block); damaged[P.HEADER_BYTES + 10] ^= 1
    assert P.parse_block(bytes(damaged))[0] is None                                  # a damaged record is skipped, as in the firmware


def test_bad_files_are_refused(tmp_path):
    (tmp_path / "x.prst").write_text("<GP-100></GP-100>")
    with pytest.raises(PresetError):
        P.load_prst(tmp_path / "x.prst")
    (tmp_path / "y.prst").write_text("garbage")
    with pytest.raises(PresetError):
        P.load_prst(tmp_path / "y.prst")


def test_presets_go_into_the_update_file(update_file, tmp_path):
    from gp100mods import update_file as U
    ps = P.load_prst(make_prst(tmp_path / "a.prst", count=2))
    s0 = U.section0_of(update_file.read_bytes())
    from gp100mods import frmw, valeton
    data = update_file.read_bytes()
    r = frmw.parse(data)
    parts = valeton.Parts(data[:frmw.HEADER_SIZE], tuple(data[x["file_offset"]:x["file_offset"] + x["size"]] for x in r["sections"][1:4]))
    out = U.compose(s0, parts, None, presets=[p.record for p in ps])
    path = tmp_path / "o.bin"; path.write_bytes(out)
    info = U.inspect(path)
    assert info.ok and info.presets == 2
    sec = U.section0_of(out)
    block = sec[U.AREA_REL + 0x1000:]                                               # right after the (4 KiB padded) empty animation
    assert block[:4] == b"GPPR" and P.parse_block(block) == [p.record for p in ps]
