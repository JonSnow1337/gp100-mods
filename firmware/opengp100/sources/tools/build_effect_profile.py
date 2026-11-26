#!/usr/bin/env python3
"""Resolve a build's pedal names against the preset-compatible C catalog."""

import argparse
import re
from pathlib import Path


FAMILIES = (
    "pre_fx", "distortion", "amplifier", "noise_reduction", "cabinet",
    "equalizer", "modulation", "delay", "reverb",
)
USER_FAMILIES = {"amplifier": "AMP", "cabinet": "Cab"}
LEGACY_EFFECT_NAMES = {
    "rams head": "muff",
    "bigmuffpi": "muff",
    "uo300": "octaver2",
    "bassbals": "balls",
}


def catalog_entries(source, family):
    names = re.search(rf"\b{family}_names\[\]\s*=\s*\{{([^}}]+)\}};", source)
    algorithms = re.search(rf"\b{family}_algorithms\[\]\s*=\s*\{{([^}}]+)\}};", source)
    if names is None or algorithms is None:
        raise ValueError(f"catalog arrays missing for {family}")
    labels = re.findall(r'"([^"\\]*)"|\b(NULL)\b', names[1])
    ids = re.findall(r"UINT32_C\((0x[0-9a-fA-F]+)\)", algorithms[1])
    if len(labels) != len(ids) or not 0 < len(labels) <= 64:
        raise ValueError(f"unexpected catalog layout for {family}")
    user_slot = 0
    for (label, is_user_slot), model_id in zip(labels, ids):
        display_name = label if not is_user_slot else None
        if is_user_slot:
            user_slot += 1
            label = f"{USER_FAMILIES[family]} User {user_slot:02d}"
        yield label, display_name, model_id


def write_if_changed(path, value):
    if not path.exists() or path.read_text() != value:
        path.write_text(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--reduced", action="store_true")
    parser.add_argument("--effects", nargs="*", default=[])
    args = parser.parse_args()
    try:
        source = args.catalog.read_text()
        catalog = {family: list(catalog_entries(source, family)) for family in FAMILIES}
        known = {name.casefold() for entries in catalog.values() for name, _, _ in entries}
        selected = {name.strip().casefold() for name in args.effects}
        selected = {LEGACY_EFFECT_NAMES.get(name, name) for name in selected}
        if selected - known:
            raise ValueError("unknown pedal names: " + ", ".join(sorted(selected - known)))
        masks = {}
        metadata = ["/* Generated catalog entries for the selected models. */"]
        for family, entries in catalog.items():
            masks[family] = sum(1 << i for i, (name, _, _) in enumerate(entries)
                                if not args.reduced or name.casefold() in selected)
            enabled = [(i, label, model_id) for i, (_, label, model_id) in enumerate(entries)
                       if masks[family] & (1 << i)]
            metadata.append(f"static const uint32_t {family}_algorithms[{len(entries)}] = {{")
            metadata.extend([f"    [{i}] = UINT32_C({model_id})," for i, _, model_id in enabled]
                            or ["    0u"])
            metadata.append("};")
            metadata.append(f"static const char *const {family}_names[{len(entries)}] = {{")
            metadata.extend([f'    [{i}] = "{label}",' for i, label, _ in enabled if label is not None]
                            or ["    NULL"])
            metadata.append("};")
        header = ["/* Generated from the build's effect profile. */", "#pragma once",
                  "#include <stdint.h>", f"#define GP100_PROFILE_FULL {int(not args.reduced)}"]
        header.extend(f"#define GP100_PROFILE_{family.upper()} UINT64_C(0x{mask:x})"
                      for family, mask in masks.items())
        args.output.mkdir(parents=True, exist_ok=True)
        write_if_changed(args.output / "gp100_effect_profile.h", "\n".join(header) + "\n")
        write_if_changed(args.output / "gp100_effect_catalog_profile.inc", "\n".join(metadata) + "\n")
        write_if_changed(args.output / "gp100_cabinet_slots.txt", "\n".join(
            str(i + 1) for i in range(40) if masks["cabinet"] & (1 << i)) + "\n")
    except (OSError, ValueError) as error:
        parser.exit(1, f"Effect profile: {error}\n")


if __name__ == "__main__":
    main()
