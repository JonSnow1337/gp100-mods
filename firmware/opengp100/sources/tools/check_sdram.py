#!/usr/bin/env python3
"""Validate the semantic SDRAM sections in the linked base firmware."""

import argparse
import subprocess


SDRAM_START = 0x80000000
SDRAM_END = 0x82000000
SYMBOLS = (
    "__sdram_code_start__",
    "__sdram_code_end__",
    "__sdram_bss_start__",
    "__sdram_bss_end__",
    "__sdram_noinit_start__",
    "__sdram_noinit_end__",
)


def read_symbols(nm: str, elf: str) -> dict[str, int]:
    output = subprocess.check_output([nm, "-n", elf], text=True)
    values: dict[str, int] = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[2] in SYMBOLS:
            values[fields[2]] = int(fields[0], 16)
    missing = [symbol for symbol in SYMBOLS if symbol not in values]
    if missing:
        raise ValueError("missing linker symbols: " + ", ".join(missing))
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf")
    parser.add_argument("nm")
    args = parser.parse_args()
    values = read_symbols(args.nm, args.elf)

    ordered = [values[symbol] for symbol in SYMBOLS]
    if ordered != sorted(ordered):
        raise ValueError("SDRAM sections overlap or are out of order")
    if ordered[0] != SDRAM_START or ordered[-1] > SDRAM_END:
        raise ValueError("SDRAM sections fall outside the 32 MiB SEMC range")
    if any(value % 32 != 0 for value in ordered):
        raise ValueError("SDRAM section boundary is not cache-line aligned")

    print(
        "SDRAM layout OK: "
        f"code={ordered[1] - ordered[0]} bytes, "
        f"bss={ordered[3] - ordered[2]} bytes, "
        f"noinit={ordered[5] - ordered[4]} bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
