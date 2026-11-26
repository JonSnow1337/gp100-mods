ENTRY(Reset_Handler)

HEAP_SIZE  = DEFINED(__heap_size__)  ? __heap_size__  : 0x1000;
STACK_SIZE = DEFINED(__stack_size__) ? __stack_size__ : 0x4000;
IMAGE_BASE = DEFINED(__stock_slot_build) ? 0x60081000 : 0x60000000;

MEMORY
{
    STOCK_ENTRY  (rx)  : ORIGIN = 0x60080020, LENGTH = 0x0fe0
    FLASH_CONFIG (rx)  : ORIGIN = IMAGE_BASE, LENGTH = 0x1000
    IVT          (rx)  : ORIGIN = IMAGE_BASE + 0x1000, LENGTH = 0x1000
    VECTORS      (rx)  : ORIGIN = IMAGE_BASE + 0x2000, LENGTH = 0x0400
    /* The first factory bank (rhythm PCM) begins at offset 0x133DDC. */
    FLASH        (rx)  : ORIGIN = IMAGE_BASE + 0x2400, LENGTH = 0x60133ddc - (IMAGE_BASE + 0x2400)
    ITCM         (rwx) : ORIGIN = 0x00000000, LENGTH = 0x00020000
    DTCM         (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00020000
    OCRAM        (rwx) : ORIGIN = 0x20200000, LENGTH = 0x00040000
    SDRAM        (rwx) : ORIGIN = 0x80000000, LENGTH = 0x02000000
}

SECTIONS
{
    .stock_slot_entry :
    {
        KEEP(*(.stock_slot_entry))
    } > STOCK_ENTRY

    .flash_config :
    {
        __FLASH_BASE = .;
        KEEP(*(.boot_hdr.conf))
    } > FLASH_CONFIG

    .ivt :
    {
        KEEP(*(.boot_hdr.ivt))
        KEEP(*(.boot_hdr.boot_data))
        KEEP(*(.boot_hdr.dcd_data))
    } > IVT

    .isr_vector :
    {
        __VECTOR_TABLE = .;
        __Vectors = .;
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > VECTORS

    /* The SDK startup copies the latency-sensitive audio code into ITCM.
     * Match these objects ahead of the archive and flash catchalls below. */
    .ram_function : AT(__data_load_end__)
    {
        . = ALIGN(32);
        __ram_function_start__ = .;
        *audio_binding.c.obj(.text .text.* .rodata .rodata.*)
        *audio_transport.c.obj(.text .text.* .rodata .rodata.*)
        *fsl_edma.c.obj(.text .text.* .rodata .rodata.*)
        *effect_processor.c.obj(.text .text.* .rodata .rodata.*)
        *cabinet.c.obj(.text .text.* .rodata .rodata.*)
        *partitioned_convolution.c.obj(.text .text.* .rodata .rodata.*)
        *room_reverb.c.obj(.text .text.* .rodata .rodata.*)
        *audio_biquad.c.obj(.text .text.* .rodata .rodata.*)
        *audio_engine.c.obj(.text .text.* .rodata .rodata.*)
        *audio_output.c.obj(.text .text.* .rodata .rodata.*)
        *output_limiter.c.obj(.text .text.* .rodata .rodata.*)
        *audio_pcm.c.obj(.text .text.* .rodata .rodata.*)
        *audio_route.c.obj(.text .text.* .rodata .rodata.*)
        *audio_transition.c.obj(.text .text.* .rodata .rodata.*)
        *libm.a:*(.text .text.* .rodata .rodata.*)
        /* Keep the ROM-call wrappers and their literals available while
         * FlexSPI is recovering from an erase, program, or cache reset. */
        *flash_storage.c.obj(.text .text.* .rodata .rodata.*)
        *fsl_romapi.c.obj(.text .text.* .rodata .rodata.*)
        *(CodeQuickAccess .ramfunc .ramfunc*)
        . = ALIGN(32);
        __ram_function_end__ = .;
    } > ITCM

    /* Move the remaining reference code off the FlexSPI instruction path.
     * The boot image still carries its load image in flash. */
    .sdram_code : AT(__noncache_load__ + SIZEOF(.ncache.init))
    {
        __sdram_code_start__ = .;
        *libgp100_core.a:*(.text .text.* .rodata .rodata.*)
        . = ALIGN(32);
        __sdram_code_end__ = .;
    } > SDRAM
    __sdram_code_load__ = LOADADDR(.sdram_code);

    .text :
    {
        . = ALIGN(4);
        *(.text .text*)
        *(.rodata .rodata*)
        *(.glue_7 .glue_7t)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    . = ALIGN(4);
    __etext = .;

    .data : AT(__etext)
    {
        . = ALIGN(4);
        __data_start__ = .;
        *(.data .data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > DTCM

    __data_load_end__ = __etext + SIZEOF(.data);

    __noncache_load__ = __data_load_end__ + SIZEOF(.ram_function);

    /* BOARD_ConfigMPU maps the entire OCRAM bank as non-cacheable memory. */
    .ncache.init : AT(__noncache_load__)
    {
        . = ALIGN(32);
        __noncachedata_start__ = .;
        *(NonCacheable.init)
        *(.noncache.init .noncache.init*)
        . = ALIGN(32);
        __noncachedata_init_end__ = .;
    } > OCRAM

    .ncache (NOLOAD) :
    {
        *(NonCacheable)
        *(.noncache .noncache*)
        . = ALIGN(32);
        __noncachedata_end__ = .;
    } > OCRAM

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        __bss_start__ = .;
        *(.bss .bss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end__ = .;
    } > DTCM

    /* SEMC is established by the boot-ROM DCD before Reset_Handler. These
     * sections are NOLOAD so the XIP image does not contain 32 MiB of padding;
     * main clears only .sdram_bss after its destructive-safe probe succeeds. */
    .sdram_bss (NOLOAD) :
    {
        . = ALIGN(32);
        __sdram_bss_start__ = .;
        KEEP(*(.sdram_bss .sdram_bss*))
        . = ALIGN(32);
        __sdram_bss_end__ = .;
    } > SDRAM

    .sdram_noinit (NOLOAD) :
    {
        . = ALIGN(32);
        __sdram_noinit_start__ = .;
        KEEP(*(.sdram_noinit .sdram_noinit*))
        . = ALIGN(32);
        __effect_state_start__ = .;
        . += 0x00300000;
        __effect_state_end__ = .;
        __sdram_noinit_end__ = .;
    } > SDRAM

    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __HeapBase = .;
        . += HEAP_SIZE;
        __HeapLimit = .;
        end = .;
    } > DTCM

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        . += STACK_SIZE;
        __StackLimit = . - STACK_SIZE;
        __StackTop = .;
    } > DTCM

    ASSERT(__StackTop <= ORIGIN(DTCM) + LENGTH(DTCM), "DTCM overflow")
    ASSERT(__sdram_noinit_end__ <= ORIGIN(SDRAM) + LENGTH(SDRAM), "SDRAM overflow")
    ASSERT(SIZEOF(.flash_config) == 0x200, "FCB must be exactly 512 bytes")
    ASSERT(SIZEOF(.isr_vector) <= LENGTH(VECTORS), "vector table overflow")
    ASSERT(LOADADDR(.sdram_code) + SIZEOF(.sdram_code) <= ORIGIN(FLASH) + LENGTH(FLASH),
           "application load image overlaps factory audio")
}
