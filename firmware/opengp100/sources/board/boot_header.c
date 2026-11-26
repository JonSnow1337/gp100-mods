#include <stdint.h>

/* Exact 512-byte FCB recovered from firmware/valeton_spi_flash.bin. It tells
 * the i.MX RT ROM how to read this board's 8 MiB DDR quad-SPI NOR. */
__attribute__((section(".boot_hdr.conf"), used))
const uint32_t g_gp100_flexspi_config[128] = {
    [0x00 / 4] = 0x42464346u,
    [0x04 / 4] = 0x56010400u,
    [0x0c / 4] = 0x00030300u,
    [0x40 / 4] = 0x00000051u,
    [0x44 / 4] = 0x00050400u,
    [0x50 / 4] = 0x00800000u,
    [0x70 / 4] = 0x000003e8u,
    [0x7c / 4] = 0x0001000fu,
    [0x80 / 4] = 0x0818040bu,
    [0x84 / 4] = 0x24043008u,
    [0x90 / 4] = 0x24040405u,
    [0xb0 / 4] = 0x00000406u,
    [0xd0 / 4] = 0x08180420u,
    [0x110 / 4] = 0x08180402u,
    [0x114 / 4] = 0x00002004u,
    [0x130 / 4] = 0x08000400u,
    [0x134 / 4] = 0x00002000u,
    [0x170 / 4] = 0x08000400u,
    [0x174 / 4] = 0x00002000u,
    [0x184 / 4] = 0x00000102u,
    [0x188 / 4] = 0x00000302u,
    [0x18c / 4] = 0x00000504u,
    [0x190 / 4] = 0x00000902u,
    [0x194 / 4] = 0x00000b01u,
    [0x198 / 4] = 0x00000f01u,
    [0x1c0 / 4] = 0x00000100u,
    [0x1c4 / 4] = 0x00001000u,
    [0x1c8 / 4] = 0x00000001u,
};
