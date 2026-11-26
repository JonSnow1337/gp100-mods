# OpenGP100 in this repo

`sources/` is the OpenGP100 source (see [UPSTREAM.md](UPSTREAM.md)) with our changes committed directly on top. There are no
patch files; edit the code like any other. Commit `b915f23` is his tree as received, so this shows what we changed:

```sh
git diff b915f23 -- firmware/opengp100/sources
```

## What we changed

| Change | Where | What |
| --- | --- | --- |
| boot animation | `ui/boot_animation.[ch]`, `app/application.c` | play a user animation from the reserved flash area (`0x1CC000`) at boot; none installed = his built-in logo |
| feature flags | `app/feature_flags.[ch]` | a small flag block at `0x1CBF00` that the app switches (all off in a fresh build) |
| starter preset | `app/starter_preset.[ch]`, `app/application.c` | optional built-in "Starter" preset in slot P01 (flag bit 0); the app no longer offers it |
| factory presets | `app/factory_presets.[ch]`, `app/application.c` | up to 99 presets carried in the update file ("GPPR" block after the boot animation); an empty slot shows its preset until something is saved there |
| fine master volume | `audio/engine/audio_output.[ch]`, `audio/engine/audio_binding.c` | optional finer MASTER curve (flag bit 1): `gain = p*p*(p+60)/32`, so MASTER 20 is about as loud as the stock 2 and 100 is unchanged. Runs before his output limiter |
| build wiring | `cmake/firmware.cmake` | the four new `.c` files are part of both of his targets |

Slot placement is his: the `gp100_stock_slot` target links into Valeton's bootloader slot and builds the Valeton update package
(see [docs/opengp100-port.md](../../docs/opengp100-port.md)).

## Build

`python -m gp100mods opengp100-build` builds the `gp100_stock_slot` target with the official V2.1 file as the template, with
silent placeholder audio, then `gp100mods.opengp100.finish` adds the app's reserved areas (feature flags, empty boot animation).

To work on the firmware alone, use the Makefile in `sources/` (needs NXP SDK 2.11.0, the NXP USB middleware and Arm GCC 10.3;
paths must not contain spaces). `make host-check REFERENCE_FLASH=<flash dump>` runs the host tests; the dump is in his archive, not here.

## Taking a new version from the author

1. Replace `sources/` with his new tree, minus what [UPSTREAM.md](UPSTREAM.md) lists as not included. Commit it on its own.
2. Re-apply our commits on top (`git log b915f23..` lists them), rebuild, run the tests.

`valeton_fills.json` is the recipe for restoring Valeton's drum samples and patterns (offsets, lengths, hashes) from the user's
official firmware.
