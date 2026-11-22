#!/usr/bin/env python3
"""Pack editable drum and cabinet sources for memory-mapped flash."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import wave

from compose_factory_assets import ASSET_BANKS

SAMPLE_RATE = 44100
DRUM_COUNT = 13
PATTERN_COUNT = 100
PATTERN_WINDOW = 200
PATTERN_RESTART = 0xFFFF
CABINET_COUNT = 40
CABINET_TAPS = 1024
CABINET_NAME_SIZE = 16


def read_pcm(path: Path, width: int) -> bytes:
    with wave.open(str(path), "rb") as source:
        if (source.getnchannels(), source.getsampwidth(), source.getframerate()) != (
                1, width, SAMPLE_RATE):
            raise ValueError(f"{path} must be mono {width * 8}-bit PCM at {SAMPLE_RATE} Hz")
        frames = source.getnframes()
        pcm = source.readframes(frames)
    if frames == 0 or len(pcm) != frames * width:
        raise ValueError(f"{path} has missing PCM samples")
    return pcm


def pack_drums(directory: Path) -> bytes:
    offsets = []
    lengths = []
    pcm_bank = bytearray()
    header_size = 4 * (1 + 2 * DRUM_COUNT)
    for index in range(1, DRUM_COUNT + 1):
        pcm = read_pcm(directory / f"{index:02}.wav", 2)
        # One stored length unit is 44 samples. Playback uses whole 64-sample
        # blocks. Keep the source tail and pad if a trimmed WAV needs it.
        units = (len(pcm) // 2 + 43) // 44
        played_bytes = (units * 44 // 64) * 64 * 2
        if played_bytes == 0:
            raise ValueError(f"drum {index:02} is too short for one playback block")
        offsets.append(header_size + len(pcm_bank))
        lengths.append(units)
        pcm_bank.extend(pcm.ljust(played_bytes, b"\0"))
    return struct.pack(f"<{1 + 2 * DRUM_COUNT}I", DRUM_COUNT, *offsets, *lengths) + pcm_bank


def pack_patterns(source: bytes) -> bytes:
    if len(source) % 2:
        raise ValueError("compiled rhythm pattern data must contain 16-bit words")
    words = struct.unpack(f"<{len(source) // 2}H", source)
    position = 0
    offsets = []
    events = bytearray()
    header_size = 4 * (1 + PATTERN_COUNT)
    for index in range(PATTERN_COUNT):
        if position == len(words):
            raise ValueError(f"missing rhythm pattern {index + 1}")
        count = words[position]
        pattern = words[position + 1:position + 1 + count]
        if (not 1 <= count <= PATTERN_WINDOW or len(pattern) != count
                or pattern[-1] != PATTERN_RESTART):
            raise ValueError(f"rhythm pattern {index + 1} must have 1–200 words and end in restart")
        for event in pattern[:-1]:
            # A waiting event has its high bit clear. A trigger encodes a
            # sample index and a seven-bit gain.
            if event >= 0x8000:
                sample_index = (event - 0x8000) >> 8
                if sample_index >= DRUM_COUNT or event & 0x80:
                    raise ValueError(f"invalid trigger {event:#06x} in rhythm pattern {index + 1}")
        offsets.append(header_size + len(events))
        events.extend(struct.pack(f"<{count}H", *pattern))
        position += 1 + count
    if position != len(words):
        raise ValueError(f"expected exactly {PATTERN_COUNT} rhythm patterns")

    # The reader always exposes a 200-word window from each offset, even
    # though playback restarts before the end of that window.
    last_window_end = offsets[-1] + PATTERN_WINDOW * 2
    header = struct.pack(f"<{1 + PATTERN_COUNT}I", PATTERN_COUNT, *offsets)
    return (header + events).ljust(last_window_end, b"\0")


def pack_cabinets(directory: Path, names: bytes) -> bytes:
    if len(names) != CABINET_NAME_SIZE * CABINET_COUNT:
        raise ValueError(f"expected {CABINET_COUNT} fixed-width cabinet names")
    bank = bytearray()
    for index in range(CABINET_COUNT):
        slot = index + 1
        wavs = list(directory.glob(f"{slot:02d}-*.wav"))
        if len(wavs) != 1:
            raise ValueError(f"expected exactly one WAV for cabinet slot {slot:02d}")
        pcm = read_pcm(wavs[0], 3)
        if len(pcm) != CABINET_TAPS * 3:
            raise ValueError(f"{wavs[0]} must contain exactly {CABINET_TAPS} taps")
        name_offset = index * CABINET_NAME_SIZE
        bank.extend(names[name_offset:name_offset + CABINET_NAME_SIZE])
        bank.extend(bytes(16))
        for offset in range(0, len(pcm), 3):
            tap = int.from_bytes(pcm[offset:offset + 3], "little", signed=True)
            bank.extend(struct.pack("<i", tap))
    return bank


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assets", type=Path)
    parser.add_argument("patterns", type=Path)
    parser.add_argument("cabinet_names", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    try:
        banks = {
            "rhythm_samples.bin": pack_drums(args.assets / "rhythm"),
            "rhythm_patterns.bin": pack_patterns(args.patterns.read_bytes()),
            "factory_cabinets.bin": pack_cabinets(args.assets / "cabinets", args.cabinet_names.read_bytes()),
        }
        for name, _, size in ASSET_BANKS:
            if len(banks[name]) > size:
                raise ValueError(f"{name} exceeds its {size}-byte flash allocation")
        args.output.mkdir(parents=True, exist_ok=True)
        for name, _, size in ASSET_BANKS:
            (args.output / name).write_bytes(banks[name].ljust(size, b"\0"))
    except (ValueError, OSError, wave.Error) as error:
        parser.exit(1, f"factory asset packing failed: {error}\n")


if __name__ == "__main__":
    main()
