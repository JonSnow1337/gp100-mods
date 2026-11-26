import pytest

from gp100mods import frmw, update_file as U
from gp100mods.bootanim import format as F

PIX = F.WIDTH * F.HEIGHT


def sample():
    return F.Animation(palette=[0, 0xFFFF], frames=[(bytes([1]) * PIX, 40), (bytes([0]) * PIX, 80)])


def test_packer_output_passes_checks_and_has_empty_area(update_file):
    info = U.inspect(update_file)
    assert info.ok and info.supports_animation and not info.has_custom_animation
    assert frmw.parse(update_file.read_bytes())["ok"]


def test_write_and_read_back(update_file, tmp_path):
    out = tmp_path / "out.bin"
    U.write_animation(update_file, out, sample())
    info = U.inspect(out)
    assert info.ok and info.has_custom_animation and info.frames == 2 and info.total_ms == 120
    assert frmw.parse(out.read_bytes())["ok"]


def test_only_section0_tail_changes(update_file, tmp_path):
    out = tmp_path / "out.bin"
    U.write_animation(update_file, out, sample())
    a, b = frmw.parse(update_file.read_bytes()), frmw.parse(out.read_bytes())
    da, db = update_file.read_bytes(), out.read_bytes()
    for i in (1, 2, 3):                                   # sections 1..3 are byte-identical
        sa, sb = a["sections"][i], b["sections"][i]
        assert da[sa["file_offset"]:sa["file_end"]] == db[sb["file_offset"]:sb["file_end"]]
    s0a, s0b = a["sections"][0], b["sections"][0]
    assert da[s0a["file_offset"]:s0a["file_offset"] + U.AREA_REL] == db[s0b["file_offset"]:s0b["file_offset"] + U.AREA_REL]


def test_reset_restores_default(update_file, tmp_path):
    a, b = tmp_path / "a.bin", tmp_path / "b.bin"
    U.write_animation(update_file, a, sample())
    U.write_animation(a, b, None)
    assert not U.inspect(b).has_custom_animation and U.inspect(b).ok


def test_never_overwrites_input(update_file):
    with pytest.raises(ValueError):
        U.write_animation(update_file, update_file, sample())


def test_rejects_non_firmware(tmp_path):
    p = tmp_path / "x.gif"; p.write_bytes(b"GIF89a" + bytes(500))
    assert not U.inspect(p).ok


def test_update_keeps_the_stock_section_sizes(update_file, tmp_path):
    """Like OpenGP100's own update file (and Valeton's), section 0 is 0x29FAEC bytes with erased flash behind the data."""
    out = tmp_path / "o.bin"
    U.write_animation(update_file, out, sample())
    data = out.read_bytes()
    assert [s for _, s in U._sections(data)[1]] == [0x29FAEC, 0x4000, 0x28500, 0x78000]
    sec0 = U.section0_of(data)
    assert set(sec0[-0x1000:]) == {0xFF}
