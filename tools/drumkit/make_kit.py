#!/usr/bin/env python3
"""Step 2: put the 13 WAVs from make_samples.py into the slot order Valeton's rhythm patterns expect, level-matched, and write
src/gp100mods/data/drums/kit/NN-name.wav.   usage: make_kit.py <make_samples output dir> <kit dir>

The patterns are Valeton's (restored from the user's official file); they only use slots 1-9. What each slot is was worked out from
where the patterns trigger it (kick on beats 1 and 3, snare on 2 and 4, hats on every eighth, ...) and from the sound of the original
sample, so it is an inference, not documentation. Slots 10-13 are not used by any pattern."""
import sys, wave
from pathlib import Path
import numpy as np

# slot: (make_samples slot, file name, gain). The gain matches the loudness of the first 250 ms to Valeton's sample in that slot
# (so the patterns keep their balance); slots 1, 2 and 6 are limited by the peak (31000 of 32767).
SLOTS = {1: (1, "kick-electronic", 0.995856), 2: (1, "kick", 0.995856), 3: (13, "snare-electronic", 0.399049),
         4: (7, "tom-high", 0.769508), 5: (8, "tom-mid", 0.563012), 6: (2, "snare", 0.995856),
         7: (4, "hihat-closed", 0.685839), 8: (6, "hihat-alt", 1.0), 9: (5, "hihat-open", 1.0),
         10: (10, "crash", 1.0), 11: (11, "ride", 1.0), 12: (12, "ride-bell", 1.0), 13: (3, "sidestick", 1.0)}

src, out = Path(sys.argv[1]), Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
for slot, (n, name, gain) in SLOTS.items():
    with wave.open(str(src / f"{n:02}.wav")) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(float)
    y = np.round(x * gain).astype("<i2")
    with wave.open(str(out / f"{slot:02}-{name}.wav"), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes(y.tobytes())
