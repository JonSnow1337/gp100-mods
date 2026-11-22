"""Official firmware files the patch tools know how to modify."""
import hashlib
from pathlib import Path

# SHA-256 of the official Valeton GP-100 "V2.1" update file ("GP-100 Firmware V2.1.bin"). All byte offsets used by
# the patch tools were measured on exactly this file.
V21_SHA256 = "13914c3c77d5ebe8fcf4a60373d3621b6fb16f82dfe935015cb5af1b03092b73"
V21_SIZE = 3424316


def sha256_file(path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def identify(path) -> str:
    """Return 'v2.1' if the file is the official V2.1 update, else 'unknown'."""
    p = Path(path)
    if not p.is_file() or p.stat().st_size != V21_SIZE:
        return "unknown"
    return "v2.1" if sha256_file(p) == V21_SHA256 else "unknown"


def require_v21(path) -> None:
    if identify(path) != "v2.1":
        raise SystemExit(
            f"{path}: not the official Valeton GP-100 V2.1 firmware (expected sha256 {V21_SHA256}). "
            "Download 'GP-100 Firmware V2.1' from Valeton and point to that file.")
