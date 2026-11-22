"""Rewrite the three text lines of the GP-100 firmware-version screen ("Firmware version V2.1 / (c) Valeton / All Rights Reserved"). Offline only.

How the screen works (V2.1, see NOTES.md): three right-aligned text lines drawn into stacked boxes at the bottom of the display.
  line 1: per-language string table 1 (label "Firmware version"),  line 3: per-language string table 2 ("All Rights Reserved"),
  line 2: a literal "(c) Valeton" loaded with a PC-relative `adr`, and "V2.1" built from header bytes and drawn right after line 1 by one call.
The new text goes into existing string slots (no free space needed): every language entry of a table is pointed at one new string, line 2's
`adr` becomes an `ldr` of a pointer word, and the version-draw call is replaced with two NOPs. Image address -> file offset: 0x349c1 + (a - 0x80000000).

"""

import struct

from gp100mods import frmw
from gp100mods.stock import guarded

BASE = 0x349C1
fo = lambda a: BASE + (a - 0x80000000)  # noqa: E731

T1, T2 = 0x8024CD98, 0x8024CDBC                       # 7 language pointers each
T1_EXPECT = [0x80247054, 0x8024708C, 0x8024707C, 0x8024709C, 0x80247068, 0x80247028, 0x80247040]
T2_EXPECT = [0x802470FC, 0x80247144, 0x80247154, 0x80247164, 0x802470FC, 0x802470E4, 0x8024712C]
SLOT1 = (0x80247028, b"Version del Firmware\x00")    # holds line 1
SLOT2 = (0x802470FC, b"All Rights Reserved\x00")      # holds line 2
SLOT3 = (0x802470E4, b"Todos los Derechos Res.\x00")  # holds line 3
ADR_AT, ADR_OLD = 0x8000658A, bytes.fromhex("15a0")   # adr r0,[pc,#0x54]  -> target 0x800065e0
LDR_NEW = bytes.fromhex("1548")                        # ldr r0,[pc,#0x54]  -> loads the pointer word at 0x800065e0
LIT_AT, LIT_OLD = 0x800065E0, b"\xc2\xa9 Valeton\x00"
BL_AT = 0x80006570                                     # bl 0x8000024e : draws the version string right after line 1


def bl_target(b, at):
    h1, h2 = b[0] | (b[1] << 8), b[2] | (b[3] << 8)
    if (h1 & 0xF800) != 0xF000 or (h2 & 0xD000) != 0xD000:
        return None
    s, imm10, j1, j2, imm11 = (h1 >> 10) & 1, h1 & 0x3FF, (h2 >> 13) & 1, (h2 >> 11) & 1, h2 & 0x7FF
    i1, i2 = (~(j1 ^ s)) & 1, (~(j2 ^ s)) & 1
    imm = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
    if s:
        imm -= 1 << 25
    return (at + 4 + imm) & 0xFFFFFFFF


LIMITS = {"line1": len(SLOT1[1]) - 1, "line2": len(SLOT2[1]) - 1, "line3": len(SLOT3[1]) - 1}


@guarded
def apply(orig: bytes, line1: str, line2: str, line3: str, keep_version: bool = False) -> bytes:
    """Return `orig` with the three lines of the firmware-version screen replaced. ValueError if `orig` is not V2.1."""
    if not frmw.parse(orig)["ok"]:
        raise ValueError("firmware fails the container checks")
    for name, text, slot in (("line 1", line1, SLOT1), ("line 2", line2, SLOT2), ("line 3", line3, SLOT3)):
        try:
            raw = text.encode("ascii")
        except UnicodeEncodeError:
            raise ValueError(f"{name}: plain letters, digits and punctuation only") from None
        if len(raw) + 1 > len(slot[1]):
            raise ValueError(f"{name}: {len(raw)} characters do not fit ({len(slot[1]) - 1} at most)")
    u32 = lambda d, o: struct.unpack_from("<I", d, o)[0]  # noqa: E731
    if [u32(orig, fo(T1) + 4 * i) for i in range(7)] != T1_EXPECT or [u32(orig, fo(T2) + 4 * i) for i in range(7)] != T2_EXPECT:
        raise ValueError("language tables are not what was analysed (not the V2.1 firmware)")
    for addr, old in (SLOT1, SLOT2, SLOT3, (LIT_AT, LIT_OLD)):
        if orig[fo(addr):fo(addr) + len(old)] != old:
            raise ValueError(f"unexpected data at 0x{addr:08x} (not the V2.1 firmware)")
    if orig[fo(ADR_AT):fo(ADR_AT) + 2] != ADR_OLD:
        raise ValueError("line-2 instruction not found (not the V2.1 firmware)")
    if not keep_version and bl_target(orig[fo(BL_AT):fo(BL_AT) + 4], BL_AT) != 0x8000024E:
        raise ValueError("the version-draw call is not where expected (not the V2.1 firmware)")
    d = bytearray(orig)
    touched = []

    def put(addr, data):
        d[fo(addr):fo(addr) + len(data)] = data
        touched.append((fo(addr), fo(addr) + len(data)))

    for (addr, old), text in ((SLOT1, line1), (SLOT2, line2), (SLOT3, line3)):
        put(addr, text.encode("ascii") + b"\x00" * (len(old) - len(text)))
    for i in range(7):
        put(T1 + 4 * i, struct.pack("<I", SLOT1[0]))
        put(T2 + 4 * i, struct.pack("<I", SLOT3[0]))
    put(ADR_AT, LDR_NEW)
    put(LIT_AT, struct.pack("<I", SLOT2[0]) + b"\x00" * (len(LIT_OLD) - 4))
    if not keep_version:
        put(BL_AT, bytes.fromhex("00bf00bf"))
    d[0x0E] = sum(d[frmw.HEADER_SIZE:]) & 0xFF
    if not frmw.parse(bytes(d))["ok"]:
        raise RuntimeError("internal error: output fails the container checks")
    bad = [i for i in range(len(orig)) if orig[i] != d[i] and i != 0x0E and not any(s <= i < e for s, e in touched)]
    if bad:
        raise RuntimeError(f"internal error: bytes changed outside the planned edits: {bad[:5]}")
    return bytes(d)
