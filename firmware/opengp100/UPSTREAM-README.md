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
- **Selectable effect builds.** Choose which pedals to compile into the
  firmware. Presets retain the settings of any pedal absent from the current
  build. [Effect profiles](#build-with-a-selected-set-of-pedals)
- **Global output limiter.** Set a stereo brickwall limiter for the complete
  analog output mix. [Limiter controls](#output-limiter)
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

The standalone image boots directly from FlexSPI. The stock-slot image runs
from the original Valeton bootloader. The build packs rhythm patterns from C
and cabinet impulses and drum samples from PCM WAV. [Source layout and asset formats](sources/README.md)

## Presets and effects

- Press **EDIT** and turn **PARA** to select a pedal. Tap **PARA** to
  cycle its parameter pages.
- Hold **PARA** to manage the chain in the same view. Quick 1 assigns the
  selected pedal to a footswitch in Stomp mode, Quick 2 opens the effect
  picker, and Quick 3 moves the pedal. Select **ADD** and tap **EDIT**
  to choose an effect group and model. Tap **PARA** or press **EXIT**
  to return to the selected pedal's parameter controls.
- In **Signal Chain**, tap **EDIT** on a selected pedal for **Insert before**,
  **Replace**, or **Remove**. Turn **PARA** to choose, press it to confirm,
  or press **EXIT** to cancel.
- In **Signal Chain**, select a pedal and open **EDIT → Assign EXP**. Quick 1
  selects a knob, Quick 2 sets its heel value, and Quick 3 sets its toe value.
  Choose **None** to unlink the marked pedal. Tap **PARA** to apply and
  **SAVE** to store the assignment in the preset.
- On the linked pedal, set **Footsw.** to **EXP** to toggle its bypass with
  each extra toe press. The Play screen shows the linked pedal in EXP mode.
- To recalibrate, open **Global → EXP Setting → Calibrate**. Use **PARA**
  to confirm fully raised, fully lowered, and held down firmly for the
  toe switch. The screen confirms **Calibration saved** after the write.
- In **Edit**, the Quick knobs control bypass and effect parameters.
  **SAVE** writes the chain to a user slot.
- In **Stomp** mode, the Play screen shows each footswitch's pedals. Bypassed
  pedals are dimmed, and active pedals have a lit indicator.
- Add **Dynamics → Limitter** anywhere in the chain to limit the signal at that
  point. Its Threshold, Ceiling, and Release are saved in the preset. Each
  active Limitter adds about 2 ms of lookahead.
- Add **Pitch → Octaver2** for analog-style, single-note octave division. Direct,
  Oct 1, and Oct 2 mix the input with voices one and two octaves below it.
  Tap **PARA** for Range. Start with Low for bass. Range changes the detector
  and the tone of the first octave.
- Add **Drive → Muff** for the 1974 White Can 74#3 fuzz. Sustain drives
  two silicon-diode feedback stages, Tone blends the bass and treble branches,
  and Volume sets the output level. [Circuit traced by Kit Rae](https://www.bigmuffpage.com/Big_Muff_Pi_versions_schematics_part2.html)
- Add **Wah → Balls** for the clean twin envelope filter. **Response**
  controls how far the two resonances sweep with your playing dynamics.
  [Electro-Harmonix Bassballs manual](https://www.ehx.com/wp-content/uploads/2021/07/bassballs-manual.pdf)
- Add **Wah → Envelope** for an M82-inspired envelope filter. **Dry** and **FX**
  set the direct and filtered levels. **Decay** sets where the sweep settles,
  **Q** shapes the resonance, and **Sens** adjusts how easily playing opens
  the filter. The model sweeps from 76 Hz to 3.2 kHz, with a decay stop frequency
  of 76 Hz to 1.3 kHz. Its envelope timing and sensitivity are estimates.
  [M82 control and sweep specifications](https://www.americanmusical.com/media/manuals_rebates/1266533257_M82_Manual_LoRes.pdf)

The picker groups pedals by type. Choose **Dynamics**, **Drive**, **Wah**,
**Pitch**, **Tone**, **Modulation**, **AMP**, **Cab**, **Delay**, or **Reverb**,
then choose a model for the selected position in the chain.

Construct checks the block count and the chain's DSP and history-buffer budgets
before adding an effect. The saved format is specific to OpenGP100. A slot without
a valid OpenGP100 record is shown as empty.

### Studio Comp

Add **Dynamics → Studio Comp** for parallel compression. The compressor
uses the louder channel's current peak to drive a shared gain reduction.
Dry and Wet have independent levels, and the effect adds zero samples
of audio delay. Tap **PARA** to cycle the parameter pages.

| Control | Range | Default |
| --- | --- | --- |
| Wet | −∞ to +12 dB | 0 dB |
| Dry | −∞ to +12 dB | −∞ (muted) |
| Threshold | −60 to +12 dBFS | 0 dBFS |
| Attack | 0–500 ms | 3 ms |
| Release | 0–5,000 ms | 100 ms |
| Ratio | 1:1 to 40:1, plus ∞:1 | 4:1 |
| Knee | 0–24 dB | 0 dB |

## Drums

Drum tempo ranges from 10 to 250 BPM. The firmware applies knob changes
live and saves changed drum settings when playback starts or stops.

## Output limiter

Open **Global → Output Limiter** and tap **PARA** to switch it on or off.
Quick 1 adjusts Threshold (−60 to +12 dB), Quick 2 adjusts Ceiling
(−60 to 0 dBFS), and Quick 3 adjusts Release (1 to 120 dB/sec). The factory
defaults are Off, 0 dB Threshold, 0 dB Ceiling, and 15 dB/sec Release.

The limiter runs after Master on the final stereo mix going to the DAC.
Lowering Threshold relative to Ceiling raises the level of signals below
the threshold. Both channels share gain reduction, and the limiter adds
about 2 ms of lookahead while enabled. USB recording takes its signal
before this stage.

## USB Audio and MIDI

The host sees **OpenGP100** as a stereo USB audio interface and a USB-MIDI
port. Playback and recording can run simultaneously at 44.1 kHz using
16-bit PCM or 24-bit PCM in 32-bit samples.

Open **Global → USB Audio** to set the recording level (−20 to +20 dB),
playback monitor level (−20 to +6 dB), and the recording source for each
channel. **DRY** captures before the effects chain. **EFFECT** captures
after the chain and Global EQ. Host playback joins the analog output mix
after the USB recording tap.

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

The cross-build produces both firmware targets in `sources/build/`.

| Target | Firmware image and programming address | Debug symbols |
| --- | --- | --- |
| `gp100_base` | `gp100_base.bin` at `0x60000000`. Includes the i.MX RT boot headers. | `gp100_base.elf` |
| `gp100_stock_slot` | `gp100_stock_slot.bin` at `0x60080000`. Requires the Valeton bootloader and factory cabinet bank. | `gp100_stock_slot.elf` |

Build one target with `cmake --build sources/build --target gp100_base` or
`cmake --build sources/build --target gp100_stock_slot`.

### Build with a selected set of pedals

The default build includes the full catalog. For a smaller set, copy the
[bass-live profile](sources/cmake/profiles/bass-live.cmake) and list the pedal
names to include in `GP100_EFFECTS`. Names match the Signal Chain picker and
are case-insensitive. Use `AMP User 01` or `Cab User 01` for user slots, with
the desired slot number. An empty `GP100_EFFECTS` list makes a build with no
chain pedals.

The [bass-rig profile](sources/cmake/profiles/bass-rig.cmake) includes all
current pedals outside AMP and Cab, plus the five factory bass amps and five
factory bass cabinets.

Use a separate build directory and the same SDK settings as above.

```sh
make -C sources gp100_base BUILD_DIR=build-bass \
  EFFECT_PROFILE=cmake/profiles/bass-live.cmake \
  SDK_ROOT=/path/to/mcux-sdk USB_ROOT=/path/to/mcux-sdk-middleware-usb \
  TOOLCHAIN=/path/to/bin/arm-none-eabi-
```

The picker lists the selected pedals. An effect ID absent from the build
shows as a generic **Unknown FX** pedal in Signal Chain and triggers an
**Unavailable FX** indicator on Play. Audio passes through that slot. The
original ID, parameters, and links stay in the preset and load again in a
build containing that pedal. This also covers new pedals in presets from
newer builds using the same preset record format.
The Global Output Limiter is part of the output mixer.

The standalone image fills excluded factory IR slots with erased bytes at
their original addresses. Fixed asset offsets still determine the raw file
length. The stock application slot uses the cabinet bank already on the device.

For MCUXpresso IDE, import `sources/` via
**File → Import → General → Existing Projects into Workspace**. Its make
builder accepts `MCUX_SDK_ROOT`, `MCUX_USB_ROOT`, and `TOOLCHAIN` as environment
variables. A host C compiler runs the host checks. Tests that use factory
records read the [original flash dump](firmware/README.md).

## Program over SWD

Power the pedal from its normal supply. For a first-time backup of a unit,
halt the MCU and use `savebin /path/to/backup.bin 0x60000000 0x800000` and
`verifybin /path/to/backup.bin 0x60000000` in J-Link Commander. Keep that
backup off Git so it can restore the unit's settings and identity.

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
verifybin sources/build/gp100_base.bin 0x60000000
```

After programming and verification succeed, reset the pedal. A target-pin
reset via the generic `Cortex-M7` target also works on the bench with
`RSetType 2`, `r`, and `g`. [SEGGER reset strategies](https://kb.segger.com/J-Link_Reset_Strategies)

## Stock application slot

The stock-slot target links OpenGP100 to run from the original application
slot at flash offset `0x080000`. The Valeton bootloader copies a small entry
routine to ITCM, which jumps to a flash routine that allocates OCRAM for
the audio and USB DMA buffers before entering the SDK reset handler. The
raw slot includes the rhythm banks and uses the factory cabinet bank in flash.

### Flash the stock application slot over SWD

With the original bootloader and factory cabinet bank in place, connect
J-Link as described above and program the application slot.

```text
r
loadfile sources/build/gp100_stock_slot.bin 0x60080000 noreset
verifybin sources/build/gp100_stock_slot.bin 0x60080000
```

Power-cycle after verification. This write range ends below the user data.
On the bench, direct SWD programming and readback verified the updated slot.
The audio test confirmed that the ADC and DAC DMA buffers carried samples
after restarting from the stock bootloader.

### Optional Valeton USB package

Give the stock-slot target a Valeton v2.1 update file to also produce
`sources/build/OpenGP100-stock-update.bin` with the cabinet bank. Add
`STOCK_UPDATE_TEMPLATE="$PWD/firmware/GP-100_Firmware_V2.1.bin"` to the
Make command, or configure an existing build from the repository root.

```sh
cmake -S sources -B sources/build -DGP100_STOCK_UPDATE_TEMPLATE="$PWD/firmware/GP-100_Firmware_V2.1.bin"
cmake --build sources/build --target gp100_stock_slot
```

With the pedal running stock firmware, select the package under **Settings →
Update Firmware** in Valeton GP-100 Editor. Export presets before starting
because the updater can erase them.

The USB install was tested with Valeton v2.1 and Windows GP-100 Editor 1.5.1.
For USB passthrough, attach `84ef:0021` in normal mode and `84ef:0022` when
the pedal switches into update mode.

## Verification

The host suite covers signal-chain editing, independent duplicate effects,
effect behavior, USB audio framing and buffering, and recovery from
interrupted preset and settings writes. The firmware image checks verify
the boot headers and packed audio banks.

On the pedal, both USB audio formats passed simultaneous playback and
recording while handling MIDI requests. A 56-second playback trace per
format found zero sample discontinuities, late DAC buffers, or USB
transfer errors. Capture-only and repeated stream restarts also passed.
