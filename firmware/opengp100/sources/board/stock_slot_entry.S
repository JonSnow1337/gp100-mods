.syntax unified
.thumb

/* Valeton copies this section to ITCM at zero and calls its second word. */
.section .stock_slot_entry, "ax", %progbits
.balign 4
stock_slot_vectors:
    .word __StackTop
    .word stock_slot_reset - stock_slot_vectors + 1
stock_slot_reset:
    ldr r0, stock_slot_flash_entry
    ldr r1, stock_slot_sdk_entry
    bx r0
    .balign 4
stock_slot_flash_entry:
    .word stock_slot_flash_start
stock_slot_sdk_entry:
    .word Reset_Handler

/* Switch banks while executing in flash with no stack. Valeton's layout
 * gives all 16 banks to TCM. Our linker needs eight OCRAM banks for DMA
 * and four banks each for DTCM and ITCM. SDK startup then initializes RAM. */
.thumb_func
stock_slot_flash_start:
    cpsid i
    ldr r0, =0x400ac040                 /* IOMUXC_GPR_GPR16 */
    ldr r2, =0xffaa5555                  /* GPR17, 32 KiB per bank */
    str r2, [r0, #4]
    ldr r2, [r0]
    orr r2, r2, #4                       /* FLEXRAM_BANK_CFG_SEL */
    str r2, [r0]
    dsb
    isb
    bx r1
    .ltorg
