#!/usr/bin/env python3
"""Verify the firmware links every product-facing core unit."""

from pathlib import Path
import re
import sys


EXPECTED_MODEL_ONLY = {
    "app_runtime.c.obj",
    "audio_dma.c.obj",
    "audio_route.c.obj",
    "device_identity.c.obj",
    "effect_package.c.obj",
    "factory_reset_recovery.c.obj",
    "memory_layout.c.obj",
}


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: check_core_link.py MAP CORE_SOURCE [CORE_SOURCE ...]")
        return 2

    map_path = Path(sys.argv[1])
    linked = set(
        re.findall(
            r"^libgp100_core\.a\(([^)]+)\)",
            map_path.read_text(encoding="utf-8", errors="replace"),
            re.MULTILINE,
        )
    )
    available = {f"{Path(source).name}.obj" for source in sys.argv[2:]}
    missing = available - linked
    unexpected = missing - EXPECTED_MODEL_ONLY
    accidentally_linked = EXPECTED_MODEL_ONLY & linked

    if unexpected or accidentally_linked or missing != EXPECTED_MODEL_ONLY:
        print(f"available={len(available)} linked={len(linked)}")
        if unexpected:
            print("unexpected unlinked units: " + ", ".join(sorted(unexpected)))
        if accidentally_linked:
            print(
                "model-only units unexpectedly linked: "
                + ", ".join(sorted(accidentally_linked))
            )
        return 1

    print(
        f"firmware links {len(linked)} of {len(available)} core units; "
        f"{len(missing)} model-only units remain archive-only"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
