# Ghidra scripts

Headless-Ghidra helpers used to analyse Valeton's bootloader and firmware (Cortex-M, Thumb). REA's own ARM mode cannot
handle Thumb-only Cortex-M code, so these scripts force Thumb and seed functions from the vector table.

Import with auto-analysis **off** (otherwise ARM-mode instructions are created first and the Thumb mode write fails) and
pass absolute output paths:

```sh
analyzeHeadless proj name -import image.bin -noanalysis -processor "ARM:LE:32:Cortex" -loader BinaryLoader \
  -loader-baseAddr 0x60002000 -scriptPath tools/ghidra -postScript VecDecompileAll.java /abs/out.c
```
