import hashlib

import pytest

from gp100mods import valeton as V


def test_real_profile_constants_are_well_formed():
    p = V.V21
    for h in (p.sha256, p.header_sha256, *p.section_sha256):
        assert len(h) == 64 and int(h, 16) >= 0
    assert p.size == 3424316 and p.section_sizes == (16384, 165120, 491520)


def test_import_and_load_roundtrip(official, tmp_path):
    path, prof = official
    d = tmp_path / "cache"
    assert V.load(d, prof) is None                                   # nothing yet
    parts = V.import_official(path, d, prof)
    again = V.load(d, prof)
    assert again.header == parts.header and again.sections == parts.sections
    V.forget(d, prof)
    assert V.load(d, prof) is None


@pytest.mark.parametrize("damage", ["flip", "truncate", "append"])
def test_other_files_are_refused(official, tmp_path, damage):
    path, prof = official
    data = bytearray(path.read_bytes())
    if damage == "flip":
        data[0x1234] ^= 0x01                                         # a modified official file
    elif damage == "truncate":
        data = data[:-100]
    else:
        data += b"\x00"
    bad = tmp_path / "bad.bin"; bad.write_bytes(bytes(data))
    with pytest.raises(V.NotOfficialError):
        V.import_official(bad, tmp_path / "c", prof)
    assert V.load(tmp_path / "c", prof) is None


def test_non_firmware_is_refused(official, tmp_path):
    _, prof = official
    junk = tmp_path / "x.gif"; junk.write_bytes(b"GIF89a" + bytes(100))
    with pytest.raises(V.NotOfficialError):
        V.import_official(junk, tmp_path / "c", prof)


def test_other_version_of_same_product_is_refused(official, tmp_path):
    """Same container layout, different content (e.g. another firmware version) must not pass."""
    path, prof = official
    data = bytearray(path.read_bytes()); data[-5] ^= 0xFF; data[0x0E] = sum(data[0x30:]) & 0xFF   # still a valid container
    other = tmp_path / "other.bin"; other.write_bytes(bytes(data))
    with pytest.raises(V.NotOfficialError):
        V.import_official(other, tmp_path / "c", prof)


def test_damaged_cache_is_not_trusted(official, tmp_path):
    path, prof = official
    d = tmp_path / "cache"
    V.import_official(path, d, prof)
    cache = next(d.glob("*.fw"))
    blob = bytearray(cache.read_bytes()); blob[100] ^= 0xFF; cache.write_bytes(bytes(blob))
    assert V.load(d, prof) is None


def test_real_v21_hash_is_not_confused_with_synthetic(official, tmp_path):
    path, _ = official                                               # a synthetic file is not the real V2.1
    with pytest.raises(V.NotOfficialError):
        V.import_official(path, tmp_path / "c")                       # default profile = the real V2.1
