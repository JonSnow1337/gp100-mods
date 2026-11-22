"""Build-time feature flags stored in a small reserved block of the firmware (see ui/app/feature_flags.h in patch 0003).

Layout (little-endian) at flash 0x1CBF00 = section-0 offset 0x14BF00: "GPFL", u16 version=1, u16 size=0x40, u32 flags, u32 ~flags,
then 0xFF. The complement makes a damaged or erased block read as "no flags". The app switches features on by writing the block.
"""
import struct

MAGIC = b"GPFL"
VERSION = 1
BLOCK_BYTES = 0x40
FLASH_OFFSET = 0x001CBF00
SLOT_BASE = 0x80000
REL = FLASH_OFFSET - SLOT_BASE                  # offset inside section 0

STARTER_PRESET = 1 << 0                         # offer the built-in "Starter" preset in slot P01 while it is empty
FINE_MASTER_VOLUME = 1 << 1                     # finer steps at the quiet end of MASTER (patch 0005)


def build_block(flags: int) -> bytes:
    flags &= 0xFFFFFFFF
    return MAGIC + struct.pack("<HHII", VERSION, BLOCK_BYTES, flags, ~flags & 0xFFFFFFFF) + b"\xff" * (BLOCK_BYTES - 16)


def parse_block(block: bytes) -> int | None:
    """The flags, or None if there is no valid block (an old firmware, or a damaged block)."""
    if len(block) < BLOCK_BYTES or block[:4] != MAGIC:
        return None
    ver, size, flags, inv = struct.unpack_from("<HHII", block, 4)
    if ver != VERSION or size != BLOCK_BYTES or inv != (~flags & 0xFFFFFFFF):
        return None
    return flags


def read(section0: bytes) -> int | None:
    return parse_block(section0[REL:REL + BLOCK_BYTES])


def supports(section0: bytes) -> bool:
    return read(section0) is not None


def supports_presets(section0: bytes) -> bool:
    """True if this OpenGP100 build can show factory presets from the update file (patch 0004 adds this marker string)."""
    return b"GP100-FACTORY-PRESETS-1" in section0[:REL]


def supports_fine_master(section0: bytes) -> bool:
    """True if this OpenGP100 build has the fine-MASTER switch (patch 0005 adds this marker string)."""
    return b"GP100-FINE-MASTER-1" in section0[:REL]
