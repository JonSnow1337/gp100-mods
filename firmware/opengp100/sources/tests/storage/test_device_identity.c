#include "storage/device_identity.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint8_t k_indices[50] = {
    0x66u, 0x32u, 0x76u, 0x30u, 0x3bu, 0x36u, 0x1au, 0x75u,
    0x20u, 0x19u, 0x7fu, 0x63u, 0x1cu, 0x6au, 0x11u, 0x22u,
    0x4fu, 0x0cu, 0x77u, 0x02u, 0x0fu, 0x56u, 0x20u, 0x6du,
    0x7fu, 0x36u, 0x5cu, 0x52u, 0x3cu, 0x6cu, 0x50u, 0x2eu,
    0x15u, 0x13u, 0x14u, 0x57u, 0x01u, 0x5au, 0x29u, 0x42u,
    0x01u, 0x05u, 0x16u, 0x49u, 0x2fu, 0x43u, 0x36u, 0x75u,
    0x2cu, 0x68u
};

static void encode_identity(uint8_t record[GP100_DEVICE_RECORD_SIZE],
                            const uint8_t identity[GP100_DEVICE_ID_SIZE])
{
    uint32_t byte_index;
    for (byte_index = 0u; byte_index < GP100_DEVICE_ID_SIZE;
         ++byte_index) {
        uint8_t value = (uint8_t)~identity[byte_index];
        uint32_t round;
        for (round = 0u; round < GP100_DEVICE_ID_SIZE; ++round) {
            value ^= (uint8_t)~record[GP100_DEVICE_ID_SIZE +
                                      k_indices[round * byte_index]];
        }
        record[byte_index] = value;
    }
}

int main(void)
{
    uint8_t record[GP100_DEVICE_RECORD_SIZE] = {0};
    const uint8_t expected[GP100_DEVICE_ID_SIZE] = {
        0x12u, 0x34u, 0x56u, 0x78u, 0x9au, 0xbcu, 0xdeu, 0xf0u
    };
    uint8_t decoded[GP100_DEVICE_ID_SIZE];
    uint32_t index;

    for (index = 0u; index < GP100_DEVICE_SUBSTITUTION_SIZE; ++index) {
        record[GP100_DEVICE_ID_SIZE + index] =
            (uint8_t)(index * 37u + 11u);
    }
    encode_identity(record, expected);
    if (!gp100_decode_device_identity(record, decoded) ||
        memcmp(decoded, expected, sizeof(decoded)) != 0 ||
        !gp100_device_identity_matches(record, expected)) {
        fputs("device identity round trip failed\n", stderr);
        return 1;
    }
    decoded[7] ^= 1u;
    if (gp100_device_identity_matches(record, decoded) ||
        gp100_decode_device_identity(NULL, decoded) ||
        gp100_device_identity_matches(record, NULL)) {
        fputs("device identity mismatch handling failed\n", stderr);
        return 1;
    }
    puts("device identity tests passed");
    return 0;
}
