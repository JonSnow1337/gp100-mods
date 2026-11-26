"""Valeton-derived regions are blanked in the bundle and restored from the user's official file, with a hash check."""
import hashlib

import pytest

from gp100mods import bundle, update_file as U, valeton
from gp100mods.bundle import make_fills


def region_spec(name, dest, source, length, region):
    return {"name": name, "dest": hex(dest), "source": hex(source), "length": length,
            "sha256": hashlib.sha256(region).hexdigest(), "patch": []}


def test_make_fills_blanks_and_records_differences():
    official = bytes(range(256)) * 16
    built = bytearray(official[100:300]); built[10] ^= 0xFF; built[11] ^= 0xFF; built[150] ^= 0x55     # his own edits
    sec0 = bytearray(b"\xaa" * 1000); sec0[200:400] = built
    stripped, fills = make_fills(bytes(sec0), official, regions=(("data", 200, 100, 200),))
    assert stripped[200:400] == bytes(200) and stripped[:200] == sec0[:200] and stripped[400:] == sec0[400:]
    f = fills[0]
    assert f["sha256"] == hashlib.sha256(bytes(built)).hexdigest()
    assert [(p["offset"], p["bytes"]) for p in f["patch"]] == [(hex(10), (built[10:12]).hex()), (hex(150), built[150:151].hex())]


def test_make_fills_refuses_data_that_is_not_from_this_official_file():
    with pytest.raises(ValueError, match="too many bytes"):
        make_fills(b"\x01" * 1000, b"\x02" * 1000, regions=(("data", 0, 0, 500),))


@pytest.fixture
def stripped_bundle(tmp_path, update_file, official):
    """A bundled firmware whose first 4 KiB of the cabinet-bank-like filler region is Valeton data restored at generation."""
    path, prof = official
    parts = valeton.import_official(path, tmp_path / "vc", prof)
    sec0 = bytearray(U.section0_of(update_file.read_bytes()))
    valeton_region = parts.section0[:512]                                   # Valeton's data, from the official file
    sec0[0x2000:0x2200] = valeton_region                                    # as if the build had copied it
    spec = [region_spec("test region", 0x2000, 0, 512, valeton_region)]
    data = bytearray(update_file.read_bytes()); o = U._sections(bytes(data))[1][0]
    full = tmp_path / "full.bin"
    full.write_bytes(U.compose(bytes(sec0), parts))
    fw = bundle.add_firmware(full, label="T", version="9", directory=tmp_path / "b", fills_spec=spec)
    return fw, parts, valeton_region


def test_bundle_holds_no_valeton_region_and_restores_it(stripped_bundle):
    fw, parts, region = stripped_bundle
    assert fw.load()[0x2000:0x2200] == bytes(512)                           # blank in the stored bundle
    out = U.compose(fw.load(), parts, fills=fw.fills)
    assert U.section0_of(out)[0x2000:0x2200] == region                      # restored from the official file


def test_wrong_official_content_is_refused(stripped_bundle):
    fw, parts, _ = stripped_bundle
    damaged = bytearray(parts.file); sec = valeton.frmw.parse(bytes(damaged))["sections"][0]
    damaged[sec["file_offset"] + 5] ^= 0xFF                                  # inside the region the fill copies
    bad = valeton.Parts(parts.header, parts.sections, bytes(damaged))
    with pytest.raises(ValueError, match="does not match"):
        U.compose(fw.load(), bad, fills=fw.fills)


def test_fills_need_the_official_file(stripped_bundle):
    fw, parts, _ = stripped_bundle
    with pytest.raises(ValueError, match="official Valeton firmware is needed"):
        U.compose(fw.load(), valeton.Parts(parts.header, parts.sections), fills=fw.fills)


def test_placeholder_assets_have_the_sizes_the_packer_needs(tmp_path):
    import importlib.util
    from pathlib import Path
    from gp100mods.opengp100 import build
    pack = Path(__file__).resolve().parents[1] / "firmware/opengp100/sources/tools/pack_factory_banks.py"
    if not pack.is_file():
        pytest.skip("vendored OpenGP100 source not present")
    import sys
    sys.path.insert(0, str(pack.parent))
    spec = importlib.util.spec_from_file_location("pack_factory_banks", pack)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    build.ensure_placeholder_assets(tmp_path)
    assert len(m.pack_drums(tmp_path / "assets" / "rhythm")) == 0x935E4         # exactly the real bank size
    assert (tmp_path / "assets/rhythm/patterns.c").read_text().count("    P,\n") == 100        # the packer expects exactly 100 patterns
    assert len(list((tmp_path / "assets" / "cabinets").glob("*.wav"))) == 40
    assert len(build.DRUM_FRAMES) == 13
