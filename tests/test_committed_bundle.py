"""The committed OpenGP100 build must never contain Valeton's data: its fill regions have to be blank."""
import hashlib
import json
from pathlib import Path

FW = Path(__file__).resolve().parent.parent / "src" / "gp100mods" / "data" / "firmware"


def test_committed_sec0_files_are_blanked_and_match_manifest():
    manifest = json.loads((FW / "manifest.json").read_text())
    assert len(manifest["firmwares"]) == 1, "only the current OpenGP100 build is kept"
    on_disk = {p.name for p in FW.glob("*.sec0")}
    assert on_disk == {f["file"] for f in manifest["firmwares"]}
    for f in manifest["firmwares"]:
        data = (FW / f["file"]).read_bytes()
        assert hashlib.sha256(data).hexdigest() == f["sha256"]
        assert f["fills"], "no fills recorded: build was not blanked"
        for fill in f["fills"]:
            d = int(fill["dest"], 16)
            assert data[d:d + fill["length"]] == bytes(fill["length"]), f"{fill['name']} is not blank"
