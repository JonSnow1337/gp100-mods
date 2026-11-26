# Legal / redistribution

**Valeton data is not shipped.** Everything in a generated file that is Valeton's comes from the user's own official V2.1 file
(SHA-256 verified, re-checked on every build):

| Part | Size |
| --- | --- |
| header, section 1, section 2 (cabinet bank), section 3 (factory data) | ~0.7 MB |
| drum samples inside section 0 (an exact copy of Valeton's block) | 604 KB |
| drum pattern table inside section 0 (Valeton's, plus 251 bytes of OpenGP100's) | 17 KB |

The drum sample block is only restored when the user unticks **New drum sounds**; with it on (the default), the
file contains our own samples instead (see below). The pattern table is always Valeton's.

The bundled firmware (committed to this repository) stores the last two blank; `update_file.compose` restores them and refuses unless
the result matches a hard-coded hash. Valeton's bootloader and updater are never shipped.

**OpenGP100** is the work of Aleksey Khristolyubov (Reddit: Natural-Mechanic7484) (thank you), included in `firmware/opengp100/sources/` with his written permission
(see `firmware/opengp100/UPSTREAM.md`; his cabinet/drum WAVs and pattern table are left out).

**Committed:** the current compiled OpenGP100 build, `src/gp100mods/data/firmware/*.sec0` (only one version is kept), so the
app works out of the box. Its two Valeton regions are blank; `tests/test_committed_bundle.py` fails if they are not.

**Drum sounds (committed, `src/gp100mods/data/drums/`):** derived from CrocellKit (CC BY 4.0, Lars Muldjord / Crocell /
JBOSound). The original is not in the repository, only the converted WAVs. Anyone distributing a generated file or a packaged app must
keep the credit in `data/drums/CREDITS.md` (the app shows a short version next to the option). Changes are described there.

**Never committed (git-ignored):** firmware images (`*.bin`, `*.frmw`), captures (`*.pcapng`). NXP SDK is fetched at build
time under its own license.
