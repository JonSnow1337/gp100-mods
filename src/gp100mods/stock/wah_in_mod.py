"""Build an EXPERIMENTAL GP-100 V2.1 image that puts PRE wah effects into MOD slots. Offline only.

Effects are Thumb code blobs, linked for one slot's code buffer and state memory (see NOTES.md). A PRE blob is moved to MOD by
adding the buffer deltas to every aligned word that points into PRE's code buffer or PRE's state memory. This exact transform
reproduces the firmware's own PRE->DST copies of Boost and 14 Boost byte for byte (checked by --selftest).

For each --map PRE_INDEX:MOD_INDEX the tool:
  1. relocates the PRE blob to MOD,
  2. writes it over the old MOD blob in place (it must not be longer), updating the directory length,
  3. sets MOD's code-table entry, parameter count and parameter-id pointer to the PRE effect's.
The 0x80000000 image maps as file = 0x349c1 + (address - 0x80000000).
"""

import struct

from gp100mods import frmw

IMG_BASE = 0x349C1
img = lambda a: IMG_BASE + (a - 0x80000000)  # noqa: E731
DIR = 0x80009AA4
BASES = [0x08, 0x8C, 0x128, 0x2C4, 0x2D8, 0x2E4, 0x300, 0x364, 0x3C8]
PRE, DST, MOD = 0, 1, 6
CODE_BUF = {0: (0x37F80, 0x3000), 1: (0x3AF80, 0x2800), 6: (0x46800, 0x2800)}
STATE_BUF = {0: (0x20029F80, 0x1800), 1: (0x2002B780, 0xC00), 6: (0x2002DC00, 0x1000)}
CODES = {PRE: 0x80133194, MOD: 0x80133BF0}
COUNTS = {PRE: 0x80133214, MOD: 0x80133BD8}
IDPTRS = {PRE: 0x801331D4, MOD: 0x80133BA8}
NAMES = ["COMP", "COMP4", "Boost", "14 Boost", "AC Sim", "T-Wah", "A-WAH", "V-Wah", "C-Wah", "OCTA", "Pitch", "P-Bend",
         "Hammy", "Saturate", "Step Filter", "Ring Mod"]
MOD_NAMES = ["A-Chorus", "G-Chorus", "B-Chorus", "Detune", "Flanger", "Vibrato", "Phaser", "Vibe", "Opto Trem", "Sine Trem",
             "Triangle Trem", "Bias Trem"]


def u32(d, o):
    return struct.unpack_from("<I", d, o)[0]


def entry_off(t, i):
    return img(DIR) + BASES[t] + 8 * i


def read_blob(data, t, i):
    off, ln = u32(data, entry_off(t, i)), u32(data, entry_off(t, i) + 4)
    start = img(DIR) + 4 + off
    return bytearray(data[start:start + ln]), start


def relocate(blob, src, dst):
    out = bytearray(blob)
    (cs, cl), (cd, _) = CODE_BUF[src], CODE_BUF[dst]
    (ss, sl), (sd, _) = STATE_BUF[src], STATE_BUF[dst]
    moved = 0
    for k in range(0, len(out) - 3, 4):
        w = u32(out, k)
        a = w & ~1
        if cs <= a < cs + cl:
            struct.pack_into("<I", out, k, (a - cs + cd) | (w & 1))
            moved += 1
        elif ss <= w < ss + sl:
            struct.pack_into("<I", out, k, w - ss + sd)
            moved += 1
    return out, moved


def selftest(data):
    for pre_i, dst_i, name in ((2, 17, "Boost"), (3, 18, "14 Boost")):
        a, _ = read_blob(data, PRE, pre_i)
        b, _ = read_blob(data, DST, dst_i)
        if not a or not b:
            raise ValueError("not the V2.1 firmware: effect blobs not found")
        r, n = relocate(a, PRE, DST)
        if r != b:
            raise ValueError(f"not the V2.1 firmware: relocating PRE {name} does not reproduce the firmware's own DST {name}")


WAH_PAIRS = ((7, 9), (8, 10))     # V-Wah over Sine Trem, C-Wah over Triangle Trem (flashed and confirmed on a real unit)


def apply(original: bytes, pairs=WAH_PAIRS) -> bytes:
    """Run PRE wahs from MOD slots. `pairs` = (PRE index, MOD index)."""
    try:
        return _apply(original, pairs)
    except (struct.error, IndexError):
        raise ValueError("not the V2.1 firmware (unexpected layout)") from None


def _apply(original: bytes, pairs) -> bytes:
    if not frmw.parse(original)["ok"]:
        raise ValueError("firmware fails the container checks")
    selftest(original)
    data = bytearray(original)
    if len({m for _, m in pairs}) != len(pairs):
        raise ValueError("a MOD index is used twice")
    for pre_i, mod_i in pairs:
        pre_code = u32(original, img(CODES[PRE]) + 4 * pre_i)
        if (pre_code >> 24) not in (0x01, 0x05):
            raise ValueError(f"PRE {NAMES[pre_i]} is not a wah-family effect")
        new, moved = relocate(read_blob(original, PRE, pre_i)[0], PRE, MOD)
        old, start = read_blob(original, MOD, mod_i)
        if len(new) > len(old):
            raise ValueError(f"{NAMES[pre_i]} does not fit over MOD {MOD_NAMES[mod_i]}")
        mb, ms = CODE_BUF[MOD]
        for word in (4, 8):
            w = u32(new, word)
            if not (w & 1 and mb <= (w & ~1) < mb + ms):
                raise ValueError("relocated effect points outside the MOD code buffer")
        data[start:start + len(new)] = new
        struct.pack_into("<I", data, entry_off(MOD, mod_i) + 4, len(new))
        struct.pack_into("<I", data, img(CODES[MOD]) + 4 * mod_i, pre_code)
        count = struct.unpack_from("<h", original, img(COUNTS[PRE]) + 2 * pre_i)[0]
        struct.pack_into("<h", data, img(COUNTS[MOD]) + 2 * mod_i, count)
        struct.pack_into("<I", data, img(IDPTRS[MOD]) + 4 * mod_i, u32(original, img(IDPTRS[PRE]) + 4 * pre_i))
    data[0x0E] = sum(data[frmw.HEADER_SIZE:]) & 0xFF
    if not frmw.parse(bytes(data))["ok"]:
        raise RuntimeError("internal error: output fails the container checks")
    return bytes(data)
