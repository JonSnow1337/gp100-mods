# GP-100 firmware

This is the active MCUXpresso project for the GP-100. Copy this directory to
build from another workspace. It includes the editable factory assets and boot
fixtures. The build requires the MCUXpresso SDK for MIMXRT1052,
the NXP USB middleware, an Arm GNU toolchain, CMake, Make, and Python 3.

## Build and test

The standard image starts audio and tuner capture on boot and includes
USB Audio/MIDI and FlexSPI persistence.

Set `SDK_ROOT` to the MCUXpresso 2.11.0 SDK and
`USB_ROOT` to the MCUX_2.11.0 NXP USB middleware. Set `TOOLCHAIN` to the Arm GCC
prefix, including the trailing dash, if the toolchain is outside `PATH`.

```sh
make -C sources check SDK_ROOT=/path/to/mcux-sdk \
  USB_ROOT=/path/to/mcux-sdk-middleware-usb \
  TOOLCHAIN=/path/to/bin/arm-none-eabi-
```

The build produces both targets in `sources/build/`. Each has its own
`.bin`, `.elf`, and `.map` files.

| Target | Use |
| --- | --- |
| `gp100_base` | Standalone. Program `gp100_base.bin` at `0x60000000`. |
| `gp100_stock_slot` | Keep the Valeton bootloader and factory cabinet bank. Program `gp100_stock_slot.bin` at `0x60080000`. |

You can pass either target name to Make in place of `check`. Both raw images
build from this directory with the SDK and toolchain. Set
`STOCK_UPDATE_TEMPLATE=/path/to/GP-100_Firmware_V2.1.bin` to also wrap the stock
slot in a Valeton USB update package. Use the matching ELF when attaching
over SWD.

```sh
make -C sources host-check
```

The host check runs the portable tests with a host compiler. It uses the
original dump at `../firmware/valeton_spi_flash.bin` relative to this directory.
If you copy this project to another workspace, pass
`REFERENCE_FLASH=/path/to/valeton_spi_flash.bin` for the host checks.

In MCUXpresso, use **File → Import → General → Existing Projects into
Workspace**, select `sources/`, and use the existing directory as the project
location. The Eclipse project invokes the Makefile.

## Audio assets and test fixtures

Edit the files in `assets/`. The build packs the audio banks into
`build/generated/factory_banks/` and places them at their flash offsets in
`gp100_base.bin`. The stock slot carries the rhythm banks. Its USB package
also contains the cabinet bank. Both targets compile the font and boot logo.

| Editable input | Format | Flash offset |
| --- | --- | --- |
| `rhythm/patterns.c` | 100 patterns in menu order. Waits use 120 ticks per quarter note, and drum hits use the WAV file number and a gain of 0–127. | `0x1C7414` |
| `rhythm/01.wav`–`13.wav` | 16-bit mono PCM at 44.1 kHz. Playback uses whole 64-frame blocks. | `0x133DDC` |
| `cabinets/cabinet_names.c` and WAVs | 40 display names in slot order. Each numbered WAV contains 1,024 taps in 24-bit mono PCM at 44.1 kHz. | `0x680000` |
| `ui/development_font.c` | 5×7 glyphs as C bitmap rows. The firmware scales them for headings. | Linked into the image |
| `ui/iq_boot_logo.c` | C bitmap with four bits of ink coverage per pixel. Width and height are in `iq_boot_logo.h`. | Linked into the image |

The drum banks must fit their existing flash allocations. The packer checks
that each pattern ends in a restart and fits the player's 200-word window.
The OpenGP100 user preset format stores the ordered chain as block IDs, plus
each block's algorithm, parameters, bypass state, and footswitch links.
The effect catalog maps stable algorithm IDs to models and logical controls
to parameter storage slots. Presets resolve their effects by ID across
display-name changes.

### Flash layout

| Offset range | Contents |
| --- | --- |
| `0x000000–0x6BFFFF` | i.MX RT boot headers, application, cabinet impulses, and rhythm assets |
| `0x6C0000–0x7FFFFF` | User amp and cabinet slots, user presets, settings, genres, expression calibration, and device identity |

Program `gp100_base.bin` at `0x60000000` using a range-limited flash update.
The image ends within the firmware region. A chip erase would also erase
the user region, so use a range erase for firmware updates. A slot without a
valid native OpenGP100 preset record opens empty.

Preset groups of four slots alternate between two 4 KiB sectors at
`0x78B000–0x7BCFFF`. Global settings, including the last selected slot,
rotate through nine sectors at `0x7E7000–0x7EFFFF`. Both stores verify the
new data before programming a separate commit page. The previous committed
copy stays available during the write, including during sector rollover.
Customized output limiter controls use a versioned settings record in the
same two-page slot. Firmware with the limiter reads both record versions.
Firmware from before the limiter reads only the original version.

Existing OpenGP100 presets and global settings load from their old locations
until their first committed write in the new stores. Normal runtime writes
only the new stores. Factory Reset clears both formats. Older firmware still
reads the old records after a downgrade.

`tests/fixtures/` holds the reference FlexSPI configuration and SDRAM startup
data for the image check. The host build extracts the factory application's
SDRAM data into its build directory from the original flash dump.

## Real-time audio and USB

SAI and eDMA exchange 64-frame stereo blocks at 44.1 kHz. The firmware
tracks block execution time and late DAC buffer updates. With automatic
audio startup enabled, CMake compiles the DSP core, audio binding, and
transport at `-O3` in every build configuration.
[Transport](audio/transport/audio_transport.c) · [Firmware build flags](cmake/firmware.cmake)

The USB interrupt can preempt the SAI DMA render. For host playback, the
adapter keeps four receive transfers queued on the EHCI controller, each
with its own non-cacheable DMA buffer. After decoding a completed packet
into the playback ring, the callback queues that buffer again. This keeps
receives pending across consecutive 125 µs high-speed USB microframes.
[USB adapter](protocol/usb/usb_device_adapter.c)

## Source layout

| Directory | Responsibility |
| --- | --- |
| `app/` | Startup, application state, event dispatch, and preset recall |
| `board/` | Boot data, clocks, pins, peripherals, and FlexSPI writes |
| `board/input/` and `input/` | Hardware sampling, control events, and expression processing |
| `audio/` | Codec, SAI/DMA transport, audio engine, shared DSP, and effects by picker group |
| `display/` and `ui/` | Panel transport, screens, and rendering |
| `rhythm/`, `looper/`, `tuner/` | Drum playback, loop recording and playback, and pitch detection |
| `storage/` | Flash record formats, preset data, settings, and calibration journals |
| `protocol/` | DIN MIDI, SysEx, and USB Audio/MIDI |
| `assets/` | Editable cabinet, drum, font, and logo sources |
| `tests/` | Host tests grouped by module |

In picker order, the directories in `audio/effects/` are `dynamics/`,
`drive/`, `wah/`, `pitch/`, `tone/`, `modulation/`, `amp/`, `cab/`,
`delay/`, and `reverb/`. The catalog, chain, and effect dispatch code live
directly in `audio/effects/`. Effect tests follow the same groups under
`tests/audio/effects/`.

Headers live next to their implementation. Within a module directory, use
the header's filename. Across directories, include it by its path from
`sources/`, such as `audio/effects/delay/dual_echo.h`.
