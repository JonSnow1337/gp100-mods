# gp100-mods

A desktop app that builds firmware for the Valeton GP-100 pedal: **OpenGP100** (recommended) or Valeton's original firmware
with a few mods. You install the result with the **official GP-100 app**; no J-Link, no compiler. Valeton's bootloader and
footswitch+PARA recovery stay untouched.

<p>
  <img src="docs/img/screen-main.jpg" height="240" alt="OpenGP100 main screen">
  <img src="docs/img/screen-signal-chain.jpg" height="240" alt="OpenGP100 signal chain editor">
</p>

> **Unofficial.** Not affiliated with Valeton. Flashing can leave a device unusable; read [docs/recovery.md](docs/recovery.md)
> first. Every firmware update erases the presets on the pedal.

## Why OpenGP100 (recommended)

OpenGP100 is a community firmware written from scratch by Aleksey Khristolyubov (Reddit: Natural-Mechanic7484). Compared to
the original firmware it has:

* **A better UI.**
* **An editable signal chain.** Add, remove and reorder effects freely.
* **Any preset in any slot.**
* **Any effect in any position**, even the same effect several times in one chain.

**Trade-off:** while OpenGP100 is installed, the official GP-100 app only shows "Device Connected" and can't edit patches.
It works again only in **recovery mode** ([docs/recovery.md](docs/recovery.md)), e.g. to flash Valeton's firmware back.
Your Valeton `.prst` presets can be converted and installed by this app.

The "Original firmware + mods" mode keeps the normal app workflow (V-Wah/C-Wah in the MOD slot, finer MASTER volume, custom
About text, boot animation), but has a **known bug** when you swap out the wah and the chorus.

## Using the app

Run the packaged **GP-100 Firmware Builder**, or from source: `pip install -e ".[app]"`, `python -m gp100mods.app`
(Python 3.10+).

1. On first run, give it your copy of Valeton's official **GP-100 Firmware V2.1** (the GP-100 app can download it). It checks
   the SHA-256, refuses anything else and keeps a local copy. Nothing is uploaded.
2. Pick a mode. For OpenGP100 you can tick **Finer MASTER volume steps**, add `.prst` presets (installed in P01, P02, ...)
   and choose a boot animation (default dog, your own GIF/video/image, or the original logo; 160x128).
3. Click **Generate firmware** and install the file with the official GP-100 app like any update.

**Tested:** firmware V2.1 and GP-100 app 1.5.0, on Windows and WSL. **Not tested:** other app versions, macOS, native Linux.
Please report what happens if you try them.

**Before you flash:** back up your presets, keep the pedal powered and the USB cable untouched, and know the recovery steps.

## How it works

An update file has a header and four sections. In OpenGP100 mode, section 0 (the main program) is the bundled OpenGP100 build
and everything else is from your official file; in original+mods mode, section 0 is your official file, patched.

* [`firmware/opengp100/upstream/`](firmware/opengp100/upstream/): OpenGP100 source, used with the author's written permission.
* [`firmware/opengp100/patches/`](firmware/opengp100/patches/): make it run in Valeton's bootloader slot, play a boot
  animation, install your presets.
* `src/gp100mods/data/firmware/`: the compiled OpenGP100 build. Valeton's drum samples/patterns are stored blank and restored
  from your own official file at generation time (hash-checked, see `firmware/opengp100/valeton_fills.json`).

More: [architecture](docs/architecture.md), [boot chain](docs/boot-chain.md), [animation format](docs/boot-animation-format.md),
[OpenGP100 port](docs/opengp100-port.md), [legal](docs/legal.md).

## Building (developers)

Needs your official V2.1 file, an Arm GNU toolchain (10.3), CMake and git.

```sh
pip install -e .
python -m gp100mods opengp100-build --stock-fw "GP-100 Firmware V2.1.bin" --fetch-deps \
    --out opengp100-update.bin --register-version 0.7 --register-notes "what changed"
```

This applies the patches, builds, wraps the result into an update file and (with `--register-version`) adds it to the bundle.
To add an existing update file: `python -m gp100mods firmware-add FILE --version 0.5 --fills firmware/opengp100/valeton_fills.json`.
The packaged app must be built on each OS (`packaging/build-windows.ps1`, or `.github/workflows/app.yml`).

Tests: `pip install -e ".[app,dev]" && pytest` (optional env vars: `GP100_V21_FILE`, `GP100_FW_TREE`, `GP100_FW_LIB`).

## License

MIT for this repository's code. OpenGP100 is Aleksey Khristolyubov's work; OpenGP100, Valeton firmware and the NXP SDK have
their own terms: [docs/legal.md](docs/legal.md).
