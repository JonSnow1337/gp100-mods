"""Valeton editor presets (.prst) -> OpenGP100 factory presets.

A .prst file is XML: one <presets> element per preset, each with nine <Effect> elements (one per effect family) plus the
footswitch and expression-pedal assignments. OpenGP100 has its own preset format (storage/patch_chain.c in its source): a
0x30E-byte record holding up to 8 blocks in any order. Switched-off blocks that no footswitch can turn on are left out; if a
preset needs more processing power than the pedal has, the least audible blocks are dropped and reported as warnings.

The converted records travel inside the update file as a "GPPR" block (app/factory_presets.h in patch 0004) placed right
after the boot-animation area. A slot shows its factory preset until you save something there.
"""
from __future__ import annotations

import functools
import json
import struct
import zlib
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

RECORD_SIZE = 0x30E
CRC_OFFSET = 0x30A
HEADER_BYTES = 0x20
MAX_PRESETS = 99
NAME_BYTES, AUTHOR_BYTES, NOTES_BYTES = 12, 20, 50
MAGIC, VERSION = b"GPPR", 1

FAMILIES = ("PRE", "DST", "AMP", "NR", "CAB", "EQ", "MOD", "DLY", "RVB")      # record order
NONE = 0xFFFF


class PresetError(ValueError):
    pass


@dataclass
class Preset:
    name: str
    record: bytes
    author: str = ""
    warnings: list = field(default_factory=list)


def _num(el, key, default=0.0):
    v = el.get(key)
    if v is None or v == "":
        return default
    try:
        return float(v)
    except ValueError:
        raise PresetError(f"{key}={v!r} is not a number") from None


def _text(s: str, size: int) -> bytes:
    """ASCII, at most size-1 bytes (the field is NUL-terminated)."""
    return s.encode("ascii", "replace")[:size - 1]


# ---- OpenGP100's own preset ("patch") format -------------------------------------------------------------------------
# Instead of Valeton's fixed nine-slot chain, an OpenGP100 preset is a list of at most 8 blocks in any order (storage/patch_chain.c).
# The numbers below mirror that file.
CHAIN_HEADER, CHAIN_VERSION = 0x0E, 1
BLOCK_START, BLOCK_SIZE, MAX_BLOCKS = 0x20, 68, 8
EXP_START = BLOCK_START + MAX_BLOCKS * BLOCK_SIZE
COMMON_START = 0x272
CPU_BUDGET, DSP_BYTES = 72, 3 * 1024 * 1024
FOOTSWITCH_BITS = (1, 2)                       # FS1, FS2: a block with that bit is toggled by the footswitch

# CPU charge per algorithm index (patch_chain.c: units of 10,000 cycles per 64-frame block). Amp profiles past the table cost 35.
_COST = (
    (3, 4, 8, 2, 4, 4, 3, 4, 4, 3, 3, 2, 9, 8, 4, 2),
    (24, 12, 8, 11, 14, 12, 14, 11, 14, 15, 18, 5, 20, 9, 19, 9, 15, 8, 2),
    (12, 18, 12, 14, 16, 16, 5, 9, 13, 15, 11, 23, 33, 16, 20, 15, 18, 17, 24, 18, 9, 16, 13, 19, 16, 16, 14, 16, 13, 17,
     16, 13, 22, 18, 19, 12, 22, 19, 18, 10, 6, 4, 11, 3, 18, 10),
    (2, 2), (17,), (3, 3, 3),
    (6, 6, 5, 3, 3, 4, 4, 5, 4, 3, 5, 3), (9, 2, 5, 3, 3, 4, 3, 4, 6, 3, 5, 2), (7, 6, 7, 6, 6, 6, 6, 6, 6, 9),
)
# When a preset does not fit, blocks are given up in this order (least audible first).
_DROP_ORDER = (5, 3, 0, 6, 8, 7, 1)             # EQ, NR, PRE, MOD, RVB, DLY, DST


@functools.lru_cache(maxsize=1)
def catalog() -> dict:
    return json.loads((Path(__file__).resolve().parent / "data" / "opengp100_catalog.json").read_text())


def _cost(fam, alg):
    if fam == 4:
        return _COST[4][0]
    if fam == 2 and alg >= len(_COST[2]):
        return 35
    return _COST[fam][alg] if alg < len(_COST[fam]) else CPU_BUDGET


def _fits(blocks) -> bool:
    cat = catalog()
    return (len(blocks) <= MAX_BLOCKS
            and sum(cat["state_bytes"][b["fam"]] for b in blocks) <= DSP_BYTES
            and sum(_cost(b["fam"], b["alg"]) for b in blocks) <= CPU_BUDGET)


def convert_preset(el: ET.Element) -> Preset:
    """One <presets> element -> an OpenGP100 preset record (with a valid CRC)."""
    cat = catalog()
    rec = bytearray(bytes.fromhex(cat["template"]))
    warnings = []
    name = el.get("ppName", "").strip() or "Preset"

    def w16(off, v): struct.pack_into("<H", rec, off, max(0, min(0xFFFF, int(round(v)))))
    def wf(off, v): struct.pack_into("<f", rec, off, float(v))

    ctrl = el.find("ppCtrl")
    fs_of = {}                                                             # family -> footswitch bits
    for i, k in enumerate(("c11", "c12", "c13", "c21", "c22", "c23")):
        v = _num(ctrl, k, NONE) if ctrl is not None else NONE
        if 0 <= v < 9:
            fs_of[int(v)] = fs_of.get(int(v), 0) | FOOTSWITCH_BITS[i // 3]
    exp = el.find("ppEXP1")
    exp_target = None                                                      # first expression-controlled parameter
    if exp is not None and _num(exp, "expTarget") == 1:
        for t in range(3):
            node = exp.find(f"ppEXP1_{t}")
            if node is not None and 0 <= _num(node, "expMId", NONE) < 9:
                if exp_target is None:
                    exp_target = (int(_num(node, "expMId")), int(_num(node, "expIndex")),
                                  _num(node, "expMin", 0), _num(node, "expMax", 99))
                else:
                    warnings.append("only one expression-pedal target is supported; the others were dropped")
                break

    blocks, seen = [], set()
    for e in el.findall("Effect"):
        mod = e.get("effectModuleName", "")
        if mod not in FAMILIES:
            warnings.append(f"unknown effect block {mod!r} ignored")
            continue
        fam = FAMILIES.index(mod)
        if fam in seen:
            raise PresetError(f"{name}: two {mod} blocks")
        seen.add(fam)
        code = int(_num(e, "effectCode"))
        alg = next((i for i, a in enumerate(cat["families"][fam]) if a["id"] == code), None)
        on = bool(_num(e, "effectState"))
        linked = exp_target is not None and exp_target[0] == fam
        if not (on or fs_of.get(fam) or linked):
            continue                                                       # a switched-off block nobody can turn on adds nothing
        if alg is None:
            warnings.append(f"{e.get('effectName', mod)} is not available in OpenGP100; left out")
            continue
        params = [_num(e, f"params_{i}") for i in range(15)]
        blocks.append({"fam": fam, "alg": alg, "on": on, "fs": fs_of.get(fam, 0), "pos": _num(e, "x"), "params": params})
    if len(seen) != len(FAMILIES):
        raise PresetError(f"{name}: has {len(seen)} of 9 effect blocks")
    blocks.sort(key=lambda b: b["pos"])
    for fam in _DROP_ORDER:
        if _fits(blocks):
            break
        drop = [b for b in blocks if b["fam"] == fam and not (exp_target and exp_target[0] == fam)]
        if drop:
            blocks.remove(drop[0])
            warnings.append(f"too much for the pedal's processor: {cat['families'][fam][drop[0]['alg']]['name']} left out")
    if not _fits(blocks):
        raise PresetError(f"{name}: does not fit the pedal's processor even after dropping blocks")

    rec[2:2 + NAME_BYTES] = _text(name, NAME_BYTES).ljust(NAME_BYTES, b"\0")
    rec[CHAIN_HEADER:COMMON_START] = bytes(COMMON_START - CHAIN_HEADER)
    rec[CHAIN_HEADER], rec[CHAIN_HEADER + 1] = CHAIN_VERSION, len(blocks)
    for i, b in enumerate(blocks):
        rec[CHAIN_HEADER + 2 + i] = i                                      # chain order = block ids in order
        o = BLOCK_START + i * BLOCK_SIZE
        rec[o], rec[o + 1] = b["fam"] + 1, (b["fs"] << 1) | int(b["on"])
        struct.pack_into("<I", rec, o + 4, cat["families"][b["fam"]][b["alg"]]["id"])
        for j, v in enumerate(b["params"]):
            wf(o + 8 + j * 4, v)
    if exp_target and any(b["fam"] == exp_target[0] for b in blocks):
        fam, slot, lo, hi = exp_target
        bi = next(i for i, b in enumerate(blocks) if b["fam"] == fam)
        params = cat["families"][fam][blocks[bi]["alg"]]["params"]
        idx = next((i for i, p in enumerate(params) if p[0] == slot), None)
        if idx is None:
            warnings.append("the expression-pedal parameter does not exist in OpenGP100; pedal set to volume")
        else:
            _, pmin, pmax = params[idx]
            rec[EXP_START:EXP_START + 12] = bytes(12)
            rec[EXP_START], rec[EXP_START + 1], rec[EXP_START + 2] = 0xA1, bi, idx
            wf(EXP_START + 4, pmin + min(99, max(0, lo)) * (pmax - pmin) / 99)
            wf(EXP_START + 8, pmin + min(99, max(0, hi)) * (pmax - pmin) / 99)
            w16(0x282, 1)
    w16(0x272, min(250, max(40, _num(el, "ppBPM", 120))))
    w16(0x274, min(100, max(0, _num(el, "ppVolume", 50))))
    if rec[EXP_START] != 0xA1:
        w16(0x282, 0)
    w16(0x284, _num(exp, "expVolume") if exp is not None else 0)
    if exp is not None:
        wf(0x29E, min(99, max(0, _num(exp, "expVolumeMin", 0))))
        wf(0x2A2, min(99, max(0, _num(exp, "expVolumeMax", 99))))
    genre = _num(el, "ppType")
    w16(0x2C0, genre if 0 <= genre < 61 else 0)
    rec[0x2C2:0x2C2 + AUTHOR_BYTES] = bytes(AUTHOR_BYTES)
    rec[0x2D6:0x2D6 + NOTES_BYTES] = bytes(NOTES_BYTES)
    author, notes = _text(el.get("ppAuthor", ""), AUTHOR_BYTES), _text(el.get("ppNotes", ""), NOTES_BYTES)
    rec[0x2C2:0x2C2 + len(author)] = author
    rec[0x2D6:0x2D6 + len(notes)] = notes
    struct.pack_into("<I", rec, CRC_OFFSET, zlib.crc32(bytes(rec[:CRC_OFFSET])))
    return Preset(name, bytes(rec), el.get("ppAuthor", ""), warnings)


def load_prst(path) -> list:
    """All presets in a .prst file (in slot order), as `Preset`. Raises PresetError if the file is not a usable preset file."""
    try:
        root = ET.parse(str(path)).getroot()
    except (ET.ParseError, OSError) as e:
        raise PresetError(f"{Path(path).name}: not a readable preset file ({e})") from None
    els = root.findall("presets")
    if not els:
        raise PresetError(f"{Path(path).name}: contains no presets")
    els.sort(key=lambda p: (_num(p, "ppBank"), _num(p, "ppID")))
    return [convert_preset(p) for p in els]


def build_block(records) -> bytes:
    """The GPPR block for `records` (bytes or None for an empty slot), slot P01 first. Not padded."""
    records = list(records)
    if len(records) > MAX_PRESETS:
        raise PresetError(f"at most {MAX_PRESETS} presets fit, got {len(records)}")
    for r in records:
        if r is not None and len(r) != RECORD_SIZE:
            raise PresetError("bad record size")
    head = MAGIC + struct.pack("<HHHH", VERSION, HEADER_BYTES, len(records), RECORD_SIZE).ljust(HEADER_BYTES - 4, b"\xff")
    return head + b"".join(r if r is not None else b"\xff" * RECORD_SIZE for r in records)


def parse_block(block: bytes) -> list:
    """Inverse of build_block (validates like the firmware): list of record bytes or None. Raises PresetError."""
    if len(block) < HEADER_BYTES or block[:4] != MAGIC:
        raise PresetError("no GPPR header")
    ver, hsz, count, rsz = struct.unpack_from("<HHHH", block, 4)
    if ver != VERSION or hsz != HEADER_BYTES or rsz != RECORD_SIZE or not 0 <= count <= MAX_PRESETS:
        raise PresetError("bad GPPR header")
    if len(block) < HEADER_BYTES + count * RECORD_SIZE:
        raise PresetError("GPPR block is truncated")
    out = []
    for i in range(count):
        r = block[HEADER_BYTES + i * RECORD_SIZE:HEADER_BYTES + (i + 1) * RECORD_SIZE]
        if r == b"\xff" * RECORD_SIZE:
            out.append(None)
        elif struct.unpack_from("<I", r, CRC_OFFSET)[0] == zlib.crc32(r[:CRC_OFFSET]):
            out.append(r)
        else:
            out.append(None)
    return out
