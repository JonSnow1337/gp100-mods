# Recovery

Installing OpenGP100 with this project never touches Valeton's bootloader (flash `0x2000`) or the updater image (slot A,
flash `0x20000`). That is what makes recovery possible.

**If the pedal shows nothing, hangs, or you want stock firmware back:**

1. Unplug the power adapter. Keep the USB cable connected.
2. Open the official GP-100 app on the PC.
3. Hold **both footswitches and the PARA knob** pressed.
4. Plug the power back in while holding. The app lets you pick a firmware file; flash the official V2.1.

Why it works: the bootloader reads three buttons at power-on (GPIO1 pins for footswitch -, footswitch + and PARA). If all
three read pressed it boots the updater image regardless of what is in the main firmware slot. See
[boot-chain.md](boot-chain.md).

Every firmware update erases the user presets: back them up first.

## Upgrading OpenGP100 to a newer version

If OpenGP100 is already on your pedal you do **not** have to go back to the stock V2.1 first. Recovery mode accepts any
valid update file, including one made by this app:

1. Generate the new OpenGP100 firmware with the app (as usual; add your `.prst` presets again, they are erased by the update).
2. Unplug the power adapter, keep USB connected, open the official GP-100 app.
3. Hold **both footswitches and the PARA knob**, plug the power in while holding.
4. Pick the newly generated file and install it with GP100 app.

The "flash V2.1, then flash the mods" route is only needed the first time, when the pedal still runs Valeton's original
firmware (the official app's normal update button works there).
