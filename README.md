# gp100-mods

A desktop app that builds firmware for the Valeton GP-100 pedal: [OpenGP100](#how-it-all-fits-together) (a community
firmware) or Valeton's original firmware with mods. You install the result with the **official GP-100 app**; no J-Link, no
compiler. Valeton's bootloader and footswitch+PARA recovery stay untouched.

> **Unofficial.** Not affiliated with Valeton. Flashing can leave a device unusable; read [docs/recovery.md](docs/recovery.md)
> first. Every firmware update erases the presets on the pedal.

> **OpenGP100 mode replaces Valeton's firmware, and the official GP-100 app stops working with the pedal.** While OpenGP100
> is installed, the app only shows its "Device Connected" screen (with the note "If you cannot access the software edit
> interface after a long time, please restart the GP-100 and software") and you can't edit your effect patches. The app
> works again only when the pedal is in **recovery mode** (see [docs/recovery.md](docs/recovery.md)), and then only to
> install a firmware update, for example to go back to Valeton's firmware. (The "Original firmware + mods" mode keeps the
> normal app workflow, but it has a **known bug**: it is buggy when you swap out the wah and the chorus.)

**Tested with:** Valeton's official firmware **V2.1** as the base file and the **GP-100 app 1.5.0**, on Windows and WSL
(Linux). **Not tested:** older firmwares (this app only accepts V2.1 anyway), other GP-100 app versions (e.g. 1.5.1), macOS,
and native Linux. Other versions may work or may not; if you try one, please report what happened.

**Recommendations:** keep a copy of your presets (the update erases them), keep the pedal powered and the USB cable
untouched during the update, and know the recovery steps ([docs/recovery.md](docs/recovery.md)) before you flash. If you
are unsure, try it first with the same app version listed above.

**Credits:** huge thanks to Reddit user **Natural-Mechanic7484** (Aleksey Khristolyubov), who wrote OpenGP100 from scratch and let us build on it.
Everything this project does with OpenGP100 rests on his work.

## Using the app

Run the packaged **GP-100 Firmware Builder** (or from source: `pip install -e ".[app]"`, `python -m gp100mods.app`, Python 3.10+).

* **OpenGP100 mode:** the newest OpenGP100 build is always used; tick **Finer MASTER volume steps** if the pedal is too loud at low MASTER values, add your Valeton editor presets (`.prst` files, converted to OpenGP100 presets and installed in P01, P02, ...), choose a boot animation (default: dog boot with a
  loading bar; or your own GIF/video/image; or the original logo), click **Generate firmware**.
* **Original firmware + mods mode:** V-Wah/C-Wah in the MOD slot, finer MASTER volume, custom About text, boot animation.
* Install the saved file with the official GP-100 app like any update.

**First run:** the app asks once for your copy of Valeton's official **GP-100 Firmware V2.1** update file (the GP-100 app can
download it). It checks the SHA-256, refuses anything else, and keeps a copy on your computer. Nothing is uploaded.
Animations are 160x128; the app fits them to the available space.

## How it all fits together

An update file for the GP-100 app has a header and four sections. Where each part comes from:

| Part | OpenGP100 mode | Original + mods mode |
| --- | --- | --- |
| section 0: the main program | OpenGP100, bundled in the app | the user's official file, patched |
| sections 1-3 + header (Valeton's data) | the user's official file | the user's official file |
| boot animation | written into a reserved flash area | patched in |

* **OpenGP100** is another author's from-scratch firmware, used here with his written permission. Its source is in
  `firmware/opengp100/upstream/`.
* **Patches** (`firmware/opengp100/patches/`) change it to run in a slot of Valeton's bootloader (so the official app can
  install it), to play a boot animation, and to install your own presets. They are applied at build time.
* **The bundle** (`src/gp100mods/data/firmware/`, committed) holds the current compiled OpenGP100 build the app ships. Two Valeton
  regions inside them (drum samples, drum patterns) are stored blank and restored from the user's own official file at
  generation time, checked against a hash (`firmware/opengp100/valeton_fills.json`). A different file is refused.
* **The app** (`src/gp100mods/app/`) just collects choices; `update_file.py` assembles the output.

Details: [architecture](docs/architecture.md), [boot chain](docs/boot-chain.md), [animation format](docs/boot-animation-format.md),
[OpenGP100 port](docs/opengp100-port.md), [legal](docs/legal.md).

## Building (developers)

Needs your official V2.1 file, an Arm GNU toolchain (10.3), CMake and git.

```sh
pip install -e .
python -m gp100mods opengp100-build --stock-fw "GP-100 Firmware V2.1.bin" --fetch-deps \
    --out opengp100-update.bin --register-version 0.7 --register-notes "what changed"
```

This copies `upstream/`, creates silent placeholder audio (Valeton's data is not in the repo), applies the patches, builds,
wraps the result into an update file and, with `--register-version`, adds it to the bundle with Valeton's regions blanked.
To add an existing update file: `python -m gp100mods firmware-add FILE --version 0.5 --fills firmware/opengp100/valeton_fills.json`.
Option switches are guarded byte patches in the bundle's `manifest.json` (see `manifest.example.json`).

Packaged app: PyInstaller can't cross-compile; build on each OS (`packaging/build-windows.ps1` on Windows, or
`.github/workflows/app.yml`). The current OpenGP100 build is committed in the bundle; after changing the firmware, rebuild and register it first (see above).

## Layout

```
src/gp100mods/
  app/            desktop app (PySide6)
  bootanim/       GPBA animation format + encoder
  opengp100/      build script, slot packer, bootloader simulator
  stock/          patches for the original firmware (wah, volume, About, boot GIF)
  valeton.py      verifies/keeps the user's official file
  bundle.py       bundled builds, blanking of Valeton regions, option switches
  update_file.py  assembles the output file
  frmw.py         update-container parser/checker
  dev/            reverse-engineering helpers
firmware/opengp100/{upstream,patches,valeton_fills.json}
tools/ghidra/  packaging/  tests/  docs/
```

## Tests

`pip install -e ".[app,dev]" && pytest`. Optional env vars for the real-file tests: `GP100_V21_FILE`, `GP100_FW_TREE`, `GP100_FW_LIB`.

## License

MIT for this repository's code (OpenGP100 is Aleksey Khristolyubov's work). OpenGP100, Valeton firmware and the NXP SDK have their own terms: [docs/legal.md](docs/legal.md).
