# Regenerating the drum kit (developers)

Needs the repository's Python environment (numpy, scipy). The original is not in the repository:
CrocellKit_Stereo_MIX (DrumGizmo); see `src/gp100mods/data/drums/CREDITS.md` for where to get it. Treat downloaded files as
untrusted: unzip them into their own folder.

```sh
python3 tools/drumkit/make_samples.py <CrocellKit_Stereo_MIX> /tmp/wav                 # 13 WAVs, one per instrument
python3 tools/drumkit/make_kit.py /tmp/wav src/gp100mods/data/drums/kit                # slot order + levels for Valeton's patterns
```

Edit the `SLOTS` table in `make_samples.py` for other instruments and lengths, and `SLOTS` in `make_kit.py` to change which sound goes
in which slot (see docs/drums.md for the slot roles). After changing the files, update the pinned hash in `tests/test_drums.py`.
