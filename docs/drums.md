# Drum sounds

OpenGP100 plays its built-in rhythms from two data banks in flash: 13 drum samples and 100 rhythm patterns. This project replaces
**only the samples**. The 100 patterns stay Valeton's (restored from the user's official file, as before), so every rhythm keeps its
groove and only the sounds change. The sounds are plain WAV files in [`src/gp100mods/data/drums/kit/`](../src/gp100mods/data/drums/kit/),
so they can be swapped without any firmware change:

| Files | Format |
| --- | --- |
| `kit/NN-name.wav` (NN = 01..13) | mono, 16-bit, 44.1 kHz. All 13 together may use about 589 KB (about 6.7 s). |
| `CREDITS.md` | where the sounds come from (CC BY 4.0: keep the credit if you share the result) |

**Slot roles.** The patterns decide what each slot is, so a replacement sound must fit its slot. The roles below were worked out from
when the patterns play each slot (kick on beats 1 and 3, snare on 2 and 4, hats on every eighth note) and from the original samples.
This is an inference, not documentation:

| Slot | Role | Slot | Role |
| --- | --- | --- | --- |
| 01 | electronic kick | 08 | hi-hat, second kind (main hat of some rhythms) |
| 02 | kick (beats 1 and 3) | 09 | open hi-hat |
| 03 | electronic snare / clap | 10-13 | not used by any pattern (crash, ride, ride bell, sidestick) |
| 04, 05 | toms (fills) | 06 | snare (beats 2 and 4) |
| 07 | closed hi-hat | | |

The metronome rhythms (91-100) use slot 03 for the accent and slot 07 for the tick.

The kit's sounds are level-matched to Valeton's original samples per slot, so the rhythms keep their balance and cannot clip the mixer.

`gp100mods.drums.load_kit()` packs the files, then parses the result the way the firmware does. This matters: if the firmware
rejects the bank, `GP100_AudioBindingInitialize` fails and the pedal does not start normally (recovery with both footswitches + PARA still
works). The packer therefore refuses anything outside the limits instead of trying to fit it.

The app option **New drum sounds** (on by default) writes the sample bank into the drum-sample region of the firmware
(`update_file.compose(..., drums=kit)`); unticked, Valeton's drum samples are restored from the user's official file as before.
Tests: `tests/test_drums.py` (the shipped files must pack to a pinned bank, damaged input must be refused, the C parser's checks are mirrored).

For developers only: `tools/drumkit/` regenerates the files from the original CrocellKit (see its README; the original is not in this repository).
