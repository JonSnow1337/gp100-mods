#!/usr/bin/env python3
"""Sanity-check the standalone i.MX RT FlexSPI image."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from compose_factory_assets import ASSET_BANKS, FACTORY_IMAGE_END, FLASH_SIZE, USER_DATA_START

FLASH_BASE = 0x60000000
IVT_OFFSET = 0x1000
DCD_OFFSET = 0x1030
DCD_SIZE = 0x430
VECTOR_OFFSET = 0x2000


def u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"image check failed: {message}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("boot_fixtures", type=Path)
    parser.add_argument("--assets", type=Path)
    args = parser.parse_args()

    image = args.image.read_bytes()

    require(VECTOR_OFFSET + 8 <= len(image) <= USER_DATA_START,
            f"image size {len(image):#x} exceeds the firmware region or lacks a vector table")
    require(image[:0x200] == (args.boot_fixtures / "flexspi_config.bin").read_bytes(),
            "FlexSPI configuration block differs from reference")
    require(image[DCD_OFFSET:DCD_OFFSET + DCD_SIZE] ==
            (args.boot_fixtures / "sdram_dcd.bin").read_bytes(),
            "DCD differs from reference")

    ivt = tuple(u32(image, IVT_OFFSET + i * 4) for i in range(8))
    require(ivt[0] == 0x412000D1, f"bad IVT header {ivt[0]:#010x}")
    require(ivt[1] == FLASH_BASE + VECTOR_OFFSET,
            f"entry is {ivt[1]:#010x}, expected vector table")
    require(ivt[2] == 0 and ivt[7] == 0, "IVT reserved fields are nonzero")
    require(ivt[3] == FLASH_BASE + DCD_OFFSET, "bad DCD pointer")
    require(ivt[4] == FLASH_BASE + IVT_OFFSET + 0x20, "bad boot-data pointer")
    require(ivt[5] == FLASH_BASE + IVT_OFFSET, "bad IVT self pointer")
    require(ivt[6] == 0, "unexpected CSF pointer")

    boot_start, boot_size, plugin, _ = struct.unpack_from("<4I", image, IVT_OFFSET + 0x20)
    require(boot_start == FLASH_BASE, "bad boot-data start")
    require(boot_size == FLASH_SIZE, "boot-data does not cover the 8 MiB flash")
    require(plugin == 0, "image is unexpectedly marked as a plugin")

    initial_sp = u32(image, VECTOR_OFFSET)
    reset = u32(image, VECTOR_OFFSET + 4)
    require(0x20000000 <= initial_sp <= 0x20020000,
            f"initial SP {initial_sp:#010x} is outside DTCM")
    require((reset & 1) == 1, "Reset_Handler is not a Thumb address")
    require(FLASH_BASE + VECTOR_OFFSET <= (reset & ~1) < FLASH_BASE + len(image),
            f"Reset_Handler {reset:#010x} is outside flash")

    if args.assets is not None:
        require(len(image) == FACTORY_IMAGE_END,
                "composed image size differs from the end of the audio banks")
        for name, offset, size in ASSET_BANKS:
            require(image[offset:offset + size] == (args.assets / name).read_bytes(),
                    f"{name} differs at flash offset {offset:#x}")

    print(f"OK: {args.image} ({len(image)} bytes), FCB/DCD match reference")


if __name__ == "__main__":
    main()
