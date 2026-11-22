# Legal / redistribution


**Valeton data is not shipped.** Everything in a generated file that is Valeton's comes from the user's own official V2.1 file
(SHA-256 verified, re-checked on every build):

| Part | Size |
| --- | --- |
| header, section 1, section 2 (cabinet bank), section 3 (factory data) | ~0.7 MB |
| drum samples inside section 0 (an exact copy of Valeton's block) | 604 KB |
| drum pattern table inside section 0 (Valeton's, plus 251 bytes of OpenGP100's) | 17 KB |

The bundled firmware (committed to this repository) stores the last two blank; `update_file.compose` restores them and refuses unless the result matches a
hard-coded hash. The repository, bundle and packaged app were byte-scanned for these blocks (none found). Valeton's bootloader
and updater are never shipped. (An earlier version of this file wrongly said section 0 held no Valeton data.)

**OpenGP100** is the work of Aleksey Khristolyubov (Reddit: Natural-Mechanic7484) (thank you), included in `firmware/opengp100/upstream/` with his written permission (see
`UPSTREAM.md` there; his cabinet/drum WAVs and pattern table are left out).

**Committed:** the current compiled OpenGP100 build, `src/gp100mods/data/firmware/*.sec0` (only one version is kept), so the
app works out of the box. Its two Valeton regions are blank; `tests/test_committed_bundle.py` fails if they are not.

**Never committed (git-ignored):** firmware images (`*.bin`, `*.frmw`), captures (`*.pcapng`). NXP SDK is fetched at build
time under its own license.
