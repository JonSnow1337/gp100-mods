"""The drum kit (WAV files -> the sample bank) and its use in the firmware build."""
import hashlib
import struct
import wave

import pytest

from gp100mods import bundle, drums, update_file as U

# the sample bank that was parsed by OpenGP100's own C parser (see docs/drums.md); set after the kit was last regenerated
REFERENCE_SAMPLES = "38aa1d0688301d52c1f87923599a6c23139efd793a8eb84f4e3f11981b6f9d96"


@pytest.fixture(scope="module")
def kit():
    return drums.load_kit()


def test_shipped_kit_packs_to_the_reference_bank(kit):
    assert hashlib.sha256(kit.samples).hexdigest() == REFERENCE_SAMPLES
    assert len(kit.samples) == drums.SAMPLE_BANK_SIZE


def _wav(path, frames, rate=44100, width=2, channels=1):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(channels); w.setsampwidth(width); w.setframerate(rate); w.writeframes(bytes(frames * width * channels))


@pytest.mark.parametrize("kw", [dict(rate=48000), dict(channels=2), dict(width=3)])
def test_wrong_wav_format_is_refused(tmp_path, kw):
    _wav(tmp_path / "a.wav", 1000, **kw)
    with pytest.raises(drums.KitError, match="mono, 16-bit"):
        drums.read_wav(tmp_path / "a.wav")


def test_kit_size_limits():
    with pytest.raises(drums.KitError, match="exactly 13"):
        drums.pack_samples([bytes(200)] * 12)
    with pytest.raises(drums.KitError, match="bytes too big"):
        drums.pack_samples([bytes(2 * 30000)] * 13)
    with pytest.raises(drums.KitError, match="too short"):
        drums.pack_samples([bytes(20)] + [bytes(200)] * 12)


def test_verify_rejects_banks_the_firmware_would_reject(kit):
    drums.verify(kit.samples)
    for offset, value in ((0, 12), (4, 1), (4 * 14, 0)):
        bad = bytearray(kit.samples)
        struct.pack_into("<I", bad, offset, value)
        with pytest.raises(drums.KitError):
            drums.verify(bytes(bad))


def _fw_with_drum_fills(update_file):
    """The synthetic firmware keeps its banks at the packer's offsets; describe them the way the real bundle does."""
    return [bundle.Fill("drum samples", 0x133DDC - 0x80000, 0, drums.SAMPLE_BANK_SIZE, "x")]


def test_compose_writes_our_samples_and_skips_the_valeton_sample_fill(update_file, official, tmp_path, kit):
    from gp100mods import valeton
    path, prof = official
    parts = valeton.import_official(path, tmp_path / "vc", prof)
    sec0 = U.section0_of(update_file.read_bytes())
    fills = _fw_with_drum_fills(update_file)
    out = U.compose(sec0, parts, fills=fills, drums=kit)                  # no official drum sample data is needed
    s = U.section0_of(out)
    assert s[fills[0].dest:fills[0].dest + drums.SAMPLE_BANK_SIZE] == kit.samples
    from gp100mods import frmw
    assert frmw.parse(out)["ok"]


def test_compose_refuses_a_firmware_without_drum_regions(update_file, official, tmp_path, kit):
    from gp100mods import valeton
    path, prof = official
    parts = valeton.import_official(path, tmp_path / "vc", prof)
    with pytest.raises(ValueError, match="cannot take a custom drum kit"):
        U.compose(U.section0_of(update_file.read_bytes()), parts, drums=kit)
