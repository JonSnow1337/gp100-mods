#!/usr/bin/env python3
"""Parse and verify a Valeton GP-100 FRMW firmware image. Read-only.

Layout (observed in V2.0 and V2.1; fields marked "unknown" are not understood):

  Outer header, 0x30 bytes
    0x00  4s   magic "FRMW"
    0x04  u32  body size = file size - 0x30                      [confirmed]
    0x08  u32  0x01000003 in both versions                       [unknown]
    0x0C  u16  0x0000                                            [unknown]
    0x0E  u8   8-bit sum of body bytes (file[0x30:])             [confirmed]
    0x0F  u8   0x00                                              [unknown]
    0x10  16s  model "GP100", NUL padded
    0x20  16s  "\\x01\\x00EX" then zeros                         [unknown]

  Body (file offset 0x30), offsets below are relative to the body
    +0x00  4 x (u32 offset, u32 size) section table; sections are contiguous
           and the last one ends exactly at body size            [confirmed]
    section 0 starts at body+0x20 (file 0x50) with a 0x20-byte sub-header:
      +0x00 4 x u32  unknown; word2 == word0 + word1 in both versions
      +0x10 12 bytes marker 55555555 aaaaaaaa a5a5a5a5
      +0x1C 4s       version, e.g. "V210"
    then a Cortex-M vector table at file 0x70 (initial SP, reset, ...).
"""

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

HEADER_SIZE = 0x30
SECTION_COUNT = 4
SEC0_SUBHEADER = 0x20
MARKER = bytes.fromhex("55555555aaaaaaaaa5a5a5a5")


def u32(d, o):
    return struct.unpack_from("<I", d, o)[0]


def parse(data):
    checks = []

    def check(name, ok, detail):
        checks.append({"check": name, "ok": bool(ok), "detail": detail})

    if len(data) < HEADER_SIZE + 8 * SECTION_COUNT:
        raise ValueError(f"file too small ({len(data)} bytes)")

    body = data[HEADER_SIZE:]
    magic = data[0:4]
    body_size = u32(data, 0x04)
    stored_sum = data[0x0E]
    computed_sum = sum(body) & 0xFF

    check("magic", magic == b"FRMW", repr(magic))
    check("body_size", body_size == len(body),
          f"field 0x{body_size:x}, actual body 0x{len(body):x}")
    check("body_sum8", stored_sum == computed_sum,
          f"stored 0x{stored_sum:02x}, computed 0x{computed_sum:02x}")

    sections = []
    for i in range(SECTION_COUNT):
        off, size = u32(body, 8 * i), u32(body, 8 * i + 4)
        sections.append({
            "index": i,
            "body_offset": off,
            "size": size,
            "file_offset": HEADER_SIZE + off,
            "file_end": HEADER_SIZE + off + size,
            "sha256": hashlib.sha256(body[off:off + size]).hexdigest()
            if off + size <= len(body) else None,
        })

    check("table_end", sections[0]["body_offset"] == 8 * SECTION_COUNT,
          f"section 0 at body+0x{sections[0]['body_offset']:x}, table ends at +0x{8 * SECTION_COUNT:x}")
    for a, b in zip(sections, sections[1:]):
        check(f"contiguous_{a['index']}_{b['index']}",
              a["body_offset"] + a["size"] == b["body_offset"],
              f"0x{a['body_offset']:x}+0x{a['size']:x} vs 0x{b['body_offset']:x}")
    last = sections[-1]
    check("last_ends_at_body_size", last["body_offset"] + last["size"] == len(body),
          f"ends 0x{last['body_offset'] + last['size']:x}, body 0x{len(body):x}")

    s0 = sections[0]["file_offset"]
    sub_words = [u32(data, s0 + 4 * i) for i in range(4)]
    marker = data[s0 + 0x10:s0 + 0x1C]
    version = data[s0 + 0x1C:s0 + 0x20]
    check("sec0_marker", marker == MARKER, marker.hex())
    check("sec0_word2_eq_word0_plus_word1", sub_words[2] == sub_words[0] + sub_words[1],
          " ".join(f"0x{w:x}" for w in sub_words) + " (meaning unknown)")

    # The sub-header's first word is the flash offset of the "ITCM payload" (the vector table). Stock V2.1 keeps its
    # whole ITCM image there (code runs from address 0); slot-style images (OpenGP100) keep only a vector table whose
    # reset vector points into flash (0x60000000 + offset) and must be 1 KiB aligned for VTOR.
    SLOT_BASE = 0x80000
    itcm_src = sub_words[0]
    vt = s0 + (itcm_src - SLOT_BASE)
    sec0_code_size = sections[0]["size"] - SEC0_SUBHEADER
    if not (0 <= vt - s0 < sections[0]["size"] - 8):
        check("vector_table_inside_section0", False, f"itcm_src 0x{itcm_src:x} is outside section 0")
        initial_sp = reset = 0
    else:
        initial_sp, reset = u32(data, vt), u32(data, vt + 4)
        check("vector_sp_in_sram", 0x20000000 <= initial_sp < 0x30000000, f"0x{initial_sp:08x}")
        if 0x60000000 <= reset < 0x60800000:
            in_slot = SLOT_BASE <= (reset & ~1) - 0x60000000 < SLOT_BASE + sections[0]["size"]
            check("vector_reset_thumb_in_slot", bool(reset & 1) and in_slot, f"0x{reset:08x} (execute in place from flash)")
            check("vector_table_1k_aligned", itcm_src % 0x400 == 0,
                  f"flash offset 0x{itcm_src:x}: VTOR drops the low 7 bits and the M7 needs size alignment")
            check("slot_payload_words_nonzero", all(sub_words),
                  "the vendor bootloader's select_slot rejects a slot with a zero src/len word")
        else:
            check("vector_reset_thumb_in_image", bool(reset & 1) and (reset & ~1) < sec0_code_size,
                  f"0x{reset:08x} (code at file 0x{vt:x} maps to address 0)")

    return {
        "file_size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "header": {
            "magic": magic.decode("ascii", "replace"),
            "body_size": body_size,
            "field_0x08": u32(data, 0x08),
            "sum8_0x0e": stored_sum,
            "model": data[0x10:0x20].rstrip(b"\0").decode("ascii", "replace"),
            "field_0x20_hex": data[0x20:0x30].hex(),
        },
        "sections": sections,
        "sec0_subheader": {
            "words": sub_words,
            "marker_hex": marker.hex(),
            "version": version.decode("ascii", "replace"),
        },
        "vector_table": {"file_offset": vt, "initial_sp": initial_sp, "reset": reset},
        "checks": checks,
        "ok": all(c["ok"] for c in checks),
    }


def print_report(path, r):
    h = r["header"]
    print(f"{path}")
    print(f"  size {r['file_size']} (0x{r['file_size']:x})  sha256 {r['sha256'][:16]}...")
    print(f"  model {h['model']}  version {r['sec0_subheader']['version']}  "
          f"field@0x08 0x{h['field_0x08']:08x}  sum8 0x{h['sum8_0x0e']:02x}")
    print("  sections (file offsets):")
    for s in r["sections"]:
        print(f"    [{s['index']}] 0x{s['file_offset']:07x}-0x{s['file_end']:07x}  "
              f"size 0x{s['size']:x} ({s['size']})")
    vt = r["vector_table"]
    print(f"  vector table @0x{vt['file_offset']:x}: SP 0x{vt['initial_sp']:08x}  reset 0x{vt['reset']:08x}")
    print("  checks:")
    for c in r["checks"]:
        print(f"    {'OK  ' if c['ok'] else 'FAIL'} {c['check']}: {c['detail']}")
    print(f"  result: {'all checks passed' if r['ok'] else 'SOME CHECKS FAILED'}")


def extract(data, r, out_dir):
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for s in r["sections"]:
        p = out / f"section{s['index']}.bin"
        p.write_bytes(data[s["file_offset"]:s["file_end"]])
        print(f"  wrote {p}")
    s0 = r["sections"][0]
    code = out / "section0_code.bin"
    code.write_bytes(data[s0["file_offset"] + SEC0_SUBHEADER:s0["file_end"]])
    print(f"  wrote {code}  (load at base 0x0 in Ghidra: ARM Cortex, little-endian, Thumb)")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("files", nargs="+", type=Path)
    ap.add_argument("--json", action="store_true", help="print JSON instead of text")
    ap.add_argument("--extract", metavar="DIR",
                    help="write sections to DIR (single input file only)")
    args = ap.parse_args()
    if args.extract and len(args.files) != 1:
        ap.error("--extract takes exactly one input file")

    results, ok = {}, True
    for f in args.files:
        data = f.read_bytes()
        try:
            r = parse(data)
        except ValueError as e:
            print(f"{f}: {e}", file=sys.stderr)
            ok = False
            continue
        ok &= r["ok"]
        results[str(f)] = r
        if not args.json:
            print_report(f, r)
        if args.extract:
            extract(data, r, args.extract)
    if args.json:
        print(json.dumps(results, indent=2))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
