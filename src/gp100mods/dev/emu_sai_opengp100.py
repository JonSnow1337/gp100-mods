#!/usr/bin/env python3
"""Emulate OpenGP100 (ELF) from Reset_Handler through main() with clock queries, delays and the codec I2C stubbed,
and print SAI1's final registers once GP100_AudioTransportStart has returned. Peripherals = plain RAM."""
import sys, struct, subprocess
from gp100mods.dev import emu_startup as E
from unicorn import *; from unicorn.arm_const import *
from gp100mods.dev.emu_sai_compare import NAMES, SAI1
def main():
    elf = sys.argv[1]
    flash, syms = E.load(elf)
    B = elf.rsplit("/", 1)[0] + "/generated/factory_banks/"
    for name, off in [("rhythm_samples.bin",0x133DDC),("rhythm_patterns.bin",0x1C7414),("factory_cabinets.bin",0x680000)]:
        d = open(B+name,"rb").read(); flash[off:off+len(d)] = d
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    try: uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M7)
    except Exception as e: print("cpu model:", e)
    uc.mem_map(0x60000000,0x800000); uc.mem_write(0x60000000, bytes(flash))
    for a,n in [(0,0x80000),(0x20000000,0x80000),(0x20200000,0x80000),(0x80000000,0x2000000),(0x40000000,0x1000000),(0x00200000,0x20000),(0x90000000,0x1000),(0xE0000000,0x100000)]: uc.mem_map(a,n)
    def s(name): return syms[name] & ~1
    stubs = {s("CLOCK_GetClockRootFreq"): 45158400, s("CLOCK_GetFreq"): 600000000, s("SDK_DelayAtLeastUs"): 0,
             s("GP100_CodecStartup"): 1, s("GP100_ApplicationInitializeDisplay"): 1}
    for opt in ["BOARD_InitAudioClockRoots", "GP100_USB_DeviceInitialize", "GP100_ApplicationStartAnalog"]:
        if opt in syms: stubs[s(opt)] = 1
    start_ret = {"hit": False}
    tstart = s("GP100_AudioTransportStart")
    state = {"in_start": False, "lr": None}
    def code(u, addr, size, ud):
        if addr in stubs:
            u.reg_write(UC_ARM_REG_R0, stubs[addr]); u.reg_write(UC_ARM_REG_PC, u.reg_read(UC_ARM_REG_LR) | 1); return
        if addr == tstart and not state["in_start"]:
            state["in_start"] = True; state["lr"] = u.reg_read(UC_ARM_REG_LR) & ~1
        elif state["in_start"] and addr == state["lr"]:
            start_ret["hit"] = True; u.emu_stop()
    uc.hook_add(UC_HOOK_CODE, code)
    uc.reg_write(UC_ARM_REG_SP, 0x81F80800)
    err = None
    try: uc.emu_start(syms["Reset_Handler"] | 1, 0, count=400_000_000)
    except UcError as e: err = str(e)
    pc = uc.reg_read(UC_ARM_REG_PC)
    fn = subprocess.run(["arm-none-eabi-addr2line","-f","-e",elf,hex(pc)],capture_output=True,text=True).stdout.split()[0]
    print("OPENGP100 stop:", err or "ok", hex(pc), fn, "| AudioTransportStart returned:", start_ret["hit"])
    for o, k in NAMES.items(): print(f"   {k:5s} = {struct.unpack('<I', uc.mem_read(SAI1+o,4))[0]:#010x}")


if __name__ == "__main__":
    main()
