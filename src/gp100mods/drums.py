"""The drum kit: 13 samples for OpenGP100's rhythm player, built from editable WAV files.

  data/drums/kit/NN-name.wav    13 mono 16-bit 44.1 kHz WAVs, NN = slot 01..13

Only the sounds are ours. The 100 rhythm patterns stay Valeton's (restored from the user's official file), so each slot must keep the
role the patterns expect (see docs/drums.md). `load_kit()` packs the WAVs into the flash bank the firmware reads (OpenGP100's format),
and the result is parsed back the way the firmware does it, because a bank the firmware rejects stops the pedal from starting."""
import struct
import wave
from dataclasses import dataclass
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent / "data" / "drums"
SAMPLE_RATE = 44100
SLOTS = 13
SAMPLE_BANK_SIZE = 0x935E4                  # flash allocation of the V2.1 layout


class KitError(ValueError):
    """The drum files cannot be turned into a bank the firmware accepts (the message says why, in plain words)."""


@dataclass
class Kit:
    samples: bytes          # packed sample bank, SAMPLE_BANK_SIZE bytes
    sample_bytes: int       # used

    @property
    def summary(self) -> str:
        return f"kit {self.sample_bytes // 1024} KB of {SAMPLE_BANK_SIZE // 1024} KB"


def read_wav(path) -> bytes:
    try:
        with wave.open(str(path), "rb") as w:
            if (w.getnchannels(), w.getsampwidth(), w.getframerate()) != (1, 2, SAMPLE_RATE):
                raise KitError(f"{Path(path).name} must be mono, 16-bit, {SAMPLE_RATE} Hz")
            frames = w.getnframes(); pcm = w.readframes(frames)
    except (wave.Error, EOFError) as e:
        raise KitError(f"{Path(path).name} is not a usable WAV file ({e})") from e
    if len(pcm) != frames * 2 or frames == 0:
        raise KitError(f"{Path(path).name} has no audio")
    return pcm


def pack_samples(pcms) -> bytes:
    """13 PCM byte strings -> the sample bank. A stored length unit is 44 samples; the player plays whole 64-sample blocks."""
    if len(pcms) != SLOTS:
        raise KitError(f"the kit needs exactly {SLOTS} samples, not {len(pcms)}")
    header = 4 * (1 + 2 * SLOTS); offsets, lengths, bank = [], [], bytearray()
    for i, pcm in enumerate(pcms, 1):
        units = (len(pcm) // 2 + 43) // 44
        played = (units * 44 // 64) * 64 * 2
        if played == 0:
            raise KitError(f"sample {i:02d} is too short (it must have at least 64 samples)")
        offsets.append(header + len(bank)); lengths.append(units)
        bank.extend(pcm.ljust(played, b"\0"))
    out = struct.pack(f"<{1 + 2 * SLOTS}I", SLOTS, *offsets, *lengths) + bytes(bank)
    if len(out) > SAMPLE_BANK_SIZE:
        raise KitError(f"the kit is {len(out) - SAMPLE_BANK_SIZE} bytes too big ({len(out)} bytes, the pedal has room for "
                       f"{SAMPLE_BANK_SIZE}); shorten some samples")
    return out


def verify(samples: bytes) -> None:
    """Parse the bank like gp100_rhythm_assets_parse (firmware) does. Raises KitError."""
    u32 = lambda b, o: struct.unpack_from("<I", b, o)[0]
    if len(samples) != SAMPLE_BANK_SIZE:
        raise KitError("the sample bank has the wrong size")
    if u32(samples, 0) != SLOTS:
        raise KitError("the sample bank has the wrong entry count")
    sh = 4 * (1 + 2 * SLOTS)
    for i in range(SLOTS):
        off, raw = u32(samples, 4 * (1 + i)), u32(samples, 4 * (1 + SLOTS + i))
        blocks = (raw * 44) >> 6
        if blocks > 0xFFFF or blocks == 0 or off < sh or off % 2 or off + blocks * 128 > len(samples):
            raise KitError(f"sample {i + 1}: bad position or length")


def load_kit(directory=None) -> Kit:
    d = Path(directory) if directory else DATA_DIR
    wavs = []
    for slot in range(1, SLOTS + 1):
        found = sorted((d / "kit").glob(f"{slot:02d}-*.wav"))
        if len(found) != 1:
            raise KitError(f"expected exactly one kit/{slot:02d}-*.wav, found {len(found)}")
        wavs.append(read_wav(found[0]))
    samples = pack_samples(wavs)
    used = len(samples)
    samples = samples.ljust(SAMPLE_BANK_SIZE, b"\0")
    verify(samples)
    return Kit(samples, used)
