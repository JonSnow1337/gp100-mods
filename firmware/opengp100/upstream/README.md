# OpenGP100

OpenGP100 is a standalone firmware project for the Valeton GP-100 guitar
effects pedal. The C source covers the board bring-up, audio and effects,
front-panel UI, preset and settings storage, rhythm player, looper, tuner,
and USB Audio/MIDI.

## Main differences from the factory firmware

- **Build the chain from scratch.** Add, remove, and reorder up to eight effects.
  The same effect group or model can appear more than once, with independent
  settings for each instance. [Signal Chain controls](#presets-and-effects)
- **One bank of 99 user presets.** Unused slots from P01 to P99 open with an
  empty chain. Valeton's firmware includes a second bank of 99 factory presets.
  [Valeton GP-100 specifications](https://www.valeton.net/product/gp-100/)
- **Per-pedal controls.** Assign individual pedals to the stomp switches and
  see those assignments on the Play screen. Each preset can link the expression
  pedal to one knob, with its own heel and toe values and optional toe-switch
  bypass. [Front-panel controls](#presets-and-effects)
- **Recovery after an interrupted save.** Preset and global settings writes
  retain the previous valid record until the new record has been verified and
  committed. This also protects settings saved during preset selection.
  [Flash storage](sources/README.md#flash-layout)

## Hardware

| Subsystem | GP-100 implementation |
| --- | --- |
| Processor | NXP i.MX RT1052, Arm Cortex-M7 |
| Flash | 8 MiB quad SPI NOR on FlexSPI, mapped at `0x60000000` |
| External RAM | 32 MiB SDRAM on SEMC at `0x80000000` |
| Audio | SAI1 and eDMA, 44.1 kHz stereo, 24-bit samples in 32-bit slots. Board markings identify an ES7243 ADC and ES8156 DAC. |
| Display | 128 × 160 RGB565 panel on LPSPI1, using an ST7735-compatible command set |
| Controls | GPIO buttons, footswitches and encoders, ADC1_IN6 expression input, and GPIO1_IO21 tuner edge capture |
| MIDI and USB | LPUART1 MIDI receive at 31,250 baud and USB_OTG1 Audio/MIDI. The USB product name is OpenGP100. |
| Debug | SWD and target reset |

Startup configures the pins and clocks, tests SDRAM, and opens flash storage.
Expression sampling and tuner capture start before the display. The first Play
frame reaches the panel before the audio transport starts. Timing-sensitive
audio code runs from ITCM, while effect code and the display framebuffers use
external SDRAM. [Pin mux](sources/board/pin_mux.c) · [Startup](sources/app/main.c) · [Linker script](sources/linker/gp100_flexspi.ld)

## Firmware layout

| Flash offsets | Contents |
| --- | --- |
| `0x000000–0x6BFFFF` | i.MX RT boot headers, application, cabinet impulses, and rhythm assets |
| `0x6C0000–0x7FFFFF` | User presets, amps and cabinets, settings, genres, expression calibration, and device identity |

The i.MX RT ROM boots the application directly from FlexSPI. The build packs rhythm
patterns from C and cabinet impulses and drum sounds from PCM WAV into the firmware
region. [Source layout and asset formats](sources/README.md)

## Presets and effects

- Press **EDIT** and turn **PARA** to select a pedal. Tap **PARA** to
  cycle its parameter pages.
- With a pedal selected, tap **EDIT** for **Insert before**, **Replace**, or
  **Remove**. Turn **PARA** to choose, press it to confirm, or press **EXIT**
  to cancel.
- Hold **PARA** to manage the chain in the same view. Quick 1 assigns the
  selected pedal to a footswitch in Stomp mode, Quick 2 opens the effect
  picker, and Quick 3 moves the pedal. Select **ADD** and tap **EDIT**
  to choose an effect group and model. Tap **PARA** or press **EXIT**
  to return to the selected pedal's parameter controls.
- In **Signal Chain**, select a pedal and open **EDIT → Assign EXP**. Quick 1
  selects a knob, Quick 2 sets its heel value, and Quick 3 sets its toe value.
  Choose **None** to unlink the marked pedal. Tap **PARA** to apply and
  **SAVE** to store the assignment in the preset.
- On the linked pedal, set **Footsw.** to **EXP** to toggle its bypass with
  each extra toe press. The Play screen shows the linked pedal in EXP mode.
- In **Edit**, the Quick knobs adjust the controls along the bottom of the
  screen. **SAVE** writes the chain to a user slot.
- In **Stomp** mode, the Play screen shows each footswitch's pedals. Bypassed
  pedals are dimmed, and active pedals have a lit indicator.

Construct checks the block count and the chain's DSP and history-buffer budgets
before adding an effect. The saved format is specific to OpenGP100. A slot without
a valid OpenGP100 record is shown as empty.

## Build

Use CMake, Make, Python 3, an Arm GNU Embedded toolchain, and the MCUXpresso SDK
and USB middleware at tag `MCUX_2.11.0`. The verified compiler is Arm GNU Embedded
10.3-2021.10. Keep the SDK, middleware, and toolchain outside the checkout.
[Arm GNU toolchains](https://developer.arm.com/downloads/-/gnu-rm)

From a separate dependency directory, obtain the NXP sources.

```sh
git clone --depth 1 --branch MCUX_2.11.0 --filter=blob:none --sparse \
  https://github.com/nxp-mcuxpresso/mcux-sdk.git
git -C mcux-sdk sparse-checkout set CMSIS devices/MIMXRT1052 drivers components/lists components/osa
git clone --depth 1 --branch MCUX_2.11.0 \
  https://github.com/nxp-mcuxpresso/mcux-sdk-middleware-usb.git
```

From this repository root, build and run the checks.

```sh
make -C sources check SDK_ROOT=/path/to/mcux-sdk \
  USB_ROOT=/path/to/mcux-sdk-middleware-usb \
  TOOLCHAIN=/path/to/bin/arm-none-eabi-
make -C sources host-check
```

The cross-build writes `sources/build/gp100_base.bin` for programming and
`gp100_base.elf` for debugging. For MCUXpresso IDE, import `sources/` via
**File → Import → General → Existing Projects into Workspace**. Its make
builder accepts `MCUX_SDK_ROOT`, `MCUX_USB_ROOT`, and `TOOLCHAIN` as environment
variables. A host C compiler runs the host checks. Tests that use factory
records read the [original flash dump](firmware/README.md).

## Program over SWD

With the pedal on its normal supply, halt the MCU and save a verified 8 MiB
readback from `0x60000000` before programming. Keep the backup off Git so it
can restore this unit's settings and identity. With J-Link Commander, use
`savebin /path/to/backup.bin 0x60000000 0x800000` and then
`verifybin /path/to/backup.bin 0x60000000` while halted.

Use the explicit QSPI loader for the RT1052.
[SEGGER flash loader selection](https://kb.segger.com/J-Link_Multiple_Flashloader)

```sh
JLinkExe -NoGui 1 -device 'MIMXRT1052xxxxB?BankAddr=0x60000000&Loader=QSPI' \
  -if SWD -speed 1000 -autoconnect 1 -ExitOnError 1
```

In Commander, use these commands from the repository root. Program the image
as a range update to preserve the user-data partition.

```text
r
loadfile sources/build/gp100_base.bin 0x60000000 noreset
```

After programming and verification succeed, reset the pedal. A target-pin
reset via the generic `Cortex-M7` target also works on the bench with
`RSetType 2`, `r`, and `g`. [SEGGER reset strategies](https://kb.segger.com/J-Link_Reset_Strategies)

## Verification

The host suite covers constructing a chain, independent duplicate effects, footswitch links,
and saving and recalling a user patch. The firmware image check verifies the
boot headers and packed audio banks. OpenGP100 exposes 44.1 kHz stereo USB audio
and a USB-MIDI port.
