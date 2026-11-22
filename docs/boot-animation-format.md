# GPBA boot animation format (version 1)

Stored at flash offset `0x1CC000`, at most 1 MiB, inside section 0 of the update file (section-0 offset `0x14C000`).
Little-endian. Reference implementations: `src/gp100mods/bootanim/format.py` (writer/reader) and
`ui/boot_animation.c` in patch 0002 (firmware).

| Offset | Size | Field |
| --- | --- | --- |
| 0x00 | 4 | magic `GPBA` |
| 0x04 | 2 | version = 1 |
| 0x06 | 2 | header size = 0x20 |
| 0x08 | 2 | width = 160 |
| 0x0A | 2 | height = 128 |
| 0x0C | 2 | frame count (0 = none installed -> built-in logo) |
| 0x0E | 2 | palette entries (1..256) |
| 0x10 | 4 | total size in bytes |
| 0x14 | 4 | checksum: sum of bytes 0x20..total, mod 2^32 |
| 0x18 | 8 | reserved |
| 0x20 | 2 x n | RGB565 palette |
| ... | 8 x frames | table: `u32 offset, u16 duration_ms, u8 encoding, u8 reserved` |
| ... | | frame data: encoding 0 = raw 8-bit indices (w*h); 1 = RLE of `(count 1..255, index)` pairs |

Playback is time-driven (late frames are skipped) and limited to 10 s in total. Anything that fails validation
(including a bad checksum) falls back to the built-in logo.
