"""Cross-check the firmware's C animation parser against the Python writer. Needs gcc and a patched OpenGP100 tree
(set GP100_FW_TREE to the directory containing sources/)."""
import os
import random
import shutil
import subprocess
from pathlib import Path

import pytest

from gp100mods.bootanim import format as F

TREE = os.environ.get("GP100_FW_TREE")
pytestmark = pytest.mark.skipif(not (TREE and shutil.which("gcc")), reason="GP100_FW_TREE and gcc required")


def expected_crc(anim):
    return sum(sum(v * (k % 251 + 1) for k, v in enumerate(idx)) + i * len(idx) for i, (idx, _) in enumerate(anim.frames))


def test_c_parser_matches_python(tmp_path):
    ui = Path(TREE) / "sources" / "ui"
    exe = tmp_path / "t"
    subprocess.run(["gcc", "-O1", f"-I{ui}", "-o", str(exe), str(Path(__file__).parent / "c" / "test_boot_anim.c"),
                    str(ui / "boot_animation.c")], check=True)
    random.seed(1)
    px = F.WIDTH * F.HEIGHT
    anim = F.Animation(palette=list(range(64)), frames=[
        (bytes(random.randrange(64) for _ in range(px)), 40), (bytes([5]) * px, 100),
        (bytes((k // 160) % 7 for k in range(px)), 60)])
    f = tmp_path / "a.gpba"; f.write_bytes(F.build_area(anim))
    out = subprocess.run([str(exe), str(f)], capture_output=True, text=True).stdout.strip()
    assert out == f"OK frames=3 total_ms=200 crc={expected_crc(anim)}"
    bad = bytearray(f.read_bytes()); bad[100] ^= 0xFF; f.write_bytes(bad)
    assert subprocess.run([str(exe), str(f)], capture_output=True, text=True).stdout.strip() == "REJECTED"


LIB = os.environ.get("GP100_FW_LIB")          # libgp100_core.a built from the same tree (host-check build)


@pytest.mark.skipif(not (TREE and LIB and shutil.which("gcc")), reason="GP100_FW_TREE, GP100_FW_LIB and gcc required")
def test_starter_preset_and_feature_flags(tmp_path):
    from gp100mods import features
    src = Path(TREE) / "sources"
    incs = [f"-I{d}" for d in sorted({str(p.parent) for p in src.rglob("*.h")} | {str(src)}) if "/tests" not in d and "/build" not in d]
    exe = tmp_path / "t"
    subprocess.run(["gcc", "-O1", *incs, "-o", str(exe), str(Path(__file__).parent / "c" / "test_starter.c"),
                    str(src / "app" / "starter_preset.c"), str(src / "app" / "feature_flags.c"), LIB, "-lm"], check=True)
    out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.splitlines()
    assert out[0] == "starter native=1 blocks=5"
    assert out[1:] == ["block 0 Distortion/Green OD enabled=1 fs=1", "block 1 Amplifier/Bellman 59N enabled=1 fs=0",
                       "block 2 Cabinet/Bellman 4x10 enabled=1 fs=0", "block 3 Delay/Slapbk enabled=0 fs=2",
                       "block 4 Reverb/Spring enabled=1 fs=0"]
    damaged = bytearray(features.build_block(1)); damaged[13] ^= 1
    for blob, want in ((features.build_block(1), 1), (features.build_block(0), 0), (features.build_block(0xA5), 0xA5),
                       (bytes(damaged), 0), (b"\xff" * 64, 0)):
        f = tmp_path / "b.bin"; f.write_bytes(blob)
        assert subprocess.run([str(exe), str(f)], capture_output=True, text=True).stdout.strip() == f"flags={want}"


@pytest.mark.skipif(not (TREE and LIB and shutil.which("gcc")), reason="GP100_FW_TREE, GP100_FW_LIB and gcc required")
def test_factory_presets_are_accepted_by_the_firmware(tmp_path):
    from gp100mods import presets as P
    from test_presets import make_prst
    src = Path(TREE) / "sources"
    incs = [f"-I{d}" for d in sorted({str(p.parent) for p in src.rglob("*.h")} | {str(src)}) if "/tests" not in d and "/build" not in d]
    exe = tmp_path / "t"
    subprocess.run(["gcc", "-O1", *incs, "-o", str(exe), str(Path(__file__).parent / "c" / "test_factory_presets.c"),
                    str(src / "app" / "factory_presets.c"), LIB, "-lm"], check=True)
    ps = P.load_prst(make_prst(tmp_path / "a.prst", count=3))
    blob = tmp_path / "b.bin"
    blob.write_bytes(P.build_block([ps[0].record, None, ps[2].record]))
    out = subprocess.run([str(exe), str(blob)], capture_output=True, text=True, check=True).stdout.splitlines()
    assert out[0] == "count 3" and out[1].startswith("slot 0 ok") and out[2] == "slot 1 rejected" and out[3].startswith("slot 2 ok")
    bad = bytearray(blob.read_bytes()); bad[P.HEADER_BYTES + 5] ^= 0xFF           # damaged record -> that slot is rejected
    blob.write_bytes(bytes(bad))
    assert subprocess.run([str(exe), str(blob)], capture_output=True, text=True).stdout.splitlines()[1] == "slot 0 rejected"


def test_fine_master_curve(tmp_path):
    src = Path(TREE) / "sources"
    exe = tmp_path / "t"
    subprocess.run(["gcc", "-O1", f"-I{src}", "-o", str(exe), str(Path(__file__).parent / "c" / "test_fine_master.c"),
                    str(src / "audio" / "engine" / "audio_output.c"), str(src / "protocol" / "usb" / "usb_audio_level.c"), "-lm"],
                   check=True)
    assert subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.strip() == "ok"
