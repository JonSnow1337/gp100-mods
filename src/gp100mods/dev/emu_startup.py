#!/usr/bin/env python3
"""Run an OpenGP100 ELF from Reset_Handler to main() in Unicorn (Cortex-M7), flash image built from the ELF's load addresses.
Only checks startup *logic* (data/ram_function/bss init, stack, vector table); peripherals are plain RAM. Read-only on inputs.
Usage: emu_startup.py ELF [--dump OUT.npz]"""
import sys, struct, argparse
from elftools.elf.elffile import ELFFile
from unicorn import *
from unicorn.arm_const import *

FLASH = 0x60000000; FSZ = 0x800000

def load(elf_path):
    e = ELFFile(open(elf_path, "rb")); flash = bytearray(b"\xff" * FSZ); syms = {}
    for s in e.get_section_by_name(".symtab").iter_symbols(): syms[s.name] = s["st_value"]
    for seg in e.iter_segments():
        if seg["p_type"] != "PT_LOAD" or seg["p_filesz"] == 0: continue
        lma = seg["p_paddr"]
        if FLASH <= lma < FLASH + FSZ:
            flash[lma - FLASH: lma - FLASH + seg["p_filesz"]] = seg.data()
    return flash, syms

def run(elf_path, max_insn=20_000_000):
    flash, syms = load(elf_path)
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(FLASH, FSZ); uc.mem_write(FLASH, bytes(flash))
    uc.mem_map(0x00000000, 0x80000)        # ITCM 512K
    uc.mem_map(0x20000000, 0x80000)        # DTCM region (192K real)
    uc.mem_map(0x20200000, 0x80000)        # OCRAM
    uc.mem_map(0x80000000, 0x2000000)      # SDRAM 32M
    uc.mem_map(0x40000000, 0x1000000)      # peripherals (plain RAM)
    uc.mem_map(0xE0000000, 0x100000)       # PPB (plain RAM)
    # emulate what the vendor bootloader leaves: vector copy in ITCM is irrelevant, Reset_Handler sets MSP itself
    reset = syms["Reset_Handler"]; main = syms["main"]
    uc.reg_write(UC_ARM_REG_SP, 0x81F80800)
    state = {"n": 0}
    stop_at = main & ~1
    def hook_code(u, addr, size, ud):
        ud["n"] += 1
        if addr == stop_at: u.emu_stop()
    uc.hook_add(UC_HOOK_CODE, hook_code, state)
    try:
        uc.emu_start(reset | 1, 0, count=max_insn)
    except UcError as ex:
        pc = uc.reg_read(UC_ARM_REG_PC)
        return dict(ok=False, err=str(ex), pc=pc, n=state["n"], uc=uc, syms=syms)
    pc = uc.reg_read(UC_ARM_REG_PC)
    return dict(ok=(pc & ~1) == stop_at, pc=pc, n=state["n"], uc=uc, syms=syms,
                sp=uc.reg_read(UC_ARM_REG_SP), vtor=struct.unpack("<I", uc.mem_read(0xE000ED08, 4))[0])

if __name__ == "__main__":
    r = run(sys.argv[1])
    print({k: (hex(v) if isinstance(v, int) and k != "n" else v) for k, v in r.items() if k not in ("uc", "syms")})
