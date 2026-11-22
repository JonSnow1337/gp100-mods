# OpenGP100 patches

OpenGP100 is by Aleksey Khristolyubov (Reddit: Natural-Mechanic7484), used with his permission - thank you.

`upstream/` holds OpenGP100's source (see `upstream/UPSTREAM.md`); `python -m gp100mods opengp100-build` applies these patches to a
copy of it. To do it by hand, in a copy of `upstream/`:

```sh
cd /path/to/opengp100 && for p in /path/to/gp100-mods/firmware/opengp100/patches/*.patch; do git apply -p1 "$p"; done
```

| Patch | What |
| --- | --- |
| `0001-slot-layout.patch` | link into Valeton slot B (slot header, 1 KiB-aligned vector table), keep `.ncache` in DTCM, no FCB/IVT/DCD |
| `0002-boot-animation.patch` | play a user animation from the reserved flash area at boot (`ui/boot_animation.c`) |
| `0003-starter-preset.patch` | optional built-in "Starter" preset in slot P01 + a feature-flag block the app switches (`app/starter_preset.c`, `app/feature_flags.c`); the app no longer offers it |
| `0004-factory-presets.patch` | up to 99 presets carried in the update file ("GPPR" block after the boot animation, `app/factory_presets.c`); an empty slot shows its preset until something is saved there |
| `0005-fine-master-volume.patch` | optional finer MASTER curve (flag bit 1 of the feature-flag block, `audio_output.c`): `gain = p*p*(p+60)/32`, so MASTER 20 is about as loud as the stock 2 and 100 is unchanged; the app offers it as a checkbox (on by default) |

`valeton_fills.json` is the recipe for restoring Valeton's drum samples and patterns (offsets, lengths, hashes) from the user's
official firmware; the build uses silent placeholders for them.

The patches are diffs against `upstream/`. The build uses the Arm GNU Embedded
Toolchain 10.3, CMake and NXP MCUXpresso SDK 2.11.0 (`--fetch-deps` clones the SDK).
