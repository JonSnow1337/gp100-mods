# Making OpenGP100 installable through the GP-100 app

OpenGP100 is normally flashed with an SWD probe as a whole-chip image that replaces Valeton's bootloader. The patches in
`firmware/opengp100/patches` instead link it **into Valeton's slot B** and keep executing in place from flash.

* The slot header and a 1 KiB vector table come first. The bootloader copies only the vector table to `0x0` and jumps to
  its reset vector; OpenGP100's own start-up code then takes over (it sets VTOR to the table in flash).
* The SDRAM payload is 32 bytes of filler: `select_slot` rejects a slot with any zero src/len word.
* Rhythm banks stay at their fixed flash offsets (`0x133DDC`, `0x1C7414`), the cabinet bank is update section 2.

## Three bugs this exposed (all boot-path related)

1. **Zero SDRAM length** - the bootloader silently kept booting the updater.
2. **Vector table alignment** - VTOR ignores bits 6:0 and the M7 needs the table aligned to its size rounded up to a power of
   two (176 vectors -> 1 KiB). A table at slot+0x20 made the core lock up on the first interrupt (SysTick).
3. **No OCRAM** - Valeton's bootloader gives all FlexRAM to ITCM/DTCM, so memory at `0x20200000` does not exist. OpenGP100
   keeps its DMA buffers (SAI audio, USB) there: the DMA writes vanished, input read as zeros and nothing reached the DAC.
   The `.ncache` sections now live in DTCM.

`gp100mods.frmw` checks 1 and 2 on every update file, and the packer refuses to produce them.

## Boot animation

Patch 0002 adds `ui/boot_animation.c`: if the reserved area at flash `0x1CC000` holds a valid animation (see
[boot-animation-format.md](boot-animation-format.md)) it is played at boot instead of the built-in logo.

## Factory presets (patch 0004)

The app converts Valeton editor presets (`.prst`, see `src/gp100mods/presets.py`) into OpenGP100's native preset records and stores them
in a "GPPR" block right after the boot-animation area (4 KiB aligned; the firmware finds it from the animation header's size). Layout:
`"GPPR"`, u16 version 1, u16 header size 0x20, u16 count, u16 record size 0x30E, reserved 0xFF up to 0x20, then `count` records
(slot P01 first; an all-0xFF record leaves that slot empty). The firmware uses a record only while nothing has been saved in that slot and
its CRC is valid. The block is always written (even with 0 presets) so presets of an older install can never show up.
Conversion keeps blocks that are on, plus switched-off blocks a footswitch or the expression pedal controls; one expression target is
supported; if a preset exceeds the CPU/memory budget the least audible blocks are dropped and reported.
`src/gp100mods/data/opengp100_catalog.json` (algorithm ids, parameter ranges, state sizes, record template) was generated from his C catalog.
