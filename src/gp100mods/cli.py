"""`python -m gp100mods <command> ...` : one entry point for every tool."""
import importlib
import sys

COMMANDS = {
    "info":            ("gp100mods.frmw",                    "inspect and verify an update file"),
    "firmware-add":    ("gp100mods.bundle",                  "add a firmware version to the app bundle"),
    "opengp100-build": ("gp100mods.opengp100.build",         "build OpenGP100 as an update file for the GP-100 app"),
    "opengp100-pack":  ("gp100mods.opengp100.slot_packer",   "wrap a slot image into an update file"),
    "bootloader-sim":  ("gp100mods.opengp100.bootloader_sim", "simulate the vendor bootloader on an update file"),
}


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or argv[0] in ("-h", "--help") or argv[0] not in COMMANDS:
        print("usage: python -m gp100mods <command> [args]\n\ncommands:")
        for name, (_, help_) in COMMANDS.items():
            print(f"  {name:16s} {help_}")
        return 0 if (not argv or argv[0] in ("-h", "--help")) else 2
    module = importlib.import_module(COMMANDS[argv[0]][0])
    sys.argv = [f"gp100mods {argv[0]}"] + argv[1:]
    result = module.main()
    return result if isinstance(result, int) else 0


if __name__ == "__main__":
    sys.exit(main())
