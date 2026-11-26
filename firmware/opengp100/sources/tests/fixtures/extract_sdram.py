#!/usr/bin/env python3
"""Extract the original V2.1 application data used by the host tests."""

import argparse
from pathlib import Path
import struct

FLASH_SIZE = 0x800000
APPLICATION_OFFSET = 0x080000
APPLICATION_HEADER = struct.Struct("<7I4s")
APPLICATION_MARKERS = (0x55555555, 0xAAAAAAAA, 0xA5A5A5A5, b"V210")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flash", type=Path)
    parser.add_argument("sdram", type=Path)
    args = parser.parse_args()

    flash = args.flash.read_bytes()
    if len(flash) != FLASH_SIZE:
        parser.error("expected an 8 MiB flash dump")

    itcm_base, itcm_size, sdram_base, sdram_size, *markers = (
        APPLICATION_HEADER.unpack_from(flash, APPLICATION_OFFSET)
    )
    if tuple(markers) != APPLICATION_MARKERS:
        parser.error("V2.1 application header is missing at flash offset 0x080000")
    sdram_end = sdram_base + sdram_size
    if (itcm_base != APPLICATION_OFFSET + APPLICATION_HEADER.size
            or itcm_size == 0
            or sdram_base != itcm_base + itcm_size
            or sdram_size == 0
            or sdram_end > len(flash)):
        parser.error("V2.1 application data is outside the expected flash layout")

    args.sdram.write_bytes(flash[sdram_base:sdram_end])


if __name__ == "__main__":
    main()
