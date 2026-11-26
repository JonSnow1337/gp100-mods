#!/usr/bin/env python3
"""CrocellKit (CC BY 4.0) -> 13 mono 16-bit 44.1 kHz drum WAVs for the GP-100 drum bank.
Usage: make_samples.py <CrocellKit_Stereo_MIX dir> <out dir>"""
import re, sys, wave
from pathlib import Path
import numpy as np
from scipy.signal import resample_poly

KIT = Path(sys.argv[1]); OUT = Path(sys.argv[2]); OUT.mkdir(parents=True, exist_ok=True)
# slot, instrument folder, seconds kept, peak (fraction of full scale), fade fraction
SLOTS = [
    (1, "KDrumL",        0.30, 0.95),
    (2, "Snare",         0.45, 0.95),
    (3, "SnareRim",      0.20, 0.90),
    (4, "HihatClosed",   0.16, 0.85),
    (5, "HihatOpen",     0.50, 0.85),
    (6, "HihatPedal",    0.14, 0.80),
    (7, "Tom1",          0.55, 0.95),
    (8, "Tom2",          0.60, 0.95),
    (9, "FTom1",         0.75, 0.95),
    (10, "CrashL",       1.50, 0.90),
    (11, "RideR",        0.85, 0.85),
    (12, "RideRBell",    0.50, 0.85),
    (13, "SnareRimShot", 0.28, 0.95),
]
def read24(p):
    w = wave.open(str(p)); n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    raw = np.frombuffer(w.readframes(n), np.uint8).reshape(n, ch, sw)
    v = (raw[..., 0].astype(np.int32) | raw[..., 1].astype(np.int32) << 8 | raw[..., 2].astype(np.int32) << 16)
    v = np.where(v >= 1 << 23, v - (1 << 24), v) / float(1 << 23)
    return v, w.getframerate()
def best_sample(folder):
    xml = (KIT / folder / f"{folder}.xml").read_text(encoding="utf-8-sig")
    items = re.findall(r'<sample name="[^"]*" power="([0-9.eE+-]+)">\s*<audiofile[^>]*file="samples/([^"]+)"', xml)
    p, f = max(((float(a), b) for a, b in items))
    return f, p, len(items)
tot = 0
for slot, folder, secs, peak in SLOTS:
    f, power, n = best_sample(folder)
    x, sr = read24(KIT / folder / "samples" / f)
    m = x[:, :2].mean(axis=1)                       # the two ambience/overhead mix channels -> mono
    m = resample_poly(m, 147, 160) if sr == 48000 else m
    m -= m[:200].mean() if len(m) > 400 else 0
    env = np.abs(m); thr = env.max() * 0.02
    on = int(np.argmax(env > thr)); on = max(0, on - 44)   # start ~1 ms before the hit
    m = m[on:]
    n_keep = int(secs * 44100)
    m = m[:n_keep]
    fade = int(len(m) * 0.45)                      # smooth tail instead of a hard cut
    m[-fade:] *= np.cos(np.linspace(0, np.pi / 2, fade)) ** 2
    m[:32] *= np.linspace(0, 1, 32)
    m *= peak / np.abs(m).max()
    pcm = np.round(m * 32767).astype("<i2")
    with wave.open(str(OUT / f"{slot:02}.wav"), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes(pcm.tobytes())
    tot += len(pcm)
    print(f"{slot:02} {folder:14s} {f:20s} power {power:.3f} (max of {n}) -> {len(pcm)/44100:.2f}s, rms {np.sqrt((m**2).mean()):.3f}")
print("total samples", tot, "budget", (603620 - 108) // 2)
