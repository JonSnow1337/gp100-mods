"""The user's own copy of Valeton's official firmware.

Update files for the GP-100 app contain four sections. Section 0 is the OpenGP100 application (shipped in the app);
the container header and sections 1-3 are Valeton's data. This project never ships Valeton data: on first use the app asks
the user for the official V2.1 update file, verifies its SHA-256 and keeps a copy in the user's own data directory.
It is re-verified against the hard-coded hash each time it is used, and any other file (another
firmware version, another product, a modified file) is refused.
"""
from __future__ import annotations

import hashlib
import os
import sys
from dataclasses import dataclass
from pathlib import Path

from . import frmw

HEADER_BYTES = frmw.HEADER_SIZE


@dataclass(frozen=True)
class Profile:
    name: str
    sha256: str                      # of the whole official update file
    size: int
    header_sha256: str               # of the first 0x30 bytes
    section_sizes: tuple             # sizes of sections 1, 2, 3
    section_sha256: tuple            # of sections 1, 2, 3


# Official "GP-100 Firmware V2.1" update file. Only hashes live here, never data.
V21 = Profile(
    name="Valeton GP-100 Firmware V2.1",
    sha256="13914c3c77d5ebe8fcf4a60373d3621b6fb16f82dfe935015cb5af1b03092b73",
    size=3424316,
    header_sha256="b98e7134f0b0a90aa87210ea692838fdad451a6cda47b8a9b204c7616ba24eb4",
    section_sizes=(16384, 165120, 491520),
    section_sha256=("7ecaa50b5b9fc16b5edb264a9e3dfa2d2db5635661d014140587ceebacf4ebf7",
                    "ff1c3aad5659b211272eff5f37d65708fb9ca7996542d51c7f3a28e8f8300e41",
                    "26f483f586d0c05e3b8beb7c84133468ac9d87c3291661f2cdf9bf34c272f407"),
)


class NotOfficialError(ValueError):
    """The selected file is not the official firmware we need."""


@dataclass
class Parts:
    header: bytes
    sections: tuple                  # sections 1, 2, 3
    file: bytes = b""                # the complete verified official file (needed to patch the original firmware)

    @property
    def section0(self) -> bytes:
        """Section 0 of the official file (Valeton's application)."""
        sec = frmw.parse(self.file)["sections"][0]
        return self.file[sec["file_offset"]:sec["file_end"]]


def _sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def user_data_dir() -> Path:
    if sys.platform == "win32":
        base = Path(os.environ.get("APPDATA", Path.home() / "AppData" / "Roaming"))
    elif sys.platform == "darwin":
        base = Path.home() / "Library" / "Application Support"
    else:
        base = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share"))
    return base / "gp100-mods"


def _cache_file(directory: Path | None, profile: Profile) -> Path:
    return (Path(directory) if directory else user_data_dir()) / f"valeton-{profile.sha256[:12]}.fw"


def extract(data: bytes, profile: Profile = V21) -> Parts:
    """Verify `data` is exactly the official file and return the parts we reuse."""
    if len(data) != profile.size or _sha(data) != profile.sha256:
        raise NotOfficialError(
            f"This is not the official {profile.name} update file. It must be the unmodified file from Valeton "
            "(the official GP-100 app can download it); other versions and modified files cannot be used.")
    r = frmw.parse(data)
    secs = r["sections"]
    return Parts(data[:HEADER_BYTES], tuple(data[s["file_offset"]:s["file_end"]] for s in secs[1:4]), bytes(data))


def _check(parts: Parts, profile: Profile) -> None:
    if _sha(parts.header) != profile.header_sha256:
        raise NotOfficialError("stored Valeton data is damaged (header)")
    for i, (b, n, h) in enumerate(zip(parts.sections, profile.section_sizes, profile.section_sha256), start=1):
        if len(b) != n or _sha(b) != h:
            raise NotOfficialError(f"stored Valeton data is damaged (section {i})")


def import_official(path, directory: Path | None = None, profile: Profile = V21) -> Parts:
    """Verify the user's file, then keep a copy of it in the user's data directory."""
    data = Path(path).read_bytes()
    parts = extract(data, profile)
    _check(parts, profile)
    cache = _cache_file(directory, profile)
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_bytes(data)
    return parts


def load(directory: Path | None = None, profile: Profile = V21) -> Parts | None:
    """The stored official file, re-verified, or None if the user has not provided it yet (or it got damaged)."""
    cache = _cache_file(directory, profile)
    if not cache.is_file():
        return None
    try:
        parts = extract(cache.read_bytes(), profile)
        _check(parts, profile)
    except (NotOfficialError, OSError):
        return None
    return parts


def forget(directory: Path | None = None, profile: Profile = V21) -> None:
    _cache_file(directory, profile).unlink(missing_ok=True)
