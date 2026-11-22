"""The default boot animation: "dog boot" with a loading bar (160x128, 3.5 s). It is the GIF that was flashed and confirmed on a
real pedal; for the original firmware it is installed byte for byte, for OpenGP100 it is converted like any other GIF."""
from pathlib import Path

DOGBOOT_GIF = Path(__file__).resolve().parent / "data" / "dogboot.gif"


def dogboot_bytes() -> bytes:
    return DOGBOOT_GIF.read_bytes()
