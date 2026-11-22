# How the GP-100 boots and updates (as observed)

Flash is an 8 MiB SPI NOR behind an i.MX RT1052 (Cortex-M7). Relevant layout:

| Flash offset | Contents |
| --- | --- |
| `0x0000` | boot header (FCB, IVT, DCD: sets up the external SDRAM) |
| `0x2000` | **bootloader** (6 KB, runs in place) |
| `0x20000` | slot A: small USB/MIDI **updater** image |
| `0x80000` | slot B: the main firmware (stock V2.1, or OpenGP100 here) |
| `0x680000` | cabinet impulse bank (update section 2) |
| `0x700000` | factory data (update section 3) |
| `0x78B000...` | user data (presets, settings) |
| `0x7FF000` | per-device identity record (its first 8 bytes are the "token" the bootloader checks) |

**Slot format.** A slot starts with seven 32-bit words: `itcm_src, itcm_len, sdram_src, sdram_len, 0x55555555,
0xAAAAAAAA, 0xA5A5A5A5` (offsets are flash offsets). The bootloader copies the SDRAM payload to `0x80000000`, the ITCM
payload to `0x0`, verifies the ITCM copy, checks the three magic words and jumps to the word at ITCM+4.

**Slot choice (`select_slot`).** Slot B is booted only if: all four src/len words are non-zero, the magic words match,
the token is not erased, and at least one of the three buttons is *not* held. Otherwise slot A (the updater) runs - this
is the recovery path. Holding both footswitches and PARA at power-on pulls all three pins low.

**State the bootloader leaves behind:** clocks and SDRAM up, MPU configured, I+D caches on, interrupts masked, FlexRAM set
to ITCM 320 KB + DTCM 192 KB + **OCRAM 0** (`IOMUXC_GPR17 = 0xAAAFFFFF`).

**Update container (FRMW).** 0x30-byte header (`FRMW`, body size, model `GP100`, 8-bit sum of the body at byte 0x0E) and
four sections. The container has no destination addresses; the updater decides by transfer type: type 0 -> `0x80000`,
1 -> `0x680000`, 2 -> `0x700000`, `0x10` -> a different device (section 1). The updater erases and programs only the sectors
it writes, erases the token at the start of a type-0 transfer, and the PC app writes it back at the end - so an
interrupted update boots the updater. It verifies per-packet sums and packet counts, not the content.

Transport is MIDI SysEx `F0 7E 48 <cmd> ... F7` with nibble-coded values.
