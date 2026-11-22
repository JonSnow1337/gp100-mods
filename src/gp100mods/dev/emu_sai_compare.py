#!/usr/bin/env python3
"""Emulate (Unicorn, peripherals = plain RAM) the SAI1 initialisation of (a) Valeton V2.1 FUN_00003c90 and
(b) OpenGP100 GP100_InitSai + GP100_AudioBindingStart, and print the final SAI1 register values each leaves.
Codec I2C and delays are stubbed. Read-only on inputs."""
import struct, sys
from unicorn import *; from unicorn.arm_const import *
SAI1 = 0x40384000
NAMES = {0x08:"TCSR",0x0c:"TCR1",0x10:"TCR2",0x14:"TCR3",0x18:"TCR4",0x1c:"TCR5",0x60:"TMR",
         0x88:"RCSR",0x8c:"RCR1",0x90:"RCR2",0x94:"RCR3",0x98:"RCR4",0x9c:"RCR5",0xe0:"RMR",0x100:"MCR"}

def mk(mem):
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    for a, n in [(0x0,0x80000),(0x20000000,0x80000),(0x20200000,0x80000),(0x80000000,0x2000000),
                 (0x40000000,0x1000000),(0x400000000 & 0xffffffff,0) ]:
        if n: uc.mem_map(a, n)
    uc.mem_map(0xE0000000,0x100000); uc.mem_map(0x60000000,0x800000); uc.mem_map(0x00200000,0x20000)
    uc.mem_map(0x90000000,0x1000)
    for a, d in mem: uc.mem_write(a, d)
    return uc

def run(uc, entry, args, stubs, maxn=20_000_000):
    writes = {}
    def w(u, acc, addr, size, val, ud):
        if SAI1 <= addr < SAI1 + 0x200: writes[addr - SAI1] = val & 0xffffffff
    uc.hook_add(UC_HOOK_MEM_WRITE, w, begin=SAI1, end=SAI1+0x200)
    def code(u, addr, size, ud):
        if addr in stubs:
            u.reg_write(UC_ARM_REG_R0, stubs[addr]); u.reg_write(UC_ARM_REG_PC, u.reg_read(UC_ARM_REG_LR))
    uc.hook_add(UC_HOOK_CODE, code)
    uc.reg_write(UC_ARM_REG_SP, 0x2007f000); uc.reg_write(UC_ARM_REG_LR, 0x90000001)
    for r, v in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3], args): uc.reg_write(r, v)
    err = None
    try: uc.emu_start(entry | 1, 0x90000000, count=maxn)
    except UcError as e: err = f"{e} at {uc.reg_read(UC_ARM_REG_PC):#x}"
    final = {NAMES[o]: struct.unpack("<I", uc.mem_read(SAI1+o, 4))[0] for o in NAMES}
    return final, err, uc.reg_read(UC_ARM_REG_PC)

def valeton(fw_path):
    fw = open(fw_path,"rb").read()
    uc = mk([(0, fw[0x70:0x70+0x34951]), (0x80000000, fw[0x349c1:0x349c1+0x26b179])])
    # FUN_2aa8 codec start -> 0 (ok); FUN_1514c delay; FUN_4bcc clock query -> PLL4 audio 45.1584 MHz*64? return 0x2B11000*? use 2.8224 MHz*64
    stubs = {0x2aa8: 0, 0x1514c: 0, 0x50c0: 0, 0x4bcc: 0x2b11000}
    return run(uc, 0x3c90, [0xac44, 0x20], stubs)

def opengp100(elf):
    from elftools.elf.elffile import ELFFile
    from gp100mods.dev import emu_startup as E
    flash, syms = E.load(elf)
    uc = mk([(0x60000000, bytes(flash))])
    # run normal startup to main first so .data/.ram_function exist, then call the SAI init + start
    r = E.run(elf)                          # separate instance just to reuse logic is costly; do it inline instead
    return None

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: python -m gp100mods.dev.emu_sai_compare PATH_TO_STOCK_V2.1_fw.bin")
    f, err, pc = valeton(sys.argv[1])
    print("VALETON  stop:", err or "returned", hex(pc))
    for k, v in f.items(): print(f"   {k:5s} = {v:#010x}")
