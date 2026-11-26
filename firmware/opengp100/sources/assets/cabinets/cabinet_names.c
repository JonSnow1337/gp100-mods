#include "audio/effects/cab/cabinet.h"

/* Display name by one-based slot, in the same order as the numbered WAVs. */
__attribute__((section(".gp100_asset"), used))
const char gp100_factory_cabinet_names[][GP100_CABINET_NAME_SIZE] = {
    "TWD 2x12", /* 01 */
    "DarkTW 2x12", /* 02 */
    "L-Star 2x12", /* 03 */
    "2Rick 2x12", /* 04 */
    "J-120 2x12", /* 05 */
    "UK-GN 2x12", /* 06 */
    "Free 2x12", /* 07 */
    "UK-75 4X12", /* 08 */
    "UK-GN 4x12", /* 09 */
    "UK-LD 4X12", /* 10 */
    "UK-DK 4X12", /* 11 */
    "UK-MD 4X12", /* 12 */
    "Pogner 4x12", /* 13 */
    "Dizz 4x12", /* 14 */
    "Eagle 4X12", /* 15 */
    "EV51 4X12", /* 16 */
    "Solo 4X12", /* 17 */
    "US 4X12", /* 18 */
    "Mess-D 4x12", /* 19 */
    "U-ban 4X12", /* 20 */
    "Juice 4X12", /* 21 */
    "H-Way 4x12", /* 22 */
    "BogSV 1x12", /* 23 */
    "Dark 1x12", /* 24 */
    "Regular 1x12", /* 25 */
    "BAD-KT 1X12", /* 26 */
    "Foxy 1X12", /* 27 */
    "Studio 1X12", /* 28 */
    "SUP 1x6", /* 29 */
    "TWD 1X8", /* 30 */
    "TWD-P 1X10", /* 31 */
    "Bellman 4x10", /* 32 */
    "MessBass 2x10", /* 33 */
    "Max 4x10", /* 34 */
    "Ameg 4x10", /* 35 */
    "Ameg 8x10", /* 36 */
    "D", /* 37 */
    "OM", /* 38 */
    "Jumbo", /* 39 */
    "GA", /* 40 */
};

_Static_assert(sizeof(gp100_factory_cabinet_names) ==
                   GP100_CABINET_FACTORY_COUNT * GP100_CABINET_NAME_SIZE,
               "Expected one name for each factory cabinet");
