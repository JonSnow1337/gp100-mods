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
