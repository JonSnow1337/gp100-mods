# OpenGP100 source

`sources/` is OpenGP100 by Aleksey Khristolyubov (Reddit: Natural-Mechanic7484), included with his written permission.
Based on his commit `c696365`. Our changes are committed directly on top (see [README.md](README.md)).
`UPSTREAM-README.md` is his own README.

## Not included

| What | Why |
| --- | --- |
| `firmware/`, `archive/` from his repo | Valeton's firmware, a flash dump and decompiled bootloader code |
| `sources/assets/cabinets/*.wav` | Valeton's cabinet impulse responses |
| `sources/assets/rhythm/*.wav`, `patterns.c` | Valeton's drum samples and patterns |
| `sources/tests/fixtures/*.bin` | hardware captures (all `*.bin` are git-ignored) |

`opengp100-build` fills the missing audio with silent placeholders of the right size. The app restores the real data from your own
official V2.1 file when it generates an update (recipe in `valeton_fills.json`).
