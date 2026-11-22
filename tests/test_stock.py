"""Patches for the original Valeton V2.1 firmware. The exact-output tests need the official file: set GP100_V21_FILE to it
(they are skipped otherwise). Hashes are of the patched OUTPUT and contain no Valeton data."""
import hashlib
import os

import pytest
from PIL import Image

from gp100mods import frmw, valeton
from gp100mods.stock import about_text, boot_gif, master_volume, wah_in_mod

V21_FILE = os.environ.get("GP100_V21_FILE")
needs_official = pytest.mark.skipif(not (V21_FILE and os.path.isfile(V21_FILE or "")), reason="GP100_V21_FILE not set")

EXPECTED = {
    "wah": "dcb460e2e33853d2fe9dad5d84de42431ab84152de2b6cba895771ca48f578d6",
    "volume": "1b9aa4d6776225d6da43ff85e3eb909d6bd84df0b36ea4e4450f69ad49775b13",
    "about": "83e6a05147baf1184e83e10e0fec02c9dc6c4551aaffccdc76cb4dea93a8898f",
}


def sha(b):
    return hashlib.sha256(b).hexdigest()


@pytest.fixture(scope="module")
def official():
    data = open(V21_FILE, "rb").read()
    valeton.extract(data)                                   # must be exactly the real V2.1
    return data


@needs_official
def test_exact_outputs(official):
    assert sha(wah_in_mod.apply(official)) == EXPECTED["wah"]
    assert sha(master_volume.apply(official)) == EXPECTED["volume"]
    assert sha(about_text.apply(official, "Line one", "Line two", "Line three")) == EXPECTED["about"]


@needs_official
def test_patches_chain_and_stay_valid(official, tmp_path):
    gif = tmp_path / "a.gif"
    ims = [Image.new("RGB", (80, 60), (40 * i, 255 - 30 * i, 90)) for i in range(5)]
    ims[0].save(gif, save_all=True, append_images=ims[1:], duration=80, loop=0)
    d = wah_in_mod.apply(official)
    d, _, info = boot_gif.apply(d, gif)
    d = about_text.apply(d, "A", "B", "C")
    d = master_volume.apply(d)
    assert frmw.parse(d)["ok"] and info["frames"] == 5 and info["bytes"] <= info["capacity"]


@needs_official
def test_about_text_validation(official):
    with pytest.raises(ValueError, match="do not fit"):
        about_text.apply(official, "x" * (about_text.LIMITS["line1"] + 1), "b", "c")
    with pytest.raises(ValueError, match="plain letters"):
        about_text.apply(official, "café", "b", "c")


@needs_official
def test_second_application_is_refused(official):
    once = master_volume.apply(official)
    with pytest.raises(ValueError):
        master_volume.apply(once)                           # not the stock bytes any more


@pytest.mark.parametrize("blob", [b"FRMW" + bytes(1000), b"", bytes(5000)])
def test_foreign_input_is_refused_cleanly(blob, tmp_path):
    gif = tmp_path / "a.gif"; Image.new("RGB", (8, 8)).save(gif)
    for fn in (wah_in_mod.apply, master_volume.apply, lambda b: about_text.apply(b, "a", "b", "c"),
               lambda b: boot_gif.apply(b, gif)):
        with pytest.raises(ValueError):
            fn(blob)


@needs_official
def test_bundled_dogboot_matches_the_flashed_build(official):
    """wah + the bundled default animation == the build that was flashed and confirmed on a real pedal."""
    from gp100mods.defaults import dogboot_bytes
    out = boot_gif.install(wah_in_mod.apply(official), dogboot_bytes())
    assert sha(out) == "573e877344fd2417a4b195a2f397b0440b92638d5f388555921db4fde57d90ae"
