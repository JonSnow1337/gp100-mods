import json

import pytest

from gp100mods import bundle, frmw, update_file as U, valeton
from gp100mods.bootanim import format as F

PIX = F.WIDTH * F.HEIGHT


def sample_anim():
    return F.Animation(palette=[0, 0xFFFF], frames=[(bytes([1]) * PIX, 40)])


def _patch(offset, old, new):
    return {"offset": offset, "old": bytes.fromhex(old), "new": bytes.fromhex(new)}


@pytest.fixture
def parts(official, tmp_path):
    path, prof = official
    return valeton.import_official(path, tmp_path / "vc", prof)


@pytest.fixture
def store(tmp_path, update_file):
    d = tmp_path / "bundle"
    fw = bundle.add_firmware(update_file, label="Test FW", version="1", notes="n", directory=d)
    return d, fw


def test_add_and_load(store, update_file):
    d, fw = store
    loaded = bundle.load_manifest(d)
    assert [f.id for f in loaded] == [fw.id] and loaded[0].load() == fw.path.read_bytes()
    assert bundle.latest(loaded).id == fw.id
    assert loaded[0].load() == U.section0_of(update_file.read_bytes())          # only section 0 is stored


def test_bundle_holds_no_valeton_sections(store, update_file):
    d, fw = store
    stored = fw.path.read_bytes()
    data = update_file.read_bytes()
    r, secs = U._sections(data)
    for i in (1, 3):                                                          # sections copied from the official file never end up in the bundle
        assert data[secs[i][0]:secs[i][0] + 4096] not in stored


def test_add_refuses_stock_style_files(tmp_path):
    from conftest import make_stock
    make_stock(tmp_path / "stock.bin")
    with pytest.raises(ValueError):
        bundle.add_firmware(tmp_path / "stock.bin", label="x", version="1", directory=tmp_path / "b")


def test_damaged_firmware_is_detected(store):
    d, fw = store
    data = bytearray(fw.path.read_bytes()); data[5000] ^= 0xFF
    fw.path.write_bytes(data)
    with pytest.raises(ValueError, match="damaged"):
        bundle.load_manifest(d)[0].load()


def test_empty_bundle(tmp_path):
    assert bundle.load_manifest(tmp_path / "nothing") == []


def test_composed_file_is_a_valid_update_with_valeton_parts(store, parts, official):
    d, fw = store
    out = U.compose(fw.load(), parts)
    r = frmw.parse(out)
    assert r["ok"]
    stock = official[0].read_bytes()
    rs = frmw.parse(stock)["sections"]
    for i in (1, 2, 3):                                                       # sections 1-3 come from the user's file
        assert out[r["sections"][i]["file_offset"]:r["sections"][i]["file_end"]] == stock[rs[i]["file_offset"]:rs[i]["file_end"]]


def test_switch_applies_and_is_guarded(store, parts):
    d, fw = store
    sec0 = fw.load()
    old = sec0[0x1000:0x1002].hex()
    out = U.compose(sec0, parts, None, [_patch(0x1000, old, "abcd")])
    assert U.section0_of(out)[0x1000:0x1002] == bytes.fromhex("abcd") and frmw.parse(out)["ok"]
    out2 = U.compose(U.section0_of(out), parts, None, [_patch(0x1000, old, "abcd")])     # already applied: idempotent
    assert out2 == out
    with pytest.raises(ValueError, match="does not match"):                     # wrong build: refused
        U.compose(sec0, parts, None, [_patch(0x1000, "0000" if old != "0000" else "1111", "abcd")])
    with pytest.raises(ValueError, match="animation area"):                      # may not clobber the animation area
        U.compose(sec0, parts, None, [_patch(U.AREA_REL, "47", "00")])


def test_switch_and_animation_together(store, parts):
    d, fw = store
    sec0 = fw.load()
    out = U.compose(sec0, parts, sample_anim(), [_patch(0x1000, sec0[0x1000:0x1001].hex(), "5a")])
    s0 = U.section0_of(out)
    assert s0[0x1000] == 0x5A
    assert len(F.parse_area(s0[U.AREA_REL:]).frames) == 1


def test_manifest_features_parse(tmp_path, update_file):
    d = tmp_path / "b"
    bundle.add_firmware(update_file, label="T", version="2", directory=d)
    m = json.loads((d / "manifest.json").read_text())
    m["firmwares"][0]["features"] = [{"id": "s", "label": "Switch", "default": True,
                                      "patches": [{"offset": "0x10", "old": "00", "new": "01"}]}]
    (d / "manifest.json").write_text(json.dumps(m))
    f = bundle.load_manifest(d)[0].features[0]
    assert f.label == "Switch" and f.default and f.patches[0] == {"offset": 0x10, "old": b"\x00", "new": b"\x01"}


@pytest.mark.skipif(not (bundle.DATA_DIR / "manifest.json").is_file(), reason="no bundled firmware in this checkout")
def test_real_bundle_contains_no_valeton_data():
    """Whatever is bundled must not contain any of Valeton's official sections."""
    for fw in bundle.load_manifest():
        data = fw.load()
        import hashlib
        assert hashlib.sha256(data).hexdigest() not in valeton.V21.section_sha256
        for n, h in zip(valeton.V21.section_sizes, valeton.V21.section_sha256):
            assert n != len(data) or hashlib.sha256(data).hexdigest() != h


def test_flags_are_written_and_guarded(store, parts):
    from gp100mods import features
    d, fw = store
    sec0 = fw.load()
    assert features.read(sec0) == 0                                          # the packer reserved an empty block
    out = U.compose(sec0, parts, None, flags=features.STARTER_PRESET)
    assert features.read(U.section0_of(out)) == features.STARTER_PRESET
    with pytest.raises(ValueError, match="feature-flags block"):             # patches may not clobber it
        U.compose(sec0, parts, None, [_patch(features.REL, "47", "00")])
    legacy = bytearray(sec0); legacy[features.REL:features.REL + features.BLOCK_BYTES] = b"\xff" * features.BLOCK_BYTES
    with pytest.raises(ValueError, match="no feature switches"):
        U.compose(bytes(legacy), parts, None, flags=1)
