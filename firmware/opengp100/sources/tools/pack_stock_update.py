#!/usr/bin/env python3
"""Build the GP-100 stock application slot and an optional v2.1 update package."""

import argparse
import struct
from pathlib import Path

from compose_factory_assets import ASSET_BANKS


CHECKSUM_OFFSET = 0x0E
TABLE_OFFSET = 0x30
SECTION_SIZES = (0x29FAEC, 0x4000, 0x28500, 0x78000)
SLOT_OFFSET = 0x080000
SLOT_HEADER_SIZE = 0x20
FLASH_BASE = 0x60000000
XIP_VECTOR_OFFSET = 0x083000
TRAMPOLINE_FLASH_OFFSET = 24
CABINET_BANK = "factory_cabinets.bin"
SLOT_HEADER = struct.pack(
    "<7I4s", 0x080020, 0x34951, 0x0B4971, 0x26B179,
    0x55555555, 0xAAAAAAAA, 0xA5A5A5A5, b"V210",
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sections(package):
    require(len(package) >= TABLE_OFFSET + 0x20 and package[:4] == b"FRMW",
            "expected a Valeton FRMW package")
    size, = struct.unpack_from("<I", package, 4)
    require(size == len(package) - TABLE_OFFSET and package[0x10:0x16] == b"GP100\0",
            "invalid GP-100 package header")
    require(package[CHECKSUM_OFFSET] == sum(memoryview(package)[TABLE_OFFSET:]) % 256,
            "invalid FRMW payload checksum")

    ranges = []
    expected_offset = 0x20
    for index, expected_size in enumerate(SECTION_SIZES):
        offset, size = struct.unpack_from("<II", package, TABLE_OFFSET + 8 * index)
        require((offset, size) == (expected_offset, expected_size),
                f"unexpected v2.1 section {index + 1} layout")
        start = TABLE_OFFSET + offset
        ranges.append(slice(start, start + size))
        expected_offset += size
    require(ranges[-1].stop == len(package), "package length disagrees with section table")
    return ranges


def build_slot(payload, assets):
    vector_index = XIP_VECTOR_OFFSET - SLOT_OFFSET - SLOT_HEADER_SIZE
    require(len(payload) >= vector_index + 8, "slot payload is missing XIP vectors")
    stack, entry = struct.unpack_from("<2I", payload)
    xip_stack, xip_entry = struct.unpack_from("<2I", payload, vector_index)
    redirect, reset = struct.unpack_from("<2I", payload, 16)
    require(entry == 9 and stack == xip_stack and 0x20000000 < stack <= 0x20020000,
            "invalid ITCM trampoline vector")
    flash_start = FLASH_BASE + SLOT_OFFSET + SLOT_HEADER_SIZE + TRAMPOLINE_FLASH_OFFSET
    require(redirect == (flash_start | 1) and
            reset == xip_entry and xip_entry & 1 and
            FLASH_BASE + XIP_VECTOR_OFFSET < xip_entry <
            FLASH_BASE + SLOT_OFFSET + SLOT_HEADER_SIZE + len(payload),
            "invalid flash startup or relocated SDK reset handler address")

    slot = bytearray(b"\xff" * SECTION_SIZES[0])
    payload_end = SLOT_HEADER_SIZE + len(payload)
    require(payload_end <= ASSET_BANKS[0][1] - SLOT_OFFSET, "payload overlaps rhythm assets")
    slot[:SLOT_HEADER_SIZE] = SLOT_HEADER
    slot[SLOT_HEADER_SIZE:payload_end] = payload

    for name, offset, size in ASSET_BANKS:
        if name == CABINET_BANK:
            continue
        start = offset - SLOT_OFFSET
        require(payload_end <= start and start + size <= len(slot),
                f"{name} falls outside the asset area in the stock application slot")
        slot[start:start + size] = read_bank(assets, name, size)
    return slot


def read_bank(assets, name, size):
    bank = (assets / name).read_bytes()
    require(len(bank) == size, f"{name} must be {size:#x} bytes")
    return bank


def build_package(template, slot, assets):
    app, _, cabinets, _ = sections(template)
    require(template[app.start:app.start + SLOT_HEADER_SIZE] == SLOT_HEADER,
            "unexpected bootloader application header")
    package = bytearray(template)
    for name, _, size in ASSET_BANKS:
        if name == CABINET_BANK:
            require(size == cabinets.stop - cabinets.start, "invalid cabinet section size")
            package[cabinets] = read_bank(assets, name, size)
            break

    package[app] = slot
    # The Valeton editor compares the low byte of the sum from the section
    # table through the end of the file to the header's checksum field.
    package[CHECKSUM_OFFSET] = sum(memoryview(package)[TABLE_OFFSET:]) % 256
    return package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("assets", type=Path)
    parser.add_argument("raw_slot", type=Path)
    parser.add_argument("--template", type=Path,
                        help="use the official Valeton v2.1 package as a USB update template")
    parser.add_argument("--update", type=Path, help="output path for the USB update package")
    args = parser.parse_args()

    if (args.template is None) != (args.update is None):
        parser.error("--template and --update must be supplied together")
    paths = [args.payload, args.raw_slot]
    if args.template is not None:
        paths.extend([args.template, args.update])
    if len({path.resolve() for path in paths}) != len(paths):
        parser.error("input and output paths must differ")
    try:
        slot = build_slot(args.payload.read_bytes(), args.assets)
        if args.template is not None:
            package = build_package(args.template.read_bytes(), slot, args.assets)
            write_binary(args.update, package)
        write_binary(args.raw_slot, slot)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Stock slot build failed: {error}\n")


def write_binary(path, contents):
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(contents)
    temporary.replace(path)


if __name__ == "__main__":
    main()
