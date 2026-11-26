#ifndef GP100_FLASH_LAYOUT_H
#define GP100_FLASH_LAYOUT_H

/* Offsets from the start of the 8 MiB FlexSPI device. */
#define GP100_USER_DATA_START     0x006c0000u

/* Both stores are in previously unused sectors of the factory-reset range.
 * Leave old preset and settings records available for read-only migration. */
#define GP100_PRESET_STORE_BASE   0x0078b000u
#define GP100_SETTINGS_STORE_BASE 0x007e7000u

#define GP100_RESET_PRESET_START   0x00778000u
#define GP100_RESET_PRESET_SECTORS 120u

#endif
