#!/usr/bin/env python3
"""Wrap an OpenGP100 slot image into a Valeton FRMW update file (GP-100 editor format).

Inputs
  --slot    gp100_base.bin from the slot-layout build; byte 0 = flash offset 0x80000
            (7-word slot header + 0x400 vector table + code, see NOTES.md "slot prototype")
  --banks   directory with rhythm_samples.bin, rhythm_patterns.bin, factory_cabinets.bin
  --stock   stock V2.1 fw.bin: its outer header, section 1 and section 3 are reused unchanged
Output
  FRMW file whose section 0 = flash 0x80000.., section 2 = flash 0x680000 (cab bank),
  sections 1 and 3 = stock bytes. Only the body size and the 8-bit body sum are recomputed.

Nothing here talks to a device. The result is UNTESTED until flashed by the owner.
"""
import argparse
import struct
from pathlib import Path

SLOT_BASE = 0x80000
RHYTHM_SAMPLES = (0x133DDC, "rhythm_samples.bin", 0x935E4)
RHYTHM_PATTERNS = (0x1C7414, "rhythm_patterns.bin", 0x447E)
CABINETS_SIZE = 40 * 0x1020          # 0x28500, same as stock section 2
MARKER = bytes.fromhex("55555555aaaaaaaaa5a5a5a5")


def fail(msg):
    raise SystemExit(f"pack_slot_frmw: {msg}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--slot", type=Path, required=True)
    ap.add_argument("--banks", type=Path, required=True)
    ap.add_argument("--stock", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()

    slot = a.slot.read_bytes()
    stock = a.stock.read_bytes()
    if stock[:4] != b"FRMW":
        fail("stock file is not FRMW")

    # stock section table (offsets relative to body = file+0x30)
    body = stock[0x30:]
    table = [struct.unpack_from("<II", body, 8 * i) for i in range(4)]
    stock_sec = [body[o:o + s] for o, s in table]
    if table[2][1] != CABINETS_SIZE:
        fail(f"stock section 2 is {table[2][1]:#x}, expected {CABINETS_SIZE:#x}")

    # slot header sanity (what the vendor bootloader's boot_image() will read)
    w = struct.unpack_from("<7I", slot, 0)
    if slot[16:28] != MARKER:
        fail("slot header magic words missing")
    itcm_src, itcm_len, sdram_src, sdram_len = w[:4]
    if not (SLOT_BASE + 0x20 <= itcm_src) or itcm_src + itcm_len > SLOT_BASE + len(slot):
        fail(f"bad ITCM payload {itcm_src:#x}+{itcm_len:#x}")
    # the ITCM payload is the vector table, and the firmware points VTOR at it in flash: VTOR ignores
    # bits [6:0] and the M7 needs size-aligned tables -> require 1 KiB alignment (bug found on device 2026-11-21)
    if itcm_src & 0x3FF:
        fail(f"vector table at flash {itcm_src:#x} is not 1 KiB aligned: the first interrupt would lock up the core")
    # vendor select_slot() (0x60003108) requires ALL FOUR of src/len words non-zero, else it boots slot A
    if not (itcm_src and itcm_len and sdram_src and sdram_len):
        fail("slot header has a zero payload word: the vendor bootloader would reject this slot and boot slot A")
    if sdram_src + sdram_len > SLOT_BASE + len(slot):
        fail("SDRAM payload runs past the slot image")
    reset = struct.unpack_from("<I", slot, itcm_src - SLOT_BASE + 4)[0]
    if not (0x60000000 + SLOT_BASE <= reset < 0x60000000 + RHYTHM_SAMPLES[0]) or not reset & 1:
        fail(f"reset vector {reset:#x} is not a Thumb address inside the slot image")

    # section 0 = slot image + banks at their fixed flash offsets
    sec0 = bytearray(slot)
    for off, name, size in (RHYTHM_SAMPLES, RHYTHM_PATTERNS):
        bank = (a.banks / name).read_bytes()
        if len(bank) != size:
            fail(f"{name} is {len(bank):#x}, expected {size:#x}")
        rel = off - SLOT_BASE
        if rel < len(sec0):
            fail(f"slot image ({len(slot):#x} B) runs into {name} at {off:#x}")
        sec0 += b"\xff" * (rel - len(sec0))
        sec0 += bank
    # reserved, empty boot-animation area (the gp100-mods app fills it; empty = firmware shows its built-in logo)
    from gp100mods import features
    from gp100mods.bootanim import format as anim_fmt
    anim_rel = anim_fmt.FLASH_OFFSET - SLOT_BASE
    if len(sec0) > features.REL:
        fail(f"section 0 ({len(sec0):#x} B) overlaps the feature-flags block at {features.FLASH_OFFSET:#x}")
    sec0 += b"\xff" * (features.REL - len(sec0))
    sec0 += features.build_block(0)                      # all features off; the gp100-mods app switches them on
    if len(sec0) > anim_rel:
        fail(f"section 0 ({len(sec0):#x} B) overlaps the boot-animation area at {anim_fmt.FLASH_OFFSET:#x}")
    sec0 += b"\xff" * (anim_rel - len(sec0))
    sec0 += anim_fmt.empty_area()
    cabs = (a.banks / "factory_cabinets.bin").read_bytes()
    if len(cabs) != CABINETS_SIZE:
        fail(f"factory_cabinets.bin is {len(cabs):#x}")

    secs = [bytes(sec0), stock_sec[1], cabs, stock_sec[3]]
    new_body = bytearray()
    off = 0x20
    tbl = b""
    for s in secs:
        tbl += struct.pack("<II", off, len(s))
        off += len(s)
    new_body = tbl + b"".join(secs)

    hdr = bytearray(stock[:0x30])
    struct.pack_into("<I", hdr, 4, len(new_body))
    hdr[0x0E] = sum(new_body) & 0xFF
    out = bytes(hdr) + new_body
    a.out.write_bytes(out)
    print(f"wrote {a.out}  {len(out)} bytes  body sum 0x{hdr[0x0E]:02x}")
    for i, s in enumerate(secs):
        print(f"  section {i}: {len(s):#x} bytes")
    print(f"  slot header: itcm {itcm_src:#x}+{itcm_len:#x}  reset vector {reset:#x}")


if __name__ == "__main__":
    main()
