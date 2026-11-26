#!/usr/bin/env python3
"""Add audio assets to a standalone flash image."""

from __future__ import annotations

import argparse
import os
import re
from pathlib import Path

FLASH_SIZE = 0x00800000


def read_flash_offset(name: str) -> int:
    layout = Path(__file__).resolve().parents[1] / "storage" / "flash_layout.h"
    match = re.search(rf"^#define\s+{name}\s+(0x[0-9a-fA-F]+)u$", layout.read_text(), re.MULTILINE)
    if match is None:
        raise SystemExit(f"missing flash offset {name} in {layout}")
    return int(match[1], 16)


USER_DATA_START = read_flash_offset("GP100_USER_DATA_START")
ASSET_BANKS = (
    ("rhythm_samples.bin", 0x00133DDC, 0x935E4),
    ("rhythm_patterns.bin", 0x001C7414, 0x447E),
    ("factory_cabinets.bin", 0x00680000, 40 * 0x1020),
)
FACTORY_IMAGE_END = max(offset + size for _, offset, size in ASSET_BANKS)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"factory asset composition failed: {message}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("assets", type=Path)
    args = parser.parse_args()

    image = bytearray(args.image.read_bytes())
    require(FACTORY_IMAGE_END <= USER_DATA_START < FLASH_SIZE,
            "factory image exceeds the firmware region")

    previous_end = len(image)
    image.extend(b"\xff" * (FACTORY_IMAGE_END - len(image)))
    for name, offset, size in ASSET_BANKS:
        require(offset >= previous_end, f"{name} overlaps earlier image data")
        bank = (args.assets / name).read_bytes()
        require(len(bank) == size, f"{name} must be {size:#x} bytes")
        image[offset:offset + size] = bank
        previous_end = offset + size

    temporary = args.image.with_name(args.image.name + ".tmp")
    temporary.write_bytes(image)
    os.replace(temporary, args.image)
    print("Added rhythm and cabinet banks. "
          f"Image size is {len(image):#x} bytes.")


if __name__ == "__main__":
    main()
