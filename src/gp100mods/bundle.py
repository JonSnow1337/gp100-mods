"""Firmware versions that ship inside the app.

Each version is only **section 0 of an update file** - the OpenGP100 application (plus its empty boot-animation area).
The rest of an update file is Valeton's data, which is never shipped: the app combines a bundled section 0 with the parts
of the user's own official V2.1 file (see valeton.py). Versions live in `data/firmware/` next to a `manifest.json`:

    {"firmwares": [{
        "id": "opengp100-4", "label": "OpenGP100 build 4", "version": "4", "file": "opengp100-4.sec0",
        "sha256": "...", "notes": "what changed",
        "features": [{"id": "x", "label": "Some switch", "description": "...", "default": false,
                      "patches": [{"offset": "0x1234", "old": "00", "new": "01"}]}]}]}

`offset` is relative to the start of update-file section 0 (= flash 0x80000). A switch only applies if the bytes there are
exactly `old` - a different firmware build can never be patched by accident. Add versions with
`python -m gp100mods firmware-add`.
"""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass, field
from pathlib import Path

from . import update_file

DATA_DIR = Path(__file__).resolve().parent / "data" / "firmware"


@dataclass
class Feature:
    id: str
    label: str
    description: str = ""
    default: bool = False
    patches: list = field(default_factory=list)       # [{"offset": int, "old": bytes, "new": bytes}]


@dataclass
class Fill:
    """A region of section 0 that holds Valeton's data (drum audio, patterns). The bundle stores it blank; the app copies it
    from the user's verified official file, applies the few bytes that differ (`patch`) and checks the result's SHA-256."""
    name: str
    dest: int            # offset in the bundled section 0
    source: int          # offset in the official file's section 0
    length: int
    sha256: str          # of the finished region
    patch: list = field(default_factory=list)      # [(offset in region, bytes)]


# V2.1 regions that OpenGP100 copies from Valeton's firmware: (name, offset in section 0 of the OpenGP100 build,
# offset in section 0 of the official V2.1 file, length). Measured: the drum samples are an exact copy; the pattern table
# differs in a few bytes (his own edits), which travel as `patch`.
REGIONS_V21 = (("drum samples", 0xB3DDC, 0x16874D, 0x935E4),
               ("drum patterns", 0x147414, 0x1FBD85, 0x447E))


@dataclass
class Firmware:
    id: str
    label: str
    version: str
    path: Path
    sha256: str
    notes: str = ""
    features: list = field(default_factory=list)
    fills: list = field(default_factory=list)

    def load(self) -> bytes:
        """Section 0, verified against the manifest hash."""
        data = self.path.read_bytes()
        if hashlib.sha256(data).hexdigest() != self.sha256:
            raise ValueError(f"{self.path.name} is damaged (checksum mismatch). Reinstall the app.")
        return data


def _hex(v) -> int:
    return int(v, 16) if isinstance(v, str) else int(v)


def load_manifest(directory: Path | None = None) -> list[Firmware]:
    d = Path(directory) if directory else DATA_DIR
    mf = d / "manifest.json"
    if not mf.is_file():
        return []
    out = []
    for e in json.loads(mf.read_text())["firmwares"]:
        feats = [Feature(f["id"], f["label"], f.get("description", ""), bool(f.get("default", False)),
                         [{"offset": _hex(p["offset"]), "old": bytes.fromhex(p["old"]), "new": bytes.fromhex(p["new"])}
                          for p in f["patches"]]) for f in e.get("features", [])]
        fills = [Fill(f["name"], _hex(f["dest"]), _hex(f["source"]), int(f["length"]), f["sha256"],
                      [(_hex(p["offset"]), bytes.fromhex(p["bytes"])) for p in f.get("patch", [])]) for f in e.get("fills", [])]
        out.append(Firmware(e["id"], e["label"], str(e["version"]), d / e["file"], e["sha256"], e.get("notes", ""), feats, fills))
    return out


def latest(firmwares: list[Firmware]) -> Firmware | None:
    return firmwares[-1] if firmwares else None          # manifest order = oldest -> newest


def make_fills(sec0: bytes, official_sec0: bytes, regions=REGIONS_V21):
    """Blank the Valeton-derived regions of `sec0`. -> (stripped section 0, [Fill-as-dict])."""
    out, fills = bytearray(sec0), []
    for name, dest, source, length in regions:
        built, src = sec0[dest:dest + length], official_sec0[source:source + length]
        if len(built) != length or len(src) != length:
            raise ValueError(f"{name}: region lies outside the firmware")
        runs, i = [], 0
        while i < length:
            if built[i] != src[i]:
                j = i
                while j < length and built[j] != src[j]:
                    j += 1
                runs.append((i, built[i:j]))
                i = j
            else:
                i += 1
        if sum(len(b) for _, b in runs) > length // 20:
            raise ValueError(f"{name}: differs from Valeton's data in too many bytes - wrong official file?")
        out[dest:dest + length] = bytes(length)
        fills.append({"name": name, "dest": hex(dest), "source": hex(source), "length": length,
                      "sha256": hashlib.sha256(built).hexdigest(),
                      "patch": [{"offset": hex(o), "bytes": b.hex()} for o, b in runs]})
    return bytes(out), fills


def add_firmware(src, *, label, version, notes="", directory: Path | None = None, official=None, regions=REGIONS_V21,
                 fills_spec=None) -> Firmware:
    """Developer tool: take a complete, animation-capable update file, keep only its section 0 and register it.
    With `official` (the official Valeton V2.1 file) the Valeton-derived regions are blanked and recorded as fills, so the
    bundle holds no Valeton data. Without it the section 0 is stored as is (private use only)."""
    d = Path(directory) if directory else DATA_DIR
    info = update_file.inspect(src)
    if not (info.ok and info.supports_animation):
        raise ValueError(f"{src}: {info.message}")
    d.mkdir(parents=True, exist_ok=True)
    sec0 = update_file.section0_of(Path(src).read_bytes())
    # Everything behind the (empty) boot-animation header is erased flash (0xFF): compose() pads it back, so do not store it.
    keep = update_file.AREA_REL + update_file.F.HEADER_BYTES
    if sec0[keep:].count(0xFF) == len(sec0) - keep:
        sec0 = sec0[:keep]
    fills = []
    if fills_spec is not None:
        # A build from the repository source has placeholder audio, so the recipe cannot be derived by comparison: use the stored one.
        sec0 = bytearray(sec0)
        for f in fills_spec:
            dest, length = _hex(f["dest"]), int(f["length"])
            if dest + length > len(sec0):
                raise ValueError(f"{f['name']}: region lies outside the firmware")
            sec0[dest:dest + length] = bytes(length)
        sec0, fills = bytes(sec0), list(fills_spec)
    elif official is not None:
        odata = Path(official).read_bytes()
        from . import valeton
        valeton.extract(odata)                                  # must be exactly the official V2.1 file
        sec0, fills = make_fills(sec0, update_file.section0_of(odata), regions)
    digest = hashlib.sha256(sec0).hexdigest()
    fid = f"{''.join(c if c.isalnum() else '-' for c in label.lower()).strip('-')}-{version}"
    name = f"{fid}.sec0"
    (d / name).write_bytes(sec0)
    mf = d / "manifest.json"
    manifest = json.loads(mf.read_text()) if mf.is_file() else {"firmwares": []}
    manifest["firmwares"] = [e for e in manifest["firmwares"] if e["id"] != fid]
    manifest["firmwares"].append({"id": fid, "label": label, "version": str(version), "file": name,
                                  "sha256": digest, "notes": notes, "features": [], "fills": fills})
    mf.write_text(json.dumps(manifest, indent=2) + "\n")
    return [f for f in load_manifest(d) if f.id == fid][0]


def main():
    import argparse
    ap = argparse.ArgumentParser(description="Add a firmware version to the app bundle (developers).")
    ap.add_argument("file", type=Path, help="complete animation-capable OpenGP100 update file (only its section 0 is kept)")
    ap.add_argument("--label", required=True, help='name shown in the app, e.g. "OpenGP100"')
    ap.add_argument("--version", required=True)
    ap.add_argument("--notes", default="")
    ap.add_argument("--dir", type=Path, help="bundle directory (default: the package's data/firmware)")
    ap.add_argument("--official", type=Path, help="the official Valeton GP-100 Firmware V2.1 file: its data is blanked in the bundle "
                                                  "and restored from the user's own copy at generation time")
    ap.add_argument("--fills", type=Path, help="recipe file (firmware/opengp100/valeton_fills.json) for builds that used placeholder audio")
    ap.add_argument("--no-strip", action="store_true", help="keep Valeton's data inside the bundle (private use only)")
    a = ap.parse_args()
    if not (a.official or a.fills or a.no_strip):
        ap.error("--official or --fills is required (or --no-strip to keep Valeton's data in the bundle)")
    spec = json.loads(a.fills.read_text())["fills"] if a.fills else None
    fw = add_firmware(a.file, label=a.label, version=a.version, notes=a.notes, directory=a.dir,
                      official=None if spec else a.official, fills_spec=spec)
    print(f"added {fw.id}  sha256 {fw.sha256[:16]}...  -> {fw.path}  ({len(fw.fills)} Valeton region(s) blanked)")
    print("Edit manifest.json to declare option switches (see gp100mods/bundle.py).")


if __name__ == "__main__":
    main()
