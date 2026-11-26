# OpenGP100 (source tree)

`sources/` is a copy of the OpenGP100 project by its author, included **with his written permission**.
Base: his source commit `c696365` (archive `opengp100-main`, 2026-11-25). The git tag `opengp100-upstream-c696365` is that
tree exactly as received (minus the files below), so `git diff opengp100-upstream-c696365 -- firmware/opengp100/sources`
shows every change we made. We edit the code directly (no patch files); when he publishes a new version, replace the tree,
commit it as a new `opengp100-upstream-*` tag and merge/re-apply our commits on top.
(If the author publishes a LICENSE for OpenGP100, add it here.)

## Left out on purpose

| Left out | Why |
| --- | --- |
| `firmware/` (official Valeton update file, flash dump) | Valeton's firmware; the dump holds one unit's device identity |
| `archive/` (notes, board photos, recovered bootloader C) | not needed to build; includes decompiled Valeton bootloader code |
| `sources/tests/fixtures/*.bin` | hardware-config captures; git-ignored with every other `*.bin`. Copy them from his archive to run `ctest` image checks |
| `sources/assets/cabinets/*.wav` | the 40 cabinet impulse responses are Valeton's (they equal Valeton's cabinet bank byte for byte) |
| `sources/assets/rhythm/*.wav`, `patterns.c` | the drum samples and pattern table are Valeton's data |
| the prebuilt `OpenGP100-...-Update` zip | contains Valeton's bootloader-slot data; we build our own |

`gp100mods opengp100-build` generates **placeholder** versions of the missing audio assets (same sizes, silence) so the code
builds. The finished app restores the real data from the user's own official Valeton V2.1 file, checking a SHA-256 of every
restored region (`valeton_fills.json`). Only code is compiled from this tree.
`UPSTREAM-README.md` is his top-level README.

## Also worth confirming with the author
The IQ/Musicbone boot logo (`sources/assets/ui/iq_boot_logo.c`) and a few tables in the DSP and identity code were transcribed
from Valeton's firmware or other third parties; whether that is acceptable for redistribution is his call.

Author: Aleksey Khristolyubov (Reddit: Natural-Mechanic7484) - thank you.
