"""The boot-path bugs found on the real pedal, and the sanity checks of opengp100.finish."""
import pytest

from conftest import make_opengp_update
from gp100mods import features, frmw, update_file as U
from gp100mods.bootanim import format as F
from gp100mods.opengp100 import finish


def run(tmp_path, **kw):
    raw = tmp_path / "raw.bin"; make_opengp_update(raw, **kw)
    return finish.finish(raw.read_bytes())


def test_good_build_passes_and_gets_the_reserved_areas(tmp_path):
    out = run(tmp_path)
    assert frmw.parse(out)["ok"]
    sec0 = U.section0_of(out)
    assert features.read(sec0) == 0                                     # flags block present, everything off
    assert sec0[U.AREA_REL:U.AREA_REL + 4] == F.MAGIC                   # empty boot-animation area


def test_only_the_reserved_areas_change(tmp_path):
    raw = tmp_path / "raw.bin"; make_opengp_update(raw)
    a, b = raw.read_bytes(), finish.finish(raw.read_bytes())
    assert len(a) == len(b)
    sa, sb = U.section0_of(a), U.section0_of(b)
    changed = [i for i in range(len(sa)) if sa[i] != sb[i]]
    assert features.REL <= changed[0] and changed[-1] < U.AREA_REL + F.HEADER_BYTES
    assert U._sections(a)[1][1:] == U._sections(b)[1][1:] and a[0x30 + 8 * 4 + 0x20 + 0x29FAEC:] == b[0x30 + 8 * 4 + 0x20 + 0x29FAEC:]


def test_refuses_zero_payload_word(tmp_path):
    with pytest.raises(ValueError, match="zero payload word"):
        run(tmp_path, zero_word=True)


def test_refuses_a_non_thumb_entry(tmp_path):
    with pytest.raises(ValueError):                  # caught by the generic firmware checks and by finish() alike
        run(tmp_path, entry=0x8)


def test_refuses_a_build_without_factory_preset_support(tmp_path):
    with pytest.raises(ValueError, match="factory-presets"):
        run(tmp_path, marker=False)


@pytest.mark.parametrize("where", [features.REL + 3, U.AREA_REL + 0x1000])
def test_refuses_a_build_that_uses_the_reserved_areas(tmp_path, where):
    with pytest.raises(ValueError, match="already uses"):
        run(tmp_path, used=where)


def test_refuses_the_stock_update(tmp_path):
    from conftest import make_stock
    make_stock(tmp_path / "s.bin")
    with pytest.raises(ValueError):
        finish.finish((tmp_path / "s.bin").read_bytes())
