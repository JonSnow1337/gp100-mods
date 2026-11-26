# Making OpenGP100 installable through the GP-100 app

OpenGP100 is normally flashed with an SWD probe as a whole-chip image that replaces Valeton's bootloader. Its `gp100_stock_slot`
target (by its author) instead builds it for **Valeton's slot B** and wraps it into a Valeton update file, so the official GP-100
app can install it and the bootloader (and the footswitch + PARA recovery mode) stays untouched.

* The slot header is Valeton's own (same words as the stock file, so the bootloader accepts it). The bootloader copies the ITCM
  payload to `0x0` and jumps through its second word: a tiny trampoline (`board/stock_slot_entry.S`).
* The trampoline runs from flash with no stack. It switches the FlexRAM bank layout (Valeton gives all 16 banks to ITCM/DTCM,
  OpenGP100 needs 8 OCRAM banks for its DMA buffers plus DTCM and ITCM), then enters the normal SDK start-up. The image itself
  executes in place from flash, 4 KiB above the slot header.
* Rhythm banks stay at their fixed flash offsets (`0x133DDC`, `0x1C7414`), the cabinet bank is update section 2. Section 0 keeps
  Valeton's size (`0x29FAEC`), erased flash (0xFF) behind the data.
* The app's build step (`gp100mods.opengp100.finish`) adds two reserved areas of that erased space: the feature-flag block
  (`0x1CBF00`) and the boot-animation / factory-preset area (`0x1CC000`).

## Boot-path pitfalls

A slot the bootloader rejects, or a wrong memory layout, fails silently:

1. **Zero SDRAM length:** `select_slot` rejects a slot with any zero src/len word and boots the updater instead.
2. **Vector table alignment:** VTOR ignores bits 6:0 and the M7 needs the table aligned to its size rounded up to a power of two.
3. **No OCRAM:** with Valeton's FlexRAM layout, memory at `0x20200000` does not exist, so DMA buffers placed there read as zeros.

The stock-slot target avoids all three. `gp100mods.frmw` and `opengp100.finish` check what can be checked offline, and
`python -m gp100mods bootloader-sim` replays the bootloader's slot selection on an update file against a flash dump.

## Boot animation

Our `ui/boot_animation.c` (in the firmware): if the reserved area at flash `0x1CC000` holds a valid animation (see
[boot-animation-format.md](boot-animation-format.md)) it is played at boot instead of the built-in logo.

## Factory presets

The app converts Valeton editor presets (`.prst`, see `src/gp100mods/presets.py`) into OpenGP100's native preset records and stores them
in a "GPPR" block right after the boot-animation area (4 KiB aligned; the firmware finds it from the animation header's size). Layout:
`"GPPR"`, u16 version 1, u16 header size 0x20, u16 count, u16 record size 0x30E, reserved 0xFF up to 0x20, then `count` records
(slot P01 first; an all-0xFF record leaves that slot empty). The firmware uses a record only while nothing has been saved in that slot and
its CRC is valid. The block is always written (even with 0 presets) so presets of an older install can never show up.
Conversion keeps blocks that are on, plus switched-off blocks a footswitch or the expression pedal controls; one expression target is
supported; if a preset exceeds the CPU/memory budget the least audible blocks are dropped and reported.
`src/gp100mods/data/opengp100_catalog.json` (algorithm ids, parameter ranges, state sizes, record template) was generated from his C catalog.
