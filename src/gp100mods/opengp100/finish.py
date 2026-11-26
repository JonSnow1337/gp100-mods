#!/usr/bin/env python3
"""Turn OpenGP100's own stock-slot update file into the one this app builds on.

OpenGP100's build (target gp100_stock_slot with GP100_STOCK_UPDATE_TEMPLATE) already produces a complete Valeton FRMW update
file: our code in the bootloader's application slot, its rhythm banks, and its cabinet bank. This step only adds what the
gp100-mods app switches on later, in free space of section 0:

  * the feature-flags block (all features off) at flash 0x1CBF00, and
  * an empty boot-animation area at flash 0x1CC000 (empty = the firmware shows its built-in logo).

It also refuses files that the vendor bootloader would not boot (see the checks below). Nothing here talks to a device.
"""
import argparse
import struct
from pathlib import Path

from gp100mods import features, frmw, update_file
from gp100mods.bootanim import format as anim_fmt

SLOT_BASE = 0x80000
MARKER = bytes.fromhex("55555555aaaaaaaaa5a5a5a5")
PRESETS_MARKER = b"GP100-FACTORY-PRESETS-1"        # the firmware can carry factory presets (app/factory_presets.c)
FLASH_BASE = 0x60000000


def check_slot(sec0: bytes):
    """The checks of the vendor bootloader's select_slot()/boot_image() that we can make offline. Raises ValueError."""
    if len(sec0) < features.REL + features.BLOCK_BYTES:
        raise ValueError(f"section 0 is only {len(sec0):#x} bytes: the update file is not an OpenGP100 slot image")
    itcm_src, itcm_len, sdram_src, sdram_len = struct.unpack_from("<4I", sec0, 0)
    if sec0[16:28] != MARKER:
        raise ValueError("slot header magic words missing")
    # select_slot() (0x60003108) needs all four src/len words non-zero, otherwise it boots slot A (the updater) instead
    if not (itcm_src and itcm_len and sdram_src and sdram_len):
        raise ValueError("slot header has a zero payload word: the vendor bootloader would reject this slot")
    if not SLOT_BASE + 0x20 <= itcm_src or itcm_src - SLOT_BASE + itcm_len > len(sec0):
        raise ValueError(f"bad ITCM payload {itcm_src:#x}+{itcm_len:#x}")
    # The bootloader copies the ITCM payload to 0 and jumps through its second word: it must be a Thumb address.
    stack, entry = struct.unpack_from("<2I", sec0, itcm_src - SLOT_BASE)
    if not 0x20000000 < stack <= 0x20080000 or not entry & 1:
        raise ValueError(f"invalid ITCM entry vectors: stack {stack:#x}, entry {entry:#x}")
    if PRESETS_MARKER not in sec0:
        raise ValueError("this build lacks the factory-presets support (is app/factory_presets.c in the build?)")


def finish(data: bytes) -> bytes:
    """`data` = the update file from OpenGP100's build. Returns the file with the reserved areas added."""
    r, secs = update_file._sections(data)
    if not r["ok"] or len(secs) != 4:
        raise ValueError("not a valid 4-section GP-100 update file")
    parts = [data[o:o + s] for o, s in secs]
    sec0 = bytearray(parts[0])
    check_slot(bytes(sec0))
    anim_rel = anim_fmt.FLASH_OFFSET - SLOT_BASE
    area = anim_fmt.empty_area()
    if anim_rel + len(area) > len(sec0):
        raise ValueError("section 0 is too small for the boot-animation area")
    # the reserved areas must still be erased (0xFF) in the build, i.e. nothing of the firmware lives there
    for what, start, end in (("feature-flags block", features.REL, features.REL + features.BLOCK_BYTES),
                             ("boot-animation area", anim_rel, anim_rel + anim_fmt.AREA_BYTES)):
        if end > len(sec0) or sec0[start:end].count(0xFF) != end - start:
            raise ValueError(f"the build already uses the {what} at section-0 offset {start:#x}")
    sec0[features.REL:features.REL + features.BLOCK_BYTES] = features.build_block(0)
    sec0[anim_rel:anim_rel + len(area)] = area
    return update_file._seal(data, [bytes(sec0), *parts[1:]])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("update", type=Path, help="OpenGP100-stock-update.bin from OpenGP100's build")
    ap.add_argument("out", type=Path)
    a = ap.parse_args()
    if a.update.resolve() == a.out.resolve():
        ap.error("choose a different output file")
    try:
        out = finish(a.update.read_bytes())
    except ValueError as e:
        raise SystemExit(f"opengp100-finish: {e}")
    a.out.write_bytes(out)
    print(f"wrote {a.out}  {len(out)} bytes  (feature-flags block + empty boot-animation area added)")


if __name__ == "__main__":
    main()
