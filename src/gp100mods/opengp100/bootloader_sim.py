#!/usr/bin/env python3
"""Offline model of the vendor bootloader's boot_image() (from archive/bootloader/refined/boot_image.c)
applied to a virtual flash = a flash dump with an FRMW update's sections written over it.
Read-only on all inputs; writes only an optional virtual-flash file. Nothing touches a device."""
import argparse, struct, subprocess, tempfile, os
from pathlib import Path

SEC_DEST = {0: 0x80000, 2: 0x680000, 3: 0x700000}   # section 1 destination unknown -> not applied

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", type=Path, required=True)
    ap.add_argument("--frmw", type=Path, required=True)
    ap.add_argument("--save", type=Path)
    ap.add_argument("--buttons", choices=["none", "all"], default="none",
                    help="all = footswitch -, footswitch + and PARA held at power-on (recovery combo); none = normal boot")
    a = ap.parse_args()
    fl = bytearray(a.dump.read_bytes())
    fw = a.frmw.read_bytes(); body = fw[0x30:]
    for i, dest in SEC_DEST.items():
        off, size = struct.unpack_from("<II", body, 8 * i)
        if i == 0:                       # slot B is rewritten: emulate the erase of its whole old range
            fl[0x80000:0x31FAEA] = b"\xff" * (0x31FAEA - 0x80000)
        fl[dest:dest + size] = body[off:off + size]
        print(f"section {i} -> flash {dest:#08x}..{dest+size:#08x}")
    if a.save: a.save.write_bytes(fl)

    # --- select_slot() @0x60003108 as decompiled with Ghidra (ghidra/out/vendor_boot_all.c) ---
    hdr = struct.unpack_from("<7I", fl, 0x80000)
    token = fl[0x7ff000:0x7ff008]
    slotB_valid = all(hdr[:4]) and hdr[4:7] == (0x55555555, 0xAAAAAAAA, 0xA5A5A5A5)
    token_erased = token == b"\xff" * 8
    # GPIO1 bits 22, 10, 18 = footswitch -, footswitch +, PARA; buttons are active-low
    straps_all_low = a.buttons == "all"
    sel = 1 if (slotB_valid and not token_erased and not straps_all_low) else 0
    print(f"select_slot: slotB_valid={slotB_valid} token_erased={token_erased} all_three_buttons_held={straps_all_low} -> returns {sel}",
          "(boots slot B 0x60080000)" if sel else "(boots slot A 0x60020000 = updater / recovery)")
    if sel == 0:
        pa = struct.unpack_from("<7I", fl, 0x20000)
        print("slot A header:", [hex(x) for x in pa], "magic ok:", pa[4:7] == (0x55555555, 0xAAAAAAAA, 0xA5A5A5A5))
        return
    p = struct.unpack_from("<7I", fl, 0x80000)
    print("slot header @0x80000:", [hex(x) for x in p])
    ram = {}                              # sparse model of ITCM / SDRAM
    itcm = bytearray(0x50000); sdram_copied = 0
    if p[3] and p[2]: sdram_copied = p[3]
    if p[1] and p[0]:
        itcm[0:p[1]] = fl[p[0]:p[0] + p[1]]
        assert itcm[:p[1]] == fl[p[0]:p[0] + p[1]], "verify mismatch (bootloader would hang)"
    ok = (p[4], p[5], p[6]) == (0x55555555, 0xAAAAAAAA, 0xA5A5A5A5)
    print("magic ok:", ok, "| SDRAM bytes copied:", sdram_copied, "| ITCM bytes copied:", p[1])
    if not ok: raise SystemExit("bootloader would return 1 (fall back to the other slot)")
    sp, entry = struct.unpack_from("<2I", itcm, 0)
    print(f"ITCM[0]={sp:#x}  ITCM[1] (jump target)={entry:#x}")
    assert entry & 1, "target is not Thumb"
    if entry < 0x80000:                   # inside ITCM: OpenGP100's stock-slot trampoline (it then switches FlexRAM and jumps to flash)
        print("target is in ITCM (a trampoline copied by the bootloader), not in flash")
        code, base = bytes(itcm[(entry & ~1):(entry & ~1) + 48]), entry & ~1
    else:
        foff = (entry & ~1) - 0x60000000
        print("target is in flash offset", hex(foff), "inside slot:", 0x80000 <= foff < 0x31FAEA)
        code, base = fl[foff:foff + 48], entry & ~1
    with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as t:
        t.write(code); n = t.name
    out = subprocess.check_output(["arm-none-eabi-objdump", "-D", "-b", "binary", "-marm", "-Mforce-thumb",
                                   f"--adjust-vma={base:#x}", n], text=True)
    os.unlink(n)
    print("\n".join(out.splitlines()[7:17]))

if __name__ == "__main__":
    main()
