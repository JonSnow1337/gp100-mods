"""Finer MASTER volume steps (patch of the V2.1 firmware; fixed parameters, no assembler needed).

Stock: MASTER 0..100 becomes an integer gain g = m*255/100, so the quiet end is very coarse (1/2/3 -> 2/5/7).
Patch: g = (m*m*(m+60)) >> 5 with the same ramp time and the same level at MASTER = 100. New MASTER 20 is about as loud as
stock 2, 40 about stock 10. Input level, patch volume, the volume pedal and the editor's read-out are untouched.
The 40 replacement bytes were produced by assembling the sequence in tools (blend 60, shift 5) and verified on a real unit's
firmware build; here they are stored as bytes so the app needs no assembler.
"""
import struct

from gp100mods import frmw
from gp100mods.stock import guarded

BLOB = 0x70                 # file offset = blob address + 0x70
SITE = BLOB + 0x3970        # the stock gain computation (40 bytes)
LIT = BLOB + 0x3C74         # float C
OLD_C = 0x46E850A8
NEW_C = 0x4317A7C0
OLD_SITE = bytes.fromhex("c1eb012180fb01104111a1ebe07001b2adf80810bbf804008842c8bf401eb8bf401c18bfabf80400")
NEW_SITE = bytes.fromhex("01fb01f23c3101fb02f04109bbf804000a1ac52ac8bfc52212f1c50fb8bf6ff0c4028018abf80400")


@guarded
def apply(original: bytes) -> bytes:
    if not frmw.parse(original)["ok"]:
        raise ValueError("firmware fails the container checks")
    if original[SITE:SITE + 40] != OLD_SITE or struct.unpack_from("<I", original, LIT)[0] != OLD_C:
        raise ValueError("unexpected code at the patch site (not the V2.1 firmware)")
    d = bytearray(original)
    d[SITE:SITE + 40] = NEW_SITE
    struct.pack_into("<I", d, LIT, NEW_C)
    d[0x0E] = sum(d[frmw.HEADER_SIZE:]) & 0xFF
    if not frmw.parse(bytes(d))["ok"]:
        raise RuntimeError("internal error: output fails the container checks")
    return bytes(d)
