# OpenGP100 (upstream source)

This folder is a copy of the OpenGP100 project by its author, included **with his written permission**. It is the unmodified
upstream `sources/` tree minus the files listed below; our changes live in `../patches/` and are applied at build time.
(If the author publishes a LICENSE for OpenGP100, add it here.)

## Left out on purpose

| Left out | Why |
| --- | --- |
| `firmware/` (official Valeton update file, flash dump) | Valeton's firmware; the dump holds one unit's device identity |
| `archive/` (notes, board photos, recovered bootloader C) | not needed to build; includes decompiled Valeton bootloader code |
| `sources/tests/` | needs the flash dump to run |
| `sources/assets/cabinets/*.wav` | the 40 cabinet impulse responses are Valeton's (they equal Valeton's cabinet bank byte for byte) |
| `sources/assets/rhythm/*.wav`, `patterns.c` | the drum samples and pattern table are Valeton's data (the samples are an exact copy of a block in Valeton's firmware) |

`gp100mods opengp100-build` generates **placeholder** versions of the missing audio assets (same sizes, silence) so the code
builds. The finished app restores the real data from the user's own official Valeton V2.1 file, checking a SHA-256 of every
restored region (`../valeton_fills.json`). Only code is compiled from this tree.

## Also worth confirming with the author
The IQ/Musicbone boot logo (`sources/assets/ui/iq_boot_logo.c`) and a few tables in the DSP and identity code were transcribed
from Valeton's firmware or other third parties; whether that is acceptable for redistribution is his call.

Author: Aleksey Khristolyubov (Reddit: Natural-Mechanic7484) - thank you.
