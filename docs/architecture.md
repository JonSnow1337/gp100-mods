# Architecture

```
GIF / video / images --encode.py--> Animation (palette + indexed frames)
                                       |  format.py (GPBA bytes)
bundled OpenGP100 section 0 (bundle.py) + option switches
   + user's official Valeton parts (valeton.py) --update_file.compose--> new update file
                                       (guarded byte patches, animation area replaced, FRMW header re-sealed)
```

* **No compiler on the user's machine.** The firmware is built once (see `opengp100/build.py`); the app only edits a
  fixed data area in the finished update file.
* **OpenGP100 ships inside the app** (`data/firmware/`, section 0 only, with two Valeton-derived regions - drum samples and patterns - stored blank and restored from the user's official file by `update_file.compose`; hash-checked on every load). Valeton's parts do not: the user
  supplies the official V2.1 file once, `valeton.py` verifies it against hard-coded hashes and caches the parts per user. No other file
  can be fed in. Option switches are guarded byte patches declared in `manifest.json`.
* `update_file.inspect` (used by the maintainer tool and as a post-generation self-check) accepts a file only if it passes the FRMW checks (`frmw.py`) *and* contains the `GPBA` area header,
  which proves the firmware was built with the animation player.
* The original file is never overwritten.
* The app is PySide6 (Qt). It was chosen because the whole code base is Python and Qt packages cleanly with PyInstaller for
  Windows, macOS and Linux.

## Two modes

* **OpenGP100** builds `bundled section 0 + the user's Valeton parts` (see above).
* **Original firmware + mods** applies `gp100mods.stock.*` patches to the user's verified copy of Valeton's V2.1 file. Each patch
  (`wah_in_mod`, `master_volume`, `about_text`, `boot_gif`) checks that every byte it touches is exactly what was analysed and
  raises ValueError otherwise, so it can only ever modify that one firmware. Order: wah, boot GIF, About text, volume. With no
  box ticked the output equals the official file. The patches reproduce the builds originally flashed and confirmed on a real
  pedal byte for byte (`tests/test_stock.py`, run with `GP100_V21_FILE` set).

## UI

PySide6 with one dark stylesheet (`app/theme.py`): a sidebar to switch modes, cards, one primary button. Pages live in
`app/pages.py`; the shared animation picker (`MediaCard`) converts in a background job.
