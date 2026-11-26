#include "storage/factory_reset_plan.h"

#include <stdint.h>

typedef struct expected_step {
    size_t step;
    uint32_t address;
} expected_step_t;

int main(void)
{
    static const expected_step_t expected[] = {
        {0u, UINT32_C(0x006e0000)},
        {31u, UINT32_C(0x006ff000)},
        {32u, UINT32_C(0x00778000)},
        {151u, UINT32_C(0x007ef000)},
        {152u, UINT32_C(0x007f0000)},
        {160u, UINT32_C(0x007f8000)},
        {161u, UINT32_C(0x007f9000)},
        {165u, UINT32_C(0x007fd000)},
        {166u, UINT32_C(0x006c0000)},
    };
    size_t index;

    for (index = 0u; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        uint32_t address = 0u;

        if (!gp100_factory_reset_sector_address(
                expected[index].step, &address) ||
            address != expected[index].address) {
            return 1;
        }
    }
    if (gp100_factory_reset_sector_address(0u, NULL) ||
        gp100_factory_reset_sector_address(
            GP100_FACTORY_RESET_SECTOR_STEPS, &(uint32_t){0u})) {
        return 2;
    }
    return 0;
}
